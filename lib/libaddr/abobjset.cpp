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

/* file: abobjset.c 
** Some portions might derive from public domain IronDoc interfaces.
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
	static const char* ab_ObjectSet_kClassName /*i*/ = "ab_ObjectSet";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_ObjectSet::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	sprintf(outXmlBuf,
		"<ab:object:set:str me=\"^%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\" cl=\"%lu\" s=\"#%lX\"/>",
		(long) this,                         // me=\"^%lX\"
		(unsigned long) mObject_RefCount,    // rc=\"%lu\"
		this->GetObjectAccessAsString(),     // ac=\"%.9s\"
		this->GetObjectUsageAsString(),      // us=\"%.9s\"
		(unsigned long) this->count_links(), // cnt=\"%lu\"
		(unsigned long) mObjectSet_Seed      // seed=\"#%lX\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_ObjectSet::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseObjectSet(ev);
		this->MarkShut();
	}
}

void ab_ObjectSet::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:object:set>");

	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];
	ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));

	if ( this->IsOpenObject() )
	{
		ioPrinter->PushDepth(ev); // indent all objects in the list
		const AB_Deque* d = &mObjectSet_Objects;
		AB_Link* link;
		for ( link = AB_Deque_First(d); link; link = AB_Deque_After(d, link) )
		{
			ab_Object* obj = ((ab_ObjectLink*) link)->mObjectLink_Object;
			// We call ObjectAsString() instead of PrintObject() in order to
			// avoid getting caught in cyclic loops of inter-object reference:
			ioPrinter->PutString(ev, obj->ObjectAsString(ev, xmlBuf));
		}
		ioPrinter->PopDepth(ev); // stop indentation
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab:object:set>");
#endif /*AB_CONFIG_PRINT*/
}

ab_ObjectSet::~ab_ObjectSet() /*i*/
{
	AB_ASSERT(mObjectSet_Member==0);
	AB_ASSERT(mObjectSet_NextLink==0);
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_ObjectSet methods

ab_ObjectSet::ab_ObjectSet(const ab_Usage& inUsage) /*i*/
	: ab_Object(inUsage)
,   mObjectSet_Member( 0 )
,   mObjectSet_NextLink( 0 )
{
	AB_Deque* deque = &mObjectSet_Objects;
	AB_Deque_Init(deque);

	// Seed values work better (are slightly less likely to match accidentally
	// when they should not) if initialized randomly, so XOR various addresses.
	// And keep any original bits in the XOR as well, just for good measure.
	mObjectSet_Seed ^= (ab_num) this ^ (ab_num) &deque;
}

void ab_ObjectSet::CloseObjectSet(ab_Env* ev) /*i*/
{
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	ab_Object* obj = mObjectSet_Member;
	if ( mObjectSet_Member )
	{
		mObjectSet_Member = 0;
		obj->ReleaseObject(ev);
	}
	mObjectSet_NextLink = 0;

	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CutAllObjects(ev);
		this->MarkShut();
	}
	else if ( !this->IsObject() ) 
		ev->NewAbookFault(ab_Object_kFaultNotObject);
}

void ab_ObjectSet::TraceSet(ab_Env* ev) /*i*/
{
#ifdef AB_CONFIG_TRACE
	ab_Env_BeginMethod(ev, ab_ObjectSet_kClassName, "TraceSet")
	
	this->TraceObject(ev);

	const AB_Deque* d = &mObjectSet_Objects;
	AB_Link* link;
	for ( link = AB_Deque_First(d); link; link = AB_Deque_After(d, link) )
	{
		ab_Object* obj = ((ab_ObjectLink*) link)->mObjectLink_Object;
		obj->TraceObject(ev);
	}

	ab_Env_EndMethod(ev)
	
#else /*AB_CONFIG_TRACE*/
	AB_USED_PARAMS_1(ev);
#endif /*AB_CONFIG_TRACE*/
}

