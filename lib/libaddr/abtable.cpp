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

/* file: abtable.c 
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
AB_API_IMPL(char*) /* abtable.cpp */
AB_PosPair_AsXmlString(const AB_PosPair* self, char* outXmlBuf) /*i*/
  /* <ab:pos:pair first="%lu" last="%lu"/> */
{
	sprintf(outXmlBuf, "<ab:pos:pair first=\"%lu\" last=\"%lu\"/>",
		(long) self->sPosPair_First, (long) self->sPosPair_Last );
	return outXmlBuf;
}
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/


#if AB_CONFIG_TRACE_orDEBUG_orPRINT
AB_API_IMPL(char*) /* abtable.cpp */
AB_PosRange_AsXmlString(const AB_PosRange* self, char* outXmlBuf) /*i*/
  /* <ab:pos:range first="%lu" count="%lu"/> */
{
	sprintf(outXmlBuf, "<ab:pos:range first=\"%lu\" count=\"%lu\"/>",
		(long) self->sPosRange_First, (long) self->sPosRange_Count );
	return outXmlBuf;
}
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_Table_kClassName /*i*/ = "ab_Table";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* AB_Table_kClassName /*i*/ = "AB_Table";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


/* ===== ===== ===== ===== ab_Table ===== ===== ===== ===== */

// ````` ````` ````` `````   ````` ````` ````` `````  
// protected virtual ab_Table responses to ab_TableStoreView messages

/*virtual*/
void ab_Table::TableSeesBeginStoreFlux(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "TableSeesBeginStoreFlux")
	
	if ( this->IsOpenObject() )
	{
		if ( mModel_Views.HasMembers() )
			this->BeginModelFlux(ev);
	}
	ab_Env_EndMethod(ev)
}

/*virtual*/
void ab_Table::TableSeesEndStoreFlux(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "TableSeesEndStoreFlux")

	if ( this->IsOpenObject() )
	{
		if ( mModel_Views.HasMembers() )
			this->EndModelFlux(ev);
	}
	
	ab_Env_EndMethod(ev)
}

/*virtual*/
void ab_Table::TableSeesChangedStore(ab_Env* ev, /*i*/
	const ab_Change* inChange)
{
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "TableSeesChangedStore")

	if ( this->IsOpenObject() )
	{
		if ( mModel_Views.HasMembers() )
		{
			ab_error_uid newError = 0;
			ab_RowSet* rowSet = this->TableRowSet();
			if ( rowSet )
			{
				if ( rowSet->IsOpenObject() )
				{
#ifdef AB_CONFIG_TRACE
					if ( ev->DoTrace() )
						ev->Trace(
							 "<ab:table:sees:changed:store $$ me=\"^%lX\" rw=\"^%lX\" ch=\"^%lX\"/>",
							(long) this, (long) rowSet, (long) inChange);
#endif /*AB_CONFIG_TRACE*/
					
					rowSet->RowSetSeesChangedStore(ev, inChange);
				}
				else newError = ab_RowSet_kFaultNotOpen;
			}
			else newError = AB_Table_kFaultNullRowSetSlot;
			
			if ( newError )
			{
				ev->NewAbookFault(newError);
				this->CloseObject(ev);
			}
		}
		else
		{
#ifdef AB_CONFIG_TRACE
			if ( ev->DoTrace() )
				ev->Trace( "<ab:table:sees:changed:store:noviews $$ me=\"^%lX\" ch=\"^%lX\"/>",
					(long) this, (long) inChange);
#endif /*AB_CONFIG_TRACE*/
		}
	}
	
	ab_Env_EndMethod(ev)
}

/*virtual*/
void ab_Table::TableSeesClosingStore(ab_Env* ev, /*i*/
	const ab_Change* inChange)
{
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "TableSeesClosingStore")
	
	AB_USED_PARAMS_1(inChange);
	this->CloseObject(ev); // indirectly calls this->ClosingModel()
	
	ab_Env_EndMethod(ev)
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_Table::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	sprintf(outXmlBuf,
		"<ab:table:str me=\"^%lX\" row=\"#%lX\" st=\"^%lX\" t=\"%.16s\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                                // me=\"^%lX\"
		(long) mModel_RowUid,                       // row=\"#lX\"
		(long) mModel_Store,                        // st=\"^%lX\"
		this->GetTableTypeAsString(),               // t=\"%.16s\"
		(unsigned long) mObject_RefCount,           // rc=\"%lu\"
		this->GetObjectAccessAsString(),            // ac=\"%.9s\"
		this->GetObjectUsageAsString()              // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_Table::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseTable(ev);
		this->MarkShut();
	}
}

void ab_Table::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:table>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
		ioPrinter->PushDepth(ev); // indent all objects in the list
		
		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, mModel_Store->ObjectAsString(ev, xmlBuf));
		
		if ( mTable_View )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mTable_View->PrintObject(ev, ioPrinter);
		}
		
		if ( mTable_Defaults )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mTable_Defaults->PrintObject(ev, ioPrinter);
		}
		
		if ( mTable_NameSet )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mTable_NameSet->PrintObject(ev, ioPrinter);
		}
		
		if ( mTable_ColumnSet )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mTable_ColumnSet->PrintObject(ev, ioPrinter);
		}
		
		if ( mTable_RowSet )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mTable_RowSet->PrintObject(ev, ioPrinter);
		}
		
		if ( mTable_RowContent )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mTable_RowContent->PrintObject(ev, ioPrinter);
		}
		
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab:table>");
#endif /*AB_CONFIG_PRINT*/
}

ab_Table::~ab_Table() /*i*/
{
	
	AB_ASSERT(mTable_View==0);
	AB_ASSERT(mTable_Defaults==0);
	AB_ASSERT(mTable_NameSet==0);
	AB_ASSERT(mTable_ColumnSet==0);
	AB_ASSERT(mTable_RowSet==0);
	AB_ASSERT(mTable_RowContent==0);

#ifdef AB_CONFIG_DEBUG
	if ( mTable_View )
		mTable_View->ObjectNotReleasedPanic(ab_Table_kClassName);
	
	if ( mTable_Defaults )
		mTable_Defaults->ObjectNotReleasedPanic(ab_Table_kClassName);
	
	if ( mTable_NameSet )
		mTable_NameSet->ObjectNotReleasedPanic(ab_Table_kClassName);
	
	if ( mTable_ColumnSet )
		mTable_ColumnSet->ObjectNotReleasedPanic(ab_Table_kClassName);
	
	if ( mTable_RowSet )
		mTable_RowSet->ObjectNotReleasedPanic(ab_Table_kClassName);
	
	if ( mTable_RowContent )
		mTable_RowContent->ObjectNotReleasedPanic(ab_Table_kClassName);
#endif /*AB_CONFIG_DEBUG*/	
}


// ````` ````` ````` `````   ````` ````` ````` `````  
// specialized copying is protected

    
ab_bool ab_Table::TableHasAllSlots(ab_Env* ev) const /*i*/
{
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "TableHasAllSlots")

	if ( !mTable_View )
		ev->NewAbookFault(AB_Table_kFaultNullViewSlot);
	
	if ( !mTable_Defaults )
		ev->NewAbookFault(AB_Table_kFaultNullDefaultsSlot);
	
	if ( !mTable_NameSet )
		ev->NewAbookFault(AB_Table_kFaultNullNameSetSlot);
	
	if ( !mTable_ColumnSet )
		ev->NewAbookFault(AB_Table_kFaultNullColumnSetSlot);
	
	if ( !mTable_RowSet )
		ev->NewAbookFault(AB_Table_kFaultNullRowSetSlot);
	
	if ( !mTable_RowContent )
		ev->NewAbookFault(AB_Table_kFaultNullRowContentSlot);
	
	ab_Env_EndMethod(ev)
	return ev->Good();
}

ab_Table::ab_Table(ab_Env* ev, const ab_Table& other, /*i*/
	ab_RowSet* ioRowSet, ab_row_uid inRowUid, AB_Table_eType inType)
	
	: ab_Model(ev, ab_Usage::kHeap, (inRowUid)? inRowUid: other.GetPartRowUid(), 
	           other.GetPartStore()),
	mTable_Type((inType)? inType : other.mTable_Type),
	mTable_View( 0 )
{
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "ab_Table")
		
    mTable_Defaults = 0;
    mTable_NameSet = 0;
    mTable_ColumnSet = 0;
    mTable_RowSet = 0;
    mTable_RowContent = 0;
    mTable_HasOwnColumnSet = AB_kFalse;
    
    if ( !ioRowSet ) // use row set in other table if ioRowSet is nil
    	ioRowSet = other.mTable_RowSet;
    	    
	if ( ev->Good() && other.TableHasAllSlots(ev) )
	{
	    ab_TableStoreView* view = new(*ev)
		    ab_TableStoreView(ev, ab_Usage::kHeap, other.GetPartStore(), this);

		if ( view )
		{
			if ( ev->Good() )
				mTable_View = view;
			else
				view->ReleaseObject(ev);
		}
	}
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace( "<ab:table:ctor $$ me=\"^%lX\" view=\"^%lX\"/>",
			(long) this, (long) mTable_View);
