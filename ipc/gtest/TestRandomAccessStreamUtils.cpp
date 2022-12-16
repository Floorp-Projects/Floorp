/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "chrome/common/ipc_message.h"
#include "gtest/gtest.h"
#include "mozilla/NotNull.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/ipc/RandomAccessStreamParams.h"
#include "mozilla/ipc/RandomAccessStreamUtils.h"
#include "mozilla/gtest/MozAssertions.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIRandomAccessStream.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"

namespace mozilla::ipc {

namespace {

Result<nsCOMPtr<nsIRandomAccessStream>, nsresult> CreateFileStream() {
  nsCOMPtr<nsIFile> dir;
  nsresult rv =
      NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(dir));
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  nsCOMPtr<nsIFile> file;
  rv = dir->Clone(getter_AddRefs(file));
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  rv = file->Append(u"testfile"_ns);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  rv = file->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0666);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  nsCOMPtr<nsIRandomAccessStream> stream;
  rv = NS_NewLocalFileRandomAccessStream(getter_AddRefs(stream), file);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  return stream;
}

// Populate an array with the given number of bytes.  Data is lorem ipsum
// random text, but deterministic across multiple calls.
void CreateData(uint32_t aNumBytes, nsCString& aDataOut) {
  static const char data[] =
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec egestas "
      "purus eu condimentum iaculis. In accumsan leo eget odio porttitor, non "
      "rhoncus nulla vestibulum. Etiam lacinia consectetur nisl nec "
      "sollicitudin. Sed fringilla accumsan diam, pulvinar varius massa. Duis "
      "mollis dignissim felis, eget tempus nisi tristique ut. Fusce euismod, "
      "lectus non lacinia tempor, tellus diam suscipit quam, eget hendrerit "
      "lacus nunc fringilla ante. Sed ultrices massa vitae risus molestie, ut "
      "finibus quam laoreet nullam.";
  static const uint32_t dataLength = sizeof(data) - 1;

  aDataOut.SetCapacity(aNumBytes);

  while (aNumBytes > 0) {
    uint32_t amount = std::min(dataLength, aNumBytes);
    aDataOut.Append(data, amount);
    aNumBytes -= amount;
  }
}

// Synchronously consume the given input stream and validate the resulting data
// against the given string of expected values.
void ConsumeAndValidateStream(nsIInputStream* aStream,
                              const nsACString& aExpectedData) {
  uint64_t available = 0;
  nsresult rv = aStream->Available(&available);
  ASSERT_NS_SUCCEEDED(rv);
  ASSERT_EQ(available, aExpectedData.Length());

  nsAutoCString outputData;
  rv = NS_ConsumeStream(aStream, UINT32_MAX, outputData);
  ASSERT_NS_SUCCEEDED(rv);

  ASSERT_EQ(aExpectedData.Length(), outputData.Length());
  ASSERT_TRUE(aExpectedData.Equals(outputData));
}

}  // namespace

TEST(RandomAccessStreamUtils, NullRandomAccessStream_MaybeSerialize)
{
  nsCOMPtr<nsIRandomAccessStream> stream;

  Maybe<RandomAccessStreamParams> streamParams =
      SerializeRandomAccessStream(stream, nullptr);

  ASSERT_TRUE(streamParams.isNothing());

  auto res = DeserializeRandomAccessStream(streamParams);
  ASSERT_TRUE(res.isOk());

  nsCOMPtr<nsIRandomAccessStream> stream2 = res.unwrap();
  ASSERT_EQ(stream2, nullptr);
}

TEST(RandomAccessStreamUtils, FileRandomAccessStream_Serialize)
{
  const uint32_t dataSize = 256;

  auto res = CreateFileStream();
  ASSERT_TRUE(res.isOk());

  auto stream = res.unwrap();
  ASSERT_TRUE(stream);

  nsCOMPtr<nsIFileRandomAccessStream> fileStream = do_QueryInterface(stream);
  ASSERT_TRUE(fileStream);

  nsCString inputData;
  CreateData(dataSize, inputData);

  uint32_t numWritten = 0;
  nsresult rv = stream->OutputStream()->Write(inputData.BeginReading(),
                                              inputData.Length(), &numWritten);
  ASSERT_NS_SUCCEEDED(rv);
  ASSERT_EQ(numWritten, dataSize);

  RandomAccessStreamParams streamParams = SerializeRandomAccessStream(
      WrapMovingNotNullUnchecked(std::move(stream)), nullptr);

  ASSERT_EQ(streamParams.type(),
            RandomAccessStreamParams::TFileRandomAccessStreamParams);

  auto res2 = DeserializeRandomAccessStream(streamParams);
  ASSERT_TRUE(res2.isOk());

  NotNull<nsCOMPtr<nsIRandomAccessStream>> stream2 = res2.unwrap();

  nsCOMPtr<nsIFileRandomAccessStream> fileStream2 =
      do_QueryInterface(stream2.get());
  ASSERT_TRUE(fileStream2);

  int64_t offset;
  rv = stream2->Tell(&offset);
  ASSERT_NS_SUCCEEDED(rv);
  ASSERT_EQ(offset, dataSize);

  rv = stream2->Seek(nsISeekableStream::NS_SEEK_SET, 0);
  ASSERT_NS_SUCCEEDED(rv);

  ConsumeAndValidateStream(stream2->InputStream(), inputData);
}

TEST(RandomAccessStreamUtils, FileRandomAccessStream_MaybeSerialize)
{
  const uint32_t dataSize = 512;

  auto res = CreateFileStream();
  ASSERT_TRUE(res.isOk());

  auto stream = res.unwrap();
  ASSERT_TRUE(stream);

  nsCOMPtr<nsIFileRandomAccessStream> fileStream = do_QueryInterface(stream);
  ASSERT_TRUE(fileStream);

  nsCString inputData;
  CreateData(dataSize, inputData);

  uint32_t numWritten = 0;
  nsresult rv = stream->OutputStream()->Write(inputData.BeginReading(),
                                              inputData.Length(), &numWritten);
  ASSERT_NS_SUCCEEDED(rv);
  ASSERT_EQ(numWritten, dataSize);

  Maybe<RandomAccessStreamParams> streamParams =
      SerializeRandomAccessStream(stream, nullptr);

  ASSERT_TRUE(streamParams);
  ASSERT_EQ(streamParams->type(),
            RandomAccessStreamParams::TFileRandomAccessStreamParams);

  auto res2 = DeserializeRandomAccessStream(streamParams);
  ASSERT_TRUE(res2.isOk());

  nsCOMPtr<nsIRandomAccessStream> stream2 = res2.unwrap();
  ASSERT_TRUE(stream2);

  nsCOMPtr<nsIFileRandomAccessStream> fileStream2 = do_QueryInterface(stream2);
  ASSERT_TRUE(fileStream2);

  int64_t offset;
  rv = stream2->Tell(&offset);
  ASSERT_NS_SUCCEEDED(rv);
  ASSERT_EQ(offset, dataSize);

  rv = stream2->Seek(nsISeekableStream::NS_SEEK_SET, 0);
  ASSERT_NS_SUCCEEDED(rv);

  ConsumeAndValidateStream(stream2->InputStream(), inputData);
}

}  // namespace mozilla::ipc
