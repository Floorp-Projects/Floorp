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

/* file: abfile.cpp
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

#include "xp_error.h"

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_File_kClassName /*i*/ = "ab_File";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* AB_File_kClassName /*i*/ = "AB_File";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_StdioFile_kClassName /*i*/ = "ab_StdioFile";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_Stream_kClassName /*i*/ = "ab_Stream";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_StringFile_kClassName /*i*/ = "ab_StringFile";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/



/*=============================================================================
 * ab_File: abstract file interface
 */

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

static const char* ab_File_k_empty_string = ""; // for null or empty filename

ab_File::~ab_File() /*i*/
{
	AB_ASSERT(mFile_Name==ab_File_k_empty_string);
}

char* ab_File::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
    char fileFlags[ 5 ];
    
    fileFlags[ 0 ] = (FileFrozen())? 'F' : 'f';
    fileFlags[ 1 ] = (FileDoTrace())? 'T' : 't';
    fileFlags[ 2 ] = (FileIoOpen())? 'O' : 'o';
    fileFlags[ 3 ] = (FileActive())? 'A' : 'a';
    fileFlags[ 4 ] = '\0';

	const char* name = (mFile_Name)? mFile_Name : "<nil>";

	XP_SPRINTF(outXmlBuf,
		"<ab:file:str me=\"^%lX\" flags=\"%.4s\" fn=\"%.96s\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                                // me=\"^%lX\"
		fileFlags,                                  // flags=\"%.4s\"
		name,                                       // fn=\"%.96s\"
		(unsigned long) mObject_RefCount,           // rc=\"%lu\"
		this->GetObjectAccessAsString(),            // ac=\"%.9s\"
		this->GetObjectUsageAsString()              // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_File methods

ab_File::ab_File(const ab_Usage& inUsage) /*i*/
	: ab_Object(inUsage),
	mFile_Frozen( 0 ),
	mFile_DoTrace( 0 ),
	mFile_IoOpen( 0 ),
	mFile_Active( 0 ),
	mFile_Name( ab_File_k_empty_string ),
	mFile_FormatHint( ab_File_kUnknownFormat )
{
}

void ab_File::CloseFile(ab_Env* ev) /*i*/
{
	ab_File_BeginMethod(this, ev, ab_File_kClassName, "CloseFile")
	
	this->ChangeFileName(ev, ab_File_k_empty_string);

	ab_File_EndMethod(this, ev)
}

ab_File_eFormat ab_File::GuessFileFormat(ab_Env* ev) /*i*/
{
	ab_File_eFormat outFormat = ab_File_kUnknownFormat;
	ab_File_BeginMethod(this, ev, ab_File_kClassName, "GuessFileFormat")
	
	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);

	ab_File_EndMethod(this, ev)
	return outFormat;
}

void ab_File::ChangeFileName(ab_Env* ev, const char* inFileName) /*i*/
	// inFileName can be nil (which has the same effect as passing "")
{
	ab_File_BeginMethod(this, ev, ab_File_kClassName, "ChangeFileName")
	
	if ( !inFileName )
		inFileName = ab_File_k_empty_string;

#ifdef AB_CONFIG_TRACE
	if ( this->FileDoTrace() && ev->DoTrace() )
		ev->Trace("<ab:file:change:name from=\"%.96s\" to=\".96s\"/>",
			mFile_Name, inFileName);
#endif /*AB_CONFIG_TRACE*/

	char* name = (char*) mFile_Name; // cast away const in case free occurs
	if ( name && name != ab_File_k_empty_string )
		ev->FreeString(name);
		
	if ( *inFileName )
		mFile_Name = ev->CopyString(inFileName);
	else
		mFile_Name = ab_File_k_empty_string;
	
	ab_File_EndMethod(this, ev)
}

void ab_File::NewFileDownFault(ab_Env* ev) const /*i*/
	// call NewFileDownFault() when either IsOpenAndActiveFile()
	// is false, or when IsOpenActiveAndMutableFile() is false.
{
	ab_File_BeginMethod(this, ev, ab_File_kClassName, "NewFileDownFault")

#ifdef AB_CONFIG_TRACE
	if ( this->FileDoTrace() && ev->DoTrace() )
		this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	if ( this->IsOpenObject() )
	{
		if ( this->FileActive() )
		{
			if ( this->FileFrozen() )
			{
				ev->NewAbookFault(ab_File_kFaultFrozen);
			}
			else ev->NewAbookFault(ab_File_kFaultDownUnknown);
		}
		else ev->NewAbookFault(ab_File_kFaultNotActive);
	}
	else ev->NewAbookFault(ab_File_kFaultNotOpen);

	ab_File_EndMethod(this, ev)
}

void ab_File::NewFileErrnoFault(ab_Env* ev) const /*i*/
	// call NewFileErrnoFault() to convert std C errno into AB fault
{
	ab_error_uid faultCode = (ab_error_uid) errno; // capture first thing

	ab_File_BeginMethod(this, ev, ab_File_kClassName, "NewFileErrnoFault")

#ifdef AB_CONFIG_TRACE
	if ( this->FileDoTrace() && ev->DoTrace() )
		this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	if ( !faultCode ) // errno is unexpectedly equal to zero?
		faultCode = ab_File_kFaultZeroErrno;
		
	ev->NewFault(faultCode, /*space*/ AB_Fault_kErrnoSpace);

	ab_File_EndMethod(this, ev)
}

// ````` ````` ````` ````` newlines ````` ````` ````` `````  

#ifdef XP_MAC
       static const char* ab_File_kNewlines = 
       "\015\015\015\015\015\015\015\015\015\015\015\015\015\015\015\015";
#      define ab_File_kNewlinesCount 16
#else
#  if defined(XP_WIN) || defined(XP_OS2)
       static const char* ab_File_kNewlines = 
       "\015\012\015\012\015\012\015\012\015\012\015\012\015\012\015\012";
#    define ab_File_kNewlinesCount 8
#  else
#    ifdef XP_UNIX
       static const char* ab_File_kNewlines = 
       "\012\012\012\012\012\012\012\012\012\012\012\012\012\012\012\012";
#      define ab_File_kNewlinesCount 16
#    endif /* XP_UNIX */
#  endif /* XP_WIN */
#endif /* XP_MAC */

void // (close copy of IronDoc's FeStdioFile_WriteNewlines() )
ab_File::WriteNewlines(ab_Env* ev, ab_num inNewlines) /*i*/
{
    while ( inNewlines && ev->Good() ) // more newlines to write?
    {
      ab_u4 quantum = inNewlines;
      if ( quantum > ab_File_kNewlinesCount )
        quantum = ab_File_kNewlinesCount;
        
      this->Write(ev, ab_File_kNewlines, quantum * ab_kNewlineSize);
      inNewlines -= quantum;
    }
}

/*=============================================================================
 * ab_StdioFile: concrete file using standard C file io
 */

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_File methods

