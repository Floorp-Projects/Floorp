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

/* file: abenv.cpp
** Most portions derive from public domain IronDoc code and interfaces.
**
** Changes:
** <1> 05Feb1998 first implementation
** <0> 23Dec1997 first interface draft
*/

#ifndef _ABTABLE_
#include "abtable.h"
#endif

#ifndef _ABMODEL_
#include "abmodel.h"
#endif

#include "xp_mem.h"


/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_Env_kClassName /*i*/ = "ab_Env";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* AB_Env_kClassName /*i*/ = "AB_Env";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods
char* ab_Env::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	// copied from public domain IronDoc:
    char debugFlags[ 5 ];
    char printFlags[ 5 ];
    
    debugFlags[ 0 ] = (mEnv_Self.sEnv_DoTrace)? 'T' : 't';
    debugFlags[ 1 ] = (mEnv_Self.sEnv_DoDebug)? 'D' : 'd';
    debugFlags[ 2 ] = (mEnv_Self.sEnv_DoErrBreak)? 'E' : 'e';
    debugFlags[ 3 ] = (mEnv_Self.sEnv_BeParanoid)? 'P' : 'p';
    debugFlags[ 4 ] = '\0';
    
    printFlags[ 0 ] = (mEnv_BeShallow)? 'S' : 's';
    printFlags[ 1 ] = (mEnv_BeComplete)? 'C' : 'c';
    printFlags[ 2 ] = (mEnv_BeConcise)? 'I' : 'i';
    printFlags[ 3 ] = (mEnv_BeVerbose)? 'V' : 'v';
    printFlags[ 4 ] = '\0';
	
	XP_SPRINTF(outXmlBuf,
		"<ab_Env:str me=\"^%lX\" d:t=\"^%lX:^%lX\" d:p=\"%.4s:%.4s\" fc=\"%lu\" sr=\"^%lX\" md=\"%lu\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                                // me=\"^%lX\"
		(long) mEnv_Debugger,                       // d:t=\"^%lX:
		(long) mEnv_Tracer,                         // :^%lX\"
		debugFlags,                                 // d:p=\"%.4s
		printFlags,                                 // :%.4s\"
		(long) mEnv_Self.sEnv_FaultCount,           // fc=\"%lu\"
		(long) mEnv_StackRef,                       // sr=\"^%lX\"
		(long) mEnv_MaxRefDelta,                    // md=\"%lu\"
		(unsigned long) mObject_RefCount,           // rc=\"%lu\"
		this->GetObjectAccessAsString(),            // ac=\"%.9s\"
		this->GetObjectUsageAsString()              // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_Env::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseEnv(ev);
		this->MarkShut();
	}
}

void ab_Env::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab_Env>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PushDepth(ev); // indent all objects in the list
		
		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
		
		if ( mEnv_Debugger )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mEnv_Debugger->PrintObject(ev, ioPrinter);
		}
		
		if ( mEnv_Tracer )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mEnv_Tracer->PrintObject(ev, ioPrinter);
		}
				
		ab_num stackTop = mEnv_StackTop;
		if ( stackTop )
		{
			ioPrinter->Print(ev, "<ab_Env:faults top=\"%lu\">",
				(long) stackTop);
			ioPrinter->PushDepth(ev); // indent all objects in the list
			
			if ( stackTop > ab_Env_kFaultMaxStackTop )
			{
#ifdef AB_CONFIG_DEBUG
				this->BreakObject(ev);
#endif /*AB_CONFIG_DEBUG*/
				stackTop = ab_Env_kFaultMaxStackTop;
			}
			const AB_Fault* bottom = mEnv_FaultStack;
			const AB_Fault* top = mEnv_FaultStack + stackTop;
			
			while ( --top >= bottom )
			{
				ioPrinter->NewlineIndent(ev, /*count*/ 1);
				ioPrinter->PutString(ev, 
					AB_Fault_AsXmlString(top, ev->AsSelf(), xmlBuf));
			}

			ioPrinter->PopDepth(ev); // stop indentation
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			ioPrinter->PutString(ev, "</ab_Env:faults>");
		}

		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab_Env>");
#endif /*AB_CONFIG_PRINT*/
}

ab_Env::~ab_Env()
{
	AB_ASSERT(mEnv_Debugger==0);
	AB_ASSERT(mEnv_Tracer==0);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	ab_Object* obj = mEnv_Debugger;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_Env_kClassName);
	obj = mEnv_Tracer;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_Env_kClassName);
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Env methods

