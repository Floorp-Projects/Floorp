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

/* file: absearch.cpp
** Some portions derive from public domain IronDoc code and interfaces.
**
** Changes:
** <1> 05Jan1998 first implementation
** <0> 23Dec1997 first interface draft
*/

#ifndef _ABTABLE_
#include "abtable.h"
#endif

#ifndef _ABMODEL_
#include "abmodel.h"
#endif

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_Search_kClassName /*i*/ = "ab_Search";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods
char* ab_Search::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	sprintf(outXmlBuf,
		"<ab:search:str me=\"^%lX\" c:c=\"%lu:lu\" row=\"#%lX\" st=\"^%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                        // me=\"^%lX\"
		(long) mSearch_Capacity,            // c:c=\"%lu
		(long) mSearch_ColumnCount,         // :lu\"
		(long) mPart_RowUid,                // row=\"#%lX\"
		(long) mPart_Store,                 // st=\"^%lX\"
		(unsigned long) mObject_RefCount,   // rc=\"%lu\"
		this->GetObjectAccessAsString(),    // ac=\"%.9s\"
		this->GetObjectUsageAsString()      // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_Search::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseSearch(ev);
		this->MarkShut();
	}
}

void ab_Search::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:search>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PushDepth(ev); // indent following content

		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
		
		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, mPart_Store->ObjectAsString(ev, xmlBuf));
		
		if ( mSearch_Row )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mSearch_Row->PrintObject(ev, ioPrinter);
		}
		
		if ( mSearch_ColumnCount && mSearch_Columns )
		{
			ab_column_uid* cols = mSearch_Columns - 1; // before
			ab_column_uid* end = mSearch_Columns + mSearch_ColumnCount; // after
			
			ioPrinter->PutString(ev, "<ab:search:columns>");
			ioPrinter->PushDepth(ev); // indent following content
			while ( ++cols < end )
			{
				ab_column_uid c = *cols;
				const char* colString = AB_ColumnUid_AsString(c, ev->AsSelf());
				if ( !colString )
					colString = "";
				ioPrinter->NewlineIndent(ev, /*count*/ 1);
				ioPrinter->Print(ev, "<ab:column uid=\"#%lX\" name=\"%.64s\"/>",
					(long) c, colString);
			}
			ioPrinter->PopDepth(ev); // stop indentation
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			ioPrinter->PutString(ev, "</ab:search:columns>");
		}
		
		if ( mSearch_ColumnCount && mSearch_Strings )
		{
			ab_String** strings = mSearch_Strings - 1; // before
			ab_String** end = mSearch_Strings + mSearch_ColumnCount; // after

			ioPrinter->PutString(ev, "<ab:search:strings>");
			ioPrinter->PushDepth(ev); // indent following content
			
			while ( ++strings < end )
			{
				ab_String* s = *strings;
				if ( s )
				{
					ioPrinter->NewlineIndent(ev, /*count*/ 1);
					s->PrintObject(ev, ioPrinter);
				}
			}
			ioPrinter->PopDepth(ev); // stop indentation
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			ioPrinter->PutString(ev, "</ab:search:strings>");
		}
		
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab:search>");
#endif /*AB_CONFIG_PRINT*/
}

ab_Search::~ab_Search() /*i*/
{
	AB_ASSERT(mSearch_Columns==0);
	AB_ASSERT(mSearch_Strings==0);
	AB_ASSERT(mSearch_Row==0);

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	ab_Object* obj = mSearch_Row;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_Search_kClassName);
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
}
        
// ````` ````` ````` `````   ````` ````` ````` `````  
// protected non-poly ab_Search methods

void ab_Search::CutSearchStorageSpace(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Search_kClassName, "GrowSearchCapacity")

	ab_String** strings = mSearch_Strings;
	if ( strings )
	{
		mSearch_Strings = 0;
		if ( strings != mSearch_ImmedStrings ) 
		{
			ab_String** cursor = strings - 1;
			ab_String** end = strings + mSearch_ColumnCount; // after
			
			while ( ++strings < end )
			{
				ab_String* s = *cursor;
				if ( s )
				{
					s->ReleaseObject(ev);
					*cursor = 0;
				}
			}
			ev->HeapFree(strings);
		}
	}

	ab_column_uid* cols = mSearch_Columns;
	if ( cols )
	{
		mSearch_Columns = 0;
		if ( cols != mSearch_ImmedCols ) 
			ev->HeapFree(cols);
	}

	mSearch_Capacity = ab_Search_kImmedColCount;
	mSearch_Columns = mSearch_ImmedCols;
	mSearch_Strings = mSearch_ImmedStrings;

	ab_Env_EndMethod(ev)
}

