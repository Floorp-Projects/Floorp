/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* 
   gnomeloc.c --- gnome fe handling of locale specific stuff.
*/

#include "xplocale.h"

#include "structs.h"
#include "ntypes.h"
#include "xpassert.h"
#include "proto.h"
#include "fe_proto.h"

#include "libi18n.h"

#include "csid.h"


int16 fe_LocaleCharSetID = CS_LATIN1;

/*
** FE_StrfTime - format a struct tm to a character string, depending
** on the value of the format parameter.
**
** Values for format (and their mapping to unix strftime format
** strings) include:
**
** XP_TIME_FORMAT - "%H:%M"
** XP_WEEKDAY_TIME_FORMAT - "%a %H:%M"
** XP_DATE_TIME_FORMAT - "%x %H:%M"
** XP_LONG_DATE_TIME_FORMAT - "%c"
** anything else - "%c"
*/
size_t
FE_StrfTime(MWContext *context,
	    char *result,
	    size_t maxsize,
	    int format,
	    const struct tm *timeptr)
{
  printf("FE_StrfTime (empty)\n");
}

/*
** FE_StrColl - call into the platform specific strcoll function.
**
** Make sure strcoll() or equivalent works properly.  For example,
** the XFE has a check to make sure it does work, and if it doesn't
** it defaults to strcasecmp.
*/
int
FE_StrColl(const char *s1, const char *s2)
{
  printf("FE_StrColl (empty)\n");
}

/* 
** INTL_ResourceCharSet - return the ascii name for the locale's
** character set id.
**
** Use INTL_CharSetIDToName to retrieve the name given a CSID.
**
** Note: This is a silly function, IMO.  It should just return the
** CSID, and the libi18n stuff could convert it to a name if it wants
** to.
*/
char *
INTL_ResourceCharSet()
{
  printf("INTL_ResourceCharSet (empty)\n");
}

/*
** INTL_DefaultDocCharSetID - return the default character set id for
** a given context.
**
** It should first try to extract the csid from the document being shown in
** the context. using LO_GetDocumentCharacterSetInfo and INTL_GetCSIDocCSID.
**
** If this fails, and the user has specified an encoding (using the View|Encoding
** menu, is should return the CSID for that.
**
** Otherwise, it should return the FE's default preference for CSID.
*/
int16
INTL_DefaultDocCharSetID(MWContext *cxt)
{
  printf("INTL_DefaultDocCharSetID\n");
  return (CS_LATIN1);
}

/*
** INTL_Relayout - relayout a given context, as the character encoding
** has changed.
*/
void
INTL_Relayout(MWContext *pContext)
{
  printf("INTL_Relayout (empty)\n");
}


INTLCharSetID
FE_GetCharSetID(INTL_CharSetID_Selector selector)
{
  INTLCharSetID charsetID = CS_DEFAULT;
  
  switch (selector)
    {
    case INTL_FileNameCsidSel:
    case INTL_DefaultTextWidgetCsidSel:
      charsetID = (INTLCharSetID) fe_LocaleCharSetID;
      break;
    case INTL_OldBookmarkCsidSel:
      charsetID = (INTLCharSetID) INTL_DocToWinCharSetID(fe_LocaleCharSetID);
      break;
    default:
      break;
    }
  XP_ASSERT(CS_DEFAULT != charsetID);
  
  return charsetID;
}