// ````` iteration methods `````
ab_Object* ab_ObjectSet::FirstMember(ab_Env* ev) /*i*/
{
	ab_Object* outObject = 0;
	ab_Env_BeginMethod(ev, ab_ObjectSet_kClassName, "FirstMember")
	
	if ( this->IsOpenOrClosingObject() )
	{
		ab_Object* obj = mObjectSet_Member;
		if ( obj )
		{
			mObjectSet_Member = 0;
			obj->ReleaseObject(ev);
		}
		mObjectSet_NextLink = 0;

		AB_Deque* deque = &mObjectSet_Objects;
		AB_Link* link = AB_Deque_First(deque);
		if ( link )
		{
			obj = ((ab_ObjectLink*) link)->mObjectLink_Object;
			if ( obj && obj->AcquireObject(ev) )
			{
				mObjectSet_Member = outObject = obj;
				link = AB_Deque_After(deque, link);
				mObjectSet_NextLink = (ab_ObjectLink*) link;
			}
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpenOrClosing);

	ab_Env_EndMethod(ev)
	return outObject;
}

ab_Object* ab_ObjectSet::NextMember(ab_Env* ev) /*i*/
{
	ab_Object* outObject = 0;
	ab_Env_BeginMethod(ev, ab_ObjectSet_kClassName, "NextMember")
	
	if ( this->IsOpenOrClosingObject() )
	{
		ab_Object* obj = mObjectSet_Member;
		if ( obj )
		{
			mObjectSet_Member = 0;
			obj->ReleaseObject(ev);
		}
		
		AB_Link* link = (AB_Link*) mObjectSet_NextLink;
		if ( link )
		{
			obj = mObjectSet_NextLink->mObjectLink_Object;
			if ( obj && obj->AcquireObject(ev) )
			{
				AB_Deque* deque = &mObjectSet_Objects;
				mObjectSet_Member = outObject = obj;
				link = AB_Deque_After(deque, link);
				mObjectSet_NextLink = (ab_ObjectLink*) link;
			}
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpenOrClosing);

	ab_Env_EndMethod(ev)
	return outObject;
}

ab_ref_count ab_ObjectSet::AddObject(ab_Env* ev, ab_Object* ioObject) /*i*/
{
	ab_ref_count outCount = 0;
	ab_Env_BeginMethod(ev, ab_ObjectSet_kClassName, "AddObject")

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ioObject->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	if ( IsOpenObject() )
	{
#ifdef AB_CONFIG_TRACE
		this->TraceSet(ev);
#endif /*AB_CONFIG_TRACE*/

	  	ab_ObjectLink* link = this->get_link(ioObject);
	  	if ( link ) /* have this object as list member? */
	  	{
	  		outCount = ++link->mObjectLink_RefCount; /* bump refcount */
	  	}
	  	else
	  	{
	  		ioObject->AcquireObject(ev);
	  		if ( ev->Good() )
	  		{
		  		link = new ab_ObjectLink(ioObject);
		  		if ( link )
		  		{
					AB_Deque* deque = &mObjectSet_Objects;
		  			outCount = link->mObjectLink_RefCount; /* refcount == 1 */
		  			AB_Deque_AddFirst(deque, link); /* to front of list */
					++mObjectSet_Seed; /* list changed */
		  		}
				else ev->NewAbookFault(ab_ObjectSet_kFaultOutOfMemory);
	  		}
	  	}

#ifdef AB_CONFIG_TRACE
		this->TraceSet(ev);
#endif /*AB_CONFIG_TRACE*/
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace( "<ab:object:set:add:obj $$ obj=\"^%lX\" rc=\"%lu\"/>",
			(long) ioObject, (long) outCount);
#endif /*AB_CONFIG_TRACE*/

	ab_Env_EndMethod(ev)
	return outCount;
}

ab_ref_count ab_ObjectSet::HasObject(ab_Env* ev,  /*i*/
	const ab_Object* inObject) const
{
	ab_ref_count outCount = 0;
	ab_Env_BeginMethod(ev, ab_ObjectSet_kClassName, "HasObject")

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		inObject->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	if ( IsOpenObject() )
	{
	  	ab_ObjectLink* link = this->get_link(inObject);
	  	if ( link ) /* have this object as list member? */
	  	{
	  		outCount = link->mObjectLink_RefCount; /* return refcount */
	  	}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace( "<ab:object:set:has:obj $$ obj=\"^%lX\" rc=\"%lu\"/>",
			(long) inObject, (long) outCount);
#endif /*AB_CONFIG_TRACE*/

	ab_Env_EndMethod(ev)
	return outCount;
}

ab_ref_count ab_ObjectSet::SubObject(ab_Env* ev, ab_Object* ioObject) /*i*/
{
	ab_ref_count outCount = 0;
	ab_Env_BeginMethod(ev, ab_ObjectSet_kClassName, "SubObject")

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ioObject->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	if ( IsOpenObject() )
	{
#ifdef AB_CONFIG_TRACE
		this->TraceSet(ev);
#endif /*AB_CONFIG_TRACE*/

	  	ab_ObjectLink* link = this->get_link(ioObject);
	  	if ( link ) /* have this object as list member? */
	  	{
	  		outCount = --link->mObjectLink_RefCount; /* reduce refcount */
	  		if ( outCount == 0 ) /* last reference removed? */
	  		{
	  			this->cut_link(ev, link);
	  		}
	  	}

#ifdef AB_CONFIG_TRACE
		this->TraceSet(ev);
#endif /*AB_CONFIG_TRACE*/
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace( "<ab:object:set:sub:obj $$ obj=\"^%lX\" rc=\"%lu\"/>",
			(long) ioObject, (long) outCount);
#endif /*AB_CONFIG_TRACE*/

	ab_Env_EndMethod(ev)
	return outCount;
}

ab_ref_count ab_ObjectSet::CutObject(ab_Env* ev, ab_Object* ioObject) /*i*/
{
	ab_ref_count outCount = 0;
	ab_Env_BeginMethod(ev, ab_ObjectSet_kClassName, "CutObject")

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ioObject->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	if ( IsOpenObject() )
	{
#ifdef AB_CONFIG_TRACE
		this->TraceSet(ev);
#endif /*AB_CONFIG_TRACE*/

	  	ab_ObjectLink* link = this->get_link(ioObject);
	  	if ( link ) /* have this object as list member? */
	  	{
	  		outCount = link->mObjectLink_RefCount; /* return refcount */
  			this->cut_link(ev, link);
	  	}

#ifdef AB_CONFIG_TRACE
		this->TraceSet(ev);
#endif /*AB_CONFIG_TRACE*/
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	// do not trace object after cut, since it might now be destroyed
	
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace( "<ab:object:set:cut:obj $$ obj=\"^%lX\" rc=\"%lu\"/>",
			(long) ioObject, (long) outCount);
#endif /*AB_CONFIG_TRACE*/

	ab_Env_EndMethod(ev)
	return outCount;
}

ab_num ab_ObjectSet::CutAllObjects(ab_Env* ev) /*i*/
{
	ab_num outCount = 0;
	ab_Env_BeginMethod(ev, ab_ObjectSet_kClassName, "CutAllObjects")

	if ( this->IsObject() )
	{
		AB_Deque* deque = &mObjectSet_Objects;
		AB_Link* link;
		while ( (link = AB_Deque_RemoveFirst(deque) ) != 0 )
		{
			++outCount;
			++mObjectSet_Seed; // list changed
			ab_ObjectLink* objLink = (ab_ObjectLink*) link;
			
#ifdef AB_CONFIG_TRACE
			if ( ev->DoTrace() )
				ev->Trace( "<ab:object:set:cut:all:obj $$ obj=\"^%lX\" rc=\"%lu\"/>",
					(long) objLink->mObjectLink_Object,
					(long) objLink->mObjectLink_RefCount);
#endif /*AB_CONFIG_TRACE*/
			
			objLink->mObjectLink_Object->ReleaseObject(ev);
			AB_Link_Clear(link);
			delete objLink;
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotObject);

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace( "<ab:object:set:cut:all cnt=\"%lu\"/>",
			(long) outCount);
#endif /*AB_CONFIG_TRACE*/

	ab_Env_EndMethod(ev)
	return outCount;
}

ab_num ab_ObjectSet::CountObjects(ab_Env* ev) const /*i*/
{
	ab_num outCount = 0;
	ab_Env_BeginMethod(ev, ab_ObjectSet_kClassName, "CountObjects")

	if ( this->IsOpenOrClosingObject() )
	{
  		outCount = this->count_links();
	}
	else if ( !this->IsShutObject() )
		ev->NewAbookFault(ab_Object_kFaultNotObject);

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace( "<ab:object:set:count:objs cnt=\"%lu\"/>",
			(long) outCount);
#endif /*AB_CONFIG_TRACE*/

	ab_Env_EndMethod(ev)
	return outCount;
}

ab_Object* ab_ObjectSet::CutAnyObject(ab_Env* ev) /*i*/
{
	ab_Object* outObject = 0;
	ab_Env_BeginMethod(ev, ab_ObjectSet_kClassName, "CutAnyObject")

	if ( IsOpenOrClosingObject() )
	{
		AB_Deque* deque = &mObjectSet_Objects;
		AB_Link* link;
		if ( (link = AB_Deque_RemoveFirst(deque) ) != 0 )
		{
			++mObjectSet_Seed; // list changed
			ab_ObjectLink* objLink = (ab_ObjectLink*) link;
			outObject = objLink->mObjectLink_Object;
			if ( !outObject->ReleaseObject(ev) )
				outObject = 0; // might have just been destroyed
				
			AB_Link_Clear(link);
			delete objLink;
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpenOrClosing);

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() && outObject )
		outObject->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	ab_Env_EndMethod(ev)
	return outObject;
}

ab_bool ab_ObjectSet::DoObjects(ab_Env* ev,  /*i*/
	ab_Object_mAction inAction, void* ioClosure) const
{
	ab_bool outDoObjects = AB_kFalse;
	ab_Env_BeginMethod(ev, ab_ObjectSet_kClassName, "DoObjects")
	
	if ( IsOpenOrClosingObject() )
	{
  		ab_num startSeed = mObjectSet_Seed;
		const AB_Deque* d = &mObjectSet_Objects;
		AB_Link* link;
		for ( link = AB_Deque_First(d); link; link = AB_Deque_After(d, link) )
		{
			if ( mObjectSet_Seed == startSeed )
			{
				ab_Object* obj = ((ab_ObjectLink*) link)->mObjectLink_Object;
				if ( (*inAction)(obj, ev, ioClosure) )
				{
					outDoObjects = AB_kTrue;
					break; /* end loop after action returns true */
				}
			}
			else ev->NewAbookFault(ab_ObjectSet_kFaultIterOutOfSync);
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpenOrClosing);

	ab_Env_EndMethod(ev)
	return outDoObjects;
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// protected ab_Model view list manipulation
	
ab_ObjectLink* ab_ObjectSet::get_link(const ab_Object* ioObject) const /*i*/
{
	const AB_Deque* d = &mObjectSet_Objects;
	AB_Link* link;
	for ( link = AB_Deque_First(d); link; link = AB_Deque_After(d, link) )
	{
		if ( ioObject == ((ab_ObjectLink*) link)->mObjectLink_Object )
			return (ab_ObjectLink*) link; /* return found match */
	}
	return (ab_ObjectLink*) 0;  /* not found */
}

void ab_ObjectSet::cut_link(ab_Env* ev, ab_ObjectLink* ioLink) /*i*/
{
	AB_Link* link = ioLink; /* supertype */
	
	if ( ioLink && ioLink == mObjectSet_NextLink ) // breaking iteration?
	{
		AB_Deque* d = &mObjectSet_Objects;
		mObjectSet_NextLink = (ab_ObjectLink*) AB_Deque_After(d, link);
	}
	
	AB_Link_Remove(link); /* remove ioLink from list */
	AB_Link_Clear(link); /* more likely to cause errors if accessed now */
	ioLink->mObjectLink_Object->ReleaseObject(ev);
	delete ioLink;
	++mObjectSet_Seed; /* watcher list changed */
}

ab_num ab_ObjectSet::count_links() const /*i*/
{
	ab_num outCount = 0;
	const AB_Deque* d = &mObjectSet_Objects;
	AB_Link* link;
	for ( link = AB_Deque_First(d); link; link = AB_Deque_After(d, link) )
		++outCount;
		
	return outCount;
}
