/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaCache.h"

#include "ChannelMediaResource.h"
#include "FileBlockCache.h"
#include "MediaBlockCacheBase.h"
#include "MediaPrefs.h"
#include "MediaResource.h"
#include "MemoryBlockCache.h"
#include "mozilla/Attributes.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/Telemetry.h"
#include "nsContentUtils.h"
#include "nsIObserverService.h"
#include "nsIPrincipal.h"
#include "nsPrintfCString.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "prio.h"
#include <algorithm>

namespace mozilla {

#undef LOG
#undef LOGI
#undef LOGE
LazyLogModule gMediaCacheLog("MediaCache");
#define LOG(...) MOZ_LOG(gMediaCacheLog, LogLevel::Debug, (__VA_ARGS__))
#define LOGI(...) MOZ_LOG(gMediaCacheLog, LogLevel::Info, (__VA_ARGS__))
#define LOGE(...) NS_DebugBreak(NS_DEBUG_WARNING, nsPrintfCString(__VA_ARGS__).get(), nullptr, __FILE__, __LINE__)

// For HTTP seeking, if number of bytes needing to be
// seeked forward is less than this value then a read is
// done rather than a byte range request.
//
// If we assume a 100Mbit connection, and assume reissuing an HTTP seek causes
// a delay of 200ms, then in that 200ms we could have simply read ahead 2MB. So
// setting SEEK_VS_READ_THRESHOLD to 1MB sounds reasonable.
static const int64_t SEEK_VS_READ_THRESHOLD = 1 * 1024 * 1024;

// Readahead blocks for non-seekable streams will be limited to this
// fraction of the cache space. We don't normally evict such blocks
// because replacing them requires a seek, but we need to make sure
// they don't monopolize the cache.
static const double NONSEEKABLE_READAHEAD_MAX = 0.5;

// Data N seconds before the current playback position is given the same priority
// as data REPLAY_PENALTY_FACTOR*N seconds ahead of the current playback
// position. REPLAY_PENALTY_FACTOR is greater than 1 to reflect that
// data in the past is less likely to be played again than data in the future.
// We want to give data just behind the current playback position reasonably
// high priority in case codecs need to retrieve that data (e.g. because
// tracks haven't been muxed well or are being decoded at uneven rates).
// 1/REPLAY_PENALTY_FACTOR as much data will be kept behind the
// current playback position as will be kept ahead of the current playback
// position.
static const uint32_t REPLAY_PENALTY_FACTOR = 3;

// When looking for a reusable block, scan forward this many blocks
// from the desired "best" block location to look for free blocks,
// before we resort to scanning the whole cache. The idea is to try to
// store runs of stream blocks close-to-consecutively in the cache if we
// can.
static const uint32_t FREE_BLOCK_SCAN_LIMIT = 16;

#ifdef DEBUG
// Turn this on to do very expensive cache state validation
// #define DEBUG_VERIFY_CACHE
#endif

class MediaCacheFlusher final : public nsIObserver,
                                public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static void RegisterMediaCache(MediaCache* aMediaCache);
  static void UnregisterMediaCache(MediaCache* aMediaCache);

private:
  MediaCacheFlusher() {}
  ~MediaCacheFlusher() {}

  // Singleton instance created when a first MediaCache is registered, and
  // released when the last MediaCache is unregistered.
  // The observer service will keep a weak reference to it, for notifications.
  static StaticRefPtr<MediaCacheFlusher> gMediaCacheFlusher;

  nsTArray<MediaCache*> mMediaCaches;
};

/* static */ StaticRefPtr<MediaCacheFlusher>
  MediaCacheFlusher::gMediaCacheFlusher;

NS_IMPL_ISUPPORTS(MediaCacheFlusher, nsIObserver, nsISupportsWeakReference)

/* static */ void
MediaCacheFlusher::RegisterMediaCache(MediaCache* aMediaCache)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  if (!gMediaCacheFlusher) {
    gMediaCacheFlusher = new MediaCacheFlusher();

    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (observerService) {
      observerService->AddObserver(
        gMediaCacheFlusher, "last-pb-context-exited", true);
      observerService->AddObserver(
        gMediaCacheFlusher, "cacheservice:empty-cache", true);
    }
  }

  gMediaCacheFlusher->mMediaCaches.AppendElement(aMediaCache);
}

/* static */ void
MediaCacheFlusher::UnregisterMediaCache(MediaCache* aMediaCache)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  gMediaCacheFlusher->mMediaCaches.RemoveElement(aMediaCache);

  if (gMediaCacheFlusher->mMediaCaches.Length() == 0) {
    gMediaCacheFlusher = nullptr;
  }
}

class MediaCache
{
  using AutoLock = ReentrantMonitorAutoEnter;
  using AutoUnlock = ReentrantMonitorAutoExit;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaCache)

  friend class MediaCacheStream::BlockList;
  typedef MediaCacheStream::BlockList BlockList;
  static const int64_t BLOCK_SIZE = MediaCacheStream::BLOCK_SIZE;

  // Get an instance of a MediaCache (or nullptr if initialization failed).
  // aContentLength is the content length if known already, otherwise -1.
  // If the length is known and considered small enough, a discrete MediaCache
  // with memory backing will be given. Otherwise the one MediaCache with
  // file backing will be provided.
  static RefPtr<MediaCache> GetMediaCache(int64_t aContentLength);

  nsIEventTarget* OwnerThread() const { return sThread; }

  // Brutally flush the cache contents. Main thread only.
  void Flush();

  // Close all streams associated with private browsing windows. This will
  // also remove the blocks from the cache since we don't want to leave any
  // traces when PB is done.
  void CloseStreamsForPrivateBrowsing();

  // Cache-file access methods. These are the lowest-level cache methods.
  // mReentrantMonitor must be held; these can be called on any thread.
  // This can return partial reads.
  // Note mReentrantMonitor will be dropped while doing IO. The caller need
  // to handle changes happening when the monitor is not held.
  nsresult ReadCacheFile(AutoLock&,
                         int64_t aOffset,
                         void* aData,
                         int32_t aLength,
                         int32_t* aBytes);

  // The generated IDs are always positive.
  int64_t AllocateResourceID(AutoLock&) { return ++mNextResourceID; }

  // mReentrantMonitor must be held, called on main thread.
  // These methods are used by the stream to set up and tear down streams,
  // and to handle reads and writes.
  // Add aStream to the list of streams.
  void OpenStream(AutoLock&, MediaCacheStream* aStream, bool aIsClone = false);
  // Remove aStream from the list of streams.
  void ReleaseStream(MediaCacheStream* aStream);
  // Free all blocks belonging to aStream.
  void ReleaseStreamBlocks(AutoLock&, MediaCacheStream* aStream);
  // Find a cache entry for this data, and write the data into it
  void AllocateAndWriteBlock(
    AutoLock&,
    MediaCacheStream* aStream,
    int32_t aStreamBlockIndex,
    MediaCacheStream::ReadMode aMode,
    Span<const uint8_t> aData1,
    Span<const uint8_t> aData2 = Span<const uint8_t>());

  // mReentrantMonitor must be held; can be called on any thread
  // Notify the cache that a seek has been requested. Some blocks may
  // need to change their class between PLAYED_BLOCK and READAHEAD_BLOCK.
  // This does not trigger channel seeks directly, the next Update()
  // will do that if necessary. The caller will call QueueUpdate().
  void NoteSeek(AutoLock&, MediaCacheStream* aStream, int64_t aOldOffset);
  // Notify the cache that a block has been read from. This is used
  // to update last-use times. The block may not actually have a
  // cache entry yet since Read can read data from a stream's
  // in-memory mPartialBlockBuffer while the block is only partly full,
  // and thus hasn't yet been committed to the cache. The caller will
  // call QueueUpdate().
  void NoteBlockUsage(AutoLock&,
                      MediaCacheStream* aStream,
                      int32_t aBlockIndex,
                      int64_t aStreamOffset,
                      MediaCacheStream::ReadMode aMode,
                      TimeStamp aNow);
  // Mark aStream as having the block, adding it as an owner.
  void AddBlockOwnerAsReadahead(AutoLock&,
                                int32_t aBlockIndex,
                                MediaCacheStream* aStream,
                                int32_t aStreamBlockIndex);

  // This queues a call to Update() on the main thread.
  void QueueUpdate(AutoLock&);

  // Notify all streams for the resource ID that the suspended status changed
  // at the end of MediaCache::Update.
  void QueueSuspendedStatusUpdate(AutoLock&, int64_t aResourceID);

  // Updates the cache state asynchronously on the main thread:
  // -- try to trim the cache back to its desired size, if necessary
  // -- suspend channels that are going to read data that's lower priority
  // than anything currently cached
  // -- resume channels that are going to read data that's higher priority
  // than something currently cached
  // -- seek channels that need to seek to a new location
  void Update();

#ifdef DEBUG_VERIFY_CACHE
  // Verify invariants, especially block list invariants
  void Verify(AutoLock&);
#else
  void Verify(AutoLock&) {}
#endif

  ReentrantMonitor& Monitor() { return mMonitor; }

  /**
   * An iterator that makes it easy to iterate through all streams that
   * have a given resource ID and are not closed.
   * Must be used while holding the media cache lock.
   */
  class ResourceStreamIterator
  {
  public:
    ResourceStreamIterator(MediaCache* aMediaCache, int64_t aResourceID)
      : mMediaCache(aMediaCache)
      , mResourceID(aResourceID)
      , mNext(0)
    {
      aMediaCache->mMonitor.AssertCurrentThreadIn();
    }
    MediaCacheStream* Next(AutoLock& aLock)
    {
      while (mNext < mMediaCache->mStreams.Length()) {
        MediaCacheStream* stream = mMediaCache->mStreams[mNext];
        ++mNext;
        if (stream->GetResourceID() == mResourceID && !stream->IsClosed(aLock))
          return stream;
      }
      return nullptr;
    }
  private:
    MediaCache* mMediaCache;
    int64_t  mResourceID;
    uint32_t mNext;
  };

protected:
  explicit MediaCache(MediaBlockCacheBase* aCache)
    : mMonitor("MediaCache.mMonitor")
    , mBlockCache(aCache)
    , mUpdateQueued(false)
#ifdef DEBUG
    , mInUpdate(false)