#endif /*AB_CONFIG_TRACE*/

	if ( ev->Good() )
	{
		// set up local vars to show what objects need release after errors:
	    ab_Defaults* defaults = 0;
	    ab_NameSet* nameSet = 0;
	    ab_ColumnSet* columnSet = 0;
	    ab_RowSet* rowSet = 0;
	    ab_RowContent* rowContent = 0;
	    
	    // acquire other
	    if ( other.mTable_Defaults->AcquireObject(ev) )
	    {
	    	defaults = other.mTable_Defaults; // defaults acquired
		    if ( other.mTable_NameSet->AcquireObject(ev) )
		    {
		    	nameSet = other.mTable_NameSet; // name set acquired
			    if ( other.mTable_ColumnSet->AcquireObject(ev) )
			    {
			    	columnSet = other.mTable_ColumnSet; // col set acquired
				    if ( ioRowSet->AcquireObject(ev) )
				    {
				    	rowSet = ioRowSet; // row set acquired
					    if ( other.mTable_RowContent->AcquireObject(ev) )
					    {
					    	if ( rowSet->SetRowSetTable(ev, this) )
					    	{
						    	rowContent = other.mTable_RowContent;

							    mTable_Defaults = defaults;
							    mTable_NameSet = nameSet;
							    mTable_ColumnSet = columnSet;
							    mTable_RowSet = rowSet;
							    mTable_RowContent = rowContent;
							    mTable_HasOwnColumnSet = 
							    	other.mTable_HasOwnColumnSet;
					    	}
					    }
				    }
			    }
		    }
	    }
	    if ( ev->Bad() && !mTable_RowSet )
	    { // need to clean up acquires after failure?
	    	if ( defaults )   defaults->ReleaseObject(ev);
	    	if ( nameSet )    nameSet->ReleaseObject(ev);
	    	if ( columnSet )  columnSet->ReleaseObject(ev);
	    	if ( rowSet )     rowSet->ReleaseObject(ev);
	    	if ( rowContent ) rowContent->ReleaseObject(ev);
	    }
	}
	else this->MarkShut();
	
	ab_Env_EndMethod(ev)
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_Table methods


ab_Table::ab_Table(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	 ab_row_uid inRowUid, ab_Store* ioStore, AB_Table_eType inType)
	: ab_Model(ev, inUsage, inRowUid, ioStore),
	mTable_Type(inType),
	mTable_View( 0 )
{
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "ab_Table")

    mTable_Defaults = 0;
    mTable_NameSet = 0;
    mTable_ColumnSet = 0;
    mTable_RowSet = 0;
    mTable_RowContent = 0;
    mTable_HasOwnColumnSet = AB_kFalse;
    
	if ( ev->Good() )
	{
	    ab_TableStoreView* view = new(*ev)
		    ab_TableStoreView(ev, ab_Usage::kHeap, ioStore, this);
		
		if ( ev->Good() )
			mTable_View = view;
		else
			view->ReleaseObject(ev);
	}
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace( "<ab:table:ctor $$ me=\"^%lX\" view=\"^%lX\"/>",
			(long) this, (long) mTable_View);
#endif /*AB_CONFIG_TRACE*/

	ab_Env_EndMethod(ev)
}

ab_row_uid ab_Table::FindRowWithDistName(ab_Env* ev, const char* inDistName) /*i*/
	// cover for ab_RowContent method with the same name
{
	ab_row_uid outUid = 0;
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "MakeDefaultNameColumnTable")
	
	ab_RowContent* rowContent = mTable_RowContent;
	if ( rowContent && inDistName && rowContent->IsOpenObject() )
	{
		outUid = rowContent->FindRowWithDistName(ev, inDistName);
	}

	ab_Env_EndMethod(ev)
	return outUid;
}
 
/*static*/
ab_Table* ab_Table::MakeDefaultNameColumnTable(ab_Env* ev, /*i*/
	ab_row_uid inRowUid,  ab_Store* ioStore, AB_Table_eType inType)
{
	ab_Table* outTable = 0;
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "MakeDefaultNameColumnTable")
	
	// ab_row_uid id = ab_Session::GetGlobalSession()->NewTempRowUid();
	ab_Table* table = new(*ev)
		ab_Table(ev, ab_Usage::kHeap, inRowUid, ioStore, inType);
	if ( table )
	{
		if ( ev->Good() && table->IsOpenObject() )
		{
			ab_NameSet* nameSet =
				new(*ev) ab_NameSet(ev, ab_Usage::kHeap, ioStore);
			if ( nameSet )
			{
				if ( ev->Good() && table->SetTableNameSet(ev, nameSet) )
				{
					ab_Defaults* defaults =
						new(*ev) ab_Defaults(ev, ab_Usage::kHeap, ioStore);
					if ( defaults )
					{
						if ( ev->Good() && table->SetTableDefaults(ev, defaults) )
						{
							ab_ColumnSet* columnSet =
								new(*ev) ab_ColumnSet(ev, ab_Usage::kHeap, ioStore);
							if ( columnSet )
							{
								if ( ev->Good() )
									table->SetTableColumnSet(ev, columnSet);
									
								columnSet->ReleaseObject(ev); // always release
							}
						}
						defaults->ReleaseObject(ev); // always, error or not
					}
				}
				nameSet->ReleaseObject(ev); // always, error or not
			}
		}
		
		if ( ev->Good() )
			outTable = table;
		else
			table->ReleaseObject(ev);
	}
	ab_Env_EndMethod(ev)
	return outTable;
}

void ab_Table::CloseTable(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "CloseTable")

	ab_Object* obj = mTable_View;
	if ( obj )
	{
		mTable_View = 0;
		obj->ReleaseObject(ev);
	}

	obj = mTable_Defaults;
	if ( obj )
	{
		mTable_Defaults = 0;
		obj->ReleaseObject(ev);
	}
	
	obj = mTable_NameSet;
	if ( obj )
	{
		mTable_NameSet = 0;
		obj->ReleaseObject(ev);
	}
	
	obj = mTable_ColumnSet;
	if ( obj )
	{
		mTable_ColumnSet = 0;
		obj->ReleaseObject(ev);
	}
	
	obj = mTable_RowSet;
	if ( obj )
	{
		mTable_RowSet = 0;
		obj->ReleaseObject(ev);
	}
	
	obj = mTable_RowContent;
	if ( obj )
	{
		mTable_RowContent = 0;
		obj->ReleaseObject(ev);
	}
	this->CloseModel(ev);

	ab_Env_EndMethod(ev)
}

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
static const char* ab_Table_gTypeStrings[ ] =
{
	"kNone",           // 0
	"kGlobal",         // 1
	"kAddressBook",    // 2
	"kDirectory",      // 3
	"kMailingList",    // 4
	"kParentList",     // 5
	"kListSubset",     // 6
	"kSearchResult",   // 7
	"?table?"          // 8
};
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/

const char* ab_Table::GetTableTypeAsString() const /*i*/
{
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	const char* typeString = "broken";
	AB_Table_eType type = mTable_Type;
	if ( this->IsObject() )
	{
		if ( type > AB_Table_kNumberOfTypes )
			type = AB_Table_kNumberOfTypes;
		typeString = ab_Table_gTypeStrings[ type ];
	}
	else
		typeString = "nonObject";
#else
	const char* typeString = "?type?";
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return typeString;
}

const char* ab_Table::GetColumnName(ab_Env* ev, /*i*/
	 ab_column_uid inColUid) const
{
	const char* outName = 0;
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "GetColumnName")

	if ( this->IsOpenObject() )
	{
		if ( mTable_NameSet )
		{
			outName = mTable_NameSet->GetName(ev, inColUid);
		}
		else ev->NewAbookFault(AB_Table_kFaultNullNameSetSlot);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);
	
	ab_Env_EndMethod(ev)
	return outName;
}

