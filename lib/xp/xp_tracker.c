/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#ifdef XP_MAC
#include "TypesAndSwitches.h"

#if DEBUG_MAC_MEMORY
#define	BUILD_TRACKER	1
#endif

#include <Types.h>
#include <unistd.h>
#include <fcntl.h>
#endif /* XP_MAC */


#include <stdio.h>
#include <string.h>
#ifndef NSPR20
#include "prglobal.h"
#include "prfile.h"
#endif
#include "xp_tracker.h"



/*
 * These should all be runtime options.
 */
#define	LOG_ALL_OPEN_SETS		1
//#define	LOG_SITE_LEAKS_ONLY		1

/*
 * Considering that this is meant to help us reduce memory consumption, my use of memory
 * here is pretty appalling. However, I think optimizations here are only worth the time
 * if they're needed. The most important thing is to get this working quickly and start
 * using it. Then I'll come back and fix this other stuff up.
 */
 
#define	kNumBlocksTracked			100000
#define	kBlockAllocationLongs		(( kNumBlocksTracked + 31 ) >> 5 )
#define	kHashMod					1091
#define	kNumAllocationSites			30000
#define	kNumAllocationSets			20
#define	kInvalidIndex				0xFFFFFFFF


/*
 * Hash Table crap
 */
typedef struct HashBlock HashBlock;
typedef struct HashTable HashTable;

typedef unsigned long (*HashFunction)( void * key );
typedef Boolean (*HashCompare) ( void * key1, void *key2 );


struct HashBlock {
	struct HashBlock *	next;
	char				key[];
};

struct HashTable {
	HashFunction	hashFunction;
	HashCompare		compareFunction;
	HashBlock *		index[ kHashMod ];
};



static void HashInitialize ( HashTable * table, HashFunction hash, HashCompare compare );
static HashBlock * HashFind ( HashTable * table, void * key );
static void HashInsert ( HashTable * table, HashBlock * block );
static void HashRemove ( HashTable * table, HashBlock * block );


/*
 * Internal tracker data structures and constants
 */


// decoder table
#define	kNumDecoderTypes	10

typedef struct DecoderTable {
	UInt32		decoderTag[ kNumDecoderTypes ];
	DecoderProc	decoderProc[ kNumDecoderTypes ];
} DecoderTable;

typedef struct AllocationSite AllocationSite;

typedef struct Block {
	unsigned long		blockNum;
	unsigned long		blockID;
	unsigned long		blockSize;
	void *				blockAddress;
	AllocationSite *	site;
	unsigned char		blockState;
	unsigned char		refCount;
	unsigned char		overhead;
	unsigned char		pad;
	struct Block *		next;
} Block;

struct AllocationSet {
	unsigned long	numBlocks;
	unsigned long	totalAllocation;
	unsigned long	currentAllocation;
	unsigned long	maxAllocation;
	unsigned long	blockSet[ kBlockAllocationLongs ];
	unsigned char	inUse;
	unsigned char	enabledState;
	char			name[ 256 ];
};

typedef Block * Bucket;

struct AllocationSite {
	AllocationSite *	next;
	
	/* This is gross. The next two fields need to be in this order as the hash */
	/* key function depends on it */
	unsigned long		tag;
	void *				stackCrawl[ kStackDepth ];
	
	unsigned long		siteIndex;
	unsigned long		currentBlocks;
	unsigned long		currentMemUsed;
	unsigned long		maxMemUsed;
	unsigned long		maxBlocks;
	unsigned long		totalBlocks;
	unsigned long		totalMemUsed;
};

AllocationSite * NewAllocationSite ( void ** stackCrawl, UInt32 tag, UInt32 blockSize );


#if BUILD_TRACKER
Block				gBlockPool[ kNumBlocksTracked ];
Block *				gFreeBlocks;
Block *				gBlockIndex[ kHashMod ];
unsigned long		gIndexSize;
unsigned long		gBlockNumber;
unsigned long		gNumActiveTrackerBlocks;
unsigned long		gNumHeapBlocks;
unsigned long		gMaxNumHeapBlocks;
unsigned long		gNumAllocations;
AllocationSite		gAllocationSites[ kNumAllocationSites ];
unsigned long		gAllocationSitesUsed;
HashTable			gSiteTable;
AllocationSet		gAllocationSetPool[ kNumAllocationSets ];
unsigned char		gTrackerInitialized = false;
Boolean				gNeverInitialized = true;
unsigned long		gTrackerEnableState = 0;
DecoderTable		gDecoderTable;
HistogramLog		gSizeHistogram;

