/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaCache_h_
#define MediaCache_h_

#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"
#include "Intervals.h"
#include "mozilla/UniquePtr.h"

class nsIPrincipal;

namespace mozilla {
// defined in MediaResource.h
class ChannelMediaResource;
typedef media::IntervalSet<int64_t> MediaByteRangeSet;
class MediaResource;
class ReentrantMonitorAutoEnter;

/**
 * Media applications want fast, "on demand" random access to media data,
 * for pausing, seeking, etc. But we are primarily interested
 * in transporting media data using HTTP over the Internet, which has
 * high latency to open a connection, requires a new connection for every
 * seek, may not even support seeking on some connections (especially
 * live streams), and uses a push model --- data comes from the server
 * and you don't have much control over the rate. Also, transferring data
 * over the Internet can be slow and/or unpredictable, so we want to read
 * ahead to buffer and cache as much data as possible.
 *
 * The job of the media cache is to resolve this impedance mismatch.
 * The media cache reads data from Necko channels into file-backed storage,
 * and offers a random-access file-like API to the stream data
 * (MediaCacheStream). Along the way it solves several problems:
 * -- The cache intelligently reads ahead to prefetch data that may be
 * needed in the future
 * -- The size of the cache is bounded so that we don't fill up
 * storage with read-ahead data
 * -- Cache replacement is managed globally so that the most valuable
 * data (across all streams) is retained
 * -- The cache can suspend Necko channels temporarily when their data is
 * not wanted (yet)
 * -- The cache translates file-like seek requests to HTTP seeks,
 * including optimizations like not triggering a new seek if it would
 * be faster to just keep reading until we reach the seek point. The
 * "seek to EOF" idiom to determine file size is also handled efficiently
 * (seeking to EOF and then seeking back to the previous offset does not
 * trigger any Necko activity)
 * -- The cache also handles the case where the server does not support
 * seeking
 * -- Necko can only send data to the main thread, but MediaCacheStream
 * can distribute data to any thread
 * -- The cache exposes APIs so clients can detect what data is
 * currently held
 *
 * Note that although HTTP is the most important transport and we only
 * support transport-level seeking via HTTP byte-ranges, the media cache
 * works with any kind of Necko channels and provides random access to
 * cached data even for, e.g., FTP streams.
 *
 * The media cache is not persistent. It does not currently allow
 * data from one load to be used by other loads, either within the same
 * browser session or across browser sessions. The media cache file
 * is marked "delete on close" so it will automatically disappear in the
 * event of a browser crash or shutdown.
 *
 * The media cache is block-based. Streams are divided into blocks of a
 * fixed size (currently 4K) and we cache blocks. A single cache contains
 * blocks for all streams.
 *
 * The cache size is controlled by the media.cache_size preference
 * (which is in KB). The default size is 500MB.
 *
 * The replacement policy predicts a "time of next use" for each block
 * in the cache. When we need to free a block, the block with the latest
 * "time of next use" will be evicted. Blocks are divided into
 * different classes, each class having its own predictor:
 * FREE_BLOCK: these blocks are effectively infinitely far in the future;
 * a free block will always be chosen for replacement before other classes
 * of blocks.
 * METADATA_BLOCK: these are blocks that contain data that has been read
 * by the decoder in "metadata mode", e.g. while the decoder is searching
 * the stream during a seek operation. These blocks are managed with an
 * LRU policy; the "time of next use" is predicted to be as far in the
 * future as the last use was in the past.
 * PLAYED_BLOCK: these are blocks that have not been read in "metadata
 * mode", and contain data behind the current decoder read point. (They
 * may not actually have been read by the decoder, if the decoder seeked
 * forward.) These blocks are managed with an LRU policy except that we add
 * REPLAY_DELAY seconds of penalty to their predicted "time of next use",
 * to reflect the uncertainty about whether replay will actually happen
 * or not.
 * READAHEAD_BLOCK: these are blocks that have not been read in
 * "metadata mode" and that are entirely ahead of the current decoder
 * read point. (They may actually have been read by the decoder in the
 * past if the decoder has since seeked backward.) We predict the
 * time of next use for these blocks by assuming steady playback and
 * dividing the number of bytes between the block and the current decoder
 * read point by the decoder's estimate of its playback rate in bytes
 * per second. This ensures that the blocks farthest ahead are considered
 * least valuable.
 * For efficient prediction of the "latest time of next use", we maintain
 * linked lists of blocks in each class, ordering blocks by time of
 * next use. READAHEAD_BLOCKS have one linked list per stream, since their
 * time of next use depends on stream parameters, but the other lists
 * are global.
 *
 * A block containing a current decoder read point can contain data
 * both behind and ahead of the read point. It will be classified as a
 * PLAYED_BLOCK but we will give it special treatment so it is never
 * evicted --- it actually contains the highest-priority readahead data
 * as well as played data.
 *
 * "Time of next use" estimates are also used for flow control. When
 * reading ahead we can predict the time of next use for the data that
 * will be read. If the predicted time of next use is later then the
 * prediction for all currently cached blocks, and the cache is full, then
 * we should suspend reading from the Necko channel.
 *
 * Unfortunately suspending the Necko channel can't immediately stop the
 * flow of data from the server. First our desire to suspend has to be
 * transmitted to the server (in practice, Necko stops reading from the
 * socket, which causes the kernel to shrink its advertised TCP receive
 * window size to zero). Then the server can stop sending the data, but
 * we will receive data roughly corresponding to the product of the link
 * bandwidth multiplied by the round-trip latency. We deal with this by
 * letting the cache overflow temporarily and then trimming it back by
 * moving overflowing blocks back into the body of the cache, replacing
 * less valuable blocks as they become available. We try to avoid simply
 * discarding overflowing readahead data.
 *
 * All changes to the actual contents of the cache happen on the main
 * thread, since that's where Necko's notifications happen.
 *
 * The media cache maintains at most one Necko channel for each stream.
 * (In the future it might be advantageous to relax this, e.g. so that a
 * seek to near the end of the file can happen without disturbing
 * the loading of data from the beginning of the file.) The Necko channel
 * is managed through ChannelMediaResource; MediaCache does not
 * depend on Necko directly.
 *
 * Every time something changes that might affect whether we want to
 * read from a Necko channel, or whether we want to seek on the Necko
 * channel --- such as data arriving or data being consumed by the
 * decoder --- we asynchronously trigger MediaCache::Update on the main
 * thread. That method implements most cache policy. It evaluates for
 * each stream whether we want to suspend or resume the stream and what
 * offset we should seek to, if any. It is also responsible for trimming
 * back the cache size to its desired limit by moving overflowing blocks
 * into the main part of the cache.
 *
 * Streams can be opened in non-seekable mode. In non-seekable mode,
 * the cache will only call ChannelMediaResource::CacheClientSeek with
 * a 0 offset. The cache tries hard not to discard readahead data
 * for non-seekable streams, since that could trigger a potentially
 * disastrous re-read of the entire stream. It's up to cache clients
 * to try to avoid requesting seeks on such streams.
 *
 * MediaCache has a single internal monitor for all synchronization.
 * This is treated as the lowest level monitor in the media code. So,
 * we must not acquire any MediaDecoder locks or MediaResource locks
 * while holding the MediaCache lock. But it's OK to hold those locks
 * and then get the MediaCache lock.
 *
 * MediaCache associates a principal with each stream. CacheClientSeek
 * can trigger new HTTP requests; due to redirects to other domains,
 * each HTTP load can return data with a different principal. This
 * principal must be passed to NotifyDataReceived, and MediaCache
 * will detect when different principals are associated with data in the
 * same stream, and replace them with a null principal.
 */
class MediaCache;

/**
 * If the cache fails to initialize then Init will fail, so nonstatic
 * methods of this class can assume gMediaCache is non-null.
 *
 * This class can be directly embedded as a value.
 */
class MediaCacheStream {
public:
  // This needs to be a power of two
  static const int64_t BLOCK_SIZE = 32768;

