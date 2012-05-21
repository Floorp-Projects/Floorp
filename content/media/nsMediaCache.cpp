/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ReentrantMonitor.h"
#include "mozilla/XPCOM.h"

#include "nsMediaCache.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"
#include "nsNetUtil.h"
#include "prio.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "MediaResource.h"
#include "nsMathUtils.h"
#include "prlog.h"
#include "mozilla/Preferences.h"
#include "FileBlockCache.h"

using namespace mozilla;

#ifdef PR_LOGGING
PRLogModuleInfo* gMediaCacheLog;
#define LOG(type, msg) PR_LOG(gMediaCacheLog, type, msg)
#else
#define LOG(type, msg)
#endif

// Readahead blocks for non-seekable streams will be limited to this
// fraction of the cache space. We don't normally evict such blocks
// because replacing them requires a seek, but we need to make sure
// they don't monopolize the cache.
static const double NONSEEKABLE_READAHEAD_MAX = 0.5;

// Assume that any replaying or backward seeking will happen
// this far in the future (in seconds). This is a random guess/estimate
// penalty to account for the possibility that we might not replay at
// all.
static const PRUint32 REPLAY_DELAY = 30;

// When looking for a reusable block, scan forward this many blocks
// from the desired "best" block location to look for free blocks,
// before we resort to scanning the whole cache. The idea is to try to
// store runs of stream blocks close-to-consecutively in the cache if we
// can.
static const PRUint32 FREE_BLOCK_SCAN_LIMIT = 16;

#ifdef DEBUG
// Turn this on to do very expensive cache state validation
// #define DEBUG_VERIFY_CACHE
#endif

// There is at most one media cache (although that could quite easily be
// relaxed if we wanted to manage multiple caches with independent
// size limits).
static nsMediaCache* gMediaCache;

class nsMediaCacheFlusher : public nsIObserver,
                            public nsSupportsWeakReference {
  nsMediaCacheFlusher() {}
  ~nsMediaCacheFlusher();
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static void Init();
};

static nsMediaCacheFlusher* gMediaCacheFlusher;

NS_IMPL_ISUPPORTS2(nsMediaCacheFlusher, nsIObserver, nsISupportsWeakReference)

nsMediaCacheFlusher::~nsMediaCacheFlusher()
{
  gMediaCacheFlusher = nsnull;
}

void nsMediaCacheFlusher::Init()
{
  if (gMediaCacheFlusher) {
    return;
  }

  gMediaCacheFlusher = new nsMediaCacheFlusher();
  NS_ADDREF(gMediaCacheFlusher);

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(gMediaCacheFlusher, "last-pb-context-exited", true);
  }
}

class nsMediaCache {
public:
  friend class nsMediaCacheStream::BlockList;
  typedef nsMediaCacheStream::BlockList BlockList;
  enum {
    BLOCK_SIZE = nsMediaCacheStream::BLOCK_SIZE
  };

  nsMediaCache() : mNextResourceID(1),
    mReentrantMonitor("nsMediaCache.mReentrantMonitor"),
    mUpdateQueued(false)
#ifdef DEBUG
    , mInUpdate(false)
#endif
  {
    MOZ_COUNT_CTOR(nsMediaCache);
  }
  ~nsMediaCache() {
    NS_ASSERTION(mStreams.IsEmpty(), "Stream(s) still open!");
    Truncate();
    NS_ASSERTION(mIndex.Length() == 0, "Blocks leaked?");
    if (mFileCache) {
      mFileCache->Close();
      mFileCache = nsnull;
    }
    MOZ_COUNT_DTOR(nsMediaCache);
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
  nsresult ReadCacheFile(PRInt64 aOffset, void* aData, PRInt32 aLength,
                         PRInt32* aBytes);
  // This will fail if all aLength bytes are not read
  nsresult ReadCacheFileAllBytes(PRInt64 aOffset, void* aData, PRInt32 aLength);

  PRInt64 AllocateResourceID()
  {
    mReentrantMonitor.AssertCurrentThreadIn();
    return mNextResourceID++;
  }

  // mReentrantMonitor must be held, called on main thread.
  // These methods are used by the stream to set up and tear down streams,
  // and to handle reads and writes.
  // Add aStream to the list of streams.
  void OpenStream(nsMediaCacheStream* aStream);
  // Remove aStream from the list of streams.
  void ReleaseStream(nsMediaCacheStream* aStream);
  // Free all blocks belonging to aStream.
  void ReleaseStreamBlocks(nsMediaCacheStream* aStream);
  // Find a cache entry for this data, and write the data into it
  void AllocateAndWriteBlock(nsMediaCacheStream* aStream, const void* aData,
                             nsMediaCacheStream::ReadMode aMode);

  // mReentrantMonitor must be held; can be called on any thread
  // Notify the cache that a seek has been requested. Some blocks may
  // need to change their class between PLAYED_BLOCK and READAHEAD_BLOCK.
  // This does not trigger channel seeks directly, the next Update()
  // will do that if necessary. The caller will call QueueUpdate().
  void NoteSeek(nsMediaCacheStream* aStream, PRInt64 aOldOffset);
  // Notify the cache that a block has been read from. This is used
  // to update last-use times. The block may not actually have a
  // cache entry yet since Read can read data from a stream's
  // in-memory mPartialBlockBuffer while the block is only partly full,
  // and thus hasn't yet been committed to the cache. The caller will
  // call QueueUpdate().
  void NoteBlockUsage(nsMediaCacheStream* aStream, PRInt32 aBlockIndex,
                      nsMediaCacheStream::ReadMode aMode, TimeStamp aNow);
  // Mark aStream as having the block, adding it as an owner.
  void AddBlockOwnerAsReadahead(PRInt32 aBlockIndex, nsMediaCacheStream* aStream,
                                PRInt32 aStreamBlockIndex);

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
    ResourceStreamIterator(PRInt64 aResourceID) :
      mResourceID(aResourceID), mNext(0) {}
    nsMediaCacheStream* Next()
    {
      while (mNext < gMediaCache->mStreams.Length()) {
        nsMediaCacheStream* stream = gMediaCache->mStreams[mNext];
        ++mNext;
        if (stream->GetResourceID() == mResourceID && !stream->IsClosed())
          return stream;
      }
      return nsnull;
    }
  private:
    PRInt64  mResourceID;
    PRUint32 mNext;
  };

protected:
  // Find a free or reusable block and return its index. If there are no
  // free blocks and no reusable blocks, add a new block to the cache
  // and return it. Can return -1 on OOM.
  PRInt32 FindBlockForIncomingData(TimeStamp aNow, nsMediaCacheStream* aStream);
  // Find a reusable block --- a free block, if there is one, otherwise
  // the reusable block with the latest predicted-next-use, or -1 if
  // there aren't any freeable blocks. Only block indices less than
  // aMaxSearchBlockIndex are considered. If aForStream is non-null,
  // then aForStream and aForStreamBlock indicate what media data will
  // be placed; FindReusableBlock will favour returning free blocks
  // near other blocks for that point in the stream.
  PRInt32 FindReusableBlock(TimeStamp aNow,
                            nsMediaCacheStream* aForStream,
                            PRInt32 aForStreamBlock,
                            PRInt32 aMaxSearchBlockIndex);
  bool BlockIsReusable(PRInt32 aBlockIndex);
  // Given a list of blocks sorted with the most reusable blocks at the
  // end, find the last block whose stream is not pinned (if any)
  // and whose cache entry index is less than aBlockIndexLimit
  // and append it to aResult.
  void AppendMostReusableBlock(BlockList* aBlockList,
                               nsTArray<PRUint32>* aResult,
                               PRInt32 aBlockIndexLimit);

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
    BlockOwner() : mStream(nsnull), mClass(READAHEAD_BLOCK) {}

