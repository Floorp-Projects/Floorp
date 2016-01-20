/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASOURCERESOURCE_H_
#define MOZILLA_MEDIASOURCERESOURCE_H_

#include "MediaResource.h"
#include "mozilla/Monitor.h"
#include "mozilla/Logging.h"

extern mozilla::LogModule* GetMediaSourceLog();

#define MSE_DEBUG(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Debug, ("MediaSourceResource(%p:%s)::%s: " arg, this, mType.get(), __func__, ##__VA_ARGS__))

#define UNIMPLEMENTED() MSE_DEBUG("UNIMPLEMENTED FUNCTION at %s:%d", __FILE__, __LINE__)

namespace mozilla {

class MediaSourceResource final : public MediaResource
{
public:
  explicit MediaSourceResource(nsIPrincipal* aPrincipal = nullptr)
    : mPrincipal(aPrincipal)
    , mMonitor("MediaSourceResource")
    , mEnded(false)
    {}

  nsresult Close() override { return NS_OK; }
  void Suspend(bool aCloseImmediately) override { UNIMPLEMENTED(); }
  void Resume() override { UNIMPLEMENTED(); }
  bool CanClone() override { UNIMPLEMENTED(); return false; }
  already_AddRefed<MediaResource> CloneData(MediaResourceCallback*) override { UNIMPLEMENTED(); return nullptr; }
  void SetReadMode(MediaCacheStream::ReadMode aMode) override { UNIMPLEMENTED(); }
  void SetPlaybackRate(uint32_t aBytesPerSecond) override  { UNIMPLEMENTED(); }
  nsresult ReadAt(int64_t aOffset, char* aBuffer, uint32_t aCount, uint32_t* aBytes) override { UNIMPLEMENTED(); return NS_ERROR_FAILURE; }
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
  nsresult Open(nsIStreamListener** aStreamListener) override { UNIMPLEMENTED(); return NS_ERROR_FAILURE; }

  already_AddRefed<nsIPrincipal> GetCurrentPrincipal() override
  {
    return RefPtr<nsIPrincipal>(mPrincipal).forget();
  }

  nsresult GetCachedRanges(MediaByteRangeSet& aRanges) override
  {
    UNIMPLEMENTED();
    aRanges += MediaByteRange(0, GetLength());
    return NS_OK;
  }

  bool IsTransportSeekable() override { return true; }
  const nsCString& GetContentType() const override { return mType; }

  bool IsLiveStream() override
  {
    MonitorAutoLock mon(mMonitor);
    return !mEnded;
  }
  void SetEnded(bool aEnded)
  {
    MonitorAutoLock mon(mMonitor);
    mEnded = aEnded;
  }

  bool IsExpectingMoreData() override
  {
    MonitorAutoLock mon(mMonitor);
    return !mEnded;
  }

private:
  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t size = MediaResource::SizeOfExcludingThis(aMallocSizeOf);
    size += mType.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

    return size;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  RefPtr<nsIPrincipal> mPrincipal;
  const nsCString mType;
  Monitor mMonitor;
  bool mEnded; // protected by mMonitor
};

} // namespace mozilla

#undef MSE_DEBUG
#undef UNIMPLEMENTED

#endif /* MOZILLA_MEDIASOURCERESOURCE_H_ */
