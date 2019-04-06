/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsIObserverService.h"
#include "mozilla/StaticPtr.h"
#include "GMPTestMonitor.h"
#include "GMPVideoDecoderProxy.h"
#include "GMPVideoEncoderProxy.h"
#include "GMPServiceParent.h"
#include "nsAppDirectoryServiceDefs.h"
#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"
#include "nsThreadUtils.h"

using namespace std;

using namespace mozilla;
using namespace mozilla::gmp;

struct GMPTestRunner {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPTestRunner)

  GMPTestRunner() {}
  void DoTest(void (GMPTestRunner::*aTestMethod)(GMPTestMonitor&));
  void RunTestGMPTestCodec1(GMPTestMonitor& aMonitor);
  void RunTestGMPTestCodec2(GMPTestMonitor& aMonitor);
  void RunTestGMPTestCodec3(GMPTestMonitor& aMonitor);
  void RunTestGMPCrossOrigin1(GMPTestMonitor& aMonitor);
  void RunTestGMPCrossOrigin2(GMPTestMonitor& aMonitor);
  void RunTestGMPCrossOrigin3(GMPTestMonitor& aMonitor);
  void RunTestGMPCrossOrigin4(GMPTestMonitor& aMonitor);

 private:
  ~GMPTestRunner() {}
};

template <class T, class Base,
          nsresult (NS_STDCALL GeckoMediaPluginService::*Getter)(
              GMPCrashHelper*, nsTArray<nsCString>*, const nsACString&,
              UniquePtr<Base>&&)>
class RunTestGMPVideoCodec : public Base {
 public:
  void Done(T* aGMP, GMPVideoHost* aHost) override {
    EXPECT_TRUE(aGMP);
    EXPECT_TRUE(aHost);
    if (aGMP) {
      aGMP->Close();
    }
    mMonitor.SetFinished();
  }

  static void Run(GMPTestMonitor& aMonitor, const nsCString& aOrigin) {
    UniquePtr<GMPCallbackType> callback(new RunTestGMPVideoCodec(aMonitor));
    Get(aOrigin, std::move(callback));
  }

 protected:
  typedef T GMPCodecType;
  typedef Base GMPCallbackType;

  explicit RunTestGMPVideoCodec(GMPTestMonitor& aMonitor)
      : mMonitor(aMonitor) {}

  static nsresult Get(const nsACString& aNodeId, UniquePtr<Base>&& aCallback) {
    nsTArray<nsCString> tags;
    tags.AppendElement(NS_LITERAL_CSTRING("h264"));
    tags.AppendElement(NS_LITERAL_CSTRING("fake"));

    RefPtr<GeckoMediaPluginService> service =
        GeckoMediaPluginService::GetGeckoMediaPluginService();
    return ((*service).*Getter)(nullptr, &tags, aNodeId, std::move(aCallback));
  }

 protected:
  GMPTestMonitor& mMonitor;
};

typedef RunTestGMPVideoCodec<GMPVideoDecoderProxy, GetGMPVideoDecoderCallback,
                             &GeckoMediaPluginService::GetGMPVideoDecoder>
    RunTestGMPVideoDecoder;
typedef RunTestGMPVideoCodec<GMPVideoEncoderProxy, GetGMPVideoEncoderCallback,
                             &GeckoMediaPluginService::GetGMPVideoEncoder>
    RunTestGMPVideoEncoder;

void GMPTestRunner::RunTestGMPTestCodec1(GMPTestMonitor& aMonitor) {
  RunTestGMPVideoDecoder::Run(aMonitor, NS_LITERAL_CSTRING("o"));
}

void GMPTestRunner::RunTestGMPTestCodec2(GMPTestMonitor& aMonitor) {
  RunTestGMPVideoDecoder::Run(aMonitor, NS_LITERAL_CSTRING(""));
}

void GMPTestRunner::RunTestGMPTestCodec3(GMPTestMonitor& aMonitor) {
  RunTestGMPVideoEncoder::Run(aMonitor, NS_LITERAL_CSTRING(""));
}

template <class Base>
class RunTestGMPCrossOrigin : public Base {
 public:
  void Done(typename Base::GMPCodecType* aGMP, GMPVideoHost* aHost) override {
    EXPECT_TRUE(aGMP);

    UniquePtr<typename Base::GMPCallbackType> callback(
        new Step2(Base::mMonitor, aGMP, mShouldBeEqual));
    nsresult rv = Base::Get(mOrigin2, std::move(callback));
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    if (NS_FAILED(rv)) {
      Base::mMonitor.SetFinished();
    }
  }

