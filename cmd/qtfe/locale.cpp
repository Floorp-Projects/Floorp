/* $Id: locale.cpp,v 1.1 1998/09/25 18:01:35 ramiro%netscape.com Exp $
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
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
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

static void useArgs( const char *fn, ... )
{
    if ( 0 && fn )
	printf( "%s\n", fn );
}

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
extern "C"
size_t FE_StrfTime(MWContext *context, char *result, size_t maxsize,
        int format, const struct tm *timeptr)

{
        char    *fmt;

        switch (format)
        {
        case XP_TIME_FORMAT:
                /* fmt = "%X"; */
                fmt = "%H:%M";
                break;
        case XP_WEEKDAY_TIME_FORMAT:
                /* fmt = "%a %X"; */
                fmt = "%a %H:%M";
                break;
        case XP_DATE_TIME_FORMAT:
                /* fmt = "%x %X"; */
                fmt = "%x %H:%M";
                break;
        case XP_LONG_DATE_TIME_FORMAT:
                fmt = "%c";
                break;
        default:
                fmt = "%c";
                break;
        }

        return strftime(result, maxsize, fmt, timeptr);
}

#if 0
extern "C"
size_t
FE_StrfTime(MWContext *context,
	    char *result,
	    size_t maxsize,
	    int format,
	    const struct tm *timeptr)
{
    useArgs("FE_StrfTime", context, result, maxsize, format, timeptr);
    return 0;
}
#endif

/*
** FE_StrColl - call into the platform specific strcoll function.
**
** Make sure strcoll() or equivalent works properly.  For example,
** the XFE has a check to make sure it does work, and if it doesn't
** it defaults to strcasecmp.
*/
extern "C"
int
FE_StrColl(const char *s1, const char *s2)
{
    useArgs("FE_StrColl", s1, s2);
    return 0;
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
/**
 * Return the Charset name of the translated resource.
 *
 * @return      MIME charset of the cross-platform string resource and FE
 * resources
 * @see XP_GetString
 * @see XP_GetStringForHTML
 */
extern "C"
char *
INTL_ResourceCharSet()
{
    useArgs("INTL_ResourceCharSet");
    return "INTL_ResourceCharSet";
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
/**
 * Return default charset from preference or from current encoding 
 * menu selection. 
 * 
 * @param context       Specifies the context
 * @return                      Default document charset ID.  If the context is NULL
 *                                      then it returns default charset from the user pref
 *                                      If the context is specified then it returns curren
 *                                      encoding menu selection.
 */
extern "C"
int16
INTL_DefaultDocCharSetID(MWContext *cxt)
{
    useArgs("INTL_DefaultDocCharSetID",cxt);
    return 123456789;
}

/*
** INTL_Relayout - relayout a given context, as the character encoding
** has changed.
*/
/** 
 * Request a re-layout of the document.
 *
 * Libi18n calls this function in those cases where a different document
 * encoding is detected after document conversion and layout has begun.
 * This can occur because the parsing and layout of the document begins
 * immediately when the document data begins to stream in - at which time
 * all the data needed to determine the charset may not be available.  If
 * this occurs, the layout engine needs to be notified to pull the data from
 * the source (cache) again so the data will be converted by the correct
 * character codeset conversion module in the data stream.
 * 
 * @param context Specifies the context which should be relayout again. 
 */
extern "C"
void
INTL_Relayout(MWContext *pContext)
{
    useArgs("INTL_Relayout",pContext);
}

extern "C"
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

unsigned char *
fe_ConvertFromLocaleEncoding(int16 charset, unsigned char *str)
{
  CCCDataObject	obj;
  unsigned char	*ret;
  
  
  if ((charset == fe_LocaleCharSetID) || (!str) || (!*str))
    {
      return str;
    }
  
  /* handle UTF8 */
  if (IS_UTF8_CSID(charset)) {
    uint16 *ucs2_chars;
    uint32 ucs2_buflen, num_ucs2_chars;
    unsigned char *utf8p;
    
    /*
     * Convert local encoding to ucs2
     */
    ucs2_buflen = INTL_StrToUnicodeLen(fe_LocaleCharSetID, str);
    ucs2_chars = (uint16 *) XP_CALLOC(sizeof(uint16), ucs2_buflen+1);
    num_ucs2_chars = INTL_StrToUnicode(fe_LocaleCharSetID, str,
                                       ucs2_chars, ucs2_buflen);
    utf8p = INTL_UCS2ToUTF8(ucs2_chars, num_ucs2_chars);
    XP_FREE(ucs2_chars);
    return utf8p;
    
  }
  
  obj = INTL_CreateCharCodeConverter();
  if (!obj)
    {
      return str;
    }
  if (INTL_GetCharCodeConverter(fe_LocaleCharSetID, charset, obj))
    {
      ret = INTL_CallCharCodeConverter(obj, str,
                                       strlen((char *) str));
      if (!ret)
        {
          ret = str;
        }
	}
  else
    {
      ret = str;
    }
  
  INTL_DestroyCharCodeConverter(obj);
  
  return ret;
}
