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

/* file: abmodel.c 
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
	static const char* ab_Model_kClassName /*i*/ = "ab_Model";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_Model::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	ab_num viewCount = 0;
	if ( mModel_Views.IsOpenOrClosingObject() )
		viewCount = mModel_Views.count_links();
	
	sprintf(outXmlBuf,
		"<ab:model:str me=\"^%lX\" row=\"#%lX\" st=\"^%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\" vs=\"%lu\"/>",
		(long) this,                                // me=\"^%lX\"
		(long) mPart_RowUid,                        // row=\"#%lX\"
		(long) mPart_Store,                         // st=\"^%lX\"
		(unsigned long) mObject_RefCount,           // rc=\"%lu\"
		this->GetObjectAccessAsString(),            // ac=\"%.9s\"
		this->GetObjectUsageAsString(),             // us=\"%.9s\"
		(unsigned long) viewCount                   // views=\"%lu\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_Model::CloseObject(ab_Env* ev) /*i*/ // CloseModel()
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseModel(ev);
		this->MarkShut();
	}
}

void ab_Model::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:model>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PushDepth(ev); // indent all objects in the list

		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
		
		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, mPart_Store->ObjectAsString(ev, xmlBuf));
		
		if ( mModel_Views.IsOpenOrClosingObject() )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mModel_Views.PrintObject(ev, ioPrinter);
		}
		
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab:model>");
#endif /*AB_CONFIG_PRINT*/
}

