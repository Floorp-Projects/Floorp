/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaResource_h_)
#define MediaResource_h_

#include "mozilla/Mutex.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsISeekableStream.h"
#include "nsIStreamingProtocolController.h"
#include "nsIStreamListener.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "MediaCache.h"
#include "MediaData.h"
#include "mozilla/Attributes.h"
#include "mozilla/TimeStamp.h"
#include "nsThreadUtils.h"
#include <algorithm>

// For HTTP seeking, if number of bytes needing to be
// seeked forward is less than this value then a read is
// done rather than a byte range request.
static const int64_t SEEK_VS_READ_THRESHOLD = 32*1024;

static const uint32_t HTTP_REQUESTED_RANGE_NOT_SATISFIABLE_CODE = 416;

// Number of bytes we have accumulated before we assume the connection download
// rate can be reliably calculated. 57 Segments at IW=3 allows slow start to
// reach a CWND of 30 (See bug 831998)
static const int64_t RELIABLE_DATA_THRESHOLD = 57 * 1460;

class nsIHttpChannel;
class nsIPrincipal;

namespace mozilla {

class MediaDecoder;
class MediaChannelStatistics;

/**
 * This class is useful for estimating rates of data passing through
 * some channel. The idea is that activity on the channel "starts"
 * and "stops" over time. At certain times data passes through the
 * channel (usually while the channel is active; data passing through
 * an inactive channel is ignored). The GetRate() function computes
 * an estimate of the "current rate" of the channel, which is some
 * kind of average of the data passing through over the time the
 * channel is active.
 *
 * All methods take "now" as a parameter so the user of this class can
 * control the timeline used.
 */
class MediaChannelStatistics {
public:
  MediaChannelStatistics() { Reset(); }

  explicit MediaChannelStatistics(MediaChannelStatistics * aCopyFrom)
  {
    MOZ_ASSERT(aCopyFrom);
    mAccumulatedBytes = aCopyFrom->mAccumulatedBytes;
    mAccumulatedTime = aCopyFrom->mAccumulatedTime;
    mLastStartTime = aCopyFrom->mLastStartTime;
    mIsStarted = aCopyFrom->mIsStarted;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaChannelStatistics)

  void Reset() {
    mLastStartTime = TimeStamp();
    mAccumulatedTime = TimeDuration(0);
    mAccumulatedBytes = 0;
    mIsStarted = false;
  }
  void Start() {
    if (mIsStarted)
      return;
    mLastStartTime = TimeStamp::Now();
    mIsStarted = true;
  }
  void Stop() {
    if (!mIsStarted)
      return;
    mAccumulatedTime += TimeStamp::Now() - mLastStartTime;
    mIsStarted = false;
  }
  void AddBytes(int64_t aBytes) {
    if (!mIsStarted) {
      // ignore this data, it may be related to seeking or some other
      // operation we don't care about
      return;
    }
    mAccumulatedBytes += aBytes;
  }
  double GetRateAtLastStop(bool* aReliable) {
    double seconds = mAccumulatedTime.ToSeconds();
    *aReliable = (seconds >= 1.0) ||
                 (mAccumulatedBytes >= RELIABLE_DATA_THRESHOLD);
    if (seconds <= 0.0)
      return 0.0;
    return static_cast<double>(mAccumulatedBytes)/seconds;
  }
  double GetRate(bool* aReliable) {
    TimeDuration time = mAccumulatedTime;
    if (mIsStarted) {
      time += TimeStamp::Now() - mLastStartTime;
    }
    double seconds = time.ToSeconds();
    *aReliable = (seconds >= 3.0) ||
                 (mAccumulatedBytes >= RELIABLE_DATA_THRESHOLD);
    if (seconds <= 0.0)
      return 0.0;
    return static_cast<double>(mAccumulatedBytes)/seconds;
  }
private:
  ~MediaChannelStatistics() {}
  int64_t      mAccumulatedBytes;
  TimeDuration mAccumulatedTime;
  TimeStamp    mLastStartTime;
  bool         mIsStarted;
};

// Forward declaration for use in MediaByteRange.
class TimestampedMediaByteRange;

// Represents a section of contiguous media, with a start and end offset.
// Used to denote ranges of data which are cached.
class MediaByteRange {
public:
  MediaByteRange() : mStart(0), mEnd(0) {}

