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

/* file: abobject.c 
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

/* ===== ===== ===== ===== ab_Usage ===== ===== ===== ===== */

static ab_Usage ab_Usage_gHeap; // ensure EnsureReadyStaticUsage() is called
const ab_Usage& ab_Usage::kHeap = ab_Usage_gHeap;

static ab_Usage ab_Usage_gStack; // ensure EnsureReadyStaticUsage() is called
const ab_Usage& ab_Usage::kStack = ab_Usage_gStack;

static ab_Usage ab_Usage_gMember; // ensure EnsureReadyStaticUsage() is called
const ab_Usage& ab_Usage::kMember = ab_Usage_gMember;

static ab_Usage ab_Usage_gGlobal; // ensure EnsureReadyStaticUsage() is called
const ab_Usage& ab_Usage::kGlobal = ab_Usage_gGlobal;

static ab_Usage ab_Usage_gGhost; // ensure EnsureReadyStaticUsage() is called
const ab_Usage& ab_Usage::kGhost = ab_Usage_gGhost;

// This must be structured to allow for non-zero values in global variables
// just before static init time.  We can only safely check for whether a
// global has the address of some other global.  Please, do not initialize
// either of the variables below to zero, because this could break when a zero
// is assigned at static init time, but after EnsureReadyStaticUsage() runs.

static ab_u4 ab_Usage_g_static_init_target; // only the address of this matters
static ab_u4* ab_Usage_g_static_init_done; // is the address of target above?

#define ab_Usage_do_static_init() \
	( ab_Usage_g_static_init_done = &ab_Usage_g_static_init_target )

#define ab_Usage_need_static_init() \
	( ab_Usage_g_static_init_done != &ab_Usage_g_static_init_target )

/*static*/
void ab_Usage::EnsureReadyStaticUsage()
{
	if ( ab_Usage_need_static_init() )
	{
		ab_Usage_do_static_init();

		ab_Usage_gHeap.InitUsage(ab_Usage_kHeap);
		ab_Usage_gStack.InitUsage(ab_Usage_kStack);
		ab_Usage_gMember.InitUsage(ab_Usage_kMember);
		ab_Usage_gGlobal.InitUsage(ab_Usage_kGlobal);
		ab_Usage_gGhost.InitUsage(ab_Usage_kGhost);
	}
}

/*static*/
const ab_Usage& ab_Usage::GetHeap()   /*i*/ // kHeap safe at static init time
{
	EnsureReadyStaticUsage();
	return ab_Usage_gHeap;
}

/*static*/
const ab_Usage& ab_Usage::GetStack()  /*i*/ // kStack safe at static init time
{
	EnsureReadyStaticUsage();
	return ab_Usage_gStack;
}

/*static*/
const ab_Usage& ab_Usage::GetMember() /*i*/ // kMember safe at static init time
{
	EnsureReadyStaticUsage();
	return ab_Usage_gMember;
}

/*static*/
const ab_Usage& ab_Usage::GetGlobal() /*i*/ // kGlobal safe at static init time
{
	EnsureReadyStaticUsage();
	return ab_Usage_gGlobal;
}

/*static*/
const ab_Usage& ab_Usage::GetGhost()  /*i*/ // kGhost safe at static init time
{
	EnsureReadyStaticUsage();
	return ab_Usage_gGhost;
}

ab_Usage::ab_Usage()
{
	if ( ab_Usage_need_static_init() )
	{
		ab_Usage::EnsureReadyStaticUsage();
	}
}

ab_Usage::ab_Usage(ab_u4 code)
	: mUsage_Code(code)
{
	if ( ab_Usage_need_static_init() )
	{
		ab_Usage::EnsureReadyStaticUsage();
	}
}

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_Object_kClassName /*i*/ = "ab_Object";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_Object::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
		
	XP_SPRINTF(outXmlBuf,
		"<ab:object:str self=\"^%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                        // self=\"^%lX\"
		(unsigned long) mObject_RefCount,   // rc=\"%lu\"
		this->GetObjectAccessAsString(),    // ac=\"%.9s\"
		this->GetObjectUsageAsString()      // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_Object::CloseObject(ab_Env* ev) /*i*/
{
	AB_USED_PARAMS_1(ev);
	this->MarkShut();
}

void ab_Object::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	char buf[ ab_Object_kXmlBufSize + 2 ];
	ioPrinter->PutString(ev, this->ObjectAsString(ev, buf));
#endif /*AB_CONFIG_PRINT*/
}

