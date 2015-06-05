/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GMPVideoDecoderTrialCreator_h
#define mozilla_dom_GMPVideoDecoderTrialCreator_h

#include "mozilla/dom/MediaKeySystemAccess.h"
#include "mozilla/Pair.h"
#include "nsIObserver.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nsITimer.h"
#include "nsClassHashtable.h"
#include "mozilla/dom/MediaKeys.h"
#include "GMPVideoDecoderProxy.h"

namespace mozilla {
namespace dom {

class TestGMPVideoDecoder;

class GMPVideoDecoderTrialCreator {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPVideoDecoderTrialCreator);

  void MaybeAwaitTrialCreate(const nsAString& aKeySystem,
                             MediaKeySystemAccess* aAccess,
                             Promise* aPromise,
                             nsPIDOMWindow* aParent);

  void TrialCreateGMPVideoDecoderFailed(const nsAString& aKeySystem);
  void TrialCreateGMPVideoDecoderSucceeded(const nsAString& aKeySystem);

private:

  ~GMPVideoDecoderTrialCreator() {}

  // Note: Keep this in sync with GetCreateTrialState.
  enum TrialCreateState {
    Pending = 0,
    Succeeded = 1,
    Failed = 2,
  };

  static TrialCreateState GetCreateTrialState(const nsAString& aKeySystem);

  typedef Pair<nsRefPtr<Promise>, nsRefPtr<MediaKeySystemAccess>> PromiseAccessPair;

  struct TrialCreateData {
    TrialCreateData(const nsAString& aKeySystem)
      : mKeySystem(aKeySystem)
      , mStatus(GetCreateTrialState(aKeySystem))
    {}
    ~TrialCreateData() {}
    const nsString mKeySystem;
    nsRefPtr<TestGMPVideoDecoder> mTest;
    nsTArray<PromiseAccessPair> mPending;
    TrialCreateState mStatus;
  private:
    TrialCreateData(const TrialCreateData& aOther) = delete;
    TrialCreateData() = delete;
    TrialCreateData& operator =(const TrialCreateData&) = delete;
  };

  nsClassHashtable<nsStringHashKey, TrialCreateData> mTestCreate;

};

class TestGMPVideoDecoder : public GMPVideoDecoderCallbackProxy {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestGMPVideoDecoder);

  TestGMPVideoDecoder(GMPVideoDecoderTrialCreator* aInstance,
                      const nsAString& aKeySystem,
                      nsPIDOMWindow* aParent)
    : mKeySystem(aKeySystem)
    , mInstance(aInstance)
    , mWindow(aParent)
    , mGMP(nullptr)
    , mHost(nullptr)
    , mReceivedDecoded(false)
  {}

  nsresult Start();

  // GMPVideoDecoderCallbackProxy
  virtual void Decoded(GMPVideoi420Frame* aDecodedFrame) override;
  virtual void ReceivedDecodedReferenceFrame(const uint64_t aPictureId) override {}
  virtual void ReceivedDecodedFrame(const uint64_t aPictureId) override {}
  virtual void InputDataExhausted() override {}
  virtual void DrainComplete() override;
  virtual void ResetComplete() override {}
  virtual void Error(GMPErr aErr) override;
  virtual void Terminated() override;

  void ActorCreated(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost); // Main thread.

  class Callback : public GetGMPVideoDecoderCallback
  {
  public:
    Callback(TestGMPVideoDecoder* aInstance)
      : mInstance(aInstance)
    {}
    ~Callback() {}
    void Done(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost) override;
  private:
    nsRefPtr<TestGMPVideoDecoder> mInstance;
  };

private:

  void InitGMPDone(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost); // GMP thread.
  void CreateGMPVideoDecoder();
  ~TestGMPVideoDecoder() {}

  void ReportFailure();
  void ReportSuccess();

  const nsString mKeySystem;
  nsCOMPtr<mozIGeckoMediaPluginService> mGMPService;

  nsRefPtr<GMPVideoDecoderTrialCreator> mInstance;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  GMPVideoDecoderProxy* mGMP;
  GMPVideoHost* mHost;
  bool mReceivedDecoded;
};

} // namespace dom
} // namespace mozilla

#endif