ab_error_count ab_Env::CutAllFaults() /*i*/
{
	ab_error_count outCount = 0;
	ab_Env* ev = this;
	ab_Env_BeginMethod(ev, ab_Env_kClassName, "CutAllFaults")
	
	mEnv_Self.sEnv_FaultCount = 0;
	mEnv_StackTop = 0;
	ab_num stackSize = ab_Env_kFaultStackSlots * sizeof(AB_Fault);
	AB_MEMSET(mEnv_FaultStack, 0, stackSize);
	
	ab_Env_EndMethod(ev)
	return outCount;
}

ab_error_count ab_Env::GetAllFaults(AB_Fault* outVector, /*i*/
            ab_error_count inSize, ab_error_count* outLength) const
{
	ab_error_count outCount = 0;
	const ab_Env* ev = this;
	ab_Env_BeginMethod(ev, ab_Env_kClassName, "GetAllFaults")
	
	outCount = mEnv_StackTop + 1;
	if ( outCount > ab_Env_kFaultStackSlots )
		outCount = ab_Env_kFaultStackSlots;
	
	if ( inSize && outVector ) // any space to receive output?
	{
		if ( inSize > outCount ) // more than enough space?
			inSize = outCount; // copy no more than have on hand
			
		ab_num copySize = inSize * sizeof(AB_Fault);
		AB_MEMCPY(outVector, mEnv_FaultStack, copySize);
		if ( outLength ) // caller wants actual number copied?
			*outLength = inSize;
	}
	
	ab_Env_EndMethod(ev)
	return outCount;
}

#define abEnv_kStackDepthSlop 1024

ab_bool ab_Env::is_stack_depth_okay() const /*i*/
{
	// copied from public domain IronDoc:
	ab_bool outDepthOkay = AB_kTrue; /* default in case check is disabled */
	if ( this->IsOpenObject() )
	{
		ab_num maxDelta = mEnv_MaxRefDelta;
		if ( maxDelta && mEnv_StackRef ) /* stack checking enabled? */
		{      
			long delta = ((ab_u1*) mEnv_StackRef) - ((ab_u1*) &maxDelta);
			if ( delta < 0 ) /* need to convert to absolute value? */
				delta = -delta;

			// add slop so to be more forgiving than abEnv::DefaultCheckStack(),
			// so it is possible to notify of errors on soft stack overflow.
			outDepthOkay = ( delta <= (maxDelta + abEnv_kStackDepthSlop) );
		}
	}
	else
	{
		outDepthOkay = AB_kFalse; /* for a bad env, the stack is too deep */ 
	    this->ObjectPanic("is_stack_depth_okay env not open");
	}
	return outDepthOkay;
}

void ab_Env::error_notify(const AB_Fault* inFault) /*i*/
{
	ab_Env* ev = this;
	// copied from public domain IronDoc:
	if ( this->IsOpenObject() )
	{
		if ( this->is_stack_depth_okay() )
		{
			ab_Env_BeginMethod(ev, ab_Env_kClassName, "error_notify")

			char buf[ AB_Fault_kXmlBufSize + 2 ];
			AB_Fault_AsXmlString(inFault, ev->AsSelf(), buf);

#ifdef AB_CONFIG_DEBUG
			if ( mEnv_Debugger && ev->DoErrorBreak() )
				mEnv_Debugger->BreakString(ev, buf);
#endif /*AB_CONFIG_DEBUG*/

#ifdef AB_CONFIG_TRACE
			if ( mEnv_Tracer && ev->DoTrace() ) 
				mEnv_Tracer->TraceString(ev, buf);
#endif /*AB_CONFIG_TRACE*/

			ab_Env_EndMethod(ev)
		}
	}
	else
	{
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
		AB_Env* cev = this->AsSelf();
		char buf[ AB_Fault_kXmlBufSize + 2 ];
		this->ObjectPanic(AB_Fault_AsXmlString(inFault, cev, buf));
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	}
}

void ab_Env::push_error(ab_error_uid inCode, ab_u4 inSpace) /*i*/
{
	if ( this->IsOpenObject() )
	{
		// note: first fault is written on stack top without bumping pointer
		ab_num top = mEnv_StackTop;
		if ( mEnv_Self.sEnv_FaultCount ) // not the first recorded error?
			++top;
			
		++mEnv_Self.sEnv_FaultCount; // another error has occurred
		
		if ( top < ab_Env_kFaultMaxStackTop ) // room for another error?
		{
			mEnv_StackTop = top;
			AB_Fault* fault = mEnv_FaultStack + top;
			AB_Fault_Init(fault, inCode, inSpace);
		}
		else
		{
#ifdef AB_CONFIG_DEBUG
			ab_Env* ev = this;
			ab_Env_BeginMethod(ev, ab_Env_kClassName, "push_error")
			
			ev->BreakString("dropping one error too many");
			
			ab_Env_EndMethod(ev)
#endif /*AB_CONFIG_DEBUG*/
		}
	}
	else this->ObjectPanic("ev not open");
}

