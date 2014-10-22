/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOCK_MEDIA_RESOURCE_H_
#define MOCK_MEDIA_RESOURCE_H_

#include "MediaResource.h"
#include "nsTArray.h"
#include "mozilla/Atomics.h"

namespace mozilla
{

class MockMediaResource : public MediaResource
{
public:
  MockMediaResource(const char* aFileName);
  virtual nsIURI* URI() const MOZ_OVERRIDE { return nullptr; }
  virtual nsresult Close() MOZ_OVERRIDE { return NS_OK; }
  virtual void Suspend(bool aCloseImmediately) MOZ_OVERRIDE {}
  virtual void Resume() MOZ_OVERRIDE {}
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal() MOZ_OVERRIDE
  {
    return nullptr;
  }
  virtual bool CanClone() MOZ_OVERRIDE { return false; }
  virtual already_AddRefed<MediaResource> CloneData(MediaDecoder* aDecoder)
    MOZ_OVERRIDE
  {
    return nullptr;
  }
  virtual void SetReadMode(MediaCacheStream::ReadMode aMode) MOZ_OVERRIDE {}
  virtual void SetPlaybackRate(uint32_t aBytesPerSecond) MOZ_OVERRIDE {}
  virtual nsresult Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes)
    MOZ_OVERRIDE
  {
    return NS_OK;
  }
  virtual nsresult ReadAt(int64_t aOffset, char* aBuffer, uint32_t aCount,
                          uint32_t* aBytes);
  virtual nsresult Seek(int32_t aWhence, int64_t aOffset) MOZ_OVERRIDE
  {
    return NS_OK;
  }
  virtual int64_t Tell() MOZ_OVERRIDE { return 0; }
  virtual void Pin() MOZ_OVERRIDE {}
  virtual void Unpin() MOZ_OVERRIDE {}
  virtual double GetDownloadRate(bool* aIsReliable) MOZ_OVERRIDE { return 0; }
  virtual int64_t GetLength() MOZ_OVERRIDE;
  virtual int64_t GetNextCachedData(int64_t aOffset) MOZ_OVERRIDE;
  virtual int64_t GetCachedDataEnd(int64_t aOffset) MOZ_OVERRIDE;
  virtual bool IsDataCachedToEndOfResource(int64_t aOffset) MOZ_OVERRIDE
  {
    return false;
  }
  virtual bool IsSuspendedByCache() MOZ_OVERRIDE { return false; }
  virtual bool IsSuspended() MOZ_OVERRIDE { return false; }
  virtual nsresult ReadFromCache(char* aBuffer, int64_t aOffset,
                                 uint32_t aCount)
  {
    return NS_OK;
  }
  virtual bool IsTransportSeekable() MOZ_OVERRIDE { return true; }
  virtual nsresult Open(nsIStreamListener** aStreamListener) MOZ_OVERRIDE;
  virtual nsresult GetCachedRanges(nsTArray<MediaByteRange>& aRanges)
    MOZ_OVERRIDE;
  virtual const nsCString& GetContentType() const MOZ_OVERRIDE
  {
    return mContentType;
  }

  void MockClearBufferedRanges();
  void MockAddBufferedRange(int64_t aStart, int64_t aEnd);

private:
  virtual ~MockMediaResource();
  FILE* mFileHandle;
  const char* mFileName;
  nsTArray<MediaByteRange> mRanges;
  Atomic<int> mEntry;
  nsCString mContentType;
};
}

#endif