#endif
  {
    NS_ASSERTION(NS_IsMainThread(), "Only construct MediaCache on main thread");
    MOZ_COUNT_CTOR(MediaCache);
    MediaCacheFlusher::RegisterMediaCache(this);
  }

  ~MediaCache()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only destroy MediaCache on main thread");
    if (this == gMediaCache) {
      LOG("~MediaCache(Global file-backed MediaCache)");
      // This is the file-backed MediaCache, reset the global pointer.
      gMediaCache = nullptr;
      // Only gather "MEDIACACHE" telemetry for the file-based cache.
      LOG("MediaCache::~MediaCache(this=%p) MEDIACACHE_WATERMARK_KB=%u",
          this,
          unsigned(mIndexWatermark * MediaCache::BLOCK_SIZE / 1024));
      Telemetry::Accumulate(
        Telemetry::HistogramID::MEDIACACHE_WATERMARK_KB,
        uint32_t(mIndexWatermark * MediaCache::BLOCK_SIZE / 1024));
      LOG(
        "MediaCache::~MediaCache(this=%p) MEDIACACHE_BLOCKOWNERS_WATERMARK=%u",
        this,
        unsigned(mBlockOwnersWatermark));
      Telemetry::Accumulate(
        Telemetry::HistogramID::MEDIACACHE_BLOCKOWNERS_WATERMARK,
        mBlockOwnersWatermark);
    } else {
      LOG("~MediaCache(Memory-backed MediaCache %p)", this);
    }
    MediaCacheFlusher::UnregisterMediaCache(this);
    NS_ASSERTION(mStreams.IsEmpty(), "Stream(s) still open!");
    Truncate();
    NS_ASSERTION(mIndex.Length() == 0, "Blocks leaked?");

    MOZ_COUNT_DTOR(MediaCache);
  }

  // Find a free or reusable block and return its index. If there are no
  // free blocks and no reusable blocks, add a new block to the cache
  // and return it. Can return -1 on OOM.
  int32_t FindBlockForIncomingData(AutoLock&,
                                   TimeStamp aNow,
                                   MediaCacheStream* aStream,
                                   int32_t aStreamBlockIndex);
  // Find a reusable block --- a free block, if there is one, otherwise
  // the reusable block with the latest predicted-next-use, or -1 if
  // there aren't any freeable blocks. Only block indices less than
  // aMaxSearchBlockIndex are considered. If aForStream is non-null,
  // then aForStream and aForStreamBlock indicate what media data will
  // be placed; FindReusableBlock will favour returning free blocks
  // near other blocks for that point in the stream.
  int32_t FindReusableBlock(AutoLock&,
                            TimeStamp aNow,
                            MediaCacheStream* aForStream,
                            int32_t aForStreamBlock,
                            int32_t aMaxSearchBlockIndex);
  bool BlockIsReusable(AutoLock&, int32_t aBlockIndex);
  // Given a list of blocks sorted with the most reusable blocks at the
  // end, find the last block whose stream is not pinned (if any)
  // and whose cache entry index is less than aBlockIndexLimit
  // and append it to aResult.
  void AppendMostReusableBlock(AutoLock&,
                               BlockList* aBlockList,
                               nsTArray<uint32_t>* aResult,
                               int32_t aBlockIndexLimit);

  enum BlockClass {
    // block belongs to mMetadataBlockList because data has been consumed
    // from it in "metadata mode" --- in particular blocks read during
    // Ogg seeks go into this class. These blocks may have played data
    // in them too.
    METADATA_BLOCK,
    // block belongs to mPlayedBlockList because its offset is
    // less than the stream's current reader position
    PLAYED_BLOCK,
    // block belongs to the stream's mReadaheadBlockList because its
    // offset is greater than or equal to the stream's current
    // reader position
    READAHEAD_BLOCK
  };

  struct BlockOwner {
    constexpr BlockOwner() {}

    // The stream that owns this block, or null if the block is free.
    MediaCacheStream* mStream = nullptr;
    // The block index in the stream. Valid only if mStream is non-null.
    // Initialized to an insane value to highlight misuse.
    uint32_t          mStreamBlock = UINT32_MAX;
    // Time at which this block was last used. Valid only if
    // mClass is METADATA_BLOCK or PLAYED_BLOCK.
    TimeStamp         mLastUseTime;
    BlockClass        mClass = READAHEAD_BLOCK;
  };

  struct Block {
    // Free blocks have an empty mOwners array
    nsTArray<BlockOwner> mOwners;
  };

  // Get the BlockList that the block should belong to given its
  // current owner
  BlockList* GetListForBlock(AutoLock&, BlockOwner* aBlock);
  // Get the BlockOwner for the given block index and owning stream
  // (returns null if the stream does not own the block)
  BlockOwner* GetBlockOwner(AutoLock&,
                            int32_t aBlockIndex,
                            MediaCacheStream* aStream);
  // Returns true iff the block is free
  bool IsBlockFree(int32_t aBlockIndex)
  { return mIndex[aBlockIndex].mOwners.IsEmpty(); }
  // Add the block to the free list and mark its streams as not having
  // the block in cache
  void FreeBlock(AutoLock&, int32_t aBlock);
  // Mark aStream as not having the block, removing it as an owner. If
  // the block has no more owners it's added to the free list.
  void RemoveBlockOwner(AutoLock&,
                        int32_t aBlockIndex,
                        MediaCacheStream* aStream);
  // Swap all metadata associated with the two blocks. The caller
  // is responsible for swapping up any cache file state.
  void SwapBlocks(AutoLock&, int32_t aBlockIndex1, int32_t aBlockIndex2);
  // Insert the block into the readahead block list for the stream
  // at the right point in the list.
  void InsertReadaheadBlock(AutoLock&,
                            BlockOwner* aBlockOwner,
                            int32_t aBlockIndex);

  // Guess the duration until block aBlock will be next used
  TimeDuration PredictNextUse(AutoLock&, TimeStamp aNow, int32_t aBlock);
  // Guess the duration until the next incoming data on aStream will be used
  TimeDuration PredictNextUseForIncomingData(AutoLock&,
                                             MediaCacheStream* aStream);

  // Truncate the file and index array if there are free blocks at the
  // end
  void Truncate();

  void FlushInternal(AutoLock&);

  // There is at most one file-backed media cache.
  // It is owned by all MediaCacheStreams that use it.
  // This is a raw pointer set by GetMediaCache(), and reset by ~MediaCache(),
  // both on the main thread; and is not accessed anywhere else.
  static MediaCache* gMediaCache;

  // This member is main-thread only. It's used to allocate unique
  // resource IDs to streams.
  int64_t mNextResourceID = 0;

  // The monitor protects all the data members here. Also, off-main-thread
  // readers that need to block will Wait() on this monitor. When new
  // data becomes available in the cache, we NotifyAll() on this monitor.
  ReentrantMonitor mMonitor;
  // This must always be accessed when the monitor is held.
  nsTArray<MediaCacheStream*> mStreams;
  // The Blocks describing the cache entries.
  nsTArray<Block> mIndex;
  // Keep track for highest number of blocks used, for telemetry purposes.
  int32_t mIndexWatermark = 0;
  // Keep track for highest number of blocks owners, for telemetry purposes.
  uint32_t mBlockOwnersWatermark = 0;
  // Writer which performs IO, asynchronously writing cache blocks.
  RefPtr<MediaBlockCacheBase> mBlockCache;
  // The list of free blocks; they are not ordered.
  BlockList       mFreeBlocks;
  // True if an event to run Update() has been queued but not processed
  bool            mUpdateQueued;
#ifdef DEBUG
  bool            mInUpdate;
#endif
  // A list of resource IDs to notify about the change in suspended status.
  nsTArray<int64_t> mSuspendedStatusToNotify;
  // The thread on which we will run data callbacks from the channels.
  // Note this thread is shared among all MediaCache instances.
  static StaticRefPtr<nsIThread> sThread;
  // True if we've tried to init sThread. Note we try once only so it is safe
  // to access sThread on all threads.
  static bool sThreadInit;
};

// Initialized to nullptr by non-local static initialization.
/* static */ MediaCache* MediaCache::gMediaCache;

/* static */ StaticRefPtr<nsIThread> MediaCache::sThread;
/* static */ bool MediaCache::sThreadInit = false;

NS_IMETHODIMP
MediaCacheFlusher::Observe(nsISupports *aSubject, char const *aTopic, char16_t const *aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  if (strcmp(aTopic, "last-pb-context-exited") == 0) {
    for (MediaCache* mc : mMediaCaches) {
      mc->CloseStreamsForPrivateBrowsing();
    }
    return NS_OK;
  }
  if (strcmp(aTopic, "cacheservice:empty-cache") == 0) {
    for (MediaCache* mc : mMediaCaches) {
      mc->Flush();
    }
    return NS_OK;
  }
  return NS_OK;
}

MediaCacheStream::MediaCacheStream(ChannelMediaResource* aClient,
                                   bool aIsPrivateBrowsing)
  : mMediaCache(nullptr)
  , mClient(aClient)
  , mIsTransportSeekable(false)
  , mCacheSuspended(false)
  , mChannelEnded(false)
  , mStreamOffset(0)
  , mPlaybackBytesPerSecond(10000)
  , mPinCount(0)
  , mMetadataInPartialBlockBuffer(false)
  , mIsPrivateBrowsing(aIsPrivateBrowsing)
{
}

size_t MediaCacheStream::SizeOfExcludingThis(
                                MallocSizeOf aMallocSizeOf) const
{
  // Looks like these are not owned:
  // - mClient
  size_t size = mBlocks.ShallowSizeOfExcludingThis(aMallocSizeOf);
  size += mReadaheadBlocks.SizeOfExcludingThis(aMallocSizeOf);
  size += mMetadataBlocks.SizeOfExcludingThis(aMallocSizeOf);
  size += mPlayedBlocks.SizeOfExcludingThis(aMallocSizeOf);
  size += aMallocSizeOf(mPartialBlockBuffer.get());

  return size;
}

size_t MediaCacheStream::BlockList::SizeOfExcludingThis(
                                MallocSizeOf aMallocSizeOf) const
{
  return mEntries.ShallowSizeOfExcludingThis(aMallocSizeOf);
}

void MediaCacheStream::BlockList::AddFirstBlock(int32_t aBlock)
{
  NS_ASSERTION(!mEntries.GetEntry(aBlock), "Block already in list");
  Entry* entry = mEntries.PutEntry(aBlock);

  if (mFirstBlock < 0) {
    entry->mNextBlock = entry->mPrevBlock = aBlock;
  } else {
    entry->mNextBlock = mFirstBlock;
    entry->mPrevBlock = mEntries.GetEntry(mFirstBlock)->mPrevBlock;
    mEntries.GetEntry(entry->mNextBlock)->mPrevBlock = aBlock;
    mEntries.GetEntry(entry->mPrevBlock)->mNextBlock = aBlock;
  }
  mFirstBlock = aBlock;
  ++mCount;
}

void MediaCacheStream::BlockList::AddAfter(int32_t aBlock, int32_t aBefore)
{
  NS_ASSERTION(!mEntries.GetEntry(aBlock), "Block already in list");
  Entry* entry = mEntries.PutEntry(aBlock);

  Entry* addAfter = mEntries.GetEntry(aBefore);
  NS_ASSERTION(addAfter, "aBefore not in list");

  entry->mNextBlock = addAfter->mNextBlock;
  entry->mPrevBlock = aBefore;
  mEntries.GetEntry(entry->mNextBlock)->mPrevBlock = aBlock;
  mEntries.GetEntry(entry->mPrevBlock)->mNextBlock = aBlock;
  ++mCount;
}

void MediaCacheStream::BlockList::RemoveBlock(int32_t aBlock)
{
  Entry* entry = mEntries.GetEntry(aBlock);
  MOZ_DIAGNOSTIC_ASSERT(entry, "Block not in list");

  if (entry->mNextBlock == aBlock) {
    MOZ_DIAGNOSTIC_ASSERT(entry->mPrevBlock == aBlock, "Linked list inconsistency");
    MOZ_DIAGNOSTIC_ASSERT(mFirstBlock == aBlock, "Linked list inconsistency");
    mFirstBlock = -1;
  } else {
    if (mFirstBlock == aBlock) {
      mFirstBlock = entry->mNextBlock;
    }
    mEntries.GetEntry(entry->mNextBlock)->mPrevBlock = entry->mPrevBlock;
    mEntries.GetEntry(entry->mPrevBlock)->mNextBlock = entry->mNextBlock;
  }
  mEntries.RemoveEntry(entry);
  --mCount;
}

int32_t MediaCacheStream::BlockList::GetLastBlock() const
{
  if (mFirstBlock < 0)
    return -1;
  return mEntries.GetEntry(mFirstBlock)->mPrevBlock;
}

int32_t MediaCacheStream::BlockList::GetNextBlock(int32_t aBlock) const
{
  int32_t block = mEntries.GetEntry(aBlock)->mNextBlock;
  if (block == mFirstBlock)
    return -1;
  return block;
}

int32_t MediaCacheStream::BlockList::GetPrevBlock(int32_t aBlock) const
{
  if (aBlock == mFirstBlock)
    return -1;
  return mEntries.GetEntry(aBlock)->mPrevBlock;
}

#ifdef DEBUG
void MediaCacheStream::BlockList::Verify()
{
  int32_t count = 0;
  if (mFirstBlock >= 0) {
    int32_t block = mFirstBlock;
    do {
      Entry* entry = mEntries.GetEntry(block);
      NS_ASSERTION(mEntries.GetEntry(entry->mNextBlock)->mPrevBlock == block,
                   "Bad prev link");
      NS_ASSERTION(mEntries.GetEntry(entry->mPrevBlock)->mNextBlock == block,
                   "Bad next link");
      block = entry->mNextBlock;
      ++count;
    } while (block != mFirstBlock);
  }
  NS_ASSERTION(count == mCount, "Bad count");
}
#endif

static void UpdateSwappedBlockIndex(int32_t* aBlockIndex,
    int32_t aBlock1Index, int32_t aBlock2Index)
{
  int32_t index = *aBlockIndex;
  if (index == aBlock1Index) {
    *aBlockIndex = aBlock2Index;
  } else if (index == aBlock2Index) {
    *aBlockIndex = aBlock1Index;
  }
}

void
MediaCacheStream::BlockList::NotifyBlockSwapped(int32_t aBlockIndex1,
                                                  int32_t aBlockIndex2)
{
  Entry* e1 = mEntries.GetEntry(aBlockIndex1);
  Entry* e2 = mEntries.GetEntry(aBlockIndex2);
  int32_t e1Prev = -1, e1Next = -1, e2Prev = -1, e2Next = -1;

  // Fix mFirstBlock
  UpdateSwappedBlockIndex(&mFirstBlock, aBlockIndex1, aBlockIndex2);

  // Fix mNextBlock/mPrevBlock links. First capture previous/next links
  // so we don't get confused due to aliasing.
  if (e1) {
    e1Prev = e1->mPrevBlock;
    e1Next = e1->mNextBlock;
  }
  if (e2) {
    e2Prev = e2->mPrevBlock;
    e2Next = e2->mNextBlock;
  }
  // Update the entries.
  if (e1) {
    mEntries.GetEntry(e1Prev)->mNextBlock = aBlockIndex2;
    mEntries.GetEntry(e1Next)->mPrevBlock = aBlockIndex2;
  }
  if (e2) {
    mEntries.GetEntry(e2Prev)->mNextBlock = aBlockIndex1;
    mEntries.GetEntry(e2Next)->mPrevBlock = aBlockIndex1;
  }

  // Fix hashtable keys. First remove stale entries.
  if (e1) {
    e1Prev = e1->mPrevBlock;
    e1Next = e1->mNextBlock;
    mEntries.RemoveEntry(e1);
    // Refresh pointer after hashtable mutation.
    e2 = mEntries.GetEntry(aBlockIndex2);
  }
  if (e2) {
    e2Prev = e2->mPrevBlock;
    e2Next = e2->mNextBlock;
    mEntries.RemoveEntry(e2);
  }
  // Put new entries back.
  if (e1) {
    e1 = mEntries.PutEntry(aBlockIndex2);
    e1->mNextBlock = e1Next;
    e1->mPrevBlock = e1Prev;
  }
  if (e2) {
    e2 = mEntries.PutEntry(aBlockIndex1);
    e2->mNextBlock = e2Next;
    e2->mPrevBlock = e2Prev;
  }
}

void
MediaCache::FlushInternal(AutoLock& aLock)
{
  for (uint32_t blockIndex = 0; blockIndex < mIndex.Length(); ++blockIndex) {
    FreeBlock(aLock, blockIndex);
  }

  // Truncate index array.
  Truncate();
  NS_ASSERTION(mIndex.Length() == 0, "Blocks leaked?");
  // Reset block cache to its pristine state.
  mBlockCache->Flush();
}

void
MediaCache::Flush()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
    "MediaCache::Flush", [self = RefPtr<MediaCache>(this)]() {
      AutoLock lock(self->mMonitor);
      self->FlushInternal(lock);
    });
  sThread->Dispatch(r.forget());
}

