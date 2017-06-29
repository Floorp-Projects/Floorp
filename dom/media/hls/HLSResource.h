/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HLSResource_h_
#define HLSResource_h_

#include "GeneratedJNINatives.h"
#include "GeneratedJNIWrappers.h"
#include "HLSUtils.h"
#include "nsContentUtils.h"

#define UNIMPLEMENTED() HLS_DEBUG("HLSResource", "UNIMPLEMENTED FUNCTION")

using namespace mozilla::java;

namespace mozilla {

class HLSResource;

class HLSResourceCallbacksSupport
  : public GeckoHLSResourceWrapper::Callbacks::Natives<HLSResourceCallbacksSupport>
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(HLSResourceCallbacksSupport)
public:
  typedef GeckoHLSResourceWrapper::Callbacks::Natives<HLSResourceCallbacksSupport> NativeCallbacks;
  using NativeCallbacks::DisposeNative;
  using NativeCallbacks::AttachNative;

  HLSResourceCallbacksSupport(HLSResource* aResource);
  void Detach();
  void OnDataArrived();
  void OnError(int aErrorCode);

private:
  ~HLSResourceCallbacksSupport() {}
  HLSResource* mResource;
};

class HLSResource final : public MediaResource
{
public:
  HLSResource(MediaResourceCallback* aCallback,
              nsIChannel* aChannel,
              nsIURI* aURI);
  ~HLSResource();
  nsresult Close() override { return NS_OK; }
  void Suspend(bool aCloseImmediately) override { UNIMPLEMENTED(); }
  void Resume() override { UNIMPLEMENTED(); }
  void SetReadMode(MediaCacheStream::ReadMode aMode) override { UNIMPLEMENTED(); }
  void SetPlaybackRate(uint32_t aBytesPerSecond) override  { UNIMPLEMENTED(); }
  nsresult ReadAt(int64_t aOffset, char* aBuffer, uint32_t aCount, uint32_t* aBytes) override { UNIMPLEMENTED(); return NS_ERROR_FAILURE; }
  bool ShouldCacheReads() override { UNIMPLEMENTED(); return false; }
  int64_t Tell() override { UNIMPLEMENTED(); return -1; }
  void Pin() override { UNIMPLEMENTED(); }
  void Unpin() override { UNIMPLEMENTED(); }
  double GetDownloadRate(bool* aIsReliable) override { UNIMPLEMENTED(); *aIsReliable = false; return 0; }
  int64_t GetLength() override { UNIMPLEMENTED(); return -1; }
  int64_t GetNextCachedData(int64_t aOffset) override { UNIMPLEMENTED(); return -1; }
  int64_t GetCachedDataEnd(int64_t aOffset) override { UNIMPLEMENTED(); return -1; }
  bool IsDataCachedToEndOfResource(int64_t aOffset) override { UNIMPLEMENTED(); return false; }
  bool IsSuspendedByCache() override { UNIMPLEMENTED(); return false; }
  bool IsSuspended() override { UNIMPLEMENTED(); return false; }
  nsresult ReadFromCache(char* aBuffer, int64_t aOffset, uint32_t aCount) override { UNIMPLEMENTED(); return NS_ERROR_FAILURE; }
  nsresult Open(nsIStreamListener** aStreamListener) override { UNIMPLEMENTED(); return NS_OK; }

  already_AddRefed<nsIPrincipal> GetCurrentPrincipal() override
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

    nsCOMPtr<nsIPrincipal> principal;
    nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
    if (!secMan || !mChannel)
      return nullptr;
    secMan->GetChannelResultPrincipal(mChannel, getter_AddRefs(principal));
    return principal.forget();
  }

  nsresult GetCachedRanges(MediaByteRangeSet& aRanges) override
  {
    UNIMPLEMENTED();
    return NS_OK;
  }

  bool IsTransportSeekable() override { return true; }

  bool IsLiveStream() override
  {
    return false;
  }

  bool IsExpectingMoreData() override
  {
    return false;
  }

  java::GeckoHLSResourceWrapper::GlobalRef GetResourceWrapper() {
    return mHLSResourceWrapper;
  }

private:
  friend class HLSResourceCallbacksSupport;

  void onDataAvailable();
  void onError(int aErrorCode);

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t size = MediaResource::SizeOfExcludingThis(aMallocSizeOf);
    return size;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  RefPtr<MediaResourceCallback> mCallback;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIURI> mURI;
  java::GeckoHLSResourceWrapper::GlobalRef mHLSResourceWrapper;
  java::GeckoHLSResourceWrapper::Callbacks::GlobalRef mJavaCallbacks;
  RefPtr<HLSResourceCallbacksSupport> mCallbackSupport;
};

} // namespace mozilla
#endif /* HLSResource_h_ */