ab_bool ab_Table::SetTableDefaults(ab_Env* ev, ab_Defaults* ioDefaults)
{
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "SetTableDefaults")
	
	if ( this->IsOpenObject() )
	{
		if ( ev->Good() )
			ioDefaults->AcquireObject(ev);
		if ( ev->Good() )
		{
			if ( mTable_Defaults )
				mTable_Defaults->ReleaseObject(ev);
			mTable_Defaults = ioDefaults; // already acquired
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
	return ev->Good();
}

ab_bool ab_Table::SetTableNameSet(ab_Env* ev, ab_NameSet* ioNameSet) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "SetTableNameSet")
	
	if ( this->IsOpenObject() )
	{
		if ( ev->Good() )
			ioNameSet->AcquireObject(ev);
		if ( ev->Good() )
		{
			if ( mTable_NameSet )
				mTable_NameSet->ReleaseObject(ev);
			mTable_NameSet = ioNameSet; // already acquired
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
	return ev->Good();
}

ab_bool ab_Table::SetTableColumnSet(ab_Env* ev, ab_ColumnSet* ioColumnSet) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "SetSetTableColumnSet")
	
	if ( this->IsOpenObject() )
	{
		if ( ev->Good() )
			ioColumnSet->AcquireObject(ev);
		if ( ev->Good() )
		{
			if ( mTable_ColumnSet )
				mTable_ColumnSet->ReleaseObject(ev);
			mTable_ColumnSet = ioColumnSet; // already acquired
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
	return ev->Good();
}

ab_bool ab_Table::SetTableRowSet(ab_Env* ev, ab_RowSet* ioRowSet) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "SetTableRowSet")
	
	if ( this->IsOpenObject() )
	{
		if ( ev->Good() )
		{
			if ( ioRowSet->AcquireObject(ev) )
				ioRowSet->SetRowSetTable(ev, this);
		}
		if ( ev->Good() )
		{
			if ( mTable_RowSet )
				mTable_RowSet->ReleaseObject(ev);
				
			mTable_RowSet = ioRowSet; // already acquired
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
	return ev->Good();
}

ab_bool ab_Table::SetTableRowContent(ab_Env* ev, ab_RowContent* ioContent) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Table_kClassName, "SetTableRowContent")
	
	if ( this->IsOpenObject() )
	{
		if ( ev->Good() )
			ioContent->AcquireObject(ev);
		if ( ev->Good() )
		{
			if ( mTable_RowContent )
				mTable_RowContent->ReleaseObject(ev);

			mTable_RowContent = ioContent; // already acquired
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
	return ev->Good();
}


/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

AB_FILE_IMPL(ab_Defaults*)
AB_Table_get_defaults(const AB_Table* self, ab_Env* ev)
{
	ab_Defaults* defaults = 0;

	const ab_Table* table = ab_Table::AsConstThis(self);
	if ( table->IsOpenObject() )
	{
		defaults = table->TableDefaults();
		if ( !defaults )
		{
			ab_Env_BeginMethod(ev, AB_Table_kClassName, "get_defaults")
			ev->NewAbookFault(AB_Table_kFaultNullNameSetSlot);
			table->CastAwayConstCloseObject(ev);
			ab_Env_EndMethod(ev)
		}
	}
	else ev->NewAbookFault(AB_Table_kFaultNotOpenTable);
	
	return defaults;
}

AB_FILE_IMPL(ab_NameSet*)
AB_Table_get_name_set(const AB_Table* self, ab_Env* ev)
{
	ab_NameSet* nameSet = 0;

	const ab_Table* table = ab_Table::AsConstThis(self);
	if ( table->IsOpenObject() )
	{
		nameSet = table->TableNameSet();
		if ( !nameSet )
		{
			ab_Env_BeginMethod(ev, AB_Table_kClassName, "get_name_set")
			ev->NewAbookFault(AB_Table_kFaultNullNameSetSlot);
			table->CastAwayConstCloseObject(ev);
			ab_Env_EndMethod(ev)
		}
	}
	else ev->NewAbookFault(AB_Table_kFaultNotOpenTable);
	
	return nameSet;
}

AB_FILE_IMPL(ab_ColumnSet*)
AB_Table_get_col_set(const AB_Table* self, ab_Env* ev)
{
	ab_ColumnSet* colSet = 0;

	const ab_Table* table = ab_Table::AsConstThis(self);
	if ( table->IsOpenObject() )
	{
		colSet = table->TableColumnSet();
		if ( !colSet )
		{
			ab_Env_BeginMethod(ev, AB_Table_kClassName, "get_col_set")
			ev->NewAbookFault(AB_Table_kFaultNullColumnSetSlot);
			table->CastAwayConstCloseObject(ev);
			ab_Env_EndMethod(ev)
		}
	}
	else ev->NewAbookFault(AB_Table_kFaultNotOpenTable);
	
	return colSet;
}

AB_FILE_IMPL(ab_RowSet*)
AB_Table_get_row_set(const AB_Table* self, ab_Env* ev)
{
	ab_RowSet* rowSet = 0;

	const ab_Table* table = ab_Table::AsConstThis(self);
	if ( table->IsOpenObject() )
	{
		rowSet = table->TableRowSet();
		if ( !rowSet )
		{
			ab_Env_BeginMethod(ev, AB_Table_kClassName, "get_row_set")
			ev->NewAbookFault(AB_Table_kFaultNullRowSetSlot);
			table->CastAwayConstCloseObject(ev);
			ab_Env_EndMethod(ev)
		}
	}
	else ev->NewAbookFault(AB_Table_kFaultNotOpenTable);
	
	return rowSet;
}

AB_FILE_IMPL(ab_RowContent*)
AB_Table_get_row_content(const AB_Table* self, ab_Env* ev)
{
	ab_RowContent* content = 0;

	const ab_Table* table = ab_Table::AsConstThis(self);
	if ( table->IsOpenObject() )
	{
		content = table->TableRowContent();
		if ( !content )
		{
			ab_Env_BeginMethod(ev, AB_Table_kClassName, "get_row_content")
			ev->NewAbookFault(AB_Table_kFaultNullRowContentSlot);
			table->CastAwayConstCloseObject(ev);
			ab_Env_EndMethod(ev)
		}
	}
	else ev->NewAbookFault(AB_Table_kFaultNotOpenTable);
	
	return content;
}


/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


AB_API_IMPL(AB_Table*)
AB_Table_GetBookTable(const AB_Table* self, AB_Env* cev) /*i*/
	/*- GetBookTable returns the address book table containing this table. If
	self is itself the address book then self is returned. -*/
{
	AB_Table* outTable = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "GetBookTable")

	const ab_Table* table = ab_Table::AsConstThis(self);
	if ( table->IsOpenObject() )
	{
		ab_Store* store = table->GetPartStore();
		outTable = store->GetTopStoreTable(ev)->AsSelf();
	}
	else ev->NewAbookFault(AB_Table_kFaultNotOpenTable);

	ab_Env_EndMethod(ev)
	
	return outTable;
}


/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

AB_API_IMPL(AB_Table_eType)
AB_Table_GetType(AB_Table* self, AB_Env* cev) /*i*/
	/*- IsBookTable returns whether table t is an address book. -*/
{
	AB_Table_eType outType = AB_Table_kNone;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "GetType")

	const ab_Table* table = ab_Table::AsConstThis(self);
	if ( table->IsOpenObject() )
	{
		outType = table->TableType();
	}
	else ev->NewAbookFault(AB_Table_kFaultNotOpenTable);

	ab_Env_EndMethod(ev)
	return outType;
}



AB_API_IMPL(ab_row_uid)
AB_Table_GetTableRowUid(AB_Table* self, AB_Env* cev) /*i*/
	/*- GetTableRowUid returns the row uid for table self. If self is a global
	table (AB_Type_kGlobalTable, AB_GetGlobalTable()), then all such
	global uid's are globally unique. If self is a list inside a p -*/
{
	ab_row_uid outUid = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "GetTableRowUid")

	const ab_Table* table = ab_Table::AsConstThis(self);
	if ( table->IsOpenObject() )
	{
		outUid = table->GetPartRowUid();
	}
	else ev->NewAbookFault(AB_Table_kFaultNotOpenTable);

	ab_Env_EndMethod(ev)
	return outUid;
}

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


AB_API_IMPL(ab_ref_count)
AB_Table_GetRefCount(const AB_Table* self, AB_Env* cev) /*i*/
	/*- GetRefCount returns the current number of references to this table
	instance in the current runtime session. This reference count is not
	persistent. It only reflects the number of times that a table has been
	acquired and not released. When a table is released enough times that it's
	ref count becomes zero, then the table is deallocated. 

	Frontends might not need to deal with table reference counts. The
	interface will try to be set up so this issue can be ignored as long as all
	usage rules are followed. -*/
{
	ab_ref_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "GetRefCount")

	const ab_Table* table = ab_Table::AsConstThis(self);
	if ( table->IsOpenObject() )
	{
		outCount = table->ObjectRefCount();
	}
	else ev->NewAbookFault(AB_Table_kFaultNotOpenTable);

	ab_Env_EndMethod(ev)
	return outCount;
}

AB_API_IMPL(ab_ref_count)
AB_Table_Acquire(AB_Table* self, AB_Env* cev) /*i*/
	/*- Acquire increments the table's refcount. Perhaps frontends will never
	need to do this. For example, this occurs automatically when a frontend
	allocates a row by calling AB_Table_MakeRow() /*i*/
{
	ab_Env* ev = ab_Env::AsThis(cev);
	return ab_Table::AsThis(self)->AcquireObject(ev);
}

AB_API_IMPL(ab_ref_count)
AB_Table_Release(AB_Table* self, AB_Env* cev) /*i*/
	/*- Release decrements the table's refcount. Perhaps frontends will never
	need to do this. For example, this occurs automatically when a frontend
	deallocates a row by calling AB_Row_Free() -*/
{
	ab_Env* ev = ab_Env::AsThis(cev);
	return ab_Table::AsThis(self)->ReleaseObject(ev);
}

AB_PUBLIC_API(void)
AB_Table_Close(AB_Table* self, AB_Env* cev) /* i */
	/*- Close closes the table. -*/
{
	ab_Env * ev = ab_Env::AsThis(cev);
	ab_Table::AsThis(self)->CloseObject(ev);
}
                         
/* ----- ----- ----- ----- Columns ----- ----- ----- ----- */


	/*- (The notion of column replaces the old notion of token. A token
	means something more general, but the previously described address
	book model was only using tokens to descibe attribute names in
	schemas, and in the table model these are just names of columns.) -*/

AB_API_IMPL(const char*)
AB_Table_GetColumnName(AB_Table* self, AB_Env* cev, /*i*/
	ab_column_uid inColUid)
	/*- GetColumnName returns a constant string (do not modify or delete)
	which is the string representation of col. A null pointer is returned if
	this table does not know this given column uid. 

	The self table need not be the address book, but if not the address book
	table will be looked up (with AB_Table_GetBookTable()) and this table
	will be used instead. -*/
{
	const char* outName = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "GetColumnName")

	ab_NameSet* nameSet = AB_Table_get_name_set(self, ev);
	if ( nameSet )
	{
		outName = nameSet->GetName(ev, inColUid);
	}

	ab_Env_EndMethod(ev)
	return outName;
}