  enum ReadMode {
    MODE_METADATA,
    MODE_PLAYBACK
  };

  // aClient provides the underlying transport that cache will use to read
  // data for this stream.
  explicit MediaCacheStream(ChannelMediaResource* aClient);
  ~MediaCacheStream();

  // Set up this stream with the cache. Can fail on OOM. One
  // of InitAsClone or Init must be called before any other method on
  // this class. Does nothing if already initialized.
  nsresult Init();

  // Set up this stream with the cache, assuming it's for the same data
  // as the aOriginal stream. Can fail on OOM. Exactly one
  // of InitAsClone or Init must be called before any other method on
  // this class. Does nothing if already initialized.
  nsresult InitAsClone(MediaCacheStream* aOriginal);

  // These are called on the main thread.
  // Tell us whether the stream is seekable or not. Non-seekable streams
  // will always pass 0 for aOffset to CacheClientSeek. This should only
  // be called while the stream is at channel offset 0. Seekability can
  // change during the lifetime of the MediaCacheStream --- every time
  // we do an HTTP load the seekability may be different (and sometimes
  // is, in practice, due to the effects of caching proxies).
  void SetTransportSeekable(bool aIsTransportSeekable);
  // This must be called (and return) before the ChannelMediaResource
  // used to create this MediaCacheStream is deleted.
  void Close();
  // This returns true when the stream has been closed
  bool IsClosed() const { return mClosed; }
  // Returns true when this stream is can be shared by a new resource load
  bool IsAvailableForSharing() const
  {
    return !mClosed &&
      (!mDidNotifyDataEnded || NS_SUCCEEDED(mNotifyDataEndedStatus));
  }
  // Get the principal for this stream. Anything accessing the contents of
  // this stream must have a principal that subsumes this principal.
  nsIPrincipal* GetCurrentPrincipal() { return mPrincipal; }
  // Ensure a global media cache update has run with this stream present.
  // This ensures the cache has had a chance to suspend or unsuspend this stream.
  // Called only on main thread. This can change the state of streams, fire
  // notifications, etc.
  void EnsureCacheUpdate();

