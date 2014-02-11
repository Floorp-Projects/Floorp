/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ReentrantMonitor.h"

#include "MediaCache.h"
#include "prio.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "MediaResource.h"
#include "prlog.h"
#include "mozilla/Preferences.h"
#include "FileBlockCache.h"
#include "nsAnonymousTemporaryFile.h"
#include "nsIObserverService.h"
#include "nsISeekableStream.h"
#include "nsIPrincipal.h"
#include "mozilla/Attributes.h"
#include "mozilla/Services.h"
#include <algorithm>

namespace mozilla {

#ifdef PR_LOGGING
PRLogModuleInfo* gMediaCacheLog;
#define CACHE_LOG(type, msg) PR_LOG(gMediaCacheLog, type, msg)
#else
#define CACHE_LOG(type, msg)
#endif

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

// Try to save power by not resuming paused reads if the stream won't need new
// data within this time interval in the future
static const uint32_t CACHE_POWERSAVE_WAKEUP_LOW_THRESHOLD_MS = 10000;

#ifdef DEBUG
// Turn this on to do very expensive cache state validation
// #define DEBUG_VERIFY_CACHE
#endif

// There is at most one media cache (although that could quite easily be
// relaxed if we wanted to manage multiple caches with independent
// size limits).
static MediaCache* gMediaCache;

class MediaCacheFlusher MOZ_FINAL : public nsIObserver,
                                      public nsSupportsWeakReference {
  MediaCacheFlusher() {}
  ~MediaCacheFlusher();
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static void Init();
};

static MediaCacheFlusher* gMediaCacheFlusher;

NS_IMPL_ISUPPORTS2(MediaCacheFlusher, nsIObserver, nsISupportsWeakReference)

MediaCacheFlusher::~MediaCacheFlusher()
{
  gMediaCacheFlusher = nullptr;
}

void MediaCacheFlusher::Init()
{
  if (gMediaCacheFlusher) {
    return;
  }

  gMediaCacheFlusher = new MediaCacheFlusher();
  NS_ADDREF(gMediaCacheFlusher);

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(gMediaCacheFlusher, "last-pb-context-exited", true);
    observerService->AddObserver(gMediaCacheFlusher, "network-clear-cache-stored-anywhere", true);
  }
}

class MediaCache {
public:
  friend class MediaCacheStream::BlockList;
  typedef MediaCacheStream::BlockList BlockList;
  enum {
    BLOCK_SIZE = MediaCacheStream::BLOCK_SIZE
  };

  MediaCache() : mNextResourceID(1),
    mReentrantMonitor("MediaCache.mReentrantMonitor"),
    mUpdateQueued(false)
#ifdef DEBUG
    , mInUpdate(false)
#endif
  {
    MOZ_COUNT_CTOR(MediaCache);
  }
  ~MediaCache() {
    NS_ASSERTION(mStreams.IsEmpty(), "Stream(s) still open!");
    Truncate();
    NS_ASSERTION(mIndex.Length() == 0, "Blocks leaked?");
    if (mFileCache) {
      mFileCache->Close();
      mFileCache = nullptr;
    }
    MOZ_COUNT_DTOR(MediaCache);
  }

  // Main thread only. Creates the backing cache file. If this fails,
  // then the cache is still in a semi-valid state; mFD will be null,
  // so all I/O on the cache file will fail.
  nsresult Init();
  // Shut down the global cache if it's no longer needed. We shut down
  // the cache as soon as there are no streams. This means that during
  // normal operation we are likely to start up the cache and shut it down
  // many times, but that's OK since starting it up is cheap and
  // shutting it down cleans things up and releases disk space.
  static void MaybeShutdown();

  // Brutally flush the cache contents. Main thread only.
  static void Flush();
  void FlushInternal();

  // Cache-file access methods. These are the lowest-level cache methods.
  // mReentrantMonitor must be held; these can be called on any thread.
  // This can return partial reads.
  nsresult ReadCacheFile(int64_t aOffset, void* aData, int32_t aLength,
                         int32_t* aBytes);
  // This will fail if all aLength bytes are not read
  nsresult ReadCacheFileAllBytes(int64_t aOffset, void* aData, int32_t aLength);

  int64_t AllocateResourceID()
  {
    mReentrantMonitor.AssertCurrentThreadIn();
    return mNextResourceID++;
  }

  // mReentrantMonitor must be held, called on main thread.
  // These methods are used by the stream to set up and tear down streams,
  // and to handle reads and writes.
  // Add aStream to the list of streams.
  void OpenStream(MediaCacheStream* aStream);
  // Remove aStream from the list of streams.
  void ReleaseStream(MediaCacheStream* aStream);
  // Free all blocks belonging to aStream.
  void ReleaseStreamBlocks(MediaCacheStream* aStream);
  // Find a cache entry for this data, and write the data into it
  void AllocateAndWriteBlock(MediaCacheStream* aStream, const void* aData,
                             MediaCacheStream::ReadMode aMode);

  // mReentrantMonitor must be held; can be called on any thread
  // Notify the cache that a seek has been requested. Some blocks may
  // need to change their class between PLAYED_BLOCK and READAHEAD_BLOCK.
  // This does not trigger channel seeks directly, the next Update()
  // will do that if necessary. The caller will call QueueUpdate().
  void NoteSeek(MediaCacheStream* aStream, int64_t aOldOffset);
  // Notify the cache that a block has been read from. This is used
  // to update last-use times. The block may not actually have a
  // cache entry yet since Read can read data from a stream's
  // in-memory mPartialBlockBuffer while the block is only partly full,
  // and thus hasn't yet been committed to the cache. The caller will
  // call QueueUpdate().
  void NoteBlockUsage(MediaCacheStream* aStream, int32_t aBlockIndex,
                      MediaCacheStream::ReadMode aMode, TimeStamp aNow);
  // Mark aStream as having the block, adding it as an owner.
  void AddBlockOwnerAsReadahead(int32_t aBlockIndex, MediaCacheStream* aStream,
                                int32_t aStreamBlockIndex);

  // This queues a call to Update() on the main thread.
  void QueueUpdate();

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
  void Verify();
#else
  void Verify() {}
#endif

  ReentrantMonitor& GetReentrantMonitor() { return mReentrantMonitor; }

  /**
   * An iterator that makes it easy to iterate through all streams that
   * have a given resource ID and are not closed.
   * Can be used on the main thread or while holding the media cache lock.
   */
  class ResourceStreamIterator {
  public:
    ResourceStreamIterator(int64_t aResourceID) :
      mResourceID(aResourceID), mNext(0) {}
    MediaCacheStream* Next()
    {
      while (mNext < gMediaCache->mStreams.Length()) {
        MediaCacheStream* stream = gMediaCache->mStreams[mNext];
        ++mNext;
        if (stream->GetResourceID() == mResourceID && !stream->IsClosed())
          return stream;
      }
      return nullptr;
    }
  private:
    int64_t  mResourceID;
    uint32_t mNext;
  };

protected:
  // Find a free or reusable block and return its index. If there are no
  // free blocks and no reusable blocks, add a new block to the cache
  // and return it. Can return -1 on OOM.
  int32_t FindBlockForIncomingData(TimeStamp aNow, MediaCacheStream* aStream);
  // Find a reusable block --- a free block, if there is one, otherwise
  // the reusable block with the latest predicted-next-use, or -1 if
  // there aren't any freeable blocks. Only block indices less than
  // aMaxSearchBlockIndex are considered. If aForStream is non-null,
  // then aForStream and aForStreamBlock indicate what media data will
  // be placed; FindReusableBlock will favour returning free blocks
  // near other blocks for that point in the stream.
  int32_t FindReusableBlock(TimeStamp aNow,
                            MediaCacheStream* aForStream,
                            int32_t aForStreamBlock,
                            int32_t aMaxSearchBlockIndex);
  bool BlockIsReusable(int32_t aBlockIndex);
  // Given a list of blocks sorted with the most reusable blocks at the
  // end, find the last block whose stream is not pinned (if any)
  // and whose cache entry index is less than aBlockIndexLimit
  // and append it to aResult.
  void AppendMostReusableBlock(BlockList* aBlockList,
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
    BlockOwner() : mStream(nullptr), mClass(READAHEAD_BLOCK) {}

    // The stream that owns this block, or null if the block is free.
    MediaCacheStream* mStream;
    // The block index in the stream. Valid only if mStream is non-null.
    uint32_t            mStreamBlock;
    // Time at which this block was last used. Valid only if
    // mClass is METADATA_BLOCK or PLAYED_BLOCK.
    TimeStamp           mLastUseTime;
    BlockClass          mClass;
  };