ab_pos ab_StdioFile::Length(ab_Env* ev) const /*i*/
{
	ab_pos outPos = 0;
	ab_File_BeginMethod(this, ev, ab_StdioFile_kClassName, "Length")
	
	if ( this->IsOpenAndActiveFile() )
	{
		FILE* file = (FILE*) mStdioFile_File;
		if ( file )
		{
			long start = XP_FileTell(file);
			if ( start >= 0 )
			{
				long fore = XP_FileSeek(file, 0, SEEK_END);
				if ( fore >= 0 )
				{
					long eof = XP_FileTell(file);
					if ( eof >= 0 )
					{
						long back = XP_FileSeek(file, start, SEEK_SET);
						if ( back >= 0 )
							outPos = eof;
						else
							this->new_stdio_file_fault(ev);
					}
					else this->new_stdio_file_fault(ev);
				}
				else this->new_stdio_file_fault(ev);
			}
			else this->new_stdio_file_fault(ev);
		}
		else ev->NewAbookFault(ab_File_kFaultMissingIo);
	}
	else this->NewFileDownFault(ev);

	ab_File_EndMethod(this, ev)
	return outPos;
}

ab_pos ab_StdioFile::Tell(ab_Env* ev) const /*i*/
{
	ab_pos outPos = 0;
	ab_File_BeginMethod(this, ev, ab_StdioFile_kClassName, "Tell")
	
	if ( this->IsOpenAndActiveFile() )
	{
		FILE* file = (FILE*) mStdioFile_File;
		if ( file )
		{
			long where = XP_FileTell(file);
			if ( where >= 0 )
				outPos = where;
			else
				this->new_stdio_file_fault(ev);
		}
		else ev->NewAbookFault(ab_File_kFaultMissingIo);
	}
	else this->NewFileDownFault(ev);

	ab_File_EndMethod(this, ev)
	return outPos;
}

ab_num ab_StdioFile::Read(ab_Env* ev, void* outBuf, ab_num inSize) /*i*/
{
	ab_num outCount = 0;
	ab_File_BeginMethod(this, ev, ab_StdioFile_kClassName, "Read")
	
	if ( this->IsOpenAndActiveFile() )
	{
		FILE* file = (FILE*) mStdioFile_File;
		if ( file )
		{
			long count = XP_FileRead(outBuf, inSize, file);
			if ( count >= 0 )
			{
				outCount = (ab_num) count;
			}
			else this->new_stdio_file_fault(ev);
		}
		else ev->NewAbookFault(ab_File_kFaultMissingIo);
	}
	else this->NewFileDownFault(ev);

	ab_File_EndMethod(this, ev)
	return outCount;
}

ab_pos ab_StdioFile::Seek(ab_Env* ev, ab_pos inPos) /*i*/
{
	ab_pos outPos = 0;
	ab_File_BeginMethod(this, ev, ab_StdioFile_kClassName, "Seek")
	
	if ( this->IsOpenOrClosingObject() && this->FileActive() )
	{
		FILE* file = (FILE*) mStdioFile_File;
		if ( file )
		{
			long where = XP_FileSeek(file, inPos, SEEK_SET);
			if ( where >= 0 )
				outPos = inPos;
			else
				this->new_stdio_file_fault(ev);
		}
		else ev->NewAbookFault(ab_File_kFaultMissingIo);
	}
	else this->NewFileDownFault(ev);

	ab_File_EndMethod(this, ev)
	return outPos;
}

ab_num ab_StdioFile::Write(ab_Env* ev, const void* inBuf, ab_num inSize) /*i*/
{
	ab_num outCount = 0;
	ab_File_BeginMethod(this, ev, ab_StdioFile_kClassName, "Write")
	
	if ( this->IsOpenActiveAndMutableFile() )
	{
		FILE* file = (FILE*) mStdioFile_File;
		if ( file )
		{
			if ( fwrite(inBuf, 1, inSize, file) >= 0 )
				outCount = inSize;
			else
				this->new_stdio_file_fault(ev);
		}
		else ev->NewAbookFault(ab_File_kFaultMissingIo);
	}
	else this->NewFileDownFault(ev);

	ab_File_EndMethod(this, ev)
	return outCount;
}

void ab_StdioFile::Flush(ab_Env* ev) /*i*/
{
	ab_File_BeginMethod(this, ev, ab_StdioFile_kClassName, "Flush")
	
	if ( this->IsOpenOrClosingObject() && this->FileActive() )
	{
		FILE* file = (FILE*) mStdioFile_File;
		if ( file )
		{
			XP_FileFlush(file);

		}
		else ev->NewAbookFault(ab_File_kFaultMissingIo);
	}
	else this->NewFileDownFault(ev);

	ab_File_EndMethod(this, ev)
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_StdioFile::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	const char* name = (mFile_Name)? mFile_Name : "<nil>";
	XP_SPRINTF(outXmlBuf,
		"<ab:stdio:file:str me=\"^%lX\" sf=\"^%lX\" fn=\"%.96s\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                                // me=\"^%lX\"
		(long) mStdioFile_File,                     // sf=\"^%lX\"
		name,                                       // fn=\"%.96s\"
		(unsigned long) mObject_RefCount,           // rc=\"%lu\"
		this->GetObjectAccessAsString(),            // ac=\"%.9s\"
		this->GetObjectUsageAsString()              // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_StdioFile::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseStdioFile(ev);
		this->MarkShut();
	}
}

void ab_StdioFile::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:stdio:file>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PushDepth(ev); // indent all objects in the list

		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ab_File::ObjectAsString(ev, xmlBuf));

		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
				
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab:stdio:file>");
#endif /*AB_CONFIG_PRINT*/
}

ab_StdioFile::~ab_StdioFile() /*i*/
{
	AB_ASSERT(mStdioFile_File==0);
}
    
// ````` ````` ````` `````   ````` ````` ````` `````  
// protected non-poly ab_StdioFile methods

