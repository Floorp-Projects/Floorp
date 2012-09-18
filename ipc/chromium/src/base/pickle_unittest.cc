// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/pickle.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int testint = 2093847192;
const std::string teststr("Hello world"); // note non-aligned string length
const std::wstring testwstr(L"Hello, world");
const char testdata[] = "AAA\0BBB\0";
const int testdatalen = arraysize(testdata) - 1;
const bool testbool1 = false;
const bool testbool2 = true;

// checks that the result
void VerifyResult(const Pickle& pickle) {
  void* iter = NULL;

  int outint;
  EXPECT_TRUE(pickle.ReadInt(&iter, &outint));
  EXPECT_EQ(testint, outint);

  std::string outstr;
  EXPECT_TRUE(pickle.ReadString(&iter, &outstr));
  EXPECT_EQ(teststr, outstr);

  std::wstring outwstr;
  EXPECT_TRUE(pickle.ReadWString(&iter, &outwstr));
  EXPECT_EQ(testwstr, outwstr);

  bool outbool;
  EXPECT_TRUE(pickle.ReadBool(&iter, &outbool));
  EXPECT_EQ(testbool1, outbool);
  EXPECT_TRUE(pickle.ReadBool(&iter, &outbool));
  EXPECT_EQ(testbool2, outbool);

  const char* outdata;
  int outdatalen;
  EXPECT_TRUE(pickle.ReadData(&iter, &outdata, &outdatalen));
  EXPECT_EQ(testdatalen, outdatalen);
  EXPECT_EQ(memcmp(testdata, outdata, outdatalen), 0);

  EXPECT_TRUE(pickle.ReadData(&iter, &outdata, &outdatalen));
  EXPECT_EQ(testdatalen, outdatalen);
  EXPECT_EQ(memcmp(testdata, outdata, outdatalen), 0);

  // reads past the end should fail
  EXPECT_FALSE(pickle.ReadInt(&iter, &outint));
}

}  // namespace

TEST(PickleTest, EncodeDecode) {
  Pickle pickle;

  EXPECT_TRUE(pickle.WriteInt(testint));
  EXPECT_TRUE(pickle.WriteString(teststr));
  EXPECT_TRUE(pickle.WriteWString(testwstr));
  EXPECT_TRUE(pickle.WriteBool(testbool1));
  EXPECT_TRUE(pickle.WriteBool(testbool2));
  EXPECT_TRUE(pickle.WriteData(testdata, testdatalen));

  // Over allocate BeginWriteData so we can test TrimWriteData.
  char* dest = pickle.BeginWriteData(testdatalen + 100);
  EXPECT_TRUE(dest);
  memcpy(dest, testdata, testdatalen);

  pickle.TrimWriteData(testdatalen);

  VerifyResult(pickle);

  // test copy constructor
  Pickle pickle2(pickle);
  VerifyResult(pickle2);

  // test operator=
  Pickle pickle3;
  pickle3 = pickle;
  VerifyResult(pickle3);
}

TEST(PickleTest, ZeroLenStr) {
  Pickle pickle;
  EXPECT_TRUE(pickle.WriteString(""));

  void* iter = NULL;
  std::string outstr;
  EXPECT_TRUE(pickle.ReadString(&iter, &outstr));
  EXPECT_EQ("", outstr);
}

TEST(PickleTest, ZeroLenWStr) {
  Pickle pickle;
  EXPECT_TRUE(pickle.WriteWString(L""));

  void* iter = NULL;
  std::string outstr;
  EXPECT_TRUE(pickle.ReadString(&iter, &outstr));
  EXPECT_EQ("", outstr);
}

TEST(PickleTest, BadLenStr) {
  Pickle pickle;
  EXPECT_TRUE(pickle.WriteInt(-2));

  void* iter = NULL;
  std::string outstr;
  EXPECT_FALSE(pickle.ReadString(&iter, &outstr));
}