  struct Block {
    // Free blocks have an empty mOwners array
    nsTArray<BlockOwner> mOwners;
  };

  // Get the BlockList that the block should belong to given its
  // current owner
  BlockList* GetListForBlock(BlockOwner* aBlock);
  // Get the BlockOwner for the given block index and owning stream
  // (returns null if the stream does not own the block)
  BlockOwner* GetBlockOwner(int32_t aBlockIndex, MediaCacheStream* aStream);
  // Returns true iff the block is free
  bool IsBlockFree(int32_t aBlockIndex)
  { return mIndex[aBlockIndex].mOwners.IsEmpty(); }
  // Add the block to the free list and mark its streams as not having
  // the block in cache
  void FreeBlock(int32_t aBlock);
  // Mark aStream as not having the block, removing it as an owner. If
  // the block has no more owners it's added to the free list.
  void RemoveBlockOwner(int32_t aBlockIndex, MediaCacheStream* aStream);
  // Swap all metadata associated with the two blocks. The caller
  // is responsible for swapping up any cache file state.
  void SwapBlocks(int32_t aBlockIndex1, int32_t aBlockIndex2);
  // Insert the block into the readahead block list for the stream
  // at the right point in the list.
  void InsertReadaheadBlock(BlockOwner* aBlockOwner, int32_t aBlockIndex);

  // Guess the duration until block aBlock will be next used
  TimeDuration PredictNextUse(TimeStamp aNow, int32_t aBlock);
  // Guess the duration until the next incoming data on aStream will be used
  TimeDuration PredictNextUseForIncomingData(MediaCacheStream* aStream);

  // Truncate the file and index array if there are free blocks at the
  // end
  void Truncate();

  // This member is main-thread only. It's used to allocate unique
  // resource IDs to streams.
  int64_t                       mNextResourceID;

  // The monitor protects all the data members here. Also, off-main-thread
  // readers that need to block will Wait() on this monitor. When new
  // data becomes available in the cache, we NotifyAll() on this monitor.
  ReentrantMonitor         mReentrantMonitor;
  // This is only written while on the main thread and the monitor is held.
  // Thus, it can be safely read from the main thread or while holding the monitor.
  nsTArray<MediaCacheStream*> mStreams;
  // The Blocks describing the cache entries.
  nsTArray<Block> mIndex;
  // Writer which performs IO, asynchronously writing cache blocks.
  nsRefPtr<FileBlockCache> mFileCache;
  // The list of free blocks; they are not ordered.
  BlockList       mFreeBlocks;
  // True if an event to run Update() has been queued but not processed
  bool            mUpdateQueued;
#ifdef DEBUG
  bool            mInUpdate;
#endif
};

NS_IMETHODIMP
MediaCacheFlusher::Observe(nsISupports *aSubject, char const *aTopic, char16_t const *aData)
{
  if (strcmp(aTopic, "last-pb-context-exited") == 0) {
    MediaCache::Flush();
  }
  if (strcmp(aTopic, "network-clear-cache-stored-anywhere") == 0) {
    MediaCache::Flush();
  }
  return NS_OK;
}

MediaCacheStream::MediaCacheStream(ChannelMediaResource* aClient)
  : mClient(aClient),
    mInitialized(false),
    mHasHadUpdate(false),
    mClosed(false),
    mDidNotifyDataEnded(false),
    mResourceID(0),
    mIsTransportSeekable(false),
    mCacheSuspended(false),
    mChannelEnded(false),
    mChannelOffset(0),
    mStreamLength(-1),
    mStreamOffset(0),
    mPlaybackBytesPerSecond(10000),
    mPinCount(0),
    mCurrentMode(MODE_PLAYBACK),
    mMetadataInPartialBlockBuffer(false),
    mPartialBlockBuffer(new int64_t[BLOCK_SIZE/sizeof(int64_t)])
{
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
  NS_ASSERTION(entry, "Block not in list");

  if (entry->mNextBlock == aBlock) {
    NS_ASSERTION(entry->mPrevBlock == aBlock, "Linked list inconsistency");
    NS_ASSERTION(mFirstBlock == aBlock, "Linked list inconsistency");
    mFirstBlock = -1;
  } else {
    if (mFirstBlock == aBlock) {
      mFirstBlock = entry->mNextBlock;
    }
    mEntries.GetEntry(entry->mNextBlock)->mPrevBlock = entry->mPrevBlock;
    mEntries.GetEntry(entry->mPrevBlock)->mNextBlock = entry->mNextBlock;
  }
  mEntries.RemoveEntry(aBlock);
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
    mEntries.RemoveEntry(aBlockIndex1);
    // Refresh pointer after hashtable mutation.
    e2 = mEntries.GetEntry(aBlockIndex2);
  }
  if (e2) {
    e2Prev = e2->mPrevBlock;
    e2Next = e2->mNextBlock;
    mEntries.RemoveEntry(aBlockIndex2);
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

nsresult
MediaCache::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  NS_ASSERTION(!mFileCache, "Cache file already open?");

  PRFileDesc* fileDesc = nullptr;
  nsresult rv = NS_OpenAnonymousTemporaryFile(&fileDesc);
  NS_ENSURE_SUCCESS(rv,rv);

  mFileCache = new FileBlockCache();
  rv = mFileCache->Open(fileDesc);
  NS_ENSURE_SUCCESS(rv,rv);

#ifdef PR_LOGGING
  if (!gMediaCacheLog) {
    gMediaCacheLog = PR_NewLogModule("MediaCache");
  }
#endif

  MediaCacheFlusher::Init();

  return NS_OK;
}

void
MediaCache::Flush()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  if (!gMediaCache)
    return;

  gMediaCache->FlushInternal();
}

void
MediaCache::FlushInternal()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  for (uint32_t blockIndex = 0; blockIndex < mIndex.Length(); ++blockIndex) {
    FreeBlock(blockIndex);
  }

  // Truncate file, close it, and reopen
  Truncate();
  NS_ASSERTION(mIndex.Length() == 0, "Blocks leaked?");
  if (mFileCache) {
    mFileCache->Close();
    mFileCache = nullptr;
  }
  Init();
}

void
MediaCache::MaybeShutdown()
{
  NS_ASSERTION(NS_IsMainThread(),
               "MediaCache::MaybeShutdown called on non-main thread");
  if (!gMediaCache->mStreams.IsEmpty()) {
    // Don't shut down yet, streams are still alive
    return;
  }

  // Since we're on the main thread, no-one is going to add a new stream
  // while we shut down.
  // This function is static so we don't have to delete 'this'.
  delete gMediaCache;
  gMediaCache = nullptr;
  NS_IF_RELEASE(gMediaCacheFlusher);
}

static void
InitMediaCache()
{
  if (gMediaCache)
    return;

  gMediaCache = new MediaCache();
  if (!gMediaCache)
    return;

  nsresult rv = gMediaCache->Init();
  if (NS_FAILED(rv)) {
    delete gMediaCache;
    gMediaCache = nullptr;
  }
}

nsresult
MediaCache::ReadCacheFile(int64_t aOffset, void* aData, int32_t aLength,
                            int32_t* aBytes)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  if (!mFileCache)
    return NS_ERROR_FAILURE;

  return mFileCache->Read(aOffset, reinterpret_cast<uint8_t*>(aData), aLength, aBytes);
}

nsresult
MediaCache::ReadCacheFileAllBytes(int64_t aOffset, void* aData, int32_t aLength)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  int64_t offset = aOffset;
  int32_t count = aLength;
  // Cast to char* so we can do byte-wise pointer arithmetic
  char* data = static_cast<char*>(aData);
  while (count > 0) {
    int32_t bytes;
    nsresult rv = ReadCacheFile(offset, data, count, &bytes);
    if (NS_FAILED(rv))
      return rv;
    if (bytes == 0)
      return NS_ERROR_FAILURE;
    count -= bytes;
    data += bytes;
    offset += bytes;
  }
  return NS_OK;
}

