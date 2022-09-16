/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "chrome/common/ipc_message.h"
#include "gtest/gtest.h"

#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/ipc/DataPipe.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"

namespace mozilla::ipc {

namespace {

struct InputStreamCallback : public nsIInputStreamCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit InputStreamCallback(
      std::function<nsresult(nsIAsyncInputStream*)> aFunc = nullptr)
      : mFunc(std::move(aFunc)) {}

  NS_IMETHOD OnInputStreamReady(nsIAsyncInputStream* aStream) override {
    MOZ_ALWAYS_FALSE(mCalled.exchange(true));
    return mFunc ? mFunc(aStream) : NS_OK;
  }

  bool Called() const { return mCalled; }

 private:
  virtual ~InputStreamCallback() = default;

  std::atomic<bool> mCalled = false;
  std::function<nsresult(nsIAsyncInputStream*)> mFunc;
};

NS_IMPL_ISUPPORTS(InputStreamCallback, nsIInputStreamCallback)

struct OutputStreamCallback : public nsIOutputStreamCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit OutputStreamCallback(
      std::function<nsresult(nsIAsyncOutputStream*)> aFunc = nullptr)
      : mFunc(std::move(aFunc)) {}

  NS_IMETHOD OnOutputStreamReady(nsIAsyncOutputStream* aStream) override {
    MOZ_ALWAYS_FALSE(mCalled.exchange(true));
    return mFunc ? mFunc(aStream) : NS_OK;
  }

  bool Called() const { return mCalled; }

 private:
  virtual ~OutputStreamCallback() = default;

  std::atomic<bool> mCalled = false;
  std::function<nsresult(nsIAsyncOutputStream*)> mFunc;
};

NS_IMPL_ISUPPORTS(OutputStreamCallback, nsIOutputStreamCallback)

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
  nsAutoCString outputData;
  nsresult rv = NS_ConsumeStream(aStream, UINT32_MAX, outputData);
  ASSERT_NS_SUCCEEDED(rv);
  ASSERT_EQ(aExpectedData.Length(), outputData.Length());
  ASSERT_TRUE(aExpectedData.Equals(outputData));
}

}  // namespace

TEST(DataPipe, SegmentedReadWrite)
{
  RefPtr<DataPipeReceiver> reader;
  RefPtr<DataPipeSender> writer;

  nsresult rv =
      NewDataPipe(1024, getter_AddRefs(writer), getter_AddRefs(reader));
  ASSERT_NS_SUCCEEDED(rv);

  nsCString inputData1;
  CreateData(512, inputData1);

  uint32_t numWritten = 0;
  rv = writer->Write(inputData1.BeginReading(), inputData1.Length(),
                     &numWritten);
  ASSERT_NS_SUCCEEDED(rv);
  EXPECT_EQ(numWritten, 512u);

  uint64_t available = 0;
  rv = reader->Available(&available);
  EXPECT_EQ(available, 512u);
  ConsumeAndValidateStream(reader, inputData1);

  nsCString inputData2;
  CreateData(1024, inputData2);

  rv = writer->Write(inputData2.BeginReading(), inputData2.Length(),
                     &numWritten);
  ASSERT_NS_SUCCEEDED(rv);
  EXPECT_EQ(numWritten, 1024u);

  rv = reader->Available(&available);
  EXPECT_EQ(available, 1024u);
  ConsumeAndValidateStream(reader, inputData2);
}

