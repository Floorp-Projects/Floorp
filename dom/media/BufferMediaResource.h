/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(BufferMediaResource_h_)
#define BufferMediaResource_h_

#include "MediaResource.h"
#include "nsISeekableStream.h"
#include "nsIPrincipal.h"
#include <algorithm>

namespace mozilla {

// A simple MediaResource based on an in memory buffer.  This class accepts
// the address and the length of the buffer, and simulates a read/seek API
// on top of it.  The Read implementation involves copying memory, which is
// unfortunate, but the MediaResource interface mandates that.
class BufferMediaResource : public MediaResource
{
public:
  BufferMediaResource(const uint8_t* aBuffer,
                      uint32_t aLength,
                      nsIPrincipal* aPrincipal,
                      const nsACString& aContentType) :
    mBuffer(aBuffer),
    mLength(aLength),
    mOffset(0),
    mPrincipal(aPrincipal),
    mContentType(aContentType)
  {
    MOZ_COUNT_CTOR(BufferMediaResource);
  }

protected:
  virtual ~BufferMediaResource()
  {
    MOZ_COUNT_DTOR(BufferMediaResource);
  }

private:
  nsresult Close() override { return NS_OK; }
  void Suspend(bool aCloseImmediately) override {}
  void Resume() override {}
  // Get the current principal for the channel
  already_AddRefed<nsIPrincipal> GetCurrentPrincipal() override
  {
    nsCOMPtr<nsIPrincipal> principal = mPrincipal;
    return principal.forget();
  }
  bool CanClone() override { return false; }
  already_AddRefed<MediaResource> CloneData(MediaResourceCallback*) override
  {
    return nullptr;
  }

  // These methods are called off the main thread.
  // The mode is initially MODE_PLAYBACK.
  void SetReadMode(MediaCacheStream::ReadMode aMode) override {}
  void SetPlaybackRate(uint32_t aBytesPerSecond) override {}
  nsresult ReadAt(int64_t aOffset, char* aBuffer,
                  uint32_t aCount, uint32_t* aBytes) override
  {
    if (aOffset < 0 || aOffset > mLength) {
      return NS_ERROR_FAILURE;
    }
    *aBytes = std::min(mLength - static_cast<uint32_t>(aOffset), aCount);
    memcpy(aBuffer, mBuffer + aOffset, *aBytes);
    mOffset = aOffset + *aBytes;
    return NS_OK;
  }
  int64_t Tell() override { return mOffset; }

  void Pin() override {}
  void Unpin() override {}
  double GetDownloadRate(bool* aIsReliable) override { *aIsReliable = false; return 0.; }
  int64_t GetLength() override { return mLength; }
  int64_t GetNextCachedData(int64_t aOffset) override { return aOffset; }
  int64_t GetCachedDataEnd(int64_t aOffset) override { return mLength; }
  bool IsDataCachedToEndOfResource(int64_t aOffset) override { return true; }
  bool IsSuspendedByCache() override { return false; }
  bool IsSuspended() override { return false; }
  nsresult ReadFromCache(char* aBuffer,
                         int64_t aOffset,
                         uint32_t aCount) override
  {
    if (aOffset < 0) {
      return NS_ERROR_FAILURE;
    }

    uint32_t bytes = std::min(mLength - static_cast<uint32_t>(aOffset), aCount);
    memcpy(aBuffer, mBuffer + aOffset, bytes);
    return NS_OK;
  }

  nsresult Open(nsIStreamListener** aStreamListener) override
  {
    return NS_ERROR_FAILURE;
  }

  nsresult GetCachedRanges(MediaByteRangeSet& aRanges) override
  {
    aRanges += MediaByteRange(0, int64_t(mLength));
    return NS_OK;
  }

  bool IsTransportSeekable() override { return true; }

  const nsCString& GetContentType() const override
  {
    return mContentType;
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    // Not owned:
    // - mBuffer
    // - mPrincipal
    size_t size = MediaResource::SizeOfExcludingThis(aMallocSizeOf);
    size += mContentType.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

    return size;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  const uint8_t * mBuffer;
  uint32_t mLength;
  uint32_t mOffset;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  const nsCString mContentType;
};

} // namespace mozilla

#endif