static int32_t GetMaxBlocks()
{
  // We look up the cache size every time. This means dynamic changes
  // to the pref are applied.
  // Cache size is in KB
  int32_t cacheSize = Preferences::GetInt("media.cache_size", 500*1024);
  int64_t maxBlocks = static_cast<int64_t>(cacheSize)*1024/MediaCache::BLOCK_SIZE;
  maxBlocks = std::max<int64_t>(maxBlocks, 1);
  return int32_t(std::min<int64_t>(maxBlocks, INT32_MAX));
}

int32_t
MediaCache::FindBlockForIncomingData(TimeStamp aNow,
                                       MediaCacheStream* aStream)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  int32_t blockIndex = FindReusableBlock(aNow, aStream,
      aStream->mChannelOffset/BLOCK_SIZE, INT32_MAX);

  if (blockIndex < 0 || !IsBlockFree(blockIndex)) {
    // The block returned is already allocated.
    // Don't reuse it if a) there's room to expand the cache or
    // b) the data we're going to store in the free block is not higher
    // priority than the data already stored in the free block.
    // The latter can lead us to go over the cache limit a bit.
    if ((mIndex.Length() < uint32_t(GetMaxBlocks()) || blockIndex < 0 ||
         PredictNextUseForIncomingData(aStream) >= PredictNextUse(aNow, blockIndex))) {
      blockIndex = mIndex.Length();
      if (!mIndex.AppendElement())
        return -1;
      mFreeBlocks.AddFirstBlock(blockIndex);
      return blockIndex;
    }
  }

  return blockIndex;
}

bool
MediaCache::BlockIsReusable(int32_t aBlockIndex)
{
  Block* block = &mIndex[aBlockIndex];
  for (uint32_t i = 0; i < block->mOwners.Length(); ++i) {
    MediaCacheStream* stream = block->mOwners[i].mStream;
    if (stream->mPinCount > 0 ||
        stream->mStreamOffset/BLOCK_SIZE == block->mOwners[i].mStreamBlock) {
      return false;
    }
  }
  return true;
}

void
MediaCache::AppendMostReusableBlock(BlockList* aBlockList,
                                      nsTArray<uint32_t>* aResult,
                                      int32_t aBlockIndexLimit)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  int32_t blockIndex = aBlockList->GetLastBlock();
  if (blockIndex < 0)
    return;
  do {
    // Don't consider blocks for pinned streams, or blocks that are
    // beyond the specified limit, or a block that contains a stream's
    // current read position (such a block contains both played data
    // and readahead data)
    if (blockIndex < aBlockIndexLimit && BlockIsReusable(blockIndex)) {
      aResult->AppendElement(blockIndex);
      return;
    }
    blockIndex = aBlockList->GetPrevBlock(blockIndex);
  } while (blockIndex >= 0);
}

int32_t
MediaCache::FindReusableBlock(TimeStamp aNow,
                                MediaCacheStream* aForStream,
                                int32_t aForStreamBlock,
                                int32_t aMaxSearchBlockIndex)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  uint32_t length = std::min(uint32_t(aMaxSearchBlockIndex), mIndex.Length());

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
  nsAutoTArray<uint32_t,8> candidates;
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaCacheStream* stream = mStreams[i];
    if (stream->mPinCount > 0) {
      // No point in even looking at this stream's blocks
      continue;
    }

    AppendMostReusableBlock(&stream->mMetadataBlocks, &candidates, length);
    AppendMostReusableBlock(&stream->mPlayedBlocks, &candidates, length);

    // Don't consider readahead blocks in non-seekable streams. If we
    // remove the block we won't be able to seek back to read it later.
    if (stream->mIsTransportSeekable) {
      AppendMostReusableBlock(&stream->mReadaheadBlocks, &candidates, length);
    }
  }

  TimeDuration latestUse;
  int32_t latestUseBlock = -1;
  for (uint32_t i = 0; i < candidates.Length(); ++i) {
    TimeDuration nextUse = PredictNextUse(aNow, candidates[i]);
    if (nextUse > latestUse) {
      latestUse = nextUse;
      latestUseBlock = candidates[i];
    }
  }

  return latestUseBlock;
}

MediaCache::BlockList*
MediaCache::GetListForBlock(BlockOwner* aBlock)
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
MediaCache::GetBlockOwner(int32_t aBlockIndex, MediaCacheStream* aStream)
{
  Block* block = &mIndex[aBlockIndex];
  for (uint32_t i = 0; i < block->mOwners.Length(); ++i) {
    if (block->mOwners[i].mStream == aStream)
      return &block->mOwners[i];
  }
  return nullptr;
}

void
MediaCache::SwapBlocks(int32_t aBlockIndex1, int32_t aBlockIndex2)
{
  mReentrantMonitor.AssertCurrentThreadIn();

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

  Verify();
}