  // These callbacks are called on the main thread by the client
  // when data has been received via the channel.
  // Tells the cache what the server said the data length is going to be.
  // The actual data length may be greater (we receive more data than
  // specified) or smaller (the stream ends before we reach the given
  // length), because servers can lie. The server's reported data length
  // *and* the actual data length can even vary over time because a
  // misbehaving server may feed us a different stream after each seek
  // operation. So this is really just a hint. The cache may however
  // stop reading (suspend the channel) when it thinks we've read all the
  // data available based on an incorrect reported length. Seeks relative
  // EOF also depend on the reported length if we haven't managed to
  // read the whole stream yet.
  void NotifyDataLength(int64_t aLength);
  // Notifies the cache that a load has begun. We pass the offset
  // because in some cases the offset might not be what the cache
  // requested. In particular we might unexpectedly start providing
  // data at offset 0. This need not be called if the offset is the
  // offset that the cache requested in
  // ChannelMediaResource::CacheClientSeek. This can be called at any
  // time by the client, not just after a CacheClientSeek.
  void NotifyDataStarted(int64_t aOffset);
  // Notifies the cache that data has been received. The stream already
  // knows the offset because data is received in sequence and
  // the starting offset is known via NotifyDataStarted or because
  // the cache requested the offset in
  // ChannelMediaResource::CacheClientSeek, or because it defaulted to 0.
  // We pass in the principal that was used to load this data.
  void NotifyDataReceived(int64_t aSize, const char* aData,
                          nsIPrincipal* aPrincipal);
  // Notifies the cache that the current bytes should be written to disk.
  // Called on the main thread.
  void FlushPartialBlock();
  // Notifies the cache that the channel has closed with the given status.
  void NotifyDataEnded(nsresult aStatus);

  // Notifies the stream that the channel is reopened. The stream should
  // reset variables such as |mDidNotifyDataEnded|.
  void NotifyChannelRecreated();

  // These methods can be called on any thread.
  // Cached blocks associated with this stream will not be evicted
  // while the stream is pinned.
  void Pin();
  void Unpin();
  // See comments above for NotifyDataLength about how the length
  // can vary over time. Returns -1 if no length is known. Returns the
  // reported length if we haven't got any better information. If
  // the stream ended normally we return the length we actually got.
  // If we've successfully read data beyond the originally reported length,
  // we return the end of the data we've read.
  int64_t GetLength();
  // Returns the unique resource ID. Call only on the main thread or while
  // holding the media cache lock.
  int64_t GetResourceID() { return mResourceID; }
  // Returns the end of the bytes starting at the given offset
  // which are in cache.
  int64_t GetCachedDataEnd(int64_t aOffset);
  // Returns the offset of the first byte of cached data at or after aOffset,
  // or -1 if there is no such cached data.
  int64_t GetNextCachedData(int64_t aOffset);
  // Fills aRanges with the ByteRanges representing the data which is currently
  // cached. Locks the media cache while running, to prevent any ranges
  // growing. The stream should be pinned while this runs and while its results
  // are used, to ensure no data is evicted.
  nsresult GetCachedRanges(MediaByteRangeSet& aRanges);