  MediaByteRange(int64_t aStart, int64_t aEnd)
    : mStart(aStart), mEnd(aEnd)
  {
    NS_ASSERTION(mStart <= mEnd, "Range should end after start!");
  }

  explicit MediaByteRange(TimestampedMediaByteRange& aByteRange);

  bool IsNull() const {
    return mStart == 0 && mEnd == 0;
  }

  bool operator==(const MediaByteRange& aRange) const {
    return mStart == aRange.mStart && mEnd == aRange.mEnd;
  }

  // Clears byte range values.
  void Clear() {
    mStart = 0;
    mEnd = 0;
  }

  bool Contains(const MediaByteRange& aByteRange) const {
    return aByteRange.mStart >= mStart && aByteRange.mEnd <= mEnd;
  }

  MediaByteRange Extents(const MediaByteRange& aByteRange) const {
    if (IsNull()) {
      return aByteRange;
    }
    return MediaByteRange(std::min(mStart, aByteRange.mStart),
                          std::max(mEnd, aByteRange.mEnd));
  }

  int64_t Length() const {
    return mEnd - mStart;
  }

  int64_t mStart, mEnd;
};

// Represents a section of contiguous media, with a start and end offset, and
// a timestamp representing the start time.
class TimestampedMediaByteRange : public MediaByteRange {
public:
  TimestampedMediaByteRange() : MediaByteRange(), mStartTime(-1) {}

  TimestampedMediaByteRange(int64_t aStart, int64_t aEnd, int64_t aStartTime)
    : MediaByteRange(aStart, aEnd), mStartTime(aStartTime)
  {
    NS_ASSERTION(aStartTime >= 0, "Start time should not be negative!");
  }

  bool IsNull() const {
    return MediaByteRange::IsNull() && mStartTime == -1;
  }

  // Clears byte range values.
  void Clear() {
    MediaByteRange::Clear();
    mStartTime = -1;
  }

  // In usecs.
  int64_t mStartTime;
};

inline MediaByteRange::MediaByteRange(TimestampedMediaByteRange& aByteRange)
  : mStart(aByteRange.mStart), mEnd(aByteRange.mEnd)
{
  NS_ASSERTION(mStart < mEnd, "Range should end after start!");
}

class RtspMediaResource;

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
 * MediaResource::Create automatically chooses the best implementation class.
 */
class MediaResource : public nsISupports
{
public:
  // Our refcounting is threadsafe, and when our refcount drops to zero
  // we dispatch an event to the main thread to delete the MediaResource.
  // Note that this means it's safe for references to this object to be
  // released on a non main thread, but the destructor will always run on
  // the main thread.
  NS_DECL_THREADSAFE_ISUPPORTS

  // The following can be called on the main thread only:
  // Get the URI
  virtual nsIURI* URI() const { return nullptr; }
  // Close the resource, stop any listeners, channels, etc.
  // Cancels any currently blocking Read request and forces that request to
  // return an error.
  virtual nsresult Close() = 0;
  // Suspend any downloads that are in progress.
  // If aCloseImmediately is set, resources should be released immediately
  // since we don't expect to resume again any time soon. Otherwise we
  // may resume again soon so resources should be held for a little
  // while.
  virtual void Suspend(bool aCloseImmediately) = 0;
  // Resume any downloads that have been suspended.
  virtual void Resume() = 0;
  // Get the current principal for the channel
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal() = 0;
  // If this returns false, then we shouldn't try to clone this MediaResource
  // because its underlying resources are not suitable for reuse (e.g.
  // because the underlying connection has been lost, or this resource
  // just can't be safely cloned). If this returns true, CloneData could
  // still fail. If this returns false, CloneData should not be called.
  virtual bool CanClone() { return false; }
  // Create a new stream of the same type that refers to the same URI
  // with a new channel. Any cached data associated with the original
  // stream should be accessible in the new stream too.
  virtual already_AddRefed<MediaResource> CloneData(MediaDecoder* aDecoder) = 0;
  // Set statistics to be recorded to the object passed in.
  virtual void RecordStatisticsTo(MediaChannelStatistics *aStatistics) { }

