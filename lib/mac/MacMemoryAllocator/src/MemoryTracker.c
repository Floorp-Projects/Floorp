/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <Types.h>
#include <Files.h>
#include <Gestalt.h>
#include <Processes.h>

#include "TypesAndSwitches.h"
#include "MemoryTracker.h"
#include "prthread.h"


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
#define	kMaxInstructionScanDistance	4096
#define	kInvalidIndex				0xFFFFFFFF


/*
 * Hash Table crap
 */
typedef struct HashBlock HashBlock;
typedef struct HashTable HashTable;
typedef struct AllocationSite AllocationSite;

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



typedef struct Block {
	unsigned long		blockNum;
	unsigned long		blockID;
	unsigned long		blockSize;
	void *				blockAddress;
	AllocationSite *	site;
	unsigned char		allocType;
	unsigned char		blockState;
	unsigned char		refCount;
	unsigned char		overhead;
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
	void *				stackCrawl[ kStackDepth ];
	unsigned long		siteIndex;
	unsigned long		currentBlocks;
	unsigned long		currentMemUsed;
	unsigned long		maxMemUsed;
	unsigned long		totalBlocks;
	unsigned long		totalMemUsed;
};


pascal void TrackerExitToShellPatch(void);
asm void * GetCurrentStackPointer();

#if DEBUG_MAC_MEMORY
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

static void *		gMemoryTop;
static short		gLogFile = 0;


UniversalProcPtr	gExitToShellPatchCallThru = NULL;
#if GENERATINGCFM
enum {
	uppExitToShellProcInfo 				= kPascalStackBased
};

RoutineDescriptor 	gExitToShellPatchRD = BUILD_ROUTINE_DESCRIPTOR(uppExitToShellProcInfo, &TrackerExitToShellPatch);
#else
#define gExitToShellPatchRD ExitToShellPatch
#endif
#endif

void		*		gOurApplicationHeapBase;
void		*		gOurApplicationHeapMax;


static Block * AllocateBlockFromPool ( void );
static void FreeBlockFromPool ( Block * block );
static Block * FindBlock ( void * address );
static void InsertBlock ( Block * block );
static void RemoveBlock ( Block * block );
static Block * FindBlockBucket ( void * address, Bucket ** bucket );
static void AddBlockReference ( Block * block );
static void RemoveBlockReference ( Block * block );
static void CatenateRoutineNameFromPC( char * string, void *currentPC );
static void PrintHexNumber( char * string, UInt32 hexNumber );
static void PrintStackCrawl ( char * name, void ** stackCrawl );

static void AddNewBlockToAllocationSet ( AllocationSet * set, Block * block );
static void MarkBlockFreeFromSet ( AllocationSet * set, Block * block );
static void BlockFreedFromSite ( AllocationSite * site, unsigned long blockSize );


static unsigned long AllocationSiteHash( void ** stackCrawl );
static Boolean AllocationSiteHashCompare ( void ** site1, void ** site2 );

