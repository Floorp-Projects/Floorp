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
  explicit MockMediaResource(const char* aFileName,
                             const MediaContainerType& aMimeType =
                               MediaContainerType(MEDIAMIMETYPE("video/mp4")));
  nsresult Close() override { return NS_OK; }
  void Suspend(bool aCloseImmediately) override {}
  void Resume() override {}
  already_AddRefed<nsIPrincipal> GetCurrentPrincipal() override
  {
    return nullptr;
  }
  void SetReadMode(MediaCacheStream::ReadMode aMode) override {}
  void SetPlaybackRate(uint32_t aBytesPerSecond) override {}
  nsresult ReadAt(int64_t aOffset, char* aBuffer, uint32_t aCount,
                  uint32_t* aBytes) override;
  // Data stored in file, caching recommended.
  bool ShouldCacheReads() override { return true; }
  int64_t Tell() override { return 0; }
  void Pin() override {}
  void Unpin() override {}
  double GetDownloadRate(bool* aIsReliable) override { return 0; }
  int64_t GetLength() override;
  int64_t GetNextCachedData(int64_t aOffset) override;
  int64_t GetCachedDataEnd(int64_t aOffset) override;
  bool IsDataCachedToEndOfResource(int64_t aOffset) override
  {
    return false;
  }
  bool IsSuspendedByCache() override { return false; }
  bool IsSuspended() override { return false; }
  nsresult ReadFromCache(char* aBuffer, int64_t aOffset,
                         uint32_t aCount) override
  {
    uint32_t bytesRead = 0;
    nsresult rv = ReadAt(aOffset, aBuffer, aCount, &bytesRead);
    NS_ENSURE_SUCCESS(rv, rv);
    return bytesRead == aCount ? NS_OK : NS_ERROR_FAILURE;
  }

  bool IsTransportSeekable() override { return true; }
  nsresult Open(nsIStreamListener** aStreamListener) override;
  nsresult GetCachedRanges(MediaByteRangeSet& aRanges) override;
  const MediaContainerType& GetContentType() const override
  {
    return mContainerType;
  }

  void MockClearBufferedRanges();
  void MockAddBufferedRange(int64_t aStart, int64_t aEnd);

protected:
  virtual ~MockMediaResource();

private:
  FILE* mFileHandle;
  const char* mFileName;
  MediaByteRangeSet mRanges;
  Atomic<int> mEntry;
  const MediaContainerType mContainerType;
};

} // namespace mozilla

#endif
