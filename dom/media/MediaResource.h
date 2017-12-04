/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaResource_h_)
#define MediaResource_h_

#include "Intervals.h"
#include "MediaData.h"
#include "mozilla/Attributes.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/UniquePtr.h"
#include "nsISeekableStream.h"
#include "nsThreadUtils.h"

namespace mozilla {

// Represents a section of contiguous media, with a start and end offset.
// Used to denote ranges of data which are cached.

typedef media::Interval<int64_t> MediaByteRange;
typedef media::IntervalSet<int64_t> MediaByteRangeSet;

/**
 * Provides a thread-safe, seek/read interface to resources
 * loaded from a URI. Uses MediaCache to cache data received over
 * Necko's async channel API, thus resolving the mismatch between clients
 * that need efficient random access to the data and protocols that do not
 * support efficient random access, such as HTTP.
 *
 * Instances of this class must be created on the main thread.
 * Most methods must be called on the main thread only. Read, Seek and
 * Tell must only be called on non-main threads. In the case of the Ogg
 * Decoder they are called on the Decode thread for example. You must
 * ensure that no threads are calling these methods once Close is called.
 *
 * Instances of this class are reference counted. Use nsRefPtr for
 * managing the lifetime of instances of this class.
 *
 * The generic implementation of this class is ChannelMediaResource, which can
 * handle any URI for which Necko supports AsyncOpen.
 * The 'file:' protocol can be implemented efficiently with direct random
 * access, so the FileMediaResource implementation class bypasses the cache.
 * For cross-process blob URL, CloneableWithRangeMediaResource is used.
 * MediaResource::Create automatically chooses the best implementation class.
 */
class MediaResource
{
public:
  // Our refcounting is threadsafe, and when our refcount drops to zero
  // we dispatch an event to the main thread to delete the MediaResource.
  // Note that this means it's safe for references to this object to be
  // released on a non main thread, but the destructor will always run on
  // the main thread.
  NS_METHOD_(MozExternalRefCountType) AddRef(void);
  NS_METHOD_(MozExternalRefCountType) Release(void);

  // Close the resource, stop any listeners, channels, etc.
  // Cancels any currently blocking Read request and forces that request to
  // return an error.
  virtual nsresult Close() { return NS_OK; }

  // These methods are called off the main thread.
  // Read up to aCount bytes from the stream. The read starts at
  // aOffset in the stream, seeking to that location initially if
  // it is not the current stream offset. The remaining arguments,
  // results and requirements are the same as per the Read method.
  virtual nsresult ReadAt(int64_t aOffset, char* aBuffer,
                          uint32_t aCount, uint32_t* aBytes) = 0;
  // Indicate whether caching data in advance of reads is worth it.
  // E.g. Caching lockless and memory-based MediaResource subclasses would be a
  // waste, but caching lock/IO-bound resources means reducing the impact of
  // each read.
  virtual bool ShouldCacheReads() = 0;

  // These can be called on any thread.
  // Cached blocks associated with this stream will not be evicted
  // while the stream is pinned.
  virtual void Pin() = 0;
  virtual void Unpin() = 0;
  // Get the length of the stream in bytes. Returns -1 if not known.
  // This can change over time; after a seek operation, a misbehaving
  // server may give us a resource of a different length to what it had
  // reported previously --- or it may just lie in its Content-Length
  // header and give us more or less data than it reported. We will adjust
  // the result of GetLength to reflect the data that's actually arriving.
  virtual int64_t GetLength() = 0;
  // Returns the offset of the first byte of cached data at or after aOffset,
  // or -1 if there is no such cached data.
  virtual int64_t GetNextCachedData(int64_t aOffset) = 0;
  // Returns the end of the bytes starting at the given offset which are in
  // cache. Returns aOffset itself if there are zero bytes available there.
  virtual int64_t GetCachedDataEnd(int64_t aOffset) = 0;
  // Returns true if all the data from aOffset to the end of the stream
  // is in cache. If the end of the stream is not known, we return false.
  virtual bool IsDataCachedToEndOfResource(int64_t aOffset) = 0;
  // Reads only data which is cached in the media cache. If you try to read
  // any data which overlaps uncached data, or if aCount bytes otherwise can't
  // be read, this function will return failure. This function be called from
  // any thread, and it is the only read operation which is safe to call on
  // the main thread, since it's guaranteed to be non blocking.
  virtual nsresult ReadFromCache(char* aBuffer,
                                 int64_t aOffset,
                                 uint32_t aCount) = 0;