ab_error_uid ab_Env::NewFault(ab_error_uid inCode, ab_u4 inSpace) /*i*/
{
	AB_Fault fault;
	AB_Fault_Init(&fault, inCode, inSpace);
	this->error_notify(&fault);
	this->push_error(inCode, inSpace); // untouched by ErrorNotify()
	
	return inCode;
}

void ab_Env::BreakString(const char* inMessage) /*i*/
{
#ifdef AB_CONFIG_DEBUG
	if ( this->IsOpenObject() )
	{
		ab_Env* ev = this;
		if ( mEnv_Debugger )
			mEnv_Debugger->BreakString(ev, inMessage);
	}
#endif /*AB_CONFIG_DEBUG*/
}

void ab_Env::TraceString(const char* inMessage) /*i*/
{
#ifdef AB_CONFIG_TRACE
	if ( this->IsOpenObject() )
	{
		ab_Env* ev = this;
		ab_Tracer* tracer = mEnv_Tracer;
		if ( mEnv_Tracer && ev->DoTrace() )
			mEnv_Tracer->TraceString(ev, inMessage);
	}
#endif /*AB_CONFIG_TRACE*/
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// public non-poly ab_Env methods

// ````` construction `````  
ab_Env::ab_Env(const ab_Usage& inUsage) /*i*/
	: ab_Object(inUsage),
	mEnv_Debugger( 0 ),
	mEnv_Tracer( 0 )
{
	AB_MEMSET(&mEnv_Self, 0, sizeof(AB_Env));
}

ab_Env::ab_Env(const ab_Usage& inUsage, const ab_Env& other) /*i*/
	: ab_Object(inUsage),
	mEnv_Debugger( 0 ),
	mEnv_Tracer( 0 )
{
	ab_Env* ev = this;

	// copy the flags from other's C struct AB_Env...
	AB_MEMCPY(&mEnv_Self, &other.mEnv_Self, sizeof(AB_Env));
	mEnv_Self.sEnv_FaultCount = 0; // ...but keep error count equal to zero

	ab_Tracer* tracer = other.mEnv_Tracer;
	if ( tracer && tracer->AcquireObject(ev) )
		mEnv_Tracer = tracer;
	
	ab_Debugger* debugger = other.mEnv_Debugger;
	if ( ev->Good() && debugger && debugger->AcquireObject(ev) )
		mEnv_Debugger = debugger;
}

ab_Env::ab_Env(const ab_Usage& inUsage, ab_Debugger* ioDebugger, /*i*/
	ab_Tracer* ioTracer)
	: ab_Object(inUsage),
	mEnv_Debugger( 0 ),
	mEnv_Tracer( 0 )
{
	ab_Env* ev = this;

	AB_MEMSET(&mEnv_Self, 0, sizeof(AB_Env)); // zero embedded C struct

	if ( ioTracer && ioTracer->AcquireObject(ev) )
		mEnv_Tracer = ioTracer;
	
	if ( ev->Good() && ioDebugger && ioDebugger->AcquireObject(ev) )
		mEnv_Debugger = ioDebugger;
}

// ````` standard logging env `````  

static ab_Env* ab_Env_g_simple_env = 0; // for ab_Env::GetSimpleEnv() use

/*static*/
ab_Env* ab_Env::GetSimpleEnv() /*i*/
	// This env just has a debugger instance installed.  It is used
	// when instantiating the standard session log file objects.  This
	// env is installed as the top panic env when first instantiated.
	// (Please, never close this env instance.)
{
	ab_Env* outEnv = ab_Env_g_simple_env;
	
	if ( !outEnv )
	{
		ab_Env* nullEnv = 0; // note operator new() accounts
		outEnv = new(*nullEnv) ab_Env(ab_Usage::GetHeap());
	}

	if ( outEnv && outEnv->Bad() )
		outEnv->CutAllFaults();
	
	if ( outEnv && !outEnv->mEnv_Debugger )
	{
		ab_Debugger* debugger = new(*outEnv) ab_Debugger(ab_Usage::GetHeap());
		if ( debugger )
		{
			if ( outEnv->Good() )
			{
				outEnv->mEnv_Debugger = debugger;
#ifdef AB_CONFIG_DEBUG
				outEnv->mEnv_Self.sEnv_DoDebug = AB_kTrue;
				outEnv->mEnv_Self.sEnv_DoErrBreak = AB_kTrue;
				outEnv->mEnv_Self.sEnv_BeParanoid = AB_kTrue;
#endif /*AB_CONFIG_DEBUG*/
			}
			else
				debugger->ReleaseObject(outEnv);
		}
	}
	if ( outEnv && !ab_Env_g_simple_env )
	{
		ab_Env_g_simple_env = outEnv;
		outEnv->AcquireObject(outEnv);

		ab_Object::PushPanicEnv(outEnv, outEnv);
	}
	return outEnv;
}

static ab_Env* ab_Env_g_log_file_env = 0; // for ab_Env::GetLogFileEnv() use
	
/*static*/
ab_Env* ab_Env::GetLogFileEnv() /*i*/
	// standard session log env (uses ab_StdioFile::GetLogStdioFile()).
	// This uses standard session debugger, tracer, printer, etc.
	// (Please, never close this env instance.)
{
	ab_Env* outEnv = ab_Env_g_log_file_env;
	if ( !outEnv )
	{
		ab_Env* simpleEv = ab_Env::GetSimpleEnv();
		if ( simpleEv )
		{
			ab_Debugger* d = ab_Debugger::GetLogFileDebugger(simpleEv);
			if ( d )
			{
				ab_Tracer* t = ab_FileTracer::GetLogFileTracer(simpleEv);
				if ( t )
				{
					outEnv = new(*simpleEv) ab_Env(ab_Usage::GetHeap(), d, t);
					if ( outEnv )
						outEnv->AcquireObject(outEnv);
				}
			}
		}
		if ( outEnv  )
		{
			int localVar = 0; // lives somewhere on stack
			outEnv->SetStackControl(&localVar, ab_Env_kDefaultMaxRefDelta);
			
#if AB_CONFIG_TRACE_andLOGGING
			outEnv->mEnv_Self.sEnv_DoTrace = AB_kTrue;
#endif /*AB_CONFIG_TRACE_andLOGGING*/

#ifdef AB_CONFIG_DEBUG
			outEnv->mEnv_Self.sEnv_DoDebug = AB_kTrue;
			outEnv->mEnv_Self.sEnv_DoErrBreak = AB_kTrue;
			outEnv->mEnv_Self.sEnv_BeParanoid = AB_kTrue;
#endif /*AB_CONFIG_DEBUG*/

			ab_Env_g_log_file_env = outEnv;
			outEnv->AcquireObject(outEnv);
			
			ab_Object::PushPanicEnv(outEnv, outEnv);
			
#if AB_CONFIG_TRACE_andLOGGING
			if ( outEnv->DoTrace() )
				outEnv->TraceObject(outEnv);
#endif /*AB_CONFIG_TRACE_andLOGGING*/

#ifdef AB_CONFIG_PRINT
			ab_Printer* printer = ab_FilePrinter::GetLogFilePrinter(outEnv);
			if ( printer )
			{
				outEnv->PrintObject(outEnv, printer);
				printer->Flush(outEnv);
			}
#ifdef AB_CONFIG_DEBUG
			else outEnv->Break("no log printer");
#endif /*AB_CONFIG_DEBUG*/

#endif /*AB_CONFIG_PRINT*/
		}
	}
	
	return outEnv;
}

// ````` ````` begin class to statically make log class instances ````` `````
#if AB_CONFIG_TRACE_orDEBUG_orPRINT && defined(USE_STATIC_TWIDDLER)
	class ab_EnvStaticTwiddler {
	public:
		ab_Env* mEnvStaticTwiddler_LogEnv;
	public:
		ab_EnvStaticTwiddler();
		~ab_EnvStaticTwiddler();
	};

	ab_EnvStaticTwiddler::ab_EnvStaticTwiddler()
	{
		mEnvStaticTwiddler_LogEnv = ab_Env::GetLogFileEnv();
		if ( mEnvStaticTwiddler_LogEnv )
		{
			ab_Env* simple = ab_Env::GetSimpleEnv();
			mEnvStaticTwiddler_LogEnv->AcquireObject(simple);
		}
	}

	ab_EnvStaticTwiddler::~ab_EnvStaticTwiddler()
	{
		mEnvStaticTwiddler_LogEnv = 0;

		ab_Env* ev = ab_Object::TopPanicEnv();
		if ( ev && ev->IsOpenObject() )
		{
			ab_Env_BeginMethod(ev, "ab_EnvStaticTwiddler", "~ab_EnvStaticTwiddler")
			
#ifdef AB_CONFIG_TRACE
			if ( ev->DoTrace() )
				ev->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/
			
			ab_Env_EndMethod(ev)

			ab_Printer* printer = ab_FilePrinter::GetLogFilePrinter(ev);
			if ( printer )
			{
#ifdef AB_CONFIG_TRACE
				if ( ev->DoTrace() )
					printer->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/
				printer->Flush(ev);
			}
			ab_Tracer* tracer = ab_FileTracer::GetLogFileTracer(ev);
			if ( tracer )
			{
#ifdef AB_CONFIG_TRACE
				if ( ev->DoTrace() )
					tracer->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/
				tracer->Flush(ev);
			}
		}
	}

	ab_EnvStaticTwiddler ab_EnvStaticTwiddler_gStaticInstance;
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT && use-static-twiddler */
// ````` ````` end class to statically make log class instances ````` `````


// ````` destruction `````  
void ab_Env::CloseEnv(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Env_kClassName, "CloseEnv")
	
	ab_Object*obj = mEnv_Debugger;
	if ( obj )
	{
		mEnv_Debugger = 0;
		obj->ReleaseObject(ev);
	}
	
	obj = mEnv_Tracer;
	if ( obj )
	{
		mEnv_Tracer = 0;
		obj->ReleaseObject(ev);
	}
	
	ab_Env_EndMethod(ev)
}