void
MediaCache::CloseStreamsForPrivateBrowsing()
{
  MOZ_ASSERT(NS_IsMainThread());
  sThread->Dispatch(
    NS_NewRunnableFunction("MediaCache::CloseStreamsForPrivateBrowsing",
                           [self = RefPtr<MediaCache>(this)]() {
                             AutoLock lock(self->mMonitor);
                             for (MediaCacheStream* s : self->mStreams) {
                               if (s->mIsPrivateBrowsing) {
                                 s->CloseInternal(lock);
                               }
                             }
                           }));
}

/* static */ RefPtr<MediaCache>
MediaCache::GetMediaCache(int64_t aContentLength)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  if (!sThreadInit) {
    sThreadInit = true;
    nsCOMPtr<nsIThread> thread;
    nsresult rv = NS_NewNamedThread("MediaCache", getter_AddRefs(thread));
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to create a thread for MediaCache.");
      return nullptr;
    }
    sThread = thread.forget();

    static struct ClearThread
    {
      // Called during shutdown to clear sThread.
      void operator=(std::nullptr_t)
      {
        nsCOMPtr<nsIThread> thread = sThread.forget();
        MOZ_ASSERT(thread);
        thread->Shutdown();
      }
    } sClearThread;
    ClearOnShutdown(&sClearThread, ShutdownPhase::ShutdownThreads);
  }

  if (!sThread) {
    return nullptr;
  }

  if (aContentLength > 0 &&
      aContentLength <= int64_t(MediaPrefs::MediaMemoryCacheMaxSize()) * 1024) {
    // Small-enough resource, use a new memory-backed MediaCache.
    RefPtr<MediaBlockCacheBase> bc = new MemoryBlockCache(aContentLength);
    nsresult rv = bc->Init();
    if (NS_SUCCEEDED(rv)) {
      RefPtr<MediaCache> mc = new MediaCache(bc);
      LOG("GetMediaCache(%" PRIi64 ") -> Memory MediaCache %p",
          aContentLength,
          mc.get());
      return mc;
    }
    // MemoryBlockCache initialization failed, clean up and try for a
    // file-backed MediaCache below.
  }

  if (gMediaCache) {
    LOG("GetMediaCache(%" PRIi64 ") -> Existing file-backed MediaCache",
        aContentLength);
    return gMediaCache;
  }

  RefPtr<MediaBlockCacheBase> bc = new FileBlockCache();
  nsresult rv = bc->Init();
  if (NS_SUCCEEDED(rv)) {
    gMediaCache = new MediaCache(bc);
    LOG("GetMediaCache(%" PRIi64 ") -> Created file-backed MediaCache",
        aContentLength);
  } else {
    LOG("GetMediaCache(%" PRIi64 ") -> Failed to create file-backed MediaCache",
        aContentLength);
  }

  return gMediaCache;
}

nsresult
MediaCache::ReadCacheFile(AutoLock&,
                          int64_t aOffset,
                          void* aData,
                          int32_t aLength,
                          int32_t* aBytes)
{
  RefPtr<MediaBlockCacheBase> blockCache = mBlockCache;
  if (!blockCache) {
    return NS_ERROR_FAILURE;
  }
  {
    // Since the monitor might be acquired on the main thread, we need to drop
    // the monitor while doing IO in order not to block the main thread.
    AutoUnlock unlock(mMonitor);
    return blockCache->Read(
      aOffset, reinterpret_cast<uint8_t*>(aData), aLength, aBytes);
  }
}

// Allowed range is whatever can be accessed with an int32_t block index.
static bool
IsOffsetAllowed(int64_t aOffset)
{
  return aOffset < (int64_t(INT32_MAX) + 1) * MediaCache::BLOCK_SIZE &&
         aOffset >= 0;
}

// Convert 64-bit offset to 32-bit block index.
// Assumes offset range-check was already done.
static int32_t
OffsetToBlockIndexUnchecked(int64_t aOffset)
{
  // Still check for allowed range in debug builds, to catch out-of-range
  // issues early during development.
  MOZ_ASSERT(IsOffsetAllowed(aOffset));
  return int32_t(aOffset / MediaCache::BLOCK_SIZE);
}

// Convert 64-bit offset to 32-bit block index. -1 if out of allowed range.
static int32_t
OffsetToBlockIndex(int64_t aOffset)
{
  return IsOffsetAllowed(aOffset) ? OffsetToBlockIndexUnchecked(aOffset) : -1;
}

// Convert 64-bit offset to 32-bit offset inside a block.
// Will not fail (even if offset is outside allowed range), so there is no
// need to check for errors.
static int32_t
OffsetInBlock(int64_t aOffset)
{
  // Still check for allowed range in debug builds, to catch out-of-range
  // issues early during development.
  MOZ_ASSERT(IsOffsetAllowed(aOffset));
  return int32_t(aOffset % MediaCache::BLOCK_SIZE);
}

int32_t
MediaCache::FindBlockForIncomingData(AutoLock& aLock,
                                     TimeStamp aNow,
                                     MediaCacheStream* aStream,
                                     int32_t aStreamBlockIndex)
{
  MOZ_ASSERT(sThread->IsOnCurrentThread());

  int32_t blockIndex =
    FindReusableBlock(aLock, aNow, aStream, aStreamBlockIndex, INT32_MAX);

  if (blockIndex < 0 || !IsBlockFree(blockIndex)) {
    // The block returned is already allocated.
    // Don't reuse it if a) there's room to expand the cache or
    // b) the data we're going to store in the free block is not higher
    // priority than the data already stored in the free block.
    // The latter can lead us to go over the cache limit a bit.
    if ((mIndex.Length() < uint32_t(mBlockCache->GetMaxBlocks()) ||
         blockIndex < 0 ||
         PredictNextUseForIncomingData(aLock, aStream) >=
           PredictNextUse(aLock, aNow, blockIndex))) {
      blockIndex = mIndex.Length();
      if (!mIndex.AppendElement())
        return -1;
      mIndexWatermark = std::max(mIndexWatermark, blockIndex + 1);
      mFreeBlocks.AddFirstBlock(blockIndex);
      return blockIndex;
    }
  }

  return blockIndex;
}

bool
MediaCache::BlockIsReusable(AutoLock&, int32_t aBlockIndex)
{
  Block* block = &mIndex[aBlockIndex];
  for (uint32_t i = 0; i < block->mOwners.Length(); ++i) {
    MediaCacheStream* stream = block->mOwners[i].mStream;
    if (stream->mPinCount > 0 ||
        uint32_t(OffsetToBlockIndex(stream->mStreamOffset)) ==
          block->mOwners[i].mStreamBlock) {
      return false;
    }
  }
  return true;
}

void
MediaCache::AppendMostReusableBlock(AutoLock& aLock,
                                    BlockList* aBlockList,
                                    nsTArray<uint32_t>* aResult,
                                    int32_t aBlockIndexLimit)
{
  int32_t blockIndex = aBlockList->GetLastBlock();
  if (blockIndex < 0)
    return;
  do {
    // Don't consider blocks for pinned streams, or blocks that are
    // beyond the specified limit, or a block that contains a stream's
    // current read position (such a block contains both played data
    // and readahead data)
    if (blockIndex < aBlockIndexLimit && BlockIsReusable(aLock, blockIndex)) {
      aResult->AppendElement(blockIndex);
      return;
    }
    blockIndex = aBlockList->GetPrevBlock(blockIndex);
  } while (blockIndex >= 0);
}

int32_t
MediaCache::FindReusableBlock(AutoLock& aLock,
                              TimeStamp aNow,
                              MediaCacheStream* aForStream,
                              int32_t aForStreamBlock,
                              int32_t aMaxSearchBlockIndex)
{
  MOZ_ASSERT(sThread->IsOnCurrentThread());

  uint32_t length = std::min(uint32_t(aMaxSearchBlockIndex), uint32_t(mIndex.Length()));

  if (aForStream && aForStreamBlock > 0 &&
      uint32_t(aForStreamBlock) <= aForStream->mBlocks.Length()) {
    int32_t prevCacheBlock = aForStream->mBlocks[aForStreamBlock - 1];
    if (prevCacheBlock >= 0) {
      uint32_t freeBlockScanEnd =
        std::min(length, prevCacheBlock + FREE_BLOCK_SCAN_LIMIT);
      for (uint32_t i = prevCacheBlock; i < freeBlockScanEnd; ++i) {
        if (IsBlockFree(i))
          return i;
      }
    }
  }

  if (!mFreeBlocks.IsEmpty()) {
    int32_t blockIndex = mFreeBlocks.GetFirstBlock();
    do {
      if (blockIndex < aMaxSearchBlockIndex)
        return blockIndex;
      blockIndex = mFreeBlocks.GetNextBlock(blockIndex);
    } while (blockIndex >= 0);
  }

  // Build a list of the blocks we should consider for the "latest
  // predicted time of next use". We can exploit the fact that the block
  // linked lists are ordered by increasing time of next use. This is
  // actually the whole point of having the linked lists.
  AutoTArray<uint32_t,8> candidates;
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaCacheStream* stream = mStreams[i];
    if (stream->mPinCount > 0) {
      // No point in even looking at this stream's blocks
      continue;
    }

    AppendMostReusableBlock(
      aLock, &stream->mMetadataBlocks, &candidates, length);
    AppendMostReusableBlock(aLock, &stream->mPlayedBlocks, &candidates, length);

    // Don't consider readahead blocks in non-seekable streams. If we
    // remove the block we won't be able to seek back to read it later.
    if (stream->mIsTransportSeekable) {
      AppendMostReusableBlock(
        aLock, &stream->mReadaheadBlocks, &candidates, length);
    }
  }

  TimeDuration latestUse;
  int32_t latestUseBlock = -1;
  for (uint32_t i = 0; i < candidates.Length(); ++i) {
    TimeDuration nextUse = PredictNextUse(aLock, aNow, candidates[i]);
    if (nextUse > latestUse) {
      latestUse = nextUse;
      latestUseBlock = candidates[i];
    }
  }

  return latestUseBlock;
}

MediaCache::BlockList*
MediaCache::GetListForBlock(AutoLock&, BlockOwner* aBlock)
{
  switch (aBlock->mClass) {
  case METADATA_BLOCK:
    NS_ASSERTION(aBlock->mStream, "Metadata block has no stream?");
    return &aBlock->mStream->mMetadataBlocks;
  case PLAYED_BLOCK:
    NS_ASSERTION(aBlock->mStream, "Metadata block has no stream?");
    return &aBlock->mStream->mPlayedBlocks;
  case READAHEAD_BLOCK:
    NS_ASSERTION(aBlock->mStream, "Readahead block has no stream?");
    return &aBlock->mStream->mReadaheadBlocks;
  default:
    NS_ERROR("Invalid block class");
    return nullptr;
  }
}

MediaCache::BlockOwner*
MediaCache::GetBlockOwner(AutoLock&,
                          int32_t aBlockIndex,
                          MediaCacheStream* aStream)
{
  Block* block = &mIndex[aBlockIndex];
  for (uint32_t i = 0; i < block->mOwners.Length(); ++i) {
    if (block->mOwners[i].mStream == aStream)
      return &block->mOwners[i];
  }
  return nullptr;
}

void
MediaCache::SwapBlocks(AutoLock& aLock,
                       int32_t aBlockIndex1,
                       int32_t aBlockIndex2)
{
  Block* block1 = &mIndex[aBlockIndex1];
  Block* block2 = &mIndex[aBlockIndex2];

  block1->mOwners.SwapElements(block2->mOwners);

  // Now all references to block1 have to be replaced with block2 and
  // vice versa.
  // First update stream references to blocks via mBlocks.
  const Block* blocks[] = { block1, block2 };
  int32_t blockIndices[] = { aBlockIndex1, aBlockIndex2 };
  for (int32_t i = 0; i < 2; ++i) {
    for (uint32_t j = 0; j < blocks[i]->mOwners.Length(); ++j) {
      const BlockOwner* b = &blocks[i]->mOwners[j];
      b->mStream->mBlocks[b->mStreamBlock] = blockIndices[i];
    }
  }

  // Now update references to blocks in block lists.
  mFreeBlocks.NotifyBlockSwapped(aBlockIndex1, aBlockIndex2);

  nsTHashtable<nsPtrHashKey<MediaCacheStream> > visitedStreams;

  for (int32_t i = 0; i < 2; ++i) {
    for (uint32_t j = 0; j < blocks[i]->mOwners.Length(); ++j) {
      MediaCacheStream* stream = blocks[i]->mOwners[j].mStream;
      // Make sure that we don't update the same stream twice --- that
      // would result in swapping the block references back again!
      if (visitedStreams.GetEntry(stream))
        continue;
      visitedStreams.PutEntry(stream);
      stream->mReadaheadBlocks.NotifyBlockSwapped(aBlockIndex1, aBlockIndex2);
      stream->mPlayedBlocks.NotifyBlockSwapped(aBlockIndex1, aBlockIndex2);
      stream->mMetadataBlocks.NotifyBlockSwapped(aBlockIndex1, aBlockIndex2);
    }
  }

  Verify(aLock);
}

void
MediaCache::RemoveBlockOwner(AutoLock& aLock,
                             int32_t aBlockIndex,
                             MediaCacheStream* aStream)
{
  Block* block = &mIndex[aBlockIndex];
  for (uint32_t i = 0; i < block->mOwners.Length(); ++i) {
    BlockOwner* bo = &block->mOwners[i];
    if (bo->mStream == aStream) {
      GetListForBlock(aLock, bo)->RemoveBlock(aBlockIndex);
      bo->mStream->mBlocks[bo->mStreamBlock] = -1;
      block->mOwners.RemoveElementAt(i);
      if (block->mOwners.IsEmpty()) {
        mFreeBlocks.AddFirstBlock(aBlockIndex);
      }
      return;
    }
  }
}

