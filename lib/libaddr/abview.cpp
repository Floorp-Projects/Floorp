/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* file: abview.c 
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
	static const char* ab_View_kClassName /*i*/ = "ab_View";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_View::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	sprintf(outXmlBuf,
		"<ab:view:str me=\"^%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\" ms=\"%lu\" int=\"#%lX\"/>",
		(long) this,                                // me=\"^%lX\"
		(unsigned long) mObject_RefCount,           // rc=\"%lu\"
		this->GetObjectAccessAsString(),            // ac=\"%.9s\"
		this->GetObjectUsageAsString(),             // us=\"%.9s\"
		(unsigned long) mView_Models.count_links(), // models=\"%lu\"
		(unsigned long) mView_Interests             // int=\"#%lX\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_View::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseView(ev);
		this->MarkShut();
	}
}

void ab_View::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:view>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PushDepth(ev); // indent all objects in the list

		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ab_View::ObjectAsString(ev, xmlBuf));
		
		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		mView_Models.PrintObject(ev, ioPrinter);
		
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab:view>");
#endif /*AB_CONFIG_PRINT*/
}

ab_View::~ab_View() /*i*/
{
#if AB_CONFIG_DEBUG
	AB_ASSERT(mView_Models.IsEmpty());
#endif /*AB_CONFIG_DEBUG*/
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_View methods

ab_View::ab_View(const ab_Usage& inUsage, ab_change_mask interests) /*i*/
	: ab_Object(inUsage),
	  mView_Interests(interests), mView_Models(ab_Usage::kMember)
{
}

void ab_View::CloseView(ab_Env* ev)  /*i*/
{
	ab_Env_BeginMethod(ev, ab_View_kClassName, "CloseView")
	
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	if ( this->IsObject() )
	{
		ab_ObjectSet* modelSet = &mView_Models;
  		ab_Model* model = (ab_Model*) modelSet->FirstMember(ev);
		for ( ; model; model = (ab_Model*) modelSet->NextMember(ev) )
		{
			model->CutView(ev, this);
		}
  		mView_Models.CloseObjectSet(ev);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotObject);

	ab_Env_EndMethod(ev)
}

/*=============================================================================
 * ab_TableStoreView: a View that notices stale rows resulting from changes
 */
 
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_TableStoreView_kClassName /*i*/ = "ab_TableStoreView";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/
    
// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_View responses to ab_Model messages

void ab_TableStoreView::SeeBeginModelFlux(ab_Env* ev, ab_Model* ioModel) /*i*/
{
	ab_Env_BeginMethod(ev, ab_TableStoreView_kClassName, "SeeBeginModelFlux")

	AB_USED_PARAMS_1(ioModel);

	if ( this->IsOpenOrClosingObject() && mTableStoreView_Table )
	{
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
		{
			this->TraceObject(ev);
			mTableStoreView_Table->TraceObject(ev);
		}
#endif /*AB_CONFIG_TRACE*/
		mTableStoreView_Table->TableSeesBeginStoreFlux(ev);
	}

	ab_Env_EndMethod(ev)
}

void ab_TableStoreView::SeeEndModelFlux(ab_Env* ev, ab_Model* ioModel) /*i*/
{
	ab_Env_BeginMethod(ev, ab_TableStoreView_kClassName, "SeeEndModelFlux")

	AB_USED_PARAMS_1(ioModel);
	
	if ( this->IsOpenOrClosingObject() && mTableStoreView_Table )
	{
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
		{
			this->TraceObject(ev);
			mTableStoreView_Table->TraceObject(ev);
		}
#endif /*AB_CONFIG_TRACE*/
		mTableStoreView_Table->TableSeesEndStoreFlux(ev);
	}

	ab_Env_EndMethod(ev)
}

void ab_TableStoreView::SeeChangedModel(ab_Env* ev, ab_Model* ioModel, /*i*/
             const ab_Change* inChange)
{
	ab_Env_BeginMethod(ev, ab_TableStoreView_kClassName, "SeeChangedModel")

	AB_USED_PARAMS_1(ioModel);
	
	if ( this->IsOpenOrClosingObject() && mTableStoreView_Table )
	{
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
		{
			this->TraceObject(ev);
			mTableStoreView_Table->TraceObject(ev);
		}
#endif /*AB_CONFIG_TRACE*/
		mTableStoreView_Table->TableSeesChangedStore(ev, inChange);
	}

	ab_Env_EndMethod(ev)
}
             
void ab_TableStoreView::SeeClosingModel(ab_Env* ev, ab_Model* ioModel, /*i*/
             const ab_Change* inChange)
{
	ab_Env_BeginMethod(ev, ab_TableStoreView_kClassName, "SeeClosingModel")

	AB_USED_PARAMS_1(ioModel);
	
	if ( this->IsOpenOrClosingObject() && mTableStoreView_Table )
	{
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
		{
			this->TraceObject(ev);
			mTableStoreView_Table->TraceObject(ev);
		}
#endif /*AB_CONFIG_TRACE*/
		mTableStoreView_Table->TableSeesClosingStore(ev, inChange);
	}

	ab_Env_EndMethod(ev)
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_TableStoreView::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	sprintf(outXmlBuf,
		"<ab:table:store:view:str me=\"^%lX\" tb:st=\"^%lX:%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\" ms=\"%lu\" int=\"#%lX\"/>",
		(long) this,                                // me=\"^%lX\"
		(long) mTableStoreView_Table,               // tb:st=\"^%lX
		(long) mTableStoreView_Store,               // :%lX\"
		
		(unsigned long) mObject_RefCount,           // rc=\"%lu\"
		this->GetObjectAccessAsString(),            // ac=\"%.9s\"
		this->GetObjectUsageAsString(),             // us=\"%.9s\"
		(unsigned long) mView_Models.count_links(), // models=\"%lu\"
		(unsigned long) mView_Interests             // int=\"#%lX\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_TableStoreView::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseTableStoreView(ev);
		this->MarkShut();
	}
}

void ab_TableStoreView::PrintObject(ab_Env* ev, /*i*/
	ab_Printer* ioPrinter) const
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:table:store:view>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PushDepth(ev); // indent all objects in the list

		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ab_View::ObjectAsString(ev, xmlBuf));
		
		if ( mTableStoreView_Table )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			ioPrinter->PutString(ev,
				mTableStoreView_Table->ObjectAsString(ev, xmlBuf));
		}
		
		if ( mTableStoreView_Store )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			ioPrinter->PutString(ev,
				mTableStoreView_Store->ObjectAsString(ev, xmlBuf));
		}
		
		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		mView_Models.PrintObject(ev, ioPrinter);
		
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab:table:store:view>");
#endif /*AB_CONFIG_PRINT*/
}

