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

/* file: abdebug.cpp
** Most portions closely derive from public domain IronDoc code and interfaces.
**
** Changes:
** <1> 06Feb1998 first implementation
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
	static const char* ab_Debugger_kClassName /*i*/ = "ab_Debugger";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


/* ----- ----- ----- ----- XP_MAC ----- ----- ----- ----- */
#ifdef XP_MAC

#include <Types.h>
  
  // copied almost verbatim from the IronDoc debugger sources:
  AB_FILE_IMPL(void)
  ab_Debugger_mac_break_string(register const char* inMessage) /*i*/
  {
    Str255 pascalString; // to hold Pascal string version of inMessage
    ab_u4 length = XP_STRLEN(inMessage);
    
    // if longer than maximum 255 bytes, just copy 255 bytes worth
    pascalString[ 0 ] = (length > 255)? 255 : length;
    if ( length ) // anything to copy? */
    {
      register ab_u1* p = ((ab_u1*) &pascalString) + 1; // after length byte
      register ab_u1* end = p + length; // one past last byte to copy
      while ( p < end ) // more bytes to copy?
      {
        register int c = (ab_u1) *inMessage++;
        if ( c == ';' ) // c is the MacsBug ';' metacharacter?
          c = ':'; // change to ':', rendering harmless in a MacsBug context
        *p++ = (ab_u1) c;
      }
    }
    
    DebugStr(pascalString); /* call Mac debugger entry point */
  }
#endif /*XP_MAC*/
/* ----- ----- ----- ----- XP_MAC ----- ----- ----- ----- */


// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Debugger methods

void ab_Debugger::BreakString(ab_Env* ev, const char* inMessage) /*i*/
{   
#ifdef AB_CONFIG_TRACE
    if ( ev->DoTrace() )
      ev->Trace("%.256s", inMessage);
#endif /*AB_CONFIG_TRACE*/

	if ( this->IsOpenObject() )
	{
	    ab_bool needAssert = AB_kTrue;

#if defined(XP_MAC) && defined(DEBUG)
		needAssert = AB_kFalse; // debugstr() instead
		ab_Debugger_mac_break_string(inMessage);
#endif /*XP_MAC*/

#ifdef XP_WIN
		// needAssert = AB_kFalse; // native break instead
		// asm { int 3 }
#endif /*XP_WIN*/

		if ( needAssert )
			AB_ASSERT( 0 );
	}
#ifdef AB_CONFIG_DEBUG
	// presumably this debugger is inside ev, so breaking is useless:
	else this->ObjectPanic("ab_Debugger not open");
#endif /*AB_CONFIG_DEBUG*/
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_Debugger::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
		
	XP_SPRINTF(outXmlBuf,
		"<ab:debugger:str self=\"^%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
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

void ab_Debugger::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseDebugger(ev);
		this->MarkShut();
	}
}

void ab_Debugger::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	char buf[ ab_Object_kXmlBufSize + 2 ];
	ioPrinter->PutString(ev, this->ObjectAsString(ev, buf));
#endif /*AB_CONFIG_PRINT*/
}

ab_Debugger::~ab_Debugger() /*i*/
{	
	// nothing
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_Debugger methods

ab_Debugger::ab_Debugger(const ab_Usage& inUsage) /*i*/
	: ab_Object(inUsage)
{
}

void ab_Debugger::Break(ab_Env* ev, const char* inFormat, ...) /*i*/
{
#ifdef AB_CONFIG_DEBUG
	if ( this->IsOpenObject() )
	{
		char buf[ AB_Env_kFormatBufferSize ];
		va_list args;
		va_start(args,inFormat);
		PR_vsnprintf(buf, AB_Env_kFormatBufferSize, inFormat, args);
		va_end(args);
		    
		this->BreakString(ev, buf);
	}
	// presumably this debugger is inside ev, so breaking is useless:
	else this->ObjectPanic("ab_Debugger not open");
#endif /*AB_CONFIG_DEBUG*/
}

void ab_Debugger::CloseDebugger(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Debugger_kClassName, "CloseDebugger")

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	ab_Env_EndMethod(ev)
}

static ab_Debugger* ab_Debugger_g_log_debugger = 0; // GetLogFileDebugger() use

/*static*/
ab_Debugger* ab_Debugger::GetLogFileDebugger(ab_Env* ev)
	// standard session debugger (uses ab_StdioFile::GetLogStdioFile()).
	// This debugger instance is used by ab_Env::GetLogFileEnv().
	// (Please, never close this debugger instance.)
{
	ab_Debugger* outDebugger = 0;
	ab_Env_BeginMethod(ev, ab_Debugger_kClassName, "GetLogFileDebugger")

	outDebugger = ab_Debugger_g_log_debugger;
	if ( !outDebugger )
	{
		outDebugger =  new(*ev) ab_Debugger(ab_Usage::kHeap);
		if ( outDebugger )
		{
			ab_Debugger_g_log_debugger = outDebugger;
			outDebugger->AcquireObject(ev);
		}
	}
	
	if ( outDebugger )
	{
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
			outDebugger->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/
	}
	ab_Env_EndMethod(ev)
	return outDebugger;
}