long				gNumActiveSets = 0;
long				gNumEnabledSets = 0;

static PRFileHandle	gLogFile = NULL;

#endif


static Block * AllocateBlockFromPool ( void );
static void FreeBlockFromPool ( Block * block );
static Block * FindBlock ( void * address );
static void InsertBlock ( Block * block );
static void RemoveBlock ( Block * block );
static Block * FindBlockBucket ( void * address, Bucket ** bucket );
static void AddBlockReference ( Block * block );
static void RemoveBlockReference ( Block * block );

static void AddNewBlockToAllocationSet ( AllocationSet * set, Block * block );
static void MarkBlockFreeFromSet ( AllocationSet * set, Block * block );
static void BlockFreedFromSite ( AllocationSite * site, unsigned long blockSize );


static unsigned long AllocationSiteHash( void ** stackCrawl );
static Boolean AllocationSiteHashCompare ( void ** site1, void ** site2 );

void InitializeMemoryTracker ( void )
{
#if BUILD_TRACKER
	unsigned long		count;
	unsigned long		blockCount;
	AllocationSet *		set;
	Block *				block;
	Block **			blockIndex;

	/* do any one time init */
	if ( gNeverInitialized )
		{
		gNeverInitialized = false;
		}
				
	/* Be sure to dispose of ourselves if already allocated */
	if ( gTrackerInitialized != false )
		{
		ExitMemoryTracker();
		}
		
	gBlockNumber = 0;
	gNumActiveTrackerBlocks = 0;
	gNumHeapBlocks = 0;
	gMaxNumHeapBlocks = 0;
	gNumAllocations = 0;
	gAllocationSitesUsed = 0;
	gIndexSize = 0;
	gNumActiveSets = 0;
	gNumEnabledSets = 0;
	
	gLogFile = NULL;
	
	block = gBlockPool;
	gFreeBlocks = NULL;
	
	for ( count = 0; count < kNumBlocksTracked; ++count )
		{
		block->refCount = 0;
		block->blockAddress = NULL;
		block->blockID = count;
		block->next = gFreeBlocks;
		gFreeBlocks = block;
		
		++block;
		}
		
	blockIndex = gBlockIndex;

	for ( count = 0; count < kHashMod; ++count )
		{
		*blockIndex++ = NULL;
		}	
		
	set = gAllocationSetPool;
	
	for ( count = 0; count < kNumAllocationSets; ++count )
		{
		set->inUse = false;
		
		for ( blockCount = 0; blockCount < kBlockAllocationLongs; ++blockCount )
			{
			set->blockSet[ blockCount ] = 0;
			}
		
		++set;
		}
	
	for ( count = 0; count < kNumDecoderTypes; ++count )
		{
		gDecoderTable.decoderTag[ count ] = 0;
		gDecoderTable.decoderProc[ count ] = 0L;
		}
	
	memset ( &gSizeHistogram, 0, sizeof(gSizeHistogram) );
	gSizeHistogram.header.logTag = kSIZE_HISTOGRAM;
	gSizeHistogram.header.logSize = sizeof(HistogramLog);
		
	HashInitialize ( &gSiteTable, (HashFunction) &AllocationSiteHash,
		(HashCompare) AllocationSiteHashCompare );
		
	gTrackerInitialized = true;
	gTrackerEnableState = 0;
	
	NewAllocationSet ( 0, "InitializeMemoryTracker" );
#endif
}


void ExitMemoryTracker ( void )
{
#if BUILD_TRACKER
	gTrackerInitialized = false;
	
	if ( gLogFile != NULL )
		{
		gLogFile = 0;
		}
#endif
}


void DisableMemoryTracker ( void )
{
#if BUILD_TRACKER
	++gTrackerEnableState;
#endif
}


void EnableMemoryTracker ( void )
{
#if BUILD_TRACKER
	if ( gTrackerEnableState > 0 )
		{
		--gTrackerEnableState;
		}
#endif
}