void
MediaCache::AddBlockOwnerAsReadahead(AutoLock& aLock,
                                     int32_t aBlockIndex,
                                     MediaCacheStream* aStream,
                                     int32_t aStreamBlockIndex)
{
  Block* block = &mIndex[aBlockIndex];
  if (block->mOwners.IsEmpty()) {
    mFreeBlocks.RemoveBlock(aBlockIndex);
  }
  BlockOwner* bo = block->mOwners.AppendElement();
  mBlockOwnersWatermark =
    std::max(mBlockOwnersWatermark, uint32_t(block->mOwners.Length()));
  bo->mStream = aStream;
  bo->mStreamBlock = aStreamBlockIndex;
  aStream->mBlocks[aStreamBlockIndex] = aBlockIndex;
  bo->mClass = READAHEAD_BLOCK;
  InsertReadaheadBlock(aLock, bo, aBlockIndex);
}

void
MediaCache::FreeBlock(AutoLock& aLock, int32_t aBlock)
{
  Block* block = &mIndex[aBlock];
  if (block->mOwners.IsEmpty()) {
    // already free
    return;
  }

  LOG("Released block %d", aBlock);

  for (uint32_t i = 0; i < block->mOwners.Length(); ++i) {
    BlockOwner* bo = &block->mOwners[i];
    GetListForBlock(aLock, bo)->RemoveBlock(aBlock);
    bo->mStream->mBlocks[bo->mStreamBlock] = -1;
  }
  block->mOwners.Clear();
  mFreeBlocks.AddFirstBlock(aBlock);
  Verify(aLock);
}

TimeDuration
MediaCache::PredictNextUse(AutoLock&, TimeStamp aNow, int32_t aBlock)
{
  MOZ_ASSERT(sThread->IsOnCurrentThread());
  NS_ASSERTION(!IsBlockFree(aBlock), "aBlock is free");

  Block* block = &mIndex[aBlock];
  // Blocks can be belong to multiple streams. The predicted next use
  // time is the earliest time predicted by any of the streams.
  TimeDuration result;
  for (uint32_t i = 0; i < block->mOwners.Length(); ++i) {
    BlockOwner* bo = &block->mOwners[i];
    TimeDuration prediction;
    switch (bo->mClass) {
    case METADATA_BLOCK:
      // This block should be managed in LRU mode. For metadata we predict
      // that the time until the next use is the time since the last use.
      prediction = aNow - bo->mLastUseTime;
      break;
    case PLAYED_BLOCK: {
      // This block should be managed in LRU mode, and we should impose
      // a "replay delay" to reflect the likelihood of replay happening
      NS_ASSERTION(static_cast<int64_t>(bo->mStreamBlock)*BLOCK_SIZE <
                   bo->mStream->mStreamOffset,
                   "Played block after the current stream position?");
      int64_t bytesBehind =
        bo->mStream->mStreamOffset - static_cast<int64_t>(bo->mStreamBlock)*BLOCK_SIZE;
      int64_t millisecondsBehind =
        bytesBehind*1000/bo->mStream->mPlaybackBytesPerSecond;
      prediction = TimeDuration::FromMilliseconds(
          std::min<int64_t>(millisecondsBehind*REPLAY_PENALTY_FACTOR, INT32_MAX));
      break;
    }
    case READAHEAD_BLOCK: {
      int64_t bytesAhead =
        static_cast<int64_t>(bo->mStreamBlock)*BLOCK_SIZE - bo->mStream->mStreamOffset;
      NS_ASSERTION(bytesAhead >= 0,
                   "Readahead block before the current stream position?");
      int64_t millisecondsAhead =
        bytesAhead*1000/bo->mStream->mPlaybackBytesPerSecond;
      prediction = TimeDuration::FromMilliseconds(
          std::min<int64_t>(millisecondsAhead, INT32_MAX));
      break;
    }
    default:
      NS_ERROR("Invalid class for predicting next use");
      return TimeDuration(0);
    }
    if (i == 0 || prediction < result) {
      result = prediction;
    }
  }
  return result;
}

TimeDuration
MediaCache::PredictNextUseForIncomingData(AutoLock&, MediaCacheStream* aStream)
{
  MOZ_ASSERT(sThread->IsOnCurrentThread());

  int64_t bytesAhead = aStream->mChannelOffset - aStream->mStreamOffset;
  if (bytesAhead <= -BLOCK_SIZE) {
    // Hmm, no idea when data behind us will be used. Guess 24 hours.
    return TimeDuration::FromSeconds(24*60*60);
  }
  if (bytesAhead <= 0)
    return TimeDuration(0);
  int64_t millisecondsAhead = bytesAhead*1000/aStream->mPlaybackBytesPerSecond;
  return TimeDuration::FromMilliseconds(
      std::min<int64_t>(millisecondsAhead, INT32_MAX));
}

void
MediaCache::Update()
{
  MOZ_ASSERT(sThread->IsOnCurrentThread());

  AutoLock lock(mMonitor);

  struct StreamAction
  {
    enum
    {
      NONE,
      SEEK,
      RESUME,
      SUSPEND
    } mTag = NONE;
    // Members for 'SEEK' only.
    bool mResume = false;
    int64_t mSeekTarget = -1;
  };

  // The action to use for each stream. We store these so we can make
  // decisions while holding the cache lock but implement those decisions
  // without holding the cache lock, since we need to call out to
  // stream, decoder and element code.
  AutoTArray<StreamAction,10> actions;

  mUpdateQueued = false;
#ifdef DEBUG
  mInUpdate = true;
#endif

  int32_t maxBlocks = mBlockCache->GetMaxBlocks();
  TimeStamp now = TimeStamp::Now();

  int32_t freeBlockCount = mFreeBlocks.GetCount();
  TimeDuration latestPredictedUseForOverflow = 0;
  if (mIndex.Length() > uint32_t(maxBlocks)) {
    // Try to trim back the cache to its desired maximum size. The cache may
    // have overflowed simply due to data being received when we have
    // no blocks in the main part of the cache that are free or lower
    // priority than the new data. The cache can also be overflowing because
    // the media.cache_size preference was reduced.
    // First, figure out what the least valuable block in the cache overflow
    // is. We don't want to replace any blocks in the main part of the
    // cache whose expected time of next use is earlier or equal to that.
    // If we allow that, we can effectively end up discarding overflowing
    // blocks (by moving an overflowing block to the main part of the cache,
    // and then overwriting it with another overflowing block), and we try
    // to avoid that since it requires HTTP seeks.
    // We also use this loop to eliminate overflowing blocks from
    // freeBlockCount.
    for (int32_t blockIndex = mIndex.Length() - 1; blockIndex >= maxBlocks;
         --blockIndex) {
      if (IsBlockFree(blockIndex)) {
        // Don't count overflowing free blocks in our free block count
        --freeBlockCount;
        continue;
      }
      TimeDuration predictedUse = PredictNextUse(lock, now, blockIndex);
      latestPredictedUseForOverflow = std::max(latestPredictedUseForOverflow, predictedUse);
    }
  } else {
    freeBlockCount += maxBlocks - mIndex.Length();
  }

  // Now try to move overflowing blocks to the main part of the cache.
  for (int32_t blockIndex = mIndex.Length() - 1; blockIndex >= maxBlocks;
       --blockIndex) {
    if (IsBlockFree(blockIndex))
      continue;

    Block* block = &mIndex[blockIndex];
    // Try to relocate the block close to other blocks for the first stream.
    // There is no point in trying to make it close to other blocks in
    // *all* the streams it might belong to.
    int32_t destinationBlockIndex =
      FindReusableBlock(lock,
                        now,
                        block->mOwners[0].mStream,
                        block->mOwners[0].mStreamBlock,
                        maxBlocks);
    if (destinationBlockIndex < 0) {
      // Nowhere to place this overflow block. We won't be able to
      // place any more overflow blocks.
      break;
    }

    // Don't evict |destinationBlockIndex| if it is within [cur, end) otherwise
    // a new channel will be opened to download this block again which is bad.
    bool inCurrentCachedRange = false;
    for (BlockOwner& owner : mIndex[destinationBlockIndex].mOwners) {
      MediaCacheStream* stream = owner.mStream;
      int64_t end = OffsetToBlockIndexUnchecked(
        stream->GetCachedDataEndInternal(lock, stream->mStreamOffset));
      int64_t cur = OffsetToBlockIndexUnchecked(stream->mStreamOffset);
      if (cur <= owner.mStreamBlock && owner.mStreamBlock < end) {
        inCurrentCachedRange = true;
        break;
      }
    }
    if (inCurrentCachedRange) {
      continue;
    }

    if (IsBlockFree(destinationBlockIndex) ||
        PredictNextUse(lock, now, destinationBlockIndex) >
          latestPredictedUseForOverflow) {
      // Reuse blocks in the main part of the cache that are less useful than
      // the least useful overflow blocks

      nsresult rv = mBlockCache->MoveBlock(blockIndex, destinationBlockIndex);

      if (NS_SUCCEEDED(rv)) {
        // We successfully copied the file data.
        LOG("Swapping blocks %d and %d (trimming cache)",
            blockIndex, destinationBlockIndex);
        // Swapping the block metadata here lets us maintain the
        // correct positions in the linked lists
        SwapBlocks(lock, blockIndex, destinationBlockIndex);
        //Free the overflowing block even if the copy failed.
        LOG("Released block %d (trimming cache)", blockIndex);
        FreeBlock(lock, blockIndex);
      }
    } else {
      LOG("Could not trim cache block %d (destination %d, "
          "predicted next use %f, latest predicted use for overflow %f",
          blockIndex,
          destinationBlockIndex,
          PredictNextUse(lock, now, destinationBlockIndex).ToSeconds(),
          latestPredictedUseForOverflow.ToSeconds());
    }
  }
  // Try chopping back the array of cache entries and the cache file.
  Truncate();

  // Count the blocks allocated for readahead of non-seekable streams
  // (these blocks can't be freed but we don't want them to monopolize the
  // cache)
  int32_t nonSeekableReadaheadBlockCount = 0;
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaCacheStream* stream = mStreams[i];
    if (!stream->mIsTransportSeekable) {
      nonSeekableReadaheadBlockCount += stream->mReadaheadBlocks.GetCount();
    }
  }

  // If freeBlockCount is zero, then compute the latest of
  // the predicted next-uses for all blocks
  TimeDuration latestNextUse;
  if (freeBlockCount == 0) {
    int32_t reusableBlock = FindReusableBlock(lock, now, nullptr, 0, maxBlocks);
    if (reusableBlock >= 0) {
      latestNextUse = PredictNextUse(lock, now, reusableBlock);
    }
  }

  int32_t resumeThreshold = MediaPrefs::MediaCacheResumeThreshold();
  int32_t readaheadLimit = MediaPrefs::MediaCacheReadaheadLimit();

  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    actions.AppendElement(StreamAction{});

    MediaCacheStream* stream = mStreams[i];
    if (stream->mClosed) {
      LOG("Stream %p closed", stream);
      continue;
    }

    // We make decisions based on mSeekTarget when there is a pending seek.
    // Otherwise we will keep issuing seek requests until mChannelOffset
    // is changed by NotifyDataStarted() which is bad.
    int64_t channelOffset =
      stream->mSeekTarget != -1 ? stream->mSeekTarget : stream->mChannelOffset;

    // Figure out where we should be reading from. It's the first
    // uncached byte after the current mStreamOffset.
    int64_t dataOffset =
      stream->GetCachedDataEndInternal(lock, stream->mStreamOffset);
    MOZ_ASSERT(dataOffset >= 0);

    // Compute where we'd actually seek to to read at readOffset
    int64_t desiredOffset = dataOffset;
    if (stream->mIsTransportSeekable) {
      if (desiredOffset > channelOffset &&
          desiredOffset <= channelOffset + SEEK_VS_READ_THRESHOLD) {
        // Assume it's more efficient to just keep reading up to the
        // desired position instead of trying to seek
        desiredOffset = channelOffset;
      }
    } else {
      // We can't seek directly to the desired offset...
      if (channelOffset > desiredOffset) {
        // Reading forward won't get us anywhere, we need to go backwards.
        // Seek back to 0 (the client will reopen the stream) and then
        // read forward.
        NS_WARNING("Can't seek backwards, so seeking to 0");
        desiredOffset = 0;
        // Flush cached blocks out, since if this is a live stream
        // the cached data may be completely different next time we
        // read it. We have to assume that live streams don't
        // advertise themselves as being seekable...
        ReleaseStreamBlocks(lock, stream);
      } else {
        // otherwise reading forward is looking good, so just stay where we
        // are and don't trigger a channel seek!
        desiredOffset = channelOffset;
      }
    }

    // Figure out if we should be reading data now or not. It's amazing
    // how complex this is, but each decision is simple enough.
    bool enableReading;
    if (stream->mStreamLength >= 0 && dataOffset >= stream->mStreamLength) {
      // We want data at the end of the stream, where there's nothing to
      // read. We don't want to try to read if we're suspended, because that
      // might create a new channel and seek unnecessarily (and incorrectly,
      // since HTTP doesn't allow seeking to the actual EOF), and we don't want
      // to suspend if we're not suspended and already reading at the end of
      // the stream, since there just might be more data than the server
      // advertised with Content-Length, and we may as well keep reading.
      // But we don't want to seek to the end of the stream if we're not
      // already there.
      LOG("Stream %p at end of stream", stream);
      enableReading =
        !stream->mCacheSuspended && stream->mStreamLength == channelOffset;
    } else if (desiredOffset < stream->mStreamOffset) {
      // We're reading to try to catch up to where the current stream
      // reader wants to be. Better not stop.
      LOG("Stream %p catching up", stream);
      enableReading = true;
    } else if (desiredOffset < stream->mStreamOffset + BLOCK_SIZE) {
      // The stream reader is waiting for us, or nearly so. Better feed it.
      LOG("Stream %p feeding reader", stream);
      enableReading = true;
    } else if (!stream->mIsTransportSeekable &&
               nonSeekableReadaheadBlockCount >= maxBlocks*NONSEEKABLE_READAHEAD_MAX) {
      // This stream is not seekable and there are already too many blocks
      // being cached for readahead for nonseekable streams (which we can't
      // free). So stop reading ahead now.
      LOG("Stream %p throttling non-seekable readahead", stream);
      enableReading = false;
    } else if (mIndex.Length() > uint32_t(maxBlocks)) {
      // We're in the process of bringing the cache size back to the
      // desired limit, so don't bring in more data yet
      LOG("Stream %p throttling to reduce cache size", stream);
      enableReading = false;
    } else {
      TimeDuration predictedNewDataUse =
        PredictNextUseForIncomingData(lock, stream);

      if (stream->mThrottleReadahead &&
          stream->mCacheSuspended &&
          predictedNewDataUse.ToSeconds() > resumeThreshold) {
        // Don't need data for a while, so don't bother waking up the stream
        LOG("Stream %p avoiding wakeup since more data is not needed", stream);
        enableReading = false;
      } else if (stream->mThrottleReadahead &&
                 predictedNewDataUse.ToSeconds() > readaheadLimit) {
        // Don't read ahead more than this much
        LOG("Stream %p throttling to avoid reading ahead too far", stream);
        enableReading = false;
      } else if (freeBlockCount > 0) {
        // Free blocks in the cache, so keep reading
        LOG("Stream %p reading since there are free blocks", stream);
        enableReading = true;
      } else if (latestNextUse <= TimeDuration(0)) {
        // No reusable blocks, so can't read anything
        LOG("Stream %p throttling due to no reusable blocks", stream);
        enableReading = false;
      } else {
        // Read ahead if the data we expect to read is more valuable than
        // the least valuable block in the main part of the cache
        LOG("Stream %p predict next data in %f, current worst block is %f",
            stream, predictedNewDataUse.ToSeconds(), latestNextUse.ToSeconds());
        enableReading = predictedNewDataUse < latestNextUse;
      }
    }

    if (enableReading) {
      for (uint32_t j = 0; j < i; ++j) {
        MediaCacheStream* other = mStreams[j];
        if (other->mResourceID == stream->mResourceID && !other->mClosed &&
            !other->mClientSuspended && !other->mChannelEnded &&
            OffsetToBlockIndexUnchecked(other->mSeekTarget != -1
                                          ? other->mSeekTarget
                                          : other->mChannelOffset) ==
              OffsetToBlockIndexUnchecked(desiredOffset)) {
          // This block is already going to be read by the other stream.
          // So don't try to read it from this stream as well.
          enableReading = false;
          LOG("Stream %p waiting on same block (%" PRId32 ") from stream %p",
              stream,
              OffsetToBlockIndexUnchecked(desiredOffset),
              other);
          break;
        }
      }
    }

    if (channelOffset != desiredOffset && enableReading) {
      // We need to seek now.
      NS_ASSERTION(stream->mIsTransportSeekable || desiredOffset == 0,
                   "Trying to seek in a non-seekable stream!");
      // Round seek offset down to the start of the block. This is essential
      // because we don't want to think we have part of a block already
      // in mPartialBlockBuffer.
      stream->mSeekTarget =
        OffsetToBlockIndexUnchecked(desiredOffset) * BLOCK_SIZE;
      actions[i].mTag = StreamAction::SEEK;
      actions[i].mResume = stream->mCacheSuspended;
      actions[i].mSeekTarget = stream->mSeekTarget;
    } else if (enableReading && stream->mCacheSuspended) {
      actions[i].mTag = StreamAction::RESUME;
    } else if (!enableReading && !stream->mCacheSuspended) {
      actions[i].mTag = StreamAction::SUSPEND;
    }
  }