  /**
   * Fills aRanges with MediaByteRanges representing the data which is cached
   * in the media cache. Stream should be pinned during call and while
   * aRanges is being used.
   */
  virtual nsresult GetCachedRanges(MediaByteRangeSet& aRanges) = 0;

protected:
  virtual ~MediaResource() {};

private:
  void Destroy();
  mozilla::ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

/**
 * RAII class that handles pinning and unpinning for MediaResource and derived.
 * This should be used when making calculations that involve potentially-cached
 * MediaResource data, so that the state of the world can't change out from under
 * us.
 */
template<class T>
class MOZ_RAII AutoPinned {
 public:
  explicit AutoPinned(T* aResource MOZ_GUARD_OBJECT_NOTIFIER_PARAM) : mResource(aResource) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_ASSERT(mResource);
    mResource->Pin();
  }

  ~AutoPinned() {
    mResource->Unpin();
  }

  operator T*() const { return mResource; }
  T* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN { return mResource; }

private:
  T* mResource;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/*
 * MediaResourceIndex provides a way to access MediaResource objects.
 * Read, Seek and Tell must only be called on non-main threads.
 * In the case of the Ogg Decoder they are called on the Decode thread for
 * example. You must ensure that no threads are calling these methods once
 * the MediaResource has been Closed.
 */
class MediaResourceIndex
{
public:
  explicit MediaResourceIndex(MediaResource* aResource);

  // Read up to aCount bytes from the stream. The buffer must have
  // enough room for at least aCount bytes. Stores the number of
  // actual bytes read in aBytes (0 on end of file).
  // May read less than aCount bytes if the number of
  // available bytes is less than aCount. Always check *aBytes after
  // read, and call again if necessary.
  nsresult Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes);
  // Seek to the given bytes offset in the stream. aWhence can be
  // one of:
  //   nsISeekableStream::NS_SEEK_SET
  //   nsISeekableStream::NS_SEEK_CUR
  //   nsISeekableStream::NS_SEEK_END
  //
  // In the Http strategy case the cancel will cause the http
  // channel's listener to close the pipe, forcing an i/o error on any
  // blocked read. This will allow the decode thread to complete the
  // event.
  //
  // In the case of a seek in progress, the byte range request creates
  // a new listener. This is done on the main thread via seek
  // synchronously dispatching an event. This avoids the issue of us
  // closing the listener but an outstanding byte range request
  // creating a new one. They run on the same thread so no explicit
  // synchronisation is required. The byte range request checks for
  // the cancel flag and does not create a new channel or listener if
  // we are cancelling.
  //
  // The default strategy does not do any seeking - the only issue is
  // a blocked read which it handles by causing the listener to close
  // the pipe, as per the http case.
  //
  // The file strategy doesn't block for any great length of time so
  // is fine for a no-op cancel.
  nsresult Seek(int32_t aWhence, int64_t aOffset);
  // Report the current offset in bytes from the start of the stream.
  int64_t Tell() const { return mOffset; }

  // Return the underlying MediaResource.
  MediaResource* GetResource() const { return mResource; }

