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
  explicit MockMediaResource(const char* aFileName);
  virtual nsIURI* URI() const override { return nullptr; }
  virtual nsresult Close() override { return NS_OK; }
  virtual void Suspend(bool aCloseImmediately) override {}
  virtual void Resume() override {}
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal() override
  {
    return nullptr;
  }
  virtual bool CanClone() override { return false; }
  virtual already_AddRefed<MediaResource> CloneData(MediaDecoder* aDecoder)
    override
  {
    return nullptr;
  }
  virtual void SetReadMode(MediaCacheStream::ReadMode aMode) override {}
  virtual void SetPlaybackRate(uint32_t aBytesPerSecond) override {}
  virtual nsresult Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes)
    override
  {
    return NS_OK;
  }
  virtual nsresult ReadAt(int64_t aOffset, char* aBuffer, uint32_t aCount,
                          uint32_t* aBytes) override;
  virtual nsresult Seek(int32_t aWhence, int64_t aOffset) override
  {
    return NS_OK;
  }
  virtual int64_t Tell() override { return 0; }
  virtual void Pin() override {}
  virtual void Unpin() override {}
  virtual double GetDownloadRate(bool* aIsReliable) override { return 0; }
  virtual int64_t GetLength() override;
  virtual int64_t GetNextCachedData(int64_t aOffset) override;
  virtual int64_t GetCachedDataEnd(int64_t aOffset) override;
  virtual bool IsDataCachedToEndOfResource(int64_t aOffset) override
  {
    return false;
  }
  virtual bool IsSuspendedByCache() override { return false; }
  virtual bool IsSuspended() override { return false; }
  virtual nsresult ReadFromCache(char* aBuffer, int64_t aOffset,
                                 uint32_t aCount) override
  {
    uint32_t bytesRead = 0;
    nsresult rv = ReadAt(aOffset, aBuffer, aCount, &bytesRead);
    NS_ENSURE_SUCCESS(rv, rv);
    return bytesRead == aCount ? NS_OK : NS_ERROR_FAILURE;
  }

  virtual bool IsTransportSeekable() override { return true; }
  virtual nsresult Open(nsIStreamListener** aStreamListener) override;
  virtual nsresult GetCachedRanges(nsTArray<MediaByteRange>& aRanges)
    override;
  virtual const nsCString& GetContentType() const override
  {
    return mContentType;
  }

  void MockClearBufferedRanges();
  void MockAddBufferedRange(int64_t aStart, int64_t aEnd);

protected:
  virtual ~MockMediaResource();

private:
  FILE* mFileHandle;
  const char* mFileName;
  nsTArray<MediaByteRange> mRanges;
  Atomic<int> mEntry;
  nsCString mContentType;
};

} // namespace mozilla

#endif