void InitializeMemoryTracker ( void )
{
#if DEBUG_MAC_MEMORY
	unsigned long		count;
	unsigned long		blockCount;
	AllocationSet *		set;
	Block *				block;
	Block **			blockIndex;
	OSErr				err;
	ProcessSerialNumber	thisProcess = { 0, kCurrentProcess };
	ProcessInfoRec		processInfo;

	processInfo.processInfoLength = sizeof(processInfo);
	processInfo.processName = NULL;
	processInfo.processAppSpec = NULL;

	GetProcessInformation(&thisProcess, &processInfo);

	gOurApplicationHeapBase = processInfo.processLocation;
	gOurApplicationHeapMax = (Ptr)gOurApplicationHeapBase + processInfo.processSize;

	err = Gestalt ( gestaltLogicalRAMSize, (long *) &gMemoryTop );
	if ( err != noErr )
		{
		DebugStr ( "\pThis machine has no logical ram?" );
		ExitToShell();
		}

	/* do any one time init */
	if ( gNeverInitialized )
		{
		gNeverInitialized = false;
		gExitToShellPatchCallThru = GetToolboxTrapAddress(0x01F4);
		SetToolboxTrapAddress((UniversalProcPtr)&gExitToShellPatchRD, 0x01F4);
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
	
	FSDelete ( "\pMemoryLog.bin", 0 );
	Create ( "\pMemoryLog.bin", 0, 'shan', 'binl' );
	err = FSOpen ( "\pMemoryLog.bin", 0, &gLogFile );
	if ( err != noErr )
		{
		gLogFile = 0;
		}
	
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
	
	HashInitialize ( &gSiteTable, (HashFunction) &AllocationSiteHash,
		(HashCompare) AllocationSiteHashCompare );
		
	gTrackerInitialized = true;
	gTrackerEnableState = 0;
	
	NewAllocationSet ( 0, "InitializeMemoryTracker" );
#endif
}


void ExitMemoryTracker ( void )
{
#if DEBUG_MAC_MEMORY
	gTrackerInitialized = false;
	
	if ( gLogFile != NULL )
		{
		FSClose ( gLogFile );
		gLogFile = 0;
		}
#endif
}


void DisableMemoryTracker ( void )
{
#if DEBUG_MAC_MEMORY
	++gTrackerEnableState;
#endif
}


void EnableMemoryTracker ( void )
{
#if DEBUG_MAC_MEMORY
	if ( gTrackerEnableState > 0 )
		{
		--gTrackerEnableState;
		}
#endif
}


void DumpMemoryTrackerState ( void )
{
#if DEBUG_MAC_MEMORY
	unsigned long		count;
	
	printf ( "Number of tracker blocks in use: %ld\n", gNumActiveTrackerBlocks );
	printf ( "Number of heap blocks in use: %ld\n", gNumHeapBlocks );
	printf ( "Max Number of heap blocks in use: %ld\n", gMaxNumHeapBlocks );
	printf ( "Total number of allcations %ld\n", gNumAllocations );

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
#if DEBUG_MAC_MEMORY
	unsigned long			count;
	AllocationSite *		site;
	AllocationSiteLogEntry	logEntry;
	AllocationSiteLog		logHeader;
	long					size;
	OSErr					err;
	
	if ( gLogFile != NULL )
		{
		logHeader.header.logTag = kALLOCATION_SITE_LIST;
		logHeader.header.logSize = sizeof(AllocationSiteLog) +
			( gAllocationSitesUsed * sizeof(AllocationSiteLogEntry) );
		logHeader.numEntries = gAllocationSitesUsed;
		
		size = sizeof(logHeader);
		err = FSWrite ( gLogFile, &size, &logHeader );
		if ( err != noErr )
			{
			DebugStr ( "\pWrite failed" );
			return;
			}
		
		for ( count = 0; count < gAllocationSitesUsed; ++count )
			{
			site = &gAllocationSites[ count ];

			memset( &logEntry, 0, sizeof(logEntry) );

			PrintStackCrawl ( logEntry.stackNames, site->stackCrawl );
			
			logEntry.currentBlocks = site->currentBlocks;
			logEntry.currentMemUsed = site->currentMemUsed;
			logEntry.maxMemUsed = site->maxMemUsed;
			logEntry.totalBlocks = site->totalBlocks;
			logEntry.totalMemUsed = site->totalMemUsed;
			
			size = sizeof(logEntry);
			err = FSWrite ( gLogFile, &size, &logEntry );
			if ( err != noErr )
				{
				DebugStr ( "\pWrite failed" );
				return;
				}
			}
		}
#endif
}


void AddNewBlock ( void * address, size_t blockSize, size_t overhead, AllocatorType allocType,
	void * stackCrawl )
{
#if DEBUG_MAC_MEMORY
	AllocationSite *	site;
	Block *				block;
	unsigned long		setCount;
	
	++gNumHeapBlocks;
	++gNumAllocations;

	if ( gNumHeapBlocks > gMaxNumHeapBlocks )
		{
		gMaxNumHeapBlocks = gNumHeapBlocks;
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
		site = NewAllocationSite ( stackCrawl, blockSize );
		
		block->blockNum = gBlockNumber++;
		block->blockSize = blockSize;
		block->blockAddress = address;
		block->allocType = allocType;
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
#pragma unused(address, blockSize, overhead, allocType, stackCrawl)
#endif
}


void FreeBlock ( void * address )
{
#if DEBUG_MAC_MEMORY
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
	else
		{
//		DebugStr ( "\pCan't find block?" );
		}
#else
#pragma unused(address)
#endif
}

AllocationSite * NewAllocationSite ( void ** stackCrawl, unsigned long blockSize )
{
#if DEBUG_MAC_MEMORY
	AllocationSite *	site;
	unsigned long		stackCount;

		site = (AllocationSite *) HashFind ( &gSiteTable, stackCrawl );
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
				site->currentBlocks = 0;
				site->currentMemUsed = 0;
				site->maxMemUsed = 0;
				site->totalBlocks = 0;
				site->totalMemUsed = 0;
				}
			else
				{
	//			DebugStr ( "\pOut of allocation sites..." );
				}
			
			HashInsert ( &gSiteTable, (HashBlock *) site );
			}

	if ( site != NULL )
		{
		++site->currentBlocks;
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
#pragma unused(stackCrawl, blockSize)
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

	for ( count = 0; count < kStackDepth; ++count )
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
#if DEBUG_MAC_MEMORY
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
		}
	
	return set;
#else
#pragma unused(name)
	return NULL;
#endif
}


