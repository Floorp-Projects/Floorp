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

/* locale.h
 * FE support for xp_locale stuff
 */
 
 
#include "xplocale.h"
#include "macutil.h"
#include <IntlResources.h>
#include "uprefd.h"
#include "xpassert.h"
#include "csid.h"
#include "structs.h"
#include "intl_csi.h"

long WinCharsetIDToScript(UInt16 wincsid);
Intl1Hndl GetItl1Resource(long scriptcode);
Intl0Hndl GetItl0Resource(long scriptcode);
void AbbrevWeekdayString(DateTimeRec &dateTime, Str255 weekdayString, Intl1Hndl Itl1RecordHandle );

/* FE_StrColl
 *   -1, 0, 1 return guaranteed
 */
 
int FE_StrColl(const char* s1, const char* s2) 
 
 
 {
	long scriptcode;
	// get system script code
	scriptcode = ::GetScriptManagerVariable(smSysScript);
	
	// get itlb of the system script
	ItlbRecord **ItlbRecordHandle;
	ItlbRecordHandle = (ItlbRecord **)::GetResource('itlb', scriptcode);
	
	// get itl2 number of current system script from itlb if possible
	// otherwise, try script manager (Script manager won't update 
	// itl2 number when the change on the fly )
	long itl2num;
	if(ItlbRecordHandle != NULL)
	{
		if(*ItlbRecordHandle == NULL)
			::LoadResource((Handle)ItlbRecordHandle);
			
		if(*ItlbRecordHandle != NULL)
			itl2num = (*ItlbRecordHandle)->itlbSort;
		else
			itl2num = ::GetScriptVariable(scriptcode, smScriptSort);
	} else {	/* Use this as fallback */
		itl2num = ::GetScriptVariable(scriptcode, smScriptSort);
	}
	// get itl2 resource
	Handle itl2Handle;
	itl2Handle = ::GetResource('itl2', itl2num);
	
	// call CompareText with itl2Handle
	// We don't need to check itl2Handle here. CompareText will use 
	// default system sorting if itl2Handle is null
	return( (int) CompareText(s1,  s2, 
		strlen(s1), strlen(s2), itl2Handle));
}

long WinCharsetIDToScript(UInt16 wincsid)
{
	CCharSet font;
	if(CPrefs::GetFont((wincsid & ~ CS_AUTO), &font ))
		return font.fFallbackFontScriptID;
	else
		return smRoman;
}

Intl1Hndl GetItl1Resource(long scriptcode)
{
	// get itlb from currenty system script
	ItlbRecord **ItlbRecordHandle;
	ItlbRecordHandle = (ItlbRecord **)::GetResource('itlb', scriptcode);
	
	// get itl1 number from itlb resource, if possible
	// otherwise, use the one return from script manager, 
	// (Script manager won't update itl1 number when the change on the fly )
	long itl1num;
	if(ItlbRecordHandle != NULL)
	{
		if(*ItlbRecordHandle == NULL)
			::LoadResource((Handle)ItlbRecordHandle);
			
		if(*ItlbRecordHandle != NULL)
			itl1num = (*ItlbRecordHandle)->itlbDate;
		else
			itl1num = ::GetScriptVariable(scriptcode, smScriptDate);
	} else {	// Use this as fallback
		itl1num = ::GetScriptVariable(scriptcode, smScriptDate);
	}
	
	// get itl1 resource 
	Intl1Hndl Itl1RecordHandle;
	Itl1RecordHandle = (Intl1Hndl)::GetResource('itl1', itl1num);
	return Itl1RecordHandle;
}

Intl0Hndl GetItl0Resource(long scriptcode)
{
	// get itlb from currenty system script
	ItlbRecord **ItlbRecordHandle;
	ItlbRecordHandle = (ItlbRecord **)::GetResource('itlb', scriptcode);
	
	// get itl0 number from itlb resource, if possible
	// otherwise, use the one return from script manager, 
	// (Script manager won't update itl1 number when the change on the fly )
	long itl0num;
	if(ItlbRecordHandle != NULL)
	{
		if(*ItlbRecordHandle == NULL)
			::LoadResource((Handle)ItlbRecordHandle);
			
		if(*ItlbRecordHandle != NULL)
			itl0num = (*ItlbRecordHandle)->itlbNumber;
		else
			itl0num = ::GetScriptVariable(scriptcode, smScriptNumber);
	} else {	// Use this as fallback
		itl0num = ::GetScriptVariable(scriptcode, smScriptNumber);
	}
	
	// get itl1 resource 
	Intl0Hndl Itl0RecordHandle;
	Itl0RecordHandle = (Intl0Hndl)::GetResource('itl0', itl0num);
	return Itl0RecordHandle;
}


