/* -*- Mode: C++;    tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape 
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include <string.h>
#include <stdlib.h>

#define GC_DEBUG
#include "gc.h"

// GC implementation of malloc/free, for testing purposes

//--------------------------------------------------------------------
void *malloc(size_t blockSize)
//--------------------------------------------------------------------
{
#ifdef GC_DEBUG
	return GC_MALLOC(blockSize);
#else
	if (blockSize <= 10000)
		return GC_MALLOC(blockSize);
	else
		return GC_malloc_ignore_off_page(blockSize);
#endif
}


//--------------------------------------------------------------------
void free(void *deadBlock)
//--------------------------------------------------------------------
{
	if (deadBlock != NULL)
		GC_FREE(deadBlock);
}


//--------------------------------------------------------------------
void* realloc(void* block, size_t newSize)
//--------------------------------------------------------------------
{
#ifdef GC_DEBUG
	return GC_REALLOC(block, newSize);
#else
    size_t oldSize = GC_size(block);
    void * newBlock = block;
 
    if (newSize <= 10000)
    	return(GC_REALLOC(block, newSize));
    
    if (newSize <= oldSize)
    	return(block);
    	
    newBlock = GC_malloc_ignore_off_page(newSize);
    if (newBlock == NULL)
    	return NULL;
    
    memcpy(newBlock, block, oldSize);
    GC_FREE(block);
    
    return(newBlock);
#endif
}

//--------------------------------------------------------------------
void *calloc(size_t nele, size_t elesize)
//--------------------------------------------------------------------
{
	// GC_MALLOC returns cleared blocks for us.
	size_t space = nele * elesize;
	return GC_MALLOC(space);
}
