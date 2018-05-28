/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_media_FileMediaResource_h
#define mozilla_dom_media_FileMediaResource_h

#include "BaseMediaResource.h"
#include "mozilla/Mutex.h"

namespace mozilla {

class FileMediaResource : public BaseMediaResource
{
public:
  FileMediaResource(MediaResourceCallback* aCallback,
                    nsIChannel* aChannel,
                    nsIURI* aURI,
                    int64_t aSize = -1 /* unknown size */)
    : BaseMediaResource(aCallback, aChannel, aURI)
    , mSize(aSize)
    , mLock("FileMediaResource.mLock")
    , mSizeInitialized(aSize != -1)
  {
  }
  ~FileMediaResource()
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
    MutexAutoLock lock(mLock);

    EnsureSizeInitialized();
    return mSizeInitialized ? mSize : 0;
  }

  int64_t GetNextCachedData(int64_t aOffset) override
  {
    MutexAutoLock lock(mLock);

    EnsureSizeInitialized();
    return (aOffset < mSize) ? aOffset : -1;
  }

  int64_t GetCachedDataEnd(int64_t aOffset) override
  {
    MutexAutoLock lock(mLock);

    EnsureSizeInitialized();
    return std::max(aOffset, mSize);
  }
  bool    IsDataCachedToEndOfResource(int64_t aOffset) override { return true; }
  bool    IsTransportSeekable() override { return true; }

  nsresult GetCachedRanges(MediaByteRangeSet& aRanges) override;

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    // Might be useful to track in the future:
    // - mInput
    return BaseMediaResource::SizeOfExcludingThis(aMallocSizeOf);
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

protected:
  // These Unsafe variants of Read and Seek perform their operations
  // without acquiring mLock. The caller must obtain the lock before
  // calling. The implmentation of Read, Seek and ReadAt obtains the
  // lock before calling these Unsafe variants to read or seek.
  nsresult UnsafeRead(char* aBuffer, uint32_t aCount, uint32_t* aBytes);
  nsresult UnsafeSeek(int32_t aWhence, int64_t aOffset);
private:
  // Ensures mSize is initialized, if it can be.
  // mLock must be held when this is called, and mInput must be non-null.
  void EnsureSizeInitialized();
  already_AddRefed<MediaByteBuffer> UnsafeMediaReadAt(
                        int64_t aOffset, uint32_t aCount);

  // The file size, or -1 if not known. Immutable after Open().
  // Can be used from any thread.
  int64_t mSize;

  // This lock handles synchronisation between calls to Close() and
  // the Read, Seek, etc calls. Close must not be called while a
  // Read or Seek is in progress since it resets various internal
  // values to null.
  // This lock protects mSeekable, mInput, mSize, and mSizeInitialized.
  Mutex mLock;

  // Seekable stream interface to file. This can be used from any
  // thread.
  nsCOMPtr<nsISeekableStream> mSeekable;

  // Input stream for the media data. This can be used from any
  // thread.
  nsCOMPtr<nsIInputStream>  mInput;

  // Whether we've attempted to initialize mSize. Note that mSize can be -1
  // when mSizeInitialized is true if we tried and failed to get the size
  // of the file.
  bool mSizeInitialized;
  // Set to true if NotifyDataEnded callback has been processed (which only
  // occurs if resource size is known)
  bool mNotifyDataEndedProcessed = false;
};

} // namespace mozilla

#endif // mozilla_dom_media_FileMediaResource_h