  // Reads from buffered data only. Will fail if not all data to be read is
  // in the cache. Will not mark blocks as read. Can be called from the main
  // thread. It's the caller's responsibility to wrap the call in a pin/unpin,
  // and also to check that the range they want is cached before calling this.
  nsresult ReadFromCache(char* aBuffer,
                         int64_t aOffset,
                         int64_t aCount);

  // IsDataCachedToEndOfStream returns true if all the data from
  // aOffset to the end of the stream (the server-reported end, if the
  // real end is not known) is in cache. If we know nothing about the
  // end of the stream, this returns false.
  bool IsDataCachedToEndOfStream(int64_t aOffset);
  // The mode is initially MODE_PLAYBACK.
  void SetReadMode(ReadMode aMode);
  // This is the client's estimate of the playback rate assuming
  // the media plays continuously. The cache can't guess this itself
  // because it doesn't know when the decoder was paused, buffering, etc.
  // Do not pass zero.
  void SetPlaybackRate(uint32_t aBytesPerSecond);
  // Returns the last set value of SetTransportSeekable.
  bool IsTransportSeekable();

  // Returns true when all streams for this resource are suspended or their
  // channel has ended.
  bool AreAllStreamsForResourceSuspended();

  // These methods must be called on a different thread from the main
  // thread. They should always be called on the same thread for a given
  // stream.
  // This can fail when aWhence is NS_SEEK_END and no stream length
  // is known.
  nsresult Seek(int32_t aWhence, int64_t aOffset);
  int64_t Tell();
  // *aBytes gets the number of bytes that were actually read. This can
  // be less than aCount. If the first byte of data is not in the cache,
  // this will block until the data is available or the stream is
  // closed, otherwise it won't block.
  nsresult Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes);
  // Seeks to aOffset in the stream then performs a Read operation. See
  // 'Read' for argument and return details.
  nsresult ReadAt(int64_t aOffset, char* aBuffer,
                  uint32_t aCount, uint32_t* aBytes);

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;

private:
  friend class MediaCache;

  /**
   * A doubly-linked list of blocks. Add/Remove/Get methods are all
   * constant time. We declare this here so that a stream can contain a
   * BlockList of its read-ahead blocks. Blocks are referred to by index
   * into the MediaCache::mIndex array.
   *
   * Blocks can belong to more than one list at the same time, because
   * the next/prev pointers are not stored in the block.
   */
  class BlockList {
  public:
    BlockList() : mFirstBlock(-1), mCount(0) {}
    ~BlockList() {
      NS_ASSERTION(mFirstBlock == -1 && mCount == 0,
                   "Destroying non-empty block list");
    }
    void AddFirstBlock(int32_t aBlock);
    void AddAfter(int32_t aBlock, int32_t aBefore);
    void RemoveBlock(int32_t aBlock);
    // Returns the first block in the list, or -1 if empty
    int32_t GetFirstBlock() const { return mFirstBlock; }
    // Returns the last block in the list, or -1 if empty
    int32_t GetLastBlock() const;
    // Returns the next block in the list after aBlock or -1 if
    // aBlock is the last block
    int32_t GetNextBlock(int32_t aBlock) const;
    // Returns the previous block in the list before aBlock or -1 if
    // aBlock is the first block
    int32_t GetPrevBlock(int32_t aBlock) const;
    bool IsEmpty() const { return mFirstBlock < 0; }
    int32_t GetCount() const { return mCount; }
    // The contents of aBlockIndex1 and aBlockIndex2 have been swapped
    void NotifyBlockSwapped(int32_t aBlockIndex1, int32_t aBlockIndex2);
#ifdef DEBUG
    // Verify linked-list invariants
    void Verify();
#else
    void Verify() {}
#endif

    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  private:
    struct Entry : public nsUint32HashKey {
      explicit Entry(KeyTypePointer aKey) : nsUint32HashKey(aKey) { }
      Entry(const Entry& toCopy) : nsUint32HashKey(&toCopy.GetKey()),
        mNextBlock(toCopy.mNextBlock), mPrevBlock(toCopy.mPrevBlock) {}

      int32_t mNextBlock;
      int32_t mPrevBlock;
    };
    nsTHashtable<Entry> mEntries;

    // The index of the first block in the list, or -1 if the list is empty.
    int32_t mFirstBlock;
    // The number of blocks in the list.
    int32_t mCount;
  };

