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

/* file: abcell.c 
**
** Changes:
** <1> 02Dec1997 stubs
** <0> 22Oct1997 first draft
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
	static const char* AB_Cell_kClassName /*i*/ = "AB_Cell";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


/*| Init: initialize and construct the cell so sCell_Size is at
**| least inMinSize bytes in size, and contains the content pointed
**| to by inStartingContent when inStartingContent is non-null.
**| If inStartingContent is null, then zero the initial content.
**| The inColumnUid arg cannot be zero or an error will occur; in
**| fact, inColumnUid must return true from AB_Uid_IsColumn().
**|
**|| If strlen(inStartingContent)+1 is greater than inMinSize, then
**| make a cell that size instead (the plus one is for a null byte).  If
**| both inMinSize and the inStartingContent pointer are zero, allocate
**| at least a byte to avoid weird edge cases.
|*/
AB_MODEL_IMPL(ab_bool)
AB_Cell_Init(AB_Cell* self, ab_Env* ev, ab_column_uid inColumnUid, /*i*/
	ab_cell_size inMinSize, const char* inStartingContent)
{
	ab_Env_BeginMethod(ev, AB_Cell_kClassName, "Init")
	
	ab_cell_length length = 0; /* default to no content */
	if ( !inMinSize ) /* need to make min size positive? */
		inMinSize = 1;
	if ( inStartingContent ) /* have initial content to use? */
	{
		length = strlen(inStartingContent);
		if ( length+1 > inMinSize ) /* length requires more size? */
			inMinSize = length+1;
	}
	if ( inMinSize <= AB_Cell_kMaxCellSize ) /* not too large? */
	{
		if ( AB_Uid_IsColumn(inColumnUid) ) /* column looks valid? */
		{
			char* content = (char*) ev->HeapSafeTagAlloc(inMinSize);
			if ( content )
			{
				XP_MEMSET(content, 0, inMinSize); /* zero all content bytes*/
				self->sCell_Column = inColumnUid;
				self->sCell_Size = inMinSize;
				self->sCell_Length = length;
				self->sCell_Extent = 0; /* database must set this field */
				self->sCell_Content = content;
				if ( length ) /* any initial content to copy? */
					XP_MEMCPY(content, inStartingContent, length);
			}
		}
	  	else ev->NewAbookFault(AB_Cell_kFaultBadColumnUid);
	}
  	else ev->NewAbookFault(AB_Cell_kFaultSizeExceedsMax);

	ab_Env_EndMethod(ev)
	return ev->Good();
}
/*| Finalize: deallocate sCell_Content and zero all slots.
|*/
AB_MODEL_IMPL(ab_bool)
AB_Cell_Finalize(AB_Cell* self, ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, AB_Cell_kClassName, "Finalize")
	
	if ( self->sCell_Content )
		ev->HeapSafeTagFree(self->sCell_Content);
	
	self->sCell_Column = 0;
	self->sCell_Size = 0;
	self->sCell_Length = 0;
	self->sCell_Extent = 0;
	self->sCell_Content = 0;

	ab_Env_EndMethod(ev)
	return ev->Good();
}

