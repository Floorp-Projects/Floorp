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

#include <UException.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "prtypes.h"

#include "FlushAllocator.h"


//##############################################################################
//##############################################################################

void* Flush_Allocate( size_t size, Boolean zero )
{
	void *newBlock = malloc( size );
	if ((newBlock != NULL) && zero)
		memset(newBlock, 0, size);
	return newBlock;
}

void Flush_Free( void* item )
{
	free(item);
}

void* Flush_Reallocate( void* item, size_t size )
{
	void*	newBlock = NULL;
	newBlock = realloc( item, size );
	return newBlock;
}


//##############################################################################
//##############################################################################



void* operator new( size_t size )
{
	void* mem = Flush_Allocate( size, FALSE );
	ThrowIfNil_( mem );
	return mem;
}

void operator delete( void* block )
{
	Flush_Free( block );
}