void
MediaCache::RemoveBlockOwner(int32_t aBlockIndex, MediaCacheStream* aStream)
{
  Block* block = &mIndex[aBlockIndex];
  for (uint32_t i = 0; i < block->mOwners.Length(); ++i) {
    BlockOwner* bo = &block->mOwners[i];
    if (bo->mStream == aStream) {
      GetListForBlock(bo)->RemoveBlock(aBlockIndex);
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
MediaCache::AddBlockOwnerAsReadahead(int32_t aBlockIndex,
                                       MediaCacheStream* aStream,
                                       int32_t aStreamBlockIndex)
{
  Block* block = &mIndex[aBlockIndex];
  if (block->mOwners.IsEmpty()) {
    mFreeBlocks.RemoveBlock(aBlockIndex);
  }
  BlockOwner* bo = block->mOwners.AppendElement();
  bo->mStream = aStream;
  bo->mStreamBlock = aStreamBlockIndex;
  aStream->mBlocks[aStreamBlockIndex] = aBlockIndex;
  bo->mClass = READAHEAD_BLOCK;
  InsertReadaheadBlock(bo, aBlockIndex);
}

void
MediaCache::FreeBlock(int32_t aBlock)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  Block* block = &mIndex[aBlock];
  if (block->mOwners.IsEmpty()) {
    // already free
    return;
  }

  CACHE_LOG(PR_LOG_DEBUG, ("Released block %d", aBlock));

  for (uint32_t i = 0; i < block->mOwners.Length(); ++i) {
    BlockOwner* bo = &block->mOwners[i];
    GetListForBlock(bo)->RemoveBlock(aBlock);
    bo->mStream->mBlocks[bo->mStreamBlock] = -1;
  }
  block->mOwners.Clear();
  mFreeBlocks.AddFirstBlock(aBlock);
  Verify();
}

TimeDuration
MediaCache::PredictNextUse(TimeStamp aNow, int32_t aBlock)
{
  mReentrantMonitor.AssertCurrentThreadIn();
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
MediaCache::PredictNextUseForIncomingData(MediaCacheStream* aStream)
{
  mReentrantMonitor.AssertCurrentThreadIn();

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

enum StreamAction { NONE, SEEK, SEEK_AND_RESUME, RESUME, SUSPEND };

void
MediaCache::Update()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  // The action to use for each stream. We store these so we can make
  // decisions while holding the cache lock but implement those decisions
  // without holding the cache lock, since we need to call out to
  // stream, decoder and element code.
  nsAutoTArray<StreamAction,10> actions;

  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mUpdateQueued = false;
#ifdef DEBUG
    mInUpdate = true;
#endif

    int32_t maxBlocks = GetMaxBlocks();
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
        TimeDuration predictedUse = PredictNextUse(now, blockIndex);
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
        FindReusableBlock(now, block->mOwners[0].mStream,
                          block->mOwners[0].mStreamBlock, maxBlocks);
      if (destinationBlockIndex < 0) {
        // Nowhere to place this overflow block. We won't be able to
        // place any more overflow blocks.
        break;
      }

      if (IsBlockFree(destinationBlockIndex) ||
          PredictNextUse(now, destinationBlockIndex) > latestPredictedUseForOverflow) {
        // Reuse blocks in the main part of the cache that are less useful than
        // the least useful overflow blocks

        nsresult rv = mFileCache->MoveBlock(blockIndex, destinationBlockIndex);

        if (NS_SUCCEEDED(rv)) {
          // We successfully copied the file data.
          CACHE_LOG(PR_LOG_DEBUG, ("Swapping blocks %d and %d (trimming cache)",
                    blockIndex, destinationBlockIndex));
          // Swapping the block metadata here lets us maintain the
          // correct positions in the linked lists
          SwapBlocks(blockIndex, destinationBlockIndex);
          //Free the overflowing block even if the copy failed.
          CACHE_LOG(PR_LOG_DEBUG, ("Released block %d (trimming cache)", blockIndex));
          FreeBlock(blockIndex);
        }
      } else {
        CACHE_LOG(PR_LOG_DEBUG, ("Could not trim cache block %d (destination %d, predicted next use %f, latest predicted use for overflow %f",
                                 blockIndex, destinationBlockIndex,
                                 PredictNextUse(now, destinationBlockIndex).ToSeconds(),
                                 latestPredictedUseForOverflow.ToSeconds()));
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
      int32_t reusableBlock = FindReusableBlock(now, nullptr, 0, maxBlocks);
      if (reusableBlock >= 0) {
        latestNextUse = PredictNextUse(now, reusableBlock);
      }
    }

    for (uint32_t i = 0; i < mStreams.Length(); ++i) {
      actions.AppendElement(NONE);

      MediaCacheStream* stream = mStreams[i];
      if (stream->mClosed)
        continue;

      // Figure out where we should be reading from. It's the first
      // uncached byte after the current mStreamOffset.
      int64_t dataOffset = stream->GetCachedDataEndInternal(stream->mStreamOffset);

      // Compute where we'd actually seek to to read at readOffset
      int64_t desiredOffset = dataOffset;
      if (stream->mIsTransportSeekable) {
        if (desiredOffset > stream->mChannelOffset &&
            desiredOffset <= stream->mChannelOffset + SEEK_VS_READ_THRESHOLD) {
          // Assume it's more efficient to just keep reading up to the
          // desired position instead of trying to seek
          desiredOffset = stream->mChannelOffset;
        }
      } else {
        // We can't seek directly to the desired offset...
        if (stream->mChannelOffset > desiredOffset) {
          // Reading forward won't get us anywhere, we need to go backwards.
          // Seek back to 0 (the client will reopen the stream) and then
          // read forward.
          NS_WARNING("Can't seek backwards, so seeking to 0");
          desiredOffset = 0;
          // Flush cached blocks out, since if this is a live stream
          // the cached data may be completely different next time we
          // read it. We have to assume that live streams don't
          // advertise themselves as being seekable...
          ReleaseStreamBlocks(stream);
        } else {
          // otherwise reading forward is looking good, so just stay where we
          // are and don't trigger a channel seek!
          desiredOffset = stream->mChannelOffset;
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
        CACHE_LOG(PR_LOG_DEBUG, ("Stream %p at end of stream", stream));
        enableReading = !stream->mCacheSuspended &&
          stream->mStreamLength == stream->mChannelOffset;
      } else if (desiredOffset < stream->mStreamOffset) {
        // We're reading to try to catch up to where the current stream
        // reader wants to be. Better not stop.
        CACHE_LOG(PR_LOG_DEBUG, ("Stream %p catching up", stream));
        enableReading = true;
      } else if (desiredOffset < stream->mStreamOffset + BLOCK_SIZE) {
        // The stream reader is waiting for us, or nearly so. Better feed it.
        CACHE_LOG(PR_LOG_DEBUG, ("Stream %p feeding reader", stream));
        enableReading = true;
      } else if (!stream->mIsTransportSeekable &&
                 nonSeekableReadaheadBlockCount >= maxBlocks*NONSEEKABLE_READAHEAD_MAX) {
        // This stream is not seekable and there are already too many blocks
        // being cached for readahead for nonseekable streams (which we can't
        // free). So stop reading ahead now.
        CACHE_LOG(PR_LOG_DEBUG, ("Stream %p throttling non-seekable readahead", stream));
        enableReading = false;
      } else if (mIndex.Length() > uint32_t(maxBlocks)) {
        // We're in the process of bringing the cache size back to the
        // desired limit, so don't bring in more data yet
        CACHE_LOG(PR_LOG_DEBUG, ("Stream %p throttling to reduce cache size", stream));
        enableReading = false;
      } else {
        TimeDuration predictedNewDataUse = PredictNextUseForIncomingData(stream);

        if (stream->mCacheSuspended &&
            predictedNewDataUse.ToMilliseconds() > CACHE_POWERSAVE_WAKEUP_LOW_THRESHOLD_MS) {
          // Don't need data for a while, so don't bother waking up the stream
          CACHE_LOG(PR_LOG_DEBUG, ("Stream %p avoiding wakeup since more data is not needed", stream));
          enableReading = false;
        } else if (freeBlockCount > 0) {
          // Free blocks in the cache, so keep reading
          CACHE_LOG(PR_LOG_DEBUG, ("Stream %p reading since there are free blocks", stream));
          enableReading = true;
        } else if (latestNextUse <= TimeDuration(0)) {
          // No reusable blocks, so can't read anything
          CACHE_LOG(PR_LOG_DEBUG, ("Stream %p throttling due to no reusable blocks", stream));
          enableReading = false;
        } else {
          // Read ahead if the data we expect to read is more valuable than
          // the least valuable block in the main part of the cache
          CACHE_LOG(PR_LOG_DEBUG, ("Stream %p predict next data in %f, current worst block is %f",
                    stream, predictedNewDataUse.ToSeconds(), latestNextUse.ToSeconds()));
          enableReading = predictedNewDataUse < latestNextUse;
        }
      }

      if (enableReading) {
        for (uint32_t j = 0; j < i; ++j) {
          MediaCacheStream* other = mStreams[j];
          if (other->mResourceID == stream->mResourceID &&
              !other->mClient->IsSuspended() &&
              other->mChannelOffset/BLOCK_SIZE == desiredOffset/BLOCK_SIZE) {
            // This block is already going to be read by the other stream.
            // So don't try to read it from this stream as well.
            enableReading = false;
            CACHE_LOG(PR_LOG_DEBUG, ("Stream %p waiting on same block (%lld) from stream %p",
                                     stream, desiredOffset/BLOCK_SIZE, other));
            break;
          }
        }
      }

      if (stream->mChannelOffset != desiredOffset && enableReading) {
        // We need to seek now.
        NS_ASSERTION(stream->mIsTransportSeekable || desiredOffset == 0,
                     "Trying to seek in a non-seekable stream!");
        // Round seek offset down to the start of the block. This is essential
        // because we don't want to think we have part of a block already
        // in mPartialBlockBuffer.
        stream->mChannelOffset = (desiredOffset/BLOCK_SIZE)*BLOCK_SIZE;
        actions[i] = stream->mCacheSuspended ? SEEK_AND_RESUME : SEEK;
      } else if (enableReading && stream->mCacheSuspended) {
        actions[i] = RESUME;
      } else if (!enableReading && !stream->mCacheSuspended) {
        actions[i] = SUSPEND;
      }
    }
#ifdef DEBUG
    mInUpdate = false;
#endif
  }

  // Update the channel state without holding our cache lock. While we're
  // doing this, decoder threads may be running and seeking, reading or changing
  // other cache state. That's OK, they'll trigger new Update events and we'll
  // get back here and revise our decisions. The important thing here is that
  // performing these actions only depends on mChannelOffset and
  // the action, which can only be written by the main thread (i.e., this
  // thread), so we don't have races here.

  // First, update the mCacheSuspended/mCacheEnded flags so that they're all correct
  // when we fire our CacheClient commands below. Those commands can rely on these flags
  // being set correctly for all streams.
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaCacheStream* stream = mStreams[i];
    switch (actions[i]) {
    case SEEK:
	case SEEK_AND_RESUME:
      stream->mCacheSuspended = false;
      stream->mChannelEnded = false;
      break;
    case RESUME:
      stream->mCacheSuspended = false;
      break;
    case SUSPEND:
      stream->mCacheSuspended = true;
      break;
    default:
      break;
    }
    stream->mHasHadUpdate = true;
  }

  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaCacheStream* stream = mStreams[i];
    nsresult rv;
    switch (actions[i]) {
    case SEEK:
	case SEEK_AND_RESUME:
      CACHE_LOG(PR_LOG_DEBUG, ("Stream %p CacheSeek to %lld (resume=%d)", stream,
                (long long)stream->mChannelOffset, actions[i] == SEEK_AND_RESUME));
      rv = stream->mClient->CacheClientSeek(stream->mChannelOffset,
                                            actions[i] == SEEK_AND_RESUME);
      break;
    case RESUME:
      CACHE_LOG(PR_LOG_DEBUG, ("Stream %p Resumed", stream));
      rv = stream->mClient->CacheClientResume();
      break;
    case SUSPEND:
      CACHE_LOG(PR_LOG_DEBUG, ("Stream %p Suspended", stream));
      rv = stream->mClient->CacheClientSuspend();
      break;
    default:
      rv = NS_OK;
      break;
    }

    if (NS_FAILED(rv)) {
      // Close the streams that failed due to error. This will cause all
      // client Read and Seek operations on those streams to fail. Blocked
      // Reads will also be woken up.
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      stream->CloseInternal(mon);
    }
  }
}

class UpdateEvent : public nsRunnable
{
public:
  NS_IMETHOD Run()
  {
    if (gMediaCache) {
      gMediaCache->Update();
    }
    return NS_OK;
  }
};

void
MediaCache::QueueUpdate()
{
  mReentrantMonitor.AssertCurrentThreadIn();

  // Queuing an update while we're in an update raises a high risk of
  // triggering endless events
  NS_ASSERTION(!mInUpdate,
               "Queuing an update while we're in an update");
  if (mUpdateQueued)
    return;
  mUpdateQueued = true;
  nsCOMPtr<nsIRunnable> event = new UpdateEvent();
  NS_DispatchToMainThread(event);
}

#ifdef DEBUG_VERIFY_CACHE
void
MediaCache::Verify()
{
  mReentrantMonitor.AssertCurrentThreadIn();

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
MediaCache::InsertReadaheadBlock(BlockOwner* aBlockOwner,
                                   int32_t aBlockIndex)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  // Find the last block whose stream block is before aBlockIndex's
  // stream block, and insert after it
  MediaCacheStream* stream = aBlockOwner->mStream;
  int32_t readaheadIndex = stream->mReadaheadBlocks.GetLastBlock();
  while (readaheadIndex >= 0) {
    BlockOwner* bo = GetBlockOwner(readaheadIndex, stream);
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
  Verify();
}

void
MediaCache::AllocateAndWriteBlock(MediaCacheStream* aStream, const void* aData,
                                    MediaCacheStream::ReadMode aMode)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  int32_t streamBlockIndex = aStream->mChannelOffset/BLOCK_SIZE;

  // Remove all cached copies of this block
  ResourceStreamIterator iter(aStream->mResourceID);
  while (MediaCacheStream* stream = iter.Next()) {
    while (streamBlockIndex >= int32_t(stream->mBlocks.Length())) {
      stream->mBlocks.AppendElement(-1);
    }
    if (stream->mBlocks[streamBlockIndex] >= 0) {
      // We no longer want to own this block
      int32_t globalBlockIndex = stream->mBlocks[streamBlockIndex];
      CACHE_LOG(PR_LOG_DEBUG, ("Released block %d from stream %p block %d(%lld)",
                globalBlockIndex, stream, streamBlockIndex, (long long)streamBlockIndex*BLOCK_SIZE));
      RemoveBlockOwner(globalBlockIndex, stream);
    }
  }

  // Extend the mBlocks array as necessary

  TimeStamp now = TimeStamp::Now();
  int32_t blockIndex = FindBlockForIncomingData(now, aStream);
  if (blockIndex >= 0) {
    FreeBlock(blockIndex);

    Block* block = &mIndex[blockIndex];
    CACHE_LOG(PR_LOG_DEBUG, ("Allocated block %d to stream %p block %d(%lld)",
              blockIndex, aStream, streamBlockIndex, (long long)streamBlockIndex*BLOCK_SIZE));

    mFreeBlocks.RemoveBlock(blockIndex);

    // Tell each stream using this resource about the new block.
    ResourceStreamIterator iter(aStream->mResourceID);
    while (MediaCacheStream* stream = iter.Next()) {
      BlockOwner* bo = block->mOwners.AppendElement();
      if (!bo)
        return;

      bo->mStream = stream;
      bo->mStreamBlock = streamBlockIndex;
      bo->mLastUseTime = now;
      stream->mBlocks[streamBlockIndex] = blockIndex;
      if (streamBlockIndex*BLOCK_SIZE < stream->mStreamOffset) {
        bo->mClass = aMode == MediaCacheStream::MODE_PLAYBACK
          ? PLAYED_BLOCK : METADATA_BLOCK;
        // This must be the most-recently-used block, since we
        // marked it as used now (which may be slightly bogus, but we'll
        // treat it as used for simplicity).
        GetListForBlock(bo)->AddFirstBlock(blockIndex);
        Verify();
      } else {
        // This may not be the latest readahead block, although it usually
        // will be. We may have to scan for the right place to insert
        // the block in the list.
        bo->mClass = READAHEAD_BLOCK;
        InsertReadaheadBlock(bo, blockIndex);
      }
    }

    nsresult rv = mFileCache->WriteBlock(blockIndex, reinterpret_cast<const uint8_t*>(aData));
    if (NS_FAILED(rv)) {
      CACHE_LOG(PR_LOG_DEBUG, ("Released block %d from stream %p block %d(%lld)",
                blockIndex, aStream, streamBlockIndex, (long long)streamBlockIndex*BLOCK_SIZE));
      FreeBlock(blockIndex);
    }
  }

  // Queue an Update since the cache state has changed (for example
  // we might want to stop loading because the cache is full)
  QueueUpdate();
}

void
MediaCache::OpenStream(MediaCacheStream* aStream)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  CACHE_LOG(PR_LOG_DEBUG, ("Stream %p opened", aStream));
  mStreams.AppendElement(aStream);
  aStream->mResourceID = AllocateResourceID();

  // Queue an update since a new stream has been opened.
  gMediaCache->QueueUpdate();
}