TEST(DataPipe, SegmentedPartialRead)
{
  RefPtr<DataPipeReceiver> reader;
  RefPtr<DataPipeSender> writer;

  nsresult rv =
      NewDataPipe(1024, getter_AddRefs(writer), getter_AddRefs(reader));
  ASSERT_NS_SUCCEEDED(rv);

  nsCString inputData1;
  CreateData(512, inputData1);

  uint32_t numWritten = 0;
  rv = writer->Write(inputData1.BeginReading(), inputData1.Length(),
                     &numWritten);
  ASSERT_NS_SUCCEEDED(rv);
  EXPECT_EQ(numWritten, 512u);

  uint64_t available = 0;
  rv = reader->Available(&available);
  EXPECT_EQ(available, 512u);
  ConsumeAndValidateStream(reader, inputData1);

  nsCString inputData2;
  CreateData(1024, inputData2);

  rv = writer->Write(inputData2.BeginReading(), inputData2.Length(),
                     &numWritten);
  ASSERT_NS_SUCCEEDED(rv);
  EXPECT_EQ(numWritten, 1024u);

  rv = reader->Available(&available);
  EXPECT_EQ(available, 1024u);

  nsAutoCString outputData;
  rv = NS_ReadInputStreamToString(reader, outputData, 768);
  ASSERT_NS_SUCCEEDED(rv);
  ASSERT_EQ(768u, outputData.Length());
  ASSERT_TRUE(Substring(inputData2, 0, 768).Equals(outputData));

  rv = reader->Available(&available);
  EXPECT_EQ(available, 256u);

  nsAutoCString outputData2;
  rv = NS_ReadInputStreamToString(reader, outputData2, 256);
  ASSERT_NS_SUCCEEDED(rv);
  ASSERT_EQ(256u, outputData2.Length());
  ASSERT_TRUE(Substring(inputData2, 768).Equals(outputData2));
}

TEST(DataPipe, Write_AsyncWait)
{
  RefPtr<DataPipeReceiver> reader;
  RefPtr<DataPipeSender> writer;

  const uint32_t segmentSize = 1024;

  nsresult rv =
      NewDataPipe(segmentSize, getter_AddRefs(writer), getter_AddRefs(reader));
  ASSERT_NS_SUCCEEDED(rv);

  nsCString inputData;
  CreateData(segmentSize, inputData);

  uint32_t numWritten = 0;
  rv = writer->Write(inputData.BeginReading(), inputData.Length(), &numWritten);
  ASSERT_NS_SUCCEEDED(rv);
  EXPECT_EQ(numWritten, segmentSize);

  rv = writer->Write(inputData.BeginReading(), inputData.Length(), &numWritten);
  ASSERT_EQ(NS_BASE_STREAM_WOULD_BLOCK, rv);

  RefPtr<OutputStreamCallback> cb = new OutputStreamCallback();

  rv = writer->AsyncWait(cb, 0, 0, GetCurrentSerialEventTarget());
  ASSERT_NS_SUCCEEDED(rv);

  NS_ProcessPendingEvents(nullptr);

  ASSERT_FALSE(cb->Called());

  ConsumeAndValidateStream(reader, inputData);

  ASSERT_FALSE(cb->Called());

  NS_ProcessPendingEvents(nullptr);

  ASSERT_TRUE(cb->Called());
}

TEST(DataPipe, Read_AsyncWait)
{
  RefPtr<DataPipeReceiver> reader;
  RefPtr<DataPipeSender> writer;

  const uint32_t segmentSize = 1024;

  nsresult rv =
      NewDataPipe(segmentSize, getter_AddRefs(writer), getter_AddRefs(reader));
  ASSERT_NS_SUCCEEDED(rv);

  nsCString inputData;
  CreateData(segmentSize, inputData);

  RefPtr<InputStreamCallback> cb = new InputStreamCallback();

  rv = reader->AsyncWait(cb, 0, 0, GetCurrentSerialEventTarget());
  ASSERT_NS_SUCCEEDED(rv);

  NS_ProcessPendingEvents(nullptr);

  ASSERT_FALSE(cb->Called());

  uint32_t numWritten = 0;
  rv = writer->Write(inputData.BeginReading(), inputData.Length(), &numWritten);
  ASSERT_NS_SUCCEEDED(rv);

  ASSERT_FALSE(cb->Called());

  NS_ProcessPendingEvents(nullptr);

  ASSERT_TRUE(cb->Called());

  ConsumeAndValidateStream(reader, inputData);
}

