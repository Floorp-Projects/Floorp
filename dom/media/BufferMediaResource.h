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
  virtual nsresult Close() override { return NS_OK; }
  virtual void Suspend(bool aCloseImmediately) override {}
  virtual void Resume() override {}
  // Get the current principal for the channel
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal() override
  {
    nsCOMPtr<nsIPrincipal> principal = mPrincipal;
    return principal.forget();
  }
  virtual bool CanClone() override { return false; }
  virtual already_AddRefed<MediaResource> CloneData(MediaDecoder* aDecoder) override
  {
    return nullptr;
  }

  // These methods are called off the main thread.
  // The mode is initially MODE_PLAYBACK.
  virtual void SetReadMode(MediaCacheStream::ReadMode aMode) override {}
  virtual void SetPlaybackRate(uint32_t aBytesPerSecond) override {}
  virtual nsresult Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes) override
  {
    *aBytes = std::min(mLength - mOffset, aCount);
    memcpy(aBuffer, mBuffer + mOffset, *aBytes);
    mOffset += *aBytes;
    MOZ_ASSERT(mOffset <= mLength);
    return NS_OK;
  }
  virtual nsresult ReadAt(int64_t aOffset, char* aBuffer,
                          uint32_t aCount, uint32_t* aBytes) override
  {
    nsresult rv = Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
    if (NS_FAILED(rv)) return rv;
    return Read(aBuffer, aCount, aBytes);
  }
  virtual nsresult Seek(int32_t aWhence, int64_t aOffset) override
  {
    MOZ_ASSERT(aOffset <= UINT32_MAX);
    switch (aWhence) {
    case nsISeekableStream::NS_SEEK_SET:
      if (aOffset < 0 || aOffset > mLength) {
        return NS_ERROR_FAILURE;
      }
      mOffset = static_cast<uint32_t> (aOffset);
      break;
    case nsISeekableStream::NS_SEEK_CUR:
      if (aOffset >= mLength - mOffset) {
        return NS_ERROR_FAILURE;
      }
      mOffset += static_cast<uint32_t> (aOffset);
      break;
    case nsISeekableStream::NS_SEEK_END:
      if (aOffset < 0 || aOffset > mLength) {
        return NS_ERROR_FAILURE;
      }
      mOffset = mLength - aOffset;
      break;
    }

    return NS_OK;
  }
  virtual int64_t Tell() override { return mOffset; }

  virtual void Pin() override {}
  virtual void Unpin() override {}
  virtual double GetDownloadRate(bool* aIsReliable) override { *aIsReliable = false; return 0.; }
  virtual int64_t GetLength() override { return mLength; }
  virtual int64_t GetNextCachedData(int64_t aOffset) override { return aOffset; }
  virtual int64_t GetCachedDataEnd(int64_t aOffset) override { return mLength; }
  virtual bool IsDataCachedToEndOfResource(int64_t aOffset) override { return true; }
  virtual bool IsSuspendedByCache() override { return false; }
  virtual bool IsSuspended() override { return false; }
  virtual nsresult ReadFromCache(char* aBuffer,
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

  virtual nsresult Open(nsIStreamListener** aStreamListener) override
  {
    return NS_ERROR_FAILURE;
  }

  virtual nsresult GetCachedRanges(nsTArray<MediaByteRange>& aRanges) override
  {
    aRanges.AppendElement(MediaByteRange(0, mLength));
    return NS_OK;
  }

  bool IsTransportSeekable() override { return true; }

  virtual const nsCString& GetContentType() const override
  {
    return mContentType;
  }

  virtual size_t SizeOfExcludingThis(
                        MallocSizeOf aMallocSizeOf) const override
  {
    // Not owned:
    // - mBuffer
    // - mPrincipal
    size_t size = MediaResource::SizeOfExcludingThis(aMallocSizeOf);
    size += mContentType.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

    return size;
  }

  virtual size_t SizeOfIncludingThis(
                        MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  const uint8_t * mBuffer;
  uint32_t mLength;
  uint32_t mOffset;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  const nsAutoCString mContentType;
};

} // namespace mozilla

#endif