AB_API_IMPL(ab_column_uid)
AB_Table_GetColumnId(const AB_Table* self, AB_Env* cev, /*i*/
	 const char* inName)
	/*- GetColumnId returns the column id associated. A zero uid is returned if
	this table does not know the given column name. This method must be
	used instead of NewColumnId when the address book is readonly and
	cannot be modified. 

	The self table need not be the address book, but if not the address book
	table will be looked up (with AB_Table_GetBookTable()) and this table
	will be used instead. -*/
{
	ab_column_uid outUid = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "GetColumnId")

	ab_NameSet* nameSet = AB_Table_get_name_set(self, ev);
	if ( nameSet )
	{
		outUid = nameSet->GetToken(ev, inName);
	}

	ab_Env_EndMethod(ev)
	return outUid;
}

AB_API_IMPL(ab_column_uid)
AB_Table_NewColumnId(AB_Table* self, AB_Env* cev, const char* inName) /*i*/
	/*- NewColumnId is like GetColumnId except that if name is not
	already known as a column in the address book, this method will modify
	the address book and its table so that in the future name is known by
	the persisent column uid returned. So a zero uid will not be returned
	unless an error occurs. 

	The self table need not be the address book, but if not the address book
	table will be looked up (with AB_Table_GetBookTable()) and this table
	will be used instead. -*/
{
	ab_column_uid outUid = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "NewColumnId")

	ab_NameSet* nameSet = AB_Table_get_name_set(self, ev);
	if ( nameSet )
	{
		outUid = nameSet->NewToken(ev, inName);
	}

	ab_Env_EndMethod(ev)
	return outUid;
}

AB_API_IMPL(ab_bool)
AB_Table_HasDisplayColumnProperty(const AB_Table* self, AB_Env* cev) /*i*/
	/*- HasDisplayColumnProperty returns whether this table has its own
	specialized property that configures which columns it will display. If
	not, this table will default to using the columns for the containing
	address book. This method allows inspection of whether this table has
	overriden the address book defaults that will otherwise be used by the
	various column methods defined on AB_Table. -*/
{
	ab_bool outBool = AB_kFalse;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "HasDisplayColumnProperty")

	const ab_Table* table = ab_Table::AsConstThis(self);
	if ( table->IsOpenObject() )
	{
		outBool = table->TableHasOwnColumnSet();
	}
	else ev->NewAbookFault(AB_Table_kFaultNotOpenTable);

	ab_Env_EndMethod(ev)
	return outBool;
}

AB_API_IMPL(ab_column_count)
AB_Table_CountColumns(const AB_Table* self, AB_Env* cev) /*i*/
	/*- CountColumns returns the number of display columns for this table.
	This is just a subset of information returned by GetColumns. -*/
{
	ab_column_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "CountColumns")

	ab_ColumnSet* colSet = AB_Table_get_col_set(self, ev);
	if ( colSet )
	{
		outCount = colSet->CountColumns(ev);
	}

	ab_Env_EndMethod(ev)
	return outCount;
}

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


AB_API_IMPL(ab_column_count)
AB_Table_GetColumnLayout(const AB_Table* self, AB_Env* cev, /*i*/
          AB_Column* outVector, ab_column_count inSize,
          ab_column_count* outLength)
	/*- GetColumnLayout fills the vector of AB_Column instances with
	descriptions of the column layout for this table. (If this table does not
	override settings from the address book, then this is the same as the
	address book column layout.) 

	Note that columns described by GetColumnLayout are the default
	columns added to AB_Row instances created by
	AB_Table_MakeDefaultRow(). 

	The outVector out parameter must be an array of at least inSize
	AB_Column instances, but preferrably AB_Table_CountColumns() plus
	one, because given enough instances the last column will be null
	terminated by means of a zero pointer written in the sColumn_Name slot. 

	The actual number of columns (exactly equal to
	AB_Table_CountColumns()) is returned from the method as the function
	value. The number of columns actually described in outVector is
	returned in outLength, and this is the same as the method value only
	when inSize is greater than AB_Table_CountColumns(). If inSize is
	greater than AB_Table_CountColumns(), then the column after the last
	one described is null terminated with zero in the sColumn_Name slot. 

	Each column in the layout is described as follows. sColumn_Uid gets a
	copy of the column uid returned from AB_Table_GetColumnId() for the
	name written in sColumn_Name. 

	sColumn_Name gets a copy of the column name returned from
	AB_Table_GetColumnName(). Callers must not modify or delete these
	name strings, because these pointers are aliases to strings stored in a
	dictionary within the self table instance, and modifying the strings
	might have catastrophic effect. 

	sColumn_PrettyName gets either a null pointer (when the pretty name
	and the real name are identical), or a string different from sColumn_Name
	which the user prefers to see in the table display. Like sColumn_Name,
	the storage for this string also belongs to self and must not be
	modified, on pain of potential catastrophic effect. 

	sColumn_CanSort gets a boolean indicating whether the table can be
	sorted by the column named by sColumn_Name. -*/
{
	ab_column_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "GetColumnLayout")

	ab_ColumnSet* colSet = AB_Table_get_col_set(self, ev);
	if ( colSet )
	{
		outCount = colSet->GetAllColumns(ev, outVector, inSize, outLength);
	}

	ab_Env_EndMethod(ev)
	return outCount;
}


AB_API_IMPL(ab_column_count)
AB_Table_GetDefaultLayout(const AB_Table* self, AB_Env* cev,/*i*/
	AB_Column* outVector, ab_column_count inSize,
	ab_column_count* outLength) 
	/*- GetDefaultLayout is nearly the same as
	AB_Table_GetColumnLayout() except it returns the default layout used
	for a new address book table, as opposed to a specific layour currently
	in use by either this table or the actual address book table containing this
	table. -*/
{
	ab_column_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "GetDefaultLayout")

	ab_Defaults* defs = AB_Table_get_defaults(self, ev);
	if ( defs )
	{
		outCount = defs->GetDefaultColumns(ev, outVector, inSize, outLength);
	}

	ab_Env_EndMethod(ev)
	return outCount;
}

AB_API_IMPL(ab_bool)
AB_Table_ChangeColumnLayout(AB_Table* self, AB_Env* cev, /*i*/
      const AB_Column* inColVector, ab_bool inDoChangeIndex)

	/*- ChangeColumnLayout modifies the column layout of this table to be
	equal to whatever is described in inVector, which must be null
	terminated by means of a zero pointer in the final sColumn_Name slot of
	the last column instance. 

	Note changing table columns will change the default set of columns
	added to AB_Row instances created by subsequent calls to
	AB_Table_MakeDefaultRow(). 

	If changing the column layout implies a change of address book indexes,
	then changeIndex must be true in order to actually cause this change to
	occur, because removing an index can be a very expensive (slow)
	operation. (We'll add asynchronous versions of this later.) -*/
{
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "ChangeColumnLayout")

	ab_ColumnSet* colSet = AB_Table_get_col_set(self, ev);
	if ( colSet )
	{
		colSet->PutAllColumns(ev, inColVector, inDoChangeIndex);
	}

	ab_Env_EndMethod(ev)
	return ev->Good();
}

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


AB_API_IMPL(ab_bool)
AB_Table_GetColumnAt(const AB_Table* self, AB_Env* cev, /*i*/
      AB_Column* outColumn, ab_column_pos inColPos)

	/*- GetColumnAt gets the table's column layout information from column
	position pos, which must be a one-based index from one to
	AB_Table_CountColumns() (or otherwise false is returned and nothing
	happens to the col parameter). -*/
{
	ab_bool outBool = AB_kFalse;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "GetColumnAt")

	ab_ColumnSet* colSet = AB_Table_get_col_set(self, ev);
	if ( colSet )
	{
		outBool = colSet->GetColumn(ev, outColumn, inColPos);
	}

	ab_Env_EndMethod(ev)
	return outBool;
}

AB_API_IMPL(ab_bool)
AB_Table_PutColumnAt(const AB_Table* self, AB_Env* cev, /*i*/
	const AB_Column* inCol, ab_column_pos inColPos, ab_bool inDoChangeIndex)

	/*- PutColumnAt overwrites the table's column layout information at
	column position pos, which must be a one-based index from one to
	AB_Table_CountColumns() (or otherwise false is returned and nothing
	happens to the table's column layout). 

	If the column named (sColumnName) already occurs at some other
	position in the layout, then an error occurs. Otherwise the existing
	column at pos is changed to col. 

	If the value of sColumn_CanSort implies a change of address book
	indexes, then changeIndex must be true in order to actually cause this
	change to occur, because adding or removing an index can be a very
	expensive (slow) operation. (We'll add asynchronous versions of this
	later.) -*/
{
	ab_bool outBool = AB_kFalse;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "PutColumnAt")

	ab_ColumnSet* colSet = AB_Table_get_col_set(self, ev);
	if ( colSet )
	{
		outBool = colSet->PutColumn(ev, inCol, inColPos, inDoChangeIndex);
	}

	ab_Env_EndMethod(ev)
	return outBool;
}