// ````` tagged memory management `````  
void* ab_Env::HeapSafeTagAlloc(ab_num inSize) /*i*/
{
	void* outAddress = 0;
	ab_Env* ev = this;
	ab_Env_BeginMethod(ev, ab_Env_kClassName, "HeapSafeTagAlloc")

	if ( !inSize )
		inSize = 1;
				
	ab_u4* tag = (ab_u4*) XP_ALLOC(inSize + sizeof(ab_u4));
	if ( tag )
	{
		*tag = ab_Env_kHeapSafeTag; // install magic tag for later use
		outAddress = tag + 1; // slot after tag slot
	}
	else
		ev->NewAbookFault(AB_Table_kFaultOutOfMemory);
	
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace("<ab_Env::HeapSafeTagAlloc at=\"^%lX\" sz=\"%lu\"/>", 
			(long) outAddress, (long) inSize);
#endif /*AB_CONFIG_TRACE*/
	
	ab_Env_EndMethod(ev)
	return outAddress;
}

void ab_Env::HeapSafeTagFree(void* ioAddress) /*i*/
{
	ab_Env* ev = this;
	ab_Env_BeginMethod(ev, ab_Env_kClassName, "HeapSafeTagFree")
	
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace("<ab_Env::HeapSafeTagFree at=\"^%lX\" />", 
			(long) ioAddress);
#endif /*AB_CONFIG_TRACE*/
	
	if ( ioAddress )
	{
		ab_u4* tag = ((ab_u4*) ioAddress) - 1; // u4 slot before address
		if ( *tag == ab_Env_kHeapSafeTag ) // has expected leading tag?
		{
			XP_FREE(tag); // only free when the tag is correct
		}
		else
			ev->Break("<ab_Env::HeapSafeTagFree tag=\"%.4s\"/>", (char*) tag);
	}
	
	ab_Env_EndMethod(ev)
}