/*| Grow: increase the cell size sCell_Size to at least inMinSize
**| and reallocate the sCell_Content space if necessary.  But if
**| sCell_Size is already that big, then do nothing.
**| True is returned if and only if the environment shows no errors.
|*/
AB_MODEL_IMPL(ab_bool)
AB_Cell_Grow(AB_Cell* self, ab_Env* ev, ab_cell_size inMinSize) /*i*/
{
	ab_Env_BeginMethod(ev, AB_Cell_kClassName, "Grow")
	
	ab_cell_size oldSize = self->sCell_Size;
	if ( oldSize < inMinSize ) /* need to grow content? */
	{
		if ( inMinSize <= AB_Cell_kMaxCellSize ) /* not too large? */
		{
			char* newContent = (char*) ev->HeapSafeTagAlloc(inMinSize);
			if ( newContent ) /* allocated new content? */
			{
				char* oldContent = self->sCell_Content;
				ab_num diffSize = inMinSize - oldSize; /* differential */
				
				char* after = newContent + oldSize; /* after old content */
				XP_MEMSET(after, 0, diffSize); /* clear after copied content */
				if ( oldSize ) /* any old content to copy? */
					XP_MEMCPY(newContent, oldContent, oldSize);
					
				self->sCell_Content = newContent;
				self->sCell_Size = inMinSize;
				
				ev->HeapSafeTagFree(oldContent); /* discard old content space */
			}
	  	}
  	  	else ev->NewAbookFault(AB_Cell_kFaultSizeExceedsMax);
	}

	ab_Env_EndMethod(ev)
	return ev->Good();
}
/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/*| Copy: make this cell contain the same content as other when capacity
**| is greater than other's content.  Otherwise make self hold as much of
**| other's content as will fit without making self any larger.  (This
**| method is intended to support AB_Tuple_CopyRowContent().)
**| True is returned if and only if the environment shows no errors.
|*/
AB_MODEL_IMPL(ab_bool)
AB_Cell_Copy(AB_Cell* self, ab_Env* ev, const AB_Cell* other)
{
	ab_Env_BeginMethod(ev, AB_Cell_kClassName, "Copy")
	
	ab_cell_size selfSize = self->sCell_Size;
	if ( selfSize ) /* receiver has some space as expected? */
	{
		char* selfContent = self->sCell_Content;
		ab_cell_size selfRoom = selfSize - 1; /* less end null byte */
		if ( selfRoom ) /* space for more than terminating null? */
		{
			ab_cell_size otherLength = other->sCell_Length;
			if ( otherLength <= other->sCell_Size ) /* rational sizes? */
			{
				if ( otherLength > selfRoom ) /* too much content? */
					otherLength = selfRoom; /* truncate to our capacity */
				
				XP_MEMCPY(selfContent, other->sCell_Content, otherLength);
				selfContent[ otherLength ] = 0; /* add end null byte */
				
				self->sCell_Length = otherLength;
			}
			else ev->NewAbookFault(AB_Cell_kFaultLengthExceedsSize);
		}
		else 
			selfContent[ 0 ] = 0; /* can only hold empty string */
	}
	else ev->NewAbookFault(AB_Cell_kFaultZeroCellSize);
	
	ab_Env_EndMethod(ev)
	return ev->Good();
}
	
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	AB_MODEL(char*) /* abcell.c */
	AB_Cell_AsString(const AB_Cell* self, ab_Env* ev, char* outBuf) /*i*/
	{
		AB_Env* cev = ev->AsSelf();
		const char* at = AB_ColumnUid_AsString(self->sCell_Column, cev);
		if ( !at )
			at = "<name>";
		XP_SPRINTF(outBuf,
			"<ab:cell:str val=\"%.48s\" at=\"#%lX/%.32s\" sz=\"#%lX\" ln=\"%lu\" ex=\"%lu\"/>",
			self->sCell_Content,       // val=\"%.64s\"
			(long) self->sCell_Column, // at=\"#%lX
			at,                        // %.32s\"
			(long) self->sCell_Size,   // sz=\"#%lX\"
			(long) self->sCell_Length, // ln=\"%lu\"
			(long) self->sCell_Extent  // ex=\"%lu\"
		);
		return outBuf;
	}
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/


#ifdef AB_CONFIG_TRACE
	AB_MODEL_IMPL(void) /* abcell.c */
	AB_Cell_Trace(const AB_Cell* self, ab_Env* ev) /*i*/
	{
		char buf[ AB_Cell_kXmlBufSize ];
		ev->Trace("%.250s", AB_Cell_AsString(self, ev, buf));
	}
#endif /*AB_CONFIG_TRACE*/