ab_bool ab_Search::GrowSearchCapacity(ab_Env* ev, /*i*/
	ab_column_count inNewCapacity)
{
	ab_Env_BeginMethod(ev, ab_Search_kClassName, "GrowSearchCapacity")
	
	ab_error_uid newError = 0;

	if ( this->IsOpenObject() && inNewCapacity > mSearch_Capacity )
	{
		if ( inNewCapacity < ab_Search_kMaxCapacity )
		{
			if ( mSearch_Strings && mSearch_Columns )
			{
				ab_column_uid* newCols = (ab_column_uid*)
					ev->HeapAlloc(inNewCapacity * sizeof(ab_column_uid));
				if ( newCols )
				{
					ab_String** newStrings = (ab_String**)
						ev->HeapAlloc(inNewCapacity * sizeof(ab_String*));
					if ( newStrings )
					{
						ab_num oldCount = mSearch_ColumnCount;
						
						ab_num size = oldCount * sizeof(ab_column_uid);
						memcpy(newCols, mSearch_Columns, size);
						
						size = oldCount * sizeof(ab_String*);
						memcpy(newStrings, mSearch_Strings, size);
						
						this->CutSearchStorageSpace(ev);
						mSearch_Capacity = inNewCapacity;
						mSearch_Strings = newStrings;
						mSearch_Columns = newCols;
					}
					else
					{
						// newError = AB_Env_kFaultOutOfMemory;
						ev->HeapFree(newCols);
					}
				}
				// else newError = AB_Env_kFaultOutOfMemory;
			}
			else newError = ab_Search_kFaultMissingArray;
		}
		else newError = ab_Search_kFaultOverMaxCapacity;
	}
	
	if ( newError )
		ev->NewAbookFault(newError);

	ab_Env_EndMethod(ev)
	return ev->Good();
}

ab_bool ab_Search::InitCapacity(ab_Env* ev, ab_column_count inCapacity) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Search_kClassName, "InitCapacity")

	ab_num size = ab_Search_kImmedColCount * sizeof(ab_column_uid);
	XP_MEMSET(mSearch_ImmedCols, 0, size);
	
	size = ab_Search_kImmedColCount * sizeof(ab_String*);
	XP_MEMSET(mSearch_ImmedStrings, 0, size);
	
	if ( inCapacity > ab_Search_kImmedColCount ) // need more capacity?
	{
		if ( inCapacity < ab_Search_kMaxCapacity ) // not unreasonably large?
		{
			this->GrowSearchCapacity(ev, inCapacity);
		}
		else ev->NewAbookFault(ab_Search_kFaultOverMaxCapacity);
	}

	ab_Env_EndMethod(ev)
	return ev->Good();
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// public non-poly ab_Search methods

ab_Search::ab_Search(ab_Env* ev, const ab_Usage& inUsage,  /*i*/
	ab_Store* ioStore, const char* inStringValue,
	const ab_column_uid* inColUidVector, ab_column_count inCapacity)
	// inColUidVector is null terminated, so use the number of non-null
	// uid's as instead of inCapacity if this number is larger (so a
	// zero value can be passed for  inCapacity when desired).   Use
	// the same ab_String made from inStringValue for all strings.
	
	: ab_Part(ev, inUsage, ab_Session::GetGlobalSession()->NewTempRowUid(),
		ioStore),
	mSearch_Capacity( ab_Search_kImmedColCount ),
	mSearch_ColumnCount( 0 ),
	mSearch_Columns( mSearch_ImmedCols ),
	mSearch_Strings( mSearch_ImmedStrings )
{
	ab_Env_BeginMethod(ev, ab_Search_kClassName, ab_Search_kClassName)
	
	const ab_column_uid* cols = inColUidVector;
	while ( *cols ) // find the vector's null terminator
		++cols;
	ab_column_count vectorLength = cols - inColUidVector;
	const ab_column_uid* end = cols; // one after last non-null col
	
	if ( vectorLength > inCapacity )
		inCapacity = vectorLength + 1; // add 1 for gratuitous extra space
	
	if ( this->InitCapacity(ev, inCapacity) )
	{
		ab_String* string = new(*ev)
			ab_String(ev, ab_Usage::kHeap, inStringValue);
		if ( string )
		{
			if ( ev->Good() )
			{
				cols = inColUidVector - 1;
				while ( ++cols < end && ev->Good() )
				{
					this->AddSearchString(ev, string, *cols);
				}
			}
			string->ReleaseObject(ev); // always release our reference
		}
		// else ev->NewAbookFault(AB_Env_kFaultOutOfMemory);
	}
	
	ab_Env_EndMethod(ev)
}

