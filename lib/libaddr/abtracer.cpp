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

/* file: abtrace.cpp
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
	static const char* ab_Tracer_kClassName /*i*/ = "ab_Tracer";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_FileTracer_kClassName /*i*/ = "ab_FileTracer";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


/* ===== ===== ===== ===== ab_Tracer ===== ===== ===== ===== */

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

ab_Tracer::~ab_Tracer() /*i*/
{
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_Tracer methods

void ab_Tracer::Trace(ab_Env* ev, const char* inFormat, ...) /*i*/
{
#ifdef AB_CONFIG_TRACE
	if ( this->IsOpenObject() )
	{
		char buf[ AB_Env_kFormatBufferSize ];
		va_list args;
		va_start(args,inFormat);
		PR_vsnprintf(buf, AB_Env_kFormatBufferSize, inFormat, args);
		va_end(args);
		    
		this->TraceString(ev, buf);
	}
#ifdef AB_CONFIG_DEBUG
	// presumably this tracer is inside ev, so breaking might recurse:
	else this->ObjectPanic("ab_Tracer not open");
#endif /*AB_CONFIG_DEBUG*/

#endif /*AB_CONFIG_TRACE*/
}

ab_Tracer::ab_Tracer(const ab_Usage& inUsage) /*i*/
	: ab_Object(inUsage),
	mTracer_MethodDepth( 0 )
{
}

/* ===== ===== ===== ===== ab_FileTracer ===== ===== ===== ===== */

// ````` ````` ````` `````   ````` ````` ````` `````  
// protected non-poly ab_FileTracer methods


#define ab_FileTracer_kInsetSize /*i*/ 8

#define ab_FileTracer_kBlanksLength /*i*/ 32 /* static blank string length */

void ab_FileTracer::trace_indent(ab_Env* ev) /*i*/
{
	// copied almost verbatim from IronDoc sources:
	ab_File* file = mFileTracer_File;
	if ( file && file->IsOpenObject() )
	{
      static const char* kBlanks = "<indent . . . . . . . . . . . . . . . . . ";
      /* comment to count sections: 12345678 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 */
      const char* inset = kBlanks + ab_FileTracer_kInsetSize; /* skip tag */
      ab_num count = 0; /* the count of times we write something */
      
      ab_num remaining = mTracer_MethodDepth * 2; /* 2 blanks per indent */
      if ( remaining > 1024 ) /* more than some reasonable upper bound? */
        remaining = 1024;
        
      if ( remaining ) /* is there any indentation to write? */
      {
        /* keep looping even if an error has occurred */
        while ( remaining ) /* more blanks to write? */
        {
          ab_num quantum = remaining;
          if ( quantum > ab_FileTracer_kBlanksLength )
            quantum = ab_FileTracer_kBlanksLength;
            
          if ( count ) /* have we already written at least once? */
            file->Write(ev, inset, quantum);
          else
            file->Write(ev, kBlanks, quantum + ab_FileTracer_kInsetSize);
           
          ++count; /* only write the leading indent tag a single time */
          remaining -= quantum;
        }
        file->Write(ev, "/> ", 3); /* end the indent tag */
      }
      else /* we want to write at least the basic indent tag */
      {
        file->Write(ev, "<indent /> ", 11); /* empty tag */
        /* count length: 12345678901 */
      }
	}
}


// ````` ````` ````` ````` tags ````` ````` ````` `````  

/* :::: ruler for string lengths:- 0123456789012 */
static const char* ab_kTraceTag  = "<ab:trace>"; // length 10
#define ab_kTraceTagSize 10

#ifdef XP_MAC
   static const char* ab_kTraceEndTag  = "</ab:trace>\015";
#  define ab_kTraceEndTagSize 12
#else
#  if defined(XP_WIN) || defined(XP_OS2)
     static const char* ab_kTraceEndTag  = "</ab:trace>\015\012";
#    define ab_kTraceEndTagSize 13
#  else
#    ifdef XP_UNIX
       static const char* ab_kTraceEndTag  = "</ab:trace>\012";
#      define ab_kTraceEndTagSize 12
#    endif /* XP_UNIX */
#  endif /* XP_WIN */
#endif /* XP_MAC */

// ````` ````` ````` ````` tags ````` ````` ````` `````  

void ab_FileTracer::trace_string(ab_Env* ev, const char* inMessage, /*i*/
	 ab_bool inXmlWrap)
{
#ifdef AB_CONFIG_TRACE
	// copied closely from IronDoc sources:
	ab_File* file = mFileTracer_File;
	if ( file && file->IsOpenObject() )
	{
		this->trace_indent(ev);

		if ( inXmlWrap ) // need a leading trace tag?
			file->Write(ev, ab_kTraceTag, ab_kTraceTagSize);

		file->Write(ev, inMessage, XP_STRLEN(inMessage));

		if ( inXmlWrap ) // need a trailing end trace tag?
			file->Write(ev, ab_kTraceEndTag, ab_kTraceEndTagSize);
		else
			file->Write(ev, ab_kNewline, ab_kNewlineSize);
	}
#endif /*AB_CONFIG_TRACE*/
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Tracer methods

void ab_FileTracer::TraceString(ab_Env* ev, const char* inMessage) /*i*/
{
#ifdef AB_CONFIG_TRACE
	this->trace_string(ev, inMessage, /*inXmlWrap*/ AB_kTrue);
#endif /*AB_CONFIG_TRACE*/
}

void ab_FileTracer::BeginMethod(ab_Env* ev, const char* inClass, /*i*/
	const char* inMethod)
{
#ifdef AB_CONFIG_TRACE
	if ( this->IsOpenObject() )
	{
		char buf[ 256 ];
		XP_SPRINTF(buf, "<ab:trace method=\"%.48s::%.48s\"> {",
			inClass, inMethod);
		this->trace_string(ev, buf, /*inXmlWrap*/ AB_kFalse);
		++mTracer_MethodDepth;
	}
#endif /*AB_CONFIG_TRACE*/
}

void ab_FileTracer::EndMethod(ab_Env* ev) /*i*/
{
#ifdef AB_CONFIG_TRACE
	if ( this->IsOpenObject() )
	{
		if ( mTracer_MethodDepth )
			--mTracer_MethodDepth;
		else
		{
#ifdef AB_CONFIG_DEBUG
			this->BreakObject(ev);
#endif /*AB_CONFIG_DEBUG*/
		}
		this->trace_string(ev, "} </ab:trace>", /*inXmlWrap*/ AB_kFalse);
	}

#endif /*AB_CONFIG_TRACE*/
}

void ab_FileTracer::Flush(ab_Env* ev) /*i*/
{
#ifdef AB_CONFIG_TRACE
	if ( this->IsOpenObject() )
	{
		ab_File* file = mFileTracer_File;
		if ( file && file->IsOpenObject() )
		{
			file->Flush(ev);
		}
		else this->CastAwayConstCloseObject(ev);
	}
#endif /*AB_CONFIG_TRACE*/
}

ab_Printer* ab_FileTracer::GetPrinter(ab_Env* ev) /*i*/
{
	AB_USED_PARAMS_1(ev);
	ab_Printer* outPrinter = 0;
	if ( this->IsOpenObject() )
		outPrinter = mFileTracer_Printer;
	return outPrinter;
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_FileTracer::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	XP_SPRINTF(outXmlBuf,
		"<ab:file:tracer:str me=\"^%lX\" md=\"%ld\" f:p=\"^%lX:^%lx\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                                // me=\"^%lX\"
		(long) mTracer_MethodDepth,                 // md=\"%ld\"
		(long) mFileTracer_File,                    // f:p=\"^%lX
		(long) mFileTracer_Printer,                 // ^%lx\"
		(unsigned long) mObject_RefCount,           // rc=\"%lu\"
		this->GetObjectAccessAsString(),            // ac=\"%.9s\"
		this->GetObjectUsageAsString()              // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_FileTracer::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseFileTracer(ev);
		this->MarkShut();
	}
}

void ab_FileTracer::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:file:tracer>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PushDepth(ev); // indent all objects in the list

		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
		
		if ( mFileTracer_File )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mFileTracer_File->PrintObject(ev, ioPrinter);
		}
		
		if ( mFileTracer_Printer )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mFileTracer_Printer->PrintObject(ev, ioPrinter);
		}
		
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab:file:tracer>");
#endif /*AB_CONFIG_PRINT*/
}

