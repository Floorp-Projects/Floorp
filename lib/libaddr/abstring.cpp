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

/* file: abstring.cpp
** Some portions derive from public domain IronDoc code and interfaces.
**
** Changes:
** <1> 30Jan1998 first implementation
** <0> 23Dec1997 first interface draft
*/

#ifndef _ABTABLE_
#include "abtable.h"
#endif

#ifndef _ABMODEL_
#include "abmodel.h"
#endif

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/* ===== ===== ===== ===== ab_StringSink ===== ===== ===== ===== */

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_StringSink_kClassName /*i*/ = "ab_StringSink";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

void ab_StringSink::sink_spill_putc(ab_Env* ev, int c) /*i*/
{
	ab_Env_BeginMethod(ev, ab_StringSink_kClassName, "sink_spill_putc")
	
	if ( c ) // not a null byte? do we want this byte in the buffer?
	{
		this->FlushStringSink(ev);
		if ( ev->Good() && mStringSink_At < mStringSink_End )
			this->Putc(ev, c);
	}
	ab_Env_EndMethod(ev)
}

ab_StringSink::ab_StringSink(ab_Env* ev, ab_String* ioString) /*i*/
	: mStringSink_String( ioString ),
	mStringSink_At( mStringSink_Buf ),
	mStringSink_End( mStringSink_Buf + ab_StringSink_kBufSize )
{
	ab_Env_BeginMethod(ev, ab_StringSink_kClassName, ab_StringSink_kClassName)
	
	if ( ioString )
	{
		if ( ioString->IsOpenObject() )
		{
			// okay, this instance looks fine now
		}
		else ev->NewAbookFault(ab_Object_kFaultNotOpen);
	}
	else ev->NewAbookFault(ab_Object_kFaultNullObjectParameter);
	
	ab_Env_EndMethod(ev)
}

void ab_StringSink::FlushStringSink(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_StringSink_kClassName, "FlushStringSink")

	ab_u1* buf = mStringSink_Buf;
	ab_u1* cursor = mStringSink_At;
	if ( cursor >= buf && cursor <= mStringSink_End ) // expected order?
	{
		ab_num count = cursor - buf; // the number of bytes buffered
		if ( count ) // anything to write to the string?
		{
			if ( count > ab_StringSink_kBufSize ) // no more than expected max?
			{
				count = ab_StringSink_kBufSize;
				mStringSink_End = buf + ab_StringSink_kBufSize;
				ev->NewAbookFault(ab_String_kFaultBadCursorFields);
			}
			if ( ev->Good() )
			{
				buf[ count ] = 0; // terminate with null byte before append
				mStringSink_String->AppendStringContent(ev, (char*) buf);
				mStringSink_At = buf; // reset the buffer to empty
			}
		}
	}
	else ev->NewAbookFault(ab_String_kFaultBadCursorOrder);

	ab_Env_EndMethod(ev)
}

/* ===== ===== ===== ===== ab_String ===== ===== ===== ===== */

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_String_kClassName /*i*/ = "ab_String";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_String::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	const char* t = "t";
	const char* f = "f";
	const char* heapBased = (mString_IsHeapBased)? t : f;
	const char* readOnly = (mString_IsReadOnly)? t : f;
	sprintf(outXmlBuf,
		"<ab:string:str me=\"^#%lX\" c=\"%.96s\" c:l=\"%lu:%lu\" h:r=\"%.1s:%.1s\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                        // me=\"^%lX\"
		mString_Content,                    // c=\"%.128s\"
		(long) mString_Capacity,            // c:l=\"%lu
		(long) mString_Length,              // :%lu\"
		heapBased,                          // h:r=\"%.1s
		readOnly,                           // :%.1s\"
		(unsigned long) mObject_RefCount,   // rc=\"%lu\"
		this->GetObjectAccessAsString(),    // ac=\"%.9s\"
		this->GetObjectUsageAsString()      // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_String::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseString(ev);
		this->MarkShut();
	}
}

void ab_String::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:string>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PushDepth(ev); // indent following content

		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
		
		if ( mString_Length > 96 ) // more than shown in ObjectAsString()?
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			ioPrinter->Hex(ev, mString_Content, mString_Length, /*pos*/ 0);
		}
				
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab:string>");
#endif /*AB_CONFIG_PRINT*/
}