ab_Object::~ab_Object() /*i*/ // mObject_Access gets ab_Object_kDead
{
	if ( this->IsHeapObject() && mObject_RefCount )
	{
		this->ObjectPanic("destroyed with non-zero refcount");
	}
	this->MarkDead();
	mObject_Usage = ab_Usage_kGhost;
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// private non-poly ab_Object memory management methods

// It's hard to make the first ab_Env when one is needed for new() ...
// friend class ab_Env; // ... so we let ab_Env allocate without an instance

// void* ab_Object::operator new(size_t inSize, const ab_Usage& inUsage)
// {
// 	AB_USED_PARAMS_1(inUsage);
// 	void* outAddress = ::operator new(inSize);
// 	AB_ASSERT(outAddress);
// 	if ( outAddress )
// 	    XP_MEMSET(outAddress, 0, inSize);
// 
// 	return outAddress;
// }

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_Object methods

void* ab_Object::operator new(size_t inSize)
{
	void* outAddress = ::operator new(inSize);
	if ( outAddress )
	    XP_MEMSET(outAddress, 0, inSize);
	return outAddress;
}

void* ab_Object::operator new(size_t inSize, ab_Env& evRef)
{
	void* outAddress = ::operator new(inSize);
	if ( outAddress )
	    XP_MEMSET(outAddress, 0, inSize);
	else
	{
		ab_Env* ev = &evRef;
		if ( ev )
		{
			ab_Env_BeginMethod(ev, ab_Object_kClassName, "operator_new")
			
			ev->Break("<ab:object:new size=\"%lu\"/>", (long) inSize);
			ev->NewAbookFault(AB_Env_kFaultOutOfMemory);
			
			ab_Env_EndMethod(ev)
		}
	}
	return outAddress;
}

ab_Object::ab_Object(const ab_Usage& inUsage) /*i*/
	: mObject_Access( ab_Object_kOpen ),
	mObject_Usage( inUsage.Code() ),
	mObject_RefCount( 1 )
{
}

void ab_Object::CastAwayConstCloseObject(ab_Env* ev) const /*i*/
{
	ab_Env_BeginMethod(ev, ab_Object_kClassName, "CastAwayConstCloseObject")

	ab_Object* me = (ab_Object*) this; // cast away const
		
	me->CloseObject(ev);
	
	ab_Env_EndMethod(ev)
}

void ab_Object::TraceObject(ab_Env* ev) const /*i*/
{
#ifdef AB_CONFIG_TRACE
	char buf[ ab_Object_kXmlBufSize + 2 ];
	ev->Trace("%.255s", this->ObjectAsString(ev, buf));
#endif /*AB_CONFIG_TRACE*/
}

void ab_Object::BreakObject(ab_Env* ev) const /*i*/
{
#ifdef AB_CONFIG_DEBUG
	char buf[ ab_Object_kXmlBufSize + 2 ];
	ev->Break("%.255s", this->ObjectAsString(ev, buf));
#endif /*AB_CONFIG_DEBUG*/
}

ab_ref_count ab_Object::AcquireObject(ab_Env* ev) /*i*/
{
	ab_ref_count outCount = 0;
	ab_Env_BeginMethod(ev, ab_Object_kClassName, "AcquireObject")

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	if ( this->IsObject() ) // appears to be an ab_Object instance?
	{
		outCount = ++mObject_RefCount;

#ifdef AB_CONFIG_DEBUG
		if ( !this->IsHeapObject() ) // warn about acquiring non-heap obj?
		{
			char buf[ ab_Object_kXmlBufSize + 2 ];
			ev->Break("non-heap %.240s", this->ObjectAsString(ev, buf));
		}
#endif /*AB_CONFIG_DEBUG*/
	}
	else ev->NewAbookFault(ab_Object_kFaultNotObject);

	ab_Env_EndMethod(ev)
	return outCount;
}

ab_ref_count ab_Object::ReleaseObject(ab_Env* ev) /*i*/
{
	if ( ev == this && mObject_RefCount == 1 ) // trace after destroy?
	{
		ab_Env* simple = ab_Env::GetSimpleEnv();
		if ( simple != ev )
			ev = simple;
		else
		{
#ifdef AB_CONFIG_TRACE
			ev->Break("<ab:object:release msg=\"self-ref ev destroy\"/>");
#endif /*AB_CONFIG_TRACE*/
		}
	}
	ab_ref_count outCount = 0;
	ab_Env_BeginMethod(ev, ab_Object_kClassName, "ReleaseObject")
	
#ifdef AB_CONFIG_TRACE
	// note: do *not* trace after we might delete ourselves below :-)
	if ( ev->DoTrace() )
		this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	if ( this->IsObject() ) // appears to be an ab_Object instance?
	{
		if ( mObject_RefCount ) // still nonzero?
			--mObject_RefCount;
		else
			ev->NewAbookFault(ab_Object_kFaultUnderflowRefCount);
		
		outCount = mObject_RefCount;
		
		if ( !outCount ) // need to destroy?
		{
			if ( this->IsOpenObject() ) // need to close?
			{
				this->CloseObject(ev);
			}

			// the *last* operation to perform is deletion of self:
			if ( this->IsHeapObject() )
				delete this;
			else
				ev->NewAbookFault(ab_Object_kFaultNotHeapObject);
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotObject);

	ab_Env_EndMethod(ev)
	return outCount;
}

const char* ab_Object::GetObjectAccessAsString() const
{
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	const char* access = "broken";
	if ( this->IsOpenObject() )
		access = "open";
	else if ( this->IsShutObject() )
		access = "shut";
	else if ( this->IsClosingObject() )
		access = "closing";
	else if ( this->IsDeadObject() )
		access = "dead";
#else
	const char* access = "ac?";
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return access;
}

const char* ab_Object::GetObjectUsageAsString() const
{
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	const char* usage = "broken";
	if ( this->IsHeapObject() )
		usage = "heap";
	else if ( this->IsStackObject() )
		usage = "stack";
	else if ( this->IsMemberObject() )
		usage = "member";
	else if ( this->IsGhostObject() )
		usage = "ghost";
#else
	const char* usage = "us?";
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return usage;
}

void ab_Object::ReportObjectNotOpen(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Object_kClassName, "ReportObjectNotOpen")

	ev->NewAbookFault(ab_Object_kFaultNotOpen);
	
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	ab_Env_EndMethod(ev)
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// panic error handling

#define ab_Object_kPanicEnvStackSize /*i*/ 8

static ab_Env* ab_Object_gPanicEnv[ ab_Object_kPanicEnvStackSize ];
static int ab_Object_gPanicEnvIndex = -1; /* empty stack */

/*static*/
void ab_Object::PushPanicEnv(ab_Env* ev, ab_Env* inPanicEnv) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Object_kClassName, "PushPanicEnv")

	if ( inPanicEnv->IsOpenObject() )
	{
		int index = ab_Object_gPanicEnvIndex;

		if ( ++index < ab_Object_kPanicEnvStackSize )
		{
			if ( inPanicEnv->AcquireObject(ev) )
			{
				ab_Object_gPanicEnv[ index ] = inPanicEnv;
			}
			else
				--index; // put the index back again
		}
		else 
		{
			index = ab_Object_kPanicEnvStackSize - 1;

#ifdef AB_CONFIG_TRACE
			if ( ev->DoTrace() )
				inPanicEnv->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

#ifdef AB_CONFIG_DEBUG
			ev->Break("panic env stack is full");
#endif /*AB_CONFIG_DEBUG*/
		}
		ab_Object_gPanicEnvIndex = index;
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);
	
	ab_Env_EndMethod(ev)
}