AB_API_IMPL(ab_bool)
AB_Table_AddColumnAt(const AB_Table* self, AB_Env* cev,  /*i*/
	const AB_Column* inCol, ab_column_pos inColPos, ab_bool inDoChangeIndex)
	/*- AddColumnAt inserts new table column layout information at column
	position pos, which must be a one-based index from one to
	AB_Table_CountColumns() plus one (or otherwise false is returned and
	nothing happens to the table's column layout). 

	If the column named (sColumnName) already occurs at some other
	position in the layout, then an error occurs. Otherwise a new column at
	pos is inserted in the layout. 

	If the value of sColumn_CanSort implies a change of address book
	indexes, then changeIndex must be true in order to actually cause this
	change to occur, because adding an index can be a very expensive
	(slow) operation. (We'll add asynchronous versions of this later.) -*/
{
	ab_bool outBool = AB_kFalse;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "AddColumnAt")

	ab_ColumnSet* colSet = AB_Table_get_col_set(self, ev);
	if ( colSet )
	{
		outBool = colSet->AddColumn(ev, inCol, inColPos, inDoChangeIndex);
	}

	ab_Env_EndMethod(ev)
	return outBool;
}

AB_API_IMPL(ab_column_pos)
AB_Table_CutColumn(const AB_Table* self, AB_Env* cev, /*i*/
          ab_column_uid inColUid, ab_bool inDoChangeIndex)
	/*- CutColumn removes an old column from the table column layout. The
	col parameter must name a column currently in the layout (or otherwise
	false is returned and nothing happens to the table's column layout). 

	If removing the column implies a change of address book indexes, then
	changeIndex must be true in order to actually cause this change to
	occur, because removing an index can be a very expensive (slow)
	operation. (We'll add asynchronous versions of this later.) -*/
{
	ab_column_pos outPos = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "CutColumn")

	ab_ColumnSet* colSet = AB_Table_get_col_set(self, ev);
	if ( colSet )
	{
		outPos = colSet->CutColumn(ev, inColUid, inDoChangeIndex);
	}

	ab_Env_EndMethod(ev)
	return outPos;
}

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


AB_API_IMPL(ab_bool)
AB_Table_CanSortByUid(const AB_Table* self, AB_Env* cev, /*i*/
	ab_column_uid inColUid)
	/*- CanSortByUid indicates whether this table can sort rows by the
	specified column uid col. Frontends might prefer to call
	CanSortByName. -*/
{
	ab_bool outBool = AB_kFalse;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "CanSortByUid")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		outBool = rowSet->CanSortRows(ev, inColUid);
	}

	ab_Env_EndMethod(ev)
	return outBool;
}

AB_API_IMPL(ab_bool)
AB_Table_CanSortByName(const AB_Table* self, AB_Env* cev, /*i*/
	const char* inName)
	/*- CanSortByName indicates whether this table can sort rows by the
	specified column name. 

	CanSortByName is implemented by calling 
	AB_Table_CanSortByUid(self, ev,
	AB_Table_GetColumnId(self, ev, name)) -*/
{
	ab_bool outBool = AB_kFalse;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "CanSortByName")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		ab_NameSet* nameSet = AB_Table_get_name_set(self, ev); 
		if ( nameSet )
		{
			ab_column_uid colUid = nameSet->GetToken(ev, inName);
			if ( colUid )
				outBool = rowSet->CanSortRows(ev, colUid);
		}
	}

	ab_Env_EndMethod(ev)
	return outBool;
}

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


AB_API_IMPL(ab_bool)
AB_Table_SortByUid(const AB_Table* self, AB_Env* cev, /*i*/
	ab_column_uid inColUid)
	/*- SortByUid sorts this table by the specified column col. If col is zero
	this means make the table unsorted, which causes the table to order rows
	in the most convenient internal form (which is by row uid for address
	books, and some user specified order for mailing lists). -*/
{
	ab_bool outBool = AB_kFalse;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "SortByUid")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		outBool = rowSet->SortRows(ev, inColUid);
	}

	ab_Env_EndMethod(ev)
	return outBool;
}

AB_API_IMPL(ab_bool)
AB_Table_SortByName(const AB_Table* self, AB_Env* cev, const char* inName) /*i*/
	/*- SortByName sorts this table by the specified column name. The
	name parameter can be null, and this means make the table unsorted
	(with respect to specific cell values). Making a table unsorted makes
	particular sense for mailing lists. 

	SortByName is implemented by calling 
	AB_Table_SortByUid(self, ev, AB_Table_GetColumnId(self,
	ev, name)) -*/
{
	ab_bool outBool = AB_kFalse;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "SortByName")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		ab_NameSet* nameSet = AB_Table_get_name_set(self, ev); 
		if ( nameSet )
		{
			ab_column_uid colUid = nameSet->GetToken(ev, inName);
			if ( colUid )
				outBool = rowSet->SortRows(ev, colUid);
		}
	}

	ab_Env_EndMethod(ev)
	return outBool;
}

AB_API_IMPL(ab_bool) /* abtable.c */
AB_Table_SortFoward(const AB_Table* self, AB_Env* cev, ab_bool inAscend) /*i*/
	/*- If inAscend is true, arrange sorted rows in ascending order,
	and otherwise arrange sorted rows in descending order -*/
{
	ab_bool outAscend = AB_kFalse;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "SortFoward")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		if ( rowSet->GetRowSetSortForward() != inAscend )
		{
			rowSet->ChangeRowSetSortForward(ev, inAscend);
			// $$$$$ issue change notification?
		}
	}

	ab_Env_EndMethod(ev)
	return ev->Good();
}

AB_API_IMPL(ab_bool) /* abtable.c */
AB_Table_GetSortFoward(const AB_Table* self, AB_Env* cev) /*i*/
	/*- Return whether rows are arranged in ascending order -*/
{
	ab_bool outAscend = AB_kFalse;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "GetSortFoward")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		outAscend = rowSet->GetRowSetSortForward();
	}

	ab_Env_EndMethod(ev)
	return outAscend;
}

AB_API_IMPL(ab_column_uid)
AB_Table_GetSortColumn(const AB_Table* self, AB_Env* cev) /*i*/
	/*- GetSortColumn returns the column currently used by the table for
	sorting. Zero is returned when this table is currently unsorted (and also
	when any error occurs). Unsorted tables might be common for mailing
	lists. -*/
{
	ab_column_uid outUid = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "GetSortColumn")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		outUid = rowSet->GetRowSortOrder(ev);
	}

	ab_Env_EndMethod(ev)
	return outUid;
}

AB_API_IMPL(AB_Table*)
AB_Table_AcquireSortedTable(AB_Table* self, AB_Env* cev, /*i*/
          ab_column_uid inNewSortUid)
	/*- AcquireSortedTable returns a new table instance that has the same
	content as the old table, but sorts on a different column. This is similar
	to AB_Table_SortByUid(), but lets one have two different views of the
	same table with different sortings. 

	If newSortColumn is zero this means return an unsorted table. If
	newSortColumn is the same value as the current sorting returned by
	AB_Table_GetSortColumn(), this means the returned table might be
	exactly the same AB_Table instance as self. However, it will have been
	acquired one more time, so self and the returned table act as if refcounted
	separately even if they are the same table instance. 

	The caller must eventually release the returned table by calling
	AB_Table_Release(), and then make sure not to refer to this table again
	after released because it might be destroyed. -*/
{
	AB_Table* outTable = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "AcquireSortedTable")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		ab_column_uid oldSort = rowSet->GetRowSortOrder(ev);
		if ( ev->Good() )
		{
			ab_Table* me = ab_Table::AsThis(self);
			if ( oldSort == inNewSortUid ) // same sort as myself? 
			{
		  		if ( me->AcquireObject(ev) )
			  		outTable = self; // return another copy of self
			}
			else // need a new instance of AB_Table with different sort
			{
				ab_RowSet* newSet = rowSet->NewSortClone(ev, inNewSortUid);
				if ( newSet ) // made new row set?
				{
					ab_row_uid id = newSet->GetPartRowUid();
					AB_Table_eType type = (AB_Table_eType) 0; // use same type
					ab_Table* t = new(*ev) ab_Table(ev, *me, newSet, id, type);
					if ( t )
					{
						if ( ev->Good() )
							outTable = t->AsSelf();
						else
							t->ReleaseObject(ev);
					}
					newSet->ReleaseObject(ev); // always release, error or not
				}
			}
		}
	}
	ab_Env_EndMethod(ev)
	return outTable;
}

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */
                          
/* ----- ----- ----- ----- Search ----- ----- ----- ----- */

AB_PUBLIC_API(ab_row_uid) /* abtable.cpp */
AB_Table_FindFirstRowWithPrefix(AB_Table* self, AB_Env* cev, /*i*/
          const char* inCellValuePrefix, ab_column_uid inColumnUid)
{
	ab_row_uid outUid = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "AcquireSearchTable")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		outUid = rowSet->FindRowByPrefix(ev, inCellValuePrefix, inColumnUid);
	}

	ab_Env_EndMethod(ev)
	return outUid;
}

