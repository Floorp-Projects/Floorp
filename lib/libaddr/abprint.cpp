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

#ifndef _ABTABLE_
#include "abtable.h"
#endif

#ifndef _ABMODEL_
#include "abmodel.h"
#endif

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_Printer_kClassName /*i*/ = "ab_Printer";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_FilePrinter_kClassName /*i*/ = "ab_FilePrinter";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

/* ===== ===== ===== ===== ab_Printer ===== ===== ===== ===== */

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

ab_Printer::~ab_Printer() /*i*/
{
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_Printer methods

void ab_Printer::Print(ab_Env* ev, const char* inFormat, ...) /*i*/
{
#ifdef AB_CONFIG_PRINT
	if ( this->IsOpenObject() )
	{
		char buf[ AB_Env_kFormatBufferSize ];
		va_list args;
		va_start(args,inFormat);
		PR_vsnprintf(buf, AB_Env_kFormatBufferSize, inFormat, args);
		va_end(args);
		    
		this->PutString(ev, buf);
	}
#ifdef AB_CONFIG_DEBUG
	else
	{
		ab_Env_BeginMethod(ev, ab_Printer_kClassName, "Print")
	
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
			this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/
	
#ifdef AB_CONFIG_DEBUG
		ev->NewAbookFault(ab_Object_kFaultNotOpen);
#endif /*AB_CONFIG_DEBUG*/

		ab_Env_EndMethod(ev)
	}
#endif /*AB_CONFIG_DEBUG*/

#endif /*AB_CONFIG_PRINT*/
}

ab_Printer::ab_Printer(const ab_Usage& inUsage) /*i*/
	: ab_Object(inUsage),
	mPrinter_Depth( 0 )
{
}

// ~~~~~ ~~~~~ ~~~~~ ~~~~~  begin DefaultPrinterHex() ~~~~~ ~~~~~ ~~~~~ ~~~~~
// NOTE: DefaultPrinterHex() copies public domain IronDoc almost verbatim.

/*| kHexDigits: array of hexadecimal bytes useful for hex conversion. 
|*/
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
const char* ab_Printer_kHexDigits = "0123456789ABCDEF";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

/*| hex_digit_count: number of hexadecimal digits needed to write inPos.
|*/
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
  static ab_num  // (near verbatim copy of IronDoc's FePos_HexDigitCount)
  ab_Pos_hex_digit_count(ab_pos self)
  {
    ab_num count = 0;
    if ( self ) /* are we considering a non-zero value? */
    {
      if ( self > 0x0FFFF ) /* more than four digits? */
      {
        count += 4;
        self = (self >> 16) & 0x0FFFF; /* AND to avoid sign extension */
      }
      if ( self > 0x0FF ) /* at least two more digits? */
      {
        count += 2;
        self >>= 8;
      }
      if ( self > 0x0F ) /* at least one more digit? */
      {
        count += 1;
        self >>= 4;
      }
      if ( self )
        count += 1;
    }
    else
      count = 1; /* zero requires one digit to represent: "0" */
      
    return count;
  }
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

/*| as_hex: write self in hex to outBuf$ assuming that self takes
**| hexDigitCount$ digits to write in hexadecimal.  Presumably the
**| caller has called ab_Pos_hex_digit_count() for the maximum value of
**| self that will be used and knows how much space to use. Obviously
**| outBuf must point to at least hexDigitCount$ bytes of space.
**|
**|| The hexadecimal digits are written backwards, since this is an easy
**| way to do this without getting confused.  as_hex is intended to be
**| a safely faster way to format in hexadecimal than by using sprintf().
|*/
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
  static char* // (near verbatim copy of IronDoc's FePos_AsHex)
  ab_Pos_as_hex(ab_pos self, char* outBuf, ab_num hexDigitCount) /*i*/
  {
    if ( hexDigitCount > 16 ) /* does digit count exceed all reason? */
      hexDigitCount = 16; /* clamp to a more reasonable value */
    
    if ( hexDigitCount ) /* does the caller want to write anything? */
    {
      char* p = outBuf + hexDigitCount; /* one past spot for trailing digit */
      
      while ( --p >= outBuf ) /* have not yet written leading digit? */
      {
        *p = ab_Printer_kHexDigits[ self & 0x0F ]; /* least sig digit */
        self >>= 4; /* shift smaller by one hex digit */
      }
    }
    return outBuf;
  }
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

/* ````` design for ab_Printer::DefaultPrinterHex() output format `````
**  <ab:hex:seq size=42 pos="#00000400">
**    <ab:x x="74776173 20627269 6C6C6967 20616E64" a="twas brillig and" p=400/>
**    <ab:x x="20746865 20736C69 74687920 746F7665" a=" the slithy tove" p=410/>
**    <ab:x x="73206469 64206779 7265"              a="s did gyre"       p=420/>
**  </ab:hex:seq>
**
** 0123456789_123456789_123456789_123456789_123456789_123456789_123456789_
** ........|<-------------- 35 --------------->|...|<----- 16 ----->|
** <ab:x x="20746865 20736C69 74687920 746F7665" a=" the slithy tove" p=410/>
** .........^9......................................^49.................^69
**
**  <ab:hex:seq len=36 pos="#400" key="x:hex,a:ascii,p:pos">
**  </ab:hex:seq>
*/

#define ab_Printer_kAsciiRepIndex /*i*/ 49     // index of ascii representation
#define ab_Printer_kAsciiReplength /*i*/ 16   // length of ascii representation
#define ab_Printer_kHexRepIndex /*i*/ 9          // index of hex representation
#define ab_Printer_kHexRepLength /*i*/ 35       // length of hex representation

#define ab_Printer_kPosRepIndex /*i*/ 69         // index of pos representation
#define ab_Printer_kPosRepMaxLength /*i*/ 8   // max size of pos representation

#define ab_Printer_kHexBytesPerGroup /*i*/ 4   // bytes to group without spaces
#define ab_Printer_kHexPerLine /*i*/ 16             // bytes to format per line
#define ab_Printer_kHexLineBufferSize /*i*/ 128          // size of line buffer
#define ab_Printer_kMaxHexSize /*i*/ ( 256 * 1024 )    // max (to catch errors)

/*| kHexLineTemplate: formatted sample line to initialize non-variable parts. 
|*/
#ifdef AB_CONFIG_PRINT
static const char* ab_Printer_kHexLineTemplate = /*i*/
"<ab:x x=\"74776173 20627269 6C6C6967 20616E64\" a=\"twas brillig and\" p=12345678/>";
#endif /*AB_CONFIG_PRINT*/

void ab_Printer::DefaultPrinterHex(ab_Env* ev, const void* inBuf, /*i*/
	ab_num inSize, ab_pos inPos)
	// DefaultPrinterHex() is a good default implementation for Hex()
{
#ifdef AB_CONFIG_PRINT
  char posXmlBuf[ 32 ];
  char lineBuf[ ab_Printer_kHexLineBufferSize + 2 ];  // line format buffer

  ab_num maxPosDigits = ab_Pos_hex_digit_count(inPos + inSize);
  XP_SPRINTF(posXmlBuf, "#%08lX", inPos);

  if ( inSize <= ab_Printer_kMaxHexSize )  // reasonable requested size?
  {
    if ( inSize ) // is there any content to write in hex format?
    {
      char* hex = lineBuf + ab_Printer_kHexRepIndex; // hex format goes here
      char* ascii = lineBuf + ab_Printer_kAsciiRepIndex; // ascii format here
      char* pos = lineBuf + ab_Printer_kPosRepIndex; // position goes here

      ab_num quantum = ab_Printer_kHexPerLine; // bytes to write per line
      ab_num localPos = 0; // relative to beginning of inBuf
      ab_bool needEndQuotes = AB_kFalse; // need to write end quotes?
      
      this->Print(ev, "<ab:hex:seq size=\"%lu\" pos=\"%.32s\">",
        (unsigned long) inSize, posXmlBuf); // begin enclosing tag
        
      XP_STRCPY(lineBuf, ab_Printer_kHexLineTemplate); // init constant parts
      if ( maxPosDigits < 8 ) // need to move up the "/>" end of tag?
      {
        char* p = pos + maxPosDigits; // first byte after pos
        *p++ = '/';
        *p++ = '>';
        *p++ = '\0';
      }
      this->PushDepth(ev); // indent all the hex tags
      
      for ( ; inSize && ev->Good(); localPos += ab_Printer_kHexPerLine )
      {
        ab_num count = quantum; // bytes to write for this line 
        int perGroup = ab_Printer_kHexBytesPerGroup; // chars before a blank 
        const char* source = ((const char*) inBuf) + localPos;

        hex = lineBuf + ab_Printer_kHexRepIndex; // reset hex cursor
        ascii = lineBuf + ab_Printer_kAsciiRepIndex; // reset ascii cursor
        
        if ( inSize <  ab_Printer_kHexPerLine ) // short partial last line?
        {
          quantum = count = inSize; // don't write more bytes than remain
          needEndQuotes = AB_kTrue; // line is short, so move up end quote
          // for short line, write all blanks and wipe the old end quotes:
          XP_MEMSET(hex, ' ', ab_Printer_kHexRepLength + 1);
          XP_MEMSET(ascii, ' ', ab_Printer_kAsciiReplength + 1);
        }
        inSize -= quantum;
        
        // write the variable hex formatting for this line:
        while ( count ) // more hex to write for this line?
        {
          register char c = *source++; // next char to format
          *hex++ = (char) ab_Printer_kHexDigits[ (c>>4) & 0x0F ];
          *hex++ = (char) ab_Printer_kHexDigits[ c & 0x0F ];
          if ( --count && !--perGroup ) // more line but group is done?
          {
              hex++; // leave a blank between groups
              perGroup = ab_Printer_kHexBytesPerGroup;
          }
          if ( c > 0x7F || !isprint(c) ) // nonprint char?
            c = (char) '.'; // just use a period
          *ascii++ = c;
        }
        if ( needEndQuotes ) // working on the last short line?
        {
          *hex++ = '"';
          *ascii++ = '"';
        }
        
        // write the pos after "p=":
        ab_Pos_as_hex(localPos + inPos, pos, maxPosDigits);
        
        // now display the formatted line buffer:
        this->NewlineIndent(ev, /*newlines*/ 1); // prep next line
        this->PutString(ev, lineBuf); // write <ab:x .../> tag
      }
      this->PopDepth(ev);
      this->NewlineIndent(ev, /*newlines*/ 1);
      this->PutString(ev, "</ab:hex:seq>"); // now end enclosing tag
    }
    else
      this->Print(ev, "<ab:hex:seq size=\"0\" pos=\"%.32s\"/>", posXmlBuf);
  }
  else ev->NewAbookFault(ab_Printer_kFaultHexSizeTooBig );
#endif /*AB_CONFIG_PRINT*/
}
// ~~~~~ ~~~~~ ~~~~~ ~~~~~  end DefaultPrinterHex() ~~~~~ ~~~~~ ~~~~~ ~~~~~

/* ===== ===== ===== ===== ab_FilePrinter ===== ===== ===== ===== */

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Printer methods

void ab_FilePrinter::PutString(ab_Env* ev, const char* inMessage) /*i*/
{
#ifdef AB_CONFIG_PRINT
	static const char* kMethodPutString = "PutString";
	if ( this->IsOpenObject() )
	{
		ab_File* file = mFilePrinter_File;
		if ( file && file->IsOpenObject() )
		{
			file->Write(ev, inMessage, XP_STRLEN(inMessage));
		}
#ifdef AB_CONFIG_DEBUG
		else this->file_printer_bad_file(ev, kMethodPutString);
#endif /*AB_CONFIG_DEBUG*/
	}
#ifdef AB_CONFIG_DEBUG
	else this->file_printer_not_open(ev, kMethodPutString);
#endif /*AB_CONFIG_DEBUG*/

#endif /*AB_CONFIG_PRINT*/
}

void ab_FilePrinter::Newline(ab_Env* ev, ab_num inNewlines) /*i*/
{
#ifdef AB_CONFIG_PRINT
	static const char* kMethodNewline = "Newline";
	if ( this->IsOpenObject() )
	{
		ab_File* file = mFilePrinter_File;
		if ( file && file->IsOpenObject() )
		{
			file->WriteNewlines(ev, inNewlines);
		}
#ifdef AB_CONFIG_DEBUG
		else this->file_printer_bad_file(ev, kMethodNewline);
#endif /*AB_CONFIG_DEBUG*/
	}
#ifdef AB_CONFIG_DEBUG
	else this->file_printer_not_open(ev, kMethodNewline);
#endif /*AB_CONFIG_DEBUG*/

#endif /*AB_CONFIG_PRINT*/
}

#define ab_FilePrinter_kBlanksLength /*i*/ 32 /* static blank string length */

void ab_FilePrinter::file_printer_indent(ab_Env* ev) const /*i*/
{	
	static const char* kBlanks = "                                ";
	/* comment to count blanks -- 123456789 123456789 123456789 12 */

	ab_num remaining = mPrinter_Depth * 2; /* two blanks per indent */
	if ( remaining > 1024 ) /* more than some reasonable upper bound? */
		remaining = 1024;

	while ( remaining && ev->Good() ) /* more blanks to write? */
	{
		ab_num quantum = remaining;
		if ( quantum > ab_FilePrinter_kBlanksLength )
			quantum = ab_FilePrinter_kBlanksLength;
		  
		mFilePrinter_File->Write(ev, kBlanks, quantum);
		remaining -= quantum;
	}
}

void ab_FilePrinter::NewlineIndent(ab_Env* ev, ab_num inNewlines) /*i*/
{
#ifdef AB_CONFIG_PRINT
	static const char* kMethodNewlineIndent = "NewlineIndent";
	if ( this->IsOpenObject() )
	{
		ab_File* file = mFilePrinter_File;
		if ( file && file->IsOpenObject() )
		{
			file->WriteNewlines(ev, inNewlines);
			if ( ev->Good() )
				this->file_printer_indent(ev);
		}
#ifdef AB_CONFIG_DEBUG
		else this->file_printer_bad_file(ev, kMethodNewlineIndent);
#endif /*AB_CONFIG_DEBUG*/
	}
#ifdef AB_CONFIG_DEBUG
	else this->file_printer_not_open(ev, kMethodNewlineIndent);
#endif /*AB_CONFIG_DEBUG*/

#endif /*AB_CONFIG_PRINT*/
}

void ab_FilePrinter::Flush(ab_Env* ev) /*i*/
{
#ifdef AB_CONFIG_PRINT
	static const char* kMethodFlush = "Flush";
	if ( this->IsOpenObject() )
	{
		ab_File* file = mFilePrinter_File;
		if ( file && file->IsOpenObject() )
		{
			file->Flush(ev);
		}
#ifdef AB_CONFIG_DEBUG
		else this->file_printer_bad_file(ev, kMethodFlush);
#endif /*AB_CONFIG_DEBUG*/
	}
#ifdef AB_CONFIG_DEBUG
	else this->file_printer_not_open(ev, kMethodFlush);
#endif /*AB_CONFIG_DEBUG*/

#endif /*AB_CONFIG_PRINT*/
}

void ab_FilePrinter::Hex(ab_Env* ev, const void* inBuf, ab_num inSize, /*i*/
	 ab_pos inPos)
{
#ifdef AB_CONFIG_PRINT
	static const char* kMethodHex = "Hex";
	if ( this->IsOpenObject() )
	{
		ab_File* file = mFilePrinter_File;
		if ( file && file->IsOpenObject() )
		{
			this->DefaultPrinterHex(ev, inBuf, inSize, inPos);
		}
#ifdef AB_CONFIG_DEBUG
		else this->file_printer_bad_file(ev, kMethodHex);
#endif /*AB_CONFIG_DEBUG*/
	}
#ifdef AB_CONFIG_DEBUG
	else this->file_printer_not_open(ev, kMethodHex);
#endif /*AB_CONFIG_DEBUG*/

#endif /*AB_CONFIG_PRINT*/
}

void ab_FilePrinter::file_printer_not_open(ab_Env* ev, /*i*/
	const char* inMethod) const
{
	ab_Env_BeginMethod(ev, ab_FilePrinter_kClassName, inMethod)
	
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/
	
	ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
}

void ab_FilePrinter::file_printer_bad_file(ab_Env* ev, /*i*/
	const char* inMethod) const
{
	ab_Env_BeginMethod(ev, ab_FilePrinter_kClassName, inMethod)
	
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/
	
	if ( mFilePrinter_File )
	{
		if ( mFilePrinter_File->IsOpenObject() )
		{
			ev->NewAbookFault(ab_Printer_kFaultConfused);
		}
		else ev->NewAbookFault(ab_Printer_kFaultFileNotOpen);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	this->CastAwayConstCloseObject(ev);

	ab_Env_EndMethod(ev)
}


ab_num ab_FilePrinter::PushDepth(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		++mPrinter_Depth;
	}
#ifdef AB_CONFIG_DEBUG
	else this->file_printer_not_open(ev, "PushDepth");
#endif /*AB_CONFIG_DEBUG*/

	return mPrinter_Depth;
}

ab_num ab_FilePrinter::PopDepth(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() && mPrinter_Depth )
	{
		--mPrinter_Depth;
	}
#ifdef AB_CONFIG_DEBUG
	else this->file_printer_not_open(ev, "PopDepth");
#endif /*AB_CONFIG_DEBUG*/

	return mPrinter_Depth;
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_FilePrinter::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	XP_SPRINTF(outXmlBuf,
		"<ab:part:str me=\"^%lX\" pd=\"%lu\" pf=\"^%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                                // me=\"^%lX\"
		(long) mPrinter_Depth,                      // pd=\"%lu\"
		(long) mFilePrinter_File,                   // pf=\"^%lX\"
		(unsigned long) mObject_RefCount,           // rc=\"%lu\"
		this->GetObjectAccessAsString(),            // ac=\"%.9s\"
		this->GetObjectUsageAsString()              // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_FilePrinter::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseFilePrinter(ev);
		this->MarkShut();
	}
}

