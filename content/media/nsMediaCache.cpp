 /* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Robert O'Callahan <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/XPCOM.h"

#include "nsMediaCache.h"
#include "nsAutoLock.h"
#include "nsContentUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsNetUtil.h"
#include "prio.h"
#include "nsThreadUtils.h"
#include "nsMediaStream.h"
#include "nsMathUtils.h"
#include "prlog.h"

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

using mozilla::TimeStamp;
using mozilla::TimeDuration;

#ifdef DEBUG
// Turn this on to do very expensive cache state validation
// #define DEBUG_VERIFY_CACHE
#endif

class nsMediaCache {
public:
  friend class nsMediaCacheStream::BlockList;
  typedef nsMediaCacheStream::BlockList BlockList;
  enum {
    BLOCK_SIZE = nsMediaCacheStream::BLOCK_SIZE
  };

  nsMediaCache() : mMonitor(nsAutoMonitor::NewMonitor("media.cache")),
    mFD(nsnull), mFDCurrentPos(0), mUpdateQueued(PR_FALSE)
#ifdef DEBUG
    , mInUpdate(PR_FALSE)
#endif
  {
    MOZ_COUNT_CTOR(nsMediaCache);
  }
  ~nsMediaCache() {
    NS_ASSERTION(mStreams.IsEmpty(), "Stream(s) still open!");
    Truncate();
    NS_ASSERTION(mIndex.Length() == 0, "Blocks leaked?");
    if (mFD) {
      PR_Close(mFD);
    }
    if (mMonitor) {
      nsAutoMonitor::DestroyMonitor(mMonitor);
    }
    MOZ_COUNT_DTOR(nsMediaCache);
  }

  // Main thread only. Creates the backing cache file.
  nsresult Init();
  // Shut down the global cache if it's no longer needed. We shut down
  // the cache as soon as there are no streams. This means that during
  // normal operation we are likely to start up the cache and shut it down
  // many times, but that's OK since starting it up is cheap and
  // shutting it down cleans things up and releases disk space.
  static void MaybeShutdown();

  // Cache-file access methods. These are the lowest-level cache methods.
  // mMonitor must be held; these can be called on any thread.
  // This can return partial reads.
  nsresult ReadCacheFile(PRInt64 aOffset, void* aData, PRInt32 aLength,
                         PRInt32* aBytes);
  // This will fail if all aLength bytes are not read
  nsresult ReadCacheFileAllBytes(PRInt64 aOffset, void* aData, PRInt32 aLength);
  // This will fail if all aLength bytes are not written
  nsresult WriteCacheFile(PRInt64 aOffset, const void* aData, PRInt32 aLength);

  // mMonitor must be held, called on main thread.
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

  // mMonitor must be held; can be called on any thread
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
  void NoteBlockUsage(PRInt32 aBlockIndex, nsMediaCacheStream::ReadMode aMode,
                      TimeStamp aNow);

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

  PRMonitor* Monitor() { return mMonitor; }

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
  // Given a list of blocks sorted with the most reusable blocks at the
  // end, find the last block whose stream is not pinned (if any)
  // and whose cache entry index is less than aBlockIndexLimit
  // and append it to aResult.
  void AppendMostReusableBlock(BlockList* aBlockList,
                               nsTArray<PRUint32>* aResult,
                               PRInt32 aBlockIndexLimit);

  enum BlockClass {
    // block belongs to mFreeBlockList because it's free
    FREE_BLOCK,
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

  struct Block {
    Block() : mStream(nsnull), mClass(FREE_BLOCK) {}

    // The stream that owns this block, or null if the block is free.
    nsMediaCacheStream* mStream;
    // The block index in the stream. Valid only if mStream is non-null.
    PRUint32            mStreamBlock;
    // Time at which this block was last used. Valid only if
    // mClass is METADATA_BLOCK or PLAYED_BLOCK.
    TimeStamp           mLastUseTime;
    // The class is FREE_BLOCK if and only if mStream is null
    BlockClass          mClass;
  };

  // Get the BlockList that the block should belong to given its
  // current mClass and mStream
  BlockList* GetListForBlock(Block* aBlock);
  // Add the block to the free list, mark it FREE_BLOCK, and mark
  // its stream (if any) as not having the block in cache
  void FreeBlock(PRInt32 aBlock);
  // Swap all metadata associated with the two blocks. The caller
  // is responsible for swapping up any cache file state.
  void SwapBlocks(PRInt32 aBlockIndex1, PRInt32 aBlockIndex2);
  // Insert the block into the readahead block list for its stream
  // at the right point in the list.
  void InsertReadaheadBlock(PRInt32 aBlockIndex);

  // Guess the duration until block aBlock will be next used
  TimeDuration PredictNextUse(TimeStamp aNow, PRInt32 aBlock);
  // Guess the duration until the next incoming data on aStream will be used
  TimeDuration PredictNextUseForIncomingData(nsMediaCacheStream* aStream);

  // Truncate the file and index array if there are free blocks at the
  // end
  void Truncate();

  // This member is main-thread only. It contains all the streams.
  nsTArray<nsMediaCacheStream*> mStreams;

  // The monitor protects all the data members here. Also, off-main-thread
  // readers that need to block will Wait() on this monitor. When new
  // data becomes available in the cache, we NotifyAll() on this monitor.
  PRMonitor*      mMonitor;
  // The Blocks describing the cache entries.
  nsTArray<Block> mIndex;
  // The file descriptor of the cache file. The file will be deleted
  // by the operating system when this is closed.
  PRFileDesc*     mFD;
  // The current file offset in the cache file.
  PRInt64         mFDCurrentPos;
  // The list of free blocks; they are not ordered.
  BlockList       mFreeBlocks;
  // The list of metadata blocks; the first block is the most recently used
  BlockList       mMetadataBlocks;
  // The list of played-back blocks; the first block is the most recently used
  BlockList       mPlayedBlocks;
  // True if an event to run Update() has been queued but not processed
  PRPackedBool    mUpdateQueued;
#ifdef DEBUG
  PRPackedBool    mInUpdate;
#endif
};

// There is at most one media cache (although that could quite easily be
// relaxed if we wanted to manage multiple caches with independent
// size limits).
static nsMediaCache* gMediaCache;

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

  if (!mMonitor) {
    // the constructor failed
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIFile> tmp;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmp));
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsILocalFile> tmpFile = do_QueryInterface(tmp);
  if (!tmpFile)
    return NS_ERROR_FAILURE;
  rv = tmpFile->AppendNative(nsDependentCString("moz_media_cache"));
  if (NS_FAILED(rv))
    return rv;
  rv = tmpFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
  if (NS_FAILED(rv))
    return rv;
  rv = tmpFile->OpenNSPRFileDesc(PR_RDWR | nsILocalFile::DELETE_ON_CLOSE,
                                 PR_IRWXU, &mFD);
  if (NS_FAILED(rv))
    return rv;

#ifdef PR_LOGGING
  if (!gMediaCacheLog) {
    gMediaCacheLog = PR_NewLogModule("nsMediaCache");
  }
#endif

  return NS_OK;
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
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  if (!mFD)
    return NS_ERROR_FAILURE;

  if (mFDCurrentPos != aOffset) {
    PROffset64 offset = PR_Seek64(mFD, aOffset, PR_SEEK_SET);
    if (offset != aOffset)
      return NS_ERROR_FAILURE;
    mFDCurrentPos = aOffset;
  }
  PRInt32 amount = PR_Read(mFD, aData, aLength);
  if (amount <= 0)
    return NS_ERROR_FAILURE;
  mFDCurrentPos += amount;
  *aBytes = amount;
  return NS_OK;
}

nsresult
nsMediaCache::ReadCacheFileAllBytes(PRInt64 aOffset, void* aData, PRInt32 aLength)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

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

nsresult
nsMediaCache::WriteCacheFile(PRInt64 aOffset, const void* aData, PRInt32 aLength)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  if (!mFD)
    return NS_ERROR_FAILURE;

  if (mFDCurrentPos != aOffset) {
    PROffset64 offset = PR_Seek64(mFD, aOffset, PR_SEEK_SET);
    if (offset != aOffset)
      return NS_ERROR_FAILURE;
    mFDCurrentPos = aOffset;
  }

  const char* data = static_cast<const char*>(aData);
  PRInt32 length = aLength;
  while (length > 0) {
    PRInt32 amount = PR_Write(mFD, data, length);
    if (amount <= 0)
      return NS_ERROR_FAILURE;
    mFDCurrentPos += amount;
    length -= amount;
    data += amount;
  }

  return NS_OK;
}

static PRInt32 GetMaxBlocks()
{
  // We look up the cache size every time. This means dynamic changes
  // to the pref are applied.
  // Cache size is in KB
  PRInt32 cacheSize = nsContentUtils::GetIntPref("media.cache_size", 50*1024);
  PRInt64 maxBlocks = PRInt64(cacheSize)*1024/nsMediaCache::BLOCK_SIZE;
  maxBlocks = PR_MAX(maxBlocks, 1);
  return PRInt32(PR_MIN(maxBlocks, PR_INT32_MAX));
}

PRInt32
nsMediaCache::FindBlockForIncomingData(TimeStamp aNow,
                                       nsMediaCacheStream* aStream)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  PRInt32 blockIndex = FindReusableBlock(aNow, aStream,
      aStream->mChannelOffset/BLOCK_SIZE, PR_INT32_MAX);

  if (blockIndex < 0 || mIndex[blockIndex].mStream) {
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

void
nsMediaCache::AppendMostReusableBlock(BlockList* aBlockList,
                                      nsTArray<PRUint32>* aResult,
                                      PRInt32 aBlockIndexLimit)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  PRInt32 blockIndex = aBlockList->GetLastBlock();
  if (blockIndex < 0)
    return;
  do {
    // Don't consider blocks for pinned streams, or blocks that are
    // beyond the specified limit, or the block that contains its stream's
    // current read position (such a block contains both played data
    // and readahead data)
    nsMediaCacheStream* stream = mIndex[blockIndex].mStream;
    if (stream->mPinCount == 0 && blockIndex < aBlockIndexLimit &&
        stream->mStreamOffset/BLOCK_SIZE != mIndex[blockIndex].mStreamBlock) {
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
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  PRUint32 length = PR_MIN(PRUint32(aMaxSearchBlockIndex), mIndex.Length());

  if (aForStream && aForStreamBlock > 0 &&
      PRUint32(aForStreamBlock) <= aForStream->mBlocks.Length()) {
    PRInt32 prevCacheBlock = aForStream->mBlocks[aForStreamBlock - 1];
    if (prevCacheBlock >= 0) {
      PRUint32 freeBlockScanEnd =
        PR_MIN(length, prevCacheBlock + FREE_BLOCK_SCAN_LIMIT);
      for (PRUint32 i = prevCacheBlock; i < freeBlockScanEnd; ++i) {
        if (mIndex[i].mClass == FREE_BLOCK)
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
  AppendMostReusableBlock(&mMetadataBlocks, &candidates, length);
  AppendMostReusableBlock(&mPlayedBlocks, &candidates, length);
  for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
    nsMediaCacheStream* stream = mStreams[i];
    // Don't consider a) blocks for pinned streams or b) blocks in
    // non-seekable streams that contain data ahead of the current reader
    // position. In the latter case, if we remove the block we won't be
    // able to seek back to read it later.
    if (!stream->mReadaheadBlocks.IsEmpty() && stream->mIsSeekable &&
        stream->mPinCount == 0) {
      // Find a readahead block that's in the given limit
      PRInt32 blockIndex = stream->mReadaheadBlocks.GetLastBlock();
      do {
        if (PRUint32(blockIndex) < length) {
          candidates.AppendElement(blockIndex);
          break;
        }
        blockIndex = stream->mReadaheadBlocks.GetPrevBlock(blockIndex);
      } while (blockIndex >= 0);
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

#ifdef DEBUG
  for (PRUint32 blockIndex = 0; blockIndex < length; ++blockIndex) {
    Block* block = &mIndex[blockIndex];
    nsMediaCacheStream* stream = block->mStream;
    NS_ASSERTION(!stream || stream->mPinCount > 0 ||
                 (!stream->mIsSeekable && block->mClass == READAHEAD_BLOCK) ||
                 stream->mStreamOffset/BLOCK_SIZE == block->mStreamBlock ||
                 PredictNextUse(aNow, blockIndex) <= latestUse,
                 "We missed a block that should be replaced");
  }
#endif

  return latestUseBlock;
}

nsMediaCache::BlockList*
nsMediaCache::GetListForBlock(Block* aBlock)
{
  switch (aBlock->mClass) {
  case FREE_BLOCK:
    NS_ASSERTION(!aBlock->mStream, "Free block has a stream?");
    return &mFreeBlocks;
  case METADATA_BLOCK:
    NS_ASSERTION(aBlock->mStream, "Metadata block has no stream?");
    return &mMetadataBlocks;
  case PLAYED_BLOCK:
    NS_ASSERTION(aBlock->mStream, "Metadata block has no stream?");
    return &mPlayedBlocks;
  case READAHEAD_BLOCK:
    NS_ASSERTION(aBlock->mStream, "Readahead block has no stream?");
    return &aBlock->mStream->mReadaheadBlocks;
  default:
    NS_ERROR("Invalid block class");
    return nsnull;
  }
}

void
nsMediaCache::SwapBlocks(PRInt32 aBlockIndex1, PRInt32 aBlockIndex2)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  Block* block1 = &mIndex[aBlockIndex1];
  Block* block2 = &mIndex[aBlockIndex2];

  Block tmp = *block1;
  *block1 = *block2;
  *block2 = tmp;

  // Now all references to block1 have to be replaced with block2 and
  // vice versa

  if (block1->mStream) {
    block1->mStream->mBlocks[block1->mStreamBlock] = aBlockIndex1;
  }
  if (block2->mStream) {
    block2->mStream->mBlocks[block2->mStreamBlock] = aBlockIndex2;
  }

  BlockList* list1 = GetListForBlock(block1);
  list1->NotifyBlockSwapped(aBlockIndex1, aBlockIndex2);
  BlockList* list2 = GetListForBlock(block2);
  // We have to be careful we don't swap the same reference twice!
  if (list1 != list2) {
    list2->NotifyBlockSwapped(aBlockIndex1, aBlockIndex2);
  }

  Verify();
}

void
nsMediaCache::FreeBlock(PRInt32 aBlock)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  Block* block = &mIndex[aBlock];
  GetListForBlock(block)->RemoveBlock(aBlock);
  if (block->mStream) {
    block->mStream->mBlocks[block->mStreamBlock] = -1;
  }
  block->mStream = nsnull;
  block->mClass = FREE_BLOCK;
  mFreeBlocks.AddFirstBlock(aBlock);
  Verify();
}

TimeDuration
nsMediaCache::PredictNextUse(TimeStamp aNow, PRInt32 aBlock)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  Block* block = &mIndex[aBlock];

  switch (block->mClass) {
  case METADATA_BLOCK:
    // This block should be managed in LRU mode. For metadata we predict
    // that the time until the next use is the time since the last use.
    return aNow - block->mLastUseTime;
  case PLAYED_BLOCK:
    // This block should be managed in LRU mode, and we should impose
    // a "replay delay" to reflect the likelihood of replay happening
    NS_ASSERTION(PRInt64(block->mStreamBlock)*BLOCK_SIZE <
                 block->mStream->mStreamOffset,
                 "Played block after the current stream position?");
    return aNow - block->mLastUseTime +
        TimeDuration::FromSeconds(REPLAY_DELAY);
  case READAHEAD_BLOCK: {
    PRInt64 bytesAhead =
      PRInt64(block->mStreamBlock)*BLOCK_SIZE - block->mStream->mStreamOffset;
    NS_ASSERTION(bytesAhead >= 0,
                 "Readahead block before the current stream position?");
    PRInt64 millisecondsAhead =
      bytesAhead*1000/block->mStream->mPlaybackBytesPerSecond;
    return TimeDuration::FromMilliseconds(
        PR_MIN(millisecondsAhead, PR_INT32_MAX));
  }
  default:
    NS_ERROR("Invalid class for predicting next use");
    return TimeDuration(0);
  }
}

TimeDuration
nsMediaCache::PredictNextUseForIncomingData(nsMediaCacheStream* aStream)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  PRInt64 bytesAhead = aStream->mChannelOffset - aStream->mStreamOffset;
  if (bytesAhead <= -BLOCK_SIZE) {
    // Hmm, no idea when data behind us will be used. Guess 24 hours.
    return TimeDuration::FromSeconds(24*60*60);
  }
  if (bytesAhead <= 0)
    return TimeDuration(0);
  PRInt64 millisecondsAhead = bytesAhead*1000/aStream->mPlaybackBytesPerSecond;
  return TimeDuration::FromMilliseconds(
      PR_MIN(millisecondsAhead, PR_INT32_MAX));
}

void
nsMediaCache::Update()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  nsAutoMonitor mon(mMonitor);
  mUpdateQueued = PR_FALSE;
#ifdef DEBUG
  mInUpdate = PR_TRUE;
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
    nsMediaCacheStream* stream = mIndex[blockIndex].mStream;
    if (!stream) {
      // Don't count overflowing free blocks in our free block count
      --freeBlockCount;
      continue;
    }
    TimeDuration predictedUse = PredictNextUse(now, blockIndex);
    latestPredictedUseForOverflow = PR_MAX(latestPredictedUseForOverflow, predictedUse);
  }

  // Now try to move overflowing blocks to the main part of the cache.
  for (PRInt32 blockIndex = mIndex.Length() - 1; blockIndex >= maxBlocks;
       --blockIndex) {
    Block* block = &mIndex[blockIndex];
    nsMediaCacheStream* stream = block->mStream;
    if (!stream)
      continue;

    PRInt32 destinationBlockIndex =
      FindReusableBlock(now, stream, block->mStreamBlock, maxBlocks);
    if (destinationBlockIndex < 0) {
      // Nowhere to place this overflow block. We won't be able to
      // place any more overflow blocks.
      break;
    }

    Block* destinationBlock = &mIndex[destinationBlockIndex];
    if (destinationBlock->mClass == FREE_BLOCK ||
        PredictNextUse(now, destinationBlockIndex) > latestPredictedUseForOverflow) {
      // Reuse blocks in the main part of the cache that are less useful than
      // the least useful overflow blocks
      char buf[BLOCK_SIZE];
      nsresult rv = ReadCacheFileAllBytes(blockIndex*BLOCK_SIZE, buf, sizeof(buf));
      if (NS_SUCCEEDED(rv)) {
        rv = WriteCacheFile(destinationBlockIndex*BLOCK_SIZE, buf, BLOCK_SIZE);
        if (NS_SUCCEEDED(rv)) {
          // We successfully copied the file data.
          LOG(PR_LOG_DEBUG, ("Swapping blocks %d and %d (trimming cache)",
              blockIndex, destinationBlockIndex));
          // Swapping the block metadata here lets us maintain the
          // correct positions in the linked lists
          SwapBlocks(blockIndex, destinationBlockIndex);
        } else {
          // If the write fails we may have corrupted the destination
          // block. Free it now.
          LOG(PR_LOG_DEBUG, ("Released block %d from stream %p block %d(%lld) (trimming cache)",
              destinationBlockIndex, destinationBlock->mStream,
              destinationBlock->mStreamBlock,
              (long long)destinationBlock->mStreamBlock*BLOCK_SIZE));
          FreeBlock(destinationBlockIndex);
        }
        // Free the overflowing block even if the copy failed.
        if (block->mClass != FREE_BLOCK) {
          LOG(PR_LOG_DEBUG, ("Released block %d from stream %p block %d(%lld) (trimming cache)",
              blockIndex, block->mStream, block->mStreamBlock,
              (long long)block->mStreamBlock*BLOCK_SIZE));
          FreeBlock(blockIndex);
        }
      }
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

  // This array holds a list of streams which need to be closed due
  // to fatal errors. We can't close streams immediately since it would
  // confuse iteration over mStreams and generally just be confusing.
  nsTArray<nsMediaCacheStream*> streamsToClose;
  for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
    nsMediaCacheStream* stream = mStreams[i];
    if (stream->mClosed)
      continue;

    // Figure out where we should be reading from. It's normally the first
    // uncached byte after the current mStreamOffset.
    PRInt64 desiredOffset = stream->GetCachedDataEndInternal(stream->mStreamOffset);
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
    PRBool enableReading;
    if (stream->mStreamLength >= 0 &&
        desiredOffset >= stream->mStreamLength) {
      // We're at the end of the stream. Nothing to read, but we don't
      // need to suspend, we may as well just keep reading and hit EOF
      // (or discover more data if the server lied to us).
      LOG(PR_LOG_DEBUG, ("Stream %p at end of stream", stream));
      enableReading = PR_TRUE;
    } else if (desiredOffset < stream->mStreamOffset) {
      // We're reading to try to catch up to where the current stream
      // reader wants to be. Better not stop.
      LOG(PR_LOG_DEBUG, ("Stream %p catching up", stream));
      enableReading = PR_TRUE;
    } else if (desiredOffset < stream->mStreamOffset + BLOCK_SIZE) {
      // The stream reader is waiting for us, or nearly so. Better feed it.
      LOG(PR_LOG_DEBUG, ("Stream %p feeding reader", stream));
      enableReading = PR_TRUE;
    } else if (!stream->mIsSeekable &&
               nonSeekableReadaheadBlockCount >= maxBlocks*NONSEEKABLE_READAHEAD_MAX) {
      // This stream is not seekable and there are already too many blocks
      // being cached for readahead for nonseekable streams (which we can't
      // free). So stop reading ahead now.
      LOG(PR_LOG_DEBUG, ("Stream %p throttling non-seekable readahead", stream));
      enableReading = PR_FALSE;
    } else if (mIndex.Length() > PRUint32(maxBlocks)) {
      // We're in the process of bringing the cache size back to the
      // desired limit, so don't bring in more data yet
      LOG(PR_LOG_DEBUG, ("Stream %p throttling to reduce cache size", stream));
      enableReading = PR_FALSE;
    } else if (freeBlockCount > 0 || mIndex.Length() < PRUint32(maxBlocks)) {
      // Free blocks in the cache, so keep reading
      LOG(PR_LOG_DEBUG, ("Stream %p reading since there are free blocks", stream));
      enableReading = PR_TRUE;
    } else if (latestNextUse <= TimeDuration(0)) {
      // No reusable blocks, so can't read anything
      LOG(PR_LOG_DEBUG, ("Stream %p throttling due to no reusable blocks", stream));
      enableReading = PR_FALSE;
    } else {
      // Read ahead if the data we expect to read is more valuable than
      // the least valuable block in the main part of the cache
      TimeDuration predictedNewDataUse = PredictNextUseForIncomingData(stream);
      LOG(PR_LOG_DEBUG, ("Stream %p predict next data in %f, current worst block is %f",
          stream, predictedNewDataUse.ToSeconds(), latestNextUse.ToSeconds()));
      enableReading = predictedNewDataUse < latestNextUse;
    }

    nsresult rv = NS_OK;
    if (stream->mChannelOffset != desiredOffset && enableReading) {
      // We need to seek now.
      NS_ASSERTION(stream->mIsSeekable || desiredOffset == 0,
                   "Trying to seek in a non-seekable stream!");
      // Round seek offset down to the start of the block
      stream->mChannelOffset = (desiredOffset/BLOCK_SIZE)*BLOCK_SIZE;
      LOG(PR_LOG_DEBUG, ("Stream %p CacheSeek to %lld (resume=%d)", stream,
          (long long)stream->mChannelOffset, stream->mCacheSuspended));
      rv = stream->mClient->CacheClientSeek(stream->mChannelOffset,
                                            stream->mCacheSuspended);
      stream->mCacheSuspended = PR_FALSE;
    } else if (enableReading && stream->mCacheSuspended) {
      LOG(PR_LOG_DEBUG, ("Stream %p Resumed", stream));
      rv = stream->mClient->CacheClientResume();
      stream->mCacheSuspended = PR_FALSE;
    } else if (!enableReading && !stream->mCacheSuspended) {
      LOG(PR_LOG_DEBUG, ("Stream %p Suspended", stream));
      rv = stream->mClient->CacheClientSuspend();
      stream->mCacheSuspended = PR_TRUE;
    }

    if (NS_FAILED(rv)) {
      streamsToClose.AppendElement(stream);
    }
  }

  // Close the streams that failed due to error. This will cause all
  // client Read and Seek operations on those streams to fail. Blocked
  // Reads will also be woken up.
  for (PRUint32 i = 0; i < streamsToClose.Length(); ++i) {
    streamsToClose[i]->CloseInternal(&mon);
  }
#ifdef DEBUG
  mInUpdate = PR_FALSE;
#endif
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
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  // Queuing an update while we're in an update raises a high risk of
  // triggering endless events
  NS_ASSERTION(!mInUpdate,
               "Queuing an update while we're in an update");
  if (mUpdateQueued)
    return;
  mUpdateQueued = PR_TRUE;
  nsCOMPtr<nsIRunnable> event = new UpdateEvent();
  NS_DispatchToMainThread(event);
}

#ifdef DEBUG_VERIFY_CACHE
void
nsMediaCache::Verify()
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  mFreeBlocks.Verify();
  mPlayedBlocks.Verify();
  mMetadataBlocks.Verify();
  for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
    nsMediaCacheStream* stream = mStreams[i];
    stream->mReadaheadBlocks.Verify();
    if (!stream->mReadaheadBlocks.IsEmpty()) {
      // Verify that the readahead blocks are listed in stream block order
      PRInt32 block = stream->mReadaheadBlocks.GetFirstBlock();
      PRInt32 lastStreamBlock = -1;
      do {
        NS_ASSERTION(mIndex[block].mStream == stream, "Bad stream");
        NS_ASSERTION(lastStreamBlock < PRInt32(mIndex[block].mStreamBlock),
                     "Blocks not increasing in readahead stream");
        lastStreamBlock = PRInt32(mIndex[block].mStreamBlock);
        block = stream->mReadaheadBlocks.GetNextBlock(block);
      } while (block >= 0);
    }
  }
}
#endif

void
nsMediaCache::InsertReadaheadBlock(PRInt32 aBlockIndex)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  Block* block = &mIndex[aBlockIndex];
  nsMediaCacheStream* stream = block->mStream;
  if (stream->mReadaheadBlocks.IsEmpty()) {
    stream->mReadaheadBlocks.AddFirstBlock(aBlockIndex);
    return;
  }

  // Find the last block whose stream block is before aBlockIndex's
  // stream block, and insert after it
  PRInt32 readaheadIndex = stream->mReadaheadBlocks.GetLastBlock();
  do {
    if (mIndex[readaheadIndex].mStreamBlock < block->mStreamBlock) {
      stream->mReadaheadBlocks.AddAfter(aBlockIndex, readaheadIndex);
      return;
    }
    NS_ASSERTION(mIndex[readaheadIndex].mStreamBlock > block->mStreamBlock,
                 "Duplicated blocks??");
    readaheadIndex = stream->mReadaheadBlocks.GetPrevBlock(readaheadIndex);
  } while (readaheadIndex >= 0);
  stream->mReadaheadBlocks.AddFirstBlock(aBlockIndex);
  Verify();
}

void
nsMediaCache::AllocateAndWriteBlock(nsMediaCacheStream* aStream, const void* aData,
                                    nsMediaCacheStream::ReadMode aMode)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  PRInt32 streamBlockIndex = aStream->mChannelOffset/BLOCK_SIZE;
  // Extend the mBlocks array as necessary
  while (streamBlockIndex >= PRInt32(aStream->mBlocks.Length())) {
    aStream->mBlocks.AppendElement(-1);
  }
  if (aStream->mBlocks[streamBlockIndex] >= 0) {
    // Release the existing cache entry for this stream block
    PRInt32 globalBlockIndex = aStream->mBlocks[streamBlockIndex];
    LOG(PR_LOG_DEBUG, ("Released block %d from stream %p block %d(%lld)",
        globalBlockIndex, aStream, streamBlockIndex, (long long)streamBlockIndex*BLOCK_SIZE));
    FreeBlock(globalBlockIndex);
  }

  TimeStamp now = TimeStamp::Now();
  PRInt32 blockIndex = FindBlockForIncomingData(now, aStream);
  if (blockIndex >= 0) {
    Block* block = &mIndex[blockIndex];
    if (block->mClass != FREE_BLOCK) {
      LOG(PR_LOG_DEBUG, ("Released block %d from stream %p block %d(%lld)",
          blockIndex, block->mStream, block->mStreamBlock, (long long)block->mStreamBlock*BLOCK_SIZE));
      FreeBlock(blockIndex);
    }
    NS_ASSERTION(block->mClass == FREE_BLOCK, "Block should be free now!");

    LOG(PR_LOG_DEBUG, ("Allocated block %d to stream %p block %d(%lld)",
        blockIndex, aStream, streamBlockIndex, (long long)streamBlockIndex*BLOCK_SIZE));
    block->mStream = aStream;
    block->mStreamBlock = streamBlockIndex;
    block->mLastUseTime = now;
    aStream->mBlocks[streamBlockIndex] = blockIndex;
    mFreeBlocks.RemoveBlock(blockIndex);
    if (streamBlockIndex*BLOCK_SIZE < aStream->mStreamOffset) {
      block->mClass = aMode == nsMediaCacheStream::MODE_PLAYBACK
        ? PLAYED_BLOCK : METADATA_BLOCK;
      // This must be the most-recently-used block, since we
      // marked it as used now (which may be slightly bogus, but we'll
      // treat it as used for simplicity).
      GetListForBlock(block)->AddFirstBlock(blockIndex);
      Verify();
    } else {
      // This may not be the latest readahead block, although it usually
      // will be. We may have to scan for the right place to insert
      // the block in the list.
      block->mClass = READAHEAD_BLOCK;
      InsertReadaheadBlock(blockIndex);
    }

    nsresult rv = WriteCacheFile(blockIndex*BLOCK_SIZE, aData, BLOCK_SIZE);
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

  nsAutoMonitor mon(mMonitor);
  mStreams.AppendElement(aStream);
}

void
nsMediaCache::ReleaseStream(nsMediaCacheStream* aStream)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  nsAutoMonitor mon(mMonitor);
  mStreams.RemoveElement(aStream);
}

void
nsMediaCache::ReleaseStreamBlocks(nsMediaCacheStream* aStream)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  // XXX scanning the entire stream doesn't seem great, if not much of it
  // is cached, but the only easy alternative is to scan the entire cache
  // which isn't better
  PRUint32 length = aStream->mBlocks.Length();
  for (PRUint32 i = 0; i < length; ++i) {
    PRInt32 blockIndex = aStream->mBlocks[i];
    if (blockIndex >= 0) {
      LOG(PR_LOG_DEBUG, ("Released block %d from stream %p block %d(%lld)",
          blockIndex, aStream, i, (long long)i*BLOCK_SIZE));
      FreeBlock(blockIndex);
    }
  }
}

void
nsMediaCache::Truncate()
{
  PRUint32 end;
  for (end = mIndex.Length(); end > 0; --end) {
    if (mIndex[end - 1].mStream)
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
nsMediaCache::NoteBlockUsage(PRInt32 aBlockIndex,
                             nsMediaCacheStream::ReadMode aMode,
                             TimeStamp aNow)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  if (aBlockIndex < 0) {
    // this block is not in the cache yet
    return;
  }

  Block* block = &mIndex[aBlockIndex];
  if (block->mClass == FREE_BLOCK) {
    // this block is not in the cache yet
    return;
  }

  // The following check has to be <= because the stream offset has
  // not yet been updated for the data read from this block
  NS_ASSERTION(block->mStreamBlock*BLOCK_SIZE <= block->mStream->mStreamOffset,
               "Using a block that's behind the read position?");

  GetListForBlock(block)->RemoveBlock(aBlockIndex);
  block->mClass =
    (aMode == nsMediaCacheStream::MODE_METADATA || block->mClass == METADATA_BLOCK)
    ? METADATA_BLOCK : PLAYED_BLOCK;
  // Since this is just being used now, it can definitely be at the front
  // of mMetadataBlocks or mPlayedBlocks
  GetListForBlock(block)->AddFirstBlock(aBlockIndex);
  block->mLastUseTime = aNow;
  Verify();
}

void
nsMediaCache::NoteSeek(nsMediaCacheStream* aStream, PRInt64 aOldOffset)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  if (aOldOffset < aStream->mStreamOffset) {
    // We seeked forward. Convert blocks from readahead to played.
    // Any readahead block that intersects the seeked-over range must
    // be converted.
    PRInt32 blockIndex = aOldOffset/BLOCK_SIZE;
    PRInt32 endIndex =
      PR_MIN((aStream->mStreamOffset + BLOCK_SIZE - 1)/BLOCK_SIZE,
             aStream->mBlocks.Length());
    TimeStamp now = TimeStamp::Now();
    while (blockIndex < endIndex) {
      PRInt32 cacheBlockIndex = aStream->mBlocks[blockIndex];
      if (cacheBlockIndex >= 0) {
        // Marking the block used may not be exactly what we want but
        // it's simple
        NoteBlockUsage(cacheBlockIndex, nsMediaCacheStream::MODE_PLAYBACK,
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
      PR_MIN((aOldOffset + BLOCK_SIZE - 1)/BLOCK_SIZE,
             aStream->mBlocks.Length());
    while (blockIndex < endIndex) {
      PRInt32 cacheBlockIndex = aStream->mBlocks[endIndex - 1];
      if (cacheBlockIndex >= 0) {
        Block* block = &mIndex[cacheBlockIndex];
        if (block->mClass != METADATA_BLOCK) {
          mPlayedBlocks.RemoveBlock(cacheBlockIndex);
          block->mClass = READAHEAD_BLOCK;
          // Adding this as the first block is sure to be OK since
          // this must currently be the earliest readahead block
          // (that's why we're proceeding backwards from the end of
          // the seeked range to the start)
          GetListForBlock(block)->AddFirstBlock(cacheBlockIndex);
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

  nsAutoMonitor mon(gMediaCache->Monitor());
  mStreamLength = aLength;
}

void
nsMediaCacheStream::NotifyDataStarted(PRInt64 aOffset) 
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  nsAutoMonitor mon(gMediaCache->Monitor());
  NS_WARN_IF_FALSE(aOffset == mChannelOffset,
                   "Server is giving us unexpected offset");
  mChannelOffset = aOffset;
  if (mStreamLength >= 0) {
    // If we started reading at a certain offset, then for sure
    // the stream is at least that long.
    mStreamLength = PR_MAX(mStreamLength, mChannelOffset);
  }
}

void
nsMediaCacheStream::UpdatePrincipal(nsIPrincipal* aPrincipal)
{
  if (!mPrincipal) {
    NS_ASSERTION(!mUsingNullPrincipal, "Are we using a null principal or not?");
    if (mUsingNullPrincipal) {
      // Don't let mPrincipal be set to anything
      return;
    }
    mPrincipal = aPrincipal;
    return;
  }

  if (mPrincipal == aPrincipal) {
    // Common case
    NS_ASSERTION(!mUsingNullPrincipal, "We can't receive data from a null principal");
    return;
  }
  if (mUsingNullPrincipal) {
    // We've already fallen back to a null principal, so nothing more
    // to do.
    return;
  }

  PRBool equal;
  nsresult rv = mPrincipal->Equals(aPrincipal, &equal);
  if (NS_SUCCEEDED(rv) && equal)
    return;

  // Principals are not equal, so set mPrincipal to a null principal.
  mPrincipal = do_CreateInstance("@mozilla.org/nullprincipal;1");
  mUsingNullPrincipal = PR_TRUE;
}

void
nsMediaCacheStream::NotifyDataReceived(PRInt64 aSize, const char* aData,
    nsIPrincipal* aPrincipal)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  UpdatePrincipal(aPrincipal);

  nsAutoMonitor mon(gMediaCache->Monitor());
  PRInt64 size = aSize;
  const char* data = aData;

  LOG(PR_LOG_DEBUG, ("Stream %p DataReceived at %lld count=%lld",
      this, (long long)mChannelOffset, (long long)aSize));

  // We process the data one block (or part of a block) at a time
  while (size > 0) {
    PRUint32 blockIndex = mChannelOffset/BLOCK_SIZE;
    PRInt32 blockOffset = PRInt32(mChannelOffset - blockIndex*BLOCK_SIZE);
    PRInt32 chunkSize = PRInt32(PR_MIN(BLOCK_SIZE - blockOffset, size));

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
        mMetadataInPartialBlockBuffer = PR_FALSE;
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
    if (mStreamLength >= 0) {
      // The stream is at least as long as what we've read
      mStreamLength = PR_MAX(mStreamLength, mChannelOffset);
    }
    size -= chunkSize;
    data += chunkSize;
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

  nsAutoMonitor mon(gMediaCache->Monitor());
  if (NS_SUCCEEDED(aStatus)) {
    // We read the whole stream, so remember the true length
    mStreamLength = mChannelOffset;
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
}

nsMediaCacheStream::~nsMediaCacheStream()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  NS_ASSERTION(mClosed, "Stream was not closed");
  NS_ASSERTION(!mPinCount, "Unbalanced Pin");

  gMediaCache->ReleaseStream(this);
  nsMediaCache::MaybeShutdown();
}

void
nsMediaCacheStream::SetSeekable(PRBool aIsSeekable)
{
  nsAutoMonitor mon(gMediaCache->Monitor());
  NS_ASSERTION(mIsSeekable || aIsSeekable ||
               mChannelOffset == 0, "channel offset must be zero when we become non-seekable");
  mIsSeekable = aIsSeekable;
  // Queue an Update since we may change our strategy for dealing
  // with this stream
  gMediaCache->QueueUpdate();
}

PRBool
nsMediaCacheStream::IsSeekable()
{
  nsAutoMonitor mon(gMediaCache->Monitor());
  return mIsSeekable;
}

void
nsMediaCacheStream::Close()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  nsAutoMonitor mon(gMediaCache->Monitor());
  CloseInternal(&mon);
  // Queue an Update since we may have created more free space. Don't do
  // it from CloseInternal since that gets called by Update() itself
  // sometimes, and we try to not to queue updates from Update().
  gMediaCache->QueueUpdate();
}

void
nsMediaCacheStream::CloseInternal(nsAutoMonitor* aMonitor)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  if (mClosed)
    return;
  mClosed = PR_TRUE;
  gMediaCache->ReleaseStreamBlocks(this);
  // Wake up any blocked readers
  aMonitor->NotifyAll();
}

void
nsMediaCacheStream::Pin()
{
  nsAutoMonitor mon(gMediaCache->Monitor());
  ++mPinCount;
  // Queue an Update since we may no longer want to read more into the
  // cache, if this stream's block have become non-evictable
  gMediaCache->QueueUpdate();
}

void
nsMediaCacheStream::Unpin()
{
  nsAutoMonitor mon(gMediaCache->Monitor());
  NS_ASSERTION(mPinCount > 0, "Unbalanced Unpin");
  --mPinCount;
  // Queue an Update since we may be able to read more into the
  // cache, if this stream's block have become evictable
  gMediaCache->QueueUpdate();
}

PRInt64
nsMediaCacheStream::GetLength()
{
  nsAutoMonitor mon(gMediaCache->Monitor());
  return mStreamLength;
}

PRInt64
nsMediaCacheStream::GetNextCachedData(PRInt64 aOffset)
{
  nsAutoMonitor mon(gMediaCache->Monitor());
  return GetNextCachedDataInternal(aOffset);
}

PRInt64
nsMediaCacheStream::GetCachedDataEnd(PRInt64 aOffset)
{
  nsAutoMonitor mon(gMediaCache->Monitor());
  return GetCachedDataEndInternal(aOffset);
}

PRBool
nsMediaCacheStream::IsDataCachedToEndOfStream(PRInt64 aOffset)
{
  nsAutoMonitor mon(gMediaCache->Monitor());
  if (mStreamLength < 0)
    return PR_FALSE;
  return GetCachedDataEndInternal(aOffset) >= mStreamLength;
}

PRInt64
nsMediaCacheStream::GetCachedDataEndInternal(PRInt64 aOffset)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(gMediaCache->Monitor());
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
    result = PR_MIN(result, mStreamLength);
  }
  return PR_MAX(result, aOffset);
}

PRInt64
nsMediaCacheStream::GetNextCachedDataInternal(PRInt64 aOffset)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(gMediaCache->Monitor());
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
  PRBool hasPartialBlock = (mChannelOffset % BLOCK_SIZE) != 0;
  PRUint32 blockIndex = startBlockIndex + 1;
  while (PR_TRUE) {
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
  nsAutoMonitor mon(gMediaCache->Monitor());
  if (aMode == mCurrentMode)
    return;
  mCurrentMode = aMode;
  gMediaCache->QueueUpdate();
}

void
nsMediaCacheStream::SetPlaybackRate(PRUint32 aBytesPerSecond)
{
  NS_ASSERTION(aBytesPerSecond > 0, "Zero playback rate not allowed");
  nsAutoMonitor mon(gMediaCache->Monitor());
  if (aBytesPerSecond == mPlaybackBytesPerSecond)
    return;
  mPlaybackBytesPerSecond = aBytesPerSecond;
  gMediaCache->QueueUpdate();
}

nsresult
nsMediaCacheStream::Seek(PRInt32 aWhence, PRInt64 aOffset)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  nsAutoMonitor mon(gMediaCache->Monitor());
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

  nsAutoMonitor mon(gMediaCache->Monitor());
  return mStreamOffset;
}

nsresult
nsMediaCacheStream::Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  nsAutoMonitor mon(gMediaCache->Monitor());
  if (mClosed)
    return NS_ERROR_FAILURE;

  PRUint32 count = 0;
  // Read one block (or part of a block) at a time
  while (count < aCount) {
    PRUint32 streamBlock = PRUint32(mStreamOffset/BLOCK_SIZE);
    PRUint32 offsetInStreamBlock =
      PRUint32(mStreamOffset - streamBlock*BLOCK_SIZE);
    PRInt32 size = PR_MIN(aCount - count, BLOCK_SIZE - offsetInStreamBlock);

    if (mStreamLength >= 0) {
      // Don't try to read beyond the end of the stream
      PRInt64 bytesRemaining = mStreamLength - mStreamOffset;
      if (bytesRemaining <= 0) {
        // Get out of here and return NS_OK
        break;
      }
      size = PR_MIN(size, PRInt32(bytesRemaining));
    }

    PRInt32 bytes;
    PRUint32 channelBlock = PRUint32(mChannelOffset/BLOCK_SIZE);
    PRInt32 cacheBlock = streamBlock < mBlocks.Length() ? mBlocks[streamBlock] : -1;
    if (channelBlock == streamBlock && mStreamOffset < mChannelOffset) {
      // We can just use the data in mPartialBlockBuffer. In fact we should
      // use it rather than waiting for the block to fill and land in
      // the cache.
      bytes = PR_MIN(size, mChannelOffset - mStreamOffset);
      memcpy(aBuffer + count,
        reinterpret_cast<char*>(mPartialBlockBuffer) + offsetInStreamBlock, bytes);
      if (mCurrentMode == MODE_METADATA) {
        mMetadataInPartialBlockBuffer = PR_TRUE;
      }
      gMediaCache->NoteBlockUsage(cacheBlock, mCurrentMode, TimeStamp::Now());
    } else {
      if (cacheBlock < 0) {
        if (count > 0) {
          // Some data has been read, so return what we've got instead of
          // blocking
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

      gMediaCache->NoteBlockUsage(cacheBlock, mCurrentMode, TimeStamp::Now());

      PRInt64 offset = cacheBlock*BLOCK_SIZE + offsetInStreamBlock;
      nsresult rv = gMediaCache->ReadCacheFile(offset, aBuffer + count, size, &bytes);
      if (NS_FAILED(rv)) {
        if (count == 0)
          return rv;
        // If we did successfully read some data, may as well return it
        break;
      }
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
  nsAutoMonitor mon(gMediaCache->Monitor());
  if (mClosed)
    return NS_ERROR_FAILURE;

  // Read one block (or part of a block) at a time
  PRUint32 count = 0;
  PRInt64 streamOffset = aOffset;
  while (count < aCount) {
    PRUint32 streamBlock = PRUint32(streamOffset/BLOCK_SIZE);
    PRUint32 offsetInStreamBlock =
      PRUint32(streamOffset - streamBlock*BLOCK_SIZE);
    PRInt32 size = PR_MIN(aCount - count, BLOCK_SIZE - offsetInStreamBlock);

    if (mStreamLength >= 0) {
      // Don't try to read beyond the end of the stream
      PRInt64 bytesRemaining = mStreamLength - streamOffset;
      if (bytesRemaining <= 0) {
        return NS_ERROR_FAILURE;
      }
      size = PR_MIN(size, PRInt32(bytesRemaining));
    }

    PRInt32 bytes;
    PRUint32 channelBlock = PRUint32(mChannelOffset/BLOCK_SIZE);
    PRInt32 cacheBlock = streamBlock < mBlocks.Length() ? mBlocks[streamBlock] : -1;
    if (channelBlock == streamBlock && streamOffset < mChannelOffset) {
      // We can just use the data in mPartialBlockBuffer. In fact we should
      // use it rather than waiting for the block to fill and land in
      // the cache.
      bytes = PR_MIN(size, mChannelOffset - streamOffset);
      memcpy(aBuffer + count,
        reinterpret_cast<char*>(mPartialBlockBuffer) + offsetInStreamBlock, bytes);
    } else {
      if (cacheBlock < 0) {
        // We expect all blocks to be cached! Fail!
        return NS_ERROR_FAILURE;
      }
      PRInt64 offset = cacheBlock*BLOCK_SIZE + offsetInStreamBlock;
      nsresult rv = gMediaCache->ReadCacheFile(offset, aBuffer + count, size, &bytes);
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

  InitMediaCache();
  if (!gMediaCache)
    return NS_ERROR_FAILURE;
  gMediaCache->OpenStream(this);
  return NS_OK;
}