void ab_StdioFile::new_stdio_file_fault(ab_Env* ev) const
{
	ab_File_BeginMethod(this, ev, ab_StdioFile_kClassName,
		 "new_stdio_file_fault")
	
	FILE* file = (FILE*) mStdioFile_File;

#if defined(AB_CONFIG_DEBUG) || defined(AB_CONFIG_TRACE)
	if ( file && ferror(file) != errno )
	{
		long fe = ferror(file);
		long e = errno;
#if defined(AB_CONFIG_TRACE) && !defined(AB_CONFIG_DEBUG)
		if ( this->DoTrace() && ev->DoTrace() )
			ev->Trace(ev, "ferror(f)=%ld errno=%ld", fe, e);
#endif /* debug but not trace */

		ev->Break("ferror(f)=%ld errno=%ld", fe, e);
	}
#endif /*AB_CONFIG_DEBUG or AB_CONFIG_TRACE*/
  
	if ( !errno && file )
		errno = ferror(file);

	this->NewFileErrnoFault(ev);

	ab_File_EndMethod(this, ev)
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// public non-poly ab_StdioFile methods

ab_StdioFile::ab_StdioFile(const ab_Usage& inUsage) /*i*/
	: ab_File(inUsage),
	mStdioFile_File( 0 )
{
}

ab_StdioFile::ab_StdioFile(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	const char* inName, const char* inMode)
    // calls OpenStdio() after construction
	: ab_File(inUsage),
	mStdioFile_File( 0 )
{
	ab_File_BeginMethod(this, ev, ab_StdioFile_kClassName,
		ab_StdioFile_kClassName)
	
	if ( ev->Good() )
	{
		this->OpenStdio(ev, inName, inMode);
	}

	ab_File_EndMethod(this, ev)
}

ab_StdioFile::ab_StdioFile(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	void* ioFile, const char* inName, ab_bool inFrozen)
    // calls UseStdio() after construction
	: ab_File(inUsage),
	mStdioFile_File( 0 )
{
	ab_File_BeginMethod(this, ev, ab_StdioFile_kClassName,
		ab_StdioFile_kClassName)
	
	if ( ev->Good() )
	{
		this->UseStdio(ev, ioFile, inName, inFrozen);
	}

	ab_File_EndMethod(this, ev)
}

// ````` ````` globals for GetLogStdioFile() ````` `````

char ab_StdioFile_g_log_file_name[ 32 ];
FILE* ab_StdioFile_g_log_file = 0;
ab_StdioFile* ab_StdioFile_g_log_stdio_file = 0;
    
/*static*/
ab_StdioFile* ab_StdioFile::GetLogStdioFile(ab_Env* ev) /*i*/
	// return the standard log file used by ab_StdioFile for each session.
{
	// (static file methods must the Env method macros:)
	ab_Env_BeginMethod(ev, ab_StdioFile_kClassName, "GetLogStdioFile")
	
	if ( !ab_StdioFile_g_log_stdio_file )
	{
		FILE* file = ab_StdioFile_g_log_file;
		if ( !file )
		{
		    time_t now; time(&now);
		    char* fileName = ab_StdioFile_g_log_file_name;
		    XP_SPRINTF(fileName, "ab.%lX.log", (long) now);
		    file = fopen(fileName, "a+");
		    if ( file )
		    	ab_StdioFile_g_log_file = file;
		    else 
		    {
				ab_error_uid faultCode = (ab_error_uid) errno;
				if ( !faultCode ) // errno is unexpectedly equal to zero?
					faultCode = ab_File_kFaultZeroErrno;
					
				ev->NewFault(faultCode, /*space*/ AB_Fault_kErrnoSpace);
		    }
		}
	    if ( file )
	    {
		    char* fileName = ab_StdioFile_g_log_file_name;
	    	ab_bool frozen = AB_kFalse;
	    	ab_StdioFile* stdioFile = new(*ev)
		    	ab_StdioFile(ev, ab_Usage::kHeap, file, fileName, frozen);
		    if ( stdioFile )
		    {
		    	ab_StdioFile_g_log_stdio_file = stdioFile;
		    	stdioFile->AcquireObject(ev);
		    }
	    }
	    else 
	    {
			ab_error_uid faultCode = (ab_error_uid) errno;
			if ( !faultCode ) // errno is unexpectedly equal to zero?
				faultCode = ab_File_kFaultZeroErrno;
				
			ev->NewFault(faultCode, /*space*/ AB_Fault_kErrnoSpace);
	    }
	}

	ab_Env_EndMethod(ev)
	return ab_StdioFile_g_log_stdio_file;
}
    
    
void ab_StdioFile::CloseStdioFile(ab_Env* ev) /*i*/
{
	ab_File_BeginMethod(this, ev, ab_StdioFile_kClassName, "CloseStdioFile")

	if ( mStdioFile_File && this->FileActive() && this->FileIoOpen() )
	{
		this->CloseStdio(ev);
	}
	
	mStdioFile_File = 0;
	
	this->CloseFile(ev);

	ab_File_EndMethod(this, ev)
}

void ab_StdioFile::OpenStdio(ab_Env* ev, const char* inName, /*i*/
	 const char* inMode)
	// Open a new FILE with name inName, using mode flags from inMode.
{
	ab_File_BeginMethod(this, ev, ab_StdioFile_kClassName, "OpenStdio")
	
	if ( ev->Good() )
	{
		if ( !inMode )
			inMode = "";
		
		ab_bool frozen = (*inMode == 'r'); // cursory attempt to note readonly
		
		if ( this->IsOpenObject() )
		{
			if ( !this->FileActive() )
			{
				this->SetFileIoOpen(AB_kFalse);
				if ( inName && *inName )
				{
					this->ChangeFileName(ev, inName);
					if ( ev->Good() )
					{
						FILE* file = fopen(inName, inMode);
						if ( file )
						{
							mStdioFile_File = file;
							this->SetFileActive(AB_kTrue);
							this->SetFileIoOpen(AB_kTrue);
							this->SetFileFrozen(frozen);
						}
						else
							this->new_stdio_file_fault(ev);
					}
			}
				else ev->NewAbookFault(ab_File_kFaultNoFileName);
			}
			else ev->NewAbookFault(ab_File_kFaultAlreadyActive);
		}
		else this->NewFileDownFault(ev);
	}

	ab_File_EndMethod(this, ev)
}

void ab_StdioFile::UseStdio(ab_Env* ev, void* ioFile, /*i*/
	const char* inName, ab_bool inFrozen)
	// Use an existing file, like stdin/stdout/stderr, which should not
	// have the io stream closed when the file is closed.  The ioFile
	// parameter must actually be of type FILE (but we don't want to make
	// this header file include the stdio.h header file).
{
	ab_File_BeginMethod(this, ev, ab_StdioFile_kClassName, "UseStdio")
	
	if ( ev->Good() )
	{
		if ( this->IsOpenObject() )
		{
			if ( !this->FileActive() )
			{
				if ( ioFile )
				{
					this->SetFileIoOpen(AB_kFalse);
					this->ChangeFileName(ev, inName);
					if ( ev->Good() )
					{
						mStdioFile_File = ioFile;
						this->SetFileActive(AB_kTrue);
						this->SetFileFrozen(inFrozen);
					}
				}
				else ev->NewAbookFault(ab_File_kFaultNullOpaqueParameter);
			}
			else ev->NewAbookFault(ab_File_kFaultAlreadyActive);
		}
		else this->NewFileDownFault(ev);
	}

	ab_File_EndMethod(this, ev)
}
	
void ab_StdioFile::CloseStdio(ab_Env* ev) /*i*/
	// Close the stream io if both and FileActive() and FileIoOpen(), but
	// this does not close this instances (like CloseStdioFile() does).
	// If stream io was made active by means of calling UseStdio(),
	// then this method does little beyond marking the stream inactive
	// because FileIoOpen() is false.
{
	ab_File_BeginMethod(this, ev, ab_StdioFile_kClassName, "CloseStdio")
	
	if ( mStdioFile_File && this->FileActive() && this->FileIoOpen() )
	{
		FILE* file = (FILE*) mStdioFile_File;
		if ( XP_FileClose(file) < 0 )
			this->new_stdio_file_fault(ev);
		
		mStdioFile_File = 0;
		this->SetFileActive(AB_kFalse);
		this->SetFileIoOpen(AB_kFalse);
	}

	ab_File_EndMethod(this, ev)
}

/*=============================================================================
 * ab_Stream: buffered file i/o
 */

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_File methods

ab_pos ab_Stream::Length(ab_Env* ev) const /*i*/
{
	ab_pos outPos = 0;
	ab_File_BeginMethod(this, ev, ab_Stream_kClassName, "Length")
	
	ab_File* file = mStream_ContentFile;
	if ( this->IsOpenAndActiveFile() && file )
	{
		ab_pos contentEof = file->Length(ev);
		
		if ( mStream_WriteEnd ) // this stream supports writing?
		{
			// the local buffer might have buffered content past content eof
			if ( ev->Good() ) // no error happened during Length() above?
			{
				ab_u1* at = mStream_At;
				ab_u1* buf = mStream_Buf;
				if ( at >= buf ) // expected cursor order?
				{
					ab_pos localContent = mStream_BufPos + (at - buf);
					if ( localContent > contentEof ) // buffered past eof?
						contentEof = localContent; // return new logical eof

					outPos = contentEof;
				}
				else ev->NewAbookFault(ab_File_kFaultBadCursorOrder);
			}
		}
		else
			outPos = contentEof; // frozen files get length from content file
	}
	else this->NewFileDownFault(ev);

	ab_File_EndMethod(this, ev)
	return outPos;
}

ab_pos ab_Stream::Tell(ab_Env* ev) const /*i*/
{
	ab_pos outPos = 0;
	ab_File_BeginMethod(this, ev, ab_Stream_kClassName, "Tell")
	
	ab_File* file = mStream_ContentFile;
	if ( this->IsOpenAndActiveFile() && file )
	{
		ab_u1* buf = mStream_Buf;
		ab_u1* at = mStream_At;
		
		ab_u1* readEnd = mStream_ReadEnd;   // nonzero only if readonly
		ab_u1* writeEnd = mStream_WriteEnd; // nonzero only if writeonly
		
		if ( writeEnd )
		{
			if ( buf && at >= buf && at <= writeEnd ) 
			{
				outPos = mStream_BufPos + (at - buf);
			}
			else ev->NewAbookFault(ab_File_kFaultBadCursorOrder);
		}
		else if ( readEnd )
		{
			if ( buf && at >= buf && at <= readEnd ) 
			{
				outPos = mStream_BufPos + (at - buf);
			}
			else ev->NewAbookFault(ab_File_kFaultBadCursorOrder);
		}
	}
	else this->NewFileDownFault(ev);

	ab_File_EndMethod(this, ev)
	return outPos;
}

ab_num ab_Stream::Read(ab_Env* ev, void* outBuf, ab_num inSize)
{
	// First we satisfy the request from buffered bytes, if any.  Then
	// if additional bytes are needed, we satisfy these by direct reads
	// from the content file without any local buffering (but we still need
	// to adjust the buffer position to reflect the current i/o point).

	ab_pos outActual = 0;
	ab_File_BeginMethod(this, ev, ab_Stream_kClassName, "Read")

	ab_File* file = mStream_ContentFile;
	if ( this->IsOpenAndActiveFile() && file )
	{
		ab_u1* end = mStream_ReadEnd; // byte after last buffered byte
		if ( end ) // file is open for read access?
		{
			if ( inSize ) // caller wants any output?
			{
				ab_u1* sink = (ab_u1*) outBuf; // where we plan to write bytes
				if ( sink ) // caller passed good buffer address?
				{
					ab_u1* at = mStream_At;
					ab_u1* buf = mStream_Buf;
					if ( at >= buf && at <= end ) // expected cursor order?
					{
						ab_num remaining = end - at; // bytes left in buffer
						
						ab_num quantum = inSize; // number of bytes to copy
						if ( quantum > remaining ) // more than buffer content?
							quantum = remaining; // restrict to buffered bytes
							
						if ( quantum ) // any bytes left in the buffer?
						{
							AB_MEMCPY(sink, at, quantum); // from buffer bytes
							
							at += quantum; // advance past read bytes
							mStream_At = at;
							outActual += quantum;  // this much copied so far

							sink += quantum;   // in case we need to copy more
							inSize -= quantum; // filled this much of request
							mStream_HitEof = AB_kFalse;
						}
						
						if ( inSize ) // we still need to read more content?
						{
							// We need to read more bytes directly from the
							// content file, without local buffering.  We have
							// exhausted the local buffer, so we need to show
							// it is now empty, and adjust the current buf pos.
							
							ab_num posDelta = (at - buf); // old buf content
							mStream_BufPos += posDelta;   // past now empty buf
							
							mStream_At = mStream_ReadEnd = buf; // empty buffer
							
							file->Seek(ev, mStream_BufPos); // set file pos
							if ( ev->Good() ) // no seek error?
							{
								ab_num actual = file->Read(ev, sink, inSize);
								if ( ev->Good() ) // no read error?
								{
									if ( actual )
									{
										outActual += actual;
										mStream_BufPos += actual;
										mStream_HitEof = AB_kFalse;
									}
									else if ( !outActual )
										mStream_HitEof = AB_kTrue;
								}
							}
						}
					}
					else ev->NewAbookFault(ab_File_kFaultBadCursorOrder);
				}
				else ev->NewAbookFault(ab_File_kFaultNullBuffer);
			}
		}
		else ev->NewAbookFault(ab_File_kFaultCantReadSink);
	}
	else this->NewFileDownFault(ev);
	
	if ( ev->Bad() )
		outActual = 0;

	ab_File_EndMethod(this, ev)
	return outActual;
}

ab_pos ab_Stream::Seek(ab_Env* ev, ab_pos inPos)
{
	ab_pos outPos = 0;
	ab_File_BeginMethod(this, ev, ab_Stream_kClassName, "Seek")

	ab_File* file = mStream_ContentFile;
	if ( this->IsOpenOrClosingObject() && this->FileActive() && file )
	{
		ab_u1* at = mStream_At;             // current position in buffer
		ab_u1* buf = mStream_Buf;           // beginning of buffer 
		ab_u1* readEnd = mStream_ReadEnd;   // nonzero only if readonly
		ab_u1* writeEnd = mStream_WriteEnd; // nonzero only if writeonly
		
		if ( writeEnd ) // file is mutable/writeonly?
		{
			if ( mStream_Dirty ) // need to commit buffer changes?
				this->Flush(ev);

			if ( ev->Good() ) // no errors during flush or earlier?
			{
				if ( at == buf ) // expected post flush cursor value?
				{
					if ( mStream_BufPos != inPos ) // need to change pos?
					{
						ab_pos eof = file->Length(ev);
						if ( ev->Good() ) // no errors getting length?
						{
							if ( inPos <= eof ) // acceptable new position?
							{
								mStream_BufPos = inPos; // new stream position
								outPos = inPos;
							}
							else ev->NewAbookFault(ab_File_kFaultPosBeyondEof);
						}
					}
				}
				else ev->NewAbookFault(ab_File_kFaultBadCursorOrder);
			}
		}
		else if ( readEnd ) // file is frozen/readonly?
		{
			if ( at >= buf && at <= readEnd ) // expected cursor order?
			{
				ab_pos eof = file->Length(ev);
				if ( ev->Good() ) // no errors getting length?
				{
					if ( inPos <= eof ) // acceptable new position?
					{
						outPos = inPos;
						mStream_BufPos = inPos; // new stream position
						mStream_At = mStream_ReadEnd = buf; // empty buffer
						if ( inPos == eof ) // notice eof reached?
							mStream_HitEof = AB_kTrue;
					}
					else ev->NewAbookFault(ab_File_kFaultPosBeyondEof);
				}
			}
			else ev->NewAbookFault(ab_File_kFaultBadCursorOrder);
		}
			
	}
	else this->NewFileDownFault(ev);

	ab_File_EndMethod(this, ev)
	return outPos;
}

ab_num ab_Stream::Write(ab_Env* ev, const void* inBuf, ab_num inSize)
{
	ab_num outActual = 0;
	ab_File_BeginMethod(this, ev, ab_Stream_kClassName, "Write")

	ab_File* file = mStream_ContentFile;
	if ( this->IsOpenActiveAndMutableFile() && file )
	{
		ab_u1* end = mStream_WriteEnd; // byte after last buffered byte
		if ( end ) // file is open for write access?
		{
			if ( inSize ) // caller provided any input?
			{
				const ab_u1* source = (const ab_u1*) inBuf; // from where
				if ( source ) // caller passed good buffer address?
				{
					ab_u1* at = mStream_At;
					ab_u1* buf = mStream_Buf;
					if ( at >= buf && at <= end ) // expected cursor order?
					{
						ab_num space = end - at; // space left in buffer
						
						ab_num quantum = inSize; // number of bytes to write
						if ( quantum > space ) // more than buffer size?
							quantum = space; // restrict to avail space
							
						if ( quantum ) // any space left in the buffer?
						{
							mStream_Dirty = AB_kTrue; // to ensure later flush
							AB_MEMCPY(at, source, quantum); // into buffer
							
							mStream_At += quantum; // advance past written bytes
							outActual += quantum;  // this much written so far

							source += quantum; // in case we need to write more
							inSize -= quantum; // filled this much of request
						}
						
						if ( inSize ) // we still need to write more content?
						{
							// We need to write more bytes directly to the
							// content file, without local buffering.  We have
							// exhausted the local buffer, so we need to flush
							// it and empty it, and adjust the current buf pos.
							// After flushing, if the rest of the write fits
							// inside the buffer, we will put bytes into the
							// buffer rather than write them to content file.
							
							if ( mStream_Dirty )
								this->Flush(ev); // will update mStream_BufPos

							at = mStream_At;
							if ( at < buf || at > end ) // bad cursor?
								ev->NewAbookFault(ab_File_kFaultBadCursorOrder);
								
							if ( ev->Good() ) // no errors?
							{
								space = end - at; // space left in buffer
								if ( space > inSize ) // write to buffer?
								{
									mStream_Dirty = AB_kTrue; // ensure flush
									AB_MEMCPY(at, source, inSize); // copy
									
									mStream_At += inSize; // past written bytes
									outActual += inSize;  // this much written
								}
								else // directly to content file instead
								{
									file->Seek(ev, mStream_BufPos); // set pos
										
									if ( ev->Good() ) // no seek error?
									{
										ab_num actual =
											file->Write(ev, source, inSize);
										if ( ev->Good() ) // no write error?
										{
											outActual += actual;
											mStream_BufPos += actual;
										}
									}
								}
							}
						}
					}
					else ev->NewAbookFault(ab_File_kFaultBadCursorOrder);
				}
				else ev->NewAbookFault(ab_File_kFaultNullBuffer);
			}
		}
		else ev->NewAbookFault(ab_File_kFaultCantWriteSource);
	}
	else this->NewFileDownFault(ev);
	
	if ( ev->Bad() )
		outActual = 0;

	ab_File_EndMethod(this, ev)
	return outActual;
}

void ab_Stream::Flush(ab_Env* ev)
{
	ab_File_BeginMethod(this, ev, ab_Stream_kClassName, "Flush")

	ab_File* file = mStream_ContentFile;
	if ( this->IsOpenOrClosingObject() && this->FileActive() && file )
	{
		if ( mStream_Dirty )
			this->spill_buf(ev);
		file->Flush(ev);
	}
	else this->NewFileDownFault(ev);
	
	ab_File_EndMethod(this, ev)
}

void ab_Stream::spill_buf(ab_Env* ev)
{
	ab_File_BeginMethod(this, ev, ab_Stream_kClassName, "spill_buf")
	
	ab_File* file = mStream_ContentFile;
	if ( this->IsOpenOrClosingObject() && this->FileActive() && file )
	{
		ab_u1* buf = mStream_Buf;
		if ( mStream_Dirty )
		{
			ab_u1* at = mStream_At;
			if ( at >= buf && at <= mStream_WriteEnd ) // order?
			{
				ab_num count = at - buf; // the number of bytes buffered
				if ( count ) // anything to write to the string?
				{
					if ( count > mStream_BufSize ) // no more than max?
					{
						count = mStream_BufSize;
						mStream_WriteEnd = buf + mStream_BufSize;
						ev->NewAbookFault(ab_String_kFaultBadCursorFields);
					}
					if ( ev->Good() )
					{
						file->Seek(ev, mStream_BufPos);
						if ( ev->Good() )
						{
							file->Write(ev, buf, count);
							if ( ev->Good() )
							{
								mStream_BufPos += count; // past bytes written
								mStream_At = buf; // reset buffer cursor
								mStream_Dirty = AB_kFalse;
							}
						}
					}
				}
			}
			else ev->NewAbookFault(ab_File_kFaultBadCursorOrder);
		}
		else
		{
#ifdef AB_CONFIG_TRACE
			this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

#ifdef AB_CONFIG_DEBUG
			ev->Break("<ab:stream:spill:not:dirty me=\"^%lX\"/>", (long) this);
#endif /*AB_CONFIG_DEBUG*/
		}
	}
	else this->NewFileDownFault(ev);

	ab_File_EndMethod(this, ev)
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_Stream::ObjectAsString(ab_Env* ev, char* outXmlBuf) const
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	XP_SPRINTF(outXmlBuf,
		"<ab:stream:str me=\"^%lX\" cf=\"^%lX\" buf=\"^%lX\" bs=\"%lu\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                                // me=\"^%lX\"
		(long) mStream_ContentFile,                 // cf=\"^%lX\"
		(long) mStream_Buf,                         // buf=\"^%lX\"
		(long) mStream_BufSize,                     // bs=\"%lu\"
		(unsigned long) mObject_RefCount,           // rc=\"%lu\"
		this->GetObjectAccessAsString(),            // ac=\"%.9s\"
		this->GetObjectUsageAsString()              // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_Stream::CloseObject(ab_Env* ev)
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseStream(ev);
		this->MarkShut();
	}
}

void ab_Stream::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:stream>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PushDepth(ev); // indent all objects in the list

		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
		
		if ( mStream_ContentFile )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mStream_ContentFile->PrintObject(ev, ioPrinter);
		}
		
		if ( mStream_Buf && ev->mEnv_BeVerbose && !ev->mEnv_BeConcise )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			ioPrinter->Hex(ev, mStream_Buf, mStream_BufSize, /*pos*/ 0);
		}
		
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab:stream>");
#else /*AB_CONFIG_PRINT*/
	AB_USED_PARAMS_2(ev,ioPrinter);