void EnableAllocationSet ( AllocationSet * set )
{
	if ( set->enabledState > 0 )
		{
		--set->enabledState;
		}
}


void DisableAllocationSet ( AllocationSet * set )
{
	if ( set->enabledState == 255 )
		{
//		DebugStr ( "\pAllocationSet's enabledState overflowed!" );
		}
	
	++set->enabledState;
}


void AddNewBlockToAllocationSet ( AllocationSet * set, Block * block )
{
#if DEBUG_MAC_MEMORY
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
//		DebugStr ( "\pWe suck, this block is already part of the set" );
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
#if DEBUG_MAC_MEMORY
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
#if DEBUG_MAC_MEMORY
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
		err = FSWrite ( gLogFile, &size, &logHeader );
		if ( err != noErr )
			{
			DebugStr ( "\pWrite failed" );
			return;
			}
		
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
					
				logEntry.allocType = block->allocType;
				logEntry.blockState = block->blockState;
				logEntry.overhead = block->overhead;
				
				size = sizeof(logEntry);
				err = FSWrite ( gLogFile, &size, &logEntry );
				if ( err != noErr )
					{
					DebugStr ( "\pWrite failed" );
					return;
					}
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
#if DEBUG_MAC_MEMORY
	unsigned long	blockIndex;
	unsigned long *	setBlocksPtr;
	unsigned long	setBlocksLong;
	unsigned long	blockMask;
		
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
#else
#pragma unused(set)
#endif
}


/* These utility routines can all go if we're not on */
#if DEBUG_MAC_MEMORY
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
	assert(block->refCount != 0);
}

static void RemoveBlockReference ( Block * block )
{
	if ( --block->refCount == 0 )
		{
		FreeBlockFromPool ( block );
		}
}


static void PrintStackCrawl ( char * name, void ** stackCrawl )
{
	unsigned long		currentStackLevel;
	
	*name = 0;
	
	for (currentStackLevel = 0; currentStackLevel < kStackDepth; currentStackLevel++)
		{
		CatenateRoutineNameFromPC ( name, stackCrawl[ currentStackLevel ] );
		
		if (currentStackLevel < kStackDepth - 1)
			strcat( name, "," );
		}
}


