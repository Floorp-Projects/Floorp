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

#ifndef _EXCEPTION_TABLE_
#define _EXCEPTION_TABLE_

#include "Fundamentals.h"
#include "Vector.h"
#include "JavaObject.h"
#include "LogModule.h"

//-----------------------------------------------------------------------------------------------------------
// ExceptionTableEntry
class ExceptionTableEntry
{
public:
	Uint32					pStart;
	Uint32					pEnd;				// offset to byte _after_ beginning of section
	const Class*			pExceptionType;		// points to type of the handler
	Uint32					pHandler;

#ifdef DEBUG_LOG
	void print(LogModuleObject &f,Uint8* s) 
	{
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("(%x,%x) -> %x (",Uint32(s+pStart), Uint32(s+pEnd), Uint32(s+pHandler)));
		pExceptionType->printRef(f);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, (")\n"));
	}
	void print(LogModuleObject &f) 
	{
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("(%d,%d) -> %d (",Uint32(pStart), Uint32(pEnd), Uint32(pHandler)));
		pExceptionType->printRef(f);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, (")\n"));
	}
#endif
};

//-----------------------------------------------------------------------------------------------------------
// ExceptionTableBuilder
class ExceptionTableBuilder
{
private:
	Vector<ExceptionTableEntry>	eTable;
	Vector<Uint32> asynchPoints;

	Uint8*                  start;				// points to start of method

public:
	ExceptionTableBuilder(Uint8* s) : eTable(), asynchPoints() { start = s; };

	void						addEntry(Uint32& index, ExceptionTableEntry& ete);
	const ExceptionTableEntry&	getEntry(Uint32 index) const  { assert(index < eTable.size()); return (eTable[index]); }
	Uint32						getNumberofEntries() const { return (eTable.size()); }
	void addAEntry(Uint32 s) {
		asynchPoints.append(s);
	}
	inline int getAEntry(int i) {
		return asynchPoints[i];
	}
	inline int getNumberofAPoints() {
		return asynchPoints.size();
	}

	friend class ExceptionTable;

	DEBUG_ONLY(void	checkInOrder());
};

//-----------------------------------------------------------------------------------------------------------
// ExceptionTable
class ControlNode;

class ExceptionTable
{
private:
	Uint32					numberofEntries;
	ExceptionTableEntry*	mEntries;
	int                     numberofAPoints;
	Uint32*                 asynchPoints;		// array of offsets for where the asynch. points are
	Uint8*                  start;				// points to start of method
	static ClassFileSummary* excClass;			// points to the class of RuntimeException
public:	
							ExceptionTable(ExceptionTableBuilder eBuilder, Pool& inPool);	
	ExceptionTableEntry*	findCatchHandler(const Uint8* pc, const JavaObject& inObject);
	Uint32					getNumberofEntries() { return numberofEntries; } 
	ExceptionTable(ExceptionTable& et) {
		numberofEntries = et.numberofEntries;
		mEntries = et.mEntries;
		numberofAPoints = et.numberofAPoints;
		asynchPoints = et.asynchPoints;
		start = et.start;
	}
	ExceptionTable& operator=(Uint8* newstart)
	{
		start = newstart;
		return *this;
	}

	inline Uint8* getStart() { return start; }
	inline Uint32* getAsynchPoints() { return asynchPoints; }
	inline int getNrOfAsynchPoints() { return numberofAPoints; }
	static void staticInit(ClassCentral &central);

#ifdef DEBUG_LOG
	void	print(LogModuleObject &f);
	void	printShort(LogModuleObject &f);
	void	printFormatted(LogModuleObject &f, ControlNode** inNodes, Uint32 numNodes, bool isHTML);
#endif // DEBUG_LOG
};

struct ExceptionTableCache 
{
	ExceptionTable eTable;
	JavaObject* exc;
	ExceptionTableCache(ExceptionTable* et, Uint8* i, JavaObject* ex) : eTable(*et) 
	{  
		eTable = i; // relocates the table
		exc = ex;
	}
};

//-----------------------------------------------------------------------------------------------------------

#endif	// _EXCEPTION_TABLE_