#endif /*AB_CONFIG_PRINT*/
}

ab_Stream::~ab_Stream()
{
	AB_ASSERT(mStream_Buf==0);
	AB_ASSERT(mStream_ContentFile==0);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	ab_Object* obj = mStream_ContentFile;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_Stream_kClassName);
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
}

void ab_Stream::PutString(ab_Env* ev, const char* inString) /*i*/
{
	ab_File_BeginMethod(this, ev, ab_Stream_kClassName, "PutString")
	
	if ( inString )
	{
		ab_num length = AB_STRLEN(inString);
		if ( length && ev->Good() ) // any bytes to write?
		{
			this->Write(ev, inString, length);
		}
	}

	ab_File_EndMethod(this, ev)
}

void ab_Stream::PutStringThenNewline(ab_Env* ev, const char* inString) /*i*/
{
	ab_File_BeginMethod(this, ev, ab_Stream_kClassName, "PutStringThenNewline")
	
	if ( inString )
	{
		ab_num length = AB_STRLEN(inString);
		if ( length && ev->Good() ) // any bytes to write?
		{
			this->Write(ev, inString, length);
			if ( ev->Good() )
				this->WriteNewlines(ev, /*count*/ 1);
		}
	}

	ab_File_EndMethod(this, ev)
}

    
// ````` ````` ````` `````   ````` ````` ````` `````  
// protected non-poly ab_Stream methods (for char io)

