/* -*- Mode: C++;    tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-  */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include <MacMemory.h>

#include "nsMemAllocator.h"
#include "nsAllocatorManager.h"


//--------------------------------------------------------------------
nsHeapChunk::nsHeapChunk(nsMemAllocator *inOwningAllocator, Size heapSize)
:   mOwningAllocator(inOwningAllocator)
,   mNextChunk(nil)
,   mHeapSize(heapSize)
,   mUsedBlocks(0)
#if DEBUG_HEAP_INTEGRITY    
,   mSignature(kChunkSignature)
#endif
//--------------------------------------------------------------------
{

}


//--------------------------------------------------------------------
nsHeapChunk::~nsHeapChunk()
//--------------------------------------------------------------------
{
}


#pragma mark -

//--------------------------------------------------------------------
nsMemAllocator::nsMemAllocator(EAllocatorType allocatorType, size_t minBlockSize, size_t maxBlockSize)
:   mAllocatorType(allocatorType)
,   mFirstChunk(nil)
,   mLastChunk(nil)
,   mNumChunks(0)
,   mMinBlockSize(minBlockSize)
,   mMaxBlockSize(maxBlockSize)
#if DEBUG_HEAP_INTEGRITY    
,   mSignature(kMemAllocatorSignature)
#endif
#if STATS_MAC_MEMORY
,   mCurBlockCount(0)
,   mMaxBlockCount(0)
,   mCurBlockSpaceUsed(0)
,   mMaxBlockSpaceUsed(0)
,   mCurHeapSpaceUsed(0)
,   mMaxHeapSpaceUsed(0)
,   mCurSubheapCount(0)
,   mMaxSubheapCount(0)
, mCountHistogram(NULL)
#endif
//--------------------------------------------------------------------
{
#if STATS_MAC_MEMORY
  mCountHistogram = (UInt32 *)NewPtrClear(sizeof(UInt32) * (maxBlockSize - minBlockSize + 1));
  // failure is ok
#endif
}

//--------------------------------------------------------------------
nsMemAllocator::~nsMemAllocator()
//--------------------------------------------------------------------
{
#if STATS_MAC_MEMORY
  if (mCountHistogram)
    DisposePtr((Ptr)mCountHistogram);
#endif
}


#pragma mark -


//--------------------------------------------------------------------
void nsMemAllocator::AddToChunkList(nsHeapChunk *inNewChunk)
//--------------------------------------------------------------------
{
    if (mLastChunk)
        mLastChunk->SetNextChunk(inNewChunk);
    else
        mFirstChunk = inNewChunk;

    mLastChunk = inNewChunk;

    mNumChunks ++;
    
#if STATS_MAC_MEMORY
    mCurSubheapCount ++;
    if (mCurSubheapCount > mMaxSubheapCount)
        mMaxSubheapCount = mCurSubheapCount;
        
    mCurHeapSpaceUsed += inNewChunk->GetChunkSize();
    if (mCurHeapSpaceUsed > mMaxHeapSpaceUsed)
        mMaxHeapSpaceUsed = mCurHeapSpaceUsed;
#endif
}


//--------------------------------------------------------------------
void nsMemAllocator::RemoveFromChunkList(nsHeapChunk *inChunk)
//--------------------------------------------------------------------
{
    nsHeapChunk     *prevChunk = nil;
    nsHeapChunk     *nextChunk = nil;
    nsHeapChunk     *thisChunk = mFirstChunk;
        
    while (thisChunk)
    {
        nextChunk = thisChunk->GetNextChunk();
        
        if (thisChunk == inChunk)
            break;
        
        prevChunk = thisChunk;
        thisChunk = nextChunk;
    }
    
    if (thisChunk)
    {
        if (prevChunk)
            prevChunk->SetNextChunk(nextChunk);
        
        if (mFirstChunk == thisChunk)
            mFirstChunk = nextChunk;
        
        if (mLastChunk == thisChunk)
            mLastChunk = prevChunk;
    }
    
    mNumChunks --;
    
#if STATS_MAC_MEMORY
    mCurSubheapCount --;
    
    mCurHeapSpaceUsed -= inChunk->GetChunkSize();
#endif
}


#pragma mark -

#if STATS_MAC_MEMORY