AB_API_IMPL(AB_Table*)
AB_Table_AcquireSearchTable(AB_Table* self, AB_Env* cev, /*i*/
          const char* inCellValue, const ab_column_uid* inColVector)
	/*- AcquireSearchTable returns a table giving the non-persistent results of
	a search for cellValue in one or more columns of table self. The
	columns to search are given in the array specified by inColumnVector
	which should be null-terminated with a zero ab_column_uid value after
	the last column to be searched. 

	The returned table will contain no duplicates, even if cellValue causes a
	match in more than one column. The table content is virtual (not
	persistent, and possibly listed only by indirect reference) and will not
	exist after the table is released down to a zero refcount. (If the caller
	wants a persistent table, the caller can make a new mailing list and iterate
	over this table, adding all rows to the new mailing list.) 

	The caller must eventually release the returned table by calling
	AB_Table_Release(), and then make sure not to refer to this table again
	after released because it might be destroyed. (Debug builds might keep
	around "destroyed" tables for a time to detect invalid access after ref
	counts reach zero.) -*/
{
	AB_Table* outTable = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "AcquireSearchTable")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		ab_RowSet* newSet = rowSet->AcquireSearchSubset(ev, inCellValue,
			inColVector);
		if ( newSet ) // made new row set?
		{
			ab_Table* me = ab_Table::AsThis(self);
			AB_Table_eType type = AB_Table_kSearchResult;
			ab_row_uid id = newSet->GetPartRowUid();
			ab_Table* t = new(*ev) ab_Table(ev, *me, newSet, id, type);
			if ( t )
			{
				if ( ev->Good() )
					outTable = t->AsSelf();
				else
					t->ReleaseObject(ev);
			}
			newSet->ReleaseObject(ev); // always release, error or not
		}
	}

	ab_Env_EndMethod(ev)
	return outTable;
}

                          
/* ----- ----- ----- ----- Rows ----- ----- ----- ----- */


/*- (The notion of row replaces both the old notions of entry and tuple. In
more detail, ab_row_uid replaces ABID, and AB_Row replaces
HgAbTuple. Nothing replaces HgAbSchema which simply goes away
because this behavior is folded into AB_Row. We could introduce the idea
of a meta-row to replace schema, but this is really just an performance
issue and we can hide these details from the FEs.) 

A persistent row is identified by its uid, ab_row_uid, but callers can
only read and write such persistent objects by means of a runtime object
named AB_Row which can describe the content of a persisent row.
AB_Row denotes a transient session object and not a persistent object.
Any given instance of AB_Row has no associated ab_row_uid because
the runtime object is not intended to have a close association with a
single persisent row. 

However, each AB_Row instance is associated with a specific instance of
AB_Table (even though this could be avoided) because this runtime
relationship happens to be convenient, and causes no awkwardness in
the interfaces. AB_Row can effectively be considered a view onto the
collection of persisent rows inside a given table, except AB_Row is not
focused on a specific row in the table. -*/

#define AB_Table_k_pick_count /*i*/ 128 /* number of rows to pick at a time */

AB_API_IMPL(ab_row_count)
AB_Table_AddAllRows(AB_Table* self, AB_Env* cev,  /*i*/
            const AB_Table* inOtherTable)
	/*- AddAllRows adds all the rows in other to self, as if by iterating over
	all rows in other and adding them one at a time with
	AB_Table_AddRow(). -*/
{
	ab_row_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "AddAllRows")

	ab_RowSet* fromSet = AB_Table_get_row_set(inOtherTable, ev);
	ab_RowSet* toSet = AB_Table_get_row_set(self, ev);
	if ( toSet && fromSet && toSet != fromSet && self != inOtherTable )
	{
		toSet->AddAllRows(ev, fromSet);
	}

	ab_Env_EndMethod(ev)
	return outCount;
}


AB_API_IMPL(ab_row_uid)
AB_Table_CopyRow(AB_Table* self, AB_Env* cev, /*i*/
	 const AB_Table* inOther, ab_row_uid inRowUid)
	/*- copy row inRowUid from inOther to this table -*/
{
	ab_row_uid outUid = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "CopyRow")

	if ( self != inOther )
	{
		ab_RowContent* thisContent = AB_Table_get_row_content(self, ev);
		if ( thisContent )
		{
			ab_RowContent* otherContent = AB_Table_get_row_content(inOther, ev);
			if ( otherContent )
			{
				if ( thisContent != otherContent )
				{
					outUid = thisContent->CopyRow(ev, otherContent, inRowUid);
					if ( outUid )
					{
						AB_Table_AddRow(self, cev, outUid);
						if ( ev->Bad() )
							outUid = 0;
					}
				}
				else
				{
					AB_Table_AddRow(self, cev, inRowUid);
					if ( ev->Good() )
						outUid = inRowUid;
				}
			}
		}
	}

	ab_Env_EndMethod(ev)
	return outUid;
}

AB_API_IMPL(ab_row_pos)
AB_Table_AddRow(AB_Table* self, AB_Env* cev, ab_row_uid inRowUid) /*i*/
	/*- AddRow aliases an existing row in the address book containing table
	self (see AB_Table_GetBookTable()) so this table also contains this
	row. If self already contains this row, nothing happens and false is
	returned. If row does not denote a row in the address book, an error
	occurs and zero is returned. (The error is indicated by
	AB_Env_GetError().) 

	If self did not previously contain row, it is added and true is returned.
	This is a way to alias an existing row into a new location, rather than
	creaing a new persistent row. (New persistent rows are created with
	AB_Row_NewTableRowAt().) -*/
{
	ab_row_pos outPos = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "AddRow")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		outPos = rowSet->AddRow(ev, inRowUid, /*pos*/ 0);
	}

	ab_Env_EndMethod(ev)
	return outPos;
}

AB_API_IMPL(ab_row_pos)
AB_Table_CutRow(AB_Table* self, AB_Env* cev, ab_row_uid inRowUid) /*i*/
	/*- CutRow removes the indicated row from the table. This does not
	actually destroy the row unless this was the last reference to the row.
	False is returned when the table did not previously contain row (but it's
	okay to attempt cutting it again because the same desired result obtains
	when the row is not in the table afterwards). -*/
{
	ab_row_pos outPos = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "CutRow")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		outPos = rowSet->CutRow(ev, inRowUid);
	}

	ab_Env_EndMethod(ev)
	return outPos;
}

AB_API_IMPL(ab_row_count)
AB_Table_CutRowRange(AB_Table* self, AB_Env* cev, /*i*/
	ab_row_pos inPos, ab_row_count inCount)
	/*- CutRow removes the indicated set of c rows from the table, starting
	at one-based position p. This does not actually destroy the rows unless
	they were the last references to the rows. -*/
{
	ab_row_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "CutRowRange")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		outCount = rowSet->CutRowRange(ev, inPos, inCount);
	}

	ab_Env_EndMethod(ev)
	return outCount;
}

AB_API_IMPL(ab_row_pos)
AB_Table_DestroyRow(AB_Table* self, AB_Env* cev, ab_row_uid inRowUid) /*i*/
	/*- DestroyRow removes the indicated row from this table and from
	every other table containing the same row. Zero is returned when the
	containing address book did not previously contain row anywhere (but
	it's okay to attempt destroying it again because the same desired result
	obtains when the row is not in the table afterwards). An error might
	occur if the table can determine that row was never a valid row inside
	the address book (perhaps the size of the uid indicates it has never been
	assigned). -*/
{
	ab_row_pos outPos = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "DestroyRow")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		ab_RowContent* rowContent = AB_Table_get_row_content(self, ev);
		if ( rowContent )
		{
			outPos = rowSet->FindRow(ev, inRowUid);
			if ( ev->Good() )
				rowContent->ZapRow(ev, inRowUid);
		}
	}

	ab_Env_EndMethod(ev)
	return outPos;
}

AB_API_IMPL(ab_row_count)
AB_Table_CountRows(const AB_Table* self, AB_Env* cev) /*i*/
	/*- CountRows returns the number of rows in the table. (In other words,
	how many entries does this address book or mailing list contain?) -*/
{
	ab_row_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "CountRows")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		outCount = rowSet->CountRows(ev);
	}

	ab_Env_EndMethod(ev)
	return outCount;
}

AB_API_IMPL(AB_Table*)
AB_Table_AcquireListsTable(AB_Table* self, AB_Env* cev) /*i*/
{
	AB_Table* outTable = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "AcquireListsTable")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		ab_RowSet* newSet = rowSet->AcquireListSubset(ev);
		if ( newSet ) // made new row set?
		{
			ab_Table* me = ab_Table::AsThis(self);
			AB_Table_eType type = AB_Table_kListSubset;
			ab_row_uid id = newSet->GetPartRowUid();
			ab_Table* t = new(*ev) ab_Table(ev, *me, newSet, id, type);
			if ( t )
			{
				if ( ev->Good() )
					outTable = t->AsSelf();
				else
					t->ReleaseObject(ev);
			}
			// outTable = AB_Table_CloneWithRowSet(self, ev, t, newSet);
			newSet->ReleaseObject(ev); // always release, error or not
		}
	}

	ab_Env_EndMethod(ev)
	return outTable;
}


AB_API_IMPL(ab_row_count)
AB_Table_CountRowParents(const AB_Table* self, AB_Env* cev, /*i*/
          ab_row_uid inRowUid)
	/*- CountRowParents returns the number of parent tables that contain the
	row known by id. If zero, this means id does not exist because no
	row corresponds to this uid. If more than one, this means that more than
	one table contains an alias to this row. The value returned by
	CountRowParents is effectively the persistent reference count for row
	id. -*/
{
	ab_row_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "CountRowParents")

	ab_RowContent* rowContent = AB_Table_get_row_content(self, ev);
	if ( rowContent )
	{
		outCount = rowContent->CountRowParents(ev, inRowUid);
	}

	ab_Env_EndMethod(ev)
	return outCount;
}

