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

/* file: abnamset.c 
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
	static const char* ab_NameSet_kClassName /*i*/ = "ab_NameSet";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_NameSet methods

// default methods understand only standard column names

const char* ab_NameSet::GetName(ab_Env* ev, ab_column_uid inToken) const /*i*/
{
	const char* outName = 0;
	ab_Env_BeginMethod(ev, ab_NameSet_kClassName, "GetName")

	const char* name = AB_ColumnUid_AsString(inToken, ev->AsSelf());
	if ( name )
  	{
  		outName = name;
  	}
  	else ev->NewAbookFault(AB_Column_kFaultNotStandardUid);
  
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
	{
		const char* foundName = (outName)? outName : "";
		ev->Trace(
			"<ab:name:set:get:name uid=\"#%lX\" name=\"%.128s\">",
			 (long) inToken, foundName);
	}
#endif /*AB_CONFIG_TRACE*/
	
	ab_Env_EndMethod(ev)
	return outName;
}
               

ab_column_uid ab_NameSet::GetToken(ab_Env* ev, const char* inName) const /*i*/
{
	ab_column_uid outUid = 0;
	ab_Env_BeginMethod(ev, ab_NameSet_kClassName, "GetToken")

	ab_column_uid col = AB_String_AsStandardColumnUid(inName, ev->AsSelf());
	if ( col )
	{
		outUid = col;
	}
  	else ev->NewAbookFault(AB_Column_kFaultNotStandardName);
  
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
	{
		const char* name = (inName)? inName : "";
		ev->Trace(
			"<ab:name:set:get:token name=\"%.128s\" uid=\"#%lX\"/>",
			 name, (long) outUid);
	}
#endif /*AB_CONFIG_TRACE*/
	
	ab_Env_EndMethod(ev)
	return outUid;
}
               

ab_column_uid ab_NameSet::NewToken(ab_Env* ev, const char* inName) /*i*/
{
	ab_column_uid outUid = 0;
	ab_Env_BeginMethod(ev, ab_NameSet_kClassName, "NewToken")

	ab_column_uid col = AB_String_AsStandardColumnUid(inName, ev->AsSelf());
	if ( col )
	{
		outUid = col;
	}
  	else ev->NewAbookFault(AB_Column_kFaultNotStandardName);
  
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
	{
		const char* name = (inName)? inName : "";
		ev->Trace(
			"<ab:name:set:new:token name=\"%.128s\" uid=\"#%lX\"/>",
			 name, (long) outUid);
	}
#endif /*AB_CONFIG_TRACE*/
	
	ab_Env_EndMethod(ev)
	return outUid;
}
               

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_NameSet::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	sprintf(outXmlBuf,
		"<ab:name:set:str me=\"^%lX\" row=\"#%lX\" st=\"^%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                                // me=\"^%lX\"
		(long) mPart_RowUid,                        // row=\"#%lX\"
		(long) mPart_Store,                         // st=\"^%lX\"
		(unsigned long) mObject_RefCount,           // rc=\"%lu\"
		this->GetObjectAccessAsString(),            // ac=\"%.9s\"
		this->GetObjectUsageAsString()              // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_NameSet::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseNameSet(ev);
		this->MarkShut();
	}
}

void ab_NameSet::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:name:set>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
		ioPrinter->PushDepth(ev); // indent all objects in the list
		
		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, mPart_Store->ObjectAsString(ev, xmlBuf));
		
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab:name:set>");
#endif /*AB_CONFIG_PRINT*/
}

ab_NameSet::~ab_NameSet() /*i*/
{
}
   
// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_NameSet methods

ab_NameSet::ab_NameSet(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	 ab_Store* ioStore)
	 : ab_Part(ev, inUsage,
	 	ab_Session::GetGlobalSession()->NewTempRowUid(),
	 	ioStore
	 )
{
}

void ab_NameSet::CloseNameSet(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_NameSet_kClassName, "CloseNameSet")

	this->ClosePart(ev);
	
	ab_Env_EndMethod(ev)
}