int ab_Stream::fill_getc(ab_Env* ev) /*i*/
{
	int c = EOF;
	ab_File_BeginMethod(this, ev, ab_Stream_kClassName, "fill_getc")
	
	ab_File* file = mStream_ContentFile;
	if ( this->IsOpenAndActiveFile() && file )
	{
		ab_u1* buf = mStream_Buf;
		ab_u1* end = mStream_ReadEnd; // beyond buf after earlier read
		if ( end > buf ) // any earlier read bytes buffered?
		{
			mStream_BufPos += ( end - buf ); // advance past old read
		}
			
		if ( ev->Good() ) // no errors yet?
		{
			file->Seek(ev, mStream_BufPos); // set file pos
			if ( ev->Good() ) // no seek error?
			{
				ab_num actual = file->Read(ev, buf, mStream_BufSize);
				if ( ev->Good() ) // no read errors?
				{
					if ( actual > mStream_BufSize ) // more than asked for??
						actual = mStream_BufSize;
					
					mStream_At = buf;
					mStream_ReadEnd = buf + actual;
					if ( actual ) // any bytes actually read?
					{
						c = *mStream_At++; // return first byte from buffer
						mStream_HitEof = AB_kFalse;
					}
					else
						mStream_HitEof = AB_kTrue;
				}
			}
		}
	}
	else this->NewFileDownFault(ev);
	
	ab_File_EndMethod(this, ev)
	return c;
}

