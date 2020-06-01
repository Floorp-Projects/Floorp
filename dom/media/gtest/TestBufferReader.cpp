/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "BufferReader.h"

TEST(BufferReader, ReaderCursor)
{
  // Allocate a buffer and create a BufferReader.
  const size_t BUFFER_SIZE = 10;
  uint8_t buffer[BUFFER_SIZE] = {0};

  const uint8_t* const HEAD = reinterpret_cast<uint8_t*>(buffer);
  const uint8_t* const TAIL = HEAD + BUFFER_SIZE;

  BufferReader reader(HEAD, BUFFER_SIZE);
  ASSERT_EQ(reader.Offset(), static_cast<size_t>(0));
  ASSERT_EQ(reader.Peek(BUFFER_SIZE), HEAD);

  // Keep reading to the end, and make sure the final read failed.
  const size_t READ_SIZE = 4;
  ASSERT_NE(BUFFER_SIZE % READ_SIZE, static_cast<size_t>(0));
  for (const uint8_t* ptr = reader.Peek(0); ptr != nullptr;
       ptr = reader.Read(READ_SIZE)) {
  }

  // Check the reading cursor of the BufferReader is correct
  // after reading and seeking.
  const uint8_t* tail = reader.Peek(0);
  const uint8_t* head = reader.Seek(0);

  EXPECT_EQ(head, HEAD);
  EXPECT_EQ(tail, TAIL);
}