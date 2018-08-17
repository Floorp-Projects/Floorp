/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AllocationPolicy_h_
#define AllocationPolicy_h_

#include "MediaInfo.h"
#include "PlatformDecoderModule.h"
#include "TimeUnits.h"
#include "mozilla/MozPromise.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/ReentrantMonitor.h"
#include <queue>

namespace mozilla {

/**
 * This is a singleton which controls the number of decoders that can be
 * created concurrently. Before calling PDMFactory::CreateDecoder(), Alloc()
 * must be called to get a token object as a permission to create a decoder.
 * The token should stay alive until Shutdown() is called on the decoder.
 * The destructor of the token will restore the decoder count so it is available
 * for next calls of Alloc().
 */
class GlobalAllocPolicy
{
public:
  class Token
  {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Token)
  protected:
    virtual ~Token() {}
  };

  using Promise = MozPromise<RefPtr<Token>, bool, true>;

  // Acquire a token for decoder creation. Thread-safe.
  RefPtr<Promise> Alloc();

  // Called by ClearOnShutdown() to delete the singleton.
  void operator=(decltype(nullptr));

  // Get the singleton for the given track type. Thread-safe.
  static GlobalAllocPolicy& Instance(TrackInfo::TrackType aTrack);

private:
  class AutoDeallocToken;
  using PromisePrivate = Promise::Private;
  GlobalAllocPolicy();
  ~GlobalAllocPolicy();
  // Called by the destructor of TokenImpl to restore the decoder limit.
  void Dealloc();
  // Decrement the decoder limit and resolve a promise if available.
  void ResolvePromise(ReentrantMonitorAutoEnter& aProofOfLock);

  // Protect access to Instance().
  static StaticMutex sMutex;

  ReentrantMonitor mMonitor;
  // The number of decoders available for creation.
  int mDecoderLimit;
  // Requests to acquire tokens.
  std::queue<RefPtr<PromisePrivate>> mPromises;
};

class AllocationWrapper : public MediaDataDecoder
{
  using Token = GlobalAllocPolicy::Token;

public:
  AllocationWrapper(already_AddRefed<MediaDataDecoder> aDecoder,
                    already_AddRefed<Token> aToken);
  ~AllocationWrapper();

  RefPtr<InitPromise> Init() override { return mDecoder->Init(); }
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override
  {
    return mDecoder->Decode(aSample);
  }
  RefPtr<DecodePromise> Drain() override { return mDecoder->Drain(); }
  RefPtr<FlushPromise> Flush() override { return mDecoder->Flush(); }
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override
  {
    return mDecoder->IsHardwareAccelerated(aFailureReason);
  }
  nsCString GetDescriptionName() const override
  {
    return mDecoder->GetDescriptionName();
  }
  void SetSeekThreshold(const media::TimeUnit& aTime) override
  {
    mDecoder->SetSeekThreshold(aTime);
  }
  bool SupportDecoderRecycling() const override
  {
    return mDecoder->SupportDecoderRecycling();
  }
  RefPtr<ShutdownPromise> Shutdown() override;

private:
  RefPtr<MediaDataDecoder> mDecoder;
  RefPtr<Token> mToken;
};

} // namespace mozilla

#endif