void
MediaCache::ReleaseStream(MediaCacheStream* aStream)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  CACHE_LOG(PR_LOG_DEBUG, ("Stream %p closed", aStream));
  mStreams.RemoveElement(aStream);
}

void
MediaCache::ReleaseStreamBlocks(MediaCacheStream* aStream)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  // XXX scanning the entire stream doesn't seem great, if not much of it
  // is cached, but the only easy alternative is to scan the entire cache
  // which isn't better
  uint32_t length = aStream->mBlocks.Length();
  for (uint32_t i = 0; i < length; ++i) {
    int32_t blockIndex = aStream->mBlocks[i];
    if (blockIndex >= 0) {
      CACHE_LOG(PR_LOG_DEBUG, ("Released block %d from stream %p block %d(%lld)",
                blockIndex, aStream, i, (long long)i*BLOCK_SIZE));
      RemoveBlockOwner(blockIndex, aStream);
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
MediaCache::NoteBlockUsage(MediaCacheStream* aStream, int32_t aBlockIndex,
                             MediaCacheStream::ReadMode aMode,
                             TimeStamp aNow)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  if (aBlockIndex < 0) {
    // this block is not in the cache yet
    return;
  }

  BlockOwner* bo = GetBlockOwner(aBlockIndex, aStream);
  if (!bo) {
    // this block is not in the cache yet
    return;
  }

  // The following check has to be <= because the stream offset has
  // not yet been updated for the data read from this block
  NS_ASSERTION(bo->mStreamBlock*BLOCK_SIZE <= bo->mStream->mStreamOffset,
               "Using a block that's behind the read position?");

  GetListForBlock(bo)->RemoveBlock(aBlockIndex);
  bo->mClass =
    (aMode == MediaCacheStream::MODE_METADATA || bo->mClass == METADATA_BLOCK)
    ? METADATA_BLOCK : PLAYED_BLOCK;
  // Since this is just being used now, it can definitely be at the front
  // of mMetadataBlocks or mPlayedBlocks
  GetListForBlock(bo)->AddFirstBlock(aBlockIndex);
  bo->mLastUseTime = aNow;
  Verify();
}

void
MediaCache::NoteSeek(MediaCacheStream* aStream, int64_t aOldOffset)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  if (aOldOffset < aStream->mStreamOffset) {
    // We seeked forward. Convert blocks from readahead to played.
    // Any readahead block that intersects the seeked-over range must
    // be converted.
    int32_t blockIndex = aOldOffset/BLOCK_SIZE;
    int32_t endIndex =
      std::min<int64_t>((aStream->mStreamOffset + BLOCK_SIZE - 1)/BLOCK_SIZE,
             aStream->mBlocks.Length());
    TimeStamp now = TimeStamp::Now();
    while (blockIndex < endIndex) {
      int32_t cacheBlockIndex = aStream->mBlocks[blockIndex];
      if (cacheBlockIndex >= 0) {
        // Marking the block used may not be exactly what we want but
        // it's simple
        NoteBlockUsage(aStream, cacheBlockIndex, MediaCacheStream::MODE_PLAYBACK,
                       now);
      }
      ++blockIndex;
    }
  } else {
    // We seeked backward. Convert from played to readahead.
    // Any played block that is entirely after the start of the seeked-over
    // range must be converted.
    int32_t blockIndex =
      (aStream->mStreamOffset + BLOCK_SIZE - 1)/BLOCK_SIZE;
    int32_t endIndex =
      std::min<int64_t>((aOldOffset + BLOCK_SIZE - 1)/BLOCK_SIZE,
             aStream->mBlocks.Length());
    while (blockIndex < endIndex) {
      int32_t cacheBlockIndex = aStream->mBlocks[endIndex - 1];
      if (cacheBlockIndex >= 0) {
        BlockOwner* bo = GetBlockOwner(cacheBlockIndex, aStream);
        NS_ASSERTION(bo, "Stream doesn't own its blocks?");
        if (bo->mClass == PLAYED_BLOCK) {
          aStream->mPlayedBlocks.RemoveBlock(cacheBlockIndex);
          bo->mClass = READAHEAD_BLOCK;
          // Adding this as the first block is sure to be OK since
          // this must currently be the earliest readahead block
          // (that's why we're proceeding backwards from the end of
          // the seeked range to the start)
          aStream->mReadaheadBlocks.AddFirstBlock(cacheBlockIndex);
          Verify();
        }
      }
      --endIndex;
    }
  }
}

