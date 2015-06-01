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

extern PRLogModuleInfo* GetMediaSourceLog();

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

  virtual nsresult Close() override { return NS_OK; }
  virtual void Suspend(bool aCloseImmediately) override { UNIMPLEMENTED(); }
  virtual void Resume() override { UNIMPLEMENTED(); }
  virtual bool CanClone() override { UNIMPLEMENTED(); return false; }
  virtual already_AddRefed<MediaResource> CloneData(MediaDecoder* aDecoder) override { UNIMPLEMENTED(); return nullptr; }
  virtual void SetReadMode(MediaCacheStream::ReadMode aMode) override { UNIMPLEMENTED(); }
  virtual void SetPlaybackRate(uint32_t aBytesPerSecond) override  { UNIMPLEMENTED(); }
  virtual nsresult Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes) override { UNIMPLEMENTED(); return NS_ERROR_FAILURE; }
  virtual nsresult ReadAt(int64_t aOffset, char* aBuffer, uint32_t aCount, uint32_t* aBytes) override { UNIMPLEMENTED(); return NS_ERROR_FAILURE; }
  virtual nsresult Seek(int32_t aWhence, int64_t aOffset) override { UNIMPLEMENTED(); return NS_ERROR_FAILURE; }
  virtual int64_t Tell() override { UNIMPLEMENTED(); return -1; }
  virtual void Pin() override { UNIMPLEMENTED(); }
  virtual void Unpin() override { UNIMPLEMENTED(); }
  virtual double GetDownloadRate(bool* aIsReliable) override { UNIMPLEMENTED(); *aIsReliable = false; return 0; }
  virtual int64_t GetLength() override { UNIMPLEMENTED(); return -1; }
  virtual int64_t GetNextCachedData(int64_t aOffset) override { UNIMPLEMENTED(); return -1; }
  virtual int64_t GetCachedDataEnd(int64_t aOffset) override { UNIMPLEMENTED(); return -1; }
  virtual bool IsDataCachedToEndOfResource(int64_t aOffset) override { UNIMPLEMENTED(); return false; }
  virtual bool IsSuspendedByCache() override { UNIMPLEMENTED(); return false; }
  virtual bool IsSuspended() override { UNIMPLEMENTED(); return false; }
  virtual nsresult ReadFromCache(char* aBuffer, int64_t aOffset, uint32_t aCount) override { UNIMPLEMENTED(); return NS_ERROR_FAILURE; }
  virtual nsresult Open(nsIStreamListener** aStreamListener) override { UNIMPLEMENTED(); return NS_ERROR_FAILURE; }

  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal() override
  {
    return nsRefPtr<nsIPrincipal>(mPrincipal).forget();
  }

  virtual nsresult GetCachedRanges(nsTArray<MediaByteRange>& aRanges) override
  {
    UNIMPLEMENTED();
    aRanges.AppendElement(MediaByteRange(0, GetLength()));
    return NS_OK;
  }

  virtual bool IsTransportSeekable() override { return true; }
  virtual const nsCString& GetContentType() const override { return mType; }

  virtual bool IsLiveStream() override
  {
    MonitorAutoLock mon(mMonitor);
    return !mEnded;
  }
  void SetEnded(bool aEnded)
  {
    MonitorAutoLock mon(mMonitor);
    mEnded = aEnded;
  }

private:
  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t size = MediaResource::SizeOfExcludingThis(aMallocSizeOf);
    size += mType.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

    return size;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  nsRefPtr<nsIPrincipal> mPrincipal;
  const nsCString mType;
  Monitor mMonitor;
  bool mEnded; // protected by mMonitor
};

} // namespace mozilla

#undef MSE_DEBUG
#undef UNIMPLEMENTED

#endif /* MOZILLA_MEDIASOURCERESOURCE_H_ */
