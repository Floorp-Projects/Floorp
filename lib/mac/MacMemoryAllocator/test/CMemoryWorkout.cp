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


//----------------------------------------------------------------------------------------
/*
	A set of classes for giving the memory allocators a thorough workout.
	
	CMemoryBlock
		This class manages a single memory block, which is allocated  with
		malloc(). The block is filled with a patter, and tag words put at
		each end. The various operators allow you to change the block size;
		for each change, the block is realloc'd and the tag words replaced.
		Before each operation, the block contents are checked to ensure that
		no-one wrote over them.
	
	CMemoryTester
		This class manages an array of CMemoryBlocks. In DoMemoryTesting(), it
		allocates about 200 block with random sizes in a wide range, then 
		makes them bigger, smaller, frees some, allocates some more, and resizes
		them again.
		
		Its destructor frees the blocks.
	
	CMemoryTesterPeriodical
		This is a periodical that runs the memory tests. In the current implementaion,
		it manages an array of CMemoryTesters. On each idle, it allocates a new
		CMemoryTester, and does the testing. That CMemoryTester is put in an 
		array to keep track of it (but the memory is not freed yet). So on every
		idle it "leaks" all the blocks managed by the tester. Memory soon gets
		used up.
		
		At some point an allocation will fail, and CMemoryBlock throws an exception.
		This is caught by the CMemoryTesterPeriodical, which then frees up all the
		CMemoryTesters. Next time around, it all starts again.
	
	How to use:

		Hook up your periodical in a convenient place like CFrontApp::Initialize():
			CMemoryTesterPeriodical	*tester = new CMemoryTesterPeriodical();
			tester->StartIdling();

		The app starts to run real slow, and if you watch memory use in the Finder,
		you will see it fluctuate wildly as the periodical leaks lots of memory,
		frees it all, then leaks it again. Since this is running on a periodical,
		you can be browsing the web at the same time, albeit slowly.
		
		If any errors are detected (e.g. the block tags have been overwritten),
		then you will assert.

*/
//----------------------------------------------------------------------------------------


#include <stdlib.h>
#include <memory.h>
#include <string.h>

#include <LArrayIterator.h>

#include "CMemoryWorkout.h"

#define kMinSize 8

#define MAX(a,b)	(((a) > (b)) ? (a) : (b))

//----------------------------------------------------------------------------------------
CMemoryBlock::CMemoryBlock()
:	mBlockData(NULL)
,	mBlockSize(0)
//----------------------------------------------------------------------------------------
{
}


//----------------------------------------------------------------------------------------
CMemoryBlock::CMemoryBlock(UInt32 inBlockSize)
:	mBlockSize(inBlockSize)
//----------------------------------------------------------------------------------------
{
	if (mBlockSize < kMinSize)
		mBlockSize = kMinSize;
	mBlockData = (char *)malloc(mBlockSize);
	ThrowIfNil_(mBlockData);
	
	InsertCheckBlocks();
}


//----------------------------------------------------------------------------------------
CMemoryBlock::~CMemoryBlock()
//----------------------------------------------------------------------------------------
{
	if (GetBlockData())
	{
		Boolean		isGood = QuickCheckBlock();
		Assert_(isGood);
	}
	
	free(mBlockData);
}


//----------------------------------------------------------------------------------------
long *CMemoryBlock::GetBlockStartPtr()
//----------------------------------------------------------------------------------------
{
	return (long *)mBlockData;
}


//----------------------------------------------------------------------------------------
long *CMemoryBlock::GetBlockEndPtr()
//----------------------------------------------------------------------------------------
{
	if (!mBlockData) return nil;
	
	char *dataEnd = mBlockData + mBlockSize;
	return (long *)(dataEnd - sizeof(long));
}


