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

/* Insert copyright and license here 1997 */

/* file: abchange.c 
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
	static const char* ab_Change_kClassName /*i*/ = "ab_Change";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods


char* ab_Change::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	sprintf(outXmlBuf,
		"<ab:change:str me=\"^%lX\" id:ma=\"#%lX:%lX\" mo:st=\"#%lX:%lX\" row=\"#%lX:%lX:%lX:%lX\" col=\"#%lX:%lX:%lX\"/>",
		(long) this,                     // me=\"^%lX\"
		
		(long) mChange_Uid,              // id:ma=\"#%lX
		(long) mChange_Mask,             // :%lX\"
		
		(long) mChange_ModelUid,         // mo:st=\"#%lX:
		(long) mChange_StoreUid,         // :%lX\"
		
		(long) mChange_Row,              // row=\"#%lX
		(long) mChange_RowPos,           // :%lX:
		(long) mChange_RowCount,         // %lX:
		(long) mChange_RowParent,        // %lX\"
		
		(long) mChange_Column,           // col=\"#%lX
		(long) mChange_ColPos,           // :%lX:
		(long) mChange_ColCount          // %lX\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_Change::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseChange(ev);
		this->MarkShut();
	}
}
 
void ab_Change::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:change>");
	ioPrinter->PushDepth(ev); // indent 

	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];
	
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));

	ioPrinter->PopDepth(ev); // stop indentation
	ioPrinter->PutString(ev, "</ab:change>");
#endif /*AB_CONFIG_PRINT*/
}

ab_Change::~ab_Change() /*i*/
{
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly methods

ab_Change::ab_Change(const ab_Usage& inUsage, /*i*/
      const ab_Model* inModel, ab_change_mask inMask)
	: ab_Object(inUsage),
	mChange_Mask(inMask),
	mChange_ModelUid(inModel->GetPartRowUid())
{
	ab_Store* store = inModel->GetPartStore();
	mChange_StoreUid = store->GetPartRowUid();
	mChange_Uid = ab_Session::GetGlobalSession()->NewTempChangeUid();
	
	mChange_Column = 0;
	mChange_ColPos = 0;
	mChange_ColCount = 0;
	
	mChange_Row = 0;
	mChange_RowPos = 0;
	mChange_RowCount = 0;
	mChange_RowParent = 0;
}

void ab_Change::CloseChange(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Change_kClassName, "CloseChange")
	
	// this->MarkShut();

	ab_Env_EndMethod(ev)
}


// ````` ````` ````` `````   ````` ````` ````` `````  
// copying *is* allowed

/*protected*/
void ab_Change::SetChange(const ab_Change& other) /*i*/
{
    mChange_Uid = other.mChange_Uid;
    mChange_Mask = other.mChange_Mask;

    mChange_ModelUid = other.mChange_ModelUid;
    mChange_StoreUid = other.mChange_StoreUid;

    mChange_Row = other.mChange_Row;
    mChange_RowPos = other.mChange_RowPos;
    mChange_RowCount = other.mChange_RowCount;
	mChange_RowParent = other.mChange_RowParent;

    mChange_Column = other.mChange_Column;
    mChange_ColPos = other.mChange_ColPos;
    mChange_ColCount = other.mChange_ColCount;
}

ab_Change::ab_Change(const ab_Change& other, const ab_Usage& inUsage) /*i*/
	: ab_Object(other, inUsage)
{
	SetChange(other);
}

ab_Change& ab_Change::operator=(const ab_Change& other) /*i*/
{
	if ( this != &other )
	{
		this->ab_Object::operator=(other);
		SetChange(other);
	}
	return *this;
}
