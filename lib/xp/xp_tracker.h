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


#ifndef __XPTRACKER__
#define	__XPTRACKER__

#ifdef __cplusplus
extern "C" {
#endif

#define	kStackDepth				23
#define	kBlockTagStringLength	750


// Block State
enum {
	kBlockFree = 0,
	kBlockAllocated = 1
};

/*
 * Binary Log tags
 */

#define	kSET_BLOCK_LIST			'setb'
#define	kALLOCATION_SITE_LIST	'allc'
#define	kSIZE_HISTOGRAM			'shst'

/* a log header */
typedef struct LogHeader {
	unsigned long			logTag;
	unsigned long			logSize;
} LogHeader;

/* a single entry in an allocation site binary log */
typedef struct AllocationSiteLogEntry {
	unsigned long	currentBlocks;
	unsigned long	currentMemUsed;
	unsigned long	maxMemUsed;
	unsigned long	maxBlocks;
	unsigned long	totalBlocks;
	unsigned long	totalMemUsed;
	unsigned long	tag;
	char			stackNames[ kBlockTagStringLength ];
} AllocationSiteLogEntry;

typedef struct AllocationSiteLog {
	LogHeader				header;
	unsigned long			numEntries;
	AllocationSiteLogEntry	log[];
} AllocationSiteLog;

/* single entry in a allocation set log */
typedef struct AllocationSetLogEntry {
	unsigned long			address;
	unsigned long			blockNumber;
	unsigned long			size;
	unsigned long			siteIndex;
	unsigned char			blockState;
	unsigned char			overhead;
	unsigned char			pad[ 2 ];
} AllocationSetLogEntry;

typedef struct AllocationSetLog {
	LogHeader				header;
	unsigned long			numEntries;
	unsigned long			totalAllocation;
	unsigned long			currentAllocation;
	unsigned long			maxAllocation;
	char					name[ 256 ];
	AllocationSetLogEntry	log[];
} AllocationSetLog;

/* histogram log */
#define	kMaxRecordedSize			256
#define	kNumHistogramEntries		((kMaxRecordedSize / 4) + 2)
#define	CONVERT_SIZE_TO_INDEX(s)	((s) > kMaxRecordedSize ? (kNumHistogramEntries - 1) : ((s) + 3) >> 2)

typedef struct HistogramLogEntry {
	UInt32	total;
	UInt32	max;
	UInt32	current;
} HistogramLogEntry;

typedef struct HistogramLog {
	LogHeader				header;
	HistogramLogEntry		count[ kNumHistogramEntries ];
} HistogramLog;

/*
 * Routines to initialize the memory tracker
 */
void InitializeMemoryTracker ( void );
void ExitMemoryTracker ( void );
void DumpMemoryTrackerState ( void );
void DumpAllocationSites ( void );

void DisableMemoryTracker ( void );
void EnableMemoryTracker ( void );

/*
 * Decoder Function. This function is called with the data attached to
 * an active block when the log is being written out. resultString points
 * at a string of length kBlockTagStringLength.
 */
typedef void (*DecoderProc) ( void ** data, char * resultString );

/*
 * Call this function to set the decoder proc for a given data tag.
 * The most common thing here will be a stack crawl decoder that will
 * convert an array of address to a list of function names and offsets
 */
void SetTrackerDataDecoder ( UInt32 tag, DecoderProc proc );


/*
 * Call these functions to add a new item to be tracked and then release it later
 */
void TrackItem ( void * address, size_t blockSize, size_t overhead, UInt32 tag,
	void * decoderData );
void ReleaseItem ( void * address );


/*
 *	Allocation Sets.
 */
typedef struct AllocationSet AllocationSet;

/* Options */
enum {
	kTrackAllBlocks = 0x00000001,
	kKeepFreeBlocks = 0x00000002
};

AllocationSet * NewAllocationSet ( unsigned long trackingOptions, char * name );
void DisposeAllocationSet ( AllocationSet * set );
void LogAllocationSetState ( AllocationSet * set );
void EnableAllocationSet ( AllocationSet * set );
void DisableAllocationSet ( AllocationSet * set );

#ifdef __cplusplus
}
#endif

#endif	/* __XPTRACKER__ */
