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

#ifndef POOL_H
#define POOL_H

#include "Fundamentals.h"

// Pooled memory allocation
class NS_EXTERN Pool
{
	struct BlockHeader
	{
		BlockHeader *next;						// Linked list of all memory blocks in pool
		size_t size;							// Physical size of this block, including header
	};

	enum {blockHeaderSize = sizeof(BlockHeader) + 7 & -8}; // BlockHeader size rounded up to a doubleword multiple

	const size_t initialSize;					// Size of first chunk to allocate
	const size_t chunkSize;						// Size of other chunks to allocate
	ptr buffer;									// Doubleword-aligned pointer to free memory buffer
	ptr bufferEnd;								// Doubleword-aligned pointer to end of free memory buffer
	BlockHeader *headerList;					// Root of linked list of all allocated blocks in this pool

  public:
	explicit Pool(size_t initialSize = 8192, size_t chunkSize = 1024);
	~Pool();
  private:
	Pool(const Pool &);							// Copying forbidden
	void operator=(const Pool &);				// Copying forbidden

	void *allocateOverflow(size_t size);
  public:
	void *allocate(size_t size);
	void clear();

	// Return the total amount of memory allocated by this pool
	size_t totalSize();
};


//
// Allocate a block of the given size from the pool.
//
inline void *Pool::allocate(size_t size)
{
	size = size + 7 & -8;	// Round up to a doubleword multiple
	if ((size_t)(bufferEnd - buffer) >= size) {
		ptr result = buffer;
		buffer += size;
		return result;
	}
	return allocateOverflow(size);
}

#endif