#ifdef DEBUG
  mInUpdate = false;
#endif

  // First, update the mCacheSuspended/mCacheEnded flags so that they're all correct
  // when we fire our CacheClient commands below. Those commands can rely on these flags
  // being set correctly for all streams.
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaCacheStream* stream = mStreams[i];
    switch (actions[i].mTag) {
      case StreamAction::SEEK:
        stream->mCacheSuspended = false;
        stream->mChannelEnded = false;
        break;
      case StreamAction::RESUME:
        stream->mCacheSuspended = false;
        break;
      case StreamAction::SUSPEND:
        stream->mCacheSuspended = true;
        break;
      default:
        break;
    }
  }

  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaCacheStream* stream = mStreams[i];
    switch (actions[i].mTag) {
      case StreamAction::SEEK:
        LOG("Stream %p CacheSeek to %" PRId64 " (resume=%d)",
            stream,
            actions[i].mSeekTarget,
            actions[i].mResume);
        stream->mClient->CacheClientSeek(actions[i].mSeekTarget,
                                         actions[i].mResume);
        break;
      case StreamAction::RESUME:
        LOG("Stream %p Resumed", stream);
        stream->mClient->CacheClientResume();
        QueueSuspendedStatusUpdate(lock, stream->mResourceID);
        break;
      case StreamAction::SUSPEND:
        LOG("Stream %p Suspended", stream);
        stream->mClient->CacheClientSuspend();
        QueueSuspendedStatusUpdate(lock, stream->mResourceID);
        break;
      default:
        break;
    }
  }

  // Notify streams about the suspended status changes.
  for (uint32_t i = 0; i < mSuspendedStatusToNotify.Length(); ++i) {
    MediaCache::ResourceStreamIterator iter(this, mSuspendedStatusToNotify[i]);
    while (MediaCacheStream* stream = iter.Next(lock)) {
      stream->mClient->CacheClientNotifySuspendedStatusChanged(
        stream->AreAllStreamsForResourceSuspended(lock));
    }
  }
  mSuspendedStatusToNotify.Clear();
}

class UpdateEvent : public Runnable
{
public:
  explicit UpdateEvent(MediaCache* aMediaCache)
    : Runnable("MediaCache::UpdateEvent")
    , mMediaCache(aMediaCache)
  {
  }

  NS_IMETHOD Run() override
  {
    mMediaCache->Update();
    // Ensure MediaCache is deleted on the main thread.
    NS_ProxyRelease("UpdateEvent::mMediaCache",
                    SystemGroup::EventTargetFor(mozilla::TaskCategory::Other),
                    mMediaCache.forget());
    return NS_OK;
  }

private:
  RefPtr<MediaCache> mMediaCache;
};

void
MediaCache::QueueUpdate(AutoLock&)
{
  // Queuing an update while we're in an update raises a high risk of
  // triggering endless events
  NS_ASSERTION(!mInUpdate,
               "Queuing an update while we're in an update");
  if (mUpdateQueued)
    return;
  mUpdateQueued = true;
  // XXX MediaCache does updates when decoders are still running at
  // shutdown and get freed in the final cycle-collector cleanup.  So
  // don't leak a runnable in that case.
  nsCOMPtr<nsIRunnable> event = new UpdateEvent(this);
  sThread->Dispatch(event.forget());
}

void
MediaCache::QueueSuspendedStatusUpdate(AutoLock&, int64_t aResourceID)
{
  if (!mSuspendedStatusToNotify.Contains(aResourceID)) {
    mSuspendedStatusToNotify.AppendElement(aResourceID);
  }
}

#ifdef DEBUG_VERIFY_CACHE
void
MediaCache::Verify(AutoLock&)
{
  mFreeBlocks.Verify();
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaCacheStream* stream = mStreams[i];
    stream->mReadaheadBlocks.Verify();
    stream->mPlayedBlocks.Verify();
    stream->mMetadataBlocks.Verify();

    // Verify that the readahead blocks are listed in stream block order
    int32_t block = stream->mReadaheadBlocks.GetFirstBlock();
    int32_t lastStreamBlock = -1;
    while (block >= 0) {
      uint32_t j = 0;
      while (mIndex[block].mOwners[j].mStream != stream) {
        ++j;
      }
      int32_t nextStreamBlock =
        int32_t(mIndex[block].mOwners[j].mStreamBlock);
      NS_ASSERTION(lastStreamBlock < nextStreamBlock,
                   "Blocks not increasing in readahead stream");
      lastStreamBlock = nextStreamBlock;
      block = stream->mReadaheadBlocks.GetNextBlock(block);
    }
  }
}
#endif

void
MediaCache::InsertReadaheadBlock(AutoLock& aLock,
                                 BlockOwner* aBlockOwner,
                                 int32_t aBlockIndex)
{
  // Find the last block whose stream block is before aBlockIndex's
  // stream block, and insert after it
  MediaCacheStream* stream = aBlockOwner->mStream;
  int32_t readaheadIndex = stream->mReadaheadBlocks.GetLastBlock();
  while (readaheadIndex >= 0) {
    BlockOwner* bo = GetBlockOwner(aLock, readaheadIndex, stream);
    NS_ASSERTION(bo, "stream must own its blocks");
    if (bo->mStreamBlock < aBlockOwner->mStreamBlock) {
      stream->mReadaheadBlocks.AddAfter(aBlockIndex, readaheadIndex);
      return;
    }
    NS_ASSERTION(bo->mStreamBlock > aBlockOwner->mStreamBlock,
                 "Duplicated blocks??");
    readaheadIndex = stream->mReadaheadBlocks.GetPrevBlock(readaheadIndex);
  }

  stream->mReadaheadBlocks.AddFirstBlock(aBlockIndex);
  Verify(aLock);
}

void
MediaCache::AllocateAndWriteBlock(AutoLock& aLock,
                                  MediaCacheStream* aStream,
                                  int32_t aStreamBlockIndex,
                                  MediaCacheStream::ReadMode aMode,
                                  Span<const uint8_t> aData1,
                                  Span<const uint8_t> aData2)
{
  MOZ_ASSERT(sThread->IsOnCurrentThread());

  // Remove all cached copies of this block
  ResourceStreamIterator iter(this, aStream->mResourceID);
  while (MediaCacheStream* stream = iter.Next(aLock)) {
    while (aStreamBlockIndex >= int32_t(stream->mBlocks.Length())) {
      stream->mBlocks.AppendElement(-1);
    }
    if (stream->mBlocks[aStreamBlockIndex] >= 0) {
      // We no longer want to own this block
      int32_t globalBlockIndex = stream->mBlocks[aStreamBlockIndex];
      LOG("Released block %d from stream %p block %d(%" PRId64 ")",
          globalBlockIndex,
          stream,
          aStreamBlockIndex,
          aStreamBlockIndex * BLOCK_SIZE);
      RemoveBlockOwner(aLock, globalBlockIndex, stream);
    }
  }

  // Extend the mBlocks array as necessary

  TimeStamp now = TimeStamp::Now();
  int32_t blockIndex =
    FindBlockForIncomingData(aLock, now, aStream, aStreamBlockIndex);
  if (blockIndex >= 0) {
    FreeBlock(aLock, blockIndex);

    Block* block = &mIndex[blockIndex];
    LOG("Allocated block %d to stream %p block %d(%" PRId64 ")",
        blockIndex,
        aStream,
        aStreamBlockIndex,
        aStreamBlockIndex * BLOCK_SIZE);

    ResourceStreamIterator iter(this, aStream->mResourceID);
    while (MediaCacheStream* stream = iter.Next(aLock)) {
      BlockOwner* bo = block->mOwners.AppendElement();
      if (!bo) {
        // Roll back mOwners if any allocation fails.
        block->mOwners.Clear();
        return;
      }
      mBlockOwnersWatermark =
        std::max(mBlockOwnersWatermark, uint32_t(block->mOwners.Length()));
      bo->mStream = stream;
    }

    if (block->mOwners.IsEmpty()) {
      // This happens when all streams with the resource id are closed. We can
      // just return here now and discard the data.
      return;
    }

    // Tell each stream using this resource about the new block.
    for (auto& bo : block->mOwners) {
      bo.mStreamBlock = aStreamBlockIndex;
      bo.mLastUseTime = now;
      bo.mStream->mBlocks[aStreamBlockIndex] = blockIndex;
      if (aStreamBlockIndex * BLOCK_SIZE < bo.mStream->mStreamOffset) {
        bo.mClass = aMode == MediaCacheStream::MODE_PLAYBACK ? PLAYED_BLOCK
                                                             : METADATA_BLOCK;
        // This must be the most-recently-used block, since we
        // marked it as used now (which may be slightly bogus, but we'll
        // treat it as used for simplicity).
        GetListForBlock(aLock, &bo)->AddFirstBlock(blockIndex);
        Verify(aLock);
      } else {
        // This may not be the latest readahead block, although it usually
        // will be. We may have to scan for the right place to insert
        // the block in the list.
        bo.mClass = READAHEAD_BLOCK;
        InsertReadaheadBlock(aLock, &bo, blockIndex);
      }
    }

    // Invariant: block->mOwners.IsEmpty() iff we can find an entry
    // in mFreeBlocks for a given blockIndex.
    MOZ_DIAGNOSTIC_ASSERT(!block->mOwners.IsEmpty());
    mFreeBlocks.RemoveBlock(blockIndex);

    nsresult rv = mBlockCache->WriteBlock(blockIndex, aData1, aData2);
    if (NS_FAILED(rv)) {
      LOG("Released block %d from stream %p block %d(%" PRId64 ")",
          blockIndex,
          aStream,
          aStreamBlockIndex,
          aStreamBlockIndex * BLOCK_SIZE);
      FreeBlock(aLock, blockIndex);
    }
  }

  // Queue an Update since the cache state has changed (for example
  // we might want to stop loading because the cache is full)
  QueueUpdate(aLock);
}

