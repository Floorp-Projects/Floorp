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
//
// simon
// 

#include "JavaVM.h"
#include "ExceptionTable.h"
#include "CatchAssert.h"
#include "LogModule.h"

UT_DEFINE_LOG_MODULE(ExceptionTable);

//-----------------------------------------------------------------------------------------------------------
// ExceptionTableBuilder

// METHOD addEntry
// In/Out:		index -- created handler must be at or after stated index
// In			newEntry -- entry to add to exception table
// Assumptions:	entries must be added in ascending scheduled node order
void ExceptionTableBuilder::
addEntry(Uint32& index, ExceptionTableEntry& newEntry)
{
	Uint32 tableSize = eTable.size();

	// iterate through exception table trying to find an existing entry we can coalesce with
	for(; index < tableSize; index++)
	{
		ExceptionTableEntry& ete = eTable[index];

		// can't coalesce if handler, type or policy differ
		if(	(ete.pHandler != newEntry.pHandler) || 
			(ete.pExceptionType != newEntry.pExceptionType))
			continue;
			
		// we require that the above noted assumption has been met
		assert(ete.pEnd <= newEntry.pStart);

		// extend range of existing exception table entry (coalesce) if possible
		if(ete.pEnd == newEntry.pStart)
		{
			ete.pEnd = newEntry.pEnd;	// coalesce ranges
#ifdef DEBUG_LOG
			UT_LOG(ExceptionTable, PR_LOG_ALWAYS, ("merge"));
			newEntry.print(UT_LOG_MODULE(ExceptionTable));
#endif
			return;	// index will point to index of table entry that was extended
		}
	}

	// coalesce wasn't possible, so insert
	eTable.append(newEntry); 

#ifdef DEBUG_LOG
	UT_LOG(ExceptionTable, PR_LOG_ALWAYS, ("add   "));
	newEntry.print(UT_LOG_MODULE(ExceptionTable));
#endif
	// index will be correctly set from end of 'for' loop
}

//-----------------------------------------------------------------------------------------------------------

#ifdef DEBUG
// check important assumption that array is sorted and valid
// if it is not then nested trys will probably fail
void ExceptionTableBuilder::
checkInOrder()
{
	if(eTable.begin() == eTable.end())
		return;

	ExceptionTableEntry *curr, *next, *last;
	curr= eTable.begin();
	last = eTable.end() - 1;

	while(curr < last) {
		next = curr + 1;
		assert(curr->pStart <= curr->pEnd);		// sanity check
		assert(curr->pStart <= next->pStart);	// ensure that items are sorted by pStart field
		curr = next;
	}
}
#endif	// DEBUG

//-----------------------------------------------------------------------------------------------------------
// Constructor
// builds an exception table in the specified pool
ExceptionTable::
ExceptionTable(ExceptionTableBuilder eBuilder, Pool& inPool)
{
	numberofEntries = eBuilder.getNumberofEntries();

	start = eBuilder.start;
	if(numberofEntries == 0)
	{
		mEntries = NULL;
	}
	else
	{
		Uint32 i;
		mEntries = new(inPool) ExceptionTableEntry[numberofEntries];
		for(i = 0; i < numberofEntries; i++)
			mEntries[i] = eBuilder.getEntry(i);
		Uint32 sz = eBuilder.getNumberofAPoints();
		asynchPoints = new(inPool) Uint32[sz];
		for(i = 0; i < sz; i++)
			asynchPoints[i] = eBuilder.getAEntry(i);
	}
}

// In:	thrown object
// In:	PC/EIP of 
// Out:	pointer to an ExceptionTableEntry that matches the thrown object (or NULL if no match)
ExceptionTableEntry*
ExceptionTable::
findCatchHandler(const Uint8* pc, const JavaObject& inObject)
{
	const Class& clazz =  inObject.getClass();

	// scan through the handler list in _reverse_ looking for a match
	// reverse scanning is needed to properly respect nested try blocks
	if(numberofEntries > 0)
		for(Int32 i = numberofEntries - 1; i >= 0; i--)
		{
			ExceptionTableEntry* tableEntry = mEntries + i;
			if( (pc >= start + tableEntry->pStart) && 
				(pc < start + tableEntry->pEnd) && 
				clazz.implements( *(tableEntry->pExceptionType) ) )
				return tableEntry;
		}

	return NULL;
}

//-----------------------------------------------------------------------------------------------------------
// DEBUG code
#ifdef DEBUG_LOG


void ExceptionTable::
print(LogModuleObject &f)
{
	if(numberofEntries == 0)
	{
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("The exception table is empty.\n"));
	}
	else
	{
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("(begin-offset, end-offset) -> handler-offset (exception caught)\n"));
		for(Uint32 i = 0; i < numberofEntries; i++)
			mEntries[i].print(f);
	}
}

void ExceptionTable::
printShort(LogModuleObject &f)
{
	if(numberofEntries != 0)
		for(Uint32 i = 0; i < numberofEntries; i++)
		{
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\t"));
			mEntries[i].print(f,start);
		}
}

#endif // DEBUG