static const char* ab_String_k_static_empty_string = "";

ab_String::~ab_String() /*i*/
{
	AB_ASSERT(mString_Content==ab_String_k_static_empty_string);
}
        
// ````` ````` ````` `````   ````` ````` ````` `````  
// protected non-poly ab_String methods

ab_bool ab_String::cut_string_content(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_String_kClassName, "cut_string_content")

	char* content = mString_Content;
	if ( content && mString_IsHeapBased )
		ev->HeapFree(content);

	mString_Content = (char*) ab_String_k_static_empty_string;
	mString_Length = 0;
	mString_Capacity = 1;
	mString_IsHeapBased = AB_kFalse;
	mString_IsReadOnly = AB_kTrue;
	
	ab_Env_EndMethod(ev)
	return ev->Good();
}

#define ab_String_k_grow_slop 32

ab_bool ab_String::grow_string_content(ab_Env* ev, ab_num inNewCapacity) /*i*/
{
	ab_Env_BeginMethod(ev, ab_String_kClassName, "grow_string_content")
	
	if ( mString_IsReadOnly && inNewCapacity < mString_Capacity )
		inNewCapacity = mString_Capacity + 1;

	if ( inNewCapacity > mString_Capacity )
	{
		if ( inNewCapacity + ab_String_k_grow_slop < ab_String_kMaxCapacity )
			inNewCapacity += ab_String_k_grow_slop; // grow a bit more

		if ( inNewCapacity <= ab_String_kMaxCapacity )
		{
			ab_num length = XP_STRLEN(mString_Content);
			if ( length != mString_Length )
			{
#ifdef AB_CONFIG_TRACE
				if ( ev->DoTrace() )
					this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/
#ifdef AB_CONFIG_DEBUG
				this->BreakObject(ev);
#endif /*AB_CONFIG_DEBUG*/
			}
			
			if ( mString_Capacity > mString_Length )
			{
				char* newContent = (char*) ev->HeapAlloc(inNewCapacity);
				if ( newContent )
				{
					XP_STRCPY(newContent, mString_Content);
					this->cut_string_content(ev);
					
					mString_Content = newContent;
					mString_Capacity = inNewCapacity;
					mString_Length = length;
					mString_IsHeapBased = AB_kTrue;
					mString_IsReadOnly = AB_kFalse;
				}
			}
			else ev->NewAbookFault(ab_String_kFaultLengthExceedsSize);
		}
		else ev->NewAbookFault(ab_String_kFaultOverMaxCapacity);
	}
	
	ab_Env_EndMethod(ev)
	return ev->Good();
}

#ifdef AB_CONFIG_DEBUG
	void ab_String::fit_string_content(ab_Env* ev) /*i*/
	{
		ab_num length = XP_STRLEN(mString_Content);
		if ( length != mString_Length ) // size is wrong??
		{
			ab_Env_BeginMethod(ev, ab_String_kClassName, ab_String_kClassName)
			
#ifdef AB_CONFIG_TRACE
			if ( ev->DoTrace() ) // are we tracing
				this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/
			this->BreakObject(ev);
			
			if ( length >= mString_Capacity ) // length exceeds size??
			{
				if ( mString_Capacity ) // string has nonzero size
					length = mString_Capacity - 1; // one less for null byte
				else
					length = 0;
					
				if ( !mString_IsReadOnly ) // can we write the content?
					mString_Content[ length ] = 0; // fix null byte
			}
			mString_Length = length;
			
			ab_Env_EndMethod(ev)
		}
	}
#endif /*AB_CONFIG_DEBUG*/
        
// ````` ````` ````` `````   ````` ````` ````` `````  
// public non-poly ab_String methods