//--------------------------------------------------------------------
void nsMemAllocator::AccountForNewBlock(size_t logicalSize)
//--------------------------------------------------------------------
{
    mCurBlockCount ++;
    
    if (mCurBlockCount > mMaxBlockCount)
        mMaxBlockCount = mCurBlockCount;
        
    mCurBlockSpaceUsed += logicalSize;
    
    if (mCurBlockSpaceUsed > mMaxBlockSpaceUsed)
        mMaxBlockSpaceUsed = mCurBlockSpaceUsed;

  if (mCountHistogram)
  {
    mCountHistogram[logicalSize - mMinBlockSize]++;
  }
}

//--------------------------------------------------------------------
void nsMemAllocator::AccountForResizedBlock(size_t oldLogicalSize, size_t newLogicalSize)
//--------------------------------------------------------------------
{
    mCurBlockSpaceUsed -= oldLogicalSize;
    mCurBlockSpaceUsed += newLogicalSize;
    
    if (mCurBlockSpaceUsed > mMaxBlockSpaceUsed)
        mMaxBlockSpaceUsed = mCurBlockSpaceUsed;

}

//--------------------------------------------------------------------
void nsMemAllocator::AccountForFreedBlock(size_t logicalSize)
//--------------------------------------------------------------------
{
    mCurBlockCount --;
    mCurBlockSpaceUsed -= logicalSize;
}

//--------------------------------------------------------------------
void nsMemAllocator::DumpHeapUsage(PRFileDesc *outFile)
//--------------------------------------------------------------------
{
    char            outString[ 1024 ];
    
    sprintf(outString, "%04ld ", mMaxBlockSize);
    
    WriteString(outFile, outString);

    char    *p = outString;
    SInt32  numStars = mMaxHeapSpaceUsed / 1024;
    if (numStars > 1021)
        numStars = 1021;
    
    for (SInt32 i = 0; i < numStars; i ++)
        *p++ = '*';
    
    if (numStars == 1021)
        *p++ = 'É';
    *p++ = '\n';
    *p = '\0';
    
    WriteString(outFile, outString);
}

//--------------------------------------------------------------------
void nsMemAllocator::DumpMemoryStats(PRFileDesc *outFile)
//--------------------------------------------------------------------
{
    char            outString[ 1024 ];
    
    sprintf(outString, "Stats for heap of blocks %ld - %ld bytes\n", mMinBlockSize, mMaxBlockSize);
    
    WriteString ( outFile, "\n\n--------------------------------------------------------------------------------\n" );
    WriteString(outFile, outString);
    WriteString ( outFile, "--------------------------------------------------------------------------------\n" );
    WriteString ( outFile, "                     Current         Max\n" );
    WriteString ( outFile, "                  ----------     -------\n" );
    sprintf( outString,    "Num chunks:       %10d  %10d\n", mCurSubheapCount, mMaxSubheapCount);
    WriteString ( outFile, outString );
    sprintf( outString,    "Chunk total:      %10d  %10d\n", mCurHeapSpaceUsed, mMaxHeapSpaceUsed);
    WriteString ( outFile, outString );
    sprintf( outString,    "Block space:      %10d  %10d\n", mCurBlockSpaceUsed, mMaxBlockSpaceUsed );
    WriteString ( outFile, outString );
    sprintf( outString,    "Blocks used:      %10d  %10d\n", mCurBlockCount, mMaxBlockCount );
    WriteString ( outFile, outString );
    WriteString ( outFile, "                                 -------\n" );
    sprintf( outString,    "%s of allocated space used:    %10.2f\n", "%", 100.0 * mMaxBlockSpaceUsed / mMaxHeapSpaceUsed );
    WriteString ( outFile, outString );

 
  if (mCountHistogram)
  {
    WriteString ( outFile, "\n\n");
    WriteString ( outFile, "Block size   Total allocations\n------------------------------\n" );

    for (UInt32 i = mMinBlockSize; i <= mMaxBlockSize; i ++)
    {
        sprintf(outString,"%5d  %10d\n", i, mCountHistogram[i - mMinBlockSize]);
      WriteString ( outFile, outString );
    }
    
    WriteString(outFile, "------------------------------\n");
  }
  
    WriteString ( outFile, "\n\n");
}

#endif