void
MediaCacheStream::NotifyDataLength(int64_t aLength)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  mStreamLength = aLength;
}

void
MediaCacheStream::NotifyDataStarted(int64_t aOffset)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  NS_WARN_IF_FALSE(aOffset == mChannelOffset,
                   "Server is giving us unexpected offset");
  mChannelOffset = aOffset;
  if (mStreamLength >= 0) {
    // If we started reading at a certain offset, then for sure
    // the stream is at least that long.
    mStreamLength = std::max(mStreamLength, mChannelOffset);
  }
}

bool
MediaCacheStream::UpdatePrincipal(nsIPrincipal* aPrincipal)
{
  return nsContentUtils::CombineResourcePrincipals(&mPrincipal, aPrincipal);
}

void
MediaCacheStream::NotifyDataReceived(int64_t aSize, const char* aData,
    nsIPrincipal* aPrincipal)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  // Update principals before putting the data in the cache. This is important,
  // we want to make sure all principals are updated before any consumer
  // can see the new data.
  // We do this without holding the cache monitor, in case the client wants
  // to do something that takes a lock.
  {
    MediaCache::ResourceStreamIterator iter(mResourceID);
    while (MediaCacheStream* stream = iter.Next()) {
      if (stream->UpdatePrincipal(aPrincipal)) {
        stream->mClient->CacheClientNotifyPrincipalChanged();
      }
    }
  }

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  int64_t size = aSize;
  const char* data = aData;

  CACHE_LOG(PR_LOG_DEBUG, ("Stream %p DataReceived at %lld count=%lld",
            this, (long long)mChannelOffset, (long long)aSize));

  // We process the data one block (or part of a block) at a time
  while (size > 0) {
    uint32_t blockIndex = mChannelOffset/BLOCK_SIZE;
    int32_t blockOffset = int32_t(mChannelOffset - blockIndex*BLOCK_SIZE);
    int32_t chunkSize = std::min<int64_t>(BLOCK_SIZE - blockOffset, size);

    // This gets set to something non-null if we have a whole block
    // of data to write to the cache
    const char* blockDataToStore = nullptr;
    ReadMode mode = MODE_PLAYBACK;
    if (blockOffset == 0 && chunkSize == BLOCK_SIZE) {
      // We received a whole block, so avoid a useless copy through
      // mPartialBlockBuffer
      blockDataToStore = data;
    } else {
      if (blockOffset == 0) {
        // We've just started filling this buffer so now is a good time
        // to clear this flag.
        mMetadataInPartialBlockBuffer = false;
      }
      memcpy(reinterpret_cast<char*>(mPartialBlockBuffer.get()) + blockOffset,
             data, chunkSize);

      if (blockOffset + chunkSize == BLOCK_SIZE) {
        // We completed a block, so lets write it out.
        blockDataToStore = reinterpret_cast<char*>(mPartialBlockBuffer.get());
        if (mMetadataInPartialBlockBuffer) {
          mode = MODE_METADATA;
        }
      }
    }

    if (blockDataToStore) {
      gMediaCache->AllocateAndWriteBlock(this, blockDataToStore, mode);
    }

    mChannelOffset += chunkSize;
    size -= chunkSize;
    data += chunkSize;
  }

  MediaCache::ResourceStreamIterator iter(mResourceID);
  while (MediaCacheStream* stream = iter.Next()) {
    if (stream->mStreamLength >= 0) {
      // The stream is at least as long as what we've read
      stream->mStreamLength = std::max(stream->mStreamLength, mChannelOffset);
    }
    stream->mClient->CacheClientNotifyDataReceived();
  }

  // Notify in case there's a waiting reader
  // XXX it would be fairly easy to optimize things a lot more to
  // avoid waking up reader threads unnecessarily
  mon.NotifyAll();
}

void
MediaCacheStream::FlushPartialBlockInternal(bool aNotifyAll)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());

  int32_t blockOffset = int32_t(mChannelOffset%BLOCK_SIZE);
  if (blockOffset > 0) {
    CACHE_LOG(PR_LOG_DEBUG,
              ("Stream %p writing partial block: [%d] bytes; "
               "mStreamOffset [%lld] mChannelOffset[%lld] mStreamLength [%lld] "
               "notifying: [%s]",
               this, blockOffset, mStreamOffset, mChannelOffset, mStreamLength,
               aNotifyAll ? "yes" : "no"));

    // Write back the partial block
    memset(reinterpret_cast<char*>(mPartialBlockBuffer.get()) + blockOffset, 0,
           BLOCK_SIZE - blockOffset);
    gMediaCache->AllocateAndWriteBlock(this, mPartialBlockBuffer,
        mMetadataInPartialBlockBuffer ? MODE_METADATA : MODE_PLAYBACK);
    if (aNotifyAll) {
      // Wake up readers who may be waiting for this data
      mon.NotifyAll();
    }
  }
}

void
MediaCacheStream::FlushPartialBlock()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());

  // Write the current partial block to memory.
  // Note: This writes a full block, so if data is not at the end of the
  // stream, the decoder must subsequently choose correct start and end offsets
  // for reading/seeking.
  FlushPartialBlockInternal(false);

  gMediaCache->QueueUpdate();
}