    // The stream that owns this block, or null if the block is free.
    nsMediaCacheStream* mStream;
    // The block index in the stream. Valid only if mStream is non-null.
    PRUint32            mStreamBlock;
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
  BlockOwner* GetBlockOwner(PRInt32 aBlockIndex, nsMediaCacheStream* aStream);
  // Returns true iff the block is free
  bool IsBlockFree(PRInt32 aBlockIndex)
  { return mIndex[aBlockIndex].mOwners.IsEmpty(); }
  // Add the block to the free list and mark its streams as not having
  // the block in cache
  void FreeBlock(PRInt32 aBlock);
  // Mark aStream as not having the block, removing it as an owner. If
  // the block has no more owners it's added to the free list.
  void RemoveBlockOwner(PRInt32 aBlockIndex, nsMediaCacheStream* aStream);
  // Swap all metadata associated with the two blocks. The caller
  // is responsible for swapping up any cache file state.
  void SwapBlocks(PRInt32 aBlockIndex1, PRInt32 aBlockIndex2);
  // Insert the block into the readahead block list for the stream
  // at the right point in the list.
  void InsertReadaheadBlock(BlockOwner* aBlockOwner, PRInt32 aBlockIndex);

  // Guess the duration until block aBlock will be next used
  TimeDuration PredictNextUse(TimeStamp aNow, PRInt32 aBlock);
  // Guess the duration until the next incoming data on aStream will be used
  TimeDuration PredictNextUseForIncomingData(nsMediaCacheStream* aStream);

  // Truncate the file and index array if there are free blocks at the
  // end
  void Truncate();

  // This member is main-thread only. It's used to allocate unique
  // resource IDs to streams.
  PRInt64                       mNextResourceID;

  // The monitor protects all the data members here. Also, off-main-thread
  // readers that need to block will Wait() on this monitor. When new
  // data becomes available in the cache, we NotifyAll() on this monitor.
  ReentrantMonitor         mReentrantMonitor;
  // This is only written while on the main thread and the monitor is held.
  // Thus, it can be safely read from the main thread or while holding the monitor.
  nsTArray<nsMediaCacheStream*> mStreams;
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
nsMediaCacheFlusher::Observe(nsISupports *aSubject, char const *aTopic, PRUnichar const *aData)
{
  if (strcmp(aTopic, "last-pb-context-exited") == 0) {
    nsMediaCache::Flush();
  }
  return NS_OK;
}

void nsMediaCacheStream::BlockList::AddFirstBlock(PRInt32 aBlock)
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

void nsMediaCacheStream::BlockList::AddAfter(PRInt32 aBlock, PRInt32 aBefore)
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

void nsMediaCacheStream::BlockList::RemoveBlock(PRInt32 aBlock)
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

PRInt32 nsMediaCacheStream::BlockList::GetLastBlock() const
{
  if (mFirstBlock < 0)
    return -1;
  return mEntries.GetEntry(mFirstBlock)->mPrevBlock;
}

PRInt32 nsMediaCacheStream::BlockList::GetNextBlock(PRInt32 aBlock) const
{
  PRInt32 block = mEntries.GetEntry(aBlock)->mNextBlock;
  if (block == mFirstBlock)
    return -1;
  return block;
}

PRInt32 nsMediaCacheStream::BlockList::GetPrevBlock(PRInt32 aBlock) const
{
  if (aBlock == mFirstBlock)
    return -1;
  return mEntries.GetEntry(aBlock)->mPrevBlock;
}

#ifdef DEBUG
void nsMediaCacheStream::BlockList::Verify()
{
  PRInt32 count = 0;
  if (mFirstBlock >= 0) {
    PRInt32 block = mFirstBlock;
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

static void UpdateSwappedBlockIndex(PRInt32* aBlockIndex,
    PRInt32 aBlock1Index, PRInt32 aBlock2Index)
{
  PRInt32 index = *aBlockIndex;
  if (index == aBlock1Index) {
    *aBlockIndex = aBlock2Index;
  } else if (index == aBlock2Index) {
    *aBlockIndex = aBlock1Index;
  }
}

void
nsMediaCacheStream::BlockList::NotifyBlockSwapped(PRInt32 aBlockIndex1,
                                                  PRInt32 aBlockIndex2)
{
  Entry* e1 = mEntries.GetEntry(aBlockIndex1);
  Entry* e2 = mEntries.GetEntry(aBlockIndex2);
  PRInt32 e1Prev = -1, e1Next = -1, e2Prev = -1, e2Next = -1;

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
nsMediaCache::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  NS_ASSERTION(!mFileCache, "Cache file already open?");

  // In single process Gecko, store the media cache in the profile directory
  // so that multiple users can use separate media caches concurrently.
  // In multi-process Gecko, there is no profile dir, so just store it in the
  // system temp directory instead.
  nsresult rv;
  nsCOMPtr<nsIFile> tmp;
  const char* dir = (XRE_GetProcessType() == GeckoProcessType_Content) ?
    NS_OS_TEMP_DIR : NS_APP_USER_PROFILE_LOCAL_50_DIR;
  rv = NS_GetSpecialDirectory(dir, getter_AddRefs(tmp));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsILocalFile> tmpFile = do_QueryInterface(tmp);
  NS_ENSURE_TRUE(tmpFile != nsnull, NS_ERROR_FAILURE);

  // We put the media cache file in
  // ${TempDir}/mozilla-media-cache/media_cache
  rv = tmpFile->AppendNative(nsDependentCString("mozilla-media-cache"));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = tmpFile->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (rv == NS_ERROR_FILE_ALREADY_EXISTS) {
    // Ensure the permissions are 0700. If not, we won't be able to create,
    // read to and write from the media cache file in its subdirectory on
    // non-Windows platforms.
    PRUint32 perms;
    rv = tmpFile->GetPermissions(&perms);
    NS_ENSURE_SUCCESS(rv,rv);
    if (perms != 0700) {
      rv = tmpFile->SetPermissions(0700);
      NS_ENSURE_SUCCESS(rv,rv);
    }
  } else {
    NS_ENSURE_SUCCESS(rv,rv);
  }

  rv = tmpFile->AppendNative(nsDependentCString("media_cache"));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = tmpFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0700);
  NS_ENSURE_SUCCESS(rv,rv);

  PRFileDesc* fileDesc = nsnull;
  rv = tmpFile->OpenNSPRFileDesc(PR_RDWR | nsILocalFile::DELETE_ON_CLOSE,
                                 PR_IRWXU, &fileDesc);
  NS_ENSURE_SUCCESS(rv,rv);

  mFileCache = new FileBlockCache();
  rv = mFileCache->Open(fileDesc);
  NS_ENSURE_SUCCESS(rv,rv);

#ifdef PR_LOGGING
  if (!gMediaCacheLog) {
    gMediaCacheLog = PR_NewLogModule("nsMediaCache");
  }
#endif

  nsMediaCacheFlusher::Init();

  return NS_OK;
}

void
nsMediaCache::Flush()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  if (!gMediaCache)
    return;

  gMediaCache->FlushInternal();
}

void
nsMediaCache::FlushInternal()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  for (PRUint32 blockIndex = 0; blockIndex < mIndex.Length(); ++blockIndex) {
    FreeBlock(blockIndex);
  }