  // These methods are called off the main thread.
  // The mode is initially MODE_PLAYBACK.
  virtual void SetReadMode(MediaCacheStream::ReadMode aMode) = 0;
  // This is the client's estimate of the playback rate assuming
  // the media plays continuously. The cache can't guess this itself
  // because it doesn't know when the decoder was paused, buffering, etc.
  virtual void SetPlaybackRate(uint32_t aBytesPerSecond) = 0;
  // Read up to aCount bytes from the stream. The buffer must have
  // enough room for at least aCount bytes. Stores the number of
  // actual bytes read in aBytes (0 on end of file).
  // May read less than aCount bytes if the number of
  // available bytes is less than aCount. Always check *aBytes after
  // read, and call again if necessary.
  virtual nsresult Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes) = 0;
  // Read up to aCount bytes from the stream. The read starts at
  // aOffset in the stream, seeking to that location initially if
  // it is not the current stream offset. The remaining arguments,
  // results and requirements are the same as per the Read method.
  virtual nsresult ReadAt(int64_t aOffset, char* aBuffer,
                          uint32_t aCount, uint32_t* aBytes) = 0;
  // This method returns nullptr if anything fails.
  // Otherwise, it returns an owned buffer.
  // MediaReadAt may return fewer bytes than requested if end of stream is
  // encountered. There is no need to call it again to get more data.
  virtual already_AddRefed<MediaByteBuffer> MediaReadAt(int64_t aOffset, uint32_t aCount)
  {
    nsRefPtr<MediaByteBuffer> bytes = new MediaByteBuffer();
    bool ok = bytes->SetLength(aCount, fallible);
    NS_ENSURE_TRUE(ok, nullptr);
    char* curr = reinterpret_cast<char*>(bytes->Elements());
    const char* start = curr;
    while (aCount > 0) {
      uint32_t bytesRead;
      nsresult rv = ReadAt(aOffset, curr, aCount, &bytesRead);
      NS_ENSURE_SUCCESS(rv, nullptr);
      if (!bytesRead) {
        break;
      }
      aOffset += bytesRead;
      aCount -= bytesRead;
      curr += bytesRead;
    }
    bytes->SetLength(curr - start);
    return bytes.forget();
  }

  // ReadAt without side-effects. Given that our MediaResource infrastructure
  // is very side-effecty, this accomplishes its job by checking the initial
  // position and seeking back to it. If the seek were to fail, a side-effect
  // might be observable.
  //
  // This method returns null if anything fails, including the failure to read
  // aCount bytes. Otherwise, it returns an owned buffer.
  virtual already_AddRefed<MediaByteBuffer> SilentReadAt(int64_t aOffset, uint32_t aCount)
  {
    int64_t pos = Tell();
    nsRefPtr<MediaByteBuffer> bytes = MediaReadAt(aOffset, aCount);
    Seek(nsISeekableStream::NS_SEEK_SET, pos);
    NS_ENSURE_TRUE(bytes && bytes->Length() == aCount, nullptr);
    return bytes.forget();
  }