//----------------------------------------------------------------------------------------
Boolean CMemoryBlock::QuickCheckBlock()
//----------------------------------------------------------------------------------------
{
	ThrowIfNil_(mBlockData);
	
	long	*startCheckPtr = GetBlockStartPtr();
	long	*endCheckPtr = GetBlockEndPtr();
	
	return (*startCheckPtr == eBlockStartMarker && *endCheckPtr == eBlockEndMarker);
}


//----------------------------------------------------------------------------------------
Boolean CMemoryBlock::ThoroughCheckBlock()
//----------------------------------------------------------------------------------------
{
	if (!QuickCheckBlock())
		return false;
		
	return true;		// I'm too lazy to do the whole thing
}

//----------------------------------------------------------------------------------------
void CMemoryBlock::InsertCheckBlocks()
//----------------------------------------------------------------------------------------
{
	ThrowIfNil_(mBlockData);
	
	memset(mBlockData, eBlockFillPattern, mBlockSize);
	
	long	*startCheckPtr = GetBlockStartPtr();
	long	*endCheckPtr = GetBlockEndPtr();

	*startCheckPtr = eBlockStartMarker;
	*endCheckPtr  = eBlockEndMarker;
}


//----------------------------------------------------------------------------------------
void CMemoryBlock::ChangeBlockSize(UInt32 newSize)
//----------------------------------------------------------------------------------------
{
	ThrowIfNil_(mBlockData);
	
	Assert_(QuickCheckBlock());
	
	mBlockSize = MAX(newSize, kMinSize);
	mBlockData = (char *)realloc((void *)mBlockData, mBlockSize);

	ThrowIfNil_(mBlockData);
	
	InsertCheckBlocks();
}


//----------------------------------------------------------------------------------------
void CMemoryBlock::operator++(int)
//----------------------------------------------------------------------------------------
{
	ChangeBlockSize(mBlockSize + 1);
}

//----------------------------------------------------------------------------------------
void CMemoryBlock::operator--(int)
//----------------------------------------------------------------------------------------
{
	ChangeBlockSize(mBlockSize - 1);
}

//----------------------------------------------------------------------------------------
void CMemoryBlock::operator+=(UInt32 incSize)
//----------------------------------------------------------------------------------------
{
	ChangeBlockSize(mBlockSize + incSize);
}

//----------------------------------------------------------------------------------------
void CMemoryBlock::operator-=(UInt32 decSize)
//----------------------------------------------------------------------------------------
{
	if (decSize > mBlockSize)
		decSize = 0;
	UInt32		newSize = mBlockSize - decSize;
	ChangeBlockSize(newSize > kMinSize ? newSize : kMinSize);
}

#pragma mark -


//----------------------------------------------------------------------------------------
CMemoryTester::CMemoryTester()
:	mBlocksArray(sizeof(CMemoryBlock *))
//----------------------------------------------------------------------------------------
{
	
}


//----------------------------------------------------------------------------------------
CMemoryTester::~CMemoryTester()
//----------------------------------------------------------------------------------------
{
	LArrayIterator		iter(mBlocksArray);
	CMemoryBlock		*theBlock;
	
	while (iter.Next(&theBlock) && theBlock)
	{
		delete theBlock;
	}
	
	mBlocksArray.RemoveItemsAt(mBlocksArray.GetCount(), LArray::index_First);
}