#ifdef AB_CONFIG_DEBUG
	AB_MODEL_IMPL(void) /* abcell.c */
	AB_Cell_Break(const AB_Cell* self, ab_Env* ev) /*i*/
	{
		char buf[ AB_Cell_kXmlBufSize ];
		ev->Break("%.250s", AB_Cell_AsString(self, ev, buf));
	}
#endif /*AB_CONFIG_DEBUG*/


AB_MODEL(void) /* abcell.c */
AB_Cell_TraceAndBreak(const AB_Cell* self, ab_Env* ev) /*i*/
{
#ifdef AB_CONFIG_TRACE
	AB_Cell_Trace(self, ev);
#endif /*AB_CONFIG_TRACE*/

#ifdef AB_CONFIG_DEBUG
	AB_Cell_Break(self, ev);
#endif /*AB_CONFIG_DEBUG*/
}

AB_MODEL_IMPL(short) /* abcell.c */
AB_Cell_AsShort(const AB_Cell* self, ab_Env* ev) /*i*/
{
	short outShort = 0;
	ab_Env_BeginMethod(ev, AB_Cell_kClassName, "AsShort")

	char* content = self->sCell_Content;
	if ( content )
	{
		if ( *content )
		{
			int value = 0;
			int n = sscanf(content, "%d", &value);
			outShort = value;

#ifdef AB_CONFIG_DEBUG
			if ( n != 1 )
				AB_Cell_Break(self, ev);
#endif /*AB_CONFIG_DEBUG*/
		}
	}
	else ev->NewAbookFault(AB_Cell_kFaultNullContent);

	ab_Env_EndMethod(ev)
	return outShort;
}

AB_MODEL_IMPL(ab_bool) /* abcell.c */
AB_Cell_WriteHexLong(AB_Cell* self, ab_Env* ev, long inLong) /*i*/
{
	ab_Env_BeginMethod(ev, AB_Cell_kClassName, "WriteHexLong")
	
	char* content = self->sCell_Content;
	if ( content )
	{
		char buf[ 48 ];
		
		XP_SPRINTF(buf, "%08lX", inLong );
		XP_STRNCPY_SAFE(content, buf, self->sCell_Size);
		
		self->sCell_Length = XP_STRLEN(content);
		self->sCell_Extent = self->sCell_Length;
	}
	else ev->NewAbookFault(AB_Cell_kFaultNullContent);

	ab_Env_EndMethod(ev)
	return ev->Good();
}

AB_MODEL_IMPL(ab_bool) /* abcell.c */
AB_Cell_WriteShort(AB_Cell* self, ab_Env* ev, short inShort) /*i*/
{
	ab_Env_BeginMethod(ev, AB_Cell_kClassName, "WriteShort")
	
	char* content = self->sCell_Content;
	if ( content )
	{
		char buf[ 32 ];
		
		XP_SPRINTF(buf, "%d", (int) ( inShort & 0x0FFFF ) );
		XP_STRNCPY_SAFE(content, buf, self->sCell_Size);
		
		self->sCell_Length = XP_STRLEN(content);
		self->sCell_Extent = self->sCell_Length;
	}
	else ev->NewAbookFault(AB_Cell_kFaultNullContent);

	ab_Env_EndMethod(ev)
	return ev->Good();
}

AB_MODEL_IMPL(ab_bool) /* abcell.c */
AB_Cell_WriteBool(AB_Cell* self, ab_Env* ev, ab_bool inBool) /*i*/
{
	ab_Env_BeginMethod(ev, AB_Cell_kClassName, "WriteBool")

	char* content = self->sCell_Content;
	if ( content )
	{
		if ( self->sCell_Size >= 2 )
		{
			if (inBool)
				*content++ = 't';
			else
				*content++ = 'f';
			
			*content = '\0';
			self->sCell_Length = 1;
			self->sCell_Extent = 1;
		}
		else ev->NewAbookFault(AB_Cell_kFaultSizeTooSmall);
	}
	else ev->NewAbookFault(AB_Cell_kFaultNullContent);

	ab_Env_EndMethod(ev)
	return ev->Good();
}