/*static*/
ab_bool ab_Env::GoodHeapSafeTag(void* ioAddress)
{
	if ( ioAddress )
	{
		// take care not to act upon an illegally aligned address for a u4:
		ab_u4 expected = ab_Env_kHeapSafeTag;
		ab_u1* want = (ab_u1*) &expected;
		ab_u1* actual = (ab_u1*) (((ab_u4*) ioAddress) - 1);
		return ( want[ 0 ] == actual[ 0 ] &&
		         want[ 1 ] == actual[ 1 ] &&
		         want[ 2 ] == actual[ 2 ] &&
		         want[ 3 ] == actual[ 3 ] );
	}
	return AB_kFalse;
}

// ````` memory management `````  
void* ab_Env::HeapAlloc(ab_num inSize) /*i*/
{
	void* outAddress = 0;
	ab_Env* ev = this;
	ab_Env_BeginMethod(ev, ab_Env_kClassName, "HeapAlloc")

	if ( !inSize )
		inSize = 1;
		
	outAddress = XP_ALLOC(inSize);
	if ( !outAddress )
		ev->NewAbookFault(AB_Table_kFaultOutOfMemory);
	
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace("<ab_Env::HeapAlloc at=\"^%lX\" sz=\"%lu\"/>", 
			(long) outAddress, (long) inSize);
#endif /*AB_CONFIG_TRACE*/
	
	ab_Env_EndMethod(ev)
	return outAddress;
}