/*static*/
ab_Env* ab_Object::PopPanicEnv(ab_Env* ev) /*i*/
{
	ab_Env* outEnv = 0;
	ab_Env_BeginMethod(ev, ab_Object_kClassName, "PopPanicEnv")

	int index = ab_Object_gPanicEnvIndex;
	if ( index >= 0 && index < ab_Object_kPanicEnvStackSize )
	{
		outEnv = ab_Object_gPanicEnv[ index ];
		ab_Object_gPanicEnv[ index ] = 0;
		ab_Object_gPanicEnvIndex = --index;

		outEnv->ReleaseObject(ev);
	}
	else
	{
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
			ev->Trace("panic stack is empty");
#endif /*AB_CONFIG_TRACE*/
	}

	ab_Env_EndMethod(ev)
	return outEnv;
}

/*static*/
ab_Env* ab_Object::TopPanicEnv() /*i*/
{
	int index = ab_Object_gPanicEnvIndex;
	if ( index >= 0 && index < ab_Object_kPanicEnvStackSize )
	{
		return ab_Object_gPanicEnv[ index ];
	}
	return (ab_Env*) 0;
}

/*static*/
int ab_Object::TopPanicEnvDepth()
{
	return ab_Object_gPanicEnvIndex + 1;
}

void ab_Object::ObjectPanic(const char* inMessage) const /*i*/
{
	ab_Env* ev = ab_Object::TopPanicEnv();
	if ( ev && ev->IsOpenObject() )
	{
		ab_Env_BeginMethod(ev, ab_Object_kClassName, "ObjectPanic")
	
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
			this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/
		
#ifdef AB_CONFIG_DEBUG
		ev->Break("%.250s", inMessage);
#endif /*AB_CONFIG_DEBUG*/
		
		ab_Env_EndMethod(ev)
	}
}

void ab_Object::ObjectNotReleasedPanic(const char* inWhere) const /*i*/
{
	ab_Env* ev = ab_Object::TopPanicEnv();
	if ( ev && ev->IsOpenObject() )
	{
		ab_Env_BeginMethod(ev, ab_Object_kClassName, "ObjectNotReleasedPanic")

		char buf[ ab_Object_kXmlBufSize + 2 ];
		(void) this->ObjectAsString(ev, buf);
	
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
			ev->Trace("%.255s", buf);
#endif /*AB_CONFIG_TRACE*/
		
#ifdef AB_CONFIG_DEBUG
		ev->Break("%.32s did not release %.256s", inWhere, buf);
#endif /*AB_CONFIG_DEBUG*/
		
		ab_Env_EndMethod(ev)
	}
}
	
// ````` ````` ````` `````   ````` ````` ````` `````  
// copying is sometimes not allowed

ab_Object::ab_Object(const ab_Object& other, const ab_Usage& inUsage) /*i*/
	: mObject_Access(other.mObject_Access),
	mObject_Usage(inUsage.Code()),
	mObject_RefCount(1)
{
}

ab_Object& ab_Object::operator=(const ab_Object& other) /*i*/
{
	if ( this != &other )
	{
		mObject_Access = other.mObject_Access;
		// do not change mObject_Usage, because it cannot change
		// do not change mObject_RefCount, because references are not changed
	}
	return *this;
}