void ab_Stream::spill_putc(ab_Env* ev, int c) /*i*/
{
	ab_File_BeginMethod(this, ev, ab_Stream_kClassName, "spill_putc")

	this->spill_buf(ev);
	if ( ev->Good() && mStream_At < mStream_WriteEnd )
		this->Putc(ev, c);
	
	ab_File_EndMethod(this, ev)
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_Stream methods

ab_Stream::ab_Stream(ab_Env* ev, const ab_Usage& inUsage, /*i*/
    ab_File* ioContentFile, ab_num inBufSize, ab_bool inFrozen)
	: ab_File(inUsage),
	mStream_At( 0 ),
	mStream_ReadEnd( 0 ), // logical end of read buffered content
	mStream_WriteEnd( 0 ), // physical end of buffer
	mStream_ContentFile( 0 ),
	mStream_Buf( 0 ),
	mStream_BufSize( inBufSize ),
	mStream_BufPos( 0 ),
	mStream_Dirty( AB_kFalse ),
	mStream_HitEof( AB_kFalse )
{	
	if ( ev->Good() )
	{
		if ( inBufSize < ab_Stream_kMinBufSize )
			mStream_BufSize = inBufSize = ab_Stream_kMinBufSize;
		else if ( inBufSize > ab_Stream_kMaxBufSize )
			mStream_BufSize = inBufSize = ab_Stream_kMaxBufSize;
		
		if ( ioContentFile )
		{
			if ( ioContentFile->FileFrozen() ) // forced to be readonly?
				inFrozen = AB_kTrue; // override the input value
				
			if ( ioContentFile->AcquireObject(ev) )
			{
				mStream_ContentFile = ioContentFile;
				ab_u1* buf = (ab_u1*) ev->HeapAlloc(inBufSize);
				if ( buf )
				{
					mStream_At = mStream_Buf = buf;
					
					if ( !inFrozen )
					{
						// physical buffer end never moves:
						mStream_WriteEnd = buf + inBufSize;
					}
					else
						mStream_WriteEnd = 0; // no writing is allowed
					
					if ( inFrozen )
					{
						// logical buffer end starts at Buf with no content:
						mStream_ReadEnd = buf;
						this->SetFileFrozen(inFrozen);
					}
					else
						mStream_ReadEnd = 0; // no reading is allowed
					
					this->SetFileActive(AB_kTrue);
					this->SetFileIoOpen(AB_kTrue);
				}
			}
		}
		else ev->NewAbookFault(ab_Object_kFaultNullObjectParameter);
	}
}   

void ab_Stream::CloseStream(ab_Env* ev) /*i*/
{
	ab_File_BeginMethod(this, ev, ab_Stream_kClassName, "CloseStream")
	
	if ( this->FileActive() && mStream_ContentFile && mStream_WriteEnd )
	{
		if ( mStream_Dirty ) // need to commit buffer changes?
			this->Flush(ev); // also flushes the content file
	}
	
	ab_Object* obj = mStream_ContentFile;
	if ( obj )
	{
		mStream_ContentFile = 0;
		obj->ReleaseObject(ev);
	}
	
	void* buf = mStream_Buf;
	if ( buf )
	{
		mStream_Buf = 0;
		ev->HeapFree(buf);
	}
	
	this->CloseFile(ev);

	ab_File_EndMethod(this, ev)
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// ````` ````` stdio type methods ````` ````` 

void ab_Stream::Printf(ab_Env* ev, const char* inFormat, ...) /*i*/
{
	ab_File_BeginMethod(this, ev, ab_Stream_kClassName, "Printf")

	char buf[ ab_Stream_kPrintBufSize + 2 ];
	va_list args;
	va_start(args,inFormat);
	PR_vsnprintf(buf, ab_Stream_kPrintBufSize, inFormat, args);
	va_end(args);
	    
	this->PutString(ev, buf);
	
	ab_File_EndMethod(this, ev)
}

// ````` ````` high level i/o operations ````` ````` 
	
void ab_Stream::GetDoubleNewlineDelimitedRecord(ab_Env* ev, /*i*/
	ab_String* outString)
	// GetDoubleNewlineDelimitedRecord() reads a "record" from the stream
	// that is intended to correspond to an ldif record in a file being
	// imported into an address book. Such records are delimited by two
	// (or more) empty lines, where all lines separated by only a single
	// empty line are considered part of the record.  The record is written
	// to outString which grows as necessary to hold the record.  (This
	// string is first emptied with ab_String::TruncateStringLength(), so
	// the string should not be a nonempty static readonly string.  The
	// same string can easily be used in successive calls to this method
	// to avoid any more memory management than it needed.)
	//
	// Let LF, CR, and BL denote bytes 0xA, 0xD, and 0x20 respectively.
	// This method collapses sequences of LF and CR to a single LF because
	// this is what ldif parsing code expects to separate lines in each
	// ldif record.
	//
	// UNLIKE the original spec, BLANKS ARE NOT REMOVED, because leading
	// blanks are significant to continued line parsing in ldif format.
	// 
	// Counting the number of empty lines
	// encountered is made more complex by the need to make the process
	// cross platform, where a line-end on various platforms might be a
	// single LF or CR, or a combined CRLF.  Additionally, ldif files were
	// once erroneously written with line-ends composed of CRCRLF, so this
	// should count as a single line as well.  Otherwise, this method will
	// consider two empty lines encountered (thus ending the record) when
	// enough LF's and CR's are seen to account for two lines on some
	// platform or another.  Once either LF or CR is seen, all instances
	// of LF, CR, and BL will be consumed from the input stream, but only a
	// single LF will be written to the output record string.  While this
	// white space is being parsed and discarded, the loop decides whether
	// more than one line is being seen, which ends the record when true.
	// 
	// This method is intended to be a more efficient way to parse groups
	// of lines from an input file than by parsing individual lines which
	// then get concatenated together.  The performance difference might
	// matter for large ldif files, especially when more than one pass must
	// be performed on the same input file.
	// 
	// When this method returns, the current file position should point to
	// either eof (end of file) or the first byte of the next record.
{
	ab_File_BeginMethod(this, ev, ab_Stream_kClassName,
		"GetDoubleNewlineDelimitedRecord")
	
	if ( outString->TruncateStringLength(ev, /*len*/ 0) ) // string trimmed?
	{
		ab_StringSink sink(ev, outString);
		if ( ev->Good() )
		{
			register int c;
			while ( (c = this->Getc(ev)) != EOF && ev->Good() )
			{
				if ( c == 0xA || c == 0xD )
				{
					sink.Putc(ev, 0xA);
					if ( this->consuming_white_space_ends_record(ev, c) )
						break; // end while loop at end of record
				}
				else
					sink.Putc(ev, c);
			}
			
			if ( ev->Good() )
				sink.FlushStringSink(ev);
		}
	}
		
	ab_File_EndMethod(this, ev)
}

    
// ````` ````` ````` `````   ````` ````` ````` `````  
// protected non-poly high-level support utilities

ab_bool ab_Stream::consuming_white_space_ends_record(ab_Env* ev, /*i*/
	register int c)
	// Consume the rest of the line ending after a leading 0xA or 0xD
	// which is passed in as the value of input argument c.  Analyze the
	// consumed white space as per the description of public method
	// GetDoubleNewlineDelimitedRecord(), and return true if more than
	// one line-ending is detected which signals end of record.
	//
	// UNLIKE the original spec, BLANKS ARE NOT REMOVED, because leading
	// blanks are significant to continued line parsing in ldif format.
{
	ab_bool outMultipleLineEnds = AB_kFalse;
	ab_File_BeginMethod(this, ev, ab_Stream_kClassName,
		"consuming_white_space_ends_record")
	
	ab_num countLF = (ab_num) (c == 0xA); // total LF's seen
	ab_num countCR = (ab_num) (c == 0xD); // total CR's seen
	ab_num countWS = 0; // total other white space seen
	
	while ( (c = this->Getc(ev)) != EOF )
	{
		if ( ev->Good() ) // no read errors?
		{
			if ( c == 0xA ) // LF ?
			{
				++countLF;
			}
			else if ( c == 0xD ) // CR ?
			{
				++countCR;
			}
			// KEEP THE BLANKS because they affect continued line parsing:
			// else if ( XP_IS_SPACE(c) ) // other white space?
			// {
			// 	++countWS; // the value of this is currently unused
			// }
			else // non-white space
			{
				this->Ungetc(c); // put back non-(LF or CR) characters
				break; // end while loop after non-space is seen
			}
		}
		else
		{
			if ( c != 0xA && c != 0xD && !XP_IS_SPACE(c) )
				this->Ungetc(c); // put back non-white space characters
				
			break; // end while loop after any error occurs
		}
	}
	
	// the following calculation must show CRCRLF as a single line end:
	outMultipleLineEnds = 
		(
			( countLF > 1 )              || // more than one LF
			( countCR > 2 && countLF )   || // more than just CRCRLF
			( !countLF && countCR > 1 )     // no LF, but multiple CR
		);

	ab_File_EndMethod(this, ev)
	return outMultipleLineEnds;
}


/* ===== ===== ===== ===== ab_StringFile ===== ===== ===== ===== */

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_File methods

ab_pos ab_StringFile::Length(ab_Env* ev) const /*i*/
{
	ab_pos outLength = 0;
	ab_File_BeginMethod(this, ev, ab_StringFile_kClassName, "Length")
	
	if ( this->IsOpenAndActiveFile() && mStringFile_String )
	{
		outLength = mStringFile_String->GetStringLength();
	}
	else this->NewFileDownFault(ev);

	ab_File_EndMethod(this, ev)
	return outLength;
}

ab_pos ab_StringFile::Tell(ab_Env* ev) const /*i*/
{
	ab_pos outTell = 0;
	ab_File_BeginMethod(this, ev, ab_StringFile_kClassName, "Tell")
	
	if ( this->IsOpenAndActiveFile() )
	{
		outTell = mStringFile_Pos;
	}
	else this->NewFileDownFault(ev);

	ab_File_EndMethod(this, ev)
	return outTell;
}

ab_num ab_StringFile::Read(ab_Env* ev, void* outBuf, ab_num inSize) /*i*/
{
	ab_num outRead = 0;
	ab_File_BeginMethod(this, ev, ab_StringFile_kClassName, "Read")
	
	ab_String* string = mStringFile_String;
	if ( this->IsOpenAndActiveFile() && string )
	{
		ab_pos pos = mStringFile_Pos;
		ab_num length = string->GetStringLength();
		if ( pos < length ) // any bytes left to read?
		{
			ab_num remaining = length - pos;
			if ( inSize > remaining ) // more bytes requested than remain?
				inSize = remaining; // read no more than are left in string
				
			if ( inSize ) // anything to copy?
			{
				if ( outBuf ) // buffer to receive output?
				{
					const char* source = string->GetStringContent() + pos;
					AB_MEMCPY(outBuf, source, inSize);
					mStringFile_Pos += inSize; // advance beyond bytes read
					outRead = inSize;
				}
				else ev->NewAbookFault(ab_File_kFaultNullBuffer);
			}
		}
	}
	else this->NewFileDownFault(ev);

	ab_File_EndMethod(this, ev)
	return outRead;
}

ab_pos ab_StringFile::Seek(ab_Env* ev, ab_pos inPos) /*i*/
{
	ab_pos outSeek = 0;
	ab_File_BeginMethod(this, ev, ab_StringFile_kClassName, "Seek")
	
	if ( this->IsOpenOrClosingObject() && this->FileActive() && mStringFile_String )
	{
		if ( inPos <= mStringFile_String->GetStringLength() )
		{
			mStringFile_Pos = inPos;
			outSeek = inPos;
		}
		else ev->NewAbookFault(ab_File_kFaultPosBeyondEof);
	} 
	else this->NewFileDownFault(ev);

	ab_File_EndMethod(this, ev)
	return outSeek;
}

ab_num ab_StringFile::Write(ab_Env* ev, const void* inBuf, ab_num inSize) /*i*/
{
	ab_num outWrite = 0;
	ab_File_BeginMethod(this, ev, ab_StringFile_kClassName, "Write")
	
	ab_String* string = mStringFile_String;
	if ( this->IsOpenActiveAndMutableFile() && string )
	{
		if ( !this->FileFrozen() ) // file modification allowed?
		{
			ab_pos pos = mStringFile_Pos;
			if ( inSize ) // anything to write?
			{
				if ( inBuf ) // buffer to provide input?
				{
					const char* buf = (const char*) inBuf; // void* to char*
					if ( string->PutBlockContent(ev, buf, inSize, pos) )
					{
						mStringFile_Pos += inSize; // go past bytes written
						outWrite = inSize;
					}
				}
				else ev->NewAbookFault(ab_File_kFaultNullBuffer);
			}
		}
		else ev->NewAbookFault(ab_File_kFaultFrozen);
	}
	else this->NewFileDownFault(ev);

	ab_File_EndMethod(this, ev)
	return outWrite;
}
	
void ab_StringFile::Flush(ab_Env* ev) /*i*/
{
	ab_File_BeginMethod(this, ev, ab_StringFile_kClassName, "Flush")
	
	AB_USED_PARAMS_1(ev);
	// do nothing since the string does not need flushing

	ab_File_EndMethod(this, ev)
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_StringFile::ObjectAsString(ab_Env* ev, /*i*/
	char* outXmlBuf) const
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	XP_SPRINTF(outXmlBuf,
		"<ab:sting:file:str me=\"^%lX\" sf=\"^%lX\" pos=\"#%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                                // me=\"^%lX\"
		(long) mStringFile_String,                  // sf=\"^%lX\"
		(long) mStringFile_Pos,                     // pos=\"^%lX\"
		(unsigned long) mObject_RefCount,           // rc=\"%lu\"
		this->GetObjectAccessAsString(),            // ac=\"%.9s\"
		this->GetObjectUsageAsString()              // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_StringFile::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseStringFile(ev);
		this->MarkShut();
	}
}

void ab_StringFile::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:string:file>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PushDepth(ev); // indent all objects in the list

		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
		
		if ( mStringFile_String )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			ioPrinter->PutString(ev, 
				mStringFile_String->ObjectAsString(ev, xmlBuf));
		}
		
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab:string:file>");
#else /*AB_CONFIG_PRINT*/
	AB_USED_PARAMS_2(ev,ioPrinter);
#endif /*AB_CONFIG_PRINT*/
}