void ab_Env::HeapFree(void* ioAddress) /*i*/
{
	ab_Env* ev = this;
	ab_Env_BeginMethod(ev, ab_Env_kClassName, "HeapFree")
	
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace("<ab_Env::HeapFree at=\"^%lX\" />", 
			(long) ioAddress);
#endif /*AB_CONFIG_TRACE*/
	
	if ( ioAddress )
		XP_FREE(ioAddress);
	
	ab_Env_EndMethod(ev)
}

char* ab_Env::CopyString(const char* inStringToCopy) /*i*/
{
	char* outString = 0;
	ab_Env* ev = this;
	ab_Env_BeginMethod(ev, ab_Env_kClassName, "CopyString")
	
	if ( !inStringToCopy )
		inStringToCopy = "";

  	ab_num length = XP_STRLEN(inStringToCopy);
		
  	outString = (char*) XP_ALLOC(length + 1);
  	if ( outString )
	{
	   	XP_STRCPY(outString, inStringToCopy);
	}
	else ev->NewAbookFault(AB_Table_kFaultOutOfMemory);

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace(
			"<ab_Env::CopyString at=\"^%lX\" in=\"%.128s\" len=\"%lu\"/>", 
			(long) outString, inStringToCopy, (long) length);
#endif /*AB_CONFIG_TRACE*/

	ab_Env_EndMethod(ev)
	return outString;
}

void ab_Env::FreeString(char* ioStringToFree) /*i*/
{
	ab_Env* ev = this;
	ab_Env_BeginMethod(ev, ab_Env_kClassName, "FreeString")
	
	if ( ioStringToFree )
	{
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
			ev->Trace("<ab_Env::FreeString at=\"^%lX\" io=\"%.128s\"/>", 
				(long) ioStringToFree, ioStringToFree);
#endif /*AB_CONFIG_TRACE*/

		XP_FREE(ioStringToFree);
	}

	ab_Env_EndMethod(ev)
}

// ````` fault creation covers `````  

void ab_Env::NewAbookFault(ab_error_uid faultCode) /*i*/
{
	this->NewFault(faultCode, /*space*/ AB_Fault_kAbookSpace);
}
// ````` fault access `````  

ab_error_count ab_Env::GetAllErrors(ab_error_uid* outVector, /*i*/
      ab_error_count inSize, ab_error_count* outLength)
{
	ab_num outCount = mEnv_Self.sEnv_FaultCount;
	if ( outCount > ab_Env_kFaultStackSlots )
		outCount = ab_Env_kFaultStackSlots;
		
	ab_num copyCount = outCount;
	if ( copyCount > inSize )
		copyCount = inSize;
	
	if ( copyCount && outVector )
	{
		AB_MEMCPY(outVector, mEnv_FaultStack, copyCount * sizeof(AB_Fault));
	}
	if ( outLength )
		*outLength = copyCount;
	return outCount;
}

// ````` debugging covers `````  
void ab_Env::Break(const char* inFormat, ...) /*i*/
{
#ifdef AB_CONFIG_DEBUG
	// copied from public domain IronDoc
	ab_Env* ev = this;
	if ( this->IsOpenObject() )
	{
		ab_Debugger* debugger = mEnv_Debugger;
		if ( debugger )
		{
			char buf[ AB_Env_kFormatBufferSize ];
			va_list args;
			va_start(args,inFormat);
			PR_vsnprintf(buf, AB_Env_kFormatBufferSize, inFormat, args);
			va_end(args);
			    
			debugger->BreakString(ev, buf);
		}
	}

#endif /*AB_CONFIG_DEBUG*/
}

// ````` tracing covers ````` 

#ifdef AB_CONFIG_TRACE
	void ab_Env::Trace(const char* inFormat, ...) /*i*/
	{
		// copied from public domain IronDoc
		ab_Env* ev = this;
		if ( this->IsOpenObject() )
		{
			ab_Tracer* tracer = mEnv_Tracer;
			if ( tracer && ev->DoTrace() )
			{
				char buf[ AB_Env_kFormatBufferSize ];
				va_list args;
				va_start(args,inFormat);
				PR_vsnprintf(buf, AB_Env_kFormatBufferSize, inFormat, args);
				va_end(args);
				    
				tracer->TraceString(ev, buf);
			}
		}
	}