  // Returns the end of the bytes starting at the given offset
  // which are in cache.
  // This method assumes that the cache monitor is held and can be called on
  // any thread.
  int64_t GetCachedDataEndInternal(int64_t aOffset);
  // Returns the offset of the first byte of cached data at or after aOffset,
  // or -1 if there is no such cached data.
  // This method assumes that the cache monitor is held and can be called on
  // any thread.
  int64_t GetNextCachedDataInternal(int64_t aOffset);
  // Writes |mPartialBlock| to disk.
  // Used by |NotifyDataEnded| and |FlushPartialBlock|.
  // If |aNotifyAll| is true, this function will wake up readers who may be
  // waiting on the media cache monitor. Called on the main thread only.
  void FlushPartialBlockInternal(bool aNotify, ReentrantMonitorAutoEnter& aReentrantMonitor);
  // A helper function to do the work of closing the stream. Assumes
  // that the cache monitor is held. Main thread only.
  // aReentrantMonitor is the nsAutoReentrantMonitor wrapper holding the cache monitor.
  // This is used to NotifyAll to wake up threads that might be
  // blocked on reading from this stream.
  void CloseInternal(ReentrantMonitorAutoEnter& aReentrantMonitor);
  // Update mPrincipal given that data has been received from aPrincipal
  bool UpdatePrincipal(nsIPrincipal* aPrincipal);

  // These fields are main-thread-only.
  ChannelMediaResource*  mClient;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  // Set to true when Init or InitAsClone has been called
  bool                   mInitialized;
  // Set to true when MediaCache::Update() has finished while this stream
  // was present.
  bool                   mHasHadUpdate;
  // Set to true when the stream has been closed either explicitly or
  // due to an internal cache error
  bool                   mClosed;
  // True if CacheClientNotifyDataEnded has been called for this stream.
  bool                   mDidNotifyDataEnded;

  // The following fields must be written holding the cache's monitor and
  // only on the main thread, thus can be read either on the main thread
  // or while holding the cache's monitor.

  // This is a unique ID representing the resource we're loading.
  // All streams with the same mResourceID are loading the same
  // underlying resource and should share data.
  int64_t mResourceID;
  // The last reported seekability state for the underlying channel
  bool mIsTransportSeekable;
  // True if the cache has suspended our channel because the cache is
  // full and the priority of the data that would be received is lower
  // than the priority of the data already in the cache
  bool mCacheSuspended;
  // True if the channel ended and we haven't seeked it again.
  bool mChannelEnded;
  // The offset where the next data from the channel will arrive
  int64_t      mChannelOffset;
  // The reported or discovered length of the data, or -1 if nothing is
  // known
  int64_t      mStreamLength;

  // The following fields are protected by the cache's monitor can can be written
  // by any thread.

  // The offset where the reader is positioned in the stream
  int64_t           mStreamOffset;
  // For each block in the stream data, maps to the cache entry for the
  // block, or -1 if the block is not cached.
  nsTArray<int32_t> mBlocks;
  // The list of read-ahead blocks, ordered by stream offset; the first
  // block is the earliest in the stream (so the last block will be the
  // least valuable).
  BlockList         mReadaheadBlocks;
  // The list of metadata blocks; the first block is the most recently used
  BlockList         mMetadataBlocks;
  // The list of played-back blocks; the first block is the most recently used
  BlockList         mPlayedBlocks;
  // The last reported estimate of the decoder's playback rate
  uint32_t          mPlaybackBytesPerSecond;
  // The number of times this stream has been Pinned without a
  // corresponding Unpin
  uint32_t          mPinCount;
  // The status used when we did CacheClientNotifyDataEnded. Only valid
  // when mDidNotifyDataEnded is true.
  nsresult          mNotifyDataEndedStatus;
  // The last reported read mode
  ReadMode          mCurrentMode;
  // True if some data in mPartialBlockBuffer has been read as metadata
  bool              mMetadataInPartialBlockBuffer;

  // The following field is protected by the cache's monitor but are
  // only written on the main thread.

  // Data received for the block containing mChannelOffset. Data needs
  // to wait here so we can write back a complete block. The first
  // mChannelOffset%BLOCK_SIZE bytes have been filled in with good data,
  // the rest are garbage.
  // Use int64_t so that the data is well-aligned.
  // Heap allocate this buffer since the exact power-of-2 will cause allocation
  // slop when combined with the rest of the object members.
  UniquePtr<int64_t[]> mPartialBlockBuffer;
};

} // namespace mozilla

#endif