static void CatenateRoutineNameFromPC( char * string, void *currentPC )
{
	UInt32		instructionsToLook = kMaxInstructionScanDistance;
	UInt32		*currentPCAsLong = (UInt32 *)currentPC;
	UInt32		offset = 0;
	char		labelString[ 512 ];
	char		lameString[ 512 ];
	
	if (currentPC == NULL)
		return;

	/* make sure we're not out in the weeds */
	if ( (unsigned long) currentPC > (unsigned long) gMemoryTop )
		return;
		
	if ( (unsigned long) currentPC & 0x3 )
		return;
	
	*labelString = 0;
	*lameString = 0;
	
	while (instructionsToLook--) {
	
		if (*currentPCAsLong == 0x4E800020) {
			char	stringLength = *((char *)currentPCAsLong + 21);
			memset( labelString, 0, 512 );
			BlockMoveData( ((char *)currentPCAsLong + 22), labelString, stringLength );
			
			// if there was no routine name, then just put down the address.
			if ( *labelString == 0 )
				{
				PrintHexNumber( lameString, (unsigned long) currentPC );
				goto bail;
				}
			else
				{
				strcat ( lameString, labelString );
				}
			break;
		}
	
		currentPCAsLong++;
	}
	
	instructionsToLook = kMaxInstructionScanDistance;
	currentPCAsLong = (UInt32 *)currentPC;
	
	*labelString = 0;
	
	while (instructionsToLook--) {
		if (*currentPCAsLong == 0x7C0802A6) {
			strcat ( lameString, "+" );

			PrintHexNumber( labelString, offset - 4 );
			strcat ( lameString, labelString );
			break;
		}
		currentPCAsLong--;
		offset += 4;
	}

bail:	
	/* make sure we don't end up being too big */
	lameString[ kNameMaxLength ] = 0;
	strcat ( string, lameString );
}