ab_String::ab_String(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	const char* inCopiedString)
	// Make a heap based copy of inCopiedString, unless inCopiedString is
	// a nil pointer.  If inCopiedString is a nil pointer, then
	// mString_Content gets an empty static string, mString_IsReadOnly
	// gets true, and mString_IsHeapBased gets false.  To construct a new
	// string that uses a static string, the best approach is to pass a nil
	// pointer for inCopiedString, and then call TakeStaticStringContent().
	
	: ab_Object(inUsage)
{
	ab_Env_BeginMethod(ev, ab_String_kClassName, ab_String_kClassName)

	mString_Content = 0;
	
	if ( inCopiedString )
	{
		mString_Length = XP_STRLEN(inCopiedString);
		mString_Capacity = mString_Length + 1;
		if ( mString_Capacity <= ab_String_kMaxCapacity )
		{
			mString_Content = (char*) ev->HeapAlloc(mString_Capacity);
			if ( mString_Content )
			{
				XP_STRCPY(mString_Content, inCopiedString);
				mString_IsHeapBased = AB_kTrue;
				mString_IsReadOnly = AB_kFalse;
			}
			// else ev->NewAbookFault(AB_Env_kFaultOutOfMemory);
		}
		else ev->NewAbookFault(ab_String_kFaultOverMaxCapacity);
	}
	else
	{
		mString_Content = (char*) ab_String_k_static_empty_string;
		mString_Length = 0;
		mString_Capacity = 1;
		mString_IsHeapBased = AB_kFalse;
		mString_IsReadOnly = AB_kTrue;
	}

	if ( !mString_Content )
		this->cut_string_content(ev);

	ab_Env_EndMethod(ev)
}

void ab_String::CloseString(ab_Env* ev) /*i*/
	// Nearly equivalent to calling TakeStaticStringContent(ev, "");
{
	ab_Env_BeginMethod(ev, ab_String_kClassName, "CloseString")

	this->cut_string_content(ev);
	
	ab_Env_EndMethod(ev)
}

