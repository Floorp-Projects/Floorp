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

/* file: abpart.c 
** Some portions derive from public domain IronDoc code and interfaces.
**
** Changes:
** <1> 05Dec1998 first implementation
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
	static const char* ab_Part_kClassName /*i*/ = "ab_Part";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_Part::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	XP_SPRINTF(outXmlBuf,
		"<ab:part:str me:st=\"^%lX:^%lX\" row=\"#%lX\" seed=\"#%lX:%lX:%lX:%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                                // me:st=\"^%lX
		(long) mPart_Store,                         // :^%lX\"
		(long) mPart_RowUid,                        // row=\"#%lX\"

		(long) mPart_NameSetSeed,      // seed=\"#%lX
		(long) mPart_ColumnSetSeed,    // :%lX
		(long) mPart_RowSetSeed,       // :%lX
		(long) mPart_RowContentSeed,   // :%lX\"
		
		(unsigned long) mObject_RefCount,           // rc=\"%lu\"
		this->GetObjectAccessAsString(),            // ac=\"%.9s\"
		this->GetObjectUsageAsString()              // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_Part::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->ClosePart(ev);
		this->MarkShut();
	}
}

void ab_Part::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:part>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PushDepth(ev); // indent all objects in the list

		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
		
		if ( mPart_Store )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mPart_Store->PrintObject(ev, ioPrinter);
		}
		
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab:part>");
#endif /*AB_CONFIG_PRINT*/
}

ab_Part::~ab_Part() /*i*/
{
	AB_ASSERT(mPart_Store==0);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	ab_Object* obj = mPart_Store;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_Part_kClassName);
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
}
            
// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_Model methods

ab_Part::ab_Part(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	ab_row_uid inRowUid, ab_Store* ioStore)
	: ab_Object(inUsage),
	mPart_Store(ioStore),
	mPart_RowUid(inRowUid),
	
	// we want all part seeds to be out of sync with store at start:
	mPart_NameSetSeed( 0 ),
	mPart_ColumnSetSeed( 0 ),
	mPart_RowSetSeed( 0 ),
	mPart_RowContentSeed( 0 )
{
	ab_Env_BeginMethod(ev, ab_Part_kClassName, ab_Part_kClassName)

	if ( ioStore )
	{
	  	if ( ioStore->AcquireObject(ev) )
	  	{
#ifdef AB_CONFIG_TRACE
			if ( ev->DoTrace() )
				ev->Trace("<ab:part:make:acquire:store me=\"#%lX\" store=\"#%lX\"/>",
					(long) this, (long) ioStore);
#endif /*AB_CONFIG_TRACE*/
		}
		else
	  		mPart_Store = 0; // drop the reference if acquire fails
	}
	else ev->NewAbookFault(ab_Part_kFaultNullStore);

	ab_Env_EndMethod(ev)
}

void ab_Part::ClosePart(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Part_kClassName, "ClosePart")

	ab_Object* obj = mPart_Store;
  	if ( obj )
  	{
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
			ev->Trace("<ab:part:close:release:store me=\"#%lX\" store=\"#%lX\"/>",
				(long) this, (long) obj);
#endif /*AB_CONFIG_TRACE*/
  		mPart_Store = 0;
  		obj->ReleaseObject(ev);
  	}

	ab_Env_EndMethod(ev)
}

ab_Store* ab_Part::GetOpenStoreFromOpenPart(ab_Env* ev) const /*i*/
{
	ab_Store* outStore = 0;
	ab_error_uid newError = 0;
	
	if ( this->IsOpenObject() )
	{
	  	ab_Store* store = GetPartStore();
	  	if ( store )
	  	{
	  		if ( store->IsOpenObject() )
	  			outStore = store;
	  		else
		  		newError = ab_Store_kFaultNotOpen;
	  	}
		else newError = ab_Part_kFaultNullStore;
	}
	else newError = ab_Part_kFaultNotOpen;
	
	if ( newError )
	{
		ab_Env_BeginMethod(ev, ab_Part_kClassName, "GetOpenStoreFromOpenPart")
		
		ev->NewAbookFault(newError);

#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
			this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

		ab_Env_EndMethod(ev)
	}
	return outStore;
}