  // Seek to the given bytes offset in the stream. aWhence can be
  // one of:
  //   NS_SEEK_SET
  //   NS_SEEK_CUR
  //   NS_SEEK_END
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
  virtual nsresult Seek(int32_t aWhence, int64_t aOffset) = 0;
  // Report the current offset in bytes from the start of the stream.
  virtual int64_t Tell() = 0;
  // Moves any existing channel loads into or out of background. Background
  // loads don't block the load event. This also determines whether or not any
  // new loads initiated (for example to seek) will be in the background.
  virtual void SetLoadInBackground(bool aLoadInBackground) {}
  // Ensures that the value returned by IsSuspendedByCache below is up to date
  // (i.e. the cache has examined this stream at least once).
  virtual void EnsureCacheUpToDate() {}

  // These can be called on any thread.
  // Cached blocks associated with this stream will not be evicted
  // while the stream is pinned.
  virtual void Pin() = 0;
  virtual void Unpin() = 0;
  // Get the estimated download rate in bytes per second (assuming no
  // pausing of the channel is requested by Gecko).
  // *aIsReliable is set to true if we think the estimate is useful.
  virtual double GetDownloadRate(bool* aIsReliable) = 0;
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
  // Returns the end of the bytes starting at the given offset
  // which are in cache.
  virtual int64_t GetCachedDataEnd(int64_t aOffset) = 0;
  // Returns true if all the data from aOffset to the end of the stream
  // is in cache. If the end of the stream is not known, we return false.
  virtual bool IsDataCachedToEndOfResource(int64_t aOffset) = 0;
  // Returns true if this stream is suspended by the cache because the
  // cache is full. If true then the decoder should try to start consuming
  // data, otherwise we may not be able to make progress.
  // MediaDecoder::NotifySuspendedStatusChanged is called when this
  // changes.
  // For resources using the media cache, this returns true only when all
  // streams for the same resource are all suspended.
  virtual bool IsSuspendedByCache() = 0;
  // Returns true if this stream has been suspended.
  virtual bool IsSuspended() = 0;
  // Reads only data which is cached in the media cache. If you try to read
  // any data which overlaps uncached data, or if aCount bytes otherwise can't
  // be read, this function will return failure. This function be called from
  // any thread, and it is the only read operation which is safe to call on
  // the main thread, since it's guaranteed to be non blocking.
  virtual nsresult ReadFromCache(char* aBuffer,
                                 int64_t aOffset,
                                 uint32_t aCount) = 0;
  // Returns true if the resource can be seeked to unbuffered ranges, i.e.
  // for an HTTP network stream this returns true if HTTP1.1 Byte Range
  // requests are supported by the connection/server.
  virtual bool IsTransportSeekable() = 0;

  /**
   * Create a resource, reading data from the channel. Call on main thread only.
   * The caller must follow up by calling resource->Open().
   */
  static already_AddRefed<MediaResource> Create(MediaDecoder* aDecoder, nsIChannel* aChannel);

  /**
   * Open the stream. This creates a stream listener and returns it in
   * aStreamListener; this listener needs to be notified of incoming data.
   */
  virtual nsresult Open(nsIStreamListener** aStreamListener) = 0;

  /**
   * Fills aRanges with MediaByteRanges representing the data which is cached
   * in the media cache. Stream should be pinned during call and while
   * aRanges is being used.
   */
  virtual nsresult GetCachedRanges(nsTArray<MediaByteRange>& aRanges) = 0;

  // Ensure that the media cache writes any data held in its partial block.
  // Called on the main thread only.
  virtual void FlushCache() { }

  // Notify that the last data byte range was loaded.
  virtual void NotifyLastByteRange() { }

  // Returns the content type of the resource. This is copied from the
  // nsIChannel when the MediaResource is created. Safe to call from
  // any thread.
  virtual const nsCString& GetContentType() const = 0;

  // Get the RtspMediaResource pointer if this MediaResource really is a
  // RtspMediaResource. For calling Rtsp specific functions.
  virtual RtspMediaResource* GetRtspPointer() {
    return nullptr;
  }

  // Return true if the stream is a live stream
  virtual bool IsRealTime() {
    return false;
  }