#endif /*AB_CONFIG_TRACE*/

void ab_Env::TraceBeginMethod(const char* inClass,
	const char* inMethod) const /*i*/
{
	ab_Env* ev = (ab_Env*) this; // cast away const

	// copied from public domain IronDoc
	if ( this->IsOpenObject() )
	{
		ab_Tracer* tracer = mEnv_Tracer;
		if ( tracer && ev->DoTrace() )
		{
			tracer->BeginMethod(ev, inClass, inMethod);
		}
	}
}

void ab_Env::TraceEndMethod() const /*i*/
{
	ab_Env* ev = (ab_Env*) this; // cast away const

	// copied from public domain IronDoc
	if ( this->IsOpenObject() )
	{
		ab_Tracer* tracer = mEnv_Tracer;
		if ( tracer && ev->DoTrace() )
		{
			tracer->EndMethod(ev);
		}
	}
}
 
// ````` member access `````  
void ab_Env::ChangeDebugger(ab_Debugger* ioDebugger) /*i*/
{
	ab_Env* ev = this;
	ab_Env_BeginMethod(ev, ab_Env_kClassName, "ChangeDebugger")
	
	if ( ev->Good() && ev->IsOpenObject() )
	{
		if ( ioDebugger )
			ioDebugger->AcquireObject(ev);
		if ( ev->Good() )
		{
			if ( mEnv_Debugger )
				mEnv_Debugger->ReleaseObject(ev);
			mEnv_Debugger = ioDebugger;
		}
	}
	
	ab_Env_EndMethod(ev)
}

void ab_Env::ChangeTracer(ab_Tracer* ioTracer) /*i*/
{
	ab_Env* ev = this;
	ab_Env_BeginMethod(ev, ab_Env_kClassName, "ChangeTracer")
	
	if ( ev->Good() && this->IsOpenObject() )
	{
		if ( ioTracer )
			ioTracer->AcquireObject(ev);
		if ( ev->Good() )
		{
			if ( mEnv_Tracer )
				mEnv_Tracer->ReleaseObject(ev);
			mEnv_Tracer = ioTracer;
		}
	}
	
	ab_Env_EndMethod(ev)
}

/* ----- ----- ----- ----- C Env ----- ----- ----- ----- */

/* ----- ----- creation / ref counting ----- ----- */

AB_API_IMPL(AB_Env*) /* abenv.cpp */
AB_Env_New() /*i*/
{
	AB_Env* outEnv = 0;
#ifdef AB_CONFIG_LOGGING
	ab_Env* ev = ab_Env::GetLogFileEnv();
#else /*AB_CONFIG_LOGGING*/
	ab_Env* ev = ab_Env::GetSimpleEnv();
#endif /*AB_CONFIG_LOGGING*/
	if ( ev )
	{
		ab_Env_BeginMethod(ev, AB_Env_kClassName, "New")

		ab_Env* copyEnv = new(*ev) ab_Env(ab_Usage::GetHeap(), *ev);
		if ( copyEnv )
		{
			outEnv = copyEnv->AsSelf(); 
			
#ifdef AB_CONFIG_TRACE
			if ( ev->DoTrace() )
				copyEnv->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/
		}
		ab_Env_EndMethod(ev)
	}
	return outEnv;
}

AB_API_IMPL(AB_Env*) /* abenv.cpp */
AB_Env_GetLogFileEnv() /*i*/
{
	AB_Env* outEnv = 0;
	ab_Env* ev = ab_Env::GetLogFileEnv();
	if ( ev )
	{
		outEnv = ev->AsSelf(); 
	}
	return outEnv;
}

AB_API_IMPL(ab_ref_count) /* abenv.cpp */
AB_Env_Acquire(AB_Env* self) /*i*/
{
	ab_Env* ev = ab_Env::AsThis(self);
	return ev->AcquireObject(ev);
}

AB_API_IMPL(ab_ref_count) /* abenv.cpp */
AB_Env_Release(AB_Env* self) /*i*/
{
	ab_Env* ev = ab_Env::AsThis(self);
	return ev->ReleaseObject(ev);
}

/* ----- ----- error access ----- ----- */

