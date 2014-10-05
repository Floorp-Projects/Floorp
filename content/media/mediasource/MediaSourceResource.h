/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASOURCERESOURCE_H_
#define MOZILLA_MEDIASOURCERESOURCE_H_

#include "MediaResource.h"
#include "prlog.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetMediaSourceLog();
extern PRLogModuleInfo* GetMediaSourceAPILog();

#define MSE_DEBUG(...) PR_LOG(GetMediaSourceLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define MSE_DEBUG(...)
#endif

#define UNIMPLEMENTED() MSE_DEBUG("MediaSourceResource(%p): UNIMPLEMENTED FUNCTION at %s:%d", this, __FILE__, __LINE__)

namespace mozilla {

class MediaSourceResource MOZ_FINAL : public MediaResource
{
public:
  MediaSourceResource() {}

  virtual nsresult Close() MOZ_OVERRIDE { return NS_OK; }
  virtual void Suspend(bool aCloseImmediately) MOZ_OVERRIDE { UNIMPLEMENTED(); }
  virtual void Resume() MOZ_OVERRIDE { UNIMPLEMENTED(); }
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal() MOZ_OVERRIDE { UNIMPLEMENTED(); return nullptr; }
  virtual bool CanClone() MOZ_OVERRIDE { UNIMPLEMENTED(); return false; }
  virtual already_AddRefed<MediaResource> CloneData(MediaDecoder* aDecoder) MOZ_OVERRIDE { UNIMPLEMENTED(); return nullptr; }
  virtual void SetReadMode(MediaCacheStream::ReadMode aMode) MOZ_OVERRIDE { UNIMPLEMENTED(); }
  virtual void SetPlaybackRate(uint32_t aBytesPerSecond) MOZ_OVERRIDE  { UNIMPLEMENTED(); }
  virtual nsresult Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes) MOZ_OVERRIDE { UNIMPLEMENTED(); return NS_ERROR_FAILURE; }
  virtual nsresult ReadAt(int64_t aOffset, char* aBuffer, uint32_t aCount, uint32_t* aBytes) MOZ_OVERRIDE { UNIMPLEMENTED(); return NS_ERROR_FAILURE; }
  virtual nsresult Seek(int32_t aWhence, int64_t aOffset) MOZ_OVERRIDE { UNIMPLEMENTED(); return NS_ERROR_FAILURE; }
  virtual int64_t Tell() MOZ_OVERRIDE { UNIMPLEMENTED(); return -1; }
  virtual void Pin() MOZ_OVERRIDE { UNIMPLEMENTED(); }
  virtual void Unpin() MOZ_OVERRIDE { UNIMPLEMENTED(); }
  virtual double GetDownloadRate(bool* aIsReliable) MOZ_OVERRIDE { UNIMPLEMENTED(); *aIsReliable = false; return 0; }
  virtual int64_t GetLength() MOZ_OVERRIDE { UNIMPLEMENTED(); return -1; }
  virtual int64_t GetNextCachedData(int64_t aOffset) MOZ_OVERRIDE { UNIMPLEMENTED(); return -1; }
  virtual int64_t GetCachedDataEnd(int64_t aOffset) MOZ_OVERRIDE { UNIMPLEMENTED(); return -1; }
  virtual bool IsDataCachedToEndOfResource(int64_t aOffset) MOZ_OVERRIDE { UNIMPLEMENTED(); return false; }
  virtual bool IsSuspendedByCache() MOZ_OVERRIDE { UNIMPLEMENTED(); return false; }
  virtual bool IsSuspended() MOZ_OVERRIDE { UNIMPLEMENTED(); return false; }
  virtual nsresult ReadFromCache(char* aBuffer, int64_t aOffset, uint32_t aCount) MOZ_OVERRIDE { UNIMPLEMENTED(); return NS_ERROR_FAILURE; }
  virtual nsresult Open(nsIStreamListener** aStreamListener) MOZ_OVERRIDE { UNIMPLEMENTED(); return NS_ERROR_FAILURE; }

  virtual nsresult GetCachedRanges(nsTArray<MediaByteRange>& aRanges) MOZ_OVERRIDE
  {
    UNIMPLEMENTED();
    aRanges.AppendElement(MediaByteRange(0, GetLength()));
    return NS_OK;
  }

  virtual bool IsTransportSeekable() MOZ_OVERRIDE { return true; }
  virtual const nsCString& GetContentType() const MOZ_OVERRIDE { return mType; }

private:
  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const MOZ_OVERRIDE
  {
    size_t size = MediaResource::SizeOfExcludingThis(aMallocSizeOf);
    size += mType.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

    return size;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const MOZ_OVERRIDE
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  const nsCString mType;
};

} // namespace mozilla

#undef UNIMPLEMENTED

#endif /* MOZILLA_MEDIASOURCERESOURCE_H_ */