void
MediaCache::OpenStream(AutoLock& aLock,
                       MediaCacheStream* aStream,
                       bool aIsClone)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  LOG("Stream %p opened", aStream);
  mStreams.AppendElement(aStream);

  // A cloned stream should've got the ID from its original.
  if (!aIsClone) {
    MOZ_ASSERT(aStream->mResourceID == 0, "mResourceID has been initialized.");
    aStream->mResourceID = AllocateResourceID(aLock);
  }

  // We should have a valid ID now no matter it is cloned or not.
  MOZ_ASSERT(aStream->mResourceID > 0, "mResourceID is invalid");

  // Queue an update since a new stream has been opened.
  QueueUpdate(aLock);
}

void
MediaCache::ReleaseStream(MediaCacheStream* aStream)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  AutoLock lock(mMonitor);
  LOG("Stream %p closed", aStream);
  mStreams.RemoveElement(aStream);

  // Update MediaCache again for |mStreams| is changed.
  // We need to re-run Update() to ensure streams reading from the same resource
  // as the removed stream get a chance to continue reading.
  QueueUpdate(lock);
}

void
MediaCache::ReleaseStreamBlocks(AutoLock& aLock, MediaCacheStream* aStream)
{
  // XXX scanning the entire stream doesn't seem great, if not much of it
  // is cached, but the only easy alternative is to scan the entire cache
  // which isn't better
  uint32_t length = aStream->mBlocks.Length();
  for (uint32_t i = 0; i < length; ++i) {
    int32_t blockIndex = aStream->mBlocks[i];
    if (blockIndex >= 0) {
      LOG("Released block %d from stream %p block %d(%" PRId64 ")",
          blockIndex, aStream, i, i*BLOCK_SIZE);
      RemoveBlockOwner(aLock, blockIndex, aStream);
    }
  }
}

void
MediaCache::Truncate()
{
  uint32_t end;
  for (end = mIndex.Length(); end > 0; --end) {
    if (!IsBlockFree(end - 1))
      break;
    mFreeBlocks.RemoveBlock(end - 1);
  }

  if (end < mIndex.Length()) {
    mIndex.TruncateLength(end);
    // XXX We could truncate the cache file here, but we don't seem
    // to have a cross-platform API for doing that. At least when all
    // streams are closed we shut down the cache, which erases the
    // file at that point.
  }
}

void
MediaCache::NoteBlockUsage(AutoLock& aLock,
                           MediaCacheStream* aStream,
                           int32_t aBlockIndex,
                           int64_t aStreamOffset,
                           MediaCacheStream::ReadMode aMode,
                           TimeStamp aNow)
{
  if (aBlockIndex < 0) {
    // this block is not in the cache yet
    return;
  }

  BlockOwner* bo = GetBlockOwner(aLock, aBlockIndex, aStream);
  if (!bo) {
    // this block is not in the cache yet
    return;
  }

  // The following check has to be <= because the stream offset has
  // not yet been updated for the data read from this block
  NS_ASSERTION(bo->mStreamBlock*BLOCK_SIZE <= aStreamOffset,
               "Using a block that's behind the read position?");

  GetListForBlock(aLock, bo)->RemoveBlock(aBlockIndex);
  bo->mClass =
    (aMode == MediaCacheStream::MODE_METADATA || bo->mClass == METADATA_BLOCK)
    ? METADATA_BLOCK
    : PLAYED_BLOCK;
  // Since this is just being used now, it can definitely be at the front
  // of mMetadataBlocks or mPlayedBlocks
  GetListForBlock(aLock, bo)->AddFirstBlock(aBlockIndex);
  bo->mLastUseTime = aNow;
  Verify(aLock);
}

void
MediaCache::NoteSeek(AutoLock& aLock,
                     MediaCacheStream* aStream,
                     int64_t aOldOffset)
{
  if (aOldOffset < aStream->mStreamOffset) {
    // We seeked forward. Convert blocks from readahead to played.
    // Any readahead block that intersects the seeked-over range must
    // be converted.
    int32_t blockIndex = OffsetToBlockIndex(aOldOffset);
    if (blockIndex < 0) {
      return;
    }
    int32_t endIndex =
      std::min(OffsetToBlockIndex(aStream->mStreamOffset + (BLOCK_SIZE - 1)),
               int32_t(aStream->mBlocks.Length()));
    if (endIndex < 0) {
      return;
    }
    TimeStamp now = TimeStamp::Now();
    while (blockIndex < endIndex) {
      int32_t cacheBlockIndex = aStream->mBlocks[blockIndex];
      if (cacheBlockIndex >= 0) {
        // Marking the block used may not be exactly what we want but
        // it's simple
        NoteBlockUsage(aLock,
                       aStream,
                       cacheBlockIndex,
                       aStream->mStreamOffset,
                       MediaCacheStream::MODE_PLAYBACK,
                       now);
      }
      ++blockIndex;
    }
  } else {
    // We seeked backward. Convert from played to readahead.
    // Any played block that is entirely after the start of the seeked-over
    // range must be converted.
    int32_t blockIndex =
      OffsetToBlockIndex(aStream->mStreamOffset + (BLOCK_SIZE - 1));
    if (blockIndex < 0) {
      return;
    }
    int32_t endIndex =
      std::min(OffsetToBlockIndex(aOldOffset + (BLOCK_SIZE - 1)),
               int32_t(aStream->mBlocks.Length()));
    if (endIndex < 0) {
      return;
    }
    while (blockIndex < endIndex) {
      MOZ_ASSERT(endIndex > 0);
      int32_t cacheBlockIndex = aStream->mBlocks[endIndex - 1];
      if (cacheBlockIndex >= 0) {
        BlockOwner* bo = GetBlockOwner(aLock, cacheBlockIndex, aStream);
        NS_ASSERTION(bo, "Stream doesn't own its blocks?");
        if (bo->mClass == PLAYED_BLOCK) {
          aStream->mPlayedBlocks.RemoveBlock(cacheBlockIndex);
          bo->mClass = READAHEAD_BLOCK;
          // Adding this as the first block is sure to be OK since
          // this must currently be the earliest readahead block
          // (that's why we're proceeding backwards from the end of
          // the seeked range to the start)
          aStream->mReadaheadBlocks.AddFirstBlock(cacheBlockIndex);
          Verify(aLock);
        }
      }
      --endIndex;
    }
  }
}

void
MediaCacheStream::NotifyLoadID(uint32_t aLoadID)
{
  MOZ_ASSERT(aLoadID > 0);

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
    "MediaCacheStream::NotifyLoadID",
    [ client = RefPtr<ChannelMediaResource>(mClient), this, aLoadID ]() {
      AutoLock lock(mMediaCache->Monitor());
      mLoadID = aLoadID;
    });
  OwnerThread()->Dispatch(r.forget());
}

void
MediaCacheStream::NotifyDataStartedInternal(uint32_t aLoadID,
                                            int64_t aOffset,
                                            bool aSeekable,
                                            int64_t aLength)
{
  MOZ_ASSERT(OwnerThread()->IsOnCurrentThread());
  MOZ_ASSERT(aLoadID > 0);
  LOG("Stream %p DataStarted: %" PRId64 " aLoadID=%u aLength=%" PRId64,
      this,
      aOffset,
      aLoadID,
      aLength);

  AutoLock lock(mMediaCache->Monitor());
  NS_WARNING_ASSERTION(aOffset == mSeekTarget || aOffset == mChannelOffset,
                       "Server is giving us unexpected offset");
  MOZ_ASSERT(aOffset >= 0);
  if (aLength >= 0) {
    mStreamLength = aLength;
  }
  mChannelOffset = aOffset;
  if (mStreamLength >= 0) {
    // If we started reading at a certain offset, then for sure
    // the stream is at least that long.
    mStreamLength = std::max(mStreamLength, mChannelOffset);
  }
  mLoadID = aLoadID;

  MOZ_ASSERT(aOffset == 0 || aSeekable,
             "channel offset must be zero when we become non-seekable");
  mIsTransportSeekable = aSeekable;
  // Queue an Update since we may change our strategy for dealing
  // with this stream
  mMediaCache->QueueUpdate(lock);

  // Reset mSeekTarget since the seek is completed so MediaCache::Update() will
  // make decisions based on mChannelOffset instead of mSeekTarget.
  mSeekTarget = -1;

  // Reset these flags since a new load has begun.
  mChannelEnded = false;
  mDidNotifyDataEnded = false;

  UpdateDownloadStatistics(lock);
}

void
MediaCacheStream::NotifyDataStarted(uint32_t aLoadID,
                                    int64_t aOffset,
                                    bool aSeekable,
                                    int64_t aLength)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aLoadID > 0);

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
    "MediaCacheStream::NotifyDataStarted",
    [ =, client = RefPtr<ChannelMediaResource>(mClient) ]() {
      NotifyDataStartedInternal(aLoadID, aOffset, aSeekable, aLength);
    });
  OwnerThread()->Dispatch(r.forget());
}

void
MediaCacheStream::NotifyDataReceived(uint32_t aLoadID,
                                     uint32_t aCount,
                                     const uint8_t* aData)
{
  MOZ_ASSERT(OwnerThread()->IsOnCurrentThread());
  MOZ_ASSERT(aLoadID > 0);

  AutoLock lock(mMediaCache->Monitor());
  if (mClosed) {
    // Nothing to do if the stream is closed.
    return;
  }

  LOG("Stream %p DataReceived at %" PRId64 " count=%u aLoadID=%u",
      this,
      mChannelOffset,
      aCount,
      aLoadID);

  if (mLoadID != aLoadID) {
    // mChannelOffset is updated to a new position when loading a new channel.
    // We should discard the data coming from the old channel so it won't be
    // stored to the wrong positoin.
    return;
  }

  mDownloadStatistics.AddBytes(aCount);

  auto source = MakeSpan<const uint8_t>(aData, aCount);

  // We process the data one block (or part of a block) at a time
  while (!source.IsEmpty()) {
    // The data we've collected so far in the partial block.
    auto partial = MakeSpan<const uint8_t>(mPartialBlockBuffer.get(),
                                           OffsetInBlock(mChannelOffset));

    if (partial.IsEmpty()) {
      // We've just started filling this buffer so now is a good time
      // to clear this flag.
      mMetadataInPartialBlockBuffer = false;
    }

    // The number of bytes needed to complete the partial block.
    size_t remaining = BLOCK_SIZE - partial.Length();

    if (source.Length() >= remaining) {
      // We have a whole block now to write it out.
      mMediaCache->AllocateAndWriteBlock(
        lock,
        this,
        OffsetToBlockIndexUnchecked(mChannelOffset),
        mMetadataInPartialBlockBuffer ? MODE_METADATA : MODE_PLAYBACK,
        partial,
        source.First(remaining));
      source = source.From(remaining);
      mChannelOffset += remaining;
    } else {
      // The buffer to be filled in the partial block.
      auto buf = MakeSpan<uint8_t>(mPartialBlockBuffer.get() + partial.Length(),
                                   remaining);
      memcpy(buf.Elements(), source.Elements(), source.Length());
      mChannelOffset += source.Length();
      break;
    }
  }

  MediaCache::ResourceStreamIterator iter(mMediaCache, mResourceID);
  while (MediaCacheStream* stream = iter.Next(lock)) {
    if (stream->mStreamLength >= 0) {
      // The stream is at least as long as what we've read
      stream->mStreamLength = std::max(stream->mStreamLength, mChannelOffset);
    }
    stream->mClient->CacheClientNotifyDataReceived();
  }

  // Notify in case there's a waiting reader
  // XXX it would be fairly easy to optimize things a lot more to
  // avoid waking up reader threads unnecessarily
  lock.NotifyAll();
}

void
MediaCacheStream::FlushPartialBlockInternal(AutoLock& aLock, bool aNotifyAll)
{
  MOZ_ASSERT(OwnerThread()->IsOnCurrentThread());

  int32_t blockIndex = OffsetToBlockIndexUnchecked(mChannelOffset);
  int32_t blockOffset = OffsetInBlock(mChannelOffset);
  if (blockOffset > 0) {
    LOG("Stream %p writing partial block: [%d] bytes; "
        "mStreamOffset [%" PRId64 "] mChannelOffset[%"
        PRId64 "] mStreamLength [%" PRId64 "] notifying: [%s]",
        this, blockOffset, mStreamOffset, mChannelOffset, mStreamLength,
        aNotifyAll ? "yes" : "no");

    // Write back the partial block
    memset(mPartialBlockBuffer.get() + blockOffset, 0, BLOCK_SIZE - blockOffset);
    auto data = MakeSpan<const uint8_t>(mPartialBlockBuffer.get(), BLOCK_SIZE);
    mMediaCache->AllocateAndWriteBlock(
      aLock,
      this,
      blockIndex,
      mMetadataInPartialBlockBuffer ? MODE_METADATA : MODE_PLAYBACK,
      data);
  }

  // |mChannelOffset == 0| means download ends with no bytes received.
  // We should also wake up those readers who are waiting for data
  // that will never come.
  if ((blockOffset > 0 || mChannelOffset == 0) && aNotifyAll) {
    // Wake up readers who may be waiting for this data
    aLock.NotifyAll();
  }
}

void
MediaCacheStream::UpdateDownloadStatistics(AutoLock&)
{
  if (mChannelEnded || mClientSuspended) {
    mDownloadStatistics.Stop();
  } else {
    mDownloadStatistics.Start();
  }
}