void ab_FilePrinter::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:file:printer>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PushDepth(ev); // indent all objects in the list

		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
		
		if ( mFilePrinter_File )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mFilePrinter_File->PrintObject(ev, ioPrinter);
		}
		
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab:file:printer>");
#endif /*AB_CONFIG_PRINT*/
}

ab_FilePrinter::~ab_FilePrinter() /*i*/
{
	AB_ASSERT(mFilePrinter_File==0);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	ab_Object* obj = mFilePrinter_File;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_FilePrinter_kClassName);
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_FilePrinter methods

ab_FilePrinter::ab_FilePrinter(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	 ab_File* ioFile)
	 : ab_Printer(inUsage),
	 mFilePrinter_File( 0 )
{
	ab_Env_BeginMethod(ev, ab_FilePrinter_kClassName, ab_FilePrinter_kClassName)
	
	if ( ev->Good() )
	{
		if ( ioFile )
		{
			if ( ioFile->AcquireObject(ev) )
				mFilePrinter_File = ioFile;
		}
		else ev->NewAbookFault(ab_Object_kFaultNullObjectParameter);
	}
	ab_Env_EndMethod(ev)
}

void ab_FilePrinter::CloseFilePrinter(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_FilePrinter_kClassName, "CloseFilePrinter")

	ab_Object* obj = mFilePrinter_File;
  	if ( obj )
  	{
  		mFilePrinter_File = 0;
  		obj->ReleaseObject(ev);
  	}

	ab_Env_EndMethod(ev)
}