AB_API_IMPL(AB_Table*)
AB_Table_AcquireRowParentsTable(const AB_Table* self, AB_Env* cev, /*i*/
          ab_row_uid inRowUid)
	/*- AcquireRowParentsTable returns a table representing the collection of
	tables that contain the row known by id. A null pointer is returned
	when any problem occurs (such as id not existing (i.e. zero
	AB_Table_CountRowParents())). 

	The caller must eventually release the table by calling
	AB_Table_Release(), and then make sure not to refer to this table again
	after released because it might be destroyed. (Debug builds might keep
	around "destroyed" tables for a time to detect invalid access after ref
	counts reach zero.) -*/
{
	AB_Table* outTable = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "AcquireRowParentsTable")

	ab_RowContent* rowContent = AB_Table_get_row_content(self, ev);
	if ( rowContent )
	{
		ab_RowSet* newSet = rowContent->AcquireRowParents(ev, inRowUid);
		if ( newSet ) // made new row set?
		{
			ab_Table* me = (ab_Table*) ab_Table::AsConstThis(self);
			AB_Table_eType type = AB_Table_kParentList;
			ab_row_uid id = newSet->GetPartRowUid();
			ab_Table* t = new(*ev) ab_Table(ev, *me, newSet, id, type);
			if ( t )
			{
				if ( ev->Good() )
					outTable = t->AsSelf();
				else
					t->ReleaseObject(ev);
			}
			newSet->ReleaseObject(ev); // always release, error or not
		}
	}

	ab_Env_EndMethod(ev)
	return outTable;
}


/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

AB_API_IMPL(ab_row_count)
AB_Table_CountRowChildren(const AB_Table* self, AB_Env* cev, /*i*/
          ab_row_uid inRowUid)
	/*- CountRowChildren returns the number of children rows in the row
	known by id. When this number is positive the row is a table.
	Otherwise when zero the row is simply a row. -*/
{
	ab_row_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "CountRowChildren")

	ab_RowContent* rowContent = AB_Table_get_row_content(self, ev);
	if ( rowContent )
	{
		outCount = rowContent->CountRowChildren(ev, inRowUid);
	}

	ab_Env_EndMethod(ev)
	return outCount;
}

AB_API_IMPL(AB_Table*)
AB_Table_AcquireRowChildrenTable(const AB_Table* self, AB_Env* cev, /*i*/
          ab_row_uid inTableUid)
	/*- AcquireRowChildrenTable returns a table representing the set of rows
	inside the table known by tableId. Typically tableId is a mailing list
	and the returned table shows the content of this mailing list. A null
	pointer is returned when any problem occurs. 

	If tableId was previously not a table (because it had no children and so
	AB_Table_IsRowTable() returns false), then the table returned is
	empty. However, when the caller adds any row to this table, then
	tableId becomes a table as a result. In other words, any row can be
	made a mailing list by adding children, and this is done by acquiring the
	row's children table and adding some children. Presto, changeo, now
	the row is a table. 

	The caller must eventually release the table by calling
	AB_Table_Release(), and then make sure not to refer to this table again
	after released because it might be destroyed. (Debug builds might keep
	around "destroyed" tables for a time to detect invalid access after ref
	counts reach zero.) -*/
{
	AB_Table* outTable = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "AcquireRowChildrenTable")

	ab_RowContent* rowContent = AB_Table_get_row_content(self, ev);
	if ( rowContent )
	{
		ab_RowSet* newSet = rowContent->AcquireRowChildren(ev, inTableUid);
		if ( newSet ) // made new row set?
		{
			ab_Table* me = (ab_Table*) ab_Table::AsConstThis(self);
			AB_Table_eType type = AB_Table_kMailingList;
			ab_row_uid id = newSet->GetPartRowUid();
			ab_Table* t = new(*ev) ab_Table(ev, *me, newSet, id, type);
			if ( t )
			{
				if ( ev->Good() )
					outTable = t->AsSelf();
				else
					t->ReleaseObject(ev);
			}
			newSet->ReleaseObject(ev); // always release, error or not
		}
	}

	ab_Env_EndMethod(ev)
	return outTable;
}

AB_API_IMPL(ab_row_uid)
AB_Table_GetRowAt(const AB_Table* self, AB_Env* cev, ab_row_pos inPos) /*i*/
	/*- GetRowAt returns the uid of the row at position p, where each row in
	a table has a position from one to AB_Table_RowCount(). (One-based
	indexes are used rather than zero-based indexes, because zero is
	convenient for indicating when a row is not inside a table.) -*/
{
	ab_row_uid outUid = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "GetRowAt")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		outUid = rowSet->GetRow(ev, inPos);
	}

	ab_Env_EndMethod(ev)
	return outUid;
}

AB_API_IMPL(ab_row_count)
AB_Table_GetRows(const AB_Table* self, AB_Env* cev, /*i*/
       ab_row_uid* outVector, ab_row_count inSize, ab_row_pos inPos)
	/*- GetRows is roughly equivalent to calling AB_Table_GetRowAt()
	inSize times. It is a good way to read a contiguous sequence of row
	uids starting at position pos inside the table. 

	At most inSize uids are written to outVector, and the actual number
	written there is returned as the value of function. The only reason why
	fewer might be written is if fewer than inSize rows are in the table
	starting at position pos. 

	Remember that pos is one-based, so one is the position of the first row
	and AB_Table_CountRows() is the position of the last. -*/
{
	ab_row_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "GetRows")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		outCount = rowSet->PickRows(ev, outVector, inSize, inPos);
	}

	ab_Env_EndMethod(ev)
	return outCount;
}

AB_API_IMPL(ab_bool)
AB_Table_DoesSortRows(const AB_Table* self, AB_Env* cev) /*i*/
	/*- DoesSortRows returns whether table self maintains its row collection
	in sorted order. Presumably this is always true for address books
	(AB_Type_kAddressBookTable) but always false for mailing lists
	(AB_Type_kMailingListTable). At least this would be consistent with
	4.0 behavior. 

	DoesSortRows is equivalent to the expression
	(AB_Table_GetSortColumn()!=0). 

	We might want to permit mailing lists to sort rows by cell values, just
	like address books do. But in that case an unsorted representation should
	be kept so users can keep list recipients in a preferred order. (A user
	specified ordering might be important for social constraints in showing
	proper acknowledgement to individuals according to some ranking
	scheme.) 

	If we decide to sort mailing lists, the sorting will likely be maintained in
	memory, as opposed to persistently. (Users with huge mailing lists
	should use a separate address book for this purpose.) This would let users
	easily revert to the original unsorted view of a list. 

	Getting a sorted interface to a list is done with
	AB_Table_AcquireSortedTable(), and getting the unsorted version of
	a list is done with AB_Table_AcquireUnsortedTable(). -*/
{
	ab_bool outBool = AB_kFalse;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "DoesSortRows")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		outBool = ( rowSet->GetRowSortOrder(ev) == 0 );
	}

	ab_Env_EndMethod(ev)
	return outBool;
}

AB_API_IMPL(ab_row_pos)
AB_Table_RowPos(const AB_Table* self, AB_Env* cev, ab_row_uid inRowUid) /*i*/
	/*- RowPos returns the position of the row known by id, where each row
	in a table has a position from one to AB_Table_RowCount(). Zero is
	returned when this row is not inside the table. -*/
{
	ab_row_pos outPos = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "RowPos")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		outPos = rowSet->FindRow(ev, inRowUid);
	}

	ab_Env_EndMethod(ev)
	return outPos;
}


AB_API_IMPL(ab_row_pos)
AB_Table_ChangeRowPos(const AB_Table* self, AB_Env* cev, /*i*/
      ab_row_uid inExistingRow, ab_row_pos toNewPos)
	/*- ChangeRowPos moves the row known by existingRow to new
	position toNewPos, provided AB_Table_DoesSortRows() is true.
	Otherwise ChangeRowPos does nothing (and returns false) if the table
	is in sorted order, or if existingRow is not in the table
	(AB_Table_HasRow() returns false). 

	Specifically, ChangeRowPos is expected to be useful for mailing list
	tables (AB_Type_kMailingListTable), but not useful for address book
	tables (AB_Type_kAddressBookTable) nor useful for some other table
	types (e.g. AB_Type_kSearchResultTable). 

	If FEs wish, they need not worry about this restriction, but simply let
	users see that dragging rows in mailing lists is useful, but dragging rows
	in address books has no effect. It does not seem feasible to prevent users
	from attempting to drag within an address book, because FEs will not
	know whether the user intends to drop elsewhere, say in another address
	book. FEs might want to figure out whether a drop makes sense in the
	same table, but they can simply call ChangeRowPos and find out
	(through both the return value and notifications) whether it has any
	effect. -*/
{
	ab_row_pos outPos = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "ChangeRowPos")

	ab_RowSet* rowSet = AB_Table_get_row_set(self, ev);
	if ( rowSet )
	{
		if ( rowSet->GetRowSortOrder(ev) == 0 && ev->Good() ) // can change?
		{
			if ( rowSet->FindRow(ev, inExistingRow) ) // found in set?
				outPos = rowSet->AddRow(ev, inExistingRow, toNewPos); // move
		}
	}

	ab_Env_EndMethod(ev)
	return outPos;
}

