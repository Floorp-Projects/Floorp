/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_media_CloneableWithRangeMediaResource_h
#define mozilla_dom_media_CloneableWithRangeMediaResource_h

#include "BaseMediaResource.h"
#include "nsICloneableInputStream.h"

namespace mozilla {

class CloneableWithRangeMediaResource : public BaseMediaResource
{
public:
  CloneableWithRangeMediaResource(MediaResourceCallback* aCallback,
                                  nsIChannel* aChannel,
                                  nsIURI* aURI,
                                  nsIInputStream* aStream,
                                  uint64_t aSize)
    : BaseMediaResource(aCallback, aChannel, aURI)
    , mStream(do_QueryInterface(aStream))
    , mSize(aSize)
    , mCurrentPosition(0)
    , mInitialized(false)
  {
    MOZ_ASSERT(mStream);
  }

  ~CloneableWithRangeMediaResource()
  {
  }

  // Main thread
  nsresult Open(nsIStreamListener** aStreamListener) override;
  nsresult Close() override;
  void     Suspend(bool aCloseImmediately) override {}
  void     Resume() override {}
  already_AddRefed<nsIPrincipal> GetCurrentPrincipal() override;
  nsresult ReadFromCache(char* aBuffer, int64_t aOffset, uint32_t aCount) override;

  // These methods are called off the main thread.

  // Other thread
  void     SetReadMode(MediaCacheStream::ReadMode aMode) override {}
  void     SetPlaybackRate(uint32_t aBytesPerSecond) override {}
  nsresult ReadAt(int64_t aOffset, char* aBuffer,
                  uint32_t aCount, uint32_t* aBytes) override;
  // (Probably) file-based, caching recommended.
  bool ShouldCacheReads() override { return true; }
  int64_t  Tell() override;

  // Any thread
  void    Pin() override {}
  void    Unpin() override {}

  double  GetDownloadRate(bool* aIsReliable) override
  {
    // The data's all already here
    *aIsReliable = true;
    return 100*1024*1024; // arbitray, use 100MB/s
  }

  int64_t GetLength() override
  {
    MaybeInitialize();
    return mSize;
  }

  int64_t GetNextCachedData(int64_t aOffset) override
  {
    MaybeInitialize();
    return (aOffset < (int64_t)mSize) ? aOffset : -1;
  }

  int64_t GetCachedDataEnd(int64_t aOffset) override
  {
    MaybeInitialize();
    return std::max(aOffset, (int64_t)mSize);
  }

  bool    IsDataCachedToEndOfResource(int64_t aOffset) override { return true; }
  bool    IsTransportSeekable() override { return true; }

  nsresult GetCachedRanges(MediaByteRangeSet& aRanges) override;

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return BaseMediaResource::SizeOfExcludingThis(aMallocSizeOf);
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  void MaybeInitialize();

  // Input stream for the media data. This can be used from any
  // thread.
  nsCOMPtr<nsICloneableInputStreamWithRange> mStream;

  // The stream size.
  uint64_t mSize;

  // The current position.
  uint64_t mCurrentPosition;

  bool mInitialized;
};

} // namespace mozilla

#endif // mozilla_dom_media_CloneableWithRangeMediaResource_h