void AbbrevWeekdayString(DateTimeRec &dateTime, Str255 weekdayString, Intl1Hndl Itl1RecordHandle )
{
	Boolean gotit = false;
	
	// If we can get itl1Resource, exam it.
	if(Itl1RecordHandle != NULL )
	{
		if(*Itl1RecordHandle == NULL)
			::LoadResource((Handle)Itl1RecordHandle);

		if(*Itl1RecordHandle == NULL)
		{
			weekdayString[0] = 0;
			return;
		}
		// if itl1 resource is in the itl1ExtRec format 
		// look at the additional table
		// See IM-Text Appendix B for details
		if((unsigned short)((*Itl1RecordHandle)->localRtn[0]) == 0xA89F)
		{	// use itl1ExtRect
			Itl1ExtRec **Itl1ExtRecHandle;
			Itl1ExtRecHandle = (Itl1ExtRec **) Itl1RecordHandle;
			
			// check abbrevDaysTableLength and abbrevDaysTableOffset
			if(((*Itl1ExtRecHandle)->abbrevDaysTableLength != 0) &&
			   ((*Itl1ExtRecHandle)->abbrevDaysTableOffset != 0))
			{	
				// use the additional table for abbreviation weekday name
				// Japanese use it.
				// Start Pointer access to Handle, no HLock since we don't
				// call any API here.
				// Be careful when you debug- don't move memory :)
				Ptr abTablePt;
				short itemlen;
				
				// Ok, change it back to range [0-6]
				short weekday = dateTime.dayOfWeek - 1;	
				
				abTablePt = (Ptr)(*Itl1ExtRecHandle);
				abTablePt +=  (*Itl1ExtRecHandle)->abbrevDaysTableOffset;
				
				// first 2 byte in the table should be the count.
				itemlen = (short) *((short*)abTablePt);	
				abTablePt += 2;	
				
				if(weekday < itemlen)
				{
					unsigned char len;
					short i;
					// iterate till we hit the weekday name we want
					for(i = 0 ; i < weekday ; i++)	
					{
						len = *abTablePt;
						// shift to the next one. don't forget the len byte itself.
						abTablePt += len + 1;	
					}
					// Ok, we got it, let's copy it.
					len = *abTablePt;
					::BlockMoveData(abTablePt,&weekdayString[0] ,len+1);
					gotit = true;
				}
			}
		}
		
		// didn't get it. Either it is not in itl1ExtRect format or it don't have
		// additional abbreviation table. 
		// use itl1Rect instead.
		if(! gotit)
		{	
			// get abbreviation length
			// not the length is not always 3. Some country use longer (say 4)
			// abbreviation.
			short abbrLen = (*Itl1RecordHandle)->abbrLen;
			// Fix  Traditional Chinese problem
			if(((((*Itl1RecordHandle)->intl1Vers) >> 8) == verTaiwan ) &&
			   (abbrLen == 4) &&
			   ((*Itl1RecordHandle)->days[0][0] == 6) && 
			   ((*Itl1RecordHandle)->days[1][0] == 6) && 
			   ((*Itl1RecordHandle)->days[2][0] == 6) && 
			   ((*Itl1RecordHandle)->days[3][0] == 6) && 
			   ((*Itl1RecordHandle)->days[4][0] == 6) && 
			   ((*Itl1RecordHandle)->days[5][0] == 6) && 
			   ((*Itl1RecordHandle)->days[6][0] == 6))
			{
				abbrLen = 6;
			}
			weekdayString[0] = abbrLen;
			// copy the weekday name with that abbreviation length
			::BlockMoveData(&((*Itl1RecordHandle)->days[dateTime.dayOfWeek-1][1]),
				 &weekdayString[1] , abbrLen);
			gotit = true;
		}
	}
	else 
	{	// cannot get itl1 resource, return with null string.
		weekdayString[0] = 0;
	}
}


size_t FE_StrfTime(MWContext* context, char *result, size_t maxsize,  int format, 
	const struct tm *timeptr)

