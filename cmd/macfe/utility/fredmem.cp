/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef NSPR20
#include "prmacos.h"
#endif
#include "fredmem.h"

#include "shist.h"
#include "shistele.h"
#include "lo_ele.h"
#include "proto.h"
#include "net.h"
#include "libimg.h"
#include "fe_proto.h"
#include "uerrmgr.h"
#include "PascalString.h"
#include "resgui.h"
#include "MacMemAllocator.h"

//#include "MToolkit.h"

enum {
	kEmergencyMemoryFundSize			= 64 * 1024
};


class LMallocFlusherPeriodical : public LPeriodical {
	public:
							LMallocFlusherPeriodical();
		virtual	void		SpendTime(const EventRecord &inMacEvent);
		void				FlushMemory ( void ) { mFlushMemory = true; }
	
	private:
		Boolean				mFlushMemory;
};


class LLowMallocWarningPeriodical : public LPeriodical {
	public:
							LLowMallocWarningPeriodical();
		virtual	void		SpendTime(const EventRecord &inMacEvent);
		void				WarnUserMallocLow ( void ) { mWarnUser = true; }

	private:
		Boolean				mWarnUser;
};


unsigned char MacFEMemoryFlusher ( size_t bytesNeeded );
void ImageCacheMemoryFlusher(void);
void LayoutCacheMemoryFlusher(void);
void NetlibCacheMemoryFlusher(void);
void LibNeoCacheMemoryFlusher(void);

void MallocIsLowWarning(void);

static LMallocFlusherPeriodical 	gMallocFlusher;
static LLowMallocWarningPeriodical	gMallocWarner;

void Memory_InitializeMemory()
{
	new LGrowZone(kEmergencyMemoryFundSize);
	new LMemoryFlusherListener();

	InstallMemoryCacheFlusher(&MacFEMemoryFlusher);

	// install our low memory (malloc heap) warning proc
	InstallMallocHeapLowProc ( &MallocIsLowWarning );
}

void Memory_DoneWithMemory()
{
	//	No cleanup necessary
}

size_t Memory_MaxApplMem()
{
	return 0;
}

Boolean Memory_MemoryIsLow()
{
	return (LGrowZone::GetGrowZone())->MemoryIsLow();
}

//##############################################################################
//##############################################################################
#pragma mark MAC FE MEMORY FLUSH PROC
unsigned char MacFEMemoryFlusher ( size_t bytesNeeded )
{
	/* signal the flusher to do it's thing. I would rather start the idler here */
	/* but that involves allocating memory (and so could be reentrant)... */
	gMallocFlusher.FlushMemory();
	
	return 0;
}

//##############################################################################
//##############################################################################
#pragma mark LAYOUT CACHE FLUSHING

LArray	sFlushableLayoutContextList;

void FM_SetFlushable(MWContext* context, Boolean canFlush)
{
	if (canFlush) {
		if (sFlushableLayoutContextList.FetchIndexOf(&context) == 0)
			sFlushableLayoutContextList.InsertItemsAt(1, LArray::index_Last, &context);
	}
	
	else {
		sFlushableLayoutContextList.Remove(&context);
	}
}

void LayoutCacheMemoryFlusher(void)
{
	MWContext	*currentContext = NULL;
	SInt32 		bytesFreed = 0;
	
	LArrayIterator iter(sFlushableLayoutContextList);
	
	// go ahead and empty all the recycling bins
	while ( iter.Previous(&currentContext) )
		{
		bytesFreed += LO_EmptyRecyclingBin(currentContext);
		sFlushableLayoutContextList.RemoveItemsAt( 1, 1 );
		}
}

//##############################################################################
//##############################################################################
#pragma mark LIBNEO CACHE FLUSHING

void LibNeoCacheMemoryFlusher(void)
{
	// don't do this yet until we really understand how safe it is
#if 0	
	// we don't really know how much was freed here
	NeoOutOfMemHandler();
#endif
}

//##############################################################################
//##############################################################################
#pragma mark NETLIB CACHE FLUSHING

void NetlibCacheMemoryFlusher(void)
{
	size_t	newCacheSize;
	size_t	lastCacheSize;
	
	newCacheSize = NET_GetMemoryCacheSize();
	
	// shrink as much as we can
	do	{
		lastCacheSize = newCacheSize;
		newCacheSize = NET_RemoveLastMemoryCacheObject();
		}
	while ( newCacheSize != lastCacheSize );
}


//##############################################################################
//##############################################################################
#pragma mark IMAGE CACHE FLUSHING

void ImageCacheMemoryFlusher(void)
{
	size_t	newCacheSize;
	size_t	lastCacheSize;
	
	newCacheSize = IL_GetCacheSize();
	
	// shrink as much as we can
	do	{
		lastCacheSize = newCacheSize;
		newCacheSize = IL_ShrinkCache();
		}
	while ( newCacheSize != lastCacheSize );
}


//##############################################################################
//##############################################################################
#pragma mark MALLOC HEAP LOW USER WARNING

void MallocIsLowWarning(void)
{
	gMallocWarner.WarnUserMallocLow();
}

//##############################################################################
//##############################################################################
#pragma mark GROW ZONE LISTENER

LMemoryFlusherListener::
LMemoryFlusherListener():LListener()
{
	(LGrowZone::GetGrowZone())->AddListener(this);
}


void
LMemoryFlusherListener::ListenToMessage(MessageT inMessage, void *ioParam)
{
	Int32	*bytesToFree = (Int32 *)ioParam;
	if (inMessage == msg_GrowZone) {
		UInt8	successfulFlush = false;
		
		/* we don't do anything here yet */
		if (!successfulFlush)
			*bytesToFree = 0;
	}
}

//##############################################################################
//##############################################################################
#pragma mark MEMORY PERIODICAL

LLowMallocWarningPeriodical::LLowMallocWarningPeriodical() : LPeriodical()
{
	mWarnUser = false;
	
	StartIdling();
}


void
LLowMallocWarningPeriodical::SpendTime(const EventRecord &inMacEvent)
{
	if ( mWarnUser )
		{
		StopIdling();
		FE_Alert( NULL, GetPString ( MALLOC_HEAP_LOW_RESID ) );
		}
}


LMallocFlusherPeriodical::LMallocFlusherPeriodical() : LPeriodical()
{
	mFlushMemory = false;

	StartIdling();
}

void
LMallocFlusherPeriodical::SpendTime(const EventRecord &inMacEvent)
{
	if ( mFlushMemory )
		{
		ImageCacheMemoryFlusher();
		LayoutCacheMemoryFlusher();
		NetlibCacheMemoryFlusher();	
		LibNeoCacheMemoryFlusher();
		
		mFlushMemory = false;
		}

}