TEST(PickleTest, BadLenWStr) {
  Pickle pickle;
  EXPECT_TRUE(pickle.WriteInt(-1));

  void* iter = NULL;
  std::wstring woutstr;
  EXPECT_FALSE(pickle.ReadWString(&iter, &woutstr));
}

TEST(PickleTest, FindNext) {
  Pickle pickle;
  EXPECT_TRUE(pickle.WriteInt(1));
  EXPECT_TRUE(pickle.WriteString("Domo"));

  const char* start = reinterpret_cast<const char*>(pickle.data());
  const char* end = start + pickle.size();

  EXPECT_TRUE(end == Pickle::FindNext(pickle.header_size_, start, end));
  EXPECT_TRUE(NULL == Pickle::FindNext(pickle.header_size_, start, end - 1));
  EXPECT_TRUE(end == Pickle::FindNext(pickle.header_size_, start, end + 1));
}

TEST(PickleTest, IteratorHasRoom) {
  Pickle pickle;
  EXPECT_TRUE(pickle.WriteInt(1));
  EXPECT_TRUE(pickle.WriteInt(2));

  const void* iter = 0;
  EXPECT_FALSE(pickle.IteratorHasRoomFor(iter, 1));
  iter = pickle.payload();
  EXPECT_TRUE(pickle.IteratorHasRoomFor(iter, 0));
  EXPECT_TRUE(pickle.IteratorHasRoomFor(iter, 1));
  EXPECT_FALSE(pickle.IteratorHasRoomFor(iter, -1));
  EXPECT_TRUE(pickle.IteratorHasRoomFor(iter, sizeof(int) * 2));
  EXPECT_FALSE(pickle.IteratorHasRoomFor(iter, (sizeof(int) * 2) + 1));
}

TEST(PickleTest, Resize) {
  size_t unit = Pickle::kPayloadUnit;
  scoped_array<char> data(new char[unit]);
  char* data_ptr = data.get();
  for (size_t i = 0; i < unit; i++)
    data_ptr[i] = 'G';

  // construct a message that will be exactly the size of one payload unit,
  // note that any data will have a 4-byte header indicating the size
  const size_t payload_size_after_header = unit - sizeof(uint32_t);
  Pickle pickle;
  pickle.WriteData(data_ptr,
      static_cast<int>(payload_size_after_header - sizeof(uint32_t)));
  size_t cur_payload = payload_size_after_header;

  // note: we assume 'unit' is a power of 2
  EXPECT_EQ(unit, pickle.capacity());
  EXPECT_EQ(pickle.payload_size(), payload_size_after_header);

  // fill out a full page (noting data header)
  pickle.WriteData(data_ptr, static_cast<int>(unit - sizeof(uint32_t)));
  cur_payload += unit;
  EXPECT_EQ(unit * 2, pickle.capacity());
  EXPECT_EQ(cur_payload, pickle.payload_size());

  // one more byte should double the capacity
  pickle.WriteData(data_ptr, 1);
  cur_payload += 5;
  EXPECT_EQ(unit * 4, pickle.capacity());
  EXPECT_EQ(cur_payload, pickle.payload_size());
}

namespace {

struct CustomHeader : Pickle::Header {
  int blah;
};

}  // namespace

TEST(PickleTest, HeaderPadding) {
  const uint32_t kMagic = 0x12345678;

  Pickle pickle(sizeof(CustomHeader));
  pickle.WriteInt(kMagic);

  // this should not overwrite the 'int' payload
  pickle.headerT<CustomHeader>()->blah = 10;

  void* iter = NULL;
  int result;
  ASSERT_TRUE(pickle.ReadInt(&iter, &result));

  EXPECT_EQ(static_cast<uint32_t>(result), kMagic);
}

TEST(PickleTest, EqualsOperator) {
  Pickle source;
  source.WriteInt(1);

  Pickle copy_refs_source_buffer(static_cast<const char*>(source.data()),
                                 source.size());
  Pickle copy;
  copy = copy_refs_source_buffer;
  ASSERT_EQ(source.size(), copy.size());
}
