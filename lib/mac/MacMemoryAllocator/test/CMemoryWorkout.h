/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

//----------------------------------------------------------------------------------------
/*
	CMemoryWorkout.h
	
	See the .cp file for an explanation of what this stuff does.
	
*/
//----------------------------------------------------------------------------------------


#include <LPeriodical.h>
#include <LArray.h>

class CMemoryBlock
{
private:
	
	enum
	{
		eBlockStartMarker 	= 'SMFR',
		eBlockEndMarker		= 'smfr'
	};
	
	enum
	{
		eBlockFillPattern	= '0xBA'
	};
		
public:

				CMemoryBlock();
				CMemoryBlock(UInt32 inBlockSize);
		
				~CMemoryBlock();
			
	Boolean		QuickCheckBlock();
	Boolean		ThoroughCheckBlock();

	void		operator++(int);
	void		operator--(int);
	void		operator+=(UInt32 incSize);
	void		operator-=(UInt32 decSize);
	
protected:

	char		*GetBlockData() { return mBlockData; }
	UInt32		GetBlockSize()	{ return mBlockSize; }

	void		ChangeBlockSize(UInt32 newSize);
	void		InsertCheckBlocks();
	
	long		*GetBlockStartPtr();
	long		*GetBlockEndPtr();
	
	char		*mBlockData;
	UInt32		mBlockSize;
};


//----------------------------------------------------------------------------------------


class CMemoryTester
{
public:
					CMemoryTester();
					~CMemoryTester();
	
	void			DoMemoryTesting();
	
protected:

	LArray			mBlocksArray;
};


//----------------------------------------------------------------------------------------

class CMemoryTesterPeriodical : public LPeriodical
{
public:
					CMemoryTesterPeriodical();
					~CMemoryTesterPeriodical();
	
	virtual void	SpendTime(const EventRecord	&inMacEvent);

private:
	
	void			FreeTesters();
	
	LArray			mTestersArray;
};