  // Read up to aCount bytes from the stream. The read starts at
  // aOffset in the stream, seeking to that location initially if
  // it is not the current stream offset.
  // Unlike MediaResource::ReadAt, ReadAt only returns fewer bytes than
  // requested if end of stream or an error is encountered. There is no need to
  // call it again to get more data.
  // If the resource has cached data past the end of the request, it will be
  // used to fill a local cache, which should speed up consecutive ReadAt's
  // (mostly by avoiding using the resource's IOs and locks.)
  // *aBytes will contain the number of bytes copied, even if an error occurred.
  // ReadAt doesn't have an impact on the offset returned by Tell().
  nsresult ReadAt(int64_t aOffset,
                  char* aBuffer,
                  uint32_t aCount,
                  uint32_t* aBytes);

  // Same as ReadAt, but doesn't try to cache around the read.
  // Useful if you know that you will not read again from the same area.
  nsresult UncachedReadAt(int64_t aOffset,
                          char* aBuffer,
                          uint32_t aCount,
                          uint32_t* aBytes) const;

  // Similar to ReadAt, but doesn't try to cache around the read.
  // Useful if you know that you will not read again from the same area.
  // Will attempt to read aRequestedCount+aExtraCount, repeatedly calling
  // MediaResource/ ReadAt()'s until a read returns 0 bytes (so we may actually
  // get less than aRequestedCount bytes), or until we get at least
  // aRequestedCount bytes (so we may not get any/all of the aExtraCount bytes.)
  nsresult UncachedRangedReadAt(int64_t aOffset,
                                char* aBuffer,
                                uint32_t aRequestedCount,
                                uint32_t aExtraCount,
                                uint32_t* aBytes) const;

  // This method returns nullptr if anything fails.
  // Otherwise, it returns an owned buffer.
  // MediaReadAt may return fewer bytes than requested if end of stream is
  // encountered. There is no need to call it again to get more data.
  // Note this method will not update mOffset.
  already_AddRefed<MediaByteBuffer> MediaReadAt(int64_t aOffset,
                                                uint32_t aCount) const;

  already_AddRefed<MediaByteBuffer> CachedMediaReadAt(int64_t aOffset,
                                                      uint32_t aCount) const;

  // Get the length of the stream in bytes. Returns -1 if not known.
  // This can change over time; after a seek operation, a misbehaving
  // server may give us a resource of a different length to what it had
  // reported previously --- or it may just lie in its Content-Length
  // header and give us more or less data than it reported. We will adjust
  // the result of GetLength to reflect the data that's actually arriving.
  int64_t GetLength() const;

private:
  // If the resource has cached data past the requested range, try to grab it
  // into our local cache.
  // If there is no cached data, or attempting to read it fails, fallback on
  // a (potentially-blocking) read of just what was requested, so that we don't
  // get unexpected side-effects by trying to read more than intended.
  nsresult CacheOrReadAt(int64_t aOffset,
                         char* aBuffer,
                         uint32_t aCount,
                         uint32_t* aBytes);

  // Maps a file offset to a mCachedBlock index.
  uint32_t IndexInCache(int64_t aOffsetInFile) const;

  // Starting file offset of the cache block that contains a given file offset.
  int64_t CacheOffsetContaining(int64_t aOffsetInFile) const;

  RefPtr<MediaResource> mResource;
  int64_t mOffset;

  // Local cache used by ReadAt().
  // mCachedBlock is valid when mCachedBytes != 0, in which case it contains
  // data of length mCachedBytes, starting at offset `mCachedOffset` in the
  // resource, located at index `IndexInCache(mCachedOffset)` in mCachedBlock.
  //
  // resource: |------------------------------------------------------|
  //                                          <----------> mCacheBlockSize
  //           <---------------------------------> mCachedOffset
  //                                             <--> mCachedBytes
  // mCachedBlock:                            |..----....|
  //  CacheOffsetContaining(mCachedOffset)    <--> IndexInCache(mCachedOffset)
  //           <------------------------------>
  const uint32_t mCacheBlockSize;
  int64_t mCachedOffset;
  uint32_t mCachedBytes;
  UniquePtr<char[]> mCachedBlock;
};

} // namespace mozilla

#endif