TEST(DataPipe, Write_AsyncWait_Cancel)
{
  RefPtr<DataPipeReceiver> reader;
  RefPtr<DataPipeSender> writer;

  const uint32_t segmentSize = 1024;

  nsresult rv =
      NewDataPipe(segmentSize, getter_AddRefs(writer), getter_AddRefs(reader));
  ASSERT_NS_SUCCEEDED(rv);

  nsCString inputData;
  CreateData(segmentSize, inputData);

  uint32_t numWritten = 0;
  rv = writer->Write(inputData.BeginReading(), inputData.Length(), &numWritten);
  ASSERT_NS_SUCCEEDED(rv);
  EXPECT_EQ(numWritten, segmentSize);

  rv = writer->Write(inputData.BeginReading(), inputData.Length(), &numWritten);
  ASSERT_EQ(NS_BASE_STREAM_WOULD_BLOCK, rv);

  RefPtr<OutputStreamCallback> cb = new OutputStreamCallback();

  // Register a callback and immediately cancel it.
  rv = writer->AsyncWait(cb, 0, 0, GetCurrentSerialEventTarget());
  ASSERT_NS_SUCCEEDED(rv);
  rv = writer->AsyncWait(nullptr, 0, 0, nullptr);
  ASSERT_NS_SUCCEEDED(rv);

  // Even after consuming the stream and processing pending events, the callback
  // shouldn't be called as it was cancelled.
  ConsumeAndValidateStream(reader, inputData);
  NS_ProcessPendingEvents(nullptr);
  ASSERT_FALSE(cb->Called());
}

TEST(DataPipe, Read_AsyncWait_Cancel)
{
  RefPtr<DataPipeReceiver> reader;
  RefPtr<DataPipeSender> writer;

  const uint32_t segmentSize = 1024;

  nsresult rv =
      NewDataPipe(segmentSize, getter_AddRefs(writer), getter_AddRefs(reader));
  ASSERT_NS_SUCCEEDED(rv);

  nsCString inputData;
  CreateData(segmentSize, inputData);

  RefPtr<InputStreamCallback> cb = new InputStreamCallback();

  // Register a callback and immediately cancel it.
  rv = reader->AsyncWait(cb, 0, 0, GetCurrentSerialEventTarget());
  ASSERT_NS_SUCCEEDED(rv);

  rv = reader->AsyncWait(nullptr, 0, 0, nullptr);
  ASSERT_NS_SUCCEEDED(rv);

  // Write data into the pipe to make the callback become ready.
  uint32_t numWritten = 0;
  rv = writer->Write(inputData.BeginReading(), inputData.Length(), &numWritten);
  ASSERT_NS_SUCCEEDED(rv);

  // Even after processing pending events, the callback shouldn't be called as
  // it was cancelled.
  NS_ProcessPendingEvents(nullptr);
  ASSERT_FALSE(cb->Called());

  ConsumeAndValidateStream(reader, inputData);
}

TEST(DataPipe, SerializeReader)
{
  RefPtr<DataPipeReceiver> reader;
  RefPtr<DataPipeSender> writer;
  nsresult rv =
      NewDataPipe(1024, getter_AddRefs(writer), getter_AddRefs(reader));
  ASSERT_NS_SUCCEEDED(rv);

  IPC::Message msg(MSG_ROUTING_NONE, 0);
  IPC::MessageWriter msgWriter(msg);
  IPC::WriteParam(&msgWriter, reader);

  uint64_t available = 0;
  rv = reader->Available(&available);
  ASSERT_NS_FAILED(rv);

  nsCString inputData;
  CreateData(512, inputData);

  uint32_t numWritten = 0;
  rv = writer->Write(inputData.BeginReading(), inputData.Length(), &numWritten);
  ASSERT_NS_SUCCEEDED(rv);

  RefPtr<DataPipeReceiver> reader2;
  IPC::MessageReader msgReader(msg);
  ASSERT_TRUE(IPC::ReadParam(&msgReader, &reader2));
  ASSERT_TRUE(reader2);

  rv = reader2->Available(&available);
  ASSERT_NS_SUCCEEDED(rv);
  ASSERT_EQ(available, 512u);
  ConsumeAndValidateStream(reader2, inputData);
}

}  // namespace mozilla::ipc