  // Truncate file, close it, and reopen
  Truncate();
  NS_ASSERTION(mIndex.Length() == 0, "Blocks leaked?");
  if (mFileCache) {
    mFileCache->Close();
    mFileCache = nsnull;
  }
  Init();
}

void
nsMediaCache::MaybeShutdown()
{
  NS_ASSERTION(NS_IsMainThread(),
               "nsMediaCache::MaybeShutdown called on non-main thread");
  if (!gMediaCache->mStreams.IsEmpty()) {
    // Don't shut down yet, streams are still alive
    return;
  }

  // Since we're on the main thread, no-one is going to add a new stream
  // while we shut down.
  // This function is static so we don't have to delete 'this'.
  delete gMediaCache;
  gMediaCache = nsnull;
  NS_IF_RELEASE(gMediaCacheFlusher);
}

static void
InitMediaCache()
{
  if (gMediaCache)
    return;

  gMediaCache = new nsMediaCache();
  if (!gMediaCache)
    return;

  nsresult rv = gMediaCache->Init();
  if (NS_FAILED(rv)) {
    delete gMediaCache;
    gMediaCache = nsnull;
  }
}

nsresult
nsMediaCache::ReadCacheFile(PRInt64 aOffset, void* aData, PRInt32 aLength,
                            PRInt32* aBytes)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  if (!mFileCache)
    return NS_ERROR_FAILURE;

  return mFileCache->Read(aOffset, reinterpret_cast<PRUint8*>(aData), aLength, aBytes);
}