ab_StringFile::~ab_StringFile() /*i*/
{
	AB_ASSERT(mStringFile_String==0);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	ab_Object* obj = mStringFile_String;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_StringFile_kClassName);
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_StringFile methods

ab_StringFile::ab_StringFile(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	 ab_String* ioString)
	 : ab_File(inUsage),
	 mStringFile_String( 0 ),
	 mStringFile_Pos( 0 )
{
	if ( ev->Good() )
	{
		if ( ioString )
		{
			if ( ioString->AcquireObject(ev) )
			{
				if ( ioString->IsStringReadOnly() ) // string is readonly?
					this->SetFileFrozen(AB_kTrue); // file is readonly too
					
				mStringFile_String = ioString;
				this->SetFileActive(AB_kTrue);
				this->SetFileIoOpen(AB_kTrue);
			}
		}
		else ev->NewAbookFault(ab_Object_kFaultNullObjectParameter);
	}
}

ab_StringFile::ab_StringFile(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	 const char* inBytes)
	 : ab_File(inUsage),
	 mStringFile_String( 0 ),
	 mStringFile_Pos( 0 )
{
	if ( ev->Good() )
	{
		ab_String* newString = new(*ev)
			ab_String(ev, ab_Usage::kHeap, inBytes);
		if ( newString )
		{
			if ( ev->Good() )
			{
				this->SetFileFrozen(AB_kTrue); // string and file are readonly
				mStringFile_String = newString;
				this->SetFileActive(AB_kTrue);
				this->SetFileIoOpen(AB_kTrue);
			}
			else
				newString->ReleaseObject(ev);
		}
	}
}