  static void Run(GMPTestMonitor& aMonitor, const nsCString& aOrigin1,
                  const nsCString& aOrigin2) {
    UniquePtr<typename Base::GMPCallbackType> callback(
        new RunTestGMPCrossOrigin<Base>(aMonitor, aOrigin1, aOrigin2));
    nsresult rv = Base::Get(aOrigin1, std::move(callback));
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    if (NS_FAILED(rv)) {
      aMonitor.SetFinished();
    }
  }

 private:
  RunTestGMPCrossOrigin(GMPTestMonitor& aMonitor, const nsCString& aOrigin1,
                        const nsCString& aOrigin2)
      : Base(aMonitor),
        mGMP(nullptr),
        mOrigin2(aOrigin2),
        mShouldBeEqual(aOrigin1.Equals(aOrigin2)) {}

  class Step2 : public Base {
   public:
    Step2(GMPTestMonitor& aMonitor, typename Base::GMPCodecType* aGMP,
          bool aShouldBeEqual)
        : Base(aMonitor), mGMP(aGMP), mShouldBeEqual(aShouldBeEqual) {}
    void Done(typename Base::GMPCodecType* aGMP, GMPVideoHost* aHost) override {
      EXPECT_TRUE(aGMP);
      if (aGMP) {
        EXPECT_TRUE(mGMP && (mGMP->GetPluginId() == aGMP->GetPluginId()) ==
                                mShouldBeEqual);
      }
      if (mGMP) {
        mGMP->Close();
      }
      Base::Done(aGMP, aHost);
    }

   private:
    typename Base::GMPCodecType* mGMP;
    bool mShouldBeEqual;
  };

  typename Base::GMPCodecType* mGMP;
  nsCString mOrigin2;
  bool mShouldBeEqual;
};

typedef RunTestGMPCrossOrigin<RunTestGMPVideoDecoder>
    RunTestGMPVideoDecoderCrossOrigin;
typedef RunTestGMPCrossOrigin<RunTestGMPVideoEncoder>
    RunTestGMPVideoEncoderCrossOrigin;

void GMPTestRunner::RunTestGMPCrossOrigin1(GMPTestMonitor& aMonitor) {
  RunTestGMPVideoDecoderCrossOrigin::Run(
      aMonitor, NS_LITERAL_CSTRING("origin1"), NS_LITERAL_CSTRING("origin2"));
}

void GMPTestRunner::RunTestGMPCrossOrigin2(GMPTestMonitor& aMonitor) {
  RunTestGMPVideoEncoderCrossOrigin::Run(
      aMonitor, NS_LITERAL_CSTRING("origin1"), NS_LITERAL_CSTRING("origin2"));
}

void GMPTestRunner::RunTestGMPCrossOrigin3(GMPTestMonitor& aMonitor) {
  RunTestGMPVideoDecoderCrossOrigin::Run(
      aMonitor, NS_LITERAL_CSTRING("origin1"), NS_LITERAL_CSTRING("origin1"));
}

void GMPTestRunner::RunTestGMPCrossOrigin4(GMPTestMonitor& aMonitor) {
  RunTestGMPVideoEncoderCrossOrigin::Run(
      aMonitor, NS_LITERAL_CSTRING("origin1"), NS_LITERAL_CSTRING("origin1"));
}

void GMPTestRunner::DoTest(
    void (GMPTestRunner::*aTestMethod)(GMPTestMonitor&)) {
  RefPtr<GeckoMediaPluginService> service =
      GeckoMediaPluginService::GetGeckoMediaPluginService();
  nsCOMPtr<nsIThread> thread;
  EXPECT_TRUE(NS_SUCCEEDED(service->GetThread(getter_AddRefs(thread))));

  GMPTestMonitor monitor;
  thread->Dispatch(NewRunnableMethod<GMPTestMonitor&>(
                       "GMPTestRunner::DoTest", this, aTestMethod, monitor),
                   NS_DISPATCH_NORMAL);
  monitor.AwaitFinished();
}

TEST(GeckoMediaPlugins, GMPTestCodec)
{
  RefPtr<GMPTestRunner> runner = new GMPTestRunner();
  runner->DoTest(&GMPTestRunner::RunTestGMPTestCodec1);
  runner->DoTest(&GMPTestRunner::RunTestGMPTestCodec2);
  runner->DoTest(&GMPTestRunner::RunTestGMPTestCodec3);
}

TEST(GeckoMediaPlugins, GMPCrossOrigin)
{
  RefPtr<GMPTestRunner> runner = new GMPTestRunner();
  runner->DoTest(&GMPTestRunner::RunTestGMPCrossOrigin1);
  runner->DoTest(&GMPTestRunner::RunTestGMPCrossOrigin2);
  runner->DoTest(&GMPTestRunner::RunTestGMPCrossOrigin3);
  runner->DoTest(&GMPTestRunner::RunTestGMPCrossOrigin4);
}
