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
                      nsIPrincipal* aPrincipal) :
    mBuffer(aBuffer),
    mLength(aLength),
    mOffset(0),
    mPrincipal(aPrincipal)
  {
    MOZ_COUNT_CTOR(BufferMediaResource);
  }

  virtual ~BufferMediaResource()
  {
    MOZ_COUNT_DTOR(BufferMediaResource);
  }

  virtual nsresult Close() { return NS_OK; }
  virtual void Suspend(bool aCloseImmediately) {}
  virtual void Resume() {}
  // Get the current principal for the channel
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal()
  {
    nsCOMPtr<nsIPrincipal> principal = mPrincipal;
    return principal.forget();
  }
  virtual bool CanClone() { return false; }
  virtual MediaResource* CloneData(MediaDecoder* aDecoder)
  {
    return nullptr;
  }

  // These methods are called off the main thread.
  // The mode is initially MODE_PLAYBACK.
  virtual void SetReadMode(MediaCacheStream::ReadMode aMode) {}
  virtual void SetPlaybackRate(uint32_t aBytesPerSecond) {}
  virtual nsresult Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes)
  {
    *aBytes = std::min(mLength - mOffset, aCount);
    memcpy(aBuffer, mBuffer + mOffset, *aBytes);
    mOffset += *aBytes;
    MOZ_ASSERT(mOffset <= mLength);
    return NS_OK;
  }
  virtual nsresult Seek(int32_t aWhence, int64_t aOffset)
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
  virtual void StartSeekingForMetadata() {}
  virtual void EndSeekingForMetadata() {}
  virtual int64_t Tell() { return mOffset; }

  virtual void Pin() {}
  virtual void Unpin() {}
  virtual double GetDownloadRate(bool* aIsReliable) { return 0.; }
  virtual int64_t GetLength() { return mLength; }
  virtual int64_t GetNextCachedData(int64_t aOffset) { return aOffset; }
  virtual int64_t GetCachedDataEnd(int64_t aOffset) { return mLength; }
  virtual bool IsDataCachedToEndOfResource(int64_t aOffset) { return true; }
  virtual bool IsSuspendedByCache(MediaResource** aActiveResource) { return false; }
  virtual bool IsSuspended() { return false; }
  virtual nsresult ReadFromCache(char* aBuffer,
                                 int64_t aOffset,
                                 uint32_t aCount)
  {
    if (aOffset < 0) {
      return NS_ERROR_FAILURE;
    }

    uint32_t bytes = std::min(mLength - static_cast<uint32_t>(aOffset), aCount);
    memcpy(aBuffer, mBuffer + aOffset, bytes);
    return NS_OK;
  }

  virtual nsresult Open(nsIStreamListener** aStreamListener)
  {
    return NS_ERROR_FAILURE;
  }

  virtual nsresult OpenByteRange(nsIStreamListener** aStreamListener,
                                 MediaByteRange const &aByteRange)
  {
    return NS_ERROR_FAILURE;
  }

  virtual nsresult GetCachedRanges(nsTArray<MediaByteRange>& aRanges)
  {
    aRanges.AppendElement(MediaByteRange(0, mLength));
    return NS_OK;
  }

private:
  const uint8_t * mBuffer;
  uint32_t mLength;
  uint32_t mOffset;
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

}

#endif
