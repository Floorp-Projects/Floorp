/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AllocationPolicy_h_
#define AllocationPolicy_h_

#include <queue>
#include "MediaInfo.h"
#include "PlatformDecoderModule.h"
#include "TimeUnits.h"
#include "mozilla/MozPromise.h"
#include "mozilla/NotNull.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/StaticMutex.h"

namespace mozilla {

/**
 * Before calling PDMFactory::CreateDecoder(), Alloc() must be called on the
 * policy to get a token object as a permission to create a decoder. The
 * token should stay alive until Shutdown() is called on the decoder. The
 * destructor of the token will restore the decoder count so it is available
 * for next calls of Alloc().
 */
class AllocPolicy {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AllocPolicy)

 public:
  class Token {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Token)
   protected:
    virtual ~Token() = default;
  };
  using Promise = MozPromise<RefPtr<Token>, bool, true>;

  // Acquire a token for decoder creation. Thread-safe.
  virtual RefPtr<Promise> Alloc() = 0;

 protected:
  virtual ~AllocPolicy() = default;
};

/**
 * This is a singleton which controls the number of decoders that can be created
 * concurrently.
 * Instance() will return the TrackType global AllocPolicy.
 * Instance() will always return a non-null value.
 */
class GlobalAllocPolicy {
 public:
  // Get the singleton for the given track type. Thread-safe.
  static NotNull<AllocPolicy*> Instance(TrackInfo::TrackType aTrack);

 private:
  // Protect access to Instance().
  static StaticMutex sMutex;
};

/** This the actual base implementation underneath all AllocPolicy objects and
 * control how many decoders can be created concurrently.
 * Alloc() must be called to get a token object as a permission to perform an
 * action. The token should stay alive until Shutdown() is called on the
 * decoder. The destructor of the token will restore the decoder count so it is
 * available for next calls of Alloc().
 **/
class AllocPolicyImpl : public AllocPolicy {
 public:
  explicit AllocPolicyImpl(int aDecoderLimit);
  RefPtr<Promise> Alloc() override;

 protected:
  virtual ~AllocPolicyImpl();
  void RejectAll();
  int MaxDecoderLimit() const { return mMaxDecoderLimit; }

 private:
  class AutoDeallocToken;
  using PromisePrivate = Promise::Private;
  // Called by the destructor of TokenImpl to restore the decoder limit.
  void Dealloc();
  // Decrement the decoder limit and resolve a promise if available.
  void ResolvePromise(ReentrantMonitorAutoEnter& aProofOfLock);

  const int mMaxDecoderLimit;
  ReentrantMonitor mMonitor;
  // The number of decoders available for creation.
  int mDecoderLimit;
  // Requests to acquire tokens.
  std::queue<RefPtr<PromisePrivate>> mPromises;
};

/**
 * This class allows to track and serialise a single decoder allocation at a
 * time
 */
class SingleAllocPolicy : public AllocPolicyImpl {
  using TrackType = TrackInfo::TrackType;

 public:
  SingleAllocPolicy(TrackType aTrack, TaskQueue* aOwnerThread)
      : AllocPolicyImpl(1), mTrack(aTrack), mOwnerThread(aOwnerThread) {}

  RefPtr<Promise> Alloc() override;

  // Cancel the request to GlobalAllocPolicy and reject the current token
  // request. Note this must happen before mOwnerThread->BeginShutdown().
  void Cancel();

 private:
  class AutoDeallocCombinedToken;
  virtual ~SingleAllocPolicy();

  const TrackType mTrack;
  RefPtr<TaskQueue> mOwnerThread;
  MozPromiseHolder<Promise> mPendingPromise;
  MozPromiseRequestHolder<Promise> mTokenRequest;
};

class AllocationWrapper : public MediaDataDecoder {
  using Token = AllocPolicy::Token;

 public:
  AllocationWrapper(already_AddRefed<MediaDataDecoder> aDecoder,
                    already_AddRefed<Token> aToken);
  ~AllocationWrapper();

  RefPtr<InitPromise> Init() override { return mDecoder->Init(); }
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override {
    return mDecoder->Decode(aSample);
  }
  RefPtr<DecodePromise> Drain() override { return mDecoder->Drain(); }
  RefPtr<FlushPromise> Flush() override { return mDecoder->Flush(); }
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override {
    return mDecoder->IsHardwareAccelerated(aFailureReason);
  }
  nsCString GetDescriptionName() const override {
    return mDecoder->GetDescriptionName();
  }
  void SetSeekThreshold(const media::TimeUnit& aTime) override {
    mDecoder->SetSeekThreshold(aTime);
  }
  bool SupportDecoderRecycling() const override {
    return mDecoder->SupportDecoderRecycling();
  }
  RefPtr<ShutdownPromise> Shutdown() override;

  typedef MozPromise<RefPtr<MediaDataDecoder>, MediaResult,
                     /* IsExclusive = */ true>
      AllocateDecoderPromise;
  // Will create a decoder has soon as one can be created according to the
  // AllocPolicy (or GlobalAllocPolicy if aPolicy is null)
  // Warning: all aParams members must be valid until the promise has been
  // resolved, as some contains raw pointers to objects.
  static RefPtr<AllocateDecoderPromise> CreateDecoder(
      const CreateDecoderParams& aParams, AllocPolicy* aPolicy = nullptr);

 private:
  RefPtr<MediaDataDecoder> mDecoder;
  RefPtr<Token> mToken;
};

}  // namespace mozilla

#endif
