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

/* *
 * 
 *
 * xplocale.c
 * ----------
 */

/* xp headers */ 
#include "xplocale.h"
#include "ntypes.h"
#include "xp_str.h"

#include "prtypes.h"
#include "xpgetstr.h"
#include "csid.h"	/* Need to access CS_DEFAULT */
#include "libi18n.h"	/* Need to access 	
				INTL_CharSetNameToID()
				INTL_ResourceCharSet()	
			   We should consider rename INTL_ResourceCharSet()
			   	into XP_CharSetOfGetString() and move to here
			*/
#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (1)
#endif

/* fun: XP_StrColl 
* ---------------
* Takes two strings to compare, compares them, 
* and returns a number less than 0 if the second
* string is greater, 0 if they are the same, 
* and greater than 0 if the first string is 
* greater, according to the sorting rules 
* appropriate for the current locale.
*/

int XP_StrColl(const char* s1, const  char* s2) 
{
	return(FE_StrColl(s1, s2));	
}	


/* XP_StrfTime */
/* Returns 0 on error, size of return string otherwise */


size_t XP_StrfTime(MWContext* context, char *result, size_t maxsize, int format,
    const struct tm *timeptr)

{

/* Maybe eventually do some locale setting here */
return(FE_StrfTime(context, result, maxsize, format, timeptr));

} 


const char* INTL_ctime(MWContext* context, time_t *date)
{
  static char result[40];	
#ifdef XP_WIN
  if (*date < 0 || *date > 0x7FFFFFFF)
    *date = 0x7FFFFFFF;
#endif
  if(date != NULL)
  {
	  XP_StrfTime(context, result, sizeof(result), XP_LONG_DATE_TIME_FORMAT, localtime(date));
  } else {
  	  result[0] = '\0';
  }
  return result;
}

static int16 res_csid = CS_DEFAULT;
char *XP_GetStringForHTML(int i, int16 wincsid, char* english)
{
	/* Need to do some initialization */
	if(res_csid == CS_DEFAULT)
		res_csid = INTL_CharSetNameToID(INTL_ResourceCharSet());

	if(INTL_DocToWinCharSetID(wincsid) == res_csid)
		return XP_GetString(i);
	else
		return english;
		
}