void
MediaCacheStream::NotifyDataEnded(nsresult aStatus)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());

  if (NS_FAILED(aStatus)) {
    // Disconnect from other streams sharing our resource, since they
    // should continue trying to load. Our load might have been deliberately
    // canceled and that shouldn't affect other streams.
    mResourceID = gMediaCache->AllocateResourceID();
  }

  FlushPartialBlockInternal(true);

  if (!mDidNotifyDataEnded) {
    MediaCache::ResourceStreamIterator iter(mResourceID);
    while (MediaCacheStream* stream = iter.Next()) {
      if (NS_SUCCEEDED(aStatus)) {
        // We read the whole stream, so remember the true length
        stream->mStreamLength = mChannelOffset;
      }
      NS_ASSERTION(!stream->mDidNotifyDataEnded, "Stream already ended!");
      stream->mDidNotifyDataEnded = true;
      stream->mNotifyDataEndedStatus = aStatus;
      stream->mClient->CacheClientNotifyDataEnded(aStatus);
    }
  }

  mChannelEnded = true;
  gMediaCache->QueueUpdate();
}

MediaCacheStream::~MediaCacheStream()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  NS_ASSERTION(!mPinCount, "Unbalanced Pin");

  if (gMediaCache) {
    NS_ASSERTION(mClosed, "Stream was not closed");
    gMediaCache->ReleaseStream(this);
    MediaCache::MaybeShutdown();
  }
}

void
MediaCacheStream::SetTransportSeekable(bool aIsTransportSeekable)
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  NS_ASSERTION(mIsTransportSeekable || aIsTransportSeekable ||
               mChannelOffset == 0, "channel offset must be zero when we become non-seekable");
  mIsTransportSeekable = aIsTransportSeekable;
  // Queue an Update since we may change our strategy for dealing
  // with this stream
  gMediaCache->QueueUpdate();
}

bool
MediaCacheStream::IsTransportSeekable()
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  return mIsTransportSeekable;
}

bool
MediaCacheStream::AreAllStreamsForResourceSuspended()
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  MediaCache::ResourceStreamIterator iter(mResourceID);
  // Look for a stream that's able to read the data we need
  int64_t dataOffset = -1;
  while (MediaCacheStream* stream = iter.Next()) {
    if (stream->mCacheSuspended || stream->mChannelEnded || stream->mClosed) {
      continue;
    }
    if (dataOffset < 0) {
      dataOffset = GetCachedDataEndInternal(mStreamOffset);
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
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  CloseInternal(mon);
  // Queue an Update since we may have created more free space. Don't do
  // it from CloseInternal since that gets called by Update() itself
  // sometimes, and we try to not to queue updates from Update().
  gMediaCache->QueueUpdate();
}

void
MediaCacheStream::EnsureCacheUpdate()
{
  if (mHasHadUpdate)
    return;
  gMediaCache->Update();
}

void
MediaCacheStream::CloseInternal(ReentrantMonitorAutoEnter& aReentrantMonitor)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  if (mClosed)
    return;
  mClosed = true;
  gMediaCache->ReleaseStreamBlocks(this);
  // Wake up any blocked readers
  aReentrantMonitor.NotifyAll();
}

void
MediaCacheStream::Pin()
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  ++mPinCount;
  // Queue an Update since we may no longer want to read more into the
  // cache, if this stream's block have become non-evictable
  gMediaCache->QueueUpdate();
}

void
MediaCacheStream::Unpin()
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  NS_ASSERTION(mPinCount > 0, "Unbalanced Unpin");
  --mPinCount;
  // Queue an Update since we may be able to read more into the
  // cache, if this stream's block have become evictable
  gMediaCache->QueueUpdate();
}

int64_t
MediaCacheStream::GetLength()
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  return mStreamLength;
}

int64_t
MediaCacheStream::GetNextCachedData(int64_t aOffset)
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  return GetNextCachedDataInternal(aOffset);
}

int64_t
MediaCacheStream::GetCachedDataEnd(int64_t aOffset)
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  return GetCachedDataEndInternal(aOffset);
}

bool
MediaCacheStream::IsDataCachedToEndOfStream(int64_t aOffset)
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  if (mStreamLength < 0)
    return false;
  return GetCachedDataEndInternal(aOffset) >= mStreamLength;
}

int64_t
MediaCacheStream::GetCachedDataEndInternal(int64_t aOffset)
{
  gMediaCache->GetReentrantMonitor().AssertCurrentThreadIn();
  uint32_t startBlockIndex = aOffset/BLOCK_SIZE;
  uint32_t blockIndex = startBlockIndex;
  while (blockIndex < mBlocks.Length() && mBlocks[blockIndex] != -1) {
    ++blockIndex;
  }
  int64_t result = blockIndex*BLOCK_SIZE;
  if (blockIndex == mChannelOffset/BLOCK_SIZE) {
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
MediaCacheStream::GetNextCachedDataInternal(int64_t aOffset)
{
  gMediaCache->GetReentrantMonitor().AssertCurrentThreadIn();
  if (aOffset == mStreamLength)
    return -1;

  uint32_t startBlockIndex = aOffset/BLOCK_SIZE;
  uint32_t channelBlockIndex = mChannelOffset/BLOCK_SIZE;

  if (startBlockIndex == channelBlockIndex &&
      aOffset < mChannelOffset) {
    // The block containing mChannelOffset is partially read, but not
    // yet committed to the main cache. aOffset lies in the partially
    // read portion, thus it is effectively cached.
    return aOffset;
  }

  if (startBlockIndex >= mBlocks.Length())
    return -1;

  // Is the current block cached?
  if (mBlocks[startBlockIndex] != -1)
    return aOffset;

  // Count the number of uncached blocks
  bool hasPartialBlock = (mChannelOffset % BLOCK_SIZE) != 0;
  uint32_t blockIndex = startBlockIndex + 1;
  while (true) {
    if ((hasPartialBlock && blockIndex == channelBlockIndex) ||
        (blockIndex < mBlocks.Length() && mBlocks[blockIndex] != -1)) {
      // We at the incoming channel block, which has has data in it,
      // or are we at a cached block. Return index of block start.
      return blockIndex * BLOCK_SIZE;
    }

    // No more cached blocks?
    if (blockIndex >= mBlocks.Length())
      return -1;

    ++blockIndex;
  }

  NS_NOTREACHED("Should return in loop");
  return -1;
}

void
MediaCacheStream::SetReadMode(ReadMode aMode)
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  if (aMode == mCurrentMode)
    return;
  mCurrentMode = aMode;
  gMediaCache->QueueUpdate();
}

void
MediaCacheStream::SetPlaybackRate(uint32_t aBytesPerSecond)
{
  NS_ASSERTION(aBytesPerSecond > 0, "Zero playback rate not allowed");
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  if (aBytesPerSecond == mPlaybackBytesPerSecond)
    return;
  mPlaybackBytesPerSecond = aBytesPerSecond;
  gMediaCache->QueueUpdate();
}

nsresult
MediaCacheStream::Seek(int32_t aWhence, int64_t aOffset)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  if (mClosed)
    return NS_ERROR_FAILURE;

  int64_t oldOffset = mStreamOffset;
  switch (aWhence) {
  case PR_SEEK_END:
    if (mStreamLength < 0)
      return NS_ERROR_FAILURE;
    mStreamOffset = mStreamLength + aOffset;
    break;
  case PR_SEEK_CUR:
    mStreamOffset += aOffset;
    break;
  case PR_SEEK_SET:
    mStreamOffset = aOffset;
    break;
  default:
    NS_ERROR("Unknown whence");
    return NS_ERROR_FAILURE;
  }

  CACHE_LOG(PR_LOG_DEBUG, ("Stream %p Seek to %lld", this, (long long)mStreamOffset));
  gMediaCache->NoteSeek(this, oldOffset);

  gMediaCache->QueueUpdate();
  return NS_OK;
}

int64_t
MediaCacheStream::Tell()
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  return mStreamOffset;
}

