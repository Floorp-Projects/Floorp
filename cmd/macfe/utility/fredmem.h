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

#pragma once
#include "FlushAllocator.h"
#include "prtypes.h"
#include <LGrowZone.h>
#include <LListener.h>
#include <LPeriodical.h>

/*-----------------------------------------------------------------------------
	Cache Flushing - Layout
	Free the layout element recycle list for no-priority allocations.
-----------------------------------------------------------------------------*/
typedef struct MWContext_ MWContext;

extern void			FM_SetFlushable( MWContext* context, Boolean canFlush );

void				Memory_InitializeMemory();
size_t				Memory_MaxApplMem();
void				Memory_DoneWithMemory();
Boolean				Memory_MemoryIsLow();

inline void* operator new( size_t size, char* /*file*/, int /*line*/, char* /*kind*/ )
{
	void* mem = Flush_Allocate( size, FALSE );
	ThrowIfNil_( mem );
	return mem;
}

inline void operator delete( void* block, char* /* file */, int /* line */ )
{
	Flush_Free( block );
}

class LMemoryFlusherListener : public LListener { 

	public:
		LMemoryFlusherListener();
		virtual void	ListenToMessage(MessageT inMessage, void *ioParam);

};