void
MediaCacheStream::NotifyDataEndedInternal(uint32_t aLoadID,
                                          nsresult aStatus,
                                          bool aReopenOnError)
{
  MOZ_ASSERT(OwnerThread()->IsOnCurrentThread());
  AutoLock lock(mMediaCache->Monitor());

  if (mClosed || aLoadID != mLoadID) {
    // Nothing to do if the stream is closed or a new load has begun.
    return;
  }

  // Note that aStatus might have succeeded --- this might be a normal close
  // --- even in situations where the server cut us off because we were
  // suspended. It is also possible that the server sends us fewer bytes than
  // requested. So we need to "reopen on error" in that case too. The only
  // cases where we don't need to reopen are when *we* closed the stream.
  // But don't reopen if we need to seek and we don't think we can... that would
  // cause us to just re-read the stream, which would be really bad.
  /*
   * | length |    offset |   reopen |
   * +--------+-----------+----------+
   * |     -1 |         0 |      yes |
   * +--------+-----------+----------+
   * |     -1 |       > 0 | seekable |
   * +--------+-----------+----------+
   * |      0 |         X |       no |
   * +--------+-----------+----------+
   * |    > 0 |         0 |      yes |
   * +--------+-----------+----------+
   * |    > 0 | != length | seekable |
   * +--------+-----------+----------+
   * |    > 0 | == length |       no |
   */
  if (aReopenOnError && aStatus != NS_ERROR_PARSED_DATA_CACHED &&
      aStatus != NS_BINDING_ABORTED &&
      (mChannelOffset == 0 || mIsTransportSeekable) &&
      mChannelOffset != mStreamLength) {
    // If the stream did close normally, restart the channel if we're either
    // at the start of the resource, or if the server is seekable and we're
    // not at the end of stream. We don't restart the stream if we're at the
    // end because not all web servers handle this case consistently; see:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1373618#c36
    mClient->CacheClientSeek(mChannelOffset, false);
    return;
    // Note CacheClientSeek() will call Seek() asynchronously which might fail
    // and close the stream. This is OK for it is not an error we can recover
    // from and we have a consistent behavior with that where CacheClientSeek()
    // is called from MediaCache::Update().
  }

  // It is prudent to update channel/cache status before calling
  // CacheClientNotifyDataEnded() which will read |mChannelEnded|.
  mChannelEnded = true;
  mMediaCache->QueueUpdate(lock);

  UpdateDownloadStatistics(lock);

  if (NS_FAILED(aStatus)) {
    // Notify the client about this network error.
    mDidNotifyDataEnded = true;
    mNotifyDataEndedStatus = aStatus;
    mClient->CacheClientNotifyDataEnded(aStatus);
    // Wake up the readers so they can fail gracefully.
    lock.NotifyAll();
    return;
  }

  // Note we don't flush the partial block when download ends abnormally for
  // the padding zeros will give wrong data to other streams.
  FlushPartialBlockInternal(lock, true);

  MediaCache::ResourceStreamIterator iter(mMediaCache, mResourceID);
  while (MediaCacheStream* stream = iter.Next(lock)) {
    // We read the whole stream, so remember the true length
    stream->mStreamLength = mChannelOffset;
    if (!stream->mDidNotifyDataEnded) {
      stream->mDidNotifyDataEnded = true;
      stream->mNotifyDataEndedStatus = aStatus;
      stream->mClient->CacheClientNotifyDataEnded(aStatus);
    }
  }
}

void
MediaCacheStream::NotifyDataEnded(uint32_t aLoadID,
                                  nsresult aStatus,
                                  bool aReopenOnError)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aLoadID > 0);

  RefPtr<ChannelMediaResource> client = mClient;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
    "MediaCacheStream::NotifyDataEnded",
    [client, this, aLoadID, aStatus, aReopenOnError]() {
      NotifyDataEndedInternal(aLoadID, aStatus, aReopenOnError);
    });
  OwnerThread()->Dispatch(r.forget());
}

void
MediaCacheStream::NotifyClientSuspended(bool aSuspended)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ChannelMediaResource> client = mClient;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
    "MediaCacheStream::NotifyClientSuspended", [client, this, aSuspended]() {
      AutoLock lock(mMediaCache->Monitor());
      if (!mClosed && mClientSuspended != aSuspended) {
        mClientSuspended = aSuspended;
        // mClientSuspended changes the decision of reading streams.
        mMediaCache->QueueUpdate(lock);
        UpdateDownloadStatistics(lock);
      }
    });
  OwnerThread()->Dispatch(r.forget());
}

void
MediaCacheStream::NotifyResume()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
    "MediaCacheStream::NotifyResume",
    [ this, client = RefPtr<ChannelMediaResource>(mClient) ]() {
      AutoLock lock(mMediaCache->Monitor());
      if (mClosed) {
        return;
      }
      // Don't resume download if we are already at the end of the stream for
      // seek will fail and be wasted anyway.
      int64_t offset = mSeekTarget != -1 ? mSeekTarget : mChannelOffset;
      if (mStreamLength < 0 || offset < mStreamLength) {
        mClient->CacheClientSeek(offset, false);
        // DownloadResumed() will be notified when a new channel is opened.
      }
      // The channel remains dead. If we want to read some other data in the
      // future, CacheClientSeek() will be called to reopen the channel.
    });
  OwnerThread()->Dispatch(r.forget());
}

MediaCacheStream::~MediaCacheStream()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  NS_ASSERTION(!mPinCount, "Unbalanced Pin");

  if (mMediaCache) {
    NS_ASSERTION(mClosed, "Stream was not closed");
    mMediaCache->ReleaseStream(this);
  }

  uint32_t lengthKb = uint32_t(
    std::min(std::max(mStreamLength, int64_t(0)) / 1024, int64_t(UINT32_MAX)));
  LOG("MediaCacheStream::~MediaCacheStream(this=%p) "
      "MEDIACACHESTREAM_LENGTH_KB=%" PRIu32,
      this,
      lengthKb);
  Telemetry::Accumulate(Telemetry::HistogramID::MEDIACACHESTREAM_LENGTH_KB,
                        lengthKb);
}

bool
MediaCacheStream::AreAllStreamsForResourceSuspended(AutoLock& aLock)
{
  MOZ_ASSERT(!NS_IsMainThread());

  MediaCache::ResourceStreamIterator iter(mMediaCache, mResourceID);
  // Look for a stream that's able to read the data we need
  int64_t dataOffset = -1;
  while (MediaCacheStream* stream = iter.Next(aLock)) {
    if (stream->mCacheSuspended || stream->mChannelEnded || stream->mClosed) {
      continue;
    }
    if (dataOffset < 0) {
      dataOffset = GetCachedDataEndInternal(aLock, mStreamOffset);
    }
    // Ignore streams that are reading beyond the data we need
    if (stream->mChannelOffset > dataOffset) {
      continue;
    }
    return false;
  }

  return true;
}

void
MediaCacheStream::Close()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mMediaCache) {
    return;
  }
  OwnerThread()->Dispatch(NS_NewRunnableFunction(
    "MediaCacheStream::Close",
    [ this, client = RefPtr<ChannelMediaResource>(mClient) ]() {
      AutoLock lock(mMediaCache->Monitor());
      CloseInternal(lock);
    }));
}

void
MediaCacheStream::CloseInternal(AutoLock& aLock)
{
  MOZ_ASSERT(OwnerThread()->IsOnCurrentThread());

  if (mClosed) {
    return;
  }

  // Closing a stream will change the return value of
  // MediaCacheStream::AreAllStreamsForResourceSuspended as well as
  // ChannelMediaResource::IsSuspendedByCache. Let's notify it.
  mMediaCache->QueueSuspendedStatusUpdate(aLock, mResourceID);

  mClosed = true;
  mMediaCache->ReleaseStreamBlocks(aLock, this);
  // Wake up any blocked readers
  aLock.NotifyAll();

  // Queue an Update since we may have created more free space.
  mMediaCache->QueueUpdate(aLock);
}

void
MediaCacheStream::Pin()
{
  // TODO: Assert non-main thread.
  AutoLock lock(mMediaCache->Monitor());
  ++mPinCount;
  // Queue an Update since we may no longer want to read more into the
  // cache, if this stream's block have become non-evictable
  mMediaCache->QueueUpdate(lock);
}

void
MediaCacheStream::Unpin()
{
  // TODO: Assert non-main thread.
  AutoLock lock(mMediaCache->Monitor());
  NS_ASSERTION(mPinCount > 0, "Unbalanced Unpin");
  --mPinCount;
  // Queue an Update since we may be able to read more into the
  // cache, if this stream's block have become evictable
  mMediaCache->QueueUpdate(lock);
}

int64_t
MediaCacheStream::GetLength()
{
  MOZ_ASSERT(!NS_IsMainThread());
  AutoLock lock(mMediaCache->Monitor());
  return mStreamLength;
}

int64_t
MediaCacheStream::GetNextCachedData(int64_t aOffset)
{
  MOZ_ASSERT(!NS_IsMainThread());
  AutoLock lock(mMediaCache->Monitor());
  return GetNextCachedDataInternal(lock, aOffset);
}

int64_t
MediaCacheStream::GetCachedDataEnd(int64_t aOffset)
{
  MOZ_ASSERT(!NS_IsMainThread());
  AutoLock lock(mMediaCache->Monitor());
  return GetCachedDataEndInternal(lock, aOffset);
}

bool
MediaCacheStream::IsDataCachedToEndOfStream(int64_t aOffset)
{
  MOZ_ASSERT(!NS_IsMainThread());
  AutoLock lock(mMediaCache->Monitor());
  if (mStreamLength < 0)
    return false;
  return GetCachedDataEndInternal(lock, aOffset) >= mStreamLength;
}

int64_t
MediaCacheStream::GetCachedDataEndInternal(AutoLock&, int64_t aOffset)
{
  int32_t blockIndex = OffsetToBlockIndex(aOffset);
  if (blockIndex < 0) {
    return aOffset;
  }
  while (size_t(blockIndex) < mBlocks.Length() && mBlocks[blockIndex] != -1) {
    ++blockIndex;
  }
  int64_t result = blockIndex*BLOCK_SIZE;
  if (blockIndex == OffsetToBlockIndexUnchecked(mChannelOffset)) {
    // The block containing mChannelOffset may be partially read but not
    // yet committed to the main cache
    result = mChannelOffset;
  }
  if (mStreamLength >= 0) {
    // The last block in the cache may only be partially valid, so limit
    // the cached range to the stream length
    result = std::min(result, mStreamLength);
  }
  return std::max(result, aOffset);
}

int64_t
MediaCacheStream::GetNextCachedDataInternal(AutoLock&, int64_t aOffset)
{
  if (aOffset == mStreamLength)
    return -1;

  int32_t startBlockIndex = OffsetToBlockIndex(aOffset);
  if (startBlockIndex < 0) {
    return -1;
  }
  int32_t channelBlockIndex = OffsetToBlockIndexUnchecked(mChannelOffset);

  if (startBlockIndex == channelBlockIndex &&
      aOffset < mChannelOffset) {
    // The block containing mChannelOffset is partially read, but not
    // yet committed to the main cache. aOffset lies in the partially
    // read portion, thus it is effectively cached.
    return aOffset;
  }

  if (size_t(startBlockIndex) >= mBlocks.Length())
    return -1;

  // Is the current block cached?
  if (mBlocks[startBlockIndex] != -1)
    return aOffset;

  // Count the number of uncached blocks
  bool hasPartialBlock = OffsetInBlock(mChannelOffset) != 0;
  int32_t blockIndex = startBlockIndex + 1;
  while (true) {
    if ((hasPartialBlock && blockIndex == channelBlockIndex) ||
        (size_t(blockIndex) < mBlocks.Length() && mBlocks[blockIndex] != -1)) {
      // We at the incoming channel block, which has has data in it,
      // or are we at a cached block. Return index of block start.
      return blockIndex * BLOCK_SIZE;
    }

    // No more cached blocks?
    if (size_t(blockIndex) >= mBlocks.Length())
      return -1;

    ++blockIndex;
  }

  NS_NOTREACHED("Should return in loop");
  return -1;
}

void
MediaCacheStream::SetReadMode(ReadMode aMode)
{
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
    "MediaCacheStream::SetReadMode",
    [ this, client = RefPtr<ChannelMediaResource>(mClient), aMode ]() {
      AutoLock lock(mMediaCache->Monitor());
      if (!mClosed && mCurrentMode != aMode) {
        mCurrentMode = aMode;
        mMediaCache->QueueUpdate(lock);
      }
    });
  OwnerThread()->Dispatch(r.forget());
}

void
MediaCacheStream::SetPlaybackRate(uint32_t aBytesPerSecond)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aBytesPerSecond > 0, "Zero playback rate not allowed");

  AutoLock lock(mMediaCache->Monitor());
  if (!mClosed && mPlaybackBytesPerSecond != aBytesPerSecond) {
    mPlaybackBytesPerSecond = aBytesPerSecond;
    mMediaCache->QueueUpdate(lock);
  }
}

nsresult
MediaCacheStream::Seek(AutoLock& aLock, int64_t aOffset)
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (!IsOffsetAllowed(aOffset)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (mClosed) {
    return NS_ERROR_ABORT;
  }

  int64_t oldOffset = mStreamOffset;
  mStreamOffset = aOffset;
  LOG("Stream %p Seek to %" PRId64, this, mStreamOffset);
  mMediaCache->NoteSeek(aLock, this, oldOffset);
  mMediaCache->QueueUpdate(aLock);
  return NS_OK;
}

void
MediaCacheStream::ThrottleReadahead(bool bThrottle)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
    "MediaCacheStream::ThrottleReadahead",
    [ client = RefPtr<ChannelMediaResource>(mClient), this, bThrottle ]() {
      AutoLock lock(mMediaCache->Monitor());
      if (!mClosed && mThrottleReadahead != bThrottle) {
        LOGI("Stream %p ThrottleReadahead %d", this, bThrottle);
        mThrottleReadahead = bThrottle;
        mMediaCache->QueueUpdate(lock);
      }
    });
  OwnerThread()->Dispatch(r.forget());
}