ab_TableStoreView::~ab_TableStoreView() /*i*/
{
	AB_ASSERT(mTableStoreView_Table==0);
	AB_ASSERT(mTableStoreView_Store==0);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	ab_Object* obj = mTableStoreView_Table;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_TableStoreView_kClassName);
	obj = mTableStoreView_Store;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_TableStoreView_kClassName);
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_TableStoreView methods

ab_TableStoreView::ab_TableStoreView(ab_Env* ev, const ab_Usage& inUsage, /*i*/
     ab_Store* ioStore, ab_Table* ioTable)
     : ab_View(inUsage, (ab_change_mask) 0xFFFFFFFF),

	mTableStoreView_Table( 0 ),
	mTableStoreView_Store( 0 )
{
	ab_Env_BeginMethod(ev, ab_TableStoreView_kClassName, ab_TableStoreView_kClassName)

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace( "<ab:table:store:view:ctor $$ me=\"^%lX\" st=\"^%lX\" table=\"^%lX\"/>",
			(long) this, (long) ioStore, (long) ioTable);
#endif /*AB_CONFIG_TRACE*/

	if ( ioStore && ioTable )
	{
	  	if ( ioStore->AcquireObject(ev) )
	  	{
	  		mTableStoreView_Store = ioStore;
		  	if ( ioTable->AcquireObject(ev) )
		  	{
		  		mTableStoreView_Table = ioTable;
		  		ioStore->AddView(ev, this);
		  	}
	  	}
	}
	else ev->NewAbookFault(ab_Object_kFaultNullObjectParameter);

	ab_Env_EndMethod(ev)
}
        
void ab_TableStoreView::CloseTableStoreView(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_TableStoreView_kClassName, "CloseTableStoreView")

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace( "<ab:table:store:view:dtor $$ me=\"^%lX\" st=\"^%lX\"/>",
			(long) this, (long) mTableStoreView_Store);
#endif /*AB_CONFIG_TRACE*/

	if ( mTableStoreView_Store && mTableStoreView_Store->IsOpenObject() )
	{
		mTableStoreView_Store->CutView(ev, this);
	}

	ab_Object* obj = mTableStoreView_Table;
	if ( obj )
	{
		mTableStoreView_Table = 0;
		obj->ReleaseObject(ev);
	}
	
	obj = mTableStoreView_Store;
	if ( obj )
	{
		mTableStoreView_Store = 0;
		obj->ReleaseObject(ev);
	}
	this->CloseView(ev);

	ab_Env_EndMethod(ev)
}