ab_Model::~ab_Model() /*i*/
{
#if AB_CONFIG_DEBUG
	AB_ASSERT(mModel_Views.IsEmpty());
#endif /*AB_CONFIG_DEBUG*/
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Model methods (typically no need to override)

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

ab_ref_count ab_Model::AddView(ab_Env* ev, ab_View* ioView) /*i*/
{
	ab_ref_count outCount = 0;
	ab_Env_BeginMethod(ev, ab_Model_kClassName, "AddView")
	
	if ( this->IsOpenObject() )
	{
		outCount = mModel_Views.AddObject(ev, ioView);

#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
		{
			ev->Trace( "<ab:model:add:view $$ me=\"^%lX\" view=\"^%lX\" rc=\"%lu\"/>",
				(long) this, (long) ioView, (long) outCount);
			this->TraceObject(ev);
			ioView->TraceObject(ev);
		}
#endif /*AB_CONFIG_TRACE*/
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
	return outCount;
}

ab_ref_count ab_Model::HasView(ab_Env* ev, const ab_View* inView) const /*i*/
{
	ab_ref_count outCount = 0;
	ab_Env_BeginMethod(ev, ab_Model_kClassName, "HasView")
	
	if ( this->IsOpenOrClosingObject() )
	{
		outCount = mModel_Views.HasObject(ev, inView);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpenOrClosing);

	ab_Env_EndMethod(ev)
	return outCount;
}

ab_ref_count ab_Model::SubView(ab_Env* ev, ab_View* ioView) /*i*/
{
	ab_ref_count outCount = 0;
	ab_Env_BeginMethod(ev, ab_Model_kClassName, "SubView")
	
	if ( this->IsOpenOrClosingObject() )
	{
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
		{
			this->TraceObject(ev);
			ioView->TraceObject(ev);
		}
#endif /*AB_CONFIG_TRACE*/

		outCount = mModel_Views.SubObject(ev, ioView);

#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
			ev->Trace( "<ab:model:sub:view $$ me=\"^%lX\" view=\"^%lX\" rc=\"%lu\"/>",
				(long) this, (long) ioView, (long) outCount);
#endif /*AB_CONFIG_TRACE*/
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpenOrClosing);

	ab_Env_EndMethod(ev)
	return outCount;
}

ab_ref_count ab_Model::CutView(ab_Env* ev, ab_View* ioView) /*i*/
{
	ab_ref_count outCount = 0;
	ab_Env_BeginMethod(ev, ab_Model_kClassName, "CutView")
	
	if ( this->IsOpenOrClosingObject() )
	{
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
		{
			this->TraceObject(ev);
			ioView->TraceObject(ev);
		}
#endif /*AB_CONFIG_TRACE*/

		outCount = mModel_Views.CutObject(ev, ioView);

#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
			ev->Trace( "<ab:model:cut:view $$ me=\"^%lX\" view=\"^%lX\" rc=\"%lu\"/>",
				(long) this, (long) ioView, (long) outCount);
#endif /*AB_CONFIG_TRACE*/
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpenOrClosing);

	ab_Env_EndMethod(ev)
	return outCount;
}

ab_num ab_Model::CutAllViews(ab_Env* ev) /*i*/
{
	ab_num outCount = 0;
	ab_Env_BeginMethod(ev, ab_Model_kClassName, "CutAllViews")
	
	if ( this->IsOpenOrClosingObject() )
	{
		outCount = mModel_Views.CutAllObjects(ev);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpenOrClosing);

	ab_Env_EndMethod(ev)
	return outCount;
}

ab_num ab_Model::CountViews(ab_Env* ev) const /*i*/
{
	ab_num outCount = 0;
	ab_Env_BeginMethod(ev, ab_Model_kClassName, "CountViews")
	
	if ( this->IsObject() )
	{
		outCount = mModel_Views.CountObjects(ev);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotObject);

	ab_Env_EndMethod(ev)
	return outCount;
}
	 
// ````` ````` ````` `````   ````` ````` ````` `````  
// static ab_Model methods

ab_Model::ab_Model(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	ab_row_uid inRowUid, ab_Store* ioStore)
	: ab_Part(ev, inUsage, inRowUid, ioStore),
	mModel_Views(ab_Usage::kMember)
{
}

void ab_Model::CloseModel(ab_Env* ev) /*i*/ // CloseObject()
{
	ab_Env_BeginMethod(ev, ab_Model_kClassName, "CloseModel")
	
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	if ( this->IsObject() )
	{
		if ( !mModel_Views.IsEmpty() )
		{
			ab_Change change(ab_Usage::kStack, this, ab_Change_kClosing);
		  	this->ClosingModel(ev, &change);
		  	change.CloseChange(ev);
		}
  		mModel_Views.CloseObject(ev);
		this->ClosePart(ev);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotObject);

	ab_Env_EndMethod(ev)
}

void ab_Model::BeginModelFlux(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Model_kClassName, "BeginModelFlux")

	if ( this->IsObject() )
	{
		ab_ObjectSet* viewSet = &mModel_Views;
  		ab_View* view = (ab_View*) viewSet->FirstMember(ev);
		for ( ; view; view = (ab_View*) viewSet->NextMember(ev) )
		{
#ifdef AB_CONFIG_TRACE
			if ( ev->DoTrace() )
			{
				this->TraceObject(ev);
				view->TraceObject(ev);
			}
#endif /*AB_CONFIG_TRACE*/
			view->SeeBeginModelFlux(ev, this);
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotObject);

	ab_Env_EndMethod(ev)
}

void ab_Model::EndModelFlux(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Model_kClassName, "EndModelFlux")

	if ( this->IsObject() )
	{
		ab_ObjectSet* viewSet = &mModel_Views;
  		ab_View* view = (ab_View*) viewSet->FirstMember(ev);
		for ( ; view; view = (ab_View*) viewSet->NextMember(ev) )
		{
#ifdef AB_CONFIG_TRACE
			if ( ev->DoTrace() )
			{
				this->TraceObject(ev);
				view->TraceObject(ev);
			}
#endif /*AB_CONFIG_TRACE*/
			view->SeeEndModelFlux(ev, this);
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotObject);

	ab_Env_EndMethod(ev)
}

void ab_Model::ClosingModel(ab_Env* ev, const ab_Change* inChange) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Model_kClassName, "ClosingModel")

	if ( this->IsObject() )
	{
#ifdef AB_CONFIG_TRACE
		mModel_Views.TraceSet(ev);
#endif /*AB_CONFIG_TRACE*/

		ab_ObjectSet* viewSet = &mModel_Views;
  		ab_View* view = (ab_View*) viewSet->FirstMember(ev);
		for ( ; view; view = (ab_View*) viewSet->NextMember(ev) )
		{
#ifdef AB_CONFIG_TRACE
			if ( ev->DoTrace() )
			{
				this->TraceObject(ev);
				view->TraceObject(ev);
			}
#endif /*AB_CONFIG_TRACE*/
			view->SeeClosingModel(ev, this, inChange);
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotObject);

	ab_Env_EndMethod(ev)
}

void ab_Model::ChangedModel(ab_Env* ev, const ab_Change* inChange) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Model_kClassName, "ChangedModel")

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() ) 
		inChange->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	if ( this->IsOpenOrClosingObject() )
	{
 #ifdef AB_CONFIG_TRACE
		mModel_Views.TraceSet(ev);
#endif /*AB_CONFIG_TRACE*/

		ab_ObjectSet* viewSet = &mModel_Views;
  		ab_View* view = (ab_View*) viewSet->FirstMember(ev);
		for ( ; view; view = (ab_View*) viewSet->NextMember(ev) )
		{
#ifdef AB_CONFIG_TRACE
			if ( ev->DoTrace() )
			{
				this->TraceObject(ev);
				view->TraceObject(ev);
				ev->Trace( "<ab:model:changed $$ me=\"^%lX\" view=\"^%lX\"/>",
					(long) this, (long) view);
			}
#endif /*AB_CONFIG_TRACE*/

			view->SeeChangedModel(ev, this, inChange);
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpenOrClosing);

	ab_Env_EndMethod(ev)
}

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/
