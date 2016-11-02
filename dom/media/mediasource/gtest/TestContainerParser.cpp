/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gtest/gtest.h>
#include <stdint.h>

#include "ContainerParser.h"
#include "mozilla/ArrayUtils.h"
#include "nsAutoPtr.h"

using namespace mozilla;

TEST(ContainerParser, MIMETypes) {
  const char* content_types[] = {
    "video/webm",
    "audio/webm",
    "video/mp4",
    "audio/mp4",
    "audio/aac"
  };
  nsAutoPtr<ContainerParser> parser;
  for (size_t i = 0; i < ArrayLength(content_types); ++i) {
    nsAutoCString content_type(content_types[i]);
    parser = ContainerParser::CreateForMIMEType(content_type);
    ASSERT_NE(parser, nullptr);
  }
}


already_AddRefed<MediaByteBuffer> make_adts_header()
{
  const uint8_t test[] = { 0xff, 0xf1, 0x50, 0x80, 0x03, 0x1f, 0xfc };
  RefPtr<MediaByteBuffer> buffer(new MediaByteBuffer);
  buffer->AppendElements(test, ArrayLength(test));
  return buffer.forget();
}

TEST(ContainerParser, ADTSHeader) {
  nsAutoPtr<ContainerParser> parser;
  parser = ContainerParser::CreateForMIMEType(NS_LITERAL_CSTRING("audio/aac"));
  ASSERT_NE(parser, nullptr);

  // Audio data should have no gaps.
  EXPECT_EQ(parser->GetRoundingError(), 0);

  // Test a valid header.
  RefPtr<MediaByteBuffer> header = make_adts_header();
  EXPECT_TRUE(NS_SUCCEEDED(parser->IsInitSegmentPresent(header)));

  // Test variations.
  uint8_t save = header->ElementAt(1);
  for (uint8_t i = 1; i < 3; ++i) {
    // Set non-zero layer.
    header->ReplaceElementAt(1, (header->ElementAt(1) & 0xf9) | (i << 1));
    EXPECT_FALSE(NS_SUCCEEDED(parser->IsInitSegmentPresent(header)))
      << "Accepted non-zero layer in header.";
  }
  header->ReplaceElementAt(1, save);
  save = header->ElementAt(2);
  header->ReplaceElementAt(2, (header->ElementAt(2) & 0x3b) | (15 << 2));
  EXPECT_FALSE(NS_SUCCEEDED(parser->IsInitSegmentPresent(header)))
    << "Accepted explicit frequency in header.";
  header->ReplaceElementAt(2, save);

  // Test a short header.
  header->SetLength(6);
  EXPECT_FALSE(NS_SUCCEEDED(parser->IsInitSegmentPresent(header)))
    << "Accepted too-short header.";
  EXPECT_FALSE(NS_SUCCEEDED(parser->IsMediaSegmentPresent(header)))
    << "Found media segment when there was just a partial header.";

  // Test parse results.
  header = make_adts_header();
  EXPECT_FALSE(NS_SUCCEEDED(parser->IsMediaSegmentPresent(header)))
    << "Found media segment when there was just a header.";
  int64_t start = 0;
  int64_t end = 0;
  EXPECT_FALSE(parser->ParseStartAndEndTimestamps(header, start, end));

  EXPECT_TRUE(parser->HasInitData());
  EXPECT_TRUE(parser->HasCompleteInitData());
  MediaByteBuffer* init = parser->InitData();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->Length(), header->Length());

  EXPECT_EQ(parser->InitSegmentRange(), MediaByteRange(0, int64_t(header->Length())));
  // Media segment range should be empty here.
  EXPECT_EQ(parser->MediaHeaderRange(), MediaByteRange());
  EXPECT_EQ(parser->MediaSegmentRange(), MediaByteRange());
}