{

	Str255 timeString;
	int32 dateTime;	
	DateTimeRec macDateTime;
	
	// convert struct tm to input format of mac toolbox call
	
	XP_ASSERT(timeptr->tm_year >= 0);
	XP_ASSERT(timeptr->tm_mon >= 0);
	XP_ASSERT(timeptr->tm_mday >= 0);
	XP_ASSERT(timeptr->tm_hour >= 0);
	XP_ASSERT(timeptr->tm_min >= 0);
	XP_ASSERT(timeptr->tm_sec >= 0);
	XP_ASSERT(timeptr->tm_wday >= 0);
	
	// Mac need a number from 1904 to 2040 
	// tm only provide the last two digit of the year */
	macDateTime.year = timeptr->tm_year + 1900;	
	
	// Mac use 1 for Jan and 12 for Dec
	// tm use 0 for Jan and 11 for Dec 
	macDateTime.month = timeptr->tm_mon + 1;	
	macDateTime.day = timeptr->tm_mday;
	macDateTime.hour = timeptr->tm_hour;
	macDateTime.minute = timeptr->tm_min;
	macDateTime.second = timeptr->tm_sec;
	
	// Mac use 1 for sunday 7 for saturday
	// tm use 0 for sunday 6 for saturday 
	macDateTime.dayOfWeek = timeptr->tm_wday +1 ; 
	
	::DateToSeconds( &macDateTime, (unsigned long *) &dateTime);
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
	long scriptcode = WinCharsetIDToScript(INTL_GetCSIWinCSID(c));
	Handle itl1Handle = (Handle) GetItl1Resource(scriptcode);
	Handle itl0Handle = (Handle) GetItl0Resource(scriptcode);

	// get time string
	if(format == XP_LONG_DATE_TIME_FORMAT)
		::TimeString(dateTime, TRUE, timeString, itl0Handle);
	else
		::TimeString(dateTime, FALSE, timeString, itl0Handle);
	switch(format)
	{
		case XP_TIME_FORMAT:
			{
				if (timeString[0] >= maxsize) {
					result[0] = '\0';
					return(0);
				}
				// just copy the time string
				if(timeString[0] > 0)
					::BlockMoveData(&timeString[1], result , timeString[0]);
				result[ timeString[0]] = '\0';
				return (  timeString[0] );
			}
		case XP_LONG_DATE_TIME_FORMAT:
		case XP_WEEKDAY_TIME_FORMAT:
		case XP_DATE_TIME_FORMAT:
			{
				Str255 dateString;
				switch(format)
				{
				case XP_LONG_DATE_TIME_FORMAT:
					::DateString(dateTime, abbrevDate, dateString, itl1Handle);
					break;
				case XP_DATE_TIME_FORMAT:
					::DateString(dateTime, shortDate, dateString, itl0Handle);
					break;	
				case XP_WEEKDAY_TIME_FORMAT:
					AbbrevWeekdayString(macDateTime, dateString, (Intl1Hndl)itl1Handle);
					// try fallback if it return with null string.
					if(dateString[0] == 0)
					{	//	cannot get weekdayString from itl1 , try fallback
						dateString[0] =  strftime((char*)&dateString[1],254,"%a",timeptr);
					}
					break;	
				}
				// Append the date string and time string tother. seperate with " "
				if((dateString[0]+timeString[0] + 1) >= maxsize) {
					// try put dateString only
					if(dateString[0] >= maxsize)
					{
						result[0] = '\0';
						return(0);
					} 
					else	
					{	// we don't have space for Date and Time . So , just put date there
						if(dateString[0] > 0)
							::BlockMoveData(&dateString[1], result , dateString[0]);
						result[ dateString[0]] = '\0';
						return (  dateString[0] );
					}
				} else {
					int dlen; // we need to rescan the datestring because a bug in Cyrillic itl1
					for(dlen=0;(dlen < dateString[0]) && (dateString[dlen+1] != '\0'); dlen++)
						;
					// just incase dateString got a null string
					if(dlen > 0)
						::BlockMoveData(&dateString[1], result , dlen);
					result[ dlen] = ' ';
					// just incase timeString got a null string
					if(timeString[0] > 0)
						::BlockMoveData(&timeString[1], &result[dlen+1] , timeString[0]);
					result[ dlen+timeString[0] + 1] = '\0';
					return ( dlen+timeString[0] + 1);
				}
			}
			break;
		default:
			result[0] = '\0';
			return(0);
			break;
	}
}