  // Returns true if the resource is a live stream.
  virtual bool IsLiveStream()
  {
    return GetLength() == -1;
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    return 0;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  const nsCString& GetContentURL() const { return EmptyCString(); }

protected:
  virtual ~MediaResource() {};

private:
  void Destroy();
};

class BaseMediaResource : public MediaResource {
public:
  virtual nsIURI* URI() const override { return mURI; }
  virtual void SetLoadInBackground(bool aLoadInBackground) override;

  virtual size_t SizeOfExcludingThis(
                  MallocSizeOf aMallocSizeOf) const override
  {
    // Might be useful to track in the future:
    // - mChannel
    // - mURI (possibly owned, looks like just a ref from mChannel)
    // Not owned:
    // - mDecoder
    size_t size = MediaResource::SizeOfExcludingThis(aMallocSizeOf);
    size += mContentType.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

    return size;
  }

  virtual size_t SizeOfIncludingThis(
                  MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  // Returns the url of the resource. Safe to call from any thread?
  const nsCString& GetContentURL() const
  {
    return mContentURL;
  }

protected:
  BaseMediaResource(MediaDecoder* aDecoder,
                    nsIChannel* aChannel,
                    nsIURI* aURI,
                    const nsACString& aContentType) :
    mDecoder(aDecoder),
    mChannel(aChannel),
    mURI(aURI),
    mContentType(aContentType),
    mLoadInBackground(false)
  {
    MOZ_COUNT_CTOR(BaseMediaResource);
    NS_ASSERTION(!mContentType.IsEmpty(), "Must know content type");
    mURI->GetSpec(mContentURL);
  }
  virtual ~BaseMediaResource()
  {
    MOZ_COUNT_DTOR(BaseMediaResource);
  }

  virtual const nsCString& GetContentType() const override
  {
    return mContentType;
  }

  // Set the request's load flags to aFlags.  If the request is part of a
  // load group, the request is removed from the group, the flags are set, and
  // then the request is added back to the load group.
  void ModifyLoadFlags(nsLoadFlags aFlags);

  // Dispatches an event to call MediaDecoder::NotifyBytesConsumed(aNumBytes, aOffset)
  // on the main thread. This is called automatically after every read.
  void DispatchBytesConsumed(int64_t aNumBytes, int64_t aOffset);

  // This is not an nsCOMPointer to prevent a circular reference
  // between the decoder to the media stream object. The stream never
  // outlives the lifetime of the decoder.
  MediaDecoder* mDecoder;

  // Channel used to download the media data. Must be accessed
  // from the main thread only.
  nsCOMPtr<nsIChannel> mChannel;

  // URI in case the stream needs to be re-opened. Access from
  // main thread only.
  nsCOMPtr<nsIURI> mURI;

  // Content-Type of the channel. This is copied from the nsIChannel when the
  // MediaResource is created. This is constant, so accessing from any thread
  // is safe.
  const nsAutoCString mContentType;

  // Copy of the url of the channel resource.
  nsAutoCString mContentURL;

  // True if SetLoadInBackground() has been called with
  // aLoadInBackground = true, i.e. when the document load event is not
  // blocked by this resource, and all channel loads will be in the
  // background.
  bool mLoadInBackground;
};

/**
 * This is the MediaResource implementation that wraps Necko channels.
 * Much of its functionality is actually delegated to MediaCache via
 * an underlying MediaCacheStream.
 *
 * All synchronization is performed by MediaCacheStream; all off-main-
 * thread operations are delegated directly to that object.
 */
class ChannelMediaResource : public BaseMediaResource
{
public:
  ChannelMediaResource(MediaDecoder* aDecoder,
                       nsIChannel* aChannel,
                       nsIURI* aURI,
                       const nsACString& aContentType);
  ~ChannelMediaResource();

