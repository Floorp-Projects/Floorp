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

#include <stdlib.h>
#include <string.h>
#include "Pool.h"


//
// Allocate a new memory pool whose first chunk will have initialSize bytes
// and remaining chunks will have chunkSize bytes.
//
Pool::Pool(size_t initialSize, size_t chunkSize):
	initialSize(initialSize >= 256 ? initialSize >= chunkSize ? initialSize : chunkSize : 256),
	chunkSize(chunkSize >= 256 ? initialSize : 256),
	buffer((ptr)&buffer),
	bufferEnd((ptr)&buffer),
	headerList(0)
{}


//
// Release all memory from this pool and deallocate the pool.
//
Pool::~Pool()
{
	clear();
}


//
// Allocate a block from this pool that won't fit in the space that's
// left between buffer and bufferEnd.
// size has already been rounded up to a doubleword multiple.
//
void *Pool::allocateOverflow(size_t size)
{
	size_t sizePlusHeader = size + blockHeaderSize;
	size_t blockSize = sizePlusHeader;
	if (blockSize < chunkSize)
		blockSize = headerList ? chunkSize : initialSize;
	ptr block = static_cast<ptr>(malloc(blockSize));
	BlockHeader *h = reinterpret_cast<BlockHeader *>(block);
	h->next = headerList;
	h->size = blockSize;
	headerList = h;
	if (blockSize != sizePlusHeader) {
		buffer = block + sizePlusHeader;
		bufferEnd = block + blockSize;
	}
	return block + blockHeaderSize;
}


//
// Release all memory from this pool.
//
void Pool::clear()
{
	buffer = (ptr)&buffer;
	bufferEnd = (ptr)&buffer;
	BlockHeader *h = headerList;
	while (h) {
		BlockHeader *next = h->next;
	  #ifdef DEBUG
		memset(h, 0xFF, h->size);	// Wipe contents of block
	  #endif
		free(h);
		h = next;
	}
	headerList = 0;
}

size_t Pool::totalSize()
{
  size_t size = 0;

  for (BlockHeader *block = headerList; block; block = block->next)
	size += block->size;

  return size;
}

//
// Global operator new with a Pool argument.
//
NS_EXPORT void *operator new(size_t size, Pool &pool)
{
	return pool.allocate(size);
}


#ifndef __MWERKS__
//
// Global array operator new with a Pool argument.
//
NS_EXPORT void *operator new[](size_t size, Pool &pool)
{
	return pool.allocate(size);
}
#endif