AB_API_IMPL(ab_column_count)
AB_Table_CountRowCells(const AB_Table* self, AB_Env* cev, /*i*/
             ab_row_uid inRowUid)
	/*- CountRowCells returns the number of cells in the row known by id,
	which means the number of columns that have non-empty values in the
	row. (In other words, how many attributes does this entry have?) -*/
{
	ab_column_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "CountRowCells")

	ab_RowContent* rowContent = AB_Table_get_row_content(self, ev);
	if ( rowContent )
	{
		outCount = rowContent->CountRowCells(ev, inRowUid);
	}

	ab_Env_EndMethod(ev)
	return outCount;
}

AB_API_IMPL(AB_Row*)
AB_Table_MakeDefaultRow(AB_Table* self, AB_Env* cev) /*i*/
	/*- MakeDefaultRow creates a runtime description of a row (but not a
	persistent row). (Creating a persistent table row is done with
	AB_Row_NewTableRowAt().) This instance must be deallocated later with
	AB_Row_Free(). 

	This instance of AB_Row will already have cells corresponding to the
	columns used by the table (see AB_Table_GetColumnLayout()), so
	frontends that intend to use this row to display columns from the table
	will no need not do any more work before calling
	AB_Row_ReadTableRow() to read row content from the table. -*/
{
	AB_Row* outRow = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "MakeDefaultRow")

	ab_Defaults* defs = AB_Table_get_defaults(self, ev);
	if ( defs )
	{
		outRow = (AB_Row*) defs->MakeDefaultRow(ev, ab_Table::AsThis(self));
	}

	ab_Env_EndMethod(ev)
	return outRow;
}

AB_API_IMPL(AB_Row*)
AB_Table_MakeRow(AB_Table* self, AB_Env* cev, const AB_Cell* inVector) /*i*/
	/*- MakeRow creates a runtime description of a row (but not a persistent
	row) with N cells as described by the array of (at least) N+1 cells
	pointed to by inVector. Only the sCell_Column and sCell_Size slots
	matter. Other cell slots are ignored. The length of the inVector array is
	inferred by the first cell that contains zero in the sCell_Column slot,
	which is used for null termination. 

	If the first N cells of inVector have nonzero sCell_Column slots, then
	those first N cells must also have nonzero sCell_Size slots, or else an
	error occurs. 

	The sCell_Column slot values should be all distinct, without duplicate
	columns. If inVector has duplicate columns, the last one wins and no
	error occurs (so callers should check before calling if they care). 

	After a row is created, more cells can be added with
	AB_Row_AddCells(). -*/
{
	AB_Row* outRow = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "MakeRow")

	ab_Table* table = ab_Table::AsThis(self);
	if ( table->IsOpenObject() )
	{
  		ab_cell_count cellCount = 0;
  		const AB_Cell* c = inVector;
  		if ( c ) /* caller supplied a vector of cells? */
  		{
  			while ( c->sCell_Column ) /* another column to count? */
  				++c;

  			cellCount = c - inVector; /* number of cells in vector */
  			++cellCount; /* one more for future growth */
  		}
  			
		ab_Row* newRow = new(*ev)
			ab_Row(ev, ab_Usage::kHeap, table, cellCount);
		if ( newRow )
		{
			if ( ev->Good() && newRow->AddCells(ev, inVector) )
				outRow = (AB_Row*) newRow;
			else
				newRow->ReleaseObject(ev);
		}
	}
	else ev->NewAbookFault(AB_Table_kFaultNotOpenTable);

	ab_Env_EndMethod(ev)
	return outRow;
}

AB_API_IMPL(AB_Row*)
AB_Table_MakeRowFromColumns(AB_Table* self, AB_Env* cev, /*i*/
       const AB_Column* inColumns)
	/*- MakeRowFromColumns is just like AB_Table_MakeRow() except that
	sColumn_Uid and sColumn_CellSize slots are used (instead of
	sCell_Column and sCell_Size) and the vector is null terminated with a
	zero in sColumn_Uid. -*/
{
	AB_Row* outRow = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "MakeRowFromColumns")

	ab_Table* table = ab_Table::AsThis(self);
	ab_Defaults* defs = AB_Table_get_defaults(self, ev);
	if ( defs )
	{
		outRow = (AB_Row*) defs->MakeDefaultColumnRow(ev, table, inColumns);
	}

	ab_Env_EndMethod(ev)
	return outRow;
}

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

#define AB_Table_k_default_row_cell_count /*i*/ 32

AB_API_IMPL(AB_Row*)
AB_Table_MakePersonRow(AB_Table* self, AB_Env* cev) /*i*/
	/*- MakePersonRow creates a runtime description of a row (but not a
	persistent row) that seems most suited for editing a description of a
	person. This is like calling AB_Table_MakeRow() with the cell vector
	returned by AB_Table_GetPersonCells(). -*/
{
	AB_Row* outRow = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	if ( ev->Good() )
	{
		AB_Cell cells[ AB_Table_k_default_row_cell_count + 1 ];
		ab_cell_count length = 0; /* number of cells returned */
		ab_cell_count count = AB_Table_GetPersonCells(self, cev, cells,
			AB_Table_k_default_row_cell_count, &length);
		if ( count > length ) /* our cell vector buffer was too small? */
		{
#if defined(AB_CONFIG_DEBUG) && AB_CONFIG_DEBUG
			ev->Break("MakePersonRow cells too few");
#endif		
		}
		/* force cell vector null termination just to be careful: */
		AB_MEMSET(cells + AB_Table_k_default_row_cell_count, 0, sizeof(AB_Cell));
		if ( ev->Good() )
			outRow = AB_Table_MakeRow(self, cev, cells);
	}
	return outRow;
}

AB_API_IMPL(AB_Row*)
AB_Table_MakePersonList(AB_Table* self, AB_Env* cev) /*i*/
	/*- MakePersonList creates a runtime description of a row (but not a
	persistent row) that seems most suited for editing a description of a
	mailing list. This is like calling AB_Table_MakeRow() with the cell vector
	returned by AB_Table_GetListCells(). -*/
{
	AB_Row* outRow = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	if ( ev->Good() )
	{
		AB_Cell cells[ AB_Table_k_default_row_cell_count + 1 ];
		ab_cell_count length = 0; /* number of cells returned */
		ab_cell_count count = AB_Table_GetListCells(self, cev, cells,
			AB_Table_k_default_row_cell_count, &length);
		if ( count > length ) /* our cell vector buffer was too small? */
		{
#if defined(AB_CONFIG_DEBUG) && AB_CONFIG_DEBUG
			ev->Break("MakePersonList cells too few");
#endif		
		}
		/* force cell vector null termination just to be careful: */
		AB_MEMSET(cells + AB_Table_k_default_row_cell_count, 0, sizeof(AB_Cell));
		if ( ev->Good() )
			outRow = AB_Table_MakeRow(self, cev, cells);
	}
	return outRow;
}


AB_API_IMPL(ab_cell_count)
AB_Table_GetPersonCells(const AB_Table* self, AB_Env* cev, /*i*/
          AB_Cell* outVector, ab_cell_count inSize, ab_cell_count* outLength)
	/*- GetPersonCells returns a copy of the standard cells for a person in
	cell array outVector. This cell vector is address book specific because
	column uids in the sCell_Column slots have address book scope. The
	actual number of standard cells is returned as the function value, but the
	size of outVector is assumed to be inSize, so no more than inSize
	cells will be written to outVector. The actual number of cells written is
	returned in outLength (unless outLength is a null pointer to suppress
	this output). 

	If inSize is greater than the number of cells, N, then outVector[N] will
	be null terminated by placing all zero values in all the cell slots. 

	All the sCell_Content slots will be null pointers because these cells
	will not represent actual storage in a row. GetPersonCells is expected
	to be used in the implementation of methods like
	AB_Table_MakePersonRow(). -*/
{
	ab_cell_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "GetPersonCells")

	ab_Defaults* defs = AB_Table_get_defaults(self, ev);
	if ( defs )
	{
		outCount = defs->GetDefaultPersonCells(ev, outVector, inSize, outLength);
	}

	ab_Env_EndMethod(ev)
	return outCount;
}

AB_API_IMPL(ab_cell_count)
AB_Table_GetListCells(const AB_Table* self, AB_Env* cev, /*i*/
          AB_Cell* outVector, ab_cell_count inSize, ab_cell_count* outLength)
	/*- GetListCells returns a copy of the standard cells for a mailing list 
	in cell array outVector. This cell vector is address book specific because
	column uids in the sCell_Column slots have address book scope. The
	actual number of standard cells is returned as the function value, but the
	size of outVector is assumed to be inSize, so no more than inSize
	cells will be written to outVector. The actual number of cells written is
	returned in outLength (unless outLength is a null pointer to suppress
	this output). 

	If inSize is greater than the number of cells, N, then outVector[N] will
	be null terminated by placing all zero values in all the cell slots. 

	All the sCell_Content slots will be null pointers because these cells
	will not represent actual storage in a row. GetListCells is expected to be
	used in the implementation of methods like
	AB_Table_MakePersonList(). -*/
{
	ab_cell_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Table_kClassName, "GetListCells")

	ab_Defaults* defs = AB_Table_get_defaults(self, ev);
	if ( defs )
	{
		outCount = defs->GetDefaultListCells(ev, outVector, inSize, outLength);
	}

	ab_Env_EndMethod(ev)
	return outCount;
}