ab_Search::ab_Search(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	ab_Store* ioStore, ab_column_count inCapacity)
	// expect to have inColHint columns added with AddSearchString()
	: ab_Part(ev, inUsage, ab_Session::GetGlobalSession()->NewTempRowUid(),
		ioStore),
	mSearch_Capacity( ab_Search_kImmedColCount ),
	mSearch_ColumnCount( 0 ),
	mSearch_Columns( mSearch_ImmedCols ),
	mSearch_Strings( mSearch_ImmedStrings )
{
	ab_Env_BeginMethod(ev, ab_Search_kClassName, ab_Search_kClassName)

	this->InitCapacity(ev, inCapacity);

	ab_Env_EndMethod(ev)
}
  
void ab_Search::CloseSearch(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Search_kClassName, "CloseSearch")

	ab_Object* obj = mSearch_Row;
	if ( obj )
	{
		mSearch_Row = 0;
		obj->ReleaseObject(ev);
	}

	this->CutSearchStorageSpace(ev);
	
	this->ClosePart(ev);

	ab_Env_EndMethod(ev)
}

ab_bool ab_Search::CutAllSearchStrings(ab_Env* ev) /*i*/
     // return value equals ev->Good()
{
	ab_Env_BeginMethod(ev, ab_Search_kClassName, "CutAllSearchStrings")
	
	this->CutSearchStorageSpace(ev);
	mSearch_ColumnCount = 0;
	
	ab_Env_EndMethod(ev)
	return ev->Good();
}

ab_bool ab_Search::AddSearchString(ab_Env* ev, ab_String* ioString, /*i*/
     ab_column_uid inColUid)
     // return value equals ev->Good()
{
	ab_Env_BeginMethod(ev, ab_Search_kClassName, "AddSearchString")
	
	if ( this->IsOpenObject() ) // string appears minimally valid?
	{
		if ( AB_Uid_IsColumn(inColUid) ) // uid seems to be for a column?
		{
			ab_column_count oldCount = mSearch_ColumnCount;
			if ( oldCount >= mSearch_Capacity ) // need bigger arrays?
			{
				// at least two more than needed, and 25 percent more still:
				ab_column_count newCap = oldCount + 2 + (mSearch_Capacity / 4);
				this->GrowSearchCapacity(ev, newCap);
			}
			if ( ev->Good() && mSearch_Capacity > oldCount )
			{
				if ( mSearch_Columns && mSearch_Strings ) // arrays okay?
				{
					if ( ioString->AcquireObject(ev) ) // acquire succeeds?
					{
						mSearch_Strings[ oldCount ] = ioString;
						mSearch_Columns[ oldCount ] = inColUid;
						mSearch_ColumnCount = ++oldCount;
					}
				}
				else ev->NewAbookFault(ab_Search_kFaultMissingArray);
			}
		}
		else ev->NewAbookFault(AB_Column_kFaultNotColumnUid);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);
		
	ab_Env_EndMethod(ev)
	return ev->Good();
}

// ````` ````` accessors ````` `````

ab_bool ab_Search::ChangeSearchRow(ab_Env* ev, ab_Row* ioRow) /*i*/
	// note that passing in a null pointer for ioRow is allowed.
{
	ab_Env_BeginMethod(ev, ab_Search_kClassName, "ChangeSearchRow")

	if ( this->IsOpenObject() )
	{
		ab_Row* oldRow = mSearch_Row;
		if ( oldRow )
		{
			mSearch_Row = 0;
			oldRow->ReleaseObject(ev);
		}
		if ( ev->Good() && ioRow && ioRow->AcquireObject(ev) )
			mSearch_Row = ioRow;
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
	return ev->Good();
}

ab_Row* ab_Search::GetSearchRow(ab_Env* ev) const /*i*/
{
	ab_Row* outRow = 0;
	ab_Env_BeginMethod(ev, ab_Search_kClassName, "GetSearchRow")
	
	if ( this->IsOpenObject() )
	{
		outRow = mSearch_Row;
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);
	
	ab_Env_EndMethod(ev)
	return outRow;
}

ab_String* ab_Search::GetStringAt(ab_Env* ev, ab_pos inPos, /*i*/
	ab_column_uid* outColUid) const
	// inPos is a zero-based index from zero to GetSearchColumnCount()-1.
{
	ab_String* outString = 0;
	ab_Env_BeginMethod(ev, ab_Search_kClassName, "GetStringAt")

	if ( this->IsOpenObject() )
	{
		if ( mSearch_ColumnCount && inPos < mSearch_ColumnCount )
		{
			if ( mSearch_Strings && mSearch_Columns )
			{
				outString = mSearch_Strings[ inPos ];
				if ( outColUid )
					*outColUid = mSearch_Columns[ inPos ];
			}
			else ev->NewAbookFault(ab_Search_kFaultMissingArray);
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);
	
	ab_Env_EndMethod(ev)
	return outString;
}