nsresult
nsMediaCache::ReadCacheFileAllBytes(PRInt64 aOffset, void* aData, PRInt32 aLength)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  PRInt64 offset = aOffset;
  PRInt32 count = aLength;
  // Cast to char* so we can do byte-wise pointer arithmetic
  char* data = static_cast<char*>(aData);
  while (count > 0) {
    PRInt32 bytes;
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

static PRInt32 GetMaxBlocks()
{
  // We look up the cache size every time. This means dynamic changes
  // to the pref are applied.
  // Cache size is in KB
  PRInt32 cacheSize = Preferences::GetInt("media.cache_size", 500*1024);
  PRInt64 maxBlocks = static_cast<PRInt64>(cacheSize)*1024/nsMediaCache::BLOCK_SIZE;
  maxBlocks = NS_MAX<PRInt64>(maxBlocks, 1);
  return PRInt32(NS_MIN<PRInt64>(maxBlocks, PR_INT32_MAX));
}

PRInt32
nsMediaCache::FindBlockForIncomingData(TimeStamp aNow,
                                       nsMediaCacheStream* aStream)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  PRInt32 blockIndex = FindReusableBlock(aNow, aStream,
      aStream->mChannelOffset/BLOCK_SIZE, PR_INT32_MAX);

  if (blockIndex < 0 || !IsBlockFree(blockIndex)) {
    // The block returned is already allocated.
    // Don't reuse it if a) there's room to expand the cache or
    // b) the data we're going to store in the free block is not higher
    // priority than the data already stored in the free block.
    // The latter can lead us to go over the cache limit a bit.
    if ((mIndex.Length() < PRUint32(GetMaxBlocks()) || blockIndex < 0 ||
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
nsMediaCache::BlockIsReusable(PRInt32 aBlockIndex)
{
  Block* block = &mIndex[aBlockIndex];
  for (PRUint32 i = 0; i < block->mOwners.Length(); ++i) {
    nsMediaCacheStream* stream = block->mOwners[i].mStream;
    if (stream->mPinCount > 0 ||
        stream->mStreamOffset/BLOCK_SIZE == block->mOwners[i].mStreamBlock) {
      return false;
    }
  }
  return true;
}

void
nsMediaCache::AppendMostReusableBlock(BlockList* aBlockList,
                                      nsTArray<PRUint32>* aResult,
                                      PRInt32 aBlockIndexLimit)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  PRInt32 blockIndex = aBlockList->GetLastBlock();
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

PRInt32
nsMediaCache::FindReusableBlock(TimeStamp aNow,
                                nsMediaCacheStream* aForStream,
                                PRInt32 aForStreamBlock,
                                PRInt32 aMaxSearchBlockIndex)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  PRUint32 length = NS_MIN(PRUint32(aMaxSearchBlockIndex), mIndex.Length());

  if (aForStream && aForStreamBlock > 0 &&
      PRUint32(aForStreamBlock) <= aForStream->mBlocks.Length()) {
    PRInt32 prevCacheBlock = aForStream->mBlocks[aForStreamBlock - 1];
    if (prevCacheBlock >= 0) {
      PRUint32 freeBlockScanEnd =
        NS_MIN(length, prevCacheBlock + FREE_BLOCK_SCAN_LIMIT);
      for (PRUint32 i = prevCacheBlock; i < freeBlockScanEnd; ++i) {
        if (IsBlockFree(i))
          return i;
      }
    }
  }

  if (!mFreeBlocks.IsEmpty()) {
    PRInt32 blockIndex = mFreeBlocks.GetFirstBlock();
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
  nsAutoTArray<PRUint32,8> candidates;
  for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
    nsMediaCacheStream* stream = mStreams[i];
    if (stream->mPinCount > 0) {
      // No point in even looking at this stream's blocks
      continue;
    }

    AppendMostReusableBlock(&stream->mMetadataBlocks, &candidates, length);
    AppendMostReusableBlock(&stream->mPlayedBlocks, &candidates, length);

    // Don't consider readahead blocks in non-seekable streams. If we
    // remove the block we won't be able to seek back to read it later.
    if (stream->mIsSeekable) {
      AppendMostReusableBlock(&stream->mReadaheadBlocks, &candidates, length);
    }
  }

  TimeDuration latestUse;
  PRInt32 latestUseBlock = -1;
  for (PRUint32 i = 0; i < candidates.Length(); ++i) {
    TimeDuration nextUse = PredictNextUse(aNow, candidates[i]);
    if (nextUse > latestUse) {
      latestUse = nextUse;
      latestUseBlock = candidates[i];
    }
  }

  return latestUseBlock;
}

nsMediaCache::BlockList*
nsMediaCache::GetListForBlock(BlockOwner* aBlock)
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
    return nsnull;
  }
}

nsMediaCache::BlockOwner*
nsMediaCache::GetBlockOwner(PRInt32 aBlockIndex, nsMediaCacheStream* aStream)
{
  Block* block = &mIndex[aBlockIndex];
  for (PRUint32 i = 0; i < block->mOwners.Length(); ++i) {
    if (block->mOwners[i].mStream == aStream)
      return &block->mOwners[i];
  }
  return nsnull;
}

void
nsMediaCache::SwapBlocks(PRInt32 aBlockIndex1, PRInt32 aBlockIndex2)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  Block* block1 = &mIndex[aBlockIndex1];
  Block* block2 = &mIndex[aBlockIndex2];

  block1->mOwners.SwapElements(block2->mOwners);

  // Now all references to block1 have to be replaced with block2 and
  // vice versa.
  // First update stream references to blocks via mBlocks.
  const Block* blocks[] = { block1, block2 };
  PRInt32 blockIndices[] = { aBlockIndex1, aBlockIndex2 };
  for (PRInt32 i = 0; i < 2; ++i) {
    for (PRUint32 j = 0; j < blocks[i]->mOwners.Length(); ++j) {
      const BlockOwner* b = &blocks[i]->mOwners[j];
      b->mStream->mBlocks[b->mStreamBlock] = blockIndices[i];
    }
  }

  // Now update references to blocks in block lists.
  mFreeBlocks.NotifyBlockSwapped(aBlockIndex1, aBlockIndex2);

  nsTHashtable<nsPtrHashKey<nsMediaCacheStream> > visitedStreams;
  visitedStreams.Init();

  for (PRInt32 i = 0; i < 2; ++i) {
    for (PRUint32 j = 0; j < blocks[i]->mOwners.Length(); ++j) {
      nsMediaCacheStream* stream = blocks[i]->mOwners[j].mStream;
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
nsMediaCache::RemoveBlockOwner(PRInt32 aBlockIndex, nsMediaCacheStream* aStream)
{
  Block* block = &mIndex[aBlockIndex];
  for (PRUint32 i = 0; i < block->mOwners.Length(); ++i) {
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
nsMediaCache::AddBlockOwnerAsReadahead(PRInt32 aBlockIndex,
                                       nsMediaCacheStream* aStream,
                                       PRInt32 aStreamBlockIndex)
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
nsMediaCache::FreeBlock(PRInt32 aBlock)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  Block* block = &mIndex[aBlock];
  if (block->mOwners.IsEmpty()) {
    // already free
    return;
  }

  LOG(PR_LOG_DEBUG, ("Released block %d", aBlock));

  for (PRUint32 i = 0; i < block->mOwners.Length(); ++i) {
    BlockOwner* bo = &block->mOwners[i];
    GetListForBlock(bo)->RemoveBlock(aBlock);
    bo->mStream->mBlocks[bo->mStreamBlock] = -1;
  }
  block->mOwners.Clear();
  mFreeBlocks.AddFirstBlock(aBlock);
  Verify();
}

TimeDuration
nsMediaCache::PredictNextUse(TimeStamp aNow, PRInt32 aBlock)
{
  mReentrantMonitor.AssertCurrentThreadIn();
  NS_ASSERTION(!IsBlockFree(aBlock), "aBlock is free");

  Block* block = &mIndex[aBlock];
  // Blocks can be belong to multiple streams. The predicted next use
  // time is the earliest time predicted by any of the streams.
  TimeDuration result;
  for (PRUint32 i = 0; i < block->mOwners.Length(); ++i) {
    BlockOwner* bo = &block->mOwners[i];
    TimeDuration prediction;
    switch (bo->mClass) {
    case METADATA_BLOCK:
      // This block should be managed in LRU mode. For metadata we predict
      // that the time until the next use is the time since the last use.
      prediction = aNow - bo->mLastUseTime;
      break;
    case PLAYED_BLOCK:
      // This block should be managed in LRU mode, and we should impose
      // a "replay delay" to reflect the likelihood of replay happening
      NS_ASSERTION(static_cast<PRInt64>(bo->mStreamBlock)*BLOCK_SIZE <
                   bo->mStream->mStreamOffset,
                   "Played block after the current stream position?");
      prediction = aNow - bo->mLastUseTime +
        TimeDuration::FromSeconds(REPLAY_DELAY);
      break;
    case READAHEAD_BLOCK: {
      PRInt64 bytesAhead =
        static_cast<PRInt64>(bo->mStreamBlock)*BLOCK_SIZE - bo->mStream->mStreamOffset;
      NS_ASSERTION(bytesAhead >= 0,
                   "Readahead block before the current stream position?");
      PRInt64 millisecondsAhead =
        bytesAhead*1000/bo->mStream->mPlaybackBytesPerSecond;
      prediction = TimeDuration::FromMilliseconds(
          NS_MIN<PRInt64>(millisecondsAhead, PR_INT32_MAX));
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
nsMediaCache::PredictNextUseForIncomingData(nsMediaCacheStream* aStream)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  PRInt64 bytesAhead = aStream->mChannelOffset - aStream->mStreamOffset;
  if (bytesAhead <= -BLOCK_SIZE) {
    // Hmm, no idea when data behind us will be used. Guess 24 hours.
    return TimeDuration::FromSeconds(24*60*60);
  }
  if (bytesAhead <= 0)
    return TimeDuration(0);
  PRInt64 millisecondsAhead = bytesAhead*1000/aStream->mPlaybackBytesPerSecond;
  return TimeDuration::FromMilliseconds(
      NS_MIN<PRInt64>(millisecondsAhead, PR_INT32_MAX));
}

enum StreamAction { NONE, SEEK, SEEK_AND_RESUME, RESUME, SUSPEND };

void
nsMediaCache::Update()
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

    PRInt32 maxBlocks = GetMaxBlocks();
    TimeStamp now = TimeStamp::Now();

    PRInt32 freeBlockCount = mFreeBlocks.GetCount();
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
    TimeDuration latestPredictedUseForOverflow = 0;
    for (PRInt32 blockIndex = mIndex.Length() - 1; blockIndex >= maxBlocks;
         --blockIndex) {
      if (IsBlockFree(blockIndex)) {
        // Don't count overflowing free blocks in our free block count
        --freeBlockCount;
        continue;
      }
      TimeDuration predictedUse = PredictNextUse(now, blockIndex);
      latestPredictedUseForOverflow = NS_MAX(latestPredictedUseForOverflow, predictedUse);
    }

    // Now try to move overflowing blocks to the main part of the cache.
    for (PRInt32 blockIndex = mIndex.Length() - 1; blockIndex >= maxBlocks;
         --blockIndex) {
      if (IsBlockFree(blockIndex))
        continue;

      Block* block = &mIndex[blockIndex];
      // Try to relocate the block close to other blocks for the first stream.
      // There is no point in trying to make it close to other blocks in
      // *all* the streams it might belong to.
      PRInt32 destinationBlockIndex =
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
          LOG(PR_LOG_DEBUG, ("Swapping blocks %d and %d (trimming cache)",
              blockIndex, destinationBlockIndex));
          // Swapping the block metadata here lets us maintain the
          // correct positions in the linked lists
          SwapBlocks(blockIndex, destinationBlockIndex);
          //Free the overflowing block even if the copy failed.
          LOG(PR_LOG_DEBUG, ("Released block %d (trimming cache)", blockIndex));
          FreeBlock(blockIndex);
        }
      } else {
        LOG(PR_LOG_DEBUG, ("Could not trim cache block %d (destination %d, predicted next use %f, latest predicted use for overflow %f",
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
    PRInt32 nonSeekableReadaheadBlockCount = 0;
    for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
      nsMediaCacheStream* stream = mStreams[i];
      if (!stream->mIsSeekable) {
        nonSeekableReadaheadBlockCount += stream->mReadaheadBlocks.GetCount();
      }
    }

    // If freeBlockCount is zero, then compute the latest of
    // the predicted next-uses for all blocks
    TimeDuration latestNextUse;
    if (freeBlockCount == 0) {
      PRInt32 reusableBlock = FindReusableBlock(now, nsnull, 0, maxBlocks);
      if (reusableBlock >= 0) {
        latestNextUse = PredictNextUse(now, reusableBlock);
      }
    }

    for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
      actions.AppendElement(NONE);

      nsMediaCacheStream* stream = mStreams[i];
      if (stream->mClosed)
        continue;

      // Figure out where we should be reading from. It's the first
      // uncached byte after the current mStreamOffset.
      PRInt64 dataOffset = stream->GetCachedDataEndInternal(stream->mStreamOffset);

      // Compute where we'd actually seek to to read at readOffset
      PRInt64 desiredOffset = dataOffset;
      if (stream->mIsSeekable) {
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
        LOG(PR_LOG_DEBUG, ("Stream %p at end of stream", stream));
        enableReading = !stream->mCacheSuspended &&
          stream->mStreamLength == stream->mChannelOffset;
      } else if (desiredOffset < stream->mStreamOffset) {
        // We're reading to try to catch up to where the current stream
        // reader wants to be. Better not stop.
        LOG(PR_LOG_DEBUG, ("Stream %p catching up", stream));
        enableReading = true;
      } else if (desiredOffset < stream->mStreamOffset + BLOCK_SIZE) {
        // The stream reader is waiting for us, or nearly so. Better feed it.
        LOG(PR_LOG_DEBUG, ("Stream %p feeding reader", stream));
        enableReading = true;
      } else if (!stream->mIsSeekable &&
                 nonSeekableReadaheadBlockCount >= maxBlocks*NONSEEKABLE_READAHEAD_MAX) {
        // This stream is not seekable and there are already too many blocks
        // being cached for readahead for nonseekable streams (which we can't
        // free). So stop reading ahead now.
        LOG(PR_LOG_DEBUG, ("Stream %p throttling non-seekable readahead", stream));
        enableReading = false;
      } else if (mIndex.Length() > PRUint32(maxBlocks)) {
        // We're in the process of bringing the cache size back to the
        // desired limit, so don't bring in more data yet
        LOG(PR_LOG_DEBUG, ("Stream %p throttling to reduce cache size", stream));
        enableReading = false;
      } else if (freeBlockCount > 0 || mIndex.Length() < PRUint32(maxBlocks)) {
        // Free blocks in the cache, so keep reading
        LOG(PR_LOG_DEBUG, ("Stream %p reading since there are free blocks", stream));
        enableReading = true;
      } else if (latestNextUse <= TimeDuration(0)) {
        // No reusable blocks, so can't read anything
        LOG(PR_LOG_DEBUG, ("Stream %p throttling due to no reusable blocks", stream));
        enableReading = false;
      } else {
        // Read ahead if the data we expect to read is more valuable than
        // the least valuable block in the main part of the cache
        TimeDuration predictedNewDataUse = PredictNextUseForIncomingData(stream);
        LOG(PR_LOG_DEBUG, ("Stream %p predict next data in %f, current worst block is %f",
            stream, predictedNewDataUse.ToSeconds(), latestNextUse.ToSeconds()));
        enableReading = predictedNewDataUse < latestNextUse;
      }

      if (enableReading) {
        for (PRUint32 j = 0; j < i; ++j) {
          nsMediaCacheStream* other = mStreams[j];
          if (other->mResourceID == stream->mResourceID &&
              !other->mClient->IsSuspended() &&
              other->mChannelOffset/BLOCK_SIZE == desiredOffset/BLOCK_SIZE) {
            // This block is already going to be read by the other stream.
            // So don't try to read it from this stream as well.
            enableReading = false;
            LOG(PR_LOG_DEBUG, ("Stream %p waiting on same block (%lld) from stream %p",
                               stream, desiredOffset/BLOCK_SIZE, other));
            break;
          }
        }
      }

      if (stream->mChannelOffset != desiredOffset && enableReading) {
        // We need to seek now.
        NS_ASSERTION(stream->mIsSeekable || desiredOffset == 0,
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
  for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
    nsMediaCacheStream* stream = mStreams[i];
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

  for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
    nsMediaCacheStream* stream = mStreams[i];
    nsresult rv;
    switch (actions[i]) {
    case SEEK:
	case SEEK_AND_RESUME:
      LOG(PR_LOG_DEBUG, ("Stream %p CacheSeek to %lld (resume=%d)", stream,
           (long long)stream->mChannelOffset, actions[i] == SEEK_AND_RESUME));
      rv = stream->mClient->CacheClientSeek(stream->mChannelOffset,
                                            actions[i] == SEEK_AND_RESUME);
      break;
    case RESUME:
      LOG(PR_LOG_DEBUG, ("Stream %p Resumed", stream));
      rv = stream->mClient->CacheClientResume();
      break;
    case SUSPEND:
      LOG(PR_LOG_DEBUG, ("Stream %p Suspended", stream));
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
nsMediaCache::QueueUpdate()
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
nsMediaCache::Verify()
{
  mReentrantMonitor.AssertCurrentThreadIn();

  mFreeBlocks.Verify();
  for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
    nsMediaCacheStream* stream = mStreams[i];
    stream->mReadaheadBlocks.Verify();
    stream->mPlayedBlocks.Verify();
    stream->mMetadataBlocks.Verify();

    // Verify that the readahead blocks are listed in stream block order
    PRInt32 block = stream->mReadaheadBlocks.GetFirstBlock();
    PRInt32 lastStreamBlock = -1;
    while (block >= 0) {
      PRUint32 j = 0;
      while (mIndex[block].mOwners[j].mStream != stream) {
        ++j;
      }
      PRInt32 nextStreamBlock =
        PRInt32(mIndex[block].mOwners[j].mStreamBlock);
      NS_ASSERTION(lastStreamBlock < nextStreamBlock,
                   "Blocks not increasing in readahead stream");
      lastStreamBlock = nextStreamBlock;
      block = stream->mReadaheadBlocks.GetNextBlock(block);
    }
  }
}
#endif

void
nsMediaCache::InsertReadaheadBlock(BlockOwner* aBlockOwner,
                                   PRInt32 aBlockIndex)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  // Find the last block whose stream block is before aBlockIndex's
  // stream block, and insert after it
  nsMediaCacheStream* stream = aBlockOwner->mStream;
  PRInt32 readaheadIndex = stream->mReadaheadBlocks.GetLastBlock();
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
nsMediaCache::AllocateAndWriteBlock(nsMediaCacheStream* aStream, const void* aData,
                                    nsMediaCacheStream::ReadMode aMode)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  PRInt32 streamBlockIndex = aStream->mChannelOffset/BLOCK_SIZE;

  // Remove all cached copies of this block
  ResourceStreamIterator iter(aStream->mResourceID);
  while (nsMediaCacheStream* stream = iter.Next()) {
    while (streamBlockIndex >= PRInt32(stream->mBlocks.Length())) {
      stream->mBlocks.AppendElement(-1);
    }
    if (stream->mBlocks[streamBlockIndex] >= 0) {
      // We no longer want to own this block
      PRInt32 globalBlockIndex = stream->mBlocks[streamBlockIndex];
      LOG(PR_LOG_DEBUG, ("Released block %d from stream %p block %d(%lld)",
          globalBlockIndex, stream, streamBlockIndex, (long long)streamBlockIndex*BLOCK_SIZE));
      RemoveBlockOwner(globalBlockIndex, stream);
    }
  }

  // Extend the mBlocks array as necessary

  TimeStamp now = TimeStamp::Now();
  PRInt32 blockIndex = FindBlockForIncomingData(now, aStream);
  if (blockIndex >= 0) {
    FreeBlock(blockIndex);

    Block* block = &mIndex[blockIndex];
    LOG(PR_LOG_DEBUG, ("Allocated block %d to stream %p block %d(%lld)",
        blockIndex, aStream, streamBlockIndex, (long long)streamBlockIndex*BLOCK_SIZE));

    mFreeBlocks.RemoveBlock(blockIndex);

    // Tell each stream using this resource about the new block.
    ResourceStreamIterator iter(aStream->mResourceID);
    while (nsMediaCacheStream* stream = iter.Next()) {
      BlockOwner* bo = block->mOwners.AppendElement();
      if (!bo)
        return;

      bo->mStream = stream;
      bo->mStreamBlock = streamBlockIndex;
      bo->mLastUseTime = now;
      stream->mBlocks[streamBlockIndex] = blockIndex;
      if (streamBlockIndex*BLOCK_SIZE < stream->mStreamOffset) {
        bo->mClass = aMode == nsMediaCacheStream::MODE_PLAYBACK
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

    nsresult rv = mFileCache->WriteBlock(blockIndex, reinterpret_cast<const PRUint8*>(aData));
    if (NS_FAILED(rv)) {
      LOG(PR_LOG_DEBUG, ("Released block %d from stream %p block %d(%lld)",
          blockIndex, aStream, streamBlockIndex, (long long)streamBlockIndex*BLOCK_SIZE));
      FreeBlock(blockIndex);
    }
  }

  // Queue an Update since the cache state has changed (for example
  // we might want to stop loading because the cache is full)
  QueueUpdate();
}

void
nsMediaCache::OpenStream(nsMediaCacheStream* aStream)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  LOG(PR_LOG_DEBUG, ("Stream %p opened", aStream));
  mStreams.AppendElement(aStream);
  aStream->mResourceID = AllocateResourceID();

  // Queue an update since a new stream has been opened.
  gMediaCache->QueueUpdate();
}

void
nsMediaCache::ReleaseStream(nsMediaCacheStream* aStream)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  LOG(PR_LOG_DEBUG, ("Stream %p closed", aStream));
  mStreams.RemoveElement(aStream);
}

void
nsMediaCache::ReleaseStreamBlocks(nsMediaCacheStream* aStream)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  // XXX scanning the entire stream doesn't seem great, if not much of it
  // is cached, but the only easy alternative is to scan the entire cache
  // which isn't better
  PRUint32 length = aStream->mBlocks.Length();
  for (PRUint32 i = 0; i < length; ++i) {
    PRInt32 blockIndex = aStream->mBlocks[i];
    if (blockIndex >= 0) {
      LOG(PR_LOG_DEBUG, ("Released block %d from stream %p block %d(%lld)",
          blockIndex, aStream, i, (long long)i*BLOCK_SIZE));
      RemoveBlockOwner(blockIndex, aStream);
    }
  }
}

void
nsMediaCache::Truncate()
{
  PRUint32 end;
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
nsMediaCache::NoteBlockUsage(nsMediaCacheStream* aStream, PRInt32 aBlockIndex,
                             nsMediaCacheStream::ReadMode aMode,
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
    (aMode == nsMediaCacheStream::MODE_METADATA || bo->mClass == METADATA_BLOCK)
    ? METADATA_BLOCK : PLAYED_BLOCK;
  // Since this is just being used now, it can definitely be at the front
  // of mMetadataBlocks or mPlayedBlocks
  GetListForBlock(bo)->AddFirstBlock(aBlockIndex);
  bo->mLastUseTime = aNow;
  Verify();
}

void
nsMediaCache::NoteSeek(nsMediaCacheStream* aStream, PRInt64 aOldOffset)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  if (aOldOffset < aStream->mStreamOffset) {
    // We seeked forward. Convert blocks from readahead to played.
    // Any readahead block that intersects the seeked-over range must
    // be converted.
    PRInt32 blockIndex = aOldOffset/BLOCK_SIZE;
    PRInt32 endIndex =
      NS_MIN<PRInt64>((aStream->mStreamOffset + BLOCK_SIZE - 1)/BLOCK_SIZE,
             aStream->mBlocks.Length());
    TimeStamp now = TimeStamp::Now();
    while (blockIndex < endIndex) {
      PRInt32 cacheBlockIndex = aStream->mBlocks[blockIndex];
      if (cacheBlockIndex >= 0) {
        // Marking the block used may not be exactly what we want but
        // it's simple
        NoteBlockUsage(aStream, cacheBlockIndex, nsMediaCacheStream::MODE_PLAYBACK,
                       now);
      }
      ++blockIndex;
    }
  } else {
    // We seeked backward. Convert from played to readahead.
    // Any played block that is entirely after the start of the seeked-over
    // range must be converted.
    PRInt32 blockIndex =
      (aStream->mStreamOffset + BLOCK_SIZE - 1)/BLOCK_SIZE;
    PRInt32 endIndex =
      NS_MIN<PRInt64>((aOldOffset + BLOCK_SIZE - 1)/BLOCK_SIZE,
             aStream->mBlocks.Length());
    while (blockIndex < endIndex) {
      PRInt32 cacheBlockIndex = aStream->mBlocks[endIndex - 1];
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
nsMediaCacheStream::NotifyDataLength(PRInt64 aLength)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  mStreamLength = aLength;
}

void
nsMediaCacheStream::NotifyDataStarted(PRInt64 aOffset)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  NS_WARN_IF_FALSE(aOffset == mChannelOffset,
                   "Server is giving us unexpected offset");
  mChannelOffset = aOffset;
  if (mStreamLength >= 0) {
    // If we started reading at a certain offset, then for sure
    // the stream is at least that long.
    mStreamLength = NS_MAX(mStreamLength, mChannelOffset);
  }
}

bool
nsMediaCacheStream::UpdatePrincipal(nsIPrincipal* aPrincipal)
{
  return nsContentUtils::CombineResourcePrincipals(&mPrincipal, aPrincipal);
}

void
nsMediaCacheStream::NotifyDataReceived(PRInt64 aSize, const char* aData,
    nsIPrincipal* aPrincipal)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  // Update principals before putting the data in the cache. This is important,
  // we want to make sure all principals are updated before any consumer
  // can see the new data.
  // We do this without holding the cache monitor, in case the client wants
  // to do something that takes a lock.
  {
    nsMediaCache::ResourceStreamIterator iter(mResourceID);
    while (nsMediaCacheStream* stream = iter.Next()) {
      if (stream->UpdatePrincipal(aPrincipal)) {
        stream->mClient->CacheClientNotifyPrincipalChanged();
      }
    }
  }

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  PRInt64 size = aSize;
  const char* data = aData;

  LOG(PR_LOG_DEBUG, ("Stream %p DataReceived at %lld count=%lld",
      this, (long long)mChannelOffset, (long long)aSize));

  // We process the data one block (or part of a block) at a time
  while (size > 0) {
    PRUint32 blockIndex = mChannelOffset/BLOCK_SIZE;
    PRInt32 blockOffset = PRInt32(mChannelOffset - blockIndex*BLOCK_SIZE);
    PRInt32 chunkSize = NS_MIN<PRInt64>(BLOCK_SIZE - blockOffset, size);

    // This gets set to something non-null if we have a whole block
    // of data to write to the cache
    const char* blockDataToStore = nsnull;
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
      memcpy(reinterpret_cast<char*>(mPartialBlockBuffer) + blockOffset,
             data, chunkSize);

      if (blockOffset + chunkSize == BLOCK_SIZE) {
        // We completed a block, so lets write it out.
        blockDataToStore = reinterpret_cast<char*>(mPartialBlockBuffer);
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

  nsMediaCache::ResourceStreamIterator iter(mResourceID);
  while (nsMediaCacheStream* stream = iter.Next()) {
    if (stream->mStreamLength >= 0) {
      // The stream is at least as long as what we've read
      stream->mStreamLength = NS_MAX(stream->mStreamLength, mChannelOffset);
    }
    stream->mClient->CacheClientNotifyDataReceived();
  }

  // Notify in case there's a waiting reader
  // XXX it would be fairly easy to optimize things a lot more to
  // avoid waking up reader threads unnecessarily
  mon.NotifyAll();
}

void
nsMediaCacheStream::NotifyDataEnded(nsresult aStatus)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());

  if (NS_FAILED(aStatus)) {
    // Disconnect from other streams sharing our resource, since they
    // should continue trying to load. Our load might have been deliberately
    // canceled and that shouldn't affect other streams.
    mResourceID = gMediaCache->AllocateResourceID();
  }

  PRInt32 blockOffset = PRInt32(mChannelOffset%BLOCK_SIZE);
  if (blockOffset > 0) {
    // Write back the partial block
    memset(reinterpret_cast<char*>(mPartialBlockBuffer) + blockOffset, 0,
           BLOCK_SIZE - blockOffset);
    gMediaCache->AllocateAndWriteBlock(this, mPartialBlockBuffer,
        mMetadataInPartialBlockBuffer ? MODE_METADATA : MODE_PLAYBACK);
    // Wake up readers who may be waiting for this data
    mon.NotifyAll();
  }

  if (!mDidNotifyDataEnded) {
    nsMediaCache::ResourceStreamIterator iter(mResourceID);
    while (nsMediaCacheStream* stream = iter.Next()) {
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

nsMediaCacheStream::~nsMediaCacheStream()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  NS_ASSERTION(!mPinCount, "Unbalanced Pin");

  if (gMediaCache) {
    NS_ASSERTION(mClosed, "Stream was not closed");
    gMediaCache->ReleaseStream(this);
    nsMediaCache::MaybeShutdown();
  }
}

void
nsMediaCacheStream::SetSeekable(bool aIsSeekable)
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  NS_ASSERTION(mIsSeekable || aIsSeekable ||
               mChannelOffset == 0, "channel offset must be zero when we become non-seekable");
  mIsSeekable = aIsSeekable;
  // Queue an Update since we may change our strategy for dealing
  // with this stream
  gMediaCache->QueueUpdate();
}

bool
nsMediaCacheStream::IsSeekable()
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  return mIsSeekable;
}

bool
nsMediaCacheStream::AreAllStreamsForResourceSuspended(MediaResource** aActiveStream)
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  nsMediaCache::ResourceStreamIterator iter(mResourceID);
  while (nsMediaCacheStream* stream = iter.Next()) {
    if (!stream->mCacheSuspended && !stream->mChannelEnded && !stream->mClosed) {
      if (aActiveStream) {
        *aActiveStream = stream->mClient;
      }
      return false;
    }
  }
  if (aActiveStream) {
    *aActiveStream = nsnull;
  }
  return true;
}

void
nsMediaCacheStream::Close()
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
nsMediaCacheStream::EnsureCacheUpdate()
{
  if (mHasHadUpdate)
    return;
  gMediaCache->Update();
}

void
nsMediaCacheStream::CloseInternal(ReentrantMonitorAutoEnter& aReentrantMonitor)
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
nsMediaCacheStream::Pin()
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  ++mPinCount;
  // Queue an Update since we may no longer want to read more into the
  // cache, if this stream's block have become non-evictable
  gMediaCache->QueueUpdate();
}

void
nsMediaCacheStream::Unpin()
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  NS_ASSERTION(mPinCount > 0, "Unbalanced Unpin");
  --mPinCount;
  // Queue an Update since we may be able to read more into the
  // cache, if this stream's block have become evictable
  gMediaCache->QueueUpdate();
}

PRInt64
nsMediaCacheStream::GetLength()
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  return mStreamLength;
}

PRInt64
nsMediaCacheStream::GetNextCachedData(PRInt64 aOffset)
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  return GetNextCachedDataInternal(aOffset);
}

PRInt64
nsMediaCacheStream::GetCachedDataEnd(PRInt64 aOffset)
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  return GetCachedDataEndInternal(aOffset);
}

bool
nsMediaCacheStream::IsDataCachedToEndOfStream(PRInt64 aOffset)
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  if (mStreamLength < 0)
    return false;
  return GetCachedDataEndInternal(aOffset) >= mStreamLength;
}

PRInt64
nsMediaCacheStream::GetCachedDataEndInternal(PRInt64 aOffset)
{
  gMediaCache->GetReentrantMonitor().AssertCurrentThreadIn();
  PRUint32 startBlockIndex = aOffset/BLOCK_SIZE;
  PRUint32 blockIndex = startBlockIndex;
  while (blockIndex < mBlocks.Length() && mBlocks[blockIndex] != -1) {
    ++blockIndex;
  }
  PRInt64 result = blockIndex*BLOCK_SIZE;
  if (blockIndex == mChannelOffset/BLOCK_SIZE) {
    // The block containing mChannelOffset may be partially read but not
    // yet committed to the main cache
    result = mChannelOffset;
  }
  if (mStreamLength >= 0) {
    // The last block in the cache may only be partially valid, so limit
    // the cached range to the stream length
    result = NS_MIN(result, mStreamLength);
  }
  return NS_MAX(result, aOffset);
}

PRInt64
nsMediaCacheStream::GetNextCachedDataInternal(PRInt64 aOffset)
{
  gMediaCache->GetReentrantMonitor().AssertCurrentThreadIn();
  if (aOffset == mStreamLength)
    return -1;

  PRUint32 startBlockIndex = aOffset/BLOCK_SIZE;
  PRUint32 channelBlockIndex = mChannelOffset/BLOCK_SIZE;

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
  PRUint32 blockIndex = startBlockIndex + 1;
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
nsMediaCacheStream::SetReadMode(ReadMode aMode)
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  if (aMode == mCurrentMode)
    return;
  mCurrentMode = aMode;
  gMediaCache->QueueUpdate();
}

void
nsMediaCacheStream::SetPlaybackRate(PRUint32 aBytesPerSecond)
{
  NS_ASSERTION(aBytesPerSecond > 0, "Zero playback rate not allowed");
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  if (aBytesPerSecond == mPlaybackBytesPerSecond)
    return;
  mPlaybackBytesPerSecond = aBytesPerSecond;
  gMediaCache->QueueUpdate();
}

nsresult
nsMediaCacheStream::Seek(PRInt32 aWhence, PRInt64 aOffset)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  if (mClosed)
    return NS_ERROR_FAILURE;

  PRInt64 oldOffset = mStreamOffset;
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

  LOG(PR_LOG_DEBUG, ("Stream %p Seek to %lld", this, (long long)mStreamOffset));
  gMediaCache->NoteSeek(this, oldOffset);

  gMediaCache->QueueUpdate();
  return NS_OK;
}