uint32_t
MediaCacheStream::ReadPartialBlock(AutoLock&,
                                   int64_t aOffset,
                                   Span<char> aBuffer)
{
  MOZ_ASSERT(IsOffsetAllowed(aOffset));

  if (OffsetToBlockIndexUnchecked(mChannelOffset) !=
        OffsetToBlockIndexUnchecked(aOffset) ||
      aOffset >= mChannelOffset) {
    // Not in the partial block or no data to read.
    return 0;
  }

  auto source = MakeSpan<const uint8_t>(
    mPartialBlockBuffer.get() + OffsetInBlock(aOffset),
    OffsetInBlock(mChannelOffset) - OffsetInBlock(aOffset));
  // We have |source.Length() <= BLOCK_SIZE < INT32_MAX| to guarantee
  // that |bytesToRead| can fit into a uint32_t.
  uint32_t bytesToRead = std::min(aBuffer.Length(), source.Length());
  memcpy(aBuffer.Elements(), source.Elements(), bytesToRead);
  return bytesToRead;
}

Result<uint32_t, nsresult>
MediaCacheStream::ReadBlockFromCache(AutoLock& aLock,
                                     int64_t aOffset,
                                     Span<char> aBuffer,
                                     bool aNoteBlockUsage)
{
  MOZ_ASSERT(IsOffsetAllowed(aOffset));

  // OffsetToBlockIndexUnchecked() is always non-negative.
  uint32_t index = OffsetToBlockIndexUnchecked(aOffset);
  int32_t cacheBlock = index < mBlocks.Length() ? mBlocks[index] : -1;
  if (cacheBlock < 0) {
    // Not in the cache.
    return 0;
  }

  if (aBuffer.Length() > size_t(BLOCK_SIZE)) {
    // Clamp the buffer to avoid overflow below since we will read at most
    // BLOCK_SIZE bytes.
    aBuffer = aBuffer.First(BLOCK_SIZE);
  }
  // |BLOCK_SIZE - OffsetInBlock(aOffset)| <= BLOCK_SIZE
  int32_t bytesToRead =
    std::min<int32_t>(BLOCK_SIZE - OffsetInBlock(aOffset), aBuffer.Length());
  int32_t bytesRead = 0;
  nsresult rv =
    mMediaCache->ReadCacheFile(aLock,
                               cacheBlock * BLOCK_SIZE + OffsetInBlock(aOffset),
                               aBuffer.Elements(),
                               bytesToRead,
                               &bytesRead);

  // Ensure |cacheBlock * BLOCK_SIZE + OffsetInBlock(aOffset)| won't overflow.
  static_assert(INT64_MAX >= BLOCK_SIZE * (uint32_t(INT32_MAX) + 1),
                "BLOCK_SIZE too large!");

  if (NS_FAILED(rv)) {
    nsCString name;
    GetErrorName(rv, name);
    LOGE("Stream %p ReadCacheFile failed, rv=%s", this, name.Data());
    return mozilla::Err(rv);
  }

  if (aNoteBlockUsage) {
    mMediaCache->NoteBlockUsage(
      aLock, this, cacheBlock, aOffset, mCurrentMode, TimeStamp::Now());
  }

  return bytesRead;
}

nsresult
MediaCacheStream::Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes)
{
  MOZ_ASSERT(!NS_IsMainThread());
  AutoLock lock(mMediaCache->Monitor());

  // Cache the offset in case it is changed again when we are waiting for the
  // monitor to be notified to avoid reading at the wrong position.
  auto streamOffset = mStreamOffset;

  // The buffer we are about to fill.
  auto buffer = MakeSpan<char>(aBuffer, aCount);

  // Read one block (or part of a block) at a time
  while (!buffer.IsEmpty()) {
    if (mClosed) {
      return NS_ERROR_ABORT;
    }

    if (!IsOffsetAllowed(streamOffset)) {
      LOGE("Stream %p invalid offset=%" PRId64, this, streamOffset);
      return NS_ERROR_ILLEGAL_VALUE;
    }

    if (mStreamLength >= 0 && streamOffset >= mStreamLength) {
      // Don't try to read beyond the end of the stream
      break;
    }

    Result<uint32_t, nsresult> rv = ReadBlockFromCache(
      lock, streamOffset, buffer, true /* aNoteBlockUsage */);
    if (rv.isErr()) {
      return rv.unwrapErr();
    }

    uint32_t bytes = rv.unwrap();
    if (bytes > 0) {
      // Got data from the cache successfully. Read next block.
      streamOffset += bytes;
      buffer = buffer.From(bytes);
      continue;
    }

    // See if we can use the data in the partial block of any stream reading
    // this resource. Note we use the partial block only when it is completed,
    // that is reaching EOS.
    bool foundDataInPartialBlock = false;
    MediaCache::ResourceStreamIterator iter(mMediaCache, mResourceID);
    while (MediaCacheStream* stream = iter.Next(lock)) {
      if (OffsetToBlockIndexUnchecked(stream->mChannelOffset) ==
            OffsetToBlockIndexUnchecked(streamOffset) &&
          stream->mChannelOffset == stream->mStreamLength) {
        uint32_t bytes = stream->ReadPartialBlock(lock, streamOffset, buffer);
        streamOffset += bytes;
        buffer = buffer.From(bytes);
        foundDataInPartialBlock = true;
        break;
      }
    }
    if (foundDataInPartialBlock) {
      // Break for we've reached EOS.
      break;
    }

    if (mDidNotifyDataEnded && NS_FAILED(mNotifyDataEndedStatus)) {
      // Since download ends abnormally, there is no point in waiting for new
      // data to come. We will check the partial block to read as many bytes as
      // possible before exiting this function.
      bytes = ReadPartialBlock(lock, streamOffset, buffer);
      streamOffset += bytes;
      buffer = buffer.From(bytes);
      break;
    }

    if (mStreamOffset != streamOffset) {
      // Update mStreamOffset before we drop the lock. We need to run
      // Update() again since stream reading strategy might have changed.
      mStreamOffset = streamOffset;
      mMediaCache->QueueUpdate(lock);
    }

    // No data to read, so block
    lock.Wait();
    continue;
  }

  uint32_t count = buffer.Elements() - aBuffer;
  *aBytes = count;
  if (count == 0) {
    return NS_OK;
  }

  // Some data was read, so queue an update since block priorities may
  // have changed
  mMediaCache->QueueUpdate(lock);

  LOG("Stream %p Read at %" PRId64 " count=%d", this, streamOffset-count, count);
  mStreamOffset = streamOffset;
  return NS_OK;
}

nsresult
MediaCacheStream::ReadAt(int64_t aOffset, char* aBuffer,
                         uint32_t aCount, uint32_t* aBytes)
{
  MOZ_ASSERT(!NS_IsMainThread());
  AutoLock lock(mMediaCache->Monitor());
  nsresult rv = Seek(lock, aOffset);
  if (NS_FAILED(rv)) return rv;
  return Read(aBuffer, aCount, aBytes);
}

nsresult
MediaCacheStream::ReadFromCache(char* aBuffer, int64_t aOffset, uint32_t aCount)
{
  MOZ_ASSERT(!NS_IsMainThread());
  AutoLock lock(mMediaCache->Monitor());

  // The buffer we are about to fill.
  auto buffer = MakeSpan<char>(aBuffer, aCount);

  // Read one block (or part of a block) at a time
  int64_t streamOffset = aOffset;
  while (!buffer.IsEmpty()) {
    if (mClosed) {
      // We need to check |mClosed| in each iteration which might be changed
      // after calling |mMediaCache->ReadCacheFile|.
      return NS_ERROR_FAILURE;
    }

    if (!IsOffsetAllowed(streamOffset)) {
      LOGE("Stream %p invalid offset=%" PRId64, this, streamOffset);
      return NS_ERROR_ILLEGAL_VALUE;
    }

    Result<uint32_t, nsresult> rv =
      ReadBlockFromCache(lock, streamOffset, buffer);
    if (rv.isErr()) {
      return rv.unwrapErr();
    }

    uint32_t bytes = rv.unwrap();
    if (bytes > 0) {
      // Read data from the cache successfully. Let's try next block.
      streamOffset += bytes;
      buffer = buffer.From(bytes);
      continue;
    }

    // The partial block is our last chance to get data.
    bytes = ReadPartialBlock(lock, streamOffset, buffer);
    if (bytes < buffer.Length()) {
      // Not enough data to read.
      return NS_ERROR_FAILURE;
    }

    // Return for we've got all the requested bytes.
    return NS_OK;
  }

  return NS_OK;
}

nsresult
MediaCacheStream::Init(int64_t aContentLength)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  MOZ_ASSERT(!mMediaCache, "Has been initialized.");

  if (aContentLength > 0) {
    uint32_t length = uint32_t(std::min(aContentLength, int64_t(UINT32_MAX)));
    LOG("MediaCacheStream::Init(this=%p) "
        "MEDIACACHESTREAM_NOTIFIED_LENGTH=%" PRIu32,
        this,
        length);
    Telemetry::Accumulate(
      Telemetry::HistogramID::MEDIACACHESTREAM_NOTIFIED_LENGTH, length);

    mStreamLength = aContentLength;
  }

  mMediaCache = MediaCache::GetMediaCache(aContentLength);
  if (!mMediaCache) {
    return NS_ERROR_FAILURE;
  }

  AutoLock lock(mMediaCache->Monitor());
  mMediaCache->OpenStream(lock, this);
  return NS_OK;
}

void
MediaCacheStream::InitAsClone(MediaCacheStream* aOriginal)
{
  MOZ_ASSERT(!mMediaCache, "Has been initialized.");
  MOZ_ASSERT(aOriginal->mMediaCache, "Don't clone an uninitialized stream.");

  // Use the same MediaCache as our clone.
  mMediaCache = aOriginal->mMediaCache;
  // This needs to be done before OpenStream() to avoid data race.
  mClientSuspended = true;
  // Cloned streams are initially suspended, since there is no channel open
  // initially for a clone.
  mCacheSuspended = true;
  mChannelEnded = true;

  AutoLock lock(aOriginal->mMediaCache->Monitor());

  mResourceID = aOriginal->mResourceID;

  // Grab cache blocks from aOriginal as readahead blocks for our stream
  mStreamLength = aOriginal->mStreamLength;
  mIsTransportSeekable = aOriginal->mIsTransportSeekable;
  mDownloadStatistics = aOriginal->mDownloadStatistics;
  mDownloadStatistics.Stop();

  if (aOriginal->mDidNotifyDataEnded &&
      NS_SUCCEEDED(aOriginal->mNotifyDataEndedStatus)) {
    mNotifyDataEndedStatus = aOriginal->mNotifyDataEndedStatus;
    mDidNotifyDataEnded = true;
    mClient->CacheClientNotifyDataEnded(mNotifyDataEndedStatus);
  }

  for (uint32_t i = 0; i < aOriginal->mBlocks.Length(); ++i) {
    int32_t cacheBlockIndex = aOriginal->mBlocks[i];
    if (cacheBlockIndex < 0)
      continue;

    while (i >= mBlocks.Length()) {
      mBlocks.AppendElement(-1);
    }
    // Every block is a readahead block for the clone because the clone's initial
    // stream offset is zero
    mMediaCache->AddBlockOwnerAsReadahead(lock, cacheBlockIndex, this, i);
  }

  // Copy the partial block.
  mChannelOffset = aOriginal->mChannelOffset;
  memcpy(mPartialBlockBuffer.get(),
         aOriginal->mPartialBlockBuffer.get(),
         BLOCK_SIZE);

  mMediaCache->OpenStream(lock, this, true /* aIsClone */);
}

nsIEventTarget*
MediaCacheStream::OwnerThread() const
{
  return mMediaCache->OwnerThread();
}

nsresult MediaCacheStream::GetCachedRanges(MediaByteRangeSet& aRanges)
{
  MOZ_ASSERT(!NS_IsMainThread());
  // Take the monitor, so that the cached data ranges can't grow while we're
  // trying to loop over them.
  AutoLock lock(mMediaCache->Monitor());

  // We must be pinned while running this, otherwise the cached data ranges may
  // shrink while we're trying to loop over them.
  NS_ASSERTION(mPinCount > 0, "Must be pinned");

  int64_t startOffset = GetNextCachedDataInternal(lock, 0);
  while (startOffset >= 0) {
    int64_t endOffset = GetCachedDataEndInternal(lock, startOffset);
    NS_ASSERTION(startOffset < endOffset, "Buffered range must end after its start");
    // Bytes [startOffset..endOffset] are cached.
    aRanges += MediaByteRange(startOffset, endOffset);
    startOffset = GetNextCachedDataInternal(lock, endOffset);
    NS_ASSERTION(startOffset == -1 || startOffset > endOffset,
      "Must have advanced to start of next range, or hit end of stream");
  }
  return NS_OK;
}

double
MediaCacheStream::GetDownloadRate(bool* aIsReliable)
{
  MOZ_ASSERT(!NS_IsMainThread());
  AutoLock lock(mMediaCache->Monitor());
  return mDownloadStatistics.GetRate(aIsReliable);
}

nsCString
MediaCacheStream::GetDebugInfo()
{
  AutoLock lock(mMediaCache->Monitor());
  return nsPrintfCString("mStreamLength=%" PRId64 " mChannelOffset=%" PRId64
                         " mCacheSuspended=%d mChannelEnded=%d mLoadID=%u",
                         mStreamLength,
                         mChannelOffset,
                         mCacheSuspended,
                         mChannelEnded,
                         mLoadID);
}

} // namespace mozilla

// avoid redefined macro in unified build
#undef LOG
#undef LOGI