ab_FileTracer::~ab_FileTracer() /*i*/
{
	AB_ASSERT(mFileTracer_File==0);
	AB_ASSERT(mFileTracer_Printer==0);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	ab_Object* obj = mFileTracer_File;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_FileTracer_kClassName);
	obj = mFileTracer_Printer;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_FileTracer_kClassName);
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// public non-poly ab_FileTracer methods

ab_FileTracer::ab_FileTracer(ab_Env* ev, const ab_Usage& inUsage, /*i*/
    ab_File* ioFile, ab_Printer* ioPrinter)
    : ab_Tracer(inUsage),
    mFileTracer_File( 0 ),
    mFileTracer_Printer( 0 )
{
	ab_Env_BeginMethod(ev, ab_FileTracer_kClassName, ab_FileTracer_kClassName)
	
	if ( ev->Good() )
	{
		if ( ioFile && ioPrinter )
		{
			if ( ioFile->AcquireObject(ev) )
			{
				mFileTracer_File = ioFile;
				if ( ioPrinter->AcquireObject(ev) )
					mFileTracer_Printer = ioPrinter;
			}
		}
		else ev->NewAbookFault(ab_Object_kFaultNullObjectParameter);
	}

	ab_Env_EndMethod(ev)
}

void ab_FileTracer::CloseFileTracer(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_FileTracer_kClassName, "CloseFileTracer")

	ab_Object* obj = mFileTracer_File;
  	if ( obj )
  	{
  		mFileTracer_File = 0;
  		obj->ReleaseObject(ev);
  	}
	obj = mFileTracer_Printer;
  	if ( obj )
  	{
  		mFileTracer_Printer = 0;
  		obj->ReleaseObject(ev);
  	}

	ab_Env_EndMethod(ev)
}


static ab_FileTracer* ab_FileTracer_g_log_tracer = 0;

/*static*/
ab_FileTracer* ab_FileTracer::GetLogFileTracer(ab_Env* ev)
	// standard session file tracer (uses ab_StdioFile::GetLogStdioFile()).
	// This tracer instance is used by ab_Env::GetLogFileEnv().
	// (Please, never close this tracer instance.)
{
	ab_FileTracer* outTracer = 0;
	ab_Env_BeginMethod(ev, ab_FileTracer_kClassName, "GetLogFileTracer")

	outTracer = ab_FileTracer_g_log_tracer;
	if ( !outTracer )
	{
		ab_StdioFile* file = ab_StdioFile::GetLogStdioFile(ev);
		if ( file )
		{
			ab_Printer* printer = ab_FilePrinter::GetLogFilePrinter(ev);
			if ( printer )
			{
				outTracer = new(*ev)
					ab_FileTracer(ev, ab_Usage::kHeap, file, printer);
			}
		}
		if ( outTracer )
		{
			ab_FileTracer_g_log_tracer = outTracer;
			outTracer->AcquireObject(ev);
		}
	}
	
	if ( outTracer )
	{
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
			outTracer->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/
	}
	ab_Env_EndMethod(ev)
	return outTracer;
}

