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

/* file: abcolset.c 
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
	static const char* ab_ColumnSet_kClassName /*i*/ = "ab_ColumnSet";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_ColumnSet methods

// all default implementations just raise error AB_Env_kFaultMethodStubOnly
	
ab_column_pos ab_ColumnSet::AddColumn(ab_Env* ev, const AB_Column* inCol, /*i*/
                ab_column_pos inPos, ab_bool inChangeIndex)
 {
 	ab_column_pos outPos = 0;
	ab_Env_BeginMethod(ev, ab_ColumnSet_kClassName, "AddColumn")

	AB_USED_PARAMS_3(inCol, inPos, inChangeIndex);
	
  	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);
	
	ab_Env_EndMethod(ev)
	return outPos;
}
               
ab_bool ab_ColumnSet::PutColumn(ab_Env* ev, const AB_Column* col, /*i*/
                ab_column_pos inPos, ab_bool inChangeIndex)
{
	ab_Env_BeginMethod(ev, ab_ColumnSet_kClassName, "PutColumn")

	AB_USED_PARAMS_3(col, inPos, inChangeIndex);

  	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);
	
	ab_Env_EndMethod(ev)
	return ev->Good();
}
                
ab_bool ab_ColumnSet::GetColumn(ab_Env* ev, AB_Column* outCol, /*i*/
	 ab_column_pos inPos)
{
	ab_Env_BeginMethod(ev, ab_ColumnSet_kClassName, "GetColumn")

	AB_USED_PARAMS_2(outCol, inPos);

  	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);
	
	ab_Env_EndMethod(ev)
	return ev->Good();
}
                
ab_column_pos ab_ColumnSet::CutColumn(ab_Env* ev, ab_column_uid inColUid, /*i*/
			    ab_bool inDoChangeIndex)
{
	ab_column_pos outPos = 0;
	ab_Env_BeginMethod(ev, ab_ColumnSet_kClassName, "CutColumn")

	AB_USED_PARAMS_2(inColUid, inDoChangeIndex);

  	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);
	
	ab_Env_EndMethod(ev)
	return outPos;
}
                 
ab_column_pos ab_ColumnSet::FindColumn(ab_Env* ev, /*i*/
	 const AB_Column* inCol)
{
	ab_column_pos outPos = 0;
	ab_Env_BeginMethod(ev, ab_ColumnSet_kClassName, "FindColumn")

	AB_USED_PARAMS_1(inCol);

  	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);
	
	ab_Env_EndMethod(ev)
	return outPos;
}
               
ab_column_count ab_ColumnSet::GetAllColumns(ab_Env* ev, /*i*/
	AB_Column* outVector, ab_column_count inSize,
	ab_column_count* outLength) const
{
	ab_column_count outCount = 0;
	ab_Env_BeginMethod(ev, ab_ColumnSet_kClassName, "GetAllColumns")

	AB_USED_PARAMS_3(outVector,inSize,outLength);

  	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);
	
	ab_Env_EndMethod(ev)
	return outCount;
}
                
ab_column_count ab_ColumnSet::PutAllColumns(ab_Env* ev, /*i*/
                const AB_Column* inVector, ab_bool inChangeIndex)
{
	ab_column_count outCount = 0;
	ab_Env_BeginMethod(ev, ab_ColumnSet_kClassName, "PutAllColumns")

	AB_USED_PARAMS_2(inVector,inChangeIndex);

  	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);
	
	ab_Env_EndMethod(ev)
	return outCount;
}
                
ab_column_count ab_ColumnSet::CountColumns(ab_Env* ev) const /*i*/
{
	ab_column_count outCount = 0;
	ab_Env_BeginMethod(ev, ab_ColumnSet_kClassName, "CountColumns")

  	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);
	
	ab_Env_EndMethod(ev)
	return outCount;
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_ColumnSet::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	sprintf(outXmlBuf,
		"<ab:column:set:str me=\"^%lX\" row=\"#%lX\" st=\"^%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
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

void ab_ColumnSet::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseColumnSet(ev);
		this->MarkShut();
	}
}

void ab_ColumnSet::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:column:set>");
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
	ioPrinter->PutString(ev, "</ab:column:set>");
#endif /*AB_CONFIG_PRINT*/
}

ab_ColumnSet::~ab_ColumnSet() /*i*/
{
}
   
// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_ColumnSet methods

ab_ColumnSet::ab_ColumnSet(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	 ab_Store* ioStore)
	 : ab_Part(ev, inUsage,
	 	ab_Session::GetGlobalSession()->NewTempRowUid(),
	 	ioStore
	 )
{
}

void ab_ColumnSet::CloseColumnSet(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_ColumnSet_kClassName, "CloseColumnSet")

	this->ClosePart(ev);
	
	ab_Env_EndMethod(ev)
}