ab_bool ab_String::TakeStaticStringContent(ab_Env* ev, /*i*/
	const char* inStaticString)
	// Use this readonly, non-heap string instead for mString_Content.
	// Of course this involves casting away const.  Note the content
	// string should never be written other than using supplied methods,
	// and writing a readonly string will involve first replacing the
	// readonly copy with a heap allocated mutable copy before changes.
	// The return value equals ev->Good().
{
	ab_Env_BeginMethod(ev, ab_String_kClassName, "TakeStaticStringContent")

	if ( this->IsOpenObject() )
	{
		if ( this->cut_string_content(ev) && inStaticString )
		{
			mString_Content = (char*) inStaticString;
			mString_Length = XP_STRLEN(inStaticString);
			mString_Capacity = mString_Length + 1;
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);
	
	ab_Env_EndMethod(ev)
	return ev->Good();
}
	    
ab_bool ab_String::TruncateStringLength(ab_Env* ev, ab_num inNewLength) /*i*/
	// Make the string have length inNewLength if this is less than
	// the current length.  The typical expected new length is zero.
	// The capacity of the string is *not* changed, and this is the
	// principle difference between TruncateStringLength() and cutting
	// content with CutStringContent() which might resize the string,
	// and especially does so for empty strings.  TruncateStringLength()
	// is intended to be useful for rewriting strings from scratch without
	// performing any memory allocation that can be avoided. This lets a
	// string buffer reach a high water mark and stay stable for a while.
	// The string should not be a readonly static string if nonempty.
	// The return value equals ev->Good().
{
	ab_Env_BeginMethod(ev, ab_String_kClassName, "TruncateStringLength")

	if ( this->IsOpenObject() )
	{
		if ( inNewLength < mString_Length && !mString_IsReadOnly )
		{
			mString_Length = inNewLength;
			if ( inNewLength < mString_Capacity && mString_Content )
				mString_Content[ inNewLength ] = 0; // move up null termination
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);
	
	ab_Env_EndMethod(ev)
	return ev->Good();
}

// ````` ````` sensible API features currently unused  ````` `````
#ifdef AB_CONFIG_MAXIMIZE_FEATURES    
ab_bool ab_String::SetStringContent(ab_Env* ev, const char* inString) /*i*/
	// Make the new string's content equal to inString, with length
	// strlen(inString).  If this fits inside the old capacity and the
	// string is mutable, then mString_Content is modified; otherwise
	// mString_Content is freed and a new heap-based string is allocated.
	// The return value equals ev->Good().
{
	ab_Env_BeginMethod(ev, ab_String_kClassName, "SetStringContent")
	
	// This method has never been run (delete this comment after testing)

	if ( this->IsOpenObject() )
	{
		if ( inString )
		{
			ab_num length = XP_STRLEN(inString);
			ab_num inCapacity = length + 1;
			
			if ( inCapacity > mString_Capacity || mString_IsReadOnly )
			{
				if ( inCapacity <= ab_String_kMaxCapacity )
				{
					char* newContent = (char*) ev->HeapAlloc(inCapacity);
					if ( newContent )
					{
						XP_STRCPY(newContent, inString);
						this->cut_string_content(ev);
						
						mString_Content = newContent;
						mString_Capacity = inCapacity;
						mString_Length = length;
						mString_IsHeapBased = AB_kTrue;
						mString_IsReadOnly = AB_kFalse;
					}
				}
				else ev->NewAbookFault(ab_String_kFaultOverMaxCapacity);
			}
			else
			{
				XP_STRCPY(mString_Content, inString);
				mString_Length = length;
			}
		}
		else
			this->cut_string_content(ev);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);
	
	ab_Env_EndMethod(ev)
	return ev->Good();
}
#endif /*AB_CONFIG_MAXIMIZE_FEATURES*/

ab_bool ab_String::PutBlockContent(ab_Env* ev, const char* inBytes, /*i*/
	ab_num inSize, ab_pos inPos)
	// PutBlockContent() is the same as PutStringContent() except that
	// inSize is used as the string length instead of finding any ending
	// null byte.  This is very useful when it is inconvenient to put a
	// null byte into an input string (which might be readonly) in order
	// to end the string.  While this interface allows the input string to
	// contain null bytes, this is *still a very bad idea* because ab_String
	// cannot correctly handle embedded null bytes.  You've been warned.
	//
	// Overwrite the string starting at offset inPos.  An error occurs if
	// inPos is not a value from zero up to and including mString_Length.
	// When inPos equals the length, this operation appends to the string.
	// If the string is readonly or if the string must be longer to hold
	// the new length of the string (plus null terminator), then the
	// string is grown by replacing mString_Content with bigger space.
  	// The return value equals ev->Good().
{
	ab_Env_BeginMethod(ev, ab_String_kClassName, "PutBlockContent")

	if ( this->IsOpenObject() )
	{
		if ( inPos <= mString_Length )
		{
			ab_num minLength = inPos + inSize;
			ab_num minCapacity = minLength + 1;
			if ( minCapacity > mString_Capacity || mString_IsReadOnly )
				this->grow_string_content(ev, minCapacity);
				
			if ( ev->Good() && inSize )
			{
				AB_MEMCPY(mString_Content + inPos, inBytes, inSize);
				if ( minLength > mString_Length )
				{
					mString_Length = minLength;
					mString_Content[ minLength ] = 0;
				}
			}
		}
		else ev->NewAbookFault(ab_String_kFaultPosAfterLength);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);
	
	ab_Env_EndMethod(ev)
	return ev->Good();
}

#ifdef AB_CONFIG_MAXIMIZE_FEATURES    
ab_bool ab_String::PutStringContent(ab_Env* ev, const char* inString, /*i*/
	ab_pos inPos)
	// Overwrite the string starting at offset inPos.  An error occurs if
	// inPos is not a value from zero up to and including mString_Length.
	// When inPos equals the length, this operation appends to the string.
	// If the string is readonly or if the string must be longer to hold
	// the new length of the string (plus null terminator), then the
	// string is grown by replacing mString_Content with bigger space.
  	// The return value equals ev->Good().
{
	ab_Env_BeginMethod(ev, ab_String_kClassName, "PutStringContent")
	
	// This method has never been run (delete this comment after testing)

	if ( this->IsOpenObject() )
	{
#ifdef AB_CONFIG_DEBUG
		this->fit_string_content(ev); // paranoid check
#endif /*AB_CONFIG_DEBUG*/

		if ( inPos <= mString_Length )
		{
			ab_num size = XP_STRLEN(inString);
			ab_num minLength = inPos + size;
			ab_num minCapacity = minLength + 1;
			if ( minCapacity > mString_Capacity || mString_IsReadOnly )
				this->grow_string_content(ev, minCapacity);
				
			if ( ev->Good() && size )
			{
				AB_MEMCPY(mString_Content + inPos, inString, size);
				if ( minLength > mString_Length )
				{
					mString_Length = minLength;
					mString_Content[ minLength ] = 0;
				}
			}
		}
		else ev->NewAbookFault(ab_String_kFaultPosAfterLength);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
	return ev->Good();
}
#endif /*AB_CONFIG_MAXIMIZE_FEATURES*/

ab_bool ab_String::AddStringContent(ab_Env* ev, const char* inString, /*i*/
	ab_pos inPos)
	// Insert new content starting at offset inPos.  An error occurs if
	// inPos is not a value from zero up to and including mString_Length.
	// When inPos equals the length, this operation appends to the string.
	// If the string is readonly or if the string must be longer to hold
	// the new length of the string (plus null terminator), then the
	// string is grown by replacing mString_Content with bigger space.
	// The return value equals ev->Good().
{
	ab_Env_BeginMethod(ev, ab_String_kClassName, "AddStringContent")
	
	// This method has never been run (delete this comment after testing)

	if ( this->IsOpenObject() )
	{
#ifdef AB_CONFIG_DEBUG
		this->fit_string_content(ev); // paranoid check
#endif /*AB_CONFIG_DEBUG*/

		if ( inPos <= mString_Length )
		{
			ab_num size = AB_STRLEN(inString);
			ab_num newLength = mString_Length + size; // no overlap
			ab_num minCapacity = newLength + 1;
			if ( minCapacity > mString_Capacity || mString_IsReadOnly )
				this->grow_string_content(ev, minCapacity);
				
			if ( ev->Good() && size )
			{
				char* dest = mString_Content + inPos; // insertion point
				
				ab_num after = mString_Length - inPos; // bytes after insert pt
				if ( after ) // bytes to move forward before insertion?
					AB_MEMMOVE(dest, dest + size, after);
				
				AB_MEMCPY(dest, inString, size);
				if ( newLength > mString_Length )
				{
					mString_Length = newLength;
					mString_Content[ newLength ] = 0;
				}
			}
		}
		else ev->NewAbookFault(ab_String_kFaultPosAfterLength);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
	return ev->Good();
}

#ifdef AB_CONFIG_MAXIMIZE_FEATURES    
ab_bool ab_String::CutStringContent(ab_Env* ev, ab_pos inPos, /*i*/
	 ab_num inCount)
	// Remove inCount bytes of content from the string starting at offset
	// inPos, where inPos must be a value from zero up to the length of
	// the string.  inCount can be any value, since any excess simply
	// ensures that all content through the end of the string will be cut.
	// So CutStringContent(ev, 0, 0xFFFFFFFF) is roughly equivalent to
	// SetStringContent(ev, "");
	// The return value equals ev->Good().
{
	ab_Env_BeginMethod(ev, ab_String_kClassName, "CutStringContent")
	
	// This method has never been run (delete this comment after testing)

	if ( this->IsOpenObject() )
	{
#ifdef AB_CONFIG_DEBUG
		this->fit_string_content(ev); // paranoid check
#endif /*AB_CONFIG_DEBUG*/

		if ( inPos <= mString_Length )
		{
			ab_num after = mString_Length - inPos; // bytes after cut point
			if ( after && inCount ) // anything to be cut?
			{
				if ( !inPos && inCount >= mString_Length ) // cut everything?
				{
					this->cut_string_content(ev);
				}
				else
				{
					if ( mString_IsReadOnly ) // need a mutable copy of content?
					{
						// yes, we make it larger, but this is an odd case
						this->grow_string_content(ev, mString_Capacity + 1);
					}
						
					if ( ev->Good() )
					{
						char* dest = mString_Content + inPos; // cut point
						if ( inCount < after ) // cut makes a hole in content? 
						{
							ab_num tail = after - inCount; // count after hole
							AB_MEMMOVE(dest, dest + inCount, tail); // close hole
						}
						else // cut all after inPos, so inPos is new length
						{
							mString_Length = inPos;
							*dest = 0;
						}
					}
				}
			}
		}
		else ev->NewAbookFault(ab_String_kFaultPosAfterLength);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
	return ev->Good();
}
#endif /*AB_CONFIG_MAXIMIZE_FEATURES*/