/*=============================================================================
 * ab_StaleRowView: a View that notices stale rows resulting from changes
 */
 
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_StaleRowView_kClassName /*i*/ = "ab_StaleRowView";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_View responses to ab_Model messages

void ab_StaleRowView::SeeBeginModelFlux(ab_Env* ev, ab_Model* ioModel) /*i*/
{
	ab_Env_BeginMethod(ev, ab_StaleRowView_kClassName, "SeeBeginModelFlux")

	AB_USED_PARAMS_1(ioModel);

	this->PerformStaleRowHandling(ev);

	ab_Env_EndMethod(ev)
}

void ab_StaleRowView::SeeEndModelFlux(ab_Env* ev, ab_Model* ioModel) /*i*/
{
	ab_Env_BeginMethod(ev, ab_StaleRowView_kClassName, "SeeEndModelFlux")

	AB_USED_PARAMS_1(ioModel);

	ab_Env_EndMethod(ev)
}

void ab_StaleRowView::SeeChangedModel(ab_Env* ev, ab_Model* ioModel, /*i*/
                 const ab_Change* inChange)
{
	ab_Env_BeginMethod(ev, ab_StaleRowView_kClassName, "SeeChangedModel")

	AB_USED_PARAMS_1(ioModel);

	ab_change_mask mask = inChange->mChange_Mask;
	
	if ( mStaleRowView_CloseTarget && mStaleRowView_RowTarget )
	{
		if ( inChange->mChange_Row == mStaleRowView_RowTarget ||
			mask & (ab_Change_kAllRowChanges | ab_Change_kClosing) )
		{
			this->PerformStaleRowHandling(ev);
		}
	}

	ab_Env_EndMethod(ev)
}

void ab_StaleRowView::SeeClosingModel(ab_Env* ev, ab_Model* ioModel, /*i*/
                 const ab_Change* inChange)
{
	ab_Env_BeginMethod(ev, ab_StaleRowView_kClassName, "SeeClosingModel")

	AB_USED_PARAMS_1(inChange);

	this->PerformStaleRowHandling(ev);
	this->CutModel(ev, ioModel);
	
	ab_Env_EndMethod(ev)
}
    
// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_StaleRowView::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{

	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	sprintf(outXmlBuf,
		"<ab:stale:row:view:str me=\"^%lX\" obj=\"^%lX\" row=\"#%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                                // me=\"^%lX\"
		(unsigned long) mStaleRowView_CloseTarget,  // obj=\"^%lX\"
		(unsigned long) mStaleRowView_RowTarget,    // row=\"#%lX\"
		(unsigned long) mObject_RefCount,           // rc=\"%lu\"
		this->GetObjectAccessAsString(),            // ac=\"%.9s\"
		this->GetObjectUsageAsString()              // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_StaleRowView::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseStaleRowView(ev);
		this->MarkShut();
	}
}

void ab_StaleRowView::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:stale:row:view>");
	ioPrinter->PushDepth(ev); // indent 

	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];
	
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));

	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));

	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	this->ab_View::PrintObject(ev, ioPrinter);

	ioPrinter->PopDepth(ev); // stop indentation
	ioPrinter->PutString(ev, "</ab:stale:row:view>");
#endif /*AB_CONFIG_PRINT*/
}

ab_StaleRowView::~ab_StaleRowView() /*i*/
{
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	ab_Object* obj = mStaleRowView_CloseTarget;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_StaleRowView_kClassName);
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_StaleRowView methods

ab_StaleRowView::ab_StaleRowView(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	ab_Object* ioCloseTarget, ab_row_uid inRowTarget)
	: ab_View(inUsage, (ab_Change_kAllRowChanges | ab_Change_kClosing) ),
	mStaleRowView_CloseTarget(ioCloseTarget),
	mStaleRowView_RowTarget(inRowTarget)
{
	AB_USED_PARAMS_1(ev);
}

void ab_StaleRowView::CloseStaleRowView(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_StaleRowView_kClassName, "CloseStaleRowView")
	
	ab_Object* target = mStaleRowView_CloseTarget;
	if ( target )
	{
		mStaleRowView_CloseTarget = 0;
		target->ReleaseObject(ev);
	}
	this->CloseView(ev);

	ab_Env_EndMethod(ev)
}

void ab_StaleRowView::PerformStaleRowHandling(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_StaleRowView_kClassName, "PerformStaleRowHandling")

	ab_Object* target = mStaleRowView_CloseTarget;
	if ( target )
	{
		mStaleRowView_CloseTarget = 0;
		
		target->CloseObject(ev);
		target->ReleaseObject(ev);
	}

	ab_Env_EndMethod(ev)
}