AB_API_IMPL(ab_error_count) /* abenv.cpp */
AB_Env_ForgetErrors(AB_Env* self) /*i*/
	/*- Discard all error information. -*/
{
	ab_Env* ev = ab_Env::AsThis(self);
	return ev->CutAllFaults();
}

AB_API_IMPL(ab_error_count) /* abenv.cpp */
AB_Env_ErrorCount(const AB_Env* self) /*i*/
	/*- Number of errors since last forget/reset. -*/
{
	AB_Env* mutableSelf = (AB_Env*) self; // cast away const
	ab_Env* ev = ab_Env::AsThis(mutableSelf);
	return ev->ErrorCount();
}

AB_API_IMPL(ab_error_uid) /* abenv.cpp */
AB_Env_GetError(const AB_Env* self) /*i*/
	/*- Last error since forget/reset, otherwise zero. -*/
{
	AB_Env* mutableSelf = (AB_Env*) self; // cast away const
	ab_Env* ev = ab_Env::AsThis(mutableSelf);
	return ev->GetError();
}

AB_API_IMPL(ab_error_count) /* abenv.cpp */
AB_Env_GetAllErrors(const AB_Env* self, ab_error_uid* outVector, /*i*/
	ab_error_count inSize, ab_error_count* outLength)
	/*- All errors (up to inSize) since forget/reset. -*/
{
	AB_Env* mutableSelf = (AB_Env*) self; // cast away const
	ab_Env* ev = ab_Env::AsThis(mutableSelf);
	return ev->GetAllErrors(outVector, inSize, outLength);
}


AB_API_IMPL(ab_error_uid) /* abenv.cpp */
AB_Env_NewFault(AB_Env* self, ab_error_uid inFaultCode, /*i*/
	ab_u4 inFaultSpace)
{
	ab_Env* ev = ab_Env::AsThis(self);
	return ev->NewFault(inFaultCode, inFaultSpace);
}

/* ----- ----- static dispatching methods ----- ----- */

AB_API_IMPL(ab_error_uid) /* abenv.cpp */
AB_Env_NewAbookFault(AB_Env* self, ab_error_uid inFaultCode) /*i*/
{
	ab_Env* ev = ab_Env::AsThis(self);
	ev->NewAbookFault(inFaultCode);
	return inFaultCode;
}

AB_API_IMPL(void) /* abenv.cpp */
AB_Env_Break(AB_Env* self, const char* inFormat, ...) /*i*/
{
#ifdef AB_CONFIG_DEBUG
	ab_Env* ev = ab_Env::AsThis(self);
	ab_Env_BeginMethod(ev, AB_Env_kClassName, "Break")

	if ( ev->IsOpenObject() )
	{
		char buf[ AB_Env_kFormatBufferSize ];
		va_list args;
		va_start(args,inFormat);
		PR_vsnprintf(buf, AB_Env_kFormatBufferSize, inFormat, args);
		va_end(args);
		    
		ev->BreakString(buf);
	}
	ab_Env_EndMethod(ev)
#endif /*AB_CONFIG_DEBUG*/
}
 
AB_API_IMPL(void) /* abenv.cpp */
AB_Env_Trace(AB_Env* self, const char* inFormat, ...) /*i*/
{
#ifdef AB_CONFIG_TRACE
	ab_Env* ev = ab_Env::AsThis(self);
	ab_Env_BeginMethod(ev, AB_Env_kClassName, "Trace")

	if ( ev->IsOpenObject() )
	{
		char buf[ AB_Env_kFormatBufferSize ];
		va_list args;
		va_start(args,inFormat);
		PR_vsnprintf(buf, AB_Env_kFormatBufferSize, inFormat, args);
		va_end(args);
		    
		ev->TraceString(buf);
	}
	ab_Env_EndMethod(ev)
#endif /*AB_CONFIG_TRACE*/
}

AB_API_IMPL(void) /* abenv.cpp */
AB_Env_TraceBeginMethod(const AB_Env* self, const char* inClass, /*i*/
	const char* inMethod)
{
	AB_Env* mutableSelf = (AB_Env*) self; // cast away const
	ab_Env* ev = ab_Env::AsThis(mutableSelf);
	ev->TraceBeginMethod(inClass, inMethod);
}

AB_API_IMPL(void) /* abenv.cpp */
AB_Env_TraceEndMethod(const AB_Env* self) /*i*/
{
	AB_Env* mutableSelf = (AB_Env*) self; // cast away const
	ab_Env* ev = ab_Env::AsThis(mutableSelf);
	ev->TraceEndMethod();
}


