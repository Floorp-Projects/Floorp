/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <mutex>

#include "mozilla/net/WebrtcTCPSocket.h"
#include "mozilla/net/WebrtcTCPSocketCallback.h"

#include "nsISocketTransport.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

static const uint32_t kDefaultTestTimeout = 2000;
static const char kReadData[] = "Hello, World!";
static const size_t kReadDataLength = sizeof(kReadData) - 1;
static const std::string kReadDataString =
    std::string(kReadData, kReadDataLength);
static int kDataLargeOuterLoopCount = 128;
static int kDataLargeInnerLoopCount = 1024;

namespace mozilla {

using namespace net;
using namespace testing;

class WebrtcTCPSocketTestCallback;

class FakeSocketTransportProvider : public nsISocketTransport {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  // nsISocketTransport
  NS_IMETHOD GetHost(nsACString& aHost) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetPort(int32_t* aPort) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetScriptableOriginAttributes(
      JSContext* cx, JS::MutableHandle<JS::Value> aOriginAttributes) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetScriptableOriginAttributes(
      JSContext* cx, JS::Handle<JS::Value> aOriginAttributes) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  virtual nsresult GetOriginAttributes(
      mozilla::OriginAttributes* _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  virtual nsresult SetOriginAttributes(
      const mozilla::OriginAttributes& aOriginAttrs) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetPeerAddr(mozilla::net::NetAddr* _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetSelfAddr(mozilla::net::NetAddr* _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD Bind(mozilla::net::NetAddr* aLocalAddr) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetScriptablePeerAddr(nsINetAddr** _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetScriptableSelfAddr(nsINetAddr** _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetTlsSocketControl(
      nsISSLSocketControl** aTLSSocketControl) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetSecurityCallbacks(
      nsIInterfaceRequestor** aSecurityCallbacks) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetSecurityCallbacks(
      nsIInterfaceRequestor* aSecurityCallbacks) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD IsAlive(bool* _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetTimeout(uint32_t aType, uint32_t* _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetTimeout(uint32_t aType, uint32_t aValue) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetLinger(bool aPolarity, int16_t aTimeout) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetReuseAddrPort(bool reuseAddrPort) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetConnectionFlags(uint32_t* aConnectionFlags) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetConnectionFlags(uint32_t aConnectionFlags) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetIsPrivate(bool) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetTlsFlags(uint32_t* aTlsFlags) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetTlsFlags(uint32_t aTlsFlags) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetQoSBits(uint8_t* aQoSBits) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetQoSBits(uint8_t aQoSBits) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetRecvBufferSize(uint32_t* aRecvBufferSize) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetSendBufferSize(uint32_t* aSendBufferSize) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetKeepaliveEnabled(bool* aKeepaliveEnabled) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetKeepaliveEnabled(bool aKeepaliveEnabled) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetKeepaliveVals(int32_t keepaliveIdleTime,
                              int32_t keepaliveRetryInterval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetResetIPFamilyPreference(
      bool* aResetIPFamilyPreference) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetEchConfigUsed(bool* aEchConfigUsed) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetEchConfig(const nsACString& aEchConfig) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD ResolvedByTRR(bool* _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetRetryDnsIfPossible(bool* aRetryDns) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetStatus(nsresult* aStatus) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }

  // nsITransport
  NS_IMETHOD OpenInputStream(uint32_t aFlags, uint32_t aSegmentSize,
                             uint32_t aSegmentCount,
                             nsIInputStream** _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD OpenOutputStream(uint32_t aFlags, uint32_t aSegmentSize,
                              uint32_t aSegmentCount,
                              nsIOutputStream** _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetEventSink(nsITransportEventSink* aSink,
                          nsIEventTarget* aEventTarget) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }

  // fake except for these methods which are OK to call
  // nsISocketTransport
  NS_IMETHOD SetRecvBufferSize(uint32_t aRecvBufferSize) override {
    return NS_OK;
  }
  NS_IMETHOD SetSendBufferSize(uint32_t aSendBufferSize) override {
    return NS_OK;
  }
  // nsITransport
  NS_IMETHOD Close(nsresult aReason) override { return NS_OK; }

 protected:
  virtual ~FakeSocketTransportProvider() = default;
};

NS_IMPL_ISUPPORTS(FakeSocketTransportProvider, nsISocketTransport, nsITransport)

// Implements some common elements to WebrtcTCPSocketTestOutputStream and
// WebrtcTCPSocketTestInputStream.
class WebrtcTCPSocketTestStream {
 public:
  WebrtcTCPSocketTestStream();

  void Fail() { mMustFail = true; }

  size_t DataLength();
  template <typename T>
  void AppendElements(const T* aBuffer, size_t aLength);

 protected:
  virtual ~WebrtcTCPSocketTestStream() = default;

  nsTArray<uint8_t> mData;
  std::mutex mDataMutex;

  bool mMustFail;
};

WebrtcTCPSocketTestStream::WebrtcTCPSocketTestStream() : mMustFail(false) {}

template <typename T>
void WebrtcTCPSocketTestStream::AppendElements(const T* aBuffer,
                                               size_t aLength) {
  std::lock_guard<std::mutex> guard(mDataMutex);
  mData.AppendElements(aBuffer, aLength);
}

size_t WebrtcTCPSocketTestStream::DataLength() {
  std::lock_guard<std::mutex> guard(mDataMutex);
  return mData.Length();
}

class WebrtcTCPSocketTestInputStream : public nsIAsyncInputStream,
                                       public WebrtcTCPSocketTestStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSIINPUTSTREAM

  WebrtcTCPSocketTestInputStream()
      : mMaxReadSize(1024 * 1024), mAllowCallbacks(false) {}

  void DoCallback();
  void CallCallback(const nsCOMPtr<nsIInputStreamCallback>& aCallback);
  void AllowCallbacks() { mAllowCallbacks = true; }

  size_t mMaxReadSize;

 protected:
  virtual ~WebrtcTCPSocketTestInputStream() = default;

 private:
  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackTarget;

  bool mAllowCallbacks;
};

NS_IMPL_ISUPPORTS(WebrtcTCPSocketTestInputStream, nsIAsyncInputStream,
                  nsIInputStream)

nsresult WebrtcTCPSocketTestInputStream::AsyncWait(
    nsIInputStreamCallback* aCallback, uint32_t aFlags,
    uint32_t aRequestedCount, nsIEventTarget* aEventTarget) {
  MOZ_ASSERT(!aEventTarget, "no event target should be set");

  mCallback = aCallback;
  mCallbackTarget = NS_GetCurrentThread();

  if (mAllowCallbacks && DataLength() > 0) {
    DoCallback();
  }

  return NS_OK;
}

nsresult WebrtcTCPSocketTestInputStream::CloseWithStatus(nsresult aStatus) {
  return Close();
}

nsresult WebrtcTCPSocketTestInputStream::Close() { return NS_OK; }

nsresult WebrtcTCPSocketTestInputStream::Available(uint64_t* aAvailable) {
  *aAvailable = DataLength();
  return NS_OK;
}

nsresult WebrtcTCPSocketTestInputStream::Read(char* aBuffer, uint32_t aCount,
                                              uint32_t* aRead) {
  std::lock_guard<std::mutex> guard(mDataMutex);
  if (mMustFail) {
    return NS_ERROR_FAILURE;
  }
  *aRead = std::min({(size_t)aCount, mData.Length(), mMaxReadSize});
  memcpy(aBuffer, mData.Elements(), *aRead);
  mData.RemoveElementsAt(0, *aRead);
  return *aRead > 0 ? NS_OK : NS_BASE_STREAM_WOULD_BLOCK;
}

nsresult WebrtcTCPSocketTestInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                                                      void* aClosure,
                                                      uint32_t aCount,
                                                      uint32_t* _retval) {
  MOZ_ASSERT(false);
  return NS_OK;
}

nsresult WebrtcTCPSocketTestInputStream::IsNonBlocking(bool* aIsNonBlocking) {
  *aIsNonBlocking = true;
  return NS_OK;
}

void WebrtcTCPSocketTestInputStream::CallCallback(
    const nsCOMPtr<nsIInputStreamCallback>& aCallback) {
  aCallback->OnInputStreamReady(this);
}

void WebrtcTCPSocketTestInputStream::DoCallback() {
  if (mCallback) {
    mCallbackTarget->Dispatch(
        NewRunnableMethod<const nsCOMPtr<nsIInputStreamCallback>&>(
            "WebrtcTCPSocketTestInputStream::DoCallback", this,
            &WebrtcTCPSocketTestInputStream::CallCallback,
            std::move(mCallback)));

    mCallbackTarget = nullptr;
  }
}

class WebrtcTCPSocketTestOutputStream : public nsIAsyncOutputStream,
                                        public WebrtcTCPSocketTestStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIASYNCOUTPUTSTREAM
  NS_DECL_NSIOUTPUTSTREAM

  WebrtcTCPSocketTestOutputStream() : mMaxWriteSize(1024 * 1024) {}

  void DoCallback();
  void CallCallback(const nsCOMPtr<nsIOutputStreamCallback>& aCallback);

  std::string DataString();

  uint32_t mMaxWriteSize;

 protected:
  virtual ~WebrtcTCPSocketTestOutputStream() = default;

 private:
  nsCOMPtr<nsIOutputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackTarget;
};

NS_IMPL_ISUPPORTS(WebrtcTCPSocketTestOutputStream, nsIAsyncOutputStream,
                  nsIOutputStream)

nsresult WebrtcTCPSocketTestOutputStream::AsyncWait(
    nsIOutputStreamCallback* aCallback, uint32_t aFlags,
    uint32_t aRequestedCount, nsIEventTarget* aEventTarget) {
  MOZ_ASSERT(!aEventTarget, "no event target should be set");

  mCallback = aCallback;
  mCallbackTarget = NS_GetCurrentThread();

  return NS_OK;
}

nsresult WebrtcTCPSocketTestOutputStream::CloseWithStatus(nsresult aStatus) {
  return Close();
}

nsresult WebrtcTCPSocketTestOutputStream::Close() { return NS_OK; }

nsresult WebrtcTCPSocketTestOutputStream::Flush() { return NS_OK; }

nsresult WebrtcTCPSocketTestOutputStream::Write(const char* aBuffer,
                                                uint32_t aCount,
                                                uint32_t* aWrote) {
  if (mMustFail) {
    return NS_ERROR_FAILURE;
  }
  *aWrote = std::min(aCount, mMaxWriteSize);
  AppendElements(aBuffer, *aWrote);
  return NS_OK;
}

nsresult WebrtcTCPSocketTestOutputStream::WriteSegments(
    nsReadSegmentFun aReader, void* aClosure, uint32_t aCount,
    uint32_t* _retval) {
  MOZ_ASSERT(false);
  return NS_OK;
}

nsresult WebrtcTCPSocketTestOutputStream::WriteFrom(nsIInputStream* aFromStream,
                                                    uint32_t aCount,
                                                    uint32_t* _retval) {
  MOZ_ASSERT(false);
  return NS_OK;
}

nsresult WebrtcTCPSocketTestOutputStream::IsNonBlocking(bool* aIsNonBlocking) {
  *aIsNonBlocking = true;
  return NS_OK;
}

void WebrtcTCPSocketTestOutputStream::CallCallback(
    const nsCOMPtr<nsIOutputStreamCallback>& aCallback) {
  aCallback->OnOutputStreamReady(this);
}

void WebrtcTCPSocketTestOutputStream::DoCallback() {
  if (mCallback) {
    mCallbackTarget->Dispatch(
        NewRunnableMethod<const nsCOMPtr<nsIOutputStreamCallback>&>(
            "WebrtcTCPSocketTestOutputStream::CallCallback", this,
            &WebrtcTCPSocketTestOutputStream::CallCallback,
            std::move(mCallback)));

    mCallbackTarget = nullptr;
  }
}

std::string WebrtcTCPSocketTestOutputStream::DataString() {
  std::lock_guard<std::mutex> guard(mDataMutex);
  return std::string((char*)mData.Elements(), mData.Length());
}

// Fake as in not the real WebrtcTCPSocket but real enough
class FakeWebrtcTCPSocket : public WebrtcTCPSocket {
 public:
  explicit FakeWebrtcTCPSocket(WebrtcTCPSocketCallback* aCallback)
      : WebrtcTCPSocket(aCallback) {}

 protected:
  virtual ~FakeWebrtcTCPSocket() = default;

  void InvokeOnClose(nsresult aReason) override;
  void InvokeOnConnected() override;
  void InvokeOnRead(nsTArray<uint8_t>&& aReadData) override;
};

void FakeWebrtcTCPSocket::InvokeOnClose(nsresult aReason) {
  mProxyCallbacks->OnClose(aReason);
}

void FakeWebrtcTCPSocket::InvokeOnConnected() {
  mProxyCallbacks->OnConnected("http"_ns);
}

void FakeWebrtcTCPSocket::InvokeOnRead(nsTArray<uint8_t>&& aReadData) {
  mProxyCallbacks->OnRead(std::move(aReadData));
}

class WebrtcTCPSocketTest : public MtransportTest {
 public:
  WebrtcTCPSocketTest()
      : MtransportTest(),
        mSocketThread(nullptr),
        mSocketTransport(nullptr),
        mInputStream(nullptr),
        mOutputStream(nullptr),
        mChannel(nullptr),
        mCallback(nullptr),
        mOnCloseCalled(false),
        mOnConnectedCalled(false) {}

  // WebrtcTCPSocketCallback forwards from mCallback
  void OnClose(nsresult aReason);
  void OnConnected(const nsACString& aProxyType);
  void OnRead(nsTArray<uint8_t>&& aReadData);

  void SetUp() override;
  void TearDown() override;

  void DoTransportAvailable();

  std::string ReadDataAsString();
  std::string GetDataLarge();

  nsCOMPtr<nsIEventTarget> mSocketThread;

  nsCOMPtr<nsISocketTransport> mSocketTransport;
  RefPtr<WebrtcTCPSocketTestInputStream> mInputStream;
  RefPtr<WebrtcTCPSocketTestOutputStream> mOutputStream;
  RefPtr<FakeWebrtcTCPSocket> mChannel;
  RefPtr<WebrtcTCPSocketTestCallback> mCallback;

  bool mOnCloseCalled;
  bool mOnConnectedCalled;

  size_t ReadDataLength();
  template <typename T>
  void AppendReadData(const T* aBuffer, size_t aLength);

 private:
  nsTArray<uint8_t> mReadData;
  std::mutex mReadDataMutex;
};

class WebrtcTCPSocketTestCallback : public WebrtcTCPSocketCallback {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebrtcTCPSocketTestCallback, override)

  explicit WebrtcTCPSocketTestCallback(WebrtcTCPSocketTest* aTest)
      : mTest(aTest) {}

  // WebrtcTCPSocketCallback
  void OnClose(nsresult aReason) override;
  void OnConnected(const nsACString& aProxyType) override;
  void OnRead(nsTArray<uint8_t>&& aReadData) override;

 protected:
  virtual ~WebrtcTCPSocketTestCallback() = default;

 private:
  WebrtcTCPSocketTest* mTest;
};

void WebrtcTCPSocketTest::SetUp() {
  nsresult rv;
  // WebrtcTCPSocket's threading model is the same as mtransport
  // all socket operations are done on the socket thread
  // callbacks are invoked on the main thread
  mSocketThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  mSocketTransport = new FakeSocketTransportProvider();
  mInputStream = new WebrtcTCPSocketTestInputStream();
  mOutputStream = new WebrtcTCPSocketTestOutputStream();
  mCallback = new WebrtcTCPSocketTestCallback(this);
  mChannel = new FakeWebrtcTCPSocket(mCallback.get());
}

void WebrtcTCPSocketTest::TearDown() {}

// WebrtcTCPSocketCallback
void WebrtcTCPSocketTest::OnRead(nsTArray<uint8_t>&& aReadData) {
  AppendReadData(aReadData.Elements(), aReadData.Length());
}

void WebrtcTCPSocketTest::OnConnected(const nsACString& aProxyType) {
  mOnConnectedCalled = true;
}

void WebrtcTCPSocketTest::OnClose(nsresult aReason) { mOnCloseCalled = true; }

void WebrtcTCPSocketTest::DoTransportAvailable() {
  if (!mSocketThread->IsOnCurrentThread()) {
    mSocketThread->Dispatch(
        NS_NewRunnableFunction("DoTransportAvailable", [this]() -> void {
          nsresult rv;
          rv = mChannel->OnTransportAvailable(mSocketTransport, mInputStream,
                                              mOutputStream);
          ASSERT_EQ(NS_OK, rv);
        }));
  } else {
    // should always be called on the main thread
    MOZ_ASSERT(0);
  }
}

std::string WebrtcTCPSocketTest::ReadDataAsString() {
  std::lock_guard<std::mutex> guard(mReadDataMutex);
  return std::string((char*)mReadData.Elements(), mReadData.Length());
}

std::string WebrtcTCPSocketTest::GetDataLarge() {
  std::string data;
  for (int i = 0; i < kDataLargeOuterLoopCount * kDataLargeInnerLoopCount;
       ++i) {
    data += kReadData;
  }
  return data;
}

template <typename T>
void WebrtcTCPSocketTest::AppendReadData(const T* aBuffer, size_t aLength) {
  std::lock_guard<std::mutex> guard(mReadDataMutex);
  mReadData.AppendElements(aBuffer, aLength);
}

size_t WebrtcTCPSocketTest::ReadDataLength() {
  std::lock_guard<std::mutex> guard(mReadDataMutex);
  return mReadData.Length();
}

void WebrtcTCPSocketTestCallback::OnClose(nsresult aReason) {
  mTest->OnClose(aReason);
}

void WebrtcTCPSocketTestCallback::OnConnected(const nsACString& aProxyType) {
  mTest->OnConnected(aProxyType);
}

void WebrtcTCPSocketTestCallback::OnRead(nsTArray<uint8_t>&& aReadData) {
  mTest->OnRead(std::move(aReadData));
}

}  // namespace mozilla

typedef mozilla::WebrtcTCPSocketTest WebrtcTCPSocketTest;

TEST_F(WebrtcTCPSocketTest, SetUp) {}

TEST_F(WebrtcTCPSocketTest, TransportAvailable) {
  DoTransportAvailable();
  ASSERT_TRUE_WAIT(mOnConnectedCalled, kDefaultTestTimeout);
}

TEST_F(WebrtcTCPSocketTest, Read) {
  DoTransportAvailable();
  ASSERT_TRUE_WAIT(mOnConnectedCalled, kDefaultTestTimeout);

  mInputStream->AppendElements(kReadData, kReadDataLength);
  mInputStream->DoCallback();

  ASSERT_TRUE_WAIT(ReadDataAsString() == kReadDataString, kDefaultTestTimeout);
}

TEST_F(WebrtcTCPSocketTest, Write) {
  DoTransportAvailable();
  ASSERT_TRUE_WAIT(mOnConnectedCalled, kDefaultTestTimeout);

  nsTArray<uint8_t> data;
  data.AppendElements(kReadData, kReadDataLength);
  mChannel->Write(std::move(data));

  ASSERT_TRUE_WAIT(mChannel->CountUnwrittenBytes() == kReadDataLength,
                   kDefaultTestTimeout);

  mOutputStream->DoCallback();

  ASSERT_TRUE_WAIT(mOutputStream->DataString() == kReadDataString,
                   kDefaultTestTimeout);
}

TEST_F(WebrtcTCPSocketTest, ReadFail) {
  DoTransportAvailable();
  ASSERT_TRUE_WAIT(mOnConnectedCalled, kDefaultTestTimeout);

  mInputStream->AppendElements(kReadData, kReadDataLength);
  mInputStream->Fail();
  mInputStream->DoCallback();

  ASSERT_TRUE_WAIT(mOnCloseCalled, kDefaultTestTimeout);
  ASSERT_EQ(0U, ReadDataLength());
}

TEST_F(WebrtcTCPSocketTest, WriteFail) {
  DoTransportAvailable();
  ASSERT_TRUE_WAIT(mOnConnectedCalled, kDefaultTestTimeout);

  nsTArray<uint8_t> array;
  array.AppendElements(kReadData, kReadDataLength);
  mChannel->Write(std::move(array));

  ASSERT_TRUE_WAIT(mChannel->CountUnwrittenBytes() == kReadDataLength,
                   kDefaultTestTimeout);

  mOutputStream->Fail();
  mOutputStream->DoCallback();

  ASSERT_TRUE_WAIT(mOnCloseCalled, kDefaultTestTimeout);
  ASSERT_EQ(0U, mOutputStream->DataLength());
}

TEST_F(WebrtcTCPSocketTest, ReadLarge) {
  DoTransportAvailable();
  ASSERT_TRUE_WAIT(mOnConnectedCalled, kDefaultTestTimeout);

  const std::string data = GetDataLarge();

  mInputStream->AppendElements(data.c_str(), data.length());
  // make sure reading loops more than once
  mInputStream->mMaxReadSize = 3072;
  mInputStream->AllowCallbacks();
  mInputStream->DoCallback();

  ASSERT_TRUE_WAIT(ReadDataAsString() == data, kDefaultTestTimeout);
}

TEST_F(WebrtcTCPSocketTest, WriteLarge) {
  DoTransportAvailable();
  ASSERT_TRUE_WAIT(mOnConnectedCalled, kDefaultTestTimeout);

  const std::string data = GetDataLarge();

  for (int i = 0; i < kDataLargeOuterLoopCount; ++i) {
    nsTArray<uint8_t> array;
    int chunkSize = kReadDataString.length() * kDataLargeInnerLoopCount;
    int offset = i * chunkSize;
    array.AppendElements(data.c_str() + offset, chunkSize);
    mChannel->Write(std::move(array));
  }

  ASSERT_TRUE_WAIT(mChannel->CountUnwrittenBytes() == data.length(),
                   kDefaultTestTimeout);

  // make sure writing loops more than once per write request
  mOutputStream->mMaxWriteSize = 1024;
  mOutputStream->DoCallback();

  ASSERT_TRUE_WAIT(mOutputStream->DataString() == data, kDefaultTestTimeout);
}