//----------------------------------------------------------------------------------------
void CMemoryTester::DoMemoryTesting()
//----------------------------------------------------------------------------------------
{
	// let's make some objects with random sizes
	
	for (Int32 i = 0; i < 10; i ++)
	{
		CMemoryBlock	*newBlock = new CMemoryBlock( 1024 * (::Random() & 0x05FF) + (::Random() & 0x7FFF));
		
		mBlocksArray.InsertItemsAt(1, LArray::index_Last, &newBlock);
	}

	for (Int32 i = 0; i < 100; i ++)
	{
		CMemoryBlock	*newBlock = new CMemoryBlock( 1024 * (::Random() & 0x07F) + (::Random() & 0x7FFF));
		
		mBlocksArray.InsertItemsAt(1, LArray::index_Last, &newBlock);
	}
	
	for (Int32 i = 0; i < 3000; i ++)
	{
		CMemoryBlock	*newBlock = new CMemoryBlock(::Random() & 0x009F);
		
		mBlocksArray.InsertItemsAt(1, LArray::index_Last, &newBlock);
	}

	
	/* now let's resize them
	LArrayIterator		iter(mBlocksArray);
	CMemoryBlock		*theBlock;
	while (iter.Next(&theBlock) && theBlock)
	{
		(*theBlock)++;
	}

	iter.ResetTo(LArrayIterator::index_BeforeStart);
	while (iter.Next(&theBlock) && theBlock)
	{
		(*theBlock)++;
	}

	iter.ResetTo(LArrayIterator::index_BeforeStart);
	while (iter.Next(&theBlock) && theBlock)
	{
		(*theBlock)++;
	}

	iter.ResetTo(LArrayIterator::index_BeforeStart);
	while (iter.Next(&theBlock) && theBlock)
	{
		(*theBlock) -= 10;
	}
	
	iter.ResetTo(LArrayIterator::index_BeforeStart);
	while (iter.Next(&theBlock) && theBlock)
	{
		(*theBlock) += (::Random() & 0x07FF);
	}
	
	iter.ResetTo(LArrayIterator::index_BeforeStart);
	while (iter.Next(&theBlock) && theBlock)
	{
		(*theBlock) += 0;		// resize to same size
	}
	
	
	// now let's free every other block, and then reallocate them
	iter.ResetTo(LArrayIterator::index_BeforeStart);
	ArrayIndexT	n = LArray::index_First;
	
	while (iter.Next(&theBlock) && theBlock)
	{
		if ((n & 1) == 0)
		{
			delete theBlock;
			
			theBlock = new CMemoryBlock(::Random() & 0x09F);
			
			mBlocksArray.AssignItemsAt(1, n, &theBlock);
		}
		n++;
	}

	// now let's free every other block, and then reallocate them
	iter.ResetTo(LArrayIterator::index_BeforeStart);
	n = LArray::index_First;
	
	while (iter.Next(&theBlock) && theBlock)
	{
		if ((n & 1) == 1)
		{
			delete theBlock;
			
			theBlock = new CMemoryBlock(::Random() & 0x09F);

			mBlocksArray.AssignItemsAt(1, n, &theBlock);
		}
		n++;
	}
	*/
	/*
	iter.ResetTo(LArrayIterator::index_BeforeStart);
	while (iter.Next(&theBlock) && theBlock)
	{
		(*theBlock) -= (::Random() & 0x07FF);
	}
	*/
}

//----------------------------------------------------------------------------------------
CMemoryTesterPeriodical::CMemoryTesterPeriodical()
:	LPeriodical()
,	mTestersArray(sizeof(CMemoryTester *))
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
CMemoryTesterPeriodical::~CMemoryTesterPeriodical()
//----------------------------------------------------------------------------------------
{
	FreeTesters();
}

//----------------------------------------------------------------------------------------
void CMemoryTesterPeriodical::FreeTesters()
//----------------------------------------------------------------------------------------
{
	LArrayIterator		iter(mTestersArray);
	CMemoryTester		*theTester;
	
	while (iter.Next(&theTester) && theTester)
	{
		delete theTester;
	}
	
	mTestersArray.RemoveItemsAt(mTestersArray.GetCount(), LArray::index_First);
}

//----------------------------------------------------------------------------------------
void CMemoryTesterPeriodical::SpendTime(const EventRecord	& /*inMacEvent */)
//----------------------------------------------------------------------------------------
{
	try
	{
		CMemoryTester	*tester = new CMemoryTester();
		
		mTestersArray.InsertItemsAt(1, LArray::index_Last, &tester);
		
		tester->DoMemoryTesting();
		// leak the damn thing
	}
	catch (...)
	{
		DebugStr("\pWe ran out of memory. Free up time.");
		FreeTesters();
	}
}