void ab_StringFile::CloseStringFile(ab_Env* ev) /*i*/
{
	ab_File_BeginMethod(this, ev, ab_StringFile_kClassName, "CloseStringFile")
	
	ab_Object* obj = mStringFile_String;
	if ( obj )
	{
		mStringFile_String = 0;
		obj->ReleaseObject(ev);
	}
	this->SetFileActive(AB_kFalse);
	this->SetFileIoOpen(AB_kFalse);

	ab_File_EndMethod(this, ev)
}

	
/* ````` ````` ````` C APIs ````` ````` ````` */
	
/* ````` file refcounting ````` */

AB_API_IMPL(ab_ref_count) /* abstore.cpp */
AB_File_Acquire(AB_File* self, AB_Env* cev) /*i*/
{
	ab_ref_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_File_kClassName, "Acquire")

	outCount = ((ab_File*) self)->AcquireObject(ev);
	
	ab_Env_EndMethod(ev)
	return outCount;
}

AB_API_IMPL(ab_ref_count) /* abstore.cpp */
AB_File_Release(AB_File* self, AB_Env* cev) /*i*/
{
	ab_ref_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_File_kClassName, "Release")

	outCount = ((ab_File*) self)->ReleaseObject(ev);
	
	ab_Env_EndMethod(ev)
	return outCount;
}