static ab_FilePrinter* ab_FilePrinter_g_log_printer = 0; // GetLogFilePrinter()

/*static*/
ab_FilePrinter* ab_FilePrinter::GetLogFilePrinter(ab_Env* ev) /*i*/
	// standard session printer (uses ab_StdioFile::GetLogStdioFile()).
	// This printer instance is used by ab_Env::GetLogFileEnv().
	// This printer instance is used by ab_FileTracer::GetLogFileTracer().
	// (Please, never close this printer instance.)
{
	ab_FilePrinter* outPrinter = 0;
	ab_Env_BeginMethod(ev, ab_FilePrinter_kClassName, "GetLogFilePrinter")

	outPrinter = ab_FilePrinter_g_log_printer;
	if ( !outPrinter )
	{
		ab_StdioFile* file = ab_StdioFile::GetLogStdioFile(ev);
		if ( file )
		{
			outPrinter =  new(*ev) ab_FilePrinter(ev, ab_Usage::kHeap, file);
			if ( outPrinter )
			{
				ab_FilePrinter_g_log_printer = outPrinter;
				outPrinter->AcquireObject(ev);
			}
		}
	}
	
	if ( outPrinter )
	{
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
			outPrinter->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/
	}
	ab_Env_EndMethod(ev)
	return outPrinter;
}