nsresult
MediaCacheStream::Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  if (mClosed)
    return NS_ERROR_FAILURE;

  uint32_t count = 0;
  // Read one block (or part of a block) at a time
  while (count < aCount) {
    uint32_t streamBlock = uint32_t(mStreamOffset/BLOCK_SIZE);
    uint32_t offsetInStreamBlock =
      uint32_t(mStreamOffset - streamBlock*BLOCK_SIZE);
    int64_t size = std::min(aCount - count, BLOCK_SIZE - offsetInStreamBlock);

    if (mStreamLength >= 0) {
      // Don't try to read beyond the end of the stream
      int64_t bytesRemaining = mStreamLength - mStreamOffset;
      if (bytesRemaining <= 0) {
        // Get out of here and return NS_OK
        break;
      }
      size = std::min(size, bytesRemaining);
      // Clamp size until 64-bit file size issues (bug 500784) are fixed.
      size = std::min(size, int64_t(INT32_MAX));
    }

    int32_t bytes;
    int32_t cacheBlock = streamBlock < mBlocks.Length() ? mBlocks[streamBlock] : -1;
    if (cacheBlock < 0) {
      // We don't have a complete cached block here.

      if (count > 0) {
        // Some data has been read, so return what we've got instead of
        // blocking or trying to find a stream with a partial block.
        break;
      }

      // See if the data is available in the partial cache block of any
      // stream reading this resource. We need to do this in case there is
      // another stream with this resource that has all the data to the end of
      // the stream but the data doesn't end on a block boundary.
      MediaCacheStream* streamWithPartialBlock = nullptr;
      MediaCache::ResourceStreamIterator iter(mResourceID);
      while (MediaCacheStream* stream = iter.Next()) {
        if (uint32_t(stream->mChannelOffset/BLOCK_SIZE) == streamBlock &&
            mStreamOffset < stream->mChannelOffset) {
          streamWithPartialBlock = stream;
          break;
        }
      }
      if (streamWithPartialBlock) {
        // We can just use the data in mPartialBlockBuffer. In fact we should
        // use it rather than waiting for the block to fill and land in
        // the cache.
        bytes = std::min<int64_t>(size, streamWithPartialBlock->mChannelOffset - mStreamOffset);
        memcpy(aBuffer,
          reinterpret_cast<char*>(streamWithPartialBlock->mPartialBlockBuffer.get()) + offsetInStreamBlock, bytes);
        if (mCurrentMode == MODE_METADATA) {
          streamWithPartialBlock->mMetadataInPartialBlockBuffer = true;
        }
        mStreamOffset += bytes;
        count = bytes;
        break;
      }

      // No data has been read yet, so block
      mon.Wait();
      if (mClosed) {
        // We may have successfully read some data, but let's just throw
        // that out.
        return NS_ERROR_FAILURE;
      }
      continue;
    }

    gMediaCache->NoteBlockUsage(this, cacheBlock, mCurrentMode, TimeStamp::Now());

    int64_t offset = cacheBlock*BLOCK_SIZE + offsetInStreamBlock;
    NS_ABORT_IF_FALSE(size >= 0 && size <= INT32_MAX, "Size out of range.");
    nsresult rv = gMediaCache->ReadCacheFile(offset, aBuffer + count, int32_t(size), &bytes);
    if (NS_FAILED(rv)) {
      if (count == 0)
        return rv;
      // If we did successfully read some data, may as well return it
      break;
    }
    mStreamOffset += bytes;
    count += bytes;
  }

  if (count > 0) {
    // Some data was read, so queue an update since block priorities may
    // have changed
    gMediaCache->QueueUpdate();
  }
  CACHE_LOG(PR_LOG_DEBUG,
            ("Stream %p Read at %lld count=%d", this, (long long)(mStreamOffset-count), count));
  *aBytes = count;
  return NS_OK;
}

nsresult
MediaCacheStream::ReadAt(int64_t aOffset, char* aBuffer,
                         uint32_t aCount, uint32_t* aBytes)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  nsresult rv = Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
  if (NS_FAILED(rv)) return rv;
  return Read(aBuffer, aCount, aBytes);
}

nsresult
MediaCacheStream::ReadFromCache(char* aBuffer,
                                  int64_t aOffset,
                                  int64_t aCount)
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  if (mClosed)
    return NS_ERROR_FAILURE;

  // Read one block (or part of a block) at a time
  uint32_t count = 0;
  int64_t streamOffset = aOffset;
  while (count < aCount) {
    uint32_t streamBlock = uint32_t(streamOffset/BLOCK_SIZE);
    uint32_t offsetInStreamBlock =
      uint32_t(streamOffset - streamBlock*BLOCK_SIZE);
    int64_t size = std::min<int64_t>(aCount - count, BLOCK_SIZE - offsetInStreamBlock);

    if (mStreamLength >= 0) {
      // Don't try to read beyond the end of the stream
      int64_t bytesRemaining = mStreamLength - streamOffset;
      if (bytesRemaining <= 0) {
        return NS_ERROR_FAILURE;
      }
      size = std::min(size, bytesRemaining);
      // Clamp size until 64-bit file size issues (bug 500784) are fixed.
      size = std::min(size, int64_t(INT32_MAX));
    }

    int32_t bytes;
    uint32_t channelBlock = uint32_t(mChannelOffset/BLOCK_SIZE);
    int32_t cacheBlock = streamBlock < mBlocks.Length() ? mBlocks[streamBlock] : -1;
    if (channelBlock == streamBlock && streamOffset < mChannelOffset) {
      // We can just use the data in mPartialBlockBuffer. In fact we should
      // use it rather than waiting for the block to fill and land in
      // the cache.
      bytes = std::min<int64_t>(size, mChannelOffset - streamOffset);
      memcpy(aBuffer + count,
        reinterpret_cast<char*>(mPartialBlockBuffer.get()) + offsetInStreamBlock, bytes);
    } else {
      if (cacheBlock < 0) {
        // We expect all blocks to be cached! Fail!
        return NS_ERROR_FAILURE;
      }
      int64_t offset = cacheBlock*BLOCK_SIZE + offsetInStreamBlock;
      NS_ABORT_IF_FALSE(size >= 0 && size <= INT32_MAX, "Size out of range.");
      nsresult rv = gMediaCache->ReadCacheFile(offset, aBuffer + count, int32_t(size), &bytes);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    streamOffset += bytes;
    count += bytes;
  }

  return NS_OK;
}

nsresult
MediaCacheStream::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  if (mInitialized)
    return NS_OK;

  InitMediaCache();
  if (!gMediaCache)
    return NS_ERROR_FAILURE;
  gMediaCache->OpenStream(this);
  mInitialized = true;
  return NS_OK;
}

nsresult
MediaCacheStream::InitAsClone(MediaCacheStream* aOriginal)
{
  if (!aOriginal->IsAvailableForSharing())
    return NS_ERROR_FAILURE;

  if (mInitialized)
    return NS_OK;

  nsresult rv = Init();
  if (NS_FAILED(rv))
    return rv;
  mResourceID = aOriginal->mResourceID;

  // Grab cache blocks from aOriginal as readahead blocks for our stream
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());

  mPrincipal = aOriginal->mPrincipal;
  mStreamLength = aOriginal->mStreamLength;
  mIsTransportSeekable = aOriginal->mIsTransportSeekable;

  // Cloned streams are initially suspended, since there is no channel open
  // initially for a clone.
  mCacheSuspended = true;
  mChannelEnded = true;

  if (aOriginal->mDidNotifyDataEnded) {
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
    gMediaCache->AddBlockOwnerAsReadahead(cacheBlockIndex, this, i);
  }

  return NS_OK;
}

nsresult MediaCacheStream::GetCachedRanges(nsTArray<MediaByteRange>& aRanges)
{
  // Take the monitor, so that the cached data ranges can't grow while we're
  // trying to loop over them.
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());

  // We must be pinned while running this, otherwise the cached data ranges may
  // shrink while we're trying to loop over them.
  NS_ASSERTION(mPinCount > 0, "Must be pinned");

  int64_t startOffset = GetNextCachedData(0);
  while (startOffset >= 0) {
    int64_t endOffset = GetCachedDataEnd(startOffset);
    NS_ASSERTION(startOffset < endOffset, "Buffered range must end after its start");
    // Bytes [startOffset..endOffset] are cached.
    aRanges.AppendElement(MediaByteRange(startOffset, endOffset));
    startOffset = GetNextCachedData(endOffset);
    NS_ASSERTION(startOffset == -1 || startOffset > endOffset,
      "Must have advanced to start of next range, or hit end of stream");
  }
  return NS_OK;
}

} // namespace mozilla