void SetTrackerDataDecoder ( UInt32 tag, DecoderProc proc )
{
#if BUILD_TRACKER
	UInt32	count;
	
	/* find a free entry and set it */
	for ( count = 0; count < kNumDecoderTypes; ++count )
		{
		if ( gDecoderTable.decoderProc[ count ] == NULL )
			{
			gDecoderTable.decoderTag[ count ] = tag;
			gDecoderTable.decoderProc[ count ] = proc;
			break;
			}
		}
#else
#pragma unused( tag, proc )
#endif
}


void DumpMemoryTrackerState ( void )
{
#if BUILD_TRACKER
	UInt32		count;
	UInt32		bytesOut;
	
	if ( gLogFile == NULL )
		{
		gLogFile = PR_OpenFile ( "MemoryLog.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644 );
		PR_ASSERT(gLogFile);
		}
	
	/* dump the size histogram */
	bytesOut = _OS_WRITE ( gLogFile, (char *) &gSizeHistogram, sizeof(gSizeHistogram) );
	PR_ASSERT(bytesOut == sizeof(gSizeHistogram));
		
#if LOG_ALL_OPEN_SETS	
	/* Log all active allocation sets */
	for ( count = 0; count < kNumAllocationSets; ++count )
		{
		if ( gAllocationSetPool[ count ].inUse == true )
			{
			LogAllocationSetState ( &gAllocationSetPool[ count ] );
			}
		}
#endif
	
	DumpAllocationSites();
#endif
}


void DumpAllocationSites ( void )
{
#if BUILD_TRACKER
	unsigned long			count;
	AllocationSite *		site;
	AllocationSiteLogEntry	logEntry;
	AllocationSiteLog		logHeader;
	long					size;
	int32					err;
	UInt32					decoderCount;
	DecoderProc				proc;
	
	if ( gLogFile != NULL )
		{
		logHeader.header.logTag = kALLOCATION_SITE_LIST;
		logHeader.header.logSize = sizeof(AllocationSiteLog) +
			( gAllocationSitesUsed * sizeof(AllocationSiteLogEntry) );
		logHeader.numEntries = gAllocationSitesUsed;
		
		size = sizeof(logHeader);
		err = _OS_WRITE ( gLogFile, (char *) &logHeader, size );
		PR_ASSERT(err == size);
		
		for ( count = 0; count < gAllocationSitesUsed; ++count )
			{
			site = &gAllocationSites[ count ];

			memset( &logEntry, 0, sizeof(logEntry) );

			// find the decoder routine and call it */
			for ( decoderCount = 0; decoderCount < kNumDecoderTypes; ++decoderCount )
				{
				if ( gDecoderTable.decoderTag[ decoderCount ] == site->tag )
					{
					proc = gDecoderTable.decoderProc[ decoderCount ];
					if ( proc != NULL )
						{
						proc ( site->stackCrawl, logEntry.stackNames );
						break;
						}
					}
				}
			
			logEntry.tag = site->tag;
			logEntry.currentBlocks = site->currentBlocks;
			logEntry.currentMemUsed = site->currentMemUsed;
			logEntry.maxMemUsed = site->maxMemUsed;
			logEntry.maxBlocks = site->maxBlocks;
			logEntry.totalBlocks = site->totalBlocks;
			logEntry.totalMemUsed = site->totalMemUsed;
			
			size = sizeof(logEntry);
			err = _OS_WRITE ( gLogFile, (char *) &logEntry, size );
			PR_ASSERT(err == size);
			}
		}
#endif
}


void TrackItem ( void * address, size_t blockSize, size_t overhead, UInt32 tag,
	void * decoderData )
{
#if BUILD_TRACKER
	AllocationSite *	site;
	Block *				block;
	unsigned long		setCount;
	UInt32				histIndex;
	
	++gNumHeapBlocks;
	++gNumAllocations;

	if ( gNumHeapBlocks > gMaxNumHeapBlocks )
		{
		gMaxNumHeapBlocks = gNumHeapBlocks;
		}
	
	histIndex = CONVERT_SIZE_TO_INDEX(blockSize);
	gSizeHistogram.count[ histIndex ].total++;
	gSizeHistogram.count[ histIndex ].current++;
	if ( gSizeHistogram.count[ histIndex ].current > gSizeHistogram.count[ histIndex ].max )
		{
		gSizeHistogram.count[ histIndex ].max = gSizeHistogram.count[ histIndex ].current;
		}
	
	/* don't do anything if we have nowhere to put it */
	if ( gNumActiveSets == 0 )
		{
		return;
		}
	
	/* if we don't have any enabled sets, then bail */
	if ( gNumEnabledSets == 0 )
		{
		return;
		}
			
	/* tracking a null block will hose us big time */
	if ( ( address == NULL ) || ( gTrackerInitialized == false ) || ( gTrackerEnableState > 0 ) )
		{
		return;
		}
		
	/*
	 * Find a free block in our block pool
	 */
	block = AllocateBlockFromPool();
	
	/* if we found a block, allocate it */
	if ( block != NULL )
		{
		/* Find the allocation site for this block */
		site = NewAllocationSite ( decoderData, tag, blockSize );
		
		block->blockNum = gBlockNumber++;
		block->blockSize = blockSize;
		block->blockAddress = address;
		block->refCount = 0;
		block->overhead = overhead;
		block->blockState = kBlockAllocated;
		block->site = site;
		
		/* insert this block into the block index */
		InsertBlock ( block );
		
		/* add our own reference to this block */
		AddBlockReference ( block );
		
		/* and then add it to all relevant allocation sets */
		for ( setCount = 0; setCount < kNumAllocationSets; ++setCount )
			{
			if ( gAllocationSetPool[ setCount ].inUse == true )
				{
				AddNewBlockToAllocationSet ( &gAllocationSetPool[ setCount ], block );
				}
			}
		}
#else
#pragma unused(address, blockSize, overhead, tag, decoderData)
#endif
}


void ReleaseItem ( void * address )
{
#if BUILD_TRACKER
	unsigned long		count;
	Block *				block;
		
	--gNumHeapBlocks;

	if ( ( gTrackerInitialized == false ) || ( gTrackerEnableState > 0 ) )
		{
		return;
		}
		
	block = NULL;
	
	block = FindBlockBucket ( address, NULL );
	
	if ( block != NULL )
		{	
		block->blockState = kBlockFree;

		gSizeHistogram.count[ CONVERT_SIZE_TO_INDEX(block->blockSize) ].current--;

		BlockFreedFromSite ( block->site, block->blockSize );
		
		/* remove our own reference from this block */
		RemoveBlockReference ( block );

		/* remove block from all sets */
		for ( count = 0; count < kNumAllocationSets; ++count )
			{
			if ( gAllocationSetPool[ count ].inUse == true )
				{
				MarkBlockFreeFromSet ( &gAllocationSetPool[ count ], block );
				}
			}
		}
#else
#pragma unused(address)
#endif
}

AllocationSite * NewAllocationSite ( void ** stackCrawl, UInt32 tag, UInt32 blockSize )
{
#if BUILD_TRACKER
	AllocationSite *	site;
	unsigned long		stackCount;
	void *				hashKey[ kStackDepth + 1 ];
	
		/* turn the stack crawl and data tag into a hash key */
		hashKey[ 0 ] = (void *) tag;
		for ( stackCount = 1; stackCount < kStackDepth + 1; ++stackCount )
			{
			hashKey[ stackCount ] = stackCrawl[ stackCount - 1 ];
			}
			
		site = (AllocationSite *) HashFind ( &gSiteTable, hashKey );
		if ( site == NULL )
			{
			if ( gAllocationSitesUsed < kNumAllocationSites )
				{
				site = &gAllocationSites[ gAllocationSitesUsed++ ];
				for ( stackCount = 0; stackCount < kStackDepth; ++stackCount )
					{
					site->stackCrawl[ stackCount ] = stackCrawl[ stackCount ];
					}

				site->siteIndex = gAllocationSitesUsed - 1;
				site->tag = tag;
				site->currentBlocks = 0;
				site->currentMemUsed = 0;
				site->maxMemUsed = 0;
				site->maxBlocks = 0;
				site->totalBlocks = 0;
				site->totalMemUsed = 0;
				
				HashInsert ( &gSiteTable, (HashBlock *) site );
				}
			}

	if ( site != NULL )
		{
		++site->currentBlocks;
		if ( site->currentBlocks > site->maxBlocks )
			{
			site->maxBlocks = site->currentBlocks;
			}
			
		site->currentMemUsed += blockSize;
		if ( site->currentMemUsed > site->maxMemUsed )
			{
			site->maxMemUsed = site->currentMemUsed;
			}
			
		++site->totalBlocks;
		site->totalMemUsed += blockSize;
		}
		
	return site;
#else
#pragma unused(stackCrawl,tag, blockSize)
	return NULL;
#endif
}


void BlockFreedFromSite ( AllocationSite * site, unsigned long blockSize )
{
	if ( site != NULL )
		{
		--site->currentBlocks;
		site->currentMemUsed -= blockSize;
		}
}


static unsigned long AllocationSiteHash( void ** stackCrawl )
{
	unsigned long	hash;
	unsigned long	count;
	
	hash = 0;

	for ( count = 0; count < kStackDepth + 1; ++count )
		{
		hash += (unsigned long) stackCrawl[ count ];
		}
	
	return hash;
}


static Boolean AllocationSiteHashCompare ( void ** site1, void ** site2 )
{
	Boolean			matched;
	unsigned long	count;
	
	matched = true;
	for ( count = 0; count < kStackDepth; ++count )
		{
		if ( site1[ count ] != site2[ count ] )
			{
			matched = false;
			break;
			}
		}
	
	return matched;
}


AllocationSet * NewAllocationSet ( unsigned long trackingOptions, char * name )
{
#pragma unused(trackingOptions)
#if BUILD_TRACKER
	AllocationSet *	set;
	unsigned long	count;
	
	set = NULL;

	/* find a free set */
	for ( count = 0; count < kNumAllocationSets; ++count )
		{
		if ( gAllocationSetPool[ count ].inUse == false )
			{
			set = &gAllocationSetPool[ count ];
			break;
			}
		}
	
	if ( set != NULL )
		{
		set->inUse = true;
		set->numBlocks = 0;
		set->totalAllocation = 0;
		set->currentAllocation = 0;
		set->maxAllocation = 0;
		set->enabledState = 0;
		strcpy ( set->name, name );
		
		/* clear all blocks from this set */
		for ( count = 0; count < kBlockAllocationLongs; ++count )
			{
			set->blockSet[ count ] = 0;
			}
		
		++gNumActiveSets;
		++gNumEnabledSets;
		}
	
	return set;
#else
#pragma unused(name)
	return NULL;
#endif
}


void EnableAllocationSet ( AllocationSet * set )
{
#if BUILD_TRACKER
	if ( set->enabledState > 0 )
		{
		--set->enabledState;
		++gNumEnabledSets;
		}
#else
#pragma unused(set)
#endif
}


void DisableAllocationSet ( AllocationSet * set )
{
#if BUILD_TRACKER
	PR_ASSERT(set->enabledState != 255);
	
	++set->enabledState;
	--gNumEnabledSets;
#else
#pragma unused(set)
#endif
}


void AddNewBlockToAllocationSet ( AllocationSet * set, Block * block )
{
#if BUILD_TRACKER
	unsigned long	blockID;
	unsigned long	blockMask;
	unsigned long *	blockSetLong;
	
	/* if we're not enabled, then bail */
	if ( set->enabledState != 0 )
		{
		return;
		}
		
	blockID = block->blockID;
	blockSetLong = &set->blockSet[ blockID >> 5 ];
	
	blockMask = 1L << ( 31 - ( blockID & 0x1F ) );
	
	if ( *blockSetLong & blockMask )
		{
		return;
		}

	set->numBlocks++;
	set->totalAllocation += block->blockSize;
	set->currentAllocation += block->blockSize;
		
	if ( set->currentAllocation > set->maxAllocation )
		{
		set->maxAllocation = set->currentAllocation;
		}
		
	*blockSetLong |= blockMask;
	AddBlockReference ( block );
#else
#pragma unused(set, block)
#endif
}


void MarkBlockFreeFromSet ( AllocationSet * set, Block * block )
{
#if BUILD_TRACKER
	unsigned long	blockID;
	unsigned long	blockMask;
	unsigned long *	blockSetLong;
	
	blockID = block->blockID;
	blockSetLong = &set->blockSet[ blockID >> 5 ];
	
	blockMask = 1L << ( 31 - ( blockID & 0x1F ) );
	
	if ( ( *blockSetLong & blockMask ) == 0 )
		{
		return;
		}
	
	*blockSetLong &= ~blockMask;

	set->numBlocks--;
	set->currentAllocation -= block->blockSize;

	RemoveBlockReference ( block );
#else
#pragma unused(set, block)
#endif
}


void LogAllocationSetState ( AllocationSet * set )
{
#if BUILD_TRACKER
	unsigned long	blockCount;
	unsigned long	blockMask;
	unsigned long *	setLongPtr;
	unsigned long	setLong;
	Block *			block;
	AllocationSetLogEntry	logEntry;
	AllocationSetLog		logHeader;
	long					size;
	OSErr					err;
	
	if ( set == NULL )
		{
		return;
		}

	if ( gLogFile == NULL )
		{
		gLogFile = PR_OpenFile ( "MemoryLog.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644 );
		PR_ASSERT(gLogFile);
		}
	
	if ( gLogFile != NULL )
		{
		memset( &logHeader, 0, sizeof(logHeader) );
		logHeader.header.logTag = kSET_BLOCK_LIST;
		logHeader.header.logSize = sizeof(AllocationSetLog) +
			( set->numBlocks * sizeof(AllocationSetLogEntry) );
		
		logHeader.numEntries = set->numBlocks;
		logHeader.totalAllocation = set->totalAllocation;
		logHeader.currentAllocation = set->currentAllocation;
		logHeader.maxAllocation = set->maxAllocation;
		strcpy ( logHeader.name, set->name );
		
		size = sizeof(logHeader);
		err = _OS_WRITE ( gLogFile, (char *) &logHeader, size );
		PR_ASSERT(err == size);
		
		blockMask = 0;
		setLongPtr = set->blockSet;

		for ( blockCount = 0; blockCount < kNumBlocksTracked; ++blockCount )
			{
			if ( blockMask == 0 )
				{
				blockMask = 0x80000000;
				setLong = *setLongPtr++;
				}
			
			if ( setLong & blockMask )
				{
				block = &gBlockPool[ blockCount ];
				
				memset( &logEntry, 0, sizeof(logEntry) );
				logEntry.address = (unsigned long) block->blockAddress;
				logEntry.blockNumber = block->blockNum;
				logEntry.size = block->blockSize;
				if ( block->site != NULL )
					{
					logEntry.siteIndex = block->site->siteIndex;
					}
				else
					{
					logEntry.siteIndex = -1;
					}
					
				logEntry.blockState = block->blockState;
				logEntry.overhead = block->overhead;
				
				size = sizeof(logEntry);
				err = _OS_WRITE ( gLogFile, (char *) &logEntry, size );
				PR_ASSERT(err == size);
				}
			
			blockMask >>= 1;
			}
		}
#else
#pragma unused(set)
#endif
}


void DisposeAllocationSet ( AllocationSet * set )
{
#if BUILD_TRACKER
	unsigned long	blockIndex;
	unsigned long *	setBlocksPtr;
	unsigned long	setBlocksLong;
	unsigned long	blockMask;
	
	if ( set == NULL )
		{
		return;
		}
		
	/* release all the blocks we own */
	setBlocksPtr = set->blockSet;
	blockMask = 0;
	
	for ( blockIndex = 0; blockIndex < kNumBlocksTracked; ++blockIndex )
		{
		if ( blockMask == 0 )
			{
			blockMask = 0x80000000;
			setBlocksLong = *setBlocksPtr++;
			}
		
		if ( blockMask & setBlocksLong )
			{
			RemoveBlockReference ( &gBlockPool[ blockIndex ] );
			}
		
		blockMask >>= 1;
		}

	set->inUse = false;
	--gNumActiveSets;
#else
#pragma unused(set)
#endif
}


/* These utility routines can all go if we're not on */
#if BUILD_TRACKER
static Block * AllocateBlockFromPool ( void )
{
	Block *			block;
	
	block = gFreeBlocks;
	if ( block != NULL )
		{
		++gNumActiveTrackerBlocks;
		gFreeBlocks = block->next;
		}

	return block;
}


static void FreeBlockFromPool ( Block * block )
{
	/* this sucks to research the hash table, but I don't want to eat more */
	/* memory */
	RemoveBlock ( block );
	
	--gNumActiveTrackerBlocks;
	
	block->next = gFreeBlocks;
	gFreeBlocks = block;
}


static void InsertBlock ( Block * block )
{
	Bucket *		bucket;
	
	bucket = &gBlockIndex[ (unsigned long) block->blockAddress % kHashMod ];
	block->next = *bucket;
	*bucket = block;
}


static void RemoveBlock ( Block * block )
{
	Bucket *		bucket;
	Block *			prev;
	Block *			next;
	Block *			walker;
	
	bucket = &gBlockIndex[ (unsigned long) block->blockAddress % kHashMod ];
		
	/* walk the list, find our guy and remove it */
	prev = NULL;
	walker = *bucket;
	
	while ( walker != NULL )
		{
		next = walker->next;
		
		if ( walker == block )
			{
			if ( prev == NULL )
				{
				/* first block in the list */
				*bucket = next;
				}
			else
				{
				prev->next = next;
				}
			
			break;
			}
		
		prev = walker;
		walker = next;
		}
}


static Block * FindBlockBucket ( void * address, Bucket ** bucket )
{
	unsigned long	hashIndex;
	Bucket *		hashBucket;
	Block *			blockWalker;
	Block *			block;
	
	block = NULL;
	
	hashIndex = (unsigned long) address % kHashMod;
	hashBucket = &gBlockIndex[ hashIndex ];
	if ( bucket != NULL )
		{
		*bucket = hashBucket;
		}
	
	blockWalker = *hashBucket;
	while ( blockWalker != NULL )
		{
		if ( blockWalker->blockAddress == address )
			{
			block = blockWalker;
			break;
			}
		
		blockWalker = blockWalker->next;
		}
	
	return block;
}


static void AddBlockReference ( Block * block )
{
	++block->refCount;
		
	/* make sure we didn't wrap the refCount */
	PR_ASSERT(block->refCount != 0);
}

static void RemoveBlockReference ( Block * block )
{
	if ( --block->refCount == 0 )
		{
		FreeBlockFromPool ( block );
		}
}


static void HashInitialize ( HashTable * table, HashFunction hash, HashCompare compare )
{
	unsigned long	count;
	
	table->hashFunction = hash;
	table->compareFunction = compare;
	
	for ( count = 0; count < kHashMod; ++count )
		{
		table->index[ count ] = NULL;
		}
}


static void HashInsert ( HashTable * table, HashBlock * block )
{
	HashBlock **	bucket;
	unsigned long	hash;
	
	hash = (*table->hashFunction) ( block->key ) % kHashMod;
	
	bucket = &table->index[ hash ];
	block->next = *bucket;
	*bucket = block;
}


static void HashRemove ( HashTable * table, HashBlock * block )
{
	HashBlock **	bucket;
	HashBlock *		prev;
	HashBlock *		next;
	HashBlock *		walker;
	unsigned long	hash;
	
	hash = (*table->hashFunction) ( block->key ) % kHashMod;
	
	bucket = &table->index[ hash ];
		
	/* walk the list, find our guy and remove it */
	prev = NULL;
	walker = *bucket;
	
	while ( walker != NULL )
		{
		next = walker->next;
		
		if ( (*table->compareFunction)( walker->key, block->key ) )
			{
			if ( prev == NULL )
				{
				/* first block in the list */
				*bucket = next;
				}
			else
				{
				prev->next = next;
				}
			
			break;
			}
		
		prev = walker;
		walker = next;
		}
}


static HashBlock * HashFind ( HashTable * table, void * key )
{
	HashBlock **	bucket;
	HashBlock *		blockWalker;
	HashBlock *		block;
	unsigned long	hash;
	
	hash = (*table->hashFunction) ( key ) % kHashMod;
	
	bucket = &table->index[ hash ];
	
	block = NULL;
	blockWalker = *bucket;
	
	while ( blockWalker != NULL )
		{
		if ( (*table->compareFunction) ( blockWalker->key, key ) )
			{
			block = blockWalker;
			break;
			}
		
		blockWalker = blockWalker->next;
		}
	
	return block;
}

#endif