  // These are called on the main thread by MediaCache. These must
  // not block or grab locks, because the media cache is holding its lock.
  // Notify that data is available from the cache. This can happen even
  // if this stream didn't read any data, since another stream might have
  // received data for the same resource.
  void CacheClientNotifyDataReceived();
  // Notify that we reached the end of the stream. This can happen even
  // if this stream didn't read any data, since another stream might have
  // received data for the same resource.
  void CacheClientNotifyDataEnded(nsresult aStatus);
  // Notify that the principal for the cached resource changed.
  void CacheClientNotifyPrincipalChanged();
  // Notify the decoder that the cache suspended status changed.
  void CacheClientNotifySuspendedStatusChanged();

  // These are called on the main thread by MediaCache. These shouldn't block,
  // but they may grab locks --- the media cache is not holding its lock
  // when these are called.
  // Start a new load at the given aOffset. The old load is cancelled
  // and no more data from the old load will be notified via
  // MediaCacheStream::NotifyDataReceived/Ended.
  // This can fail.
  nsresult CacheClientSeek(int64_t aOffset, bool aResume);
  // Suspend the current load since data is currently not wanted
  nsresult CacheClientSuspend();
  // Resume the current load since data is wanted again
  nsresult CacheClientResume();

  // Ensure that the media cache writes any data held in its partial block.
  // Called on the main thread.
  virtual void FlushCache() override;

  // Notify that the last data byte range was loaded.
  virtual void NotifyLastByteRange() override;

  // Main thread
  virtual nsresult Open(nsIStreamListener** aStreamListener) override;
  virtual nsresult Close() override;
  virtual void     Suspend(bool aCloseImmediately) override;
  virtual void     Resume() override;
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal() override;
  // Return true if the stream has been closed.
  bool IsClosed() const { return mCacheStream.IsClosed(); }
  virtual bool     CanClone() override;
  virtual already_AddRefed<MediaResource> CloneData(MediaDecoder* aDecoder) override;
  // Set statistics to be recorded to the object passed in. If not called,
  // |ChannelMediaResource| will create it's own statistics objects in |Open|.
  void RecordStatisticsTo(MediaChannelStatistics *aStatistics) override {
    NS_ASSERTION(aStatistics, "Statistics param cannot be null!");
    MutexAutoLock lock(mLock);
    if (!mChannelStatistics) {
      mChannelStatistics = aStatistics;
    }
  }
  virtual nsresult ReadFromCache(char* aBuffer, int64_t aOffset, uint32_t aCount) override;
  virtual void     EnsureCacheUpToDate() override;

  // Other thread
  virtual void     SetReadMode(MediaCacheStream::ReadMode aMode) override;
  virtual void     SetPlaybackRate(uint32_t aBytesPerSecond) override;
  virtual nsresult Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes) override;
  virtual nsresult ReadAt(int64_t offset, char* aBuffer,
                          uint32_t aCount, uint32_t* aBytes) override;
  virtual already_AddRefed<MediaByteBuffer> MediaReadAt(int64_t aOffset, uint32_t aCount) override;
  virtual nsresult Seek(int32_t aWhence, int64_t aOffset) override;
  virtual int64_t  Tell() override;

  // Any thread
  virtual void    Pin() override;
  virtual void    Unpin() override;
  virtual double  GetDownloadRate(bool* aIsReliable) override;
  virtual int64_t GetLength() override;
  virtual int64_t GetNextCachedData(int64_t aOffset) override;
  virtual int64_t GetCachedDataEnd(int64_t aOffset) override;
  virtual bool    IsDataCachedToEndOfResource(int64_t aOffset) override;
  virtual bool    IsSuspendedByCache() override;
  virtual bool    IsSuspended() override;
  virtual bool    IsTransportSeekable() override;

  virtual size_t SizeOfExcludingThis(
                      MallocSizeOf aMallocSizeOf) const override {
    // Might be useful to track in the future:
    //   - mListener (seems minor)
    //   - mChannelStatistics (seems minor)
    //     owned if RecordStatisticsTo is not called
    //   - mDataReceivedEvent (seems minor)
    size_t size = BaseMediaResource::SizeOfExcludingThis(aMallocSizeOf);
    size += mCacheStream.SizeOfExcludingThis(aMallocSizeOf);

    return size;
  }

