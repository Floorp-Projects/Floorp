/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


/* datetime.c */




#include "libi18n.h"
#include "xp.h"

/** 
 * Locale sensitive Date formmating function 
 * 
 * Using a given selector, this returns a charset ID. 
 * Designed to retrieve a non-context dependent charset ID (e.g file system). 
 * 
 * @param	localeID		Specification for the Locale conventions to use.
 * @param	formatSelector	Specification for the type of format to select
 * @param	time			time
 * @param	utf8Buffer		result buffer (in utf8)
 * @param	bufferLength	length of result buffer
 * @return	PRSuccess when succesful, PRFailure otherwise 
 */ 
PUBLIC PRStatus INTL_FormatTime(INTLLocaleID localeID,
									   INTL_DateFormatSelector dateFormatSelector,
									   INTL_TimeFormatSelector timeFormatSelector,
									   time_t	time,
									   unsigned char* utf8Buffer,
									   PRUint32 bufferLength)
{
	struct tm*	tm_time;

	tm_time = localtime(&time);
	
	return INTL_FormatTMTime(localeID,dateFormatSelector,timeFormatSelector,tm_time,utf8Buffer,bufferLength);

}

PUBLIC PRStatus INTL_FormatTMTime(INTLLocaleID localeID,
										 INTL_DateFormatSelector dateFormatSelector,
										 INTL_TimeFormatSelector timeFormatSelector,
										 const struct tm* time,
										 unsigned char* utf8Buffer,
										 PRUint32 bufferLength)
{
	LCID		windowsLCID = GetThreadLocale();
	DWORD		windowsDateFlags, windowsTimeFlags;
	SYSTEMTIME	sysTime;
	int			resultLength;

	/* convert to windows SYSTIME structure */
	sysTime.wYear = (time->tm_year)+1900;				/* offset from 1900 */
    sysTime.wMonth = time->tm_mon;
    sysTime.wDayOfWeek = time->tm_wday;
    sysTime.wDay = time->tm_mday;
    sysTime.wHour = time->tm_hour;
    sysTime.wMinute = time->tm_min;
    sysTime.wSecond = time->tm_sec;
    sysTime.wMilliseconds = 0;


	if (dateFormatSelector!=INTL_DateFormatNone)
	{
		switch(dateFormatSelector)
		{

			case 	INTL_DateFormatLong:		windowsDateFlags = DATE_LONGDATE;
												break;
			case	INTL_DateFormatShort:		windowsDateFlags = DATE_SHORTDATE;
												break;	
			default:
					return PR_FAILURE;
		}
		
		resultLength = GetDateFormat(windowsLCID,windowsDateFlags,&sysTime,NULL,utf8Buffer,bufferLength);
	}

	if (timeFormatSelector!=INTL_TimeFormatNone)
	{
		switch(timeFormatSelector)
		{
			case	INTL_TimeFormatSeconds:			windowsTimeFlags = 0; /* no flags */
													break;
			case	INTL_TimeFormatNoSeconds:		windowsTimeFlags = TIME_NOSECONDS;
													break;
			case	INTL_TimeFormatForce24Hour:		windowsTimeFlags = TIME_FORCE24HOURFORMAT;
													break;
			default:
					return PR_FAILURE;
		}

		if (dateFormatSelector!=INTL_DateFormatNone)
		{
			utf8Buffer[resultLength-1]=0x20;				/* space character */
		}
		GetTimeFormat(windowsLCID,windowsTimeFlags,&sysTime,NULL,utf8Buffer+resultLength,bufferLength-resultLength);
	}

	return PR_SUCCESS;
}