PRInt64
nsMediaCacheStream::Tell()
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  return mStreamOffset;
}

nsresult
nsMediaCacheStream::Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  if (mClosed)
    return NS_ERROR_FAILURE;

  PRUint32 count = 0;
  // Read one block (or part of a block) at a time
  while (count < aCount) {
    PRUint32 streamBlock = PRUint32(mStreamOffset/BLOCK_SIZE);
    PRUint32 offsetInStreamBlock =
      PRUint32(mStreamOffset - streamBlock*BLOCK_SIZE);
    PRInt64 size = NS_MIN(aCount - count, BLOCK_SIZE - offsetInStreamBlock);

    if (mStreamLength >= 0) {
      // Don't try to read beyond the end of the stream
      PRInt64 bytesRemaining = mStreamLength - mStreamOffset;
      if (bytesRemaining <= 0) {
        // Get out of here and return NS_OK
        break;
      }
      size = NS_MIN(size, bytesRemaining);
      // Clamp size until 64-bit file size issues (bug 500784) are fixed.
      size = NS_MIN(size, PRInt64(PR_INT32_MAX));
    }

    PRInt32 bytes;
    PRInt32 cacheBlock = streamBlock < mBlocks.Length() ? mBlocks[streamBlock] : -1;
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
      nsMediaCacheStream* streamWithPartialBlock = nsnull;
      nsMediaCache::ResourceStreamIterator iter(mResourceID);
      while (nsMediaCacheStream* stream = iter.Next()) {
        if (PRUint32(stream->mChannelOffset/BLOCK_SIZE) == streamBlock &&
            mStreamOffset < stream->mChannelOffset) {
          streamWithPartialBlock = stream;
          break;
        }
      }
      if (streamWithPartialBlock) {
        // We can just use the data in mPartialBlockBuffer. In fact we should
        // use it rather than waiting for the block to fill and land in
        // the cache.
        bytes = NS_MIN<PRInt64>(size, streamWithPartialBlock->mChannelOffset - mStreamOffset);
        memcpy(aBuffer,
          reinterpret_cast<char*>(streamWithPartialBlock->mPartialBlockBuffer) + offsetInStreamBlock, bytes);
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

    PRInt64 offset = cacheBlock*BLOCK_SIZE + offsetInStreamBlock;
    NS_ABORT_IF_FALSE(size >= 0 && size <= PR_INT32_MAX, "Size out of range.");
    nsresult rv = gMediaCache->ReadCacheFile(offset, aBuffer + count, PRInt32(size), &bytes);
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
  LOG(PR_LOG_DEBUG,
      ("Stream %p Read at %lld count=%d", this, (long long)(mStreamOffset-count), count));
  *aBytes = count;
  return NS_OK;
}

nsresult
nsMediaCacheStream::ReadFromCache(char* aBuffer,
                                  PRInt64 aOffset,
                                  PRInt64 aCount)
{
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());
  if (mClosed)
    return NS_ERROR_FAILURE;

  // Read one block (or part of a block) at a time
  PRUint32 count = 0;
  PRInt64 streamOffset = aOffset;
  while (count < aCount) {
    PRUint32 streamBlock = PRUint32(streamOffset/BLOCK_SIZE);
    PRUint32 offsetInStreamBlock =
      PRUint32(streamOffset - streamBlock*BLOCK_SIZE);
    PRInt64 size = NS_MIN<PRInt64>(aCount - count, BLOCK_SIZE - offsetInStreamBlock);

    if (mStreamLength >= 0) {
      // Don't try to read beyond the end of the stream
      PRInt64 bytesRemaining = mStreamLength - streamOffset;
      if (bytesRemaining <= 0) {
        return NS_ERROR_FAILURE;
      }
      size = NS_MIN(size, bytesRemaining);
      // Clamp size until 64-bit file size issues (bug 500784) are fixed.
      size = NS_MIN(size, PRInt64(PR_INT32_MAX));
    }

    PRInt32 bytes;
    PRUint32 channelBlock = PRUint32(mChannelOffset/BLOCK_SIZE);
    PRInt32 cacheBlock = streamBlock < mBlocks.Length() ? mBlocks[streamBlock] : -1;
    if (channelBlock == streamBlock && streamOffset < mChannelOffset) {
      // We can just use the data in mPartialBlockBuffer. In fact we should
      // use it rather than waiting for the block to fill and land in
      // the cache.
      bytes = NS_MIN<PRInt64>(size, mChannelOffset - streamOffset);
      memcpy(aBuffer + count,
        reinterpret_cast<char*>(mPartialBlockBuffer) + offsetInStreamBlock, bytes);
    } else {
      if (cacheBlock < 0) {
        // We expect all blocks to be cached! Fail!
        return NS_ERROR_FAILURE;
      }
      PRInt64 offset = cacheBlock*BLOCK_SIZE + offsetInStreamBlock;
      NS_ABORT_IF_FALSE(size >= 0 && size <= PR_INT32_MAX, "Size out of range.");
      nsresult rv = gMediaCache->ReadCacheFile(offset, aBuffer + count, PRInt32(size), &bytes);
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
nsMediaCacheStream::Init()
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
nsMediaCacheStream::InitAsClone(nsMediaCacheStream* aOriginal)
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
  mIsSeekable = aOriginal->mIsSeekable;

  // Cloned streams are initially suspended, since there is no channel open
  // initially for a clone.
  mCacheSuspended = true;
  mChannelEnded = true;

  if (aOriginal->mDidNotifyDataEnded) {
    mNotifyDataEndedStatus = aOriginal->mNotifyDataEndedStatus;
    mDidNotifyDataEnded = true;
    mClient->CacheClientNotifyDataEnded(mNotifyDataEndedStatus);
  }

  for (PRUint32 i = 0; i < aOriginal->mBlocks.Length(); ++i) {
    PRInt32 cacheBlockIndex = aOriginal->mBlocks[i];
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

nsresult nsMediaCacheStream::GetCachedRanges(nsTArray<MediaByteRange>& aRanges)
{
  // Take the monitor, so that the cached data ranges can't grow while we're
  // trying to loop over them.
  ReentrantMonitorAutoEnter mon(gMediaCache->GetReentrantMonitor());

  // We must be pinned while running this, otherwise the cached data ranges may
  // shrink while we're trying to loop over them.
  NS_ASSERTION(mPinCount > 0, "Must be pinned");

  PRInt64 startOffset = GetNextCachedData(0);
  while (startOffset >= 0) {
    PRInt64 endOffset = GetCachedDataEnd(startOffset);
    NS_ASSERTION(startOffset < endOffset, "Buffered range must end after its start");
    // Bytes [startOffset..endOffset] are cached.
    aRanges.AppendElement(MediaByteRange(startOffset, endOffset));
    startOffset = GetNextCachedData(endOffset);
    NS_ASSERTION(startOffset == -1 || startOffset > endOffset,
      "Must have advanced to start of next range, or hit end of stream");
  }
  return NS_OK;
}
