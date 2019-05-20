/*
 * Licensed to Green Energy Corp (www.greenenergycorp.com) under one or
 * more contributor license agreements. See the NOTICE file distributed
 * with this work for additional information regarding copyright ownership.
 * Green Energy Corp licenses this file to you under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This project was forked on 01/01/2013 by Automatak, LLC and modifications
 * may have been made to this file. Automatak, LLC licenses these modifications
 * to you under the terms of the License.
 */
#include <catch.hpp>

#include <asio.hpp>

#include <iostream>

using namespace asio;
using namespace asio::ip;

using asio::ip::tcp;

constexpr size_t max_bytes = 1024;

class session
	: public std::enable_shared_from_this<session>
{
public:
	session(tcp::socket socket)
		: socket_(std::move(socket))
	{
	}

	void start()
	{
		do_read();
	}

private:
	void do_read()
	{
		auto self(shared_from_this());
		socket_.async_read_some(asio::buffer(data_, max_bytes),
			[this, self](std::error_code ec, std::size_t length)
			{
				if (!ec)
				{
					do_write(length);
				}
			});
	}

	void do_write(std::size_t length)
	{
		auto self(shared_from_this());
		asio::async_write(socket_, asio::buffer(data_, length),
			[this, self](std::error_code ec, std::size_t /*length*/)
			{
				if (!ec)
				{
					do_read();
				}
			});
	}

	tcp::socket socket_;	
	char data_[max_bytes];
};

class echoserver
{
public:
	echoserver(asio::io_context& io_context, uint16_t port)
		: acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
	{
		do_accept();
	}

private:
	void do_accept()
	{
		acceptor_.async_accept(
			[this](std::error_code ec, tcp::socket socket)
			{
				if (!ec)
				{
					std::make_shared<session>(std::move(socket))->start();
				}

				do_accept();
			});
	}

	tcp::acceptor acceptor_;
};

TEST_CASE("MyLoopbackPerformanceTest")
{
	constexpr size_t num_iter = 1024*1024;	

	io_context context;
	echoserver server(context, 20000);	
	
	tcp::socket client(context);
	tcp::resolver resolver(context);
	asio::connect(client, resolver.resolve("127.0.0.1", "20000"));	

	std::array<uint8_t, max_bytes> buffer;
	for (int i = 0; i < max_bytes; ++i) buffer[i] = i % 256;

	const auto start = std::chrono::steady_clock::now();

	for (int i = 0; i < num_iter; ++i) {

		bool written = false;
		async_write(client, asio::buffer(buffer, buffer.size()), [&written](asio::error_code ec, size_t num) { REQUIRE_FALSE(ec); written = true;  });
		while (!written) {
			context.run_one();
		}

		bool read = false;
		async_read(client, asio::buffer(buffer, buffer.size()), [&read](asio::error_code ec, size_t num) {REQUIRE_FALSE(ec); read = true;  });
	}
	
	const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

	const auto total_bytes = num_iter * max_bytes;
	const auto bytes_per_second = (total_bytes * 1000) / elapsed_ms.count();

	std::cout << "Bytes: " << total_bytes << " in " << elapsed_ms.count() << "ms" << std::endl;
	std::cout << "Bytes per second: " << bytes_per_second << std::endl;
}