  virtual size_t SizeOfIncludingThis(
                      MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  class Listener final : public nsIStreamListener,
                         public nsIInterfaceRequestor,
                         public nsIChannelEventSink
  {
    ~Listener() {}
  public:
    explicit Listener(ChannelMediaResource* aResource) : mResource(aResource) {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSICHANNELEVENTSINK
    NS_DECL_NSIINTERFACEREQUESTOR

    void Revoke() { mResource = nullptr; }

  private:
    nsRefPtr<ChannelMediaResource> mResource;
  };
  friend class Listener;

  virtual nsresult GetCachedRanges(nsTArray<MediaByteRange>& aRanges) override;

protected:
  // These are called on the main thread by Listener.
  nsresult OnStartRequest(nsIRequest* aRequest);
  nsresult OnStopRequest(nsIRequest* aRequest, nsresult aStatus);
  nsresult OnDataAvailable(nsIRequest* aRequest,
                           nsIInputStream* aStream,
                           uint32_t aCount);
  nsresult OnChannelRedirect(nsIChannel* aOld, nsIChannel* aNew, uint32_t aFlags);

  // Opens the channel, using an HTTP byte range request to start at mOffset
  // if possible. Main thread only.
  nsresult OpenChannel(nsIStreamListener** aStreamListener);
  nsresult RecreateChannel();
  // Add headers to HTTP request. Main thread only.
  nsresult SetupChannelHeaders();
  // Closes the channel. Main thread only.
  void CloseChannel();

  // Parses 'Content-Range' header and returns results via parameters.
  // Returns error if header is not available, values are not parse-able or
  // values are out of range.
  nsresult ParseContentRangeHeader(nsIHttpChannel * aHttpChan,
                                   int64_t& aRangeStart,
                                   int64_t& aRangeEnd,
                                   int64_t& aRangeTotal);

  void DoNotifyDataReceived();

  static NS_METHOD CopySegmentToCache(nsIInputStream *aInStream,
                                      void *aClosure,
                                      const char *aFromSegment,
                                      uint32_t aToOffset,
                                      uint32_t aCount,
                                      uint32_t *aWriteCount);

  // Suspend the channel only if the channels is currently downloading data.
  // If it isn't we set a flag, mIgnoreResume, so that PossiblyResume knows
  // whether to acutually resume or not.
  void PossiblySuspend();

  // Resume from a suspend if we actually suspended (See PossiblySuspend).
  void PossiblyResume();

  // Main thread access only
  int64_t            mOffset;
  nsRefPtr<Listener> mListener;
  // A data received event for the decoder that has been dispatched but has
  // not yet been processed.
  nsRevocableEventPtr<nsRunnableMethod<ChannelMediaResource, void, false> > mDataReceivedEvent;
  uint32_t           mSuspendCount;
  // When this flag is set, if we get a network error we should silently
  // reopen the stream.
  bool               mReopenOnError;
  // When this flag is set, we should not report the next close of the
  // channel.
  bool               mIgnoreClose;

  // Any thread access
  MediaCacheStream mCacheStream;

  // This lock protects mChannelStatistics
  Mutex               mLock;
  nsRefPtr<MediaChannelStatistics> mChannelStatistics;

  // True if we couldn't suspend the stream and we therefore don't want
  // to resume later. This is usually due to the channel not being in the
  // isPending state at the time of the suspend request.
  bool mIgnoreResume;

  // Start and end offset of the bytes to be requested.
  MediaByteRange mByteRange;

  // True if the stream can seek into unbuffered ranged, i.e. if the
  // connection supports byte range requests.
  bool mIsTransportSeekable;
};

/**
 * RAII class that handles pinning and unpinning for MediaResource and derived.
 * This should be used when making calculations that involve potentially-cached
 * MediaResource data, so that the state of the world can't change out from under
 * us.
 */
template<class T>
class MOZ_STACK_CLASS AutoPinned {
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

} // namespace mozilla

#endif
