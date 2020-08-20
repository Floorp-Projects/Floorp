/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mp4parse.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <vector>

static intptr_t error_reader(uint8_t* buffer, uintptr_t size, void* userdata) {
  return -1;
}

struct read_vector {
  explicit read_vector(FILE* file, size_t length);
  explicit read_vector(size_t length);

  size_t location;
  std::vector<uint8_t> buffer;
};

read_vector::read_vector(FILE* file, size_t length) : location(0) {
  buffer.resize(length);
  size_t read = fread(buffer.data(), sizeof(decltype(buffer)::value_type),
                      buffer.size(), file);
  buffer.resize(read);
}

read_vector::read_vector(size_t length) : location(0) {
  buffer.resize(length, 0);
}

static intptr_t vector_reader(uint8_t* buffer, uintptr_t size, void* userdata) {
  if (!buffer || !userdata) {
    return -1;
  }

  auto source = reinterpret_cast<read_vector*>(userdata);
  if (source->location > source->buffer.size()) {
    return -1;
  }
  uintptr_t available = source->buffer.size() - source->location;
  uintptr_t length = std::min(available, size);
  memcpy(buffer, source->buffer.data() + source->location, length);
  source->location += length;
  return length;
}

TEST(rust, MP4MetadataEmpty)
{
  Mp4parseStatus rv;
  Mp4parseIo io;
  Mp4parseParser* parser = nullptr;

  // Shouldn't be able to read with no context.
  rv = mp4parse_new(nullptr, nullptr);
  EXPECT_EQ(rv, MP4PARSE_STATUS_BAD_ARG);

  // Shouldn't be able to wrap an Mp4parseIo with null members.
  io = {nullptr, nullptr};
  rv = mp4parse_new(&io, &parser);
  EXPECT_EQ(rv, MP4PARSE_STATUS_BAD_ARG);
  EXPECT_EQ(parser, nullptr);

  io = {nullptr, &io};
  rv = mp4parse_new(&io, &parser);
  EXPECT_EQ(rv, MP4PARSE_STATUS_BAD_ARG);
  EXPECT_EQ(parser, nullptr);

  // FIXME: this should probably be accepted.
  io = {error_reader, nullptr};
  rv = mp4parse_new(&io, &parser);
  EXPECT_EQ(rv, MP4PARSE_STATUS_BAD_ARG);
  EXPECT_EQ(parser, nullptr);

  // Read method errors should propagate.
  io = {error_reader, &io};
  rv = mp4parse_new(&io, &parser);
  ASSERT_EQ(parser, nullptr);
  EXPECT_EQ(rv, MP4PARSE_STATUS_IO);

  // Short buffers should fail.
  read_vector buf(0);
  io = {vector_reader, &buf};
  rv = mp4parse_new(&io, &parser);
  ASSERT_EQ(parser, nullptr);
  EXPECT_EQ(rv, MP4PARSE_STATUS_INVALID);

  buf.buffer.reserve(4097);
  rv = mp4parse_new(&io, &parser);
  ASSERT_EQ(parser, nullptr);
  EXPECT_EQ(rv, MP4PARSE_STATUS_INVALID);

  // Empty buffers should fail.
  buf.buffer.resize(4097, 0);
  rv = mp4parse_new(&io, &parser);
  ASSERT_EQ(parser, nullptr);
  EXPECT_EQ(rv, MP4PARSE_STATUS_UNSUPPORTED);
}

TEST(rust, MP4Metadata)
{
  FILE* f = fopen("street.mp4", "rb");
  ASSERT_TRUE(f != nullptr);
  // Read just the moov header to work around the parser
  // treating mid-box eof as an error.
  // read_vector reader = read_vector(f, 1061);
  struct stat s;
  ASSERT_EQ(0, fstat(fileno(f), &s));
  read_vector reader = read_vector(f, s.st_size);
  fclose(f);

  Mp4parseIo io = {vector_reader, &reader};
  Mp4parseParser* parser = nullptr;
  Mp4parseStatus rv = mp4parse_new(&io, &parser);
  ASSERT_NE(nullptr, parser);
  EXPECT_EQ(MP4PARSE_STATUS_OK, rv);

  uint32_t tracks = 0;
  rv = mp4parse_get_track_count(parser, &tracks);
  EXPECT_EQ(MP4PARSE_STATUS_OK, rv);
  EXPECT_EQ(2U, tracks);

  mp4parse_free(parser);
}