static void PrintHexNumber( char * string, UInt32 hexNumber )
{
	char		hexString[11];
	char		hexMap[] = "0123456789ABCDEF";
	long		digits = 8;
		
	
	hexString[0] = '0';
	hexString[1] = 'x';
	
	while (digits) {
		hexString[digits + 1] = *((hexNumber & 0x0F) + hexMap);
		hexNumber = hexNumber >> 4;
		digits--;
	}
	hexString[10] = 0;
	
	strcat ( string, hexString );
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

pascal void TrackerExitToShellPatch(void)
{
static Boolean gBeenHere = false;

	/* if we haven't been here, then dump our memory state, otherwise don't */
	/* this way we don't hose ourselves if we crash in here */
	if ( !gBeenHere )
		{
		gBeenHere = true;
		DumpMemoryTrackerState();
		}

	ExitMemoryTracker();
	
#if GENERATINGCFM
	CallUniversalProc(gExitToShellPatchCallThru, uppExitToShellProcInfo);
#else 
	{
		ExitToShellProc	*exitProc = (ExitToShellProc *)&gExitToShellPatchCallThru;
		(*exitProc)();
	}
#endif
}

#endif

asm void *GetCurrentStackPointer() 
{
	mr		r3, sp
	lwz		r3, 0(r3)
	blr
}


void GetCurrentStackTrace(void **stackCrawl)
{
	void *		currentSP;
	void *		interfaceLibSP;
	void *		callerFirstStackFrame;
	void *		callerSP;
	UInt32		stackFrameLevel;
	UInt8		isPowerPC = true;
	void *		nextFrame;
	long		nativeFrames;
	PRThread *	curThread;
	void *		stackMin;
	void *		stackMax;
		
	memset(stackCrawl, 0, sizeof(void *) * kStackDepth);
	
#if !GENERATINGPOWERPC
	return;
#endif

	curThread = PR_CurrentThread();
	if ( curThread != NULL && curThread->stack->stackBottom != 0 )
		{
		stackMin = curThread->stack->stackBottom;
		stackMax = curThread->stack->stackTop;
		}
	else
		{
		stackMin = gOurApplicationHeapBase;
		stackMax = gOurApplicationHeapMax;
		}
 
	//	If the current SP is not in our heap, then bail .
	
#if GENERATINGPOWERPC
	currentSP = GetCurrentStackPointer();
#endif

	if ((currentSP > gOurApplicationHeapMax) || (currentSP < gOurApplicationHeapBase))
		return;
		
	interfaceLibSP = *((void **)currentSP);
	
	callerFirstStackFrame = interfaceLibSP;
	nextFrame = callerFirstStackFrame;
		
	//	What we really want to do here is walk up the stack until we get to a native frame that is in
	//	one of our Shared libraries... Since they can live in temp mem, this gets whacky.
	
	//	So, for now, I just walk up until I hit two native frames in a row...
	nativeFrames = 0;
	
	while (1) {
		// if this frame is outside of our thread's stack, then bail
		if ( ( nextFrame > stackMax ) || ( nextFrame < stackMin ) )
			{
			return;
			}
				
		//	Walk the stack differently whether we are at a
		//	PowerPC or 68K frame...

		if (isPowerPC) {

#if 0
			void	*framePC;

			//	If we are PowerPC, check to see if the PC
			//	corresponding to the current frame is in the
			//	the app's code space.  If so, then we are
			//	done and we break out.
			
			framePC = *((void **)callerFirstStackFrame + 2);
			if ((framePC < gOurApplicationHeapMax) && (framePC > gOurApplicationHeapBase))
				break;
#endif
				
			//	Otherwise, check the pointer to the next
			//	stack frame.  If its low bit is set, then
			//	it is a mixed-mode switch frame, and 
			//	we need to switch to 68K frames.

			nextFrame = *((void **)callerFirstStackFrame);
			
			isPowerPC = ((UInt32)nextFrame & 0x00000001) == 0;			
			callerFirstStackFrame = (void *)((UInt32)nextFrame & 0xFFFFFFFE);
			if ( isPowerPC != false )
				{
				if ( ++nativeFrames >= 1 )
					{
					break;
					}
				}
		}
	
		else {
			
			nativeFrames = 0;
			
			//	68K case:  If the location immediately above the stack
			//	frame pointer is -1, then we are at a switch frame,
			//	move back to PowerPC.

			if (*((UInt32 *)callerFirstStackFrame - 1) == 0xFFFFFFFF)
				isPowerPC = true;
			callerFirstStackFrame = *((void **)callerFirstStackFrame);
			nextFrame = callerFirstStackFrame;
		}
	
	}
	
	callerSP = callerFirstStackFrame;
	
	for (stackFrameLevel = 0; stackFrameLevel < kStackDepth; stackFrameLevel++) {
	
		if ( ( callerSP > stackMax ) || ( callerSP < stackMin ) )
			{
			return;
			}
			
		stackCrawl[stackFrameLevel] = *(((void **)callerSP) + 2);
		
		callerSP = *((void **)callerSP);
	
	}
}


void GetCurrentNativeStackTrace ( void ** stackCrawl )
{
	void *		currentSP;
	UInt32		stackFrameLevel;
	PRThread *	curThread;
	void *		stackMin;
	void *		stackMax;
	
	memset(stackCrawl, 0, sizeof(void *) * kStackDepth);
	
#if GENERATINGPOWERPC
	currentSP = GetCurrentStackPointer();
#else
	return;
#endif

	curThread = PR_CurrentThread();
	if ( curThread != NULL && curThread->stack->stackBottom != 0 )
		{
		stackMin = curThread->stack->stackBottom;
		stackMax = curThread->stack->stackTop;
		}
	else
		{
		stackMin = gOurApplicationHeapBase;
		stackMax = gOurApplicationHeapMax;
		}
		
	for (stackFrameLevel = 0; stackFrameLevel < kStackDepth; stackFrameLevel++) {
		if ( ( currentSP > stackMax ) || ( currentSP < stackMin ) )
			{
			return;
			}
			
		stackCrawl[stackFrameLevel] = *((void **)((char *)currentSP + 8));
		currentSP = *((void **)currentSP);
	}
}

