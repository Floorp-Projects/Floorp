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


//
// Public interface and shared subsystem data.
//

#ifdef EDITOR

#include "editor.h"

typedef struct PA_AmpEsc_struct {
        char *str;
        char value;
        intn len;
} PA_AmpEsc;

#ifndef XP_MAC
static PA_AmpEsc PA_AmpEscapes[] = {
	{"lt", '<', 2},
	{"LT", '<', 2},
	{"gt", '>', 2},
	{"GT", '>', 2},
	{"amp", '&', 3},
	{"AMP", '&', 3},
	{"quot", '\"', 4},
	{"QUOT", '\"', 4},
	{"nbsp", '\240', 4},
	{"reg", '\256', 3},
	{"REG", '\256', 3},
	{"copy", '\251', 4},
	{"COPY", '\251', 4},

	{"iexcl", '\241', 5},
	{"cent", '\242', 4},
	{"pound", '\243', 5},
	{"curren", '\244', 6},
	{"yen", '\245', 3},
	{"brvbar", '\246', 6},
	{"sect", '\247', 4},

	{"uml", '\250', 3},
	{"ordf", '\252', 4},
	{"laquo", '\253', 5},
	{"not", '\254', 3},
	{"shy", '\255', 3},
	{"macr", '\257', 4},

	{"deg", '\260', 3},
	{"plusmn", '\261', 6},
	{"sup2", '\262', 4},
	{"sup3", '\263', 4},
	{"acute", '\264', 5},
	{"micro", '\265', 5},
	{"para", '\266', 4},
	{"middot", '\267', 6},

	{"cedil", '\270', 5},
	{"sup1", '\271', 4},
	{"ordm", '\272', 4},
	{"raquo", '\273', 5},
	{"frac14", '\274', 6},
	{"frac12", '\275', 6},
	{"frac34", '\276', 6},
	{"iquest", '\277', 6},

	{"Agrave", '\300', 6},
	{"Aacute", '\301', 6},
	{"Acirc", '\302', 5},
	{"Atilde", '\303', 6},
	{"Auml", '\304', 4},
	{"Aring", '\305', 5},
	{"AElig", '\306', 5},
	{"Ccedil", '\307', 6},

	{"Egrave", '\310', 6},
	{"Eacute", '\311', 6},
	{"Ecirc", '\312', 5},
	{"Euml", '\313', 4},
	{"Igrave", '\314', 6},
	{"Iacute", '\315', 6},
	{"Icirc", '\316', 5},
	{"Iuml", '\317', 4},

	{"ETH", '\320', 3},
	{"Ntilde", '\321', 6},
	{"Ograve", '\322', 6},
	{"Oacute", '\323', 6},
	{"Ocirc", '\324', 5},
	{"Otilde", '\325', 6},
	{"Ouml", '\326', 4},
	{"times", '\327', 5},

	{"Oslash", '\330', 6},
	{"Ugrave", '\331', 6},
	{"Uacute", '\332', 6},
	{"Ucirc", '\333', 5},
	{"Uuml", '\334', 4},
	{"Yacute", '\335', 6},
	{"THORN", '\336', 5},
	{"szlig", '\337', 5},

	{"agrave", '\340', 6},
	{"aacute", '\341', 6},
	{"acirc", '\342', 5},
	{"atilde", '\343', 6},
	{"auml", '\344', 4},
	{"aring", '\345', 5},
	{"aelig", '\346', 5},
	{"ccedil", '\347', 6},

	{"egrave", '\350', 6},
	{"eacute", '\351', 6},
	{"ecirc", '\352', 5},
	{"euml", '\353', 4},
	{"igrave", '\354', 6},
	{"iacute", '\355', 6},
	{"icirc", '\356', 5},
	{"iuml", '\357', 4},

	{"eth", '\360', 3},
	{"ntilde", '\361', 6},
	{"ograve", '\362', 6},
	{"oacute", '\363', 6},
	{"ocirc", '\364', 5},
	{"otilde", '\365', 6},
	{"ouml", '\366', 4},
	{"divide", '\367', 6},

	{"oslash", '\370', 6},
	{"ugrave", '\371', 6},
	{"uacute", '\372', 6},
	{"ucirc", '\373', 5},
	{"uuml", '\374', 4},
	{"yacute", '\375', 6},
	{"thorn", '\376', 5},
	{"yuml", '\377', 4},

	{NULL, '\0', 0},
};
#else /* ! XP_MAC */
							/* Entities encoded in MacRoman.  */
static PA_AmpEsc PA_AmpEscapes[] = {
	{"lt", '<', 2},
	{"LT", '<', 2},
	{"gt", '>', 2},
	{"GT", '>', 2},
	{"amp", '&', 3},
	{"AMP", '&', 3},
	{"quot", '\"', 4},
	{"QUOT", '\"', 4},
	{"nbsp", '\007', 4},
	{"reg", '\250', 3},
	{"REG", '\250', 3},
	{"copy", '\251', 4},
	{"COPY", '\251', 4},

	{"iexcl", '\301', 5},
	{"cent", '\242', 4},
	{"pound", '\243', 5},
	{"curren", '\333', 6},
	{"yen", '\264', 3},

	/*
	 * Navigator Gold currently inverts this table in such a way that
	 * ASCII characters (less than 128) get converted to the names
	 * listed here.  For things like ampersand (&amp;) this is the
	 * right thing to do, but for this one (brvbar), it isn't since
	 * both broken vertical bar and vertical bar are mapped to the same
	 * character by the Latin-1 to Mac Roman table.
	 *
	 * Punt for now. This needs to be fixed later. -- erik
	 */
	/* {"brvbar", '\174', 6}, */

	{"sect", '\244', 4},

	{"uml", '\254', 3},
	{"ordf", '\273', 4},
	{"laquo", '\307', 5},
	{"not", '\302', 3},
	{"shy", '\320', 3},
	{"macr", '\370', 4},

	{"deg", '\241', 3},
	{"plusmn", '\261', 6},
	/* {"sup2", '\62', 4}, see comment above */
	/* {"sup3", '\63', 4}, see comment above */
	{"acute", '\253', 5},
	{"micro", '\265', 5},
	{"para", '\246', 4},
	{"middot", '\341', 6},

	{"cedil", '\374', 5},
	/* {"sup1", '\61', 4}, see comment above */
	{"ordm", '\274', 4},
	{"raquo", '\310', 5},
	{"frac14", '\271', 6},
	{"frac12", '\270', 6},
	{"frac34", '\262', 6},
	{"iquest", '\300', 6},

	{"Agrave", '\313', 6},
	{"Aacute", '\347', 6},
	{"Acirc", '\345', 5},
	{"Atilde", '\314', 6},
	{"Auml", '\200', 4},
	{"Aring", '\201', 5},
	{"AElig", '\256', 5},
	{"Ccedil", '\202', 6},

	{"Egrave", '\351', 6},
	{"Eacute", '\203', 6},
	{"Ecirc", '\346', 5},
	{"Euml", '\350', 4},
	{"Igrave", '\355', 6},
	{"Iacute", '\352', 6},
	{"Icirc", '\353', 5},
	{"Iuml", '\354', 4},

	{"ETH", '\334', 3},			/* Icelandic MacRoman: ETH ('D' w/horiz bar) */
	{"Ntilde", '\204', 6},
	{"Ograve", '\361', 6},
	{"Oacute", '\356', 6},
	{"Ocirc", '\357', 5},
	{"Otilde", '\315', 6},
	{"Ouml", '\205', 4},
	/* {"times", '\170', 5}, see comment above */

	{"Oslash", '\257', 6},
	{"Ugrave", '\364', 6},
	{"Uacute", '\362', 6},
	{"Ucirc", '\363', 5},
	{"Uuml", '\206', 4},
	{"Yacute", '\240', 6},		/* Icelandic MacRoman: Yacute */
	{"THORN", '\336', 5},		/* Icelandic MacRoman: THORN (kinda like 'P') */
	{"szlig", '\247', 5},

	{"agrave", '\210', 6},
	{"aacute", '\207', 6},
	{"acirc", '\211', 5},
	{"atilde", '\213', 6},
	{"auml", '\212', 4},
	{"aring", '\214', 5},
	{"aelig", '\276', 5},
	{"ccedil", '\215', 6},

	{"egrave", '\217', 6},
	{"eacute", '\216', 6},
	{"ecirc", '\220', 5},
	{"euml", '\221', 4},
	{"igrave", '\223', 6},
	{"iacute", '\222', 6},
	{"icirc", '\224', 5},
	{"iuml", '\225', 4},

	{"eth", '\335', 3},		/* Icelandic MacRoman: eth ('d' w/horiz bar) */
	{"ntilde", '\226', 6},
	{"ograve", '\230', 6},
	{"oacute", '\227', 6},
	{"ocirc", '\231', 5},
	{"otilde", '\233', 6},
	{"ouml", '\232', 4},
	{"divide", '\326', 6},

	{"oslash", '\277', 6},
	{"ugrave", '\235', 6},
	{"uacute", '\234', 6},
	{"ucirc", '\236', 5},
	{"uuml", '\237', 4},
	{"yacute", '\340', 6},		/* Icelandic MacRoman: yacute */
	{"thorn", '\337', 5},		/* Icelandic MacRoman: thorn (kinda like 'p') */
	{"yuml", '\330', 4},

	{NULL, '\0', 0},
};

#endif


// For XP Strings
extern "C" {
#include "xpgetstr.h"
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS
}
#include "prefapi.h"
#include "secnav.h"
#include "xp_str.h"

char* edt_CopyFromHuge(int16 /*csid*/, XP_HUGE_CHAR_PTR text, int32 length, int32* pOutLength){
    int len2 = length;

#ifdef XP_WIN16
    const int32 kMax = 65530;
    XP_Bool bTruncated = FALSE;
    if ( len2 > kMax ) {
        XP_TRACE(("Truncating huge data. original size: %d bytes", len2));
        bTruncated = TRUE;
        len2 = kMax;
    }
#endif

    char* pData = (char*) XP_ALLOC(len2 + 1);
    if ( pData == NULL ) return NULL;
    for(int32 i = 0; i < len2; i++ ){
        // No XP way of transfering from HUGE to normal
        pData[i] = text[i];
    }

#ifdef XP_WIN16
    if ( bTruncated ) {
        int nboc = INTL_NthByteOfChar(csid, pData, (int)(len2));
        // nboc is one-based. Stupid!
        if ( nboc > 1 ){
            len2 -= (nboc - 1);
        }
        if ( len2 < 0 ) {
            len2 = 0;
        }
        XP_TRACE(("Final size: %d bytes", len2));
    }
#endif

    pData[len2] = '\0';
    if ( pOutLength ) {
        *pOutLength = len2;
    }
    return pData;
}

void edt_StripAtHashOrQuestionMark(char *pUrl)
{
    if( !pUrl )
        return;

    char *pStart = XP_STRRCHR(pUrl, '/');

    if(!pStart)
        pStart = pUrl;
    
    char * pSuffix = XP_STRCHR(pStart, '?');

    if(pSuffix)
	    *pSuffix = '\0';

    pSuffix = XP_STRRCHR(pStart, '#');
    if(pSuffix)
	    *pSuffix = '\0';
}

char *edt_StripUsernamePassword(char *pUrl) {
  int iType = NET_URL_Type(pUrl);
  if (iType != FTP_TYPE_URL &&
      iType != HTTP_TYPE_URL &&
      iType != SECURE_HTTP_TYPE_URL) {
    return XP_STRDUP(pUrl);
  }

  char *pRet = NULL;
  if (NET_ParseUploadURL(pUrl,&pRet,NULL,NULL) && pRet) {
    return pRet;
  }
  else {
    return XP_STRDUP(pUrl);
  }
}


// Always returns newly allocated memory or NULL.
PRIVATE char *edt_remove_trailing_slash(const char *pStr) {
  if (pStr) {
    char *pNew = XP_STRDUP(pStr);
    if (pNew && *pNew) {
      // Only do strlen() if pNew is non-NULL.
      int iLen = XP_STRLEN(pNew);

      // Kill trailing slash.
      if (pNew[iLen-1] == '/') {
        pNew[iLen-1] = '\0';
      }
    }
    return pNew;
  }
  else {
    return NULL;
  }
}



// Just wrappers around the XP_* functions.  The only difference is that
// these functions will work if the directory ends in a trailing slash,
// avoiding obscure bugs on Win16.
XP_Dir edt_OpenDir(const char * name, XP_FileType type) {
  XP_ASSERT(type == xpURL);
  char *pNew = edt_remove_trailing_slash(name);
  XP_Dir ret = XP_OpenDir(pNew,type);
  XP_FREEIF(pNew);
  return ret;
}

int edt_MakeDirectory(const char* name, XP_FileType type) {
  XP_ASSERT(type == xpURL);
  char *pNew = edt_remove_trailing_slash(name);
  int ret = XP_MakeDirectory(pNew,type);
  XP_FREEIF(pNew);
  return ret;
}

int edt_RemoveDirectory(const char *name, XP_FileType type) {
  XP_ASSERT(type == xpURL);
  char *pNew = edt_remove_trailing_slash(name);
  int ret = XP_RemoveDirectory(pNew,type);
  XP_FREEIF(pNew);
  return ret;
}



// Recursively kill everything under pNameArg, which is in xpURL format.
void edt_RemoveDirectoryR(char *pNameArg) {
  // non-NULL and non-zero-length.
  if (!(pNameArg && *pNameArg))
    return;

  // Make sure name ends in slash.
  char *pName = XP_STRDUP(pNameArg);
  if (!pName) {
    return; 
  }
  if (pName[XP_STRLEN(pName)-1] != '/') {
    StrAllocCat(pName,"/");
  }


  XP_Dir dir = edt_OpenDir(pName, xpURL);
  if (dir) {
    XP_DirEntryStruct *entry = NULL;
    for (entry = XP_ReadDir(dir); entry; entry = XP_ReadDir(dir)) {
      // Don't touch "." or ".."
      if (entry->d_name && XP_STRCMP(entry->d_name,".") && XP_STRCMP(entry->d_name,"..")) {

        // Create filename inside directory.
        char *subName = XP_STRDUP(pName);
        if (entry->d_name[0] == '/') {
          // skip the slash.
          StrAllocCat(subName,entry->d_name + 1);
        }
        else {
          StrAllocCat(subName,entry->d_name);
        }

        // Recurse, will do nothing if not a directory.
        edt_RemoveDirectoryR(subName);
        // If subName is a directory, it will already be gone, so this does nothing.
        XP_FileRemove(subName,xpURL);


        XP_FREEIF(subName);
      }
    }
                
    XP_CloseDir(dir); 
    edt_RemoveDirectory(pName,xpURL);
  }  


  XP_FREE(pName);
}


// Timer


void CEditTimerCallback (void * closure){
	if (closure) {
        CEditTimer* pTimer = (CEditTimer*) closure;
        pTimer->Callback();
    }
}

CEditTimer::CEditTimer(){
    m_timeoutEnabled = FALSE;
    m_timerID = NULL;
}

CEditTimer::~CEditTimer(){
    Cancel();
}

void CEditTimer::Callback() {
    m_timeoutEnabled = FALSE;
    OnCallback();
}

void CEditTimer::Cancel(){
    if ( m_timeoutEnabled ) {
        FE_ClearTimeout(m_timerID);
        m_timeoutEnabled = FALSE;
        m_timerID = NULL;
    }
}

void CEditTimer::OnCallback() {}

void CEditTimer::SetTimeout(uint32 msecs){
    Cancel();
	m_timerID = FE_SetTimeout(&CEditTimerCallback, this, msecs);
    m_timeoutEnabled = TRUE;
}

// Color with a defined/undefined boolean

ED_Color::ED_Color() {
	m_bDefined = TRUE;
    m_color.red = 0;
    m_color.green = 0;
    m_color.blue = 0;
}
ED_Color::ED_Color(LO_Color& pLoColor) {
    m_bDefined = TRUE;
    m_color = pLoColor;
}

static int ED_Color_Clip(int c) {
    if ( c < 0 ) {
        XP_ASSERT(FALSE);
        c = 0;
    }
    if ( c > 255 ) {
        XP_ASSERT(FALSE);
        c = 255;
    }
    return c;
}

ED_Color::ED_Color(int r, int g, int b) {
    m_bDefined = TRUE;
    m_color.red = ED_Color_Clip(r);
    m_color.green = ED_Color_Clip(g);
    m_color.blue = ED_Color_Clip(b);
}

ED_Color::ED_Color(int32 rgb) {
    if ( rgb == -1 ) {
        m_bDefined = FALSE;
        m_color.red = 0;
        m_color.green = 0;
        m_color.blue = 0;
    }
    else {
        m_bDefined = TRUE;
        m_color.red = (rgb >> 16) & 255;
        m_color.green = (rgb >> 8) & 255;
        m_color.blue = rgb & 255;
    }
}

ED_Color::ED_Color(LO_Color* pLoColor) {
    if ( pLoColor ) {
        m_color.red = ED_Color_Clip(pLoColor->red);
        m_color.green = ED_Color_Clip(pLoColor->green);
        m_color.blue = ED_Color_Clip(pLoColor->blue);
        m_bDefined = TRUE;
    }
    else {
        m_bDefined = FALSE;
    }
}

XP_Bool ED_Color::operator==(const ED_Color& other) const {
    return m_bDefined == other.m_bDefined &&
        (!m_bDefined || m_color.red == other.m_color.red &&
        m_color.green == other.m_color.green &&
        m_color.blue == other.m_color.blue );
}

XP_Bool ED_Color::operator!=(const ED_Color& other) const {
    return ! operator==(other);
}

XP_Bool ED_Color::IsDefined() { return m_bDefined; }
int ED_Color::Red() { XP_ASSERT(m_bDefined); return m_color.red; }
int ED_Color::Green() { XP_ASSERT(m_bDefined); return m_color.green; }
int ED_Color::Blue() { XP_ASSERT(m_bDefined); return m_color.blue; }
LO_Color ED_Color::GetLOColor() {XP_ASSERT(m_bDefined); return m_color; }

long ED_Color::GetAsLong() {
    if ( ! m_bDefined ) return -1;
    return (((long)m_color.red << 16) | ((long)m_color.green << 8) | (long)m_color.blue);
}

void
ED_Color::SetUndefined() {
	m_bDefined = FALSE;
    m_color.red = 0;
    m_color.green = 0;
    m_color.blue = 0;
}

ED_Color 
ED_Color::GetUndefined()
{
	ED_Color undef;
	undef.SetUndefined();
	return undef;
}

//-----------------------------------------------------------------------------
// Helper functions.
//-----------------------------------------------------------------------------

//
// Convert a tagtype to a string
//

static char* TagString(int32 tagType)
{
    switch(tagType)
    {
        case P_UNKNOWN:
            return "";
        case P_TEXT:
            return "text";
        case P_TITLE:
            return PT_TITLE;
        case P_INDEX:
            return PT_INDEX;
        case P_BASE:
            return PT_BASE;
        case P_LINK:
            return PT_LINK;
        case P_HEADER_1:
            return PT_HEADER_1;
        case P_HEADER_2:
            return PT_HEADER_2;
        case P_HEADER_3:
            return PT_HEADER_3;
        case P_HEADER_4:
            return PT_HEADER_4;
        case P_HEADER_5:
            return PT_HEADER_5;
        case P_HEADER_6:
            return PT_HEADER_6;
        case P_ANCHOR:
            return PT_ANCHOR;
        case P_PARAGRAPH:
            return PT_PARAGRAPH;
        case P_ADDRESS:
            return PT_ADDRESS;
        case P_IMAGE:
            return PT_IMAGE;
        case P_NEW_IMAGE:
            return PT_NEW_IMAGE;
        case P_PLAIN_TEXT:
            return PT_PLAIN_TEXT;
        case P_PLAIN_PIECE:
            return PT_PLAIN_PIECE;
        case P_PREFORMAT:
            return PT_PREFORMAT;
        case P_LISTING_TEXT:
            return PT_LISTING_TEXT;
        case P_UNUM_LIST:
            return PT_UNUM_LIST;
        case P_NUM_LIST:
            return PT_NUM_LIST;
        case P_MENU:
            return PT_MENU;
        case P_DIRECTORY:
            return PT_DIRECTORY;
        case P_LIST_ITEM:
            return PT_LIST_ITEM;
        case P_DESC_LIST:
            return PT_DESC_LIST;
        case P_DESC_TITLE:
            return PT_DESC_TITLE;
        case P_DESC_TEXT:
            return PT_DESC_TEXT;
        case P_STRIKEOUT:
            return PT_STRIKEOUT;
        case P_STRIKE:
            return PT_STRIKE;
        case P_UNDERLINE:
            return PT_UNDERLINE;
        case P_FIXED:
            return PT_FIXED;
        case P_CENTER:
            return PT_CENTER;
        case P_EMBED:
            return PT_EMBED;
        case P_SUBDOC:
            return PT_SUBDOC;
        case P_CELL:
            return PT_CELL;
        case P_TABLE:
            return PT_TABLE;
        case P_CAPTION:
            return PT_CAPTION;
        case P_TABLE_ROW:
            return PT_TABLE_ROW;
        case P_TABLE_HEADER:
            return PT_TABLE_HEADER;
        case P_TABLE_DATA:
            return PT_TABLE_DATA;
        case P_BOLD:
            return PT_BOLD;
        case P_ITALIC:
            return PT_ITALIC;
        case P_EMPHASIZED:
            return PT_EMPHASIZED;
        case P_STRONG:
            return PT_STRONG;
        case P_CODE:
            return PT_CODE;
        case P_SAMPLE:
            return PT_SAMPLE;
        case P_KEYBOARD:
            return PT_KEYBOARD;
        case P_VARIABLE:
            return PT_VARIABLE;
        case P_CITATION:
            return PT_CITATION;
        case P_BLOCKQUOTE:
            return PT_BLOCKQUOTE;
        case P_FORM:
            return PT_FORM;
        case P_INPUT:
            return PT_INPUT;
        case P_SELECT:
            return PT_SELECT;
        case P_OPTION:
            return PT_OPTION;
        case P_TEXTAREA:
            return PT_TEXTAREA;
        case P_HRULE:
            return PT_HRULE;
        case P_BODY:
            return PT_BODY;
        case P_COLORMAP:
            return PT_COLORMAP;
        case P_HYPE:
            return PT_HYPE;
        case P_META:
            return PT_META;
        case P_LINEBREAK:
            return PT_LINEBREAK;
        case P_WORDBREAK:
            return PT_WORDBREAK;
        case P_NOBREAK:
            return PT_NOBREAK;
        case P_BASEFONT:
            return PT_BASEFONT;
        case P_FONT:
            return PT_FONT;
        case P_BLINK:
            return PT_BLINK;
        case P_BIG:
            return PT_BIG;
        case P_SMALL:
            return PT_SMALL;
        case P_SUPER:
            return PT_SUPER;
        case P_SUB:
            return PT_SUB;
        case P_GRID:
            return PT_GRID;
        case P_GRID_CELL:
            return PT_GRID_CELL;
        case P_NOGRIDS:
            return PT_NOGRIDS;
        case P_JAVA_APPLET:
            return PT_JAVA_APPLET;
        case P_MAP:
            return PT_MAP;
        case P_AREA:
            return PT_AREA;
        case P_DIVISION:
            return PT_DIVISION;
        case P_KEYGEN:
            return PT_KEYGEN;
        case P_MAX:
            return "HTML";
        case P_SCRIPT:
            return PT_SCRIPT;
        case P_NOEMBED:
            return PT_NOEMBED;
        case P_SERVER:
            return PT_SERVER;
        case P_PARAM:
            return PT_PARAM;
        case P_MULTICOLUMN:
            return PT_MULTICOLUMN;
        case P_SPACER:
            return PT_SPACER;
        case P_NOSCRIPT:
            return PT_NOSCRIPT;
        case P_HEAD:
            return PT_HEAD;
        case P_HTML:
            return PT_HTML;
        case P_CERTIFICATE:
            return PT_CERTIFICATE;
        case P_MQUOTE:
            return PT_MQUOTE;
        case P_STYLE:
            return PT_STYLE;
        case P_LAYER:
            return PT_LAYER;
        case P_ILAYER:
            return PT_ILAYER;
        case P_OBJECT:
            return PT_OBJECT;
        case P_SPAN:
            return PT_SPAN;
        case P_SPELL:
            return PT_SPELL;
        case P_INLINEINPUT:
            return PT_INLINEINPUT;
        case P_INLINEINPUTTHICK:
            return PT_INLINEINPUTTHICK;
        case P_INLINEINPUTDOTTED:
            return PT_INLINEINPUTDOTTED;
        case P_NSDT:
            return PT_NSDT;
        case P_NSCP_CLOSE:
            return PT_NSCP_OPEN;
        case P_NSCP_OPEN:
            return PT_NSDT;
        case P_NSCP_REBLOCK:
            return PT_NSCP_REBLOCK;
        case P_BLOCK:
            return "block"; // PT_BLOCK 
        case P_NOLAYER:
            return PT_NOLAYER; // (?) jrm 97/03/08 according to instructions below.
#ifdef SHACK
	    case P_BUILTIN:
			return PT_BUILTIN;
#endif
        default:
            // If we get here, then it's a new tag that's been added to lib\libparse\pa_tags.h
            // The fix is to add this new tag to the case statement above.
            XP_ASSERT(FALSE);
            return "UNKNOWN";
    }
}

const int kTagBufferSize = 40;
static char tagBuffer[kTagBufferSize];

char* EDT_UpperCase(char* tagString)
{
    int i = 0;
    if ( tagString ) {
        for ( i = 0; i < kTagBufferSize-1 && tagString[i] != '\0'; i++ ) {
            tagBuffer[i] = XP_TO_UPPER(tagString[i]);
        }
    }
    tagBuffer[i] = '\0';
    return tagBuffer;
}

char *EDT_TagString(int32 tagType){
    return EDT_UpperCase(TagString(tagType));
}

char* EDT_AlignString(ED_Alignment align){
    if ( align < ED_ALIGN_CENTER ) {
        XP_ASSERT(FALSE);
        align = ED_ALIGN_CENTER;
    }
    if ( align > ED_ALIGN_ABSTOP ) {
        XP_ASSERT(FALSE);
        align = ED_ALIGN_ABSTOP;
    }
    return EDT_UpperCase(lo_alignStrings[align]);
}

ED_TextFormat edt_TagType2TextFormat( TagType t ){
    switch(t){
    case P_BOLD:
    case P_STRONG:
        return TF_BOLD;

    case P_ITALIC:
    case P_EMPHASIZED:
    case P_VARIABLE:
    case P_CITATION:
        return TF_ITALIC;

    case P_FIXED:
    case P_CODE:
    case P_KEYBOARD:
    case P_SAMPLE:
        return TF_FIXED;

    case P_SUPER:
        return TF_SUPER;

    case P_SUB:
        return TF_SUB;

    case P_NOBREAK:
        return TF_NOBREAK;

    case P_STRIKEOUT:
        return TF_STRIKEOUT;

    case P_UNDERLINE:
        return TF_UNDERLINE;

    case P_BLINK:
        return TF_BLINK;

    case P_SERVER:
        return TF_SERVER;

    case P_SCRIPT:
        return TF_SCRIPT;

    case P_STYLE:
        return TF_STYLE;

    case P_SPELL:
        return TF_SPELL;

    case P_INLINEINPUT:
        return TF_INLINEINPUT;

    case P_INLINEINPUTTHICK:
        return TF_INLINEINPUTTHICK;

    case P_INLINEINPUTDOTTED:
        return TF_INLINEINPUTDOTTED;
    }
    return TF_NONE;
}

static int workBufSize = 0;
static char* workBuf = 0;

char *edt_WorkBuf( int iSize ){
    if( iSize > workBufSize ){
        if( workBuf != 0 ){
            delete[] workBuf;
        }
        workBuf = new char[ iSize ];
    }
    return workBuf;
}

char *edt_QuoteString( char* pString ){
    int iLen = 0;
    char *p = pString;
    while( p && *p  ){
        if( *p == '"' ){
            iLen += 6;    // &quot;
        }
        else {
            iLen++;
        }
        p++;
    }
    iLen++;
    
    char *pRet = edt_WorkBuf( iLen+1 );
    p = pRet;
    while( pString && *pString ){
        if( *pString == '"' ){
            strcpy( p, "&quot;" );
            p += 6;
        }
        else {
            *p++ = *pString;
        }
        pString++;
    }
    *p = 0;
    return pRet;
}


char *edt_MakeParamString( char* pString ){
    int iLen = 0;
    char *p = pString;
    while( p && *p  ){
        if( *p == '"' ){
            iLen += 6;    // &quot;
        }
        else {
            iLen++;
        }
        p++;
    }
    iLen += 3;      // the beginning and ending quote and the trailing 0.
    
    char *pRet = edt_WorkBuf( iLen+1 );
    p = pRet;
    if( pString && *pString == '`' ){
        do {
            *p++ = *pString++;
        } while( *pString && *pString != '`' );
        *p++ = '`';
        *p = 0;
    }
    else {
        *p = '"';
        p++;
        while( pString && *pString ){
            if( *pString == '"' ){
                strcpy( p, "&quot;" );
                p += 6;
            }
            else {
                *p++ = *pString;
            }
            pString++;
        }
        *p++ = '"';
        *p = 0;
    }
    return pRet;
}

char *edt_FetchParamString( PA_Tag *pTag, char* param, int16 win_csid ){
    PA_Block buff;
    char *str;
    char *retVal = 0;

    buff = PA_FetchParamValue(pTag, param, win_csid);
    if (buff != NULL)
    {
        PA_LOCK(str, char *, buff);
        lo_StripTextWhitespace(str, XP_STRLEN(str));
        if( str ) {
            retVal = XP_STRDUP( str );
        }
        PA_UNLOCK(buff);
        PA_FREE(buff);
    }
    return retVal;
}

XP_Bool edt_FetchParamBoolExist( PA_Tag *pTag, char* param, int16 csid ){
    PA_Block buff;

    buff = PA_FetchParamValue(pTag, param, csid);
    if (buff != NULL){
        PA_FREE(buff);
        return TRUE;
    }
    else {
        return FALSE;
    }
}

XP_Bool edt_FetchDimension( PA_Tag *pTag, char* param, 
                         int32 *pValue, XP_Bool *pPercent,
                         int32 nDefaultValue, XP_Bool bDefaultPercent, int16 csid ){
    PA_Block buff;
    char *str;
    int32   value;

    // Fill in defaults
    *pValue = nDefaultValue;
    *pPercent = bDefaultPercent;
    
    buff = PA_FetchParamValue(pTag, param, csid);
    XP_Bool result = buff != NULL;
    if( buff != NULL )
    {
        PA_LOCK(str, char *, buff);
        if( str != 0 ){
            value = XP_ATOI(str);
            *pValue = value;
            *pPercent = (NULL != XP_STRCHR(str, '%'));
        }
        PA_UNLOCK(buff);
        PA_FREE(buff);
    }
    return result;
}
    
ED_Alignment edt_FetchParamAlignment( PA_Tag* pTag, ED_Alignment eDefault, XP_Bool bVAlign, int16 csid ){
    PA_Block buff;
    char *str;
    ED_Alignment retVal = eDefault;

    buff = PA_FetchParamValue(pTag, bVAlign ? PARAM_VALIGN : PARAM_ALIGN, csid);
    if (buff != NULL)
    {
        XP_Bool floating;

        floating = FALSE;
        PA_LOCK(str, char *, buff);
        lo_StripTextWhitespace(str, XP_STRLEN(str));
        // LTNOTE: this is a hack.  We should do a better job maping these
        //  could cause problems in the future.
        retVal = (ED_Alignment) lo_EvalAlignParam(str, &floating);
        PA_UNLOCK(buff);
        PA_FREE(buff);
    }
    return retVal;
}

int32 edt_FetchParamInt( PA_Tag *pTag, char* param, int32 defValue, int16 csid ){
    return edt_FetchParamInt(pTag, param, defValue, defValue, csid);
}

int32 edt_FetchParamInt( PA_Tag *pTag, char* param, int32 defValue, int32 defValueIfParamButNoValue, int16 csid ){
    PA_Block buff;
    char *str;
    int32 retVal = defValue;

    buff = PA_FetchParamValue(pTag, param, csid);
    if (buff != NULL)
    {
        PA_LOCK(str, char *, buff);
        if( str != 0 ){
            if ( *str == '\0' ) { /* They mentioned the param, but gave to value. */
                retVal = defValueIfParamButNoValue;
            }
            else {
                retVal = XP_ATOI(str);
            }
        }
        PA_UNLOCK(buff);
        PA_FREE(buff);
    }
    return retVal;
}

ED_Color edt_FetchParamColor( PA_Tag *pTag, char* param, int16 csid ){
    PA_Block buff = PA_FetchParamValue(pTag, param, csid);
    ED_Color retVal;
    retVal.SetUndefined();
    if (buff != NULL)
    {
        char *color_str;
        PA_LOCK(color_str, char *, buff);
        lo_StripTextWhitespace(color_str, XP_STRLEN(color_str));
        uint8 red, green, blue;
        LO_ParseRGB(color_str, &red, &green, &blue);
        retVal = EDT_RGB(red,green,blue);
        PA_UNLOCK(buff);
        PA_FREE(buff);
    }
    return retVal;
}

PRIVATE
XP_Bool edt_NameInList( char* pName, char** ppNameList ){
    int i = 0;
    while( ppNameList && ppNameList[i] ){
        if( XP_STRCASECMP( pName, ppNameList[i]) == 0 ){
            return TRUE;
        }
        i++;
    }
    return FALSE;
}

char* edt_FetchParamExtras( PA_Tag *pTag, char**ppKnownParams, int16 win_csid ){
    char** ppNames;
    char** ppValues;
    char* pRet = 0;
    int i = 0;
    char *pSpace = "";
    int iMax;

    iMax = PA_FetchAllNameValues( pTag, &ppNames, &ppValues, win_csid );

    while( i < iMax ){
        if( !edt_NameInList( ppNames[i], ppKnownParams ) ){
            if( ppValues[i] ){
                pRet = PR_sprintf_append( pRet, "%s%s=%s",
                    pSpace, ppNames[i], edt_MakeParamString( ppValues[i] ) );
            }
            else {
                pRet = PR_sprintf_append( pRet, "%s%s", pSpace, ppNames[i] );
            }
            pSpace = " ";
        }
        i++;
    }

    i = 0;
    while( i < iMax ){
        if( ppNames[i] ) XP_FREE( ppNames[i] );
        if( ppValues[i] ) XP_FREE( ppValues[i] );
        i++;
    }

    if( ppNames ) XP_FREE( ppNames );
    if( ppValues ) XP_FREE( ppValues );

    return pRet;
}

// Override existing.
void edt_FetchParamString2(PA_Tag* pTag, char* param, char*& data, int16 win_csid){
    char* result = edt_FetchParamString( pTag, param, win_csid );
    if ( result ) {
        if ( data ){
            XP_FREE(data);
        }
        data = result;
    }
}

// Override existing.
void edt_FetchParamColor2( PA_Tag *pTag, char* param, ED_Color& data, int16 csid ){
    ED_Color result = edt_FetchParamColor(pTag, param, csid);
    if ( result.IsDefined() ) {
        data = result;
    }
}

// Append to existing extras. To Do: Have new versions of a parameter overwrite the old ones.
void edt_FetchParamExtras2( PA_Tag *pTag, char**ppKnownParams, char*& data, int16 win_csid ){
    char* result = edt_FetchParamExtras( pTag, ppKnownParams, win_csid );
    if ( result ) {
        if ( data ) {
            data = PR_sprintf_append(data, " %s", result);
            XP_FREE(result);
        }
        else {
            data = result;
        }
    }
}

XP_Bool edt_ReplaceParamValue( PA_Tag *pTag, char * pName, char * pValue, int16 csid ){
    XP_ASSERT(pName);
    XP_ASSERT(pTag);

    char** ppNames;
    char** ppValues;
    char* pNew = 0;
    int i = 0;
    char *pSpace = "";
    int iMax;
    XP_Bool bFound = FALSE;
    
    iMax = PA_FetchAllNameValues( pTag, &ppNames, &ppValues, csid );

    while( i < iMax ){
        if( XP_STRCASECMP( pName, ppNames[i]) == 0 ){
            bFound = TRUE;
            if( pValue){
                pNew = PR_sprintf_append( pNew, "%s%s=%s",
                    pSpace, pName, edt_MakeParamString( pValue ) );
            }
        } else {
            if( ppValues[i] ){
                pNew = PR_sprintf_append( pNew, "%s%s=%s",
                    pSpace, ppNames[i], edt_MakeParamString( ppValues[i] ) );
            }
            else {
                pNew = PR_sprintf_append( pNew, "%s%s", pSpace, ppNames[i] );
            }
            pSpace = " ";
        }
        i++;
    }
    // Add value at end if not found
    if( !bFound && pValue ){
        pNew = PR_sprintf_append( pNew, "%s%s=%s", pSpace, pName, pValue );
    }
    // Terminate
    pNew = PR_sprintf_append( pNew, "%s", ">" );

    // Cleanup memory
    i = 0;
    while( i < iMax ){
        if( ppNames[i] ) XP_FREE( ppNames[i] );
        if( ppValues[i] ) XP_FREE( ppValues[i] );
        i++;
    }

    if( ppNames ) XP_FREE( ppNames );
    if( ppValues ) XP_FREE( ppValues );
    
    // Now replace with new data

    PA_Block buff;
    char *locked_buff;
    int iLen;

    iLen = XP_STRLEN(pNew);
    buff = (PA_Block)PA_ALLOC((iLen+1) * sizeof(char));
    if (buff != NULL) {
        PA_LOCK(locked_buff, char *, buff);
        XP_BCOPY(pNew, locked_buff, iLen);
        locked_buff[iLen] = '\0';
        PA_UNLOCK(buff);

        if( pTag->data ) XP_FREE(pTag->data);
        pTag->data = buff;
        pTag->data_len = iLen;
    } else { 
        // LTNOTE: out of memory, should throw an exception
    }
    return bFound;
}

LO_Color* edt_MakeLoColor(ED_Color c) {
    if( !c.IsDefined()){
        return 0;
    }
    LO_Color *pColor = XP_NEW( LO_Color );
    pColor->red = EDT_RED(c);
    pColor->green = EDT_GREEN(c);
    pColor->blue = EDT_BLUE(c);
    return pColor;
}

void edt_SetLoColor( ED_Color c, LO_Color *pColor ){
    if( !c.IsDefined()){
        XP_ASSERT(FALSE);
        return;
    }
    pColor->red = EDT_RED(c);
    pColor->green = EDT_GREEN(c);
    pColor->blue = EDT_BLUE(c);
}


#define CHARSET_SIZE    256
static PA_AmpEsc *ed_escapes[CHARSET_SIZE];

void edt_InitEscapes(int16 /*csid*/, XP_Bool bQuoteHiBits){
    PA_AmpEsc *pEsc = PA_AmpEscapes;

    XP_BZERO( ed_escapes, CHARSET_SIZE * sizeof( PA_AmpEsc* ) );

    while( pEsc->value ){
		int ch = 0xff & (int) pEsc->value;
        if( ed_escapes[ ch ] == 0 ){
            if ( ch == '&' || ch == '<'
				|| (NON_BREAKING_SPACE == ((char) ch))			
				|| (ch > 128 && bQuoteHiBits ) ) {
           	    ed_escapes[ ch ] = pEsc;
            }
        }
        pEsc++;
    }
}

void edt_PrintWithEscapes( CPrintState *ps, char *p, XP_Bool bBreakLines ){
    int csid = ps->m_pBuffer->GetRAMCharSetID();
    char *pBegin;
    PA_AmpEsc *pEsc;

    // break the lines after 70 characters if we can, but don't really do it 
    // in the formatted case.
    while( p && *p ){
        pBegin = p;
        while( *p && ps->m_iCharPos < 70 && ed_escapes[(unsigned char)*p] == 0
            && ((unsigned char) *p) != ((unsigned char) NON_BREAKING_SPACE) ){
            char* p2 = INTL_NextChar(csid, p);
            int charWidth = p2 - p;
            ps->m_iCharPos += charWidth;
            p = p2;
        }
        while( *p && *p != ' ' && ed_escapes[(unsigned char)*p] == 0
            && ((unsigned char) *p) != ((unsigned char) NON_BREAKING_SPACE)){
            char* p2 = INTL_NextChar(csid, p);
            int charWidth = p2 - p;
            ps->m_iCharPos += charWidth;
            p = p2;
        }
        ps->m_pOut->Write( pBegin, p-pBegin);

        if( *p && *p == ' ' && ps->m_iCharPos >= 70 ){
            if( bBreakLines ){
                ps->m_pOut->Write( "\n", 1 );
            }
            else {
                ps->m_pOut->Write( " ", 1 );
            }
            ps->m_iCharPos = 0;
        }
        else if( *p && (pEsc = ed_escapes[(unsigned char)*p]) != 0 ){
            ps->m_pOut->Write( "&",1 );
            ps->m_pOut->Write( pEsc->str, pEsc->len );
            ps->m_pOut->Write( ";",1 );
            ps->m_iCharPos += pEsc->len+2;
        }
        else if (((unsigned char) *p) == ((unsigned char) NON_BREAKING_SPACE) ){
            const char* nbsp = INTL_NonBreakingSpace(csid);
            int nbsp_len = XP_STRLEN(nbsp);
            ps->m_pOut->Write( (char*) nbsp, XP_STRLEN(nbsp));
        }

        // move past the space or special char
        if( *p ){
            p = INTL_NextChar(csid, p);
        }
    }
}

char *edt_LocalName( char *pURL ){
    char *pDest = FE_URLToLocalName( pURL );
#if 0
    if( pDest && FE_EditorPrefConvertFileCaseOnWrite( ) ){
        char *url_ptr = pDest;
        while ( *url_ptr != '\0' ){
            *url_ptr = XP_TO_LOWER(*url_ptr); 
            url_ptr++;
        }
    }
#endif
    return pDest;
}

PA_Block PA_strdup( char* s ){
    char *new_str;
    PA_Block new_buff;

    if( s == 0 ){
        return 0;
    }

    new_buff = (PA_Block)PA_ALLOC(XP_STRLEN(s) + 1);
    if (new_buff != NULL)
    {
        PA_LOCK(new_str, char *, new_buff);
        XP_STRCPY(new_str, s);
        PA_UNLOCK(new_buff);
    }
    return new_buff;
}

void edt_SetTagData( PA_Tag* pTag, char* pTagData){
    PA_Block buff;
    char *locked_buff;
    int iLen;

    iLen = XP_STRLEN(pTagData);
    buff = (PA_Block)PA_ALLOC((iLen+1) * sizeof(char));
    if (buff != NULL)
    {
        PA_LOCK(locked_buff, char *, buff);
        XP_BCOPY(pTagData, locked_buff, iLen);
        locked_buff[iLen] = '\0';
        PA_UNLOCK(buff);
    }
    else { 
        // LTNOTE: out of memory, should throw an exception
        return;
    }
    pTag->data = buff;
    pTag->data_len = iLen;
    pTag->next = NULL;
    return;
}

//
// Create a tag and add it to the list
//
void edt_AddTag( PA_Tag*& pStart, PA_Tag*& pEnd, TagType t, XP_Bool bIsEnd,
        char *pTagData  ){
    PA_Tag *pTag = XP_NEW( PA_Tag );
    XP_BZERO( pTag, sizeof( PA_Tag ) );
    pTag->type = t;
    pTag->is_end = bIsEnd;
    edt_SetTagData(pTag, pTagData ? pTagData : ">" );
    if( pStart == 0 ){
        pStart = pTag;
    }
    if( pEnd ){
        pEnd->next = pTag;
    }
    pEnd = pTag;
}



//-----------------------------------------------------------------------------
// CEditPosition
//-----------------------------------------------------------------------------

void CEditPositionComparable::CalcPosition( TXP_GrowableArray_int32* pA, 
            CEditPosition *pPos ){

    CEditElement *pElement, *pParent, *pSib;
    int i;

    pA->Empty();
    pA->Add( pPos->Offset() );
    pElement = pPos->Element();
    if( pElement ){
        while( (pParent = pElement->GetParent()) != NULL ){
            i = 0;
            pSib = pParent->GetChild();        
            while( pSib != pElement ){
                pSib = pSib->GetNextSibling();
                i++;
            }
            pA->Add(i);
            pElement = pParent;
        }
    }
}

int CEditPositionComparable::Compare( CEditPosition *pPos ){
    TXP_GrowableArray_int32 toArray;

    CalcPosition( &toArray, pPos );

    int32 i = m_Array.Size();
    int32 iTo = toArray.Size();
    int32 iDiff;

    while( i && iTo ){
        iDiff = toArray[--iTo] - m_Array[--i];
        if( iDiff != 0 ) {
            return (iDiff > 0 ? 1 : -1 );
        }
    }
    if( i == 0 && iTo == 0){
        return 0;
    }
    iDiff = iTo - i;
    return (iDiff > 0 ? 1 : -1 );
}


// CEditInsertPoint

CEditInsertPoint::CEditInsertPoint() {
    m_pElement = 0;
    m_iPos = 0;
    m_bStickyAfter = FALSE;
}

CEditInsertPoint::CEditInsertPoint(CEditLeafElement* pElement, ElementOffset iPos) {
    m_pElement = pElement;
    m_iPos = iPos;
    m_bStickyAfter = FALSE;
}

CEditInsertPoint::CEditInsertPoint(CEditElement* pElement, ElementOffset iPos){
    m_pElement = pElement->Leaf();
    m_iPos = iPos;
    m_bStickyAfter = FALSE;
}

CEditInsertPoint::CEditInsertPoint(CEditElement* pElement, ElementOffset iPos, XP_Bool bStickyAfter){
    m_pElement = pElement->Leaf();
    m_iPos = iPos;
    m_bStickyAfter = bStickyAfter;
}

XP_Bool CEditInsertPoint::operator==(const CEditInsertPoint& other ) {
    return m_pElement == other.m_pElement && m_iPos == other.m_iPos;
}

XP_Bool CEditInsertPoint::operator!=(const CEditInsertPoint& other ) {
    return ! (*this == other);
}

XP_Bool CEditInsertPoint::operator<(const CEditInsertPoint& other ) {
    return Compare(other) < 0;
}

XP_Bool CEditInsertPoint::operator<=(const CEditInsertPoint& other ) {
    return Compare(other) <= 0;
}

XP_Bool CEditInsertPoint::operator>=(const CEditInsertPoint& other ) {
    return Compare(other) >= 0;
}

XP_Bool CEditInsertPoint::operator>(const CEditInsertPoint& other ) {
    return Compare(other) > 0;
}

CEditElement* CEditInsertPoint::FindNonEmptyElement( CEditElement *pStartElement ){
    CEditElement* pOldElement = NULL;
    while ( pStartElement && pStartElement != pOldElement ) {
        pOldElement = pStartElement;
        if( !pStartElement->IsLeaf() ){
            pStartElement = pStartElement->PreviousLeaf();
        }
        else if( pStartElement->Leaf()->GetLayoutElement() == 0 ){
           pStartElement = pStartElement->PreviousLeaf();
        }
    }
    return pStartElement;
}

int CEditInsertPoint::Compare(const CEditInsertPoint& other ) {
    XP_Bool bAIsBreak = FALSE;
    LO_Element* pA = m_pElement->GetLayoutElement();
    if ( ! pA ) {
        bAIsBreak = TRUE;
        pA = FindNonEmptyElement(m_pElement)->Leaf()->GetLayoutElement();
    }
    XP_Bool bBIsBreak = FALSE;
    LO_Element* pB = other.m_pElement->GetLayoutElement();
    if ( ! pB ) {
        bBIsBreak = TRUE;
        pB = FindNonEmptyElement(other.m_pElement)->Leaf()->GetLayoutElement();
    }
    if ( !pA || !pB ) {
        XP_ASSERT(FALSE); // Phantom insert points.
        return 0;
    }
    int32 aIndex = pA->lo_any.ele_id;
    int32 bIndex = pB->lo_any.ele_id;
    if ( aIndex < bIndex ) {
        return -1;
    }
    else if ( aIndex == bIndex ) {
        // Same element. Compare positions.
        if ( m_iPos < other.m_iPos ) {
            return -1;
        }
        else if ( m_iPos == other.m_iPos ) {
            if ( bAIsBreak < bBIsBreak ) {
                return -1;
            }
            else if ( bAIsBreak == bBIsBreak ) {
                return 0;
            }
            else {
                return 1;
            }
        }
        else {
            return 1;
        }
    }
    else {
        return 1;
    }
}

XP_Bool CEditInsertPoint::IsDenormalizedVersionOf(const CEditInsertPoint& other){
    return other.m_iPos == 0 &&
        m_iPos == m_pElement->GetLen() &&
        m_pElement->NextLeaf() == other.m_pElement;
}

XP_Bool CEditInsertPoint::IsStartOfElement() {
    return m_iPos <= 0;
}

XP_Bool CEditInsertPoint::IsEndOfElement() {
    return m_iPos >= m_pElement->GetLen();
}

XP_Bool CEditInsertPoint::IsStartOfContainer() {
    return IsStartOfElement()
        && m_pElement->PreviousLeafInContainer() == NULL;
}

XP_Bool CEditInsertPoint::IsEndOfContainer() {
    return IsEndOfElement()
        && m_pElement->LeafInContainerAfter() == NULL;
}

XP_Bool CEditInsertPoint::IsStartOfDocument(){
    return IsStartOfElement() &&
        m_pElement->PreviousLeaf() == NULL;
}

XP_Bool CEditInsertPoint::IsEndOfDocument(){
    return m_pElement->IsEndOfDocument();
}

XP_Bool CEditInsertPoint::GapWithBothSidesAllowed(){
    XP_Bool bResult = FALSE;
    XP_Bool bAllowBothSidesOfGap = m_pElement->AllowBothSidesOfGap();
    if ( bAllowBothSidesOfGap
            && IsEndOfElement()
            && !IsEndOfContainer() ) {
        bResult = TRUE;
    }
    else if ( IsStartOfElement() && ! IsStartOfContainer() ) {
        if ( bAllowBothSidesOfGap ) {
            bResult = TRUE;
        }
        else {
            CEditLeafElement* pPrev = m_pElement->PreviousLeaf();
            if ( pPrev && pPrev->AllowBothSidesOfGap() ) {
                bResult = TRUE;
            }
        }
    }
    return bResult;
}

XP_Bool CEditInsertPoint::IsLineBreak(){
    return IsHardLineBreak() || IsSoftLineBreak();
}

XP_Bool CEditInsertPoint::IsSoftLineBreak(){
    XP_Bool result = FALSE;
    CEditTextElement* pText = CEditTextElement::Cast(m_pElement);
    if ( pText ){
        int iOffset;
        LO_Element* pLOElement;
        if ( pText->GetLOElementAndOffset(m_iPos, m_bStickyAfter,
                pLOElement, iOffset) ){
            if ( pLOElement->type == LO_LINEFEED ){
                result = TRUE;
            }
        }
    }
    return result;
}

XP_Bool CEditInsertPoint::IsHardLineBreak(){
    return IsStartOfElement() && m_pElement->CausesBreakBefore()
        || IsEndOfElement() && m_pElement->CausesBreakAfter();
}

XP_Bool CEditInsertPoint::IsSpace() {
    XP_Bool result = FALSE;
    if ( m_pElement->IsA(P_TEXT) ) {
        if ( m_iPos == m_pElement->GetLen() ){
            CEditLeafElement *pNext = m_pElement->TextInContainerAfter();
            if( pNext
                && pNext->IsA(P_TEXT)
                && pNext->Text()->GetLen() != 0
                && pNext->Text()->GetText()[0] == ' ') {
                result = TRUE;
            }
        }
        else if ( m_pElement->Text()->GetText()[m_iPos] == ' ' ) {
            result = TRUE;
        }
    }
    return result;
}

XP_Bool CEditInsertPoint::IsSpaceBeforeOrAfter() {
    XP_Bool result = IsSpace();
    if ( !result ) {
        CEditInsertPoint before = PreviousPosition();
        if ( before != *this ) {
            result = before.IsSpace();
        }
    }
    return result;
}

CEditInsertPoint CEditInsertPoint::NextPosition(){
    CEditInsertPoint result;
    m_pElement->NextPosition(m_iPos, result.m_pElement, result.m_iPos);
    return result;
}

CEditInsertPoint CEditInsertPoint::PreviousPosition(){
    CEditInsertPoint result;
    m_pElement->PrevPosition(m_iPos, result.m_pElement, result.m_iPos);
    // Work around what is probably a bug in PrevPosition.
    if ( m_iPos == 0 && result.m_iPos == result.m_pElement->GetLen() ) {
        result.m_pElement->PrevPosition(result.m_iPos, result.m_pElement, result.m_iPos);
    }
    return result;
}

#ifdef DEBUG
void CEditInsertPoint::Print(IStreamOut& stream) {
    stream.Printf("0x%08lx.%d%s", m_pElement, m_iPos, m_bStickyAfter ? "+" : "" );
}
#endif



// CEditSelection

CEditSelection::CEditSelection(){
    m_bFromStart = FALSE;
}

CEditSelection::CEditSelection(CEditElement* pStart, intn iStartPos,
    CEditElement* pEnd, intn iEndPos, XP_Bool fromStart)
    : m_start(pStart, iStartPos), m_end(pEnd, iEndPos), m_bFromStart(fromStart)
{
}

CEditSelection::CEditSelection(const CEditInsertPoint& start,
    const CEditInsertPoint& end, XP_Bool fromStart)
    : m_start(start), m_end(end), m_bFromStart(fromStart)
{
}

XP_Bool CEditSelection::operator==(const CEditSelection& other ){
    return m_start == other.m_start &&
    m_end == other.m_end && m_bFromStart == other.m_bFromStart;
}

XP_Bool CEditSelection::operator!=(const CEditSelection& other ){
    return ! (*this == other);
}

XP_Bool CEditSelection::EqualRange(CEditSelection& other){
    return m_start == other.m_start && m_end == other.m_end;
}

XP_Bool CEditSelection::IsInsertPoint() {
    /* It's an insert point if the two edges are equal, OR if the
     * start is at the end of an element, and the
     * end is at the start of an element, and
     * the two elements are next to each other,
     * and they're in the same container. (Whew!)
     */
    return m_start == m_end ||
        ( m_start.IsEndOfElement() &&
          m_end.IsStartOfElement() &&
          m_start.m_pElement->LeafInContainerAfter() == m_end.m_pElement
        );
}

CEditInsertPoint* CEditSelection::GetEdge(XP_Bool bEnd){
    if ( bEnd ) return &m_end;
    else return &m_start;
}

CEditInsertPoint* CEditSelection::GetActiveEdge(){
     return GetEdge(!m_bFromStart);
}

CEditInsertPoint* CEditSelection::GetAnchorEdge(){
    return GetEdge(m_bFromStart);
}

XP_Bool CEditSelection::Intersects(CEditSelection& other) {
    return m_start < other.m_end && other.m_start < m_end;
}

XP_Bool CEditSelection::Contains(CEditInsertPoint& point) {
    return m_start <= point && point < m_end;
}

XP_Bool CEditSelection::Contains(CEditSelection& other) {
    return ContainsStart(other) && other.m_end <= m_end;
}

XP_Bool CEditSelection::ContainsStart(CEditSelection& other) {
    return Contains(other.m_start);
}

XP_Bool CEditSelection::ContainsEnd(CEditSelection& other) {
    return Contains(other.m_end) || other.m_end == m_end;
}

XP_Bool CEditSelection::ContainsEdge(CEditSelection& other, XP_Bool bEnd){
    if ( bEnd )
        return ContainsEnd(other);
    else
        return ContainsStart(other);
}

XP_Bool CEditSelection::CrossesOver(CEditSelection& other) {
    XP_Bool bContainsStart = ContainsStart(other);
    XP_Bool bContainsEnd = ContainsEnd(other);
    return bContainsStart ^ bContainsEnd;
}

XP_Bool CEditSelection::ClipTo(CEditSelection& other) {
    // True if the result is defined. No change to this if it isn't
    XP_Bool intersects = Intersects(other);
    if ( intersects ) {
        if ( m_start < other.m_start ) {
            m_start = other.m_start;
        }
        if ( other.m_end < m_end ) {
            m_end = other.m_end;
        }
    }
    return intersects;
}

CEditElement* CEditSelection::GetCommonAncestor(){
    return m_start.m_pElement->GetCommonAncestor(m_end.m_pElement);
}

XP_Bool CEditSelection::CrossesSubDocBoundary(){
    // The intent of this method is "If this selection were cut, would the
    // document become malformed?"

    // A document could be malformed if the selection crosses the table
    // boundary, or if it would leave a table without it's protective
    // buffer of containers.

    XP_Bool result = FALSE;
    if ( ! IsInsertPoint() ){
        CEditElement* pStartCell = m_start.m_pElement->GetSubDocOrLayerSkipRoot();
        CEditElement* pEndCell = m_end.m_pElement->GetSubDocOrLayerSkipRoot();
        CEditElement* pClosedEndCell = GetClosedEndContainer()->GetSubDocOrLayerSkipRoot();
        result = pStartCell != pEndCell || pStartCell != pClosedEndCell;
        if ( result ) {
            // This is only OK if the selection spans an entire table.
            Bool bStartBad = FALSE;
            Bool bEndBad = FALSE;
            if ( pStartCell ) {
                CEditSelection all;
                pStartCell->GetTableOrLayerIgnoreSubdoc()->GetAll(all);
                bStartBad = ! Contains(all);
            }

            if ( pEndCell ) {
                CEditSelection all;
                pEndCell->GetTableOrLayerIgnoreSubdoc()->GetAll(all);
                bEndBad = ! Contains(all);
            }
            result = bStartBad || bEndBad;
        }
        else if ( pStartCell && pEndCell != pClosedEndCell ) {
            // If the selection spans an entire cell, but not an entire table, that's bad
            CEditSelection all;
            pStartCell->GetTableOrLayerIgnoreSubdoc()->GetAll(all);
            result = *this != all;
        }
        else {
            // One further check: If the selection starts at the start of
            // a paragraph, and the selection ends at the end of a paragraph
            // and the previous item is a table, and the
            // next item is not a container, then we can't cut. The reason is
            // that tables need to have containers after them.

            // Note that m_end is one more than is selected.
            if ( m_start.IsStartOfContainer() &&
                m_end.IsStartOfContainer() ) {
                // The selection spans whole containers.
                CEditContainerElement* pStartContainer = GetStartContainer();
                CEditElement* pPreviousSib = pStartContainer->GetPreviousSibling();
                
                if ( pPreviousSib && pPreviousSib->IsTable() ) {
                    CEditContainerElement* pEndContainer = GetClosedEndContainer();
                    CEditElement* pNextSib = pEndContainer->GetNextSibling();
                    if ( !pNextSib || pNextSib->IsEndContainer() || pNextSib->IsTable() ) {
                        result = TRUE;
                    }
                }
            }
        }
    }
    return result;
}

CEditSubDocElement* CEditSelection::GetEffectiveSubDoc(XP_Bool bEnd){
    CEditInsertPoint* pEdge = GetEdge(bEnd);
    CEditSubDocElement* pSubDoc = pEdge->m_pElement->GetSubDoc();
    while ( pSubDoc ) {
        // If the edge is actually the edge of a subdoc, skip out of the subdoc.
        CEditSelection wholeSubDoc;
        pSubDoc->GetAll(wholeSubDoc);
        // If this is a wholeSubDoc Selection, bump up
        if ( *this != wholeSubDoc ) {
            if ( *pEdge != *wholeSubDoc.GetEdge(bEnd) )
                break;
            // If the other end of the selection is is in this same subdoc, stop
            if ( wholeSubDoc.ContainsEdge(*this, !bEnd) )
                break;
        }
        CEditElement* pSubDocParent = pSubDoc->GetParent();
        if ( !pSubDocParent )
            break;
        CEditSubDocElement* pParentSubDoc = pSubDocParent->GetSubDoc();
        if ( ! pParentSubDoc )
            break;
        pSubDoc = pParentSubDoc;
    }
    return pSubDoc;
}

XP_Bool CEditSelection::ExpandToNotCrossStructures(){
    XP_Bool bChanged = FALSE;
    if ( ! IsInsertPoint() ){
        CEditElement* pGrandfatherStart = m_start.m_pElement->GetParent()->GetParent();
        CEditElement* pGrandfatherEnd;
        CEditElement* pEffectiveEndContianer;
        if ( m_end.IsEndOfDocument() || EndsAtStartOfContainer()) {
            pEffectiveEndContianer = m_end.m_pElement->PreviousLeaf()->GetParent();
        }
        else {
            pEffectiveEndContianer = m_end.m_pElement->GetParent();
        }
        pGrandfatherEnd = pEffectiveEndContianer->GetParent();
        if ( pGrandfatherStart != pGrandfatherEnd ){
            bChanged = TRUE;
            CEditElement* pAncestor = pGrandfatherStart->GetCommonAncestor(pGrandfatherEnd);
            if ( ! pAncestor ) {
                XP_ASSERT(FALSE);
                return FALSE;
            }
            // If we're in a table, we need to return the whole table.
            if ( pAncestor->IsTableRow() ) {
                pAncestor = pAncestor->GetTable();
                if ( ! pAncestor ) {
                    XP_ASSERT(FALSE);
                    return FALSE;
                }
            }
            if ( pAncestor->IsTable() ) {
                pAncestor->GetAll(*this);
                return TRUE;
            }
            CEditLeafElement* pCleanStart = pAncestor->GetChildContaining(m_start.m_pElement)->GetFirstMostChild()->Leaf();
            CEditLeafElement* pCleanEnd = pAncestor->GetChildContaining(pEffectiveEndContianer)->GetLastMostChild()->NextLeaf();
            m_start.m_pElement = pCleanStart;
            m_start.m_iPos = 0;
            if ( pCleanEnd ) {
                m_end.m_pElement = pCleanEnd;
                m_end.m_iPos = 0;
            }
            else {
                XP_ASSERT(FALSE);   // Somehow we've lost the end of the document element.
                pCleanEnd = m_end.m_pElement->GetParent()->GetLastMostChild()->Leaf();
                m_end.m_pElement = pCleanEnd;
                m_end.m_iPos = pCleanEnd->Leaf()->GetLen();
            }
        }
    }
    return bChanged;
}

void CEditSelection::ExpandToBeCutable(){
    // Very similar to ExpandToNotCrossDocumentStructures, except that
    // the result is always Cutable.
    if ( ! IsInsertPoint() ){
        CEditElement* pGrandfatherStart = m_start.m_pElement->GetParent()->GetParent();
        CEditElement* pGrandfatherEnd;
        CEditElement* pEffectiveEndContianer;
        if ( m_end.IsEndOfDocument()) {
            pEffectiveEndContianer = m_end.m_pElement->PreviousLeaf()->GetParent();
        }
        else {
            pEffectiveEndContianer = m_end.m_pElement->GetParent();
        }
        pGrandfatherEnd = pEffectiveEndContianer->GetParent();
        if ( pGrandfatherStart != pGrandfatherEnd ){
            CEditElement* pAncestor = pGrandfatherStart->GetCommonAncestor(pGrandfatherEnd);
            if ( ! pAncestor ) {
                XP_ASSERT(FALSE);
                return;
            }
            // If we're in a table, we need to return the whole table.
            if ( pAncestor->IsTableRow() ) {
                pAncestor = pAncestor->GetTable();
                if ( ! pAncestor ) {
                    XP_ASSERT(FALSE);
                    return;
                }
            }
            if ( pAncestor->IsTable() ) {
                pAncestor->GetAll(*this);
                return;
            }
            CEditLeafElement* pCleanStart = pAncestor->GetChildContaining(m_start.m_pElement)->GetFirstMostChild()->Leaf();
            CEditLeafElement* pCleanEnd = pAncestor->GetChildContaining(pEffectiveEndContianer)->GetLastMostChild()->NextLeaf();
            m_start.m_pElement = pCleanStart;
            m_start.m_iPos = 0;
            if ( pCleanEnd ) {
                m_end.m_pElement = pCleanEnd;
                m_end.m_iPos = 0;
            }
            else {
                XP_ASSERT(FALSE);   // Somehow we've lost the end of the document element.
                pCleanEnd = m_end.m_pElement->GetParent()->GetLastMostChild()->Leaf();
                m_end.m_pElement = pCleanEnd;
                m_end.m_iPos = pCleanEnd->Leaf()->GetLen();
            }
        }
    }
}

void CEditSelection::ExpandToIncludeFragileSpaces() {
    // Expand the selection to include spaces that will be
    // trimmed if we did a cut of the original selection.
    if ( ! IsInsertPoint() ){
        CEditInsertPoint before = m_start.PreviousPosition();
        // Either we backed up one position, or we hit the beginning
        // of the document.
        if ( before == m_start || before.IsSpace() ) {
            if ( m_end.IsSpace() ) {
                if ( ! m_end.m_pElement->InFormattedText() ) {
                    m_end = m_end.NextPosition();
                }
            }
        }
    }
}

void CEditSelection::ExpandToEncloseWholeContainers(){
    XP_Bool bWasInsertPoint = IsInsertPoint();
    CEditLeafElement* pCleanStart = m_start.m_pElement->GetParent()->GetFirstMostChild()->Leaf();
    m_start.m_pElement = pCleanStart;
    m_start.m_iPos = 0;
    if ( !m_end.IsStartOfContainer() || bWasInsertPoint ) {
        CEditLeafElement* pCleanEnd = m_end.m_pElement->GetParent()->GetLastMostChild()->NextLeaf();
        if ( pCleanEnd ) {
            m_end.m_pElement = pCleanEnd;
            m_end.m_iPos = 0;
        }
        else {
            XP_ASSERT(FALSE);   // Somehow we've lost the end of the document element.
            pCleanEnd = m_end.m_pElement->GetParent()->GetLastMostChild()->Leaf();
            m_end.m_pElement = pCleanEnd;
            m_end.m_iPos = pCleanEnd->Leaf()->GetLen();
        }
    }
}


XP_Bool CEditSelection::EndsAtStartOfContainer() {
    return m_end.IsStartOfContainer();
}

XP_Bool CEditSelection::StartsAtEndOfContainer() {
    return m_start.IsEndOfContainer();
}

XP_Bool CEditSelection::AnyLeavesSelected(){
    // FALSE if insert point or container end.
    return ! (IsInsertPoint() || IsContainerEnd());
}

XP_Bool CEditSelection::IsContainerEnd(){
    XP_Bool result = FALSE;
    if ( ! IsInsertPoint() && EndsAtStartOfContainer()
        && StartsAtEndOfContainer() ) {
        CEditContainerElement* pStartContainer = m_start.m_pElement->FindContainer();
        CEditContainerElement* pEndContainer = m_end.m_pElement->FindContainer();
        if ( pStartContainer && pEndContainer ) {
            CEditContainerElement* pNextContainer =
                CEditContainerElement::Cast(pStartContainer->FindNextElement(&CEditElement::FindContainer, 0));
            if ( pNextContainer == pEndContainer) {
                result = TRUE;
            }
        }
    }
    return result;
}

void CEditSelection::ExcludeLastDocumentContainerEnd() {
    // Useful for cut & delete, where you don't replace the final container mark
    if ( ContainsLastDocumentContainerEnd() ){
        CEditLeafElement* pEnd = m_end.m_pElement;
        pEnd = pEnd->PreviousLeaf();
        if ( pEnd ) {
            m_end.m_pElement = pEnd;
            m_end.m_iPos = pEnd->Leaf()->GetLen();
            if ( m_start > m_end ) {
                m_start = m_end;
            }
        }
    }
}

XP_Bool CEditSelection::ContainsLastDocumentContainerEnd() {
    // Useful for cut & delete, where you don't replace the final container mark
    XP_Bool bResult = FALSE;
    CEditLeafElement* pEnd = m_end.m_pElement;
    if ( pEnd && pEnd->GetElementType() == eEndElement ){
        bResult = TRUE;
    }
    return bResult;
}

CEditContainerElement* CEditSelection::GetStartContainer() {
    CEditElement* pParent = m_start.m_pElement->GetParent();
    if ( pParent && pParent->IsContainer() ) return pParent->Container();
    else return NULL;
}

CEditContainerElement* CEditSelection::GetClosedEndContainer() {
    CEditElement* pEnd = m_end.m_pElement;
    if ( m_end.IsStartOfContainer() ) {
        // Back up one
        CEditElement* pPrev = pEnd->PreviousLeaf();
        if ( pPrev ) {
            pEnd = pPrev;
        }
        else pEnd = m_start.m_pElement;
    }
    CEditElement* pParent = pEnd->GetParent();
    if ( pParent && pParent->IsContainer() ) return pParent->Container();
    else return NULL;
}

#ifdef DEBUG
void CEditSelection::Print(IStreamOut& stream){
    stream.Printf("start ");
    m_start.Print(stream);
    stream.Printf(" end ");
    m_end.Print(stream);
    stream.Printf(" FromStart %ld", (long)m_bFromStart);
}
#endif

// Persistent selections
CPersistentEditInsertPoint::CPersistentEditInsertPoint()
{
    m_index = -1;
    m_bStickyAfter = TRUE;
}

CPersistentEditInsertPoint::CPersistentEditInsertPoint(ElementIndex index)
    : m_index(index), m_bStickyAfter(TRUE)
{}

CPersistentEditInsertPoint::CPersistentEditInsertPoint(ElementIndex index, XP_Bool bStickyAfter)
    : m_index(index), m_bStickyAfter(bStickyAfter)
{
}

XP_Bool CPersistentEditInsertPoint::operator==(const CPersistentEditInsertPoint& other ) {
    return m_index == other.m_index;
}

XP_Bool CPersistentEditInsertPoint::operator!=(const CPersistentEditInsertPoint& other ) {
    return ! (*this == other);
}

XP_Bool CPersistentEditInsertPoint::operator<(const CPersistentEditInsertPoint& other ) {
    return m_index < other.m_index;
}

XP_Bool CPersistentEditInsertPoint::operator<=(const CPersistentEditInsertPoint& other ) {
    return *this < other || *this == other;
}

XP_Bool CPersistentEditInsertPoint::operator>=(const CPersistentEditInsertPoint& other ) {
    return ! (*this < other);
}

XP_Bool CPersistentEditInsertPoint::operator>(const CPersistentEditInsertPoint& other ) {
    return ! (*this <= other);
}

#ifdef DEBUG
void CPersistentEditInsertPoint::Print(IStreamOut& stream) {
    stream.Printf("%ld%s", (long)m_index, m_bStickyAfter ? "+" : "" );
}
#endif

void CPersistentEditInsertPoint::ComputeDifference(
    CPersistentEditInsertPoint& other, CPersistentEditInsertPoint& delta){
    // delta = this - other;
    delta.m_index = m_index - other.m_index;
}

void CPersistentEditInsertPoint::AddRelative(
    CPersistentEditInsertPoint& delta, CPersistentEditInsertPoint& result){
    // result = this + delta;
    result.m_index = m_index + delta.m_index;
}

XP_Bool CPersistentEditInsertPoint::IsEqualUI(
    const CPersistentEditInsertPoint& other ) {
    return m_index == other.m_index && m_bStickyAfter == other.m_bStickyAfter;
}

// class CPersistentEditSelection

CPersistentEditSelection::CPersistentEditSelection()
{
    m_bFromStart = FALSE;
}

CPersistentEditSelection::CPersistentEditSelection(const CPersistentEditInsertPoint& start, const CPersistentEditInsertPoint& end)
    : m_start(start), m_end(end)
{
    m_bFromStart = FALSE;
}

ElementIndex CPersistentEditSelection::GetCount() {
    return m_end.m_index - m_start.m_index;
}

XP_Bool CPersistentEditSelection::IsInsertPoint(){
    return m_start == m_end;
}

XP_Bool CPersistentEditSelection::operator==(const CPersistentEditSelection& other ) {
    return SelectedRangeEqual(other) && m_bFromStart == other.m_bFromStart;
}

XP_Bool CPersistentEditSelection::operator!=(const CPersistentEditSelection& other ) {
    return ! ( *this == other );
}

XP_Bool CPersistentEditSelection::SelectedRangeEqual(const CPersistentEditSelection& other ) {
    return m_start == other.m_start && m_end == other.m_end;
}

CPersistentEditInsertPoint* CPersistentEditSelection::GetEdge(XP_Bool bEnd){
    return bEnd ? &m_end : &m_start;
} 

#ifdef DEBUG
void CPersistentEditSelection::Print(IStreamOut& stream) {
    stream.Printf("start ");
    m_start.Print(stream);
    stream.Printf(" end ");
    m_end.Print(stream);
    stream.Printf(" FromStart %ld", (long)m_bFromStart);
}
#endif

// Used for undo
// change this by the same way that original was changed into modified.
void CPersistentEditSelection::Map(CPersistentEditSelection& original,
    CPersistentEditSelection& modified){
    CPersistentEditSelection delta;
    CPersistentEditSelection result;
    modified.m_start.ComputeDifference(original.m_start, delta.m_start);
    modified.m_end.ComputeDifference(original.m_end, delta.m_end);
    m_start.AddRelative(delta.m_start, result.m_start);
    m_end.AddRelative(delta.m_end, result.m_end);
    *this = result;
}

//
//  Call this constructor with a maximum number, a series of bits and 
//  BIT_ARRAY_END for example: CBitArray a(100, 1 ,4 9 ,7, BIT_ARRAY_END);
// 
CBitArray::CBitArray(long n, int iFirst, ...) {

	m_Bits = 0;
	size = 0;

	int i;

	if( n ) {
	  SetSize(n);
	}
	va_list stack;
#ifdef OSF1
	va_start( stack, n );
#else
	va_start( stack, iFirst );

	(*this)[iFirst] = 1;
#endif

	while( (i = va_arg(stack,int)) != BIT_ARRAY_END ){
	   (*this)[i] = 1;
	}
}

void CBitArray::SetSize(long n){
    // On Win16 we can't have a size larger that int.
  XP_ASSERT(size < (1L << (16 + 3)));
  uint8 *oldBits = m_Bits;
  m_Bits = new uint8[n/8+1];
  memset(m_Bits,0,(int) (n/8+1));
  if(oldBits){
     memcpy(m_Bits,oldBits,(int) (min(n,size)/8+1));
     delete oldBits;
  }
  size = n;
}

void CBitArray::ClearAll(){
  memset(m_Bits,0, (int) (size/8+1) );
}

/* CLM: Helper to gray UI items not allowed when inside Java Script
 * Note that the return value is FALSE if partial selection, 
 *   allowing the non-script text to be changed
 * Current Font Size, Color, and Character attributes will suppress
 *   setting other attributes, so it is OK to call these when mixed
*/
#ifdef USE_SCRIPT

XP_Bool EDT_IsJavaScript(MWContext * pMWContext)
{
    if(!pMWContext) return FALSE;
    XP_Bool bRetVal = FALSE;
    EDT_CharacterData * pData = EDT_GetCharacterData(pMWContext);
    if( pData) {
        bRetVal = ( (0 != (pData->mask & TF_SERVER)) &&
                    (0 != (pData->values & TF_SERVER)) ) ||
                  ( (0 != (pData->mask & TF_STYLE)) &&
                    (0 != (pData->values & TF_STYLE)) ) ||
                  ( (0 != (pData->mask & TF_SCRIPT )) &&
                    (0 != (pData->values & TF_SCRIPT)) );
        EDT_FreeCharacterData(pData);
    }
    return bRetVal;
}

#else

XP_Bool EDT_IsJavaScript(MWContext *)
{
    return FALSE;
}

#endif

/* Helper to use for enabling Character properties 
 * (Bold, Italic, etc., but DONT use for clearing (TF_NONE)
 *  or setting Java Script (Server or Client)
 * Tests for:
 *   1. Good edit buffer and not blocked because of some action,
 *   2. Caret or selection is NOT entirely within Java Script, 
 *   3. Caret or selection has some text or is mixed selection
 *      (thus FALSE if single non-text object is selected)
*/
XP_Bool EDT_CanSetCharacterAttribute(MWContext * pMWContext)
{
    if( !pMWContext || pMWContext->waitingMode || EDT_IsBlocked(pMWContext) ){
        return FALSE;
    }
    ED_ElementType type = EDT_GetCurrentElementType(pMWContext);
    return ( (type == ED_ELEMENT_TEXT || type == ED_ELEMENT_SELECTION ||
              type >= ED_ELEMENT_TABLE) && !EDT_IsJavaScript(pMWContext) );
}

// XP string defines for FontFaces

extern "C" {
#include "xpgetstr.h"
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS
}

int iFontFaces[] = {
    XP_NSFONT_DEFAULT,
    XP_NSFONT_FIXED,
    XP_NSFONT_ARIAL,
    XP_NSFONT_TIMES,
    XP_NSFONT_COURIER
};

// IDs for the list of fonts written to the tags
// MUST CORRESPOND IN NUMBER AND LOCATION TO iFontFaces!
int iFontFaceTags[] = {
    0, 0,
    XP_NSFONTLIST_ARIAL,
    XP_NSFONTLIST_TIMES,
    XP_NSFONTLIST_COURIER
};
#define ED_NSFONT_MAX ((sizeof(iFontFaceTags))/sizeof(int))

// Cache the lists of font faces that we show to user
//  and list of tags (no shown to user)
static char * pFontFaces = NULL;
static char * pFontFaceTags = NULL;

char *EDT_GetFontFaces()
{
    if( pFontFaces ){
        return pFontFaces;
    }
    intn iSize = 512;
    int iCur = 0;
    pFontFaces = (char*)XP_ALLOC( iSize );

    if( !pFontFaces )
        return NULL;

    pFontFaces[0] = 0;
    pFontFaces[1] = 0;
    
    for( int i = 0; i < ED_NSFONT_MAX; i++ ){
        char *pFont = XP_GetString(iFontFaces[i]);

        if( pFont && *pFont ){
            int iLen = XP_STRLEN( pFont );
            if( iCur + iLen + 2 > iSize ){
                iSize += 512;
                pFontFaces = (char*)XP_REALLOC( pFontFaces, iSize );

                if( ! pFontFaces ){
                    return NULL;
                }
            }
            XP_STRCPY( &pFontFaces[iCur], pFont );
            iCur += iLen+1;
        }
    }
    // Append extra '\0' at end
    pFontFaces[iCur] = 0;

    return pFontFaces;
}

char *EDT_GetFontFaceTags()
{
    if( pFontFaceTags ){
        return pFontFaceTags;
    }
    intn iSize = 2024;
    pFontFaceTags = (char*)XP_ALLOC( iSize );

    if( !pFontFaceTags )
        return NULL;

    // First 2 strings are ignored - map to default proportional and fixed
    XP_STRCPY(pFontFaceTags, " ");
    XP_STRCPY(&pFontFaceTags[2], " ");
    int iCur = 4; // Start at 4th character - after above strings
    pFontFaceTags[5] = 0; // Add extra 0 in case there are no more strings

    for( int i = 2; i < ED_NSFONT_MAX; i++ ){
        char *pFont = XP_GetString(iFontFaceTags[i]);

        if( pFont && *pFont ){
            int iLen = XP_STRLEN( pFont );
            if( iCur + iLen + 2 > iSize ){
                iSize += 2024;
                pFontFaceTags = (char*)XP_REALLOC( pFontFaceTags, iSize );

                if( ! pFontFaceTags ){
                    return NULL;
                }
            }
            XP_STRCPY( &pFontFaceTags[iCur], pFont );
            iCur += iLen+1;
        }
    }
    // Append extra '\0' at end
    pFontFaceTags[iCur] = 0;

    return pFontFaceTags;
}

#define EDT_CLEAR_BIT(x,tf) x &= ~tf
#define EDT_SET_BIT(x,tf)   x |= tf

void EDT_SetFontFace(MWContext * pMWContext, EDT_CharacterData * pCharacterData,
                     int iFontIndex, char * pFontFace )
{
    // Must have one or the other
    if( pMWContext == NULL && pCharacterData == NULL )
        return;
#if defined(XP_WIN) || defined(XP_OS2)
    // Windows may give us this if click off of the font combobox
    if( (long)pFontFace == -1L )
        return;
#endif

    EDT_CharacterData * pData;
    XP_Bool bCanCopy;
    XP_Bool bHaveData;

    // Only 0 and 1 have any meaning now that XP groups are "automatic"
    if( iFontIndex < 0 || iFontIndex > 1 ){
        iFontIndex = -1;
    }
    if( pCharacterData ){
        // Use supplied structure
        pData = pCharacterData;
        bHaveData = TRUE;
        // If pointer to existing data is same as new, don't copy it
        bCanCopy = pData->pFontFace != pFontFace;
        // We will respect the TF_FONT_FACE bit in existing data,
        //  but we are also interested in FIXED if we are doing
        if( pData->mask & TF_FONT_FACE ){
            EDT_SET_BIT(pData->mask, TF_FIXED);
        }
    } else {
        // No data supplied, get from current selection or at caret
        bHaveData = FALSE;
        pData = EDT_GetCharacterData(pMWContext);
        if( !pData )
            return;
        bCanCopy = TRUE;
        // Set the bit to tell that we are interested in font face
        //  and fixed width
        pData->mask = TF_FONT_FACE | TF_FIXED;
    }

    // Remove any existing font face data if not same as new data
    if( pData->pFontFace && bCanCopy){
        XP_FREEIF(pData->pFontFace);
    }

    // This will force using whatever user string was supplied
    if( pFontFace ){
        // So it will be skipped in test below
        iFontIndex = -1;
        if( bCanCopy) {
            // Translate into XP font "group" if face is found in any group
            char *pTranslated = EDT_TranslateToXPFontFace(pFontFace);
            if( *pTranslated == '_' ){
                // First character may have signal for separator - skip over it
                pTranslated++;
            }            
            // Detect the 1st two items -- the Default Variable and Fixed Width fonts
            if( 0 == XP_STRCMP(pTranslated, XP_GetString(iFontFaces[0])) ){
                iFontIndex = 0;
            } else if( 0 == XP_STRCMP(pTranslated,  XP_GetString(iFontFaces[1])) ){
                iFontIndex = 1;
            } else {
                // Copy the font face - either same as pFontFace or the entire group if translated
                pData->pFontFace = XP_STRDUP(pTranslated);
                // Set font face tag - note that it is set
                EDT_SET_BIT(pData->values, TF_FONT_FACE);
                // We can't be fixed width if here
                EDT_CLEAR_BIT(pData->values, TF_FIXED);
            }
        }
    } 

    if( iFontIndex == 0 || iFontIndex == 1 ){
        // No font face tag will be set
        EDT_CLEAR_BIT(pData->values, TF_FONT_FACE);
        
        if( iFontIndex == 1){
            // Fixed Width
            EDT_SET_BIT(pData->values, TF_FIXED);
        } else {
            // Default proportional font
            EDT_CLEAR_BIT(pData->values, TF_FIXED);
        }
    }

    if( pMWContext ){
        EDT_SetCharacterData(pMWContext, pData);
    }
    // Free only the data we obtained
    if( !bHaveData ){
        EDT_FreeCharacterData(pData);
    }
}

char * EDT_TranslateToXPFontFace( char * pFontFace )
{
    char * pFontFaceTags = EDT_GetFontFaceTags();
    if( pFontFaceTags ){
        char * pFace = pFontFace;
        // Skip over initial separator signal
        if( *pFace == '_' ){
            pFace++;
        }
        // Skip over first two strings (each contains " ")
        char * pTag = pFontFaceTags+4;
        int iLen;    
        int i = 0;
        while( (iLen = XP_STRLEN(pTag)) > 0 ) {
            char * pComma;
            char * pTagFace = pTag;
            // Scan for comma separating the next font in group
            while( (pComma = XP_STRCHR(pTagFace,',')) != 0 ){
                *pComma = '\0';
                int iFaceLen = XP_STRLEN(pTagFace);
                XP_Bool bFound = (0 == XP_STRCMP(pTagFace, pFace));
                *pComma = ',';
                if( bFound ){
                    // We found the font in the list - we're done
                    return pTag;
                }
                pTagFace += iFaceLen+1;
            }
            // Check one more time - either no commas found or 
            //  pFace is on the last font in the group
            if( 0 == XP_STRCMP(pTagFace, pFace) ){
                return pTag;
            }
    
            pTag += iLen+1;
            i++;
        }
    }
    // Return exactly what we were passed if not found in font groups    
    return pFontFace;
}


// Cache the list of colors that we show to user
//static char * pFontColors = NULL;

// Use this array instead of pFontColor list
// This replaces the XP_Strings colors
uint8 NSColors[MAX_NS_COLORS][3] = 
{ { 255,255,255 },
  { 204,204,204 },
  { 192,192,192 },
  { 153,153,153 },
  { 102,102,102 },
  { 51,51,51 },   
  { 0,0,0 },      
  { 255,204,204 },
  { 255,102,102 },
  { 255,0,0 },    
  { 204,0,0 },    
  { 153,0,0 },    
  { 102,0,0 },    
  { 51,0,0 },     
  { 255,204,153 },
  { 255,204,51 }, 
  { 255,153,0 },  
  { 255,102,0 },  
  { 204,102,0 },  
  { 153,51,0 },   
  { 102,51,0 },   
  { 255,255,153 },
  { 255,255,102 },
  { 255,204,102 },
  { 255,204,51 }, 
  { 204,153,51 }, 
  { 153,102,51 }, 
  { 102,51,51 },  
  { 255,255,204 },
  { 255,255,153 },
  { 255,255,0 },  
  { 255,204,0 },  
  { 153,153,0 },  
  { 102,102,0 },  
  { 51,51,0 },    
  { 153,255,153 },
  { 102,255,153 },
  { 51,255,51 },  
  { 0,204,0 },    
  { 0,153,0 },    
  { 0,102,0 },    
  { 0,51,0 },     
  { 153,255,255 },
  { 51,255,255 }, 
  { 102,204,204 },
  { 0,204,204 },  
  { 51,153,153 }, 
  { 51,102,102 }, 
  { 0,51,51 },    
  { 204,255,255 },
  { 102,255,255 },
  { 51,204,255 }, 
  { 51,102,255 }, 
  { 51,51,255 },  
  { 0,0,153 },    
  { 0,0,102 },    
  { 204,204,255 },
  { 153,153,204 },
  { 102,102,204 },
  { 102,51,255 }, 
  { 102,0,204 },  
  { 51,51,153 },  
  { 51,0,153 },   
  { 255,204,255 },
  { 255,153,255 },
  { 204,102,204 },
  { 204,51,204 }, 
  { 153,51,102 }, 
  { 102,51,102 }, 
  { 51,0,51 } };    

void EDT_GetNSColor(intn iIndex, LO_Color * pLoColor)
{
    if( pLoColor && iIndex < MAX_NS_COLORS )
    {
        pLoColor->red = NSColors[iIndex][0];
        pLoColor->green = NSColors[iIndex][1];
        pLoColor->blue = NSColors[iIndex][2];
    }
}

/* Return list of font colors in format: "r,g,b,ColorName" where colors for r,g,b
 *  are decimal strings in range 0-255
*/
#if defined(XP_UNIX)
char *EDT_GetFontColors()
{
    return NULL;

#if 0
    if( pFontColors )
        return pFontColors;

    intn iSize = 512;
    int iCur = 0;
    pFontColors = (char*)XP_ALLOC( iSize );

    if( !pFontColors )
        return NULL;

    pFontColors[0] = 0;
    pFontColors[1] = 0;
    
    int nIDColor = XP_NSCOLOR_BASE;
    while( nIDColor < XP_NSCOLOR_END){
        char *pColor = XP_GetString(nIDColor);

        if( pColor && *pColor ){
            int iLen = XP_STRLEN( pColor );
            if( iCur + iLen + 2 > iSize ){
                iSize += 512;
                pFontColors = (char*)XP_REALLOC( pFontColors, iSize );

                if( ! pFontColors ){
                    return NULL;
                }
            }
            XP_STRCPY( &pFontColors[iCur], pColor );
            iCur += iLen+1;
        }
        // Next color ID
        nIDColor++;
    }
    // Append extra '\0' at end
    pFontColors[iCur] = 0;
    return pFontColors;
#endif
}
#endif

char * EDT_ParseColorString(LO_Color * pLoColor, char * pColorString)
{
    XP_ASSERT(pLoColor);
    XP_ASSERT(pColorString);

    char *pComma = XP_STRCHR(pColorString, ',');
    if(!pComma) return pColorString;

	*pComma = '\0';
    pLoColor->red = (uint8)atoi(pColorString);
	*pComma = ',';
    char *pColor = pComma+1;

    pComma = XP_STRCHR(pColor, ',');
    if(!pComma) return pColor;
	*pComma = '\0';
    pLoColor->green = (uint8)atoi(pColor);
	*pComma = ',';
    pColor = pComma+1;

    pComma = XP_STRCHR(pColor, ',');
    // If no comma found, assume color string
    //  doesn't have HTML-HEX format and 
    //  all thats left is the blue value
    if(pComma) 
	    *pComma = '\0';
    pLoColor->blue = (uint8)atoi(pColor);
    if(pComma) 
    {
    	*pComma = ',';
        pColor = pComma+1;
        // Return rest of string, assumned to be Hex representation
        return pColor;
    }

    // Supply a hex translation
    static char pHexColor[16];
    XP_SPRINTF(pHexColor, "#%02X%02X%02X", pLoColor->red, pLoColor->green, pLoColor->blue);
    return pHexColor;
}

/* Scan our list of colors and return index of matching color
 *   or -1 if no match found. We NEVER return 0 (default color)
*/
int EDT_GetMatchingFontColorIndex(LO_Color * pLOColor)
{
    for( intn i = 0; i < MAX_NS_COLORS; i++ )
    {
        if( NSColors[i][0] == pLOColor->red &&
            NSColors[i][1] == pLOColor->green &&
            NSColors[i][2] == pLOColor->blue ){
            return (i+1);
        }
    }
    return -1;
}


#ifdef DEBUG

// Automated test routine hook

const char* CEditTestManager::m_kTrigger = "$$$Test";

CEditTestManager::CEditTestManager(CEditBuffer* pBuffer)
    : m_pBuffer(pBuffer),
      m_bTesting(FALSE),
      m_iTriggerIndex(0),
      m_pSaveBuffer(0),
      m_pTempFileURL(0)
{
#ifdef XP_WIN32
    _CrtMemCheckpoint( &m_state ); // In theorey, avoid measuring the data before we were created.
#endif

    PowerOnTest();
}

void CEditTestManager::DumpMemoryDelta() {
    m_pBuffer->Trim(); // Clear out undo/redo log to simplify memory statistics.
#ifdef XP_WIN32
    _CrtMemDumpAllObjectsSince( &m_state );
    _CrtMemCheckpoint( &m_state );
#else
    XP_TRACE(("Not supported on this OS.\n"));
#endif
}

XP_Bool CEditTestManager::Key(char key) {
    XP_Bool result = m_bTesting;
    if ( ! m_bTesting ){
        if ( key == m_kTrigger[m_iTriggerIndex] ) {
            m_iTriggerIndex++;
            if ( m_kTrigger[m_iTriggerIndex] == '\0' ) {
                m_bTesting = TRUE;
                XP_TRACE(("Testing on! Type # of test, or Q to quit."));
            }
        }
        else {
            m_iTriggerIndex = 0;
        }
    }
    else {
        int bQuitTesting = FALSE;
        m_bTesting = FALSE; // So when the tests type, it doesn't cause a recursion into the test code.
        intn bTestResult = -1;
        if ( key >= 'A' && key <= 'Z' ) key = key + ('a' - 'A');
        switch (key) {
            case 'q':
                bQuitTesting = TRUE;
                XP_TRACE(("Quitting test mode."));
                break;
            case 'a':
                DumpLoElements();
                break;
            case 'b':
                VerifyLoElements();
                break;
            case 'c':
                DumpDocumentContents();
                break;
            case 'd':
                DumpMemoryDelta();
                break;
            case 'g':
                GetDocTempDir();
                break;            
            case '!':
                PasteINTL();
                break;
            case 'w':
                PasteINTLText();
                break;

            case 'x':
                CopyDocumentToBuffer();
                break;
            case 'y':
                CopyBufferToDocument();
                break;
#ifdef EDITOR_JAVA
            case 'j':
                DumpPlugins();
                break;
            case 'k':
                PerformFirstPlugin();
                break;
            case 'l':
                PerformPluginByName();
                break;
            case 'm':
                PerformFirstEncoder();
                break;
            case 'n':
                PerformPreOpen();
                break;
#endif
            case 's':
                SaveToTempFile();
                break;
            case 't':
                RemoveTempFile();
                break;
            case 'v':
                TestHTMLPaste();
                break;
            case '0':
                bTestResult = EveryNavigationKeyEverywhereTest();
                break;
            case '1':
                bTestResult = ArrowTest();
                break;
            case '2':
                bTestResult = BackspaceKeyTest();
                break;
            case '3':
                bTestResult = DeleteKeyTest();
                break;
            case '4':
                bTestResult = ZeroDocumentCursorTest();
                break;
            case '5':
                bTestResult = OneCharDocumentCursorTest();
                break;
            case '6':
                bTestResult = BoldTest(10); // OneDayTest(1);
                break;
            case '7':
                    m_pBuffer->m_bSkipValidation = TRUE;
                    bTestResult = OneDayTest(100);
                    m_pBuffer->m_bSkipValidation = FALSE;
                break;
           case '8':
                bTestResult = TextFETest();
                break;
             case '?':
                 XP_TRACE(("? - type this help\n"
                    "Q - quit.\n"
                    "a - print lo-elements.\n"
                    "b - verify lo-elements.\n"
                    "c - print document contents (elements, undo history, etc.)\n"
                    "d - dump memory usage delta.\n"
                    "g - get doc temp directory. \n"
                    ));
#ifdef EDITOR_JAVA
                 XP_TRACE(("j - show Composer Plugin and Image Encoder information.\n"
                     "k - run the first composer plugin.\n"
                     "l - run a composer plugin by name.\n"
                     "m - run the first image encoder.\n"
                     "n - test the EDT_PreOpen call.\n"
                    ));
#endif
                 XP_TRACE(("q - test HTML Paste Quoted.\n"
                    ));
                 XP_TRACE(("s - save doc to temporary file.\n"
                    ));
                 XP_TRACE(("t - remove saved temporary file.\n"
                    ));
                 XP_TRACE(("x - save document state.\n"
                    "y - restore document state.\n"
                    ));
                 XP_TRACE((
                    "0 - navigation key crash test.\n"
                    "1 - arrow completeness test.\n"
                    "2 - destructive backspace test.\n"
                    "3 - destructive delete test.\n"
                    "4 - empty document cursor test.\n"
                    "5 - one character document cursor test.\n"
                    "6 - one day document test - simulate editing for one whole day 1 cycle.\n"
                    "7 - one day document test - simulate editing for one whole day 500 cycle.\n"
                     "8 - write buffer to out.txt as plain text.\n"
                   ));
               break;
            default:
                 XP_TRACE(("Type ? for help, Type # of test, or Q to quit."));
                 break;
        }
        if ( ! bQuitTesting )
        {
            m_bTesting = TRUE;
        }
        if ( bTestResult >= 0 ){
            XP_TRACE(("Test %s", bTestResult ? "Passed": "Failed"));
        }
    }
    return result;
}

void CEditTestManager::PowerOnTest() {
    // Test some things that have to be tested by code. This method is
    // called whenever the editor starts up in debug mode. Don't take too long.
    
    // Test that we know about all the existing tags.
    for(int i = 0; i < P_MAX; i++ ){
        EDT_TagString(i);
    }
}

void CEditTestManager::DumpLoElements() {
    lo_PrintLayout(m_pBuffer->m_pContext);
}

void CEditTestManager::VerifyLoElements() {
    lo_VerifyLayout(m_pBuffer->m_pContext);
}

void CEditTestManager::DumpDocumentContents(){
    CStreamOutMemory buffer;
    m_pBuffer->printState.Reset( &buffer, m_pBuffer );
    m_pBuffer->DebugPrintTree( m_pBuffer->m_pRoot );
    m_pBuffer->DebugPrintState(buffer);
    TraceLargeString(buffer.GetText());
}

void TraceLargeString(char* b){
    // have to dump one line at a time. XP_TRACE has a 512 char limit on Windows.
    while ( *b != '\0' ) {
        char* b2 = b;
        while ( *b2 != '\0' && *b2 != '\n'){
            b2++;
        }
        char old = *b2;
        *b2 = '\0';
        XP_TRACE(("%s", b));
        *b2 = old;
        b = b2 + 1;
        if ( old == '\0' ) break;
    }
}

void CEditTestManager::PasteINTL(){
    unsigned char testStringJIS[] = {'J','I','S',' ','[',27,'$','B',30,21,']',' ',0};
    unsigned char testStringEUC1[] = {'E','U','C',' ','[', 0xb0, 0};
    unsigned char tesrStringEUC2[] = {0xa1,']',' ',0};
    unsigned char testStringSJIS[] = {'S','J','I','S',' ','[',0x88,0x9f,']',0};
    
    MWContext* pContext = m_pBuffer->m_pContext;
    EDT_PasteQuoteBegin(pContext, TRUE);
    EDT_PasteQuoteINTL(pContext, (char*) testStringJIS, CS_JIS);
    EDT_PasteQuoteINTL(pContext, (char*) testStringEUC1, CS_EUCJP);
    EDT_PasteQuoteINTL(pContext, (char*) tesrStringEUC2, CS_EUCJP);
    EDT_PasteQuoteINTL(pContext, (char*) testStringSJIS, CS_SJIS);
    EDT_PasteQuoteEnd(pContext);
}

void CEditTestManager::PasteINTLText(){
    unsigned char testStringJIS[] = {'J','I','S',' ','[',27,'$','B',30,21,']',' ',0};
    unsigned char testStringEUC[] = {'E','U','C',' ','[', 0xb0, 0xa1,']',' ',0};
    unsigned char testStringSJIS[] = {'S','J','I','S',' ','[',0x88,0x9f,']',0};
    
    MWContext* pContext = m_pBuffer->m_pContext;
    EDT_PasteQuoteBegin(pContext, FALSE);
    EDT_PasteQuoteINTL(pContext, (char*) testStringJIS, CS_JIS);
    EDT_PasteQuoteINTL(pContext, (char*) testStringEUC, CS_EUCJP);
    EDT_PasteQuoteINTL(pContext, (char*) testStringSJIS, CS_SJIS);
    EDT_PasteQuoteEnd(pContext);
}

void CEditTestManager::CopyDocumentToBuffer(){
    if ( m_pSaveBuffer){
        delete[] m_pSaveBuffer;
        m_pSaveBuffer = 0;
    }
    m_pBuffer->WriteToBuffer(&m_pSaveBuffer, TRUE);
    XP_TRACE(("Buffer size: %d bytes", XP_STRLEN(m_pSaveBuffer)));
}

void CEditTestManager::CopyBufferToDocument(){
    if ( m_pSaveBuffer == NULL ) {
        XP_TRACE(("The save buffer is empty."));
    }
    else {
        m_pBuffer->ReadFromBuffer(m_pSaveBuffer);
    }
}

static const char* kChunkName[EDT_NA_COUNT] = {
	"character", "word", "line edge", "document", "updown"
};

XP_Bool CEditTestManager::ZeroDocumentCursorTest(){
    XP_TRACE(("ZeroDocumentCursorTest"));
    // [selection][chunk][direction][edge]
    const ElementIndex kExpectedResults[2][EDT_NA_COUNT][2][2] =
    {   // no selection
        {
            {{ 0, 0}, {0, 0} }, // EDT_NA_CHARACTER
            {{ 0, 0}, {0, 0} }, // EDT_NA_WORD
            {{ 0, 0}, {0, 0} }, // EDT_NA_LINEEDGE
            {{ 0, 0}, {0, 0} }, // EDT_NA_DOCUMENT
            {{ 0, 0}, {0, 0} }  // EDT_NA_UPDOWN
        },
        // selection
        {
            {{ 0, 0}, {0, 1} }, // EDT_NA_CHARACTER
            {{ 0, 0}, {0, 1} }, // EDT_NA_WORD
            {{ 0, 0}, {0, 1} }, // EDT_NA_LINEEDGE
            {{ 0, 0}, {0, 1} }, // EDT_NA_DOCUMENT
            {{ 0, 0}, {0, 1} }  // EDT_NA_UPDOWN
        }
    };

    XP_Bool result = TRUE;
    // Clear everything from existing document
    EDT_SelectAll(m_pBuffer->m_pContext);
    EDT_DeleteChar(m_pBuffer->m_pContext);
    CPersistentEditInsertPoint zero(0,FALSE);
    // Verify the cursor does the right thing for every possible cursor
    for ( int select = 0; select < 2; select++ ) {
        for ( int chunk = 0; chunk < EDT_NA_COUNT; chunk++ ) {
            for ( int direction = 0; direction < 2; direction++ ) {
                m_pBuffer->SetInsertPoint(zero);
                m_pBuffer->NavigateChunk(select, chunk, direction);
                CPersistentEditSelection selection;
                m_pBuffer->GetSelection(selection);
                for ( int edge = 0; edge < 2; edge++ ) {
                    ElementIndex expected = kExpectedResults[select][chunk][direction][edge];
                    ElementIndex actual = selection.GetEdge(edge)->m_index;
                    if ( expected != actual ){
                        result = FALSE;
                        XP_TRACE(("selection: %s chunk: %s direction: %s edge: %s. Expected %d got %d",
                            select ? "TRUE" : "FALSE",
                            kChunkName[chunk],
                            direction ? "forward" : "back",
                            edge ? "end" : "start",
                            expected,
                            actual));
                    }
                }
            }
        }
    }
    return result;
}


XP_Bool CEditTestManager::OneCharDocumentCursorTest(){
    XP_TRACE(("OneCharDocumentCursorTest"));
    // [startPos][selection][chunk][direction][edge]
    const ElementIndex kExpectedResults[2][2][EDT_NA_COUNT][2][2] =
    {
        // position 0
        {   // no selection
            {
                {{ 0, 0}, {1, 1} }, // EDT_NA_CHARACTER
                {{ 0, 0}, {1, 1} }, // EDT_NA_WORD
                {{ 0, 0}, {1, 1} }, // EDT_NA_LINEEDGE
                {{ 0, 0}, {1, 1} }, // EDT_NA_DOCUMENT
                {{ 0, 0}, {0, 0} }  // EDT_NA_UPDOWN
            },
            // selection
            {
                {{ 0, 0}, {0, 1} }, // EDT_NA_CHARACTER
                {{ 0, 0}, {0, 1} }, // EDT_NA_WORD
                {{ 0, 0}, {0, 2} }, // EDT_NA_LINEEDGE
                {{ 0, 0}, {0, 2} }, // EDT_NA_DOCUMENT
                {{ 0, 0}, {0, 2} }  // EDT_NA_UPDOWN
            }
        },
        // position 1
        {   // no selection
            {
                {{ 0, 0}, {1, 1} }, // EDT_NA_CHARACTER
                {{ 0, 0}, {1, 1} }, // EDT_NA_WORD
                {{ 0, 0}, {1, 1} }, // EDT_NA_LINEEDGE
                {{ 0, 0}, {1, 1} }, // EDT_NA_DOCUMENT
                {{ 1, 1}, {1, 1} }  // EDT_NA_UPDOWN
            },
            // selection
            {
                {{ 0, 1}, {1, 2} }, // EDT_NA_CHARACTER
                {{ 0, 1}, {1, 2} }, // EDT_NA_WORD
                {{ 0, 1}, {1, 2} }, // EDT_NA_LINEEDGE
                {{ 0, 1}, {1, 2} }, // EDT_NA_DOCUMENT
                {{ 0, 1}, {1, 2} }  // EDT_NA_UPDOWN
            }
        }
    };

    XP_Bool result = TRUE;
    // Clear everything from existing document
    EDT_SelectAll(m_pBuffer->m_pContext);
    EDT_DeleteChar(m_pBuffer->m_pContext);
    m_pBuffer->InsertChar( 'X', FALSE );
    // Verify the cursor does the right thing for every possible cursor
    for ( int startPos = 0; startPos < 2; startPos++ ){
        CPersistentEditInsertPoint p(startPos,FALSE);
        for ( int select = 0; select < 2; select++ ) {
            for ( int chunk = 0; chunk < EDT_NA_COUNT; chunk++ ) {
                for ( int direction = 0; direction < 2; direction++ ) {
                    m_pBuffer->SetInsertPoint(p);
                    m_pBuffer->NavigateChunk(select, chunk, direction);
                    CPersistentEditSelection selection;
                    m_pBuffer->GetSelection(selection);
                    for ( int edge = 0; edge < 2; edge++ ) {
                        ElementIndex expected = kExpectedResults[startPos][select][chunk][direction][edge];
                        ElementIndex actual = selection.GetEdge(edge)->m_index;
                        if ( expected != actual ){
                            result = FALSE;
                            XP_TRACE(("position: %d selection: %s chunk: %s direction: %s edge: %s. Expected %d got %d",
                                startPos,
                                select ? "TRUE" : "FALSE",
                                kChunkName[chunk],
                                direction ? "forward" : "back",
                                edge ? "end" : "start",
                                expected,
                                actual));
                        }
                    }
                }
            }
        }
    }
    return result;
}

XP_Bool CEditTestManager::OneDayTest(int32 rounds) {
    char* kTitle = "One Day Test\n";
    XP_TRACE(("%s", kTitle));
    EDT_SelectAll(m_pBuffer->m_pContext);
    EDT_DeleteChar(m_pBuffer->m_pContext);
    EDT_PasteText(m_pBuffer->m_pContext, kTitle);
    for( int32 i = 0; i < rounds; i++ ) {
        XP_TRACE(("Round %d", i));
        const char* kTestString = "All work and no play makes Jack a dull boy.\nRedrum.\n";
        char c;
        for ( int j = 0;
              (c = kTestString[j]) != '\0';
            j++ ) {
            if ( c == '\n' || c == '\r') {
                EDT_ReturnKey(m_pBuffer->m_pContext);
            }
            else {
                EDT_KeyDown(m_pBuffer->m_pContext, c,0,0);
            }
        }
        // And delete a little.
        for ( int k = 0; k < 8; k++ ) {
            EDT_DeletePreviousChar(m_pBuffer->m_pContext);
        }
    }
    XP_TRACE(("End of %s.", kTitle));
    return TRUE;
}

static void TextFETestDone(PrintSetup *setup) {
    XP_FileClose(setup->out);
    XP_TRACE(("Finished TextFETest."));
}


XP_Bool CEditTestManager::TextFETest() {
    PrintSetup setup;
    char *_srcName = 0;
    char *srcName = 0;
    char *srcURL = 0;
    char *destName = 0;
    XP_File destFile = 0;
    URL_Struct *srcURLS = 0;
    XP_Bool ret = FALSE;
    ED_FileError result;

    ////  Write the edit buffer to a temporary file as HTML.
    // Get the filename of a temporary file.
#ifdef XP_MAC
    _srcName = WH_TempName(xpFileToPost,"ns");
#else
    _srcName = WH_TempName(xpTemporary,"ns");
#endif
    if (!_srcName)
        goto CLEAN_UP;

    // Is this necessary for some Mac magic to work?
    srcName = WH_FilePlatformName(_srcName);
    if (!srcName)
        goto CLEAN_UP;

    srcURL = XP_PlatformFileToURL(srcName);
    if (!srcURL)
        goto CLEAN_UP;

    result = EDT_SaveFile(m_pBuffer->m_pContext,
                                        srcURL,srcURL,
                                        FALSE,FALSE,FALSE);
    if (result != ED_ERROR_NONE)
        goto CLEAN_UP;
    XP_TRACE(("Created %s",srcURL));


    // The converted plain text will go to "out.txt".
    destName = XP_STRDUP("out.txt");
    if (!destName)
        goto CLEAN_UP;

    destFile = XP_FileOpen(destName,xpFileToPost,XP_FILE_WRITE_BIN);
    if (!destFile)
        goto CLEAN_UP;


    //// Call XL_Translate text to convert src to dest, TextFETestDone will be called on
    // completion.
    srcURLS = NET_CreateURLStruct(srcURL,NET_DONT_RELOAD);
    if (!srcURLS) {
        XP_FileClose(destFile);
        goto CLEAN_UP;
    }

    XL_InitializeTextSetup(&setup);
    setup.url = srcURLS;
    setup.completion = TextFETestDone;
    setup.out = destFile;

    // Seems that passed in MWcontext is only used to copy some info when making the
    // temporary text context.
    XL_TranslateText(m_pBuffer->m_pContext,srcURLS,&setup);
    ret = TRUE;


// Nasty, a goto.
CLEAN_UP:
    if (_srcName)
        XP_FREE(_srcName);
    if (srcName)
        XP_FREE(srcName);
    if (srcURL)
        XP_FREE(srcURL);
    if (destName)
        XP_FREE(destName);

    return ret;
}

XP_Bool CEditTestManager::BoldTest(int32 rounds) {
    char* kTitle = "Bold Test\n";
    XP_TRACE(("%s", kTitle));
    EDT_SelectAll(m_pBuffer->m_pContext);
    EDT_DeleteChar(m_pBuffer->m_pContext);
    EDT_PasteText(m_pBuffer->m_pContext, kTitle);
    EDT_SelectAll(m_pBuffer->m_pContext);
    for( int32 i = 0; i < rounds; i++ ) {
        XP_TRACE(("Round %d", i));
        EDT_CharacterData* normal = EDT_GetCharacterData( m_pBuffer->m_pContext );
        EDT_CharacterData* bold = EDT_GetCharacterData( m_pBuffer->m_pContext );
        bold->mask = TF_BOLD;
        bold->values = TF_BOLD;
        EDT_SetCharacterData(m_pBuffer->m_pContext, bold);
        EDT_SetCharacterData(m_pBuffer->m_pContext, normal);
        EDT_FreeCharacterData(normal);
        EDT_FreeCharacterData(bold);
    }
    XP_TRACE(("End of %s.", kTitle));
    return TRUE;
}

XP_Bool CEditTestManager::EveryNavigationKeyEverywhereTest() {
    // Does navigation work from every position?
    XP_Bool result;
    result = NavigateChunkCrashTest() && UArrowTest(FALSE) && UArrowTest(TRUE) 
    			&& DArrowTest(FALSE) && DArrowTest(TRUE);
    XP_TRACE(("Done.\n"));
    return result;
}

XP_Bool CEditTestManager::ArrowTest() {
    // Does the navigation move smoothly through the document?
    XP_Bool result;
    result = LArrowTest(FALSE) || LArrowTest(TRUE) || RArrowTest(FALSE)
    			|| RArrowTest(TRUE) || UArrowTest(TRUE) || DArrowTest(TRUE);
#if 0
    // Too many false positives -- need to know about up/down at first/last line
    result |= UArrowTest(FALSE);
    result |= DArrowTest(FALSE);
#endif
    XP_TRACE(("Done.\n"));
    return result;
}

void CEditTestManager::GetWholeDocumentSelection(CPersistentEditSelection& selection){
    CEditSelection wholeDocument;
    m_pBuffer->m_pRoot->GetAll(wholeDocument);
    selection = m_pBuffer->EphemeralToPersistent(wholeDocument);
    // Ignore the last 2 positions -- it's the EOF marker
    selection.m_end.m_index -= 2;
}

XP_Bool CEditTestManager::NavigateChunkCrashTest(){
    for ( int select = 0; select < 2; select++ ) {
        for ( int chunk = 0; chunk < EDT_NA_COUNT; chunk++ ) {
            for ( int direction = 0; direction < 2; direction++ ) {
                if ( !NavChunkCrashTest(select, chunk, direction) )
                	return FALSE;
            }
        }
    }
	return TRUE;
}

XP_Bool CEditTestManager::NavChunkCrashTest(XP_Bool bSelect, int chunk, XP_Bool bDirection){
    XP_Bool bResult = TRUE;
    CPersistentEditSelection w;
    GetWholeDocumentSelection(w);
    CPersistentEditInsertPoint p;
    XP_TRACE(("NavChunkCrashTest bSelect = %d chunk = %d bDirection = %d\n", bSelect, chunk, bDirection));

    for ( p = w.m_start;
        p.m_index < w.m_end.m_index;
        p.m_index++ ) {
        m_pBuffer->SetInsertPoint(p);
        m_pBuffer->NavigateChunk(bSelect, chunk, bDirection);
    }
    return bResult;
}

XP_Bool CEditTestManager::RArrowTest(XP_Bool bSelect){
    XP_Bool result = TRUE;
    CPersistentEditSelection w;
    GetWholeDocumentSelection(w);
    CPersistentEditInsertPoint p;
    XP_TRACE(("Right Arrow%s\n", bSelect ? " with shift key" : ""));

    for ( p = w.m_start;
        p.m_index < w.m_end.m_index;
        p.m_index++ ) {
        m_pBuffer->SetInsertPoint(p);
        m_pBuffer->NavigateChunk(bSelect, LO_NA_CHARACTER, TRUE);
        CPersistentEditSelection s2;
        m_pBuffer->GetSelection(s2);
        if ( (bSelect == s2.IsInsertPoint()) ||
            s2.m_end.m_index != p.m_index + 1 ) {
            XP_TRACE(("%d should be %d was %d\n", p.m_index,
                p.m_index + 1, s2.m_end.m_index));
            result = FALSE;
        }
    }
    return result;
}

XP_Bool CEditTestManager::LArrowTest(XP_Bool bSelect){
    XP_Bool result = TRUE;
    CPersistentEditSelection w;
    GetWholeDocumentSelection(w);
    CPersistentEditInsertPoint p(0,TRUE);
    XP_TRACE(("Left Arrow Test%s\n", bSelect ? " with shift key" : ""));

    for ( p.m_index = w.m_end.m_index - 1;
        p.m_index > w.m_start.m_index;
        p.m_index-- ) {

        CEditSelection startSelection;
        CPersistentEditSelection s2;

        p.m_bStickyAfter = TRUE;
        m_pBuffer->SetInsertPoint(p);

        m_pBuffer->GetSelection(startSelection);
        if ( startSelection.m_start.GapWithBothSidesAllowed() && ! bSelect ) {
            // XP_TRACE(("%d - gap with both sides allowed.", p.m_index));
            // Test that SetInsertPoint did the right thing.
            m_pBuffer->GetSelection(s2);
            if ( ! s2.IsInsertPoint() || !s2.m_start.IsEqualUI(p) ) {
                XP_TRACE(("SetInsertPoint at %d should be %d.%d was %d.%d\n", p.m_index,
                    p.m_index, p.m_bStickyAfter,
                    s2.m_start.m_index, s2.m_start.m_bStickyAfter));
                result = FALSE;
            }
            // Test hump over soft break
            m_pBuffer->NavigateChunk(bSelect, LO_NA_CHARACTER, FALSE);
            m_pBuffer->GetSelection(s2);
            CPersistentEditInsertPoint expected = p;
            expected.m_bStickyAfter = FALSE;
            if ( ! s2.IsInsertPoint() || !s2.m_start.IsEqualUI(expected) ) {
                XP_TRACE(("gap at %d should be %d.%d was %d.%d\n", p.m_index,
                    expected.m_index, expected.m_bStickyAfter,
                    s2.m_start.m_index, s2.m_start.m_bStickyAfter));
                result = FALSE;
            }
            p.m_bStickyAfter = FALSE;
            m_pBuffer->SetInsertPoint(p);
        }
        m_pBuffer->NavigateChunk(bSelect, LO_NA_CHARACTER, FALSE);
        m_pBuffer->GetSelection(s2);
        if ( bSelect == s2.IsInsertPoint() ) {
            XP_TRACE(("%d Wrong type of selection. Expected %d.%d was %d.%d\n", p.m_index,
                p.m_index, p.m_bStickyAfter,
                s2.m_start.m_index, s2.m_start.m_bStickyAfter));
            result = FALSE;
        }
        else if ( s2.m_start.m_index != p.m_index - 1 ) {
            XP_TRACE(("%d wrong edge position. Should be %d.%d was %d.%d\n", p.m_index,
                p.m_index-1, p.m_bStickyAfter,
                s2.m_start.m_index, s2.m_start.m_bStickyAfter));
            result = FALSE;
        }
    }
    return result;
}

XP_Bool CEditTestManager::UArrowTest(XP_Bool bSelect){
    XP_Bool result = TRUE;
    CPersistentEditSelection w;
    GetWholeDocumentSelection(w);
    CPersistentEditInsertPoint p;
    XP_TRACE(("Up Arrow Test%s\n", bSelect ? " with shift key" : ""));

    for ( p = w.m_end.m_index - 1;
        p.m_index > w.m_start.m_index;
        p.m_index-- ) {
        m_pBuffer->SetInsertPoint(p);
        CPersistentEditSelection s;
        m_pBuffer->GetSelection(s);
        m_pBuffer->ClearMove();
        m_pBuffer->UpDown(bSelect, FALSE);
        CPersistentEditSelection s2;
        m_pBuffer->GetSelection(s2);
        if ( s2 == s ) {
            XP_TRACE(("%d didn't change selection\n", p.m_index));
            result = FALSE;
        }
    }
    return result;
}


XP_Bool CEditTestManager::DArrowTest(XP_Bool bSelect){
    XP_Bool result = TRUE;
    CPersistentEditSelection w;
    GetWholeDocumentSelection(w);
    CPersistentEditInsertPoint p;
    XP_TRACE(("Down Arrow Test%s\n", bSelect ? " with shift key" : ""));

    for ( p = w.m_start.m_index;
        p.m_index < w.m_end.m_index;
        p.m_index++ ) {
        m_pBuffer->SetInsertPoint(p);
        CPersistentEditSelection s;
        m_pBuffer->GetSelection(s);
        m_pBuffer->ClearMove();
        m_pBuffer->UpDown(bSelect, TRUE);
        CPersistentEditSelection s2;
        m_pBuffer->GetSelection(s2);
        if ( s2 == s ) {
            XP_TRACE(("%d didn't change selection\n", p.m_index));
            result = FALSE;
        }
    }
    return result;
}

XP_Bool CEditTestManager::BackspaceKeyTest(){
    XP_Bool result = TRUE;
    CPersistentEditSelection w;
    GetWholeDocumentSelection(w);
    CPersistentEditInsertPoint p;
    XP_TRACE(("DeleteKeyTest"));
    p = w.m_end;
    p.m_index--;
    m_pBuffer->SetInsertPoint(p);
    for ( p = w.m_start.m_index;
        p.m_index < w.m_end.m_index;
        p.m_index++ ) {
        m_pBuffer->DeletePreviousChar();
    }
    return result;
}

XP_Bool CEditTestManager::DeleteKeyTest(){
    XP_Bool result = TRUE;
    CPersistentEditSelection w;
    GetWholeDocumentSelection(w);
    CPersistentEditInsertPoint p;
    XP_TRACE(("DeleteKeyTest"));

    m_pBuffer->SetInsertPoint(w.m_start);
    for ( p = w.m_start.m_index;
        p.m_index < w.m_end.m_index;
        p.m_index++ ) {
        m_pBuffer->DeleteNextChar();
    }
    return result;
}

XP_Bool CEditTestManager::Backspace() {
    if ( ! m_bTesting ){
        m_iTriggerIndex--;
        if (m_iTriggerIndex < 0 ) m_iTriggerIndex = 0;
    }
    return m_bTesting;
}

XP_Bool CEditTestManager::ReturnKey() {
    if ( ! m_bTesting ){
        m_iTriggerIndex = 0;
    }
    return m_bTesting;
}

#ifdef EDITOR_JAVA

void CEditTestManager::DumpPlugins(){
    XP_TRACE(("Dump Plugin database."));
    int32 numCategories = EDT_NumberOfPluginCategories();
    XP_TRACE(("EDT_NumberOfPluginCategories: %d", numCategories));
    for(int category = 0; category < numCategories; category++ ) {
        char* pcsCategoryName = EDT_GetPluginCategoryName(category);
        XP_TRACE(("EDT_GetPluginCategoryName(%d) = \"%s\"", category, pcsCategoryName));
        int plugins = EDT_NumberOfPlugins(category);
        XP_TRACE(("EDT_NumberOfPlugins(%d) = %d", category, plugins));
        for(int plugin = 0; plugin < plugins; plugin++){
            char* pcsPluginName = EDT_GetPluginName(category, plugin);
            XP_TRACE(("EDT_GetPluginName(%d, %d) = \"%s\"", category, plugin, pcsPluginName));
            char* pcsPluginMenuHelp = EDT_GetPluginMenuHelp(category, plugin);
            XP_TRACE(("EDT_GetPluginMenuHelp(%d, %d) = \"%s\"", category, plugin, pcsPluginMenuHelp));
        }
    }
    XP_TRACE(("Dump Image Encoder database."));
    int encoders = EDT_NumberOfEncoders();
    XP_TRACE(("EDT_NumberOfEncoders() = %d", encoders));
    for(int encoder = 0; encoder < encoders; encoder++){
        char* pcsEncoderName = EDT_GetEncoderName(encoder);
        XP_TRACE(("EDT_GetEncoderName(%d) = \"%s\"", encoder, pcsEncoderName));

        char* pcsEncoderFileExtension = EDT_GetEncoderFileExtension(encoder);
        XP_TRACE(("EDT_GetEncoderFileExtension(%d) = \"%s\"", encoder, pcsEncoderFileExtension));

        char* pcsEncoderFileType = EDT_GetEncoderFileType(encoder);
        XP_TRACE(("EDT_GetEncoderFileType(%d) = \"%s\"", encoder, pcsEncoderFileType));

        XP_TRACE(("EDT_GetEncoderNeedsQuantizedSource(%d) = \"%d\"", encoder, EDT_GetEncoderNeedsQuantizedSource(encoder)));

        char* pcsEncoderMenuHelp = EDT_GetEncoderMenuHelp(encoder);
        XP_TRACE(("EDT_GetEncoderMenuHelp(%d) = \"%s\"", encoder, pcsEncoderMenuHelp));

    }
}

#ifndef XP_OS2
static 
#else
extern "OPTLINK"
#endif
void edt_TestImageEncoderCallbackFn(EDT_ImageEncoderStatus status, void* hook) {
    XP_TRACE(("edt_TestImageEncoderCallbackFn Status: %d hook: %08x", status, hook));
}

void CEditTestManager::PerformFirstPlugin(){
    if ( EDT_NumberOfPluginCategories() <= 0 ) {
        XP_TRACE(("EDT_NumberOfPluginCategories: %d", EDT_NumberOfPluginCategories()));
        return;
    }
    if ( EDT_NumberOfPlugins(0) <= 0 ) {
        XP_TRACE(("EDT_NumberOfPlugins(0): %d", EDT_NumberOfPlugins(0)));
        return;
    }
    XP_Bool bItem = EDT_PerformPlugin(m_pBuffer->m_pContext, 0, 0, edt_TestImageEncoderCallbackFn, (void*) 0x1234);
    XP_TRACE(("EDT_PerformPlugin: returned %d\n", bItem));
}

void CEditTestManager::PerformPluginByName(){
    XP_TRACE(("Calling dummyPlugin.does.not.exist"));
    XP_Bool bResult = EDT_PerformPluginByClassName(m_pBuffer->m_pContext, "dummyPlugin.does.not.exist",
        edt_TestImageEncoderCallbackFn, (void*) 12);
    XP_TRACE(("EDT_PerformPluginByClassName returned %d\n", bResult));
    XP_TRACE(("Calling netscape.test.plugin.composer.EditRaw"));
    bResult = EDT_PerformPluginByClassName(m_pBuffer->m_pContext, "netscape.test.plugin.composer.EditRaw",
         edt_TestImageEncoderCallbackFn, (void*) 13);
    XP_TRACE(("EDT_PerformPluginByClassName returned %d\n", bResult));
}

void CEditTestManager::PerformFirstEncoder(){
    if ( EDT_NumberOfEncoders() <= 0 ) {
        XP_TRACE(("EDT_NumberOfEncoders: %d", EDT_NumberOfEncoders()));
        return;
    }
    char** ppPixels = (char**) XP_ALLOC(sizeof(char*));
    char* pPixels = (char*) XP_ALLOC(3);
    ppPixels[0] = pPixels;
    pPixels[0] = 1;
    pPixels[1] = 2;
    pPixels[2] = 3;
    XP_TRACE(("EDT_StartEncoder: starting\n"));
    XP_Bool bItem = EDT_StartEncoder(m_pBuffer->m_pContext, 0, 1, 1, ppPixels,
        "C:\\junk.txt", edt_TestImageEncoderCallbackFn, (void*) 0xfeedface);
    XP_TRACE(("EDT_StartEncoder: returned %d\n", bItem));
    XP_FREE(pPixels);
    XP_FREE(ppPixels);
}

#ifndef XP_OS2
static 
#else
extern "OPTLINK"
#endif
void edt_TestPreOpenCallbackFn(XP_Bool bUserCancled, char* pURL, void* hook) {
    XP_TRACE(("edt_TestPreOpenCallbackFn bUserCancled: %s url: %s hook: %08x", bUserCancled ? "TRUE" : "FALSE",
        pURL ? pURL : "null", hook));
}

void CEditTestManager::PerformPreOpen(){
    XP_TRACE(("Calling preOpen."));
    EDT_PreOpen(m_pBuffer->m_pContext,"http://PerformPreOpen",
        edt_TestPreOpenCallbackFn, (void*) 13);
}

#endif // EDITOR_JAVA

void CEditTestManager::TestHTMLPaste(){
    EDT_PasteQuoteBegin(m_pBuffer->m_pContext, TRUE);
    EDT_PasteQuote(m_pBuffer->m_pContext, "<b>This is bold.</b>");
    EDT_PasteQuote(m_pBuffer->m_pContext, "<i>This is italic.</i>");
    EDT_PasteQuoteEnd(m_pBuffer->m_pContext);
}

void CEditTestManager::SaveToTempFile() {
  EDT_SaveToTempFile(m_pBuffer->m_pContext,(EDT_SaveToTempCallbackFn) SaveToTempFileCB,(void *)m_pBuffer->m_pContext);
}

void CEditTestManager::SaveToTempFileCB(char *pFileURL,void *hook) {
  XP_ASSERT(hook);
  MWContext *pContext = (MWContext *)hook;
  GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);

  CEditTestManager *pManager = pEditBuffer->m_pTestManager;
  if (!pManager) {
    XP_ASSERT(0);
    return;
  }
  XP_FREEIF(pManager->m_pTempFileURL);
  if (pFileURL) {
    pManager->m_pTempFileURL = XP_STRDUP(pFileURL);
    XP_TRACE(("Wrote to temp file %s",pFileURL));
  }
  else 
  {
    XP_TRACE(("Failed to write temp file."));
  }
}

void CEditTestManager::RemoveTempFile() {
  if (m_pTempFileURL) {
    XP_TRACE(("Removing file %s",m_pTempFileURL));
    EDT_RemoveTempFile(m_pBuffer->m_pContext,m_pTempFileURL);
    XP_FREEIF(m_pTempFileURL);
  }
  else {
    XP_TRACE(("No temp file has been saved"));
  }
}

void CEditTestManager::GetDocTempDir() {
  CEditCommandLog *pLog = CGlobalHistoryGroup::GetGlobalHistoryGroup()->GetLog(m_pBuffer);
  if (!pLog) {
    return;
  }

  char *pFile = pLog->GetDocTempDir();
  if (pFile) {
    XP_TRACE(("DocTempDir is %s",pFile));
    XP_FREE(pFile);
  }
  else {
    XP_TRACE(("No document temp dir was created."));
  }
}
#endif // DEBUG

//-----------------------------------------------------------------------------
// CParseState / CPrintState
//-----------------------------------------------------------------------------
#define PRE_BODY 0
#define IN_BODY 1
#define POST_BODY 2

CParseState::CParseState()
    :m_bInTitle(FALSE),
    m_iDocPart(0),
    m_inJavaScript(0),
    m_baseFontSize(0),
    m_pNextText(0),
    m_pJavaScript(0),
    m_pPostBody(0){
}

XP_Bool CParseState::InBody(){
    return m_iDocPart == IN_BODY;
}

void CParseState::StartBody(){
    m_iDocPart = IN_BODY;
}

void CParseState::EndBody(){
    m_iDocPart = POST_BODY;
}

CStreamOutMemory* CParseState::GetStream(){
    if ( m_iDocPart < POST_BODY ) {
        // m_pJavaScript is mis-named. It's actually
        // a bag we hold all our unprocessed head tags in.
        if( m_pJavaScript == 0 ){
            m_pJavaScript = new CStreamOutMemory();
        }
        return m_pJavaScript;
    }
    else {
        if( m_pPostBody == 0 ){
            m_pPostBody = new CStreamOutMemory();
        }
        return m_pPostBody;
    }
}

CParseState::~CParseState() {
    Free(m_pJavaScript);
    Free(m_pPostBody);
    delete m_pNextText;
}

void CParseState::Free(CStreamOutMemory*& pStream){
    if ( pStream ) {
        // COutMemoryStreams don't delete their text.
        XP_HUGE_CHAR_PTR pData = pStream->GetText();
        if ( pData ) {
            XP_HUGE_FREE(pData);
        }
        delete pStream;
        pStream = NULL;
    }
}

void CParseState::Reset(){
    bLastWasSpace = TRUE;               // at the beginning of the document
                                        //  we should ignore leading spaces
    m_baseFontSize = 3;
    m_formatTypeStack.Reset();
    m_formatTextStack.Reset();
    m_iDocPart = 0;
    m_bInTitle = FALSE;
    m_inJavaScript = 0;
    m_pJavaScript = 0;
    delete m_pNextText;
    // force the compiler to use the non-stream constructor
    m_pNextText = new CEditTextElement((CEditElement*)0,0);
}

//-----------------------------------------------------------------------------
//  CPrintState
//-----------------------------------------------------------------------------
void CPrintState::Reset( IStreamOut* pOut, CEditBuffer *pBuffer ){
    m_pOut = pOut;
    m_iCharPos = 0;
    m_bTextLast = FALSE;
    m_iLevel = 0;
    m_pBuffer = pBuffer;
    m_bEncodeSelectionAsComment = FALSE;
}

void CPrintState::PrintSelectionComment(XP_Bool bEnd, XP_Bool bStickyAfter){
    if ( !bEnd ) {
        m_pBuffer->PasteHTMLHook(this);
    }
    m_pOut->Printf("<!-- %s%s -->", bEnd ? EDT_SELECTION_END_COMMENT : EDT_SELECTION_START_COMMENT,
        bStickyAfter ? "+" : "");
}

XP_Bool CPrintState::ShouldPrintSelectionComments(CEditLeafElement* pElement){
    return m_bEncodeSelectionAsComment && (
        ShouldPrintSelectionComment(pElement, FALSE)
            || ShouldPrintSelectionComment(pElement, TRUE));
}

XP_Bool CPrintState::ShouldPrintSelectionComment(CEditLeafElement* pElement, XP_Bool bEnd){
    return m_bEncodeSelectionAsComment && m_selection.GetEdge(bEnd)->m_pElement == pElement;
}

//-----------------------------------------------------------------------------
// CEditLinkManager
//-----------------------------------------------------------------------------
EDT_HREFData* ED_Link::GetData(){
    EDT_HREFData* pData = EDT_NewHREFData();
    pData->pURL = XP_STRDUP( hrefStr );
    if( pExtra ){
        pData->pExtra = XP_STRDUP( pExtra );
    }
    else {
        pData->pExtra = 0;
    }
    return pData;
}

CEditLinkManager::CEditLinkManager(){
}

ED_Link* CEditLinkManager::MakeLink( char *pHREF, char *pExtra, intn iRefCount ){

    ED_Link *pNewLink = XP_NEW( ED_Link );
    pNewLink->iRefCount = iRefCount;
    pNewLink->pLinkManager = this;
    pNewLink->hrefStr = XP_STRDUP( pHREF );
    pNewLink->bAdjusted = FALSE;
    if( pExtra ){
        pNewLink->pExtra = XP_STRDUP( pExtra );
    }
    else {
        pNewLink->pExtra = 0;
    }
    return pNewLink;
}

void CEditLinkManager::AdjustAllLinks( char *pOldURL, char* pNewURL, ED_HREFList *badLinks ){
    int i;
    for (i = 0; i < m_links.Size(); i++ ){
        ED_Link *pLink;
        if( (pLink = m_links[i]) != 0 ){
            AdjustLink(&pLink->hrefStr,pOldURL,pNewURL,badLinks);
        }
    }
}

// Used for adjusting links, images, etc.
// pNewBase can be NULL, the other args shouldn't be.
//
// Change  *ppURL so that the URL will still point to the same location
// after changing the base URL.
// If there is no way to adjust ppURL, ppURL will not change and we add it to badLinks.
// E.g. if ppURL relative to pOldBase points to a local file and
// pNewBase is a remote site, we can't adjust the URL.
void CEditLinkManager::AdjustLink(char **ppURL,char *pOldBase, char *pNewBase, ED_HREFList *badLinks){

  if (!ppURL || !*ppURL) {
    return;
  }

  if ((*ppURL)[0] == '#' || (*ppURL)[0] == '`') {
    // do nothing, but not an error.
    return;
  }

  // Will make url
  // absolute if the source and dest are on the same machine, just different
  // drives.  If on the same drive, will make a relative URL.
  // If on different drives or different machine, leave URL alone, return FALSE.

  // Make absolute relative to source URL
  char *pAbsURL = NET_MakeAbsoluteURL( pOldBase, *ppURL );
  if (!pAbsURL) {
    // Add to list of bad links.
    AddHREFUnique(badLinks,*ppURL);
    return;
  }
  char *pNewURL = NULL;

  if( pNewBase ){
    // Try to make relative to dest URL.
    int result = NET_MakeRelativeURL( pNewBase, pAbsURL, &pNewURL );

    // If we can't make it relative.
    if (result != NET_URL_SAME_DIRECTORY &&
        result != NET_URL_SAME_DEVICE) {

      if (NET_URL_Type(pAbsURL) == FILE_TYPE_URL &&
          NET_URL_Type(pNewBase) != FILE_TYPE_URL) {
        // No point in adjusting URL or making relative, local file is not
        // visible to a remote machine.  So don't change anything.
        XP_FREEIF(pAbsURL);
        XP_FREEIF(pNewURL);
  
        // Add to list of bad links.
        AddHREFUnique(badLinks,*ppURL);
        return;
      }
      // else pNewURL will be a copy of pAbsURL, so just continue.
      // I.e. we have made the URL absolute.
    }
    // result is same directory or same device.
    else if (result == NET_URL_SAME_DEVICE && (*ppURL)[0] == '/') {
      // Absolute pathing.  Don't force it to be relative.  It'll work in the 
      // new location.
      XP_FREEIF(pAbsURL);
      XP_FREEIF(pNewURL);
      return;
    }
    // More aggressive absolute pathing.  If the new relative url goes all the
    // way up to the root, just make it start from the root instead of having a 
    // bunch of ../../../   Bug 50500.
    // E.g. pAbsURL  = http://host/images/bob.gif,
    //      pNewBase = http://host/dir1/dir2/dir3/doc.html
    // instead of setting pNewURL to 
    //                 ../../../images/bob.gif, use
    //                 /images/bob.gif
    else if (result == NET_URL_SAME_DEVICE) {
      // Count sets of ../ at the beginning of pNewURL.
      int dotDotCount = 0;
      char *pSeek = pNewURL;
      while (pSeek && *pSeek && !XP_STRNCMP(pSeek,"../",3)) {
        dotDotCount++;
        pSeek +=3;
      }

      char *basePath = NET_ParseURL(pNewBase,GET_PATH_PART);
      if (!basePath) {
        XP_ASSERT(0);
        return; 
      }

      /* Find location of drive separator */
      char *basePtr = (char*) XP_STRCHR(basePath, '|');
      if (!basePtr) {
        basePtr = basePath;
        // basePtr now points to first '/' in pNewBase before directories

        // Count number of subdirectories in pNewBase.
        int subDirCount = -1; // Don't count first slash.
        while (basePtr && *basePtr) {
          if (*basePtr == '/') {
            subDirCount++;
          }
          basePtr++;
        }
        XP_FREE(basePath);

        if (dotDotCount >= subDirCount && dotDotCount > 0) {
          if (dotDotCount > subDirCount) {
            // Error, Net_MakeRelativeURL added too many ../
            XP_ASSERT(0);
          }

          // pSeek is pointing to first char after all the ../ 
          char *pTemp = XP_STRDUP(pSeek - 1); // Move back to get the slash.
          XP_FREE(pNewURL);
          pNewURL = pTemp;
        }
      }
      else {
      // there is a drive separator, so this trick won't work.  
      // use pNewURL as is.
        XP_FREE(basePath);
      }
    }
  } else {
    // No new base is supplied - so we just return absolute URL
    XP_FREEIF(*ppURL);
    *ppURL = pAbsURL;
    return;
  }
  
  // else pNewURL is a correct relative URL.  Or an absolute URL if result == NET_URL_FAIL.
  
  XP_FREEIF( pAbsURL );
  XP_FREEIF(*ppURL);
  *ppURL = pNewURL;
}

ED_LinkId CEditLinkManager::Add( char *pHREF, char *pExtra ){
    int i = 0;
    int lastFree = -1;
    while( i < m_links.Size() ){
        ED_Link* pLink = m_links[i];
        if( pLink != 0 ){
            // need a more intelegent way of comparing HREFs
            if( XP_STRCMP( pLink->hrefStr, pHREF) == 0 ) {
                // Links the same. How about pExtra ?
                if ( (!!pExtra == !!pLink->pExtra)
                    && ((pExtra &&
                            XP_STRCMP( pExtra, pLink->pExtra ) == 0 )
                       || pExtra == 0 ) ){
                    pLink->iRefCount++;
                    return pLink;
                }
            }
        }
        else {
            lastFree = i;
        }
        i++;
    }

    ED_Link *pNewLink = MakeLink( pHREF, pExtra );

    //
    // Store it.
    //
    if( lastFree != -1 ){
        m_links[lastFree] = pNewLink;
    }
    else {
        lastFree = m_links.Add( pNewLink );
    }
    pNewLink->linkOffset = lastFree;
    return pNewLink;
}

void CEditLinkManager::Free(ED_LinkId id){
    if( --id->iRefCount == 0 ){
        m_links[ id->linkOffset ] = 0;
        XP_FREEIF( id->pExtra );
        XP_FREEIF( id->hrefStr );
        XP_FREE( id );
    }
}

void CEditLinkManager::AddHREFUnique(ED_HREFList *badLinks,char *pURL) {
  for (int n = 0; n < badLinks->Size(); n++) {
    // Just comparing links for equality by string comparison.
    if (!XP_STRCMP((*badLinks)[n],pURL)) {
      // Already in list.
      return;
    }
  }
  badLinks->Add(XP_STRDUP(pURL));
}

XP_Bool EDT_SelectTableElement(MWContext  *pMWContext, int32 x, int32 y, 
                               LO_Element *pLoElement, 
                               ED_HitType  iHitType, 
                               XP_Bool     bModifierKeyPressed,
                               XP_Bool     bExtendSelection)
{
    GET_EDIT_BUF_OR_RETURN(pMWContext, pEditBuffer) 0;
    return pEditBuffer->SelectTableElement(x, y, pLoElement, iHitType, 
                                           bModifierKeyPressed, bExtendSelection);
}

ED_HitType EDT_ExtendTableCellSelection(MWContext *pMWContext, int32 x, int32 y)
{
    GET_EDIT_BUF_OR_RETURN(pMWContext, pEditBuffer) ED_HIT_NONE;
    return pEditBuffer->ExtendTableCellSelection(x, y);
}

// Dynamic Sizing Object

CSizingObject::CSizingObject() :
        m_pBuffer(0),
        m_pLoElement(0),
        m_iStyle(0),
        m_iXOrigin(0),
        m_iYOrigin(0),
        m_iStartWidth(0),
        m_iStartHeight(0),
        m_iViewWidth(0),
        m_iViewHeight(0),
        m_bWidthPercent(0),
        m_bHeightPercent(0),
        m_bPercentOriginal(0),
        m_bFirstTime(1),
        m_iAddCols(0),
        m_iAddRows(0),
        m_bCenterSizing(0)
{
}

XP_Bool CSizingObject::Create(CEditBuffer *pBuffer,
                              LO_Element *pLoElement,
                              int iSizingStyle,
                              int32 xVal, int32 yVal,
                              XP_Bool bLockAspect, XP_Rect *pRect){
    XP_ASSERT(pBuffer);
    XP_ASSERT(pRect);
    XP_ASSERT(iSizingStyle);
    m_pBuffer = pBuffer;
    m_pLoElement = pLoElement;
    m_iStyle = iSizingStyle;

    if( !m_pLoElement )
    {
#ifdef LAYERS
		m_pLoElement = LO_XYToElement(pBuffer->m_pContext, xVal, yVal, 0);
#else
		m_pLoElement = LO_XYToElement(pBuffer->m_pContext, xVal, yVal);
#endif
    }
    if( !m_pLoElement )
    {
        return FALSE;
    }

    // Get view window size for "100%" sizing
    FE_GetDocAndWindowPosition(pBuffer->m_pContext, &m_iXOrigin, &m_iYOrigin, &m_iViewWidth, &m_iViewHeight);
    if( !m_iViewWidth || !m_iViewHeight )
    {
        return FALSE;
    }
	// Get extra margin
	int32 iMarginWidth;
	int32 iMarginHeight;
	LO_GetDocumentMargins(pBuffer->m_pContext, &iMarginWidth, &iMarginHeight);
	m_iViewWidth -= 2 * iMarginWidth;
	m_iViewHeight -= 2 * iMarginHeight;

    if( m_pLoElement->lo_any.type == LO_CELL )
    {
        LO_Element *pElement = NULL;
        LO_Element *pLastElement = NULL;

        // When resizing a column, we must find a cell that doesn't have COLSPAN > 1
        if( m_iStyle == ED_SIZE_RIGHT && lo_GetColSpan(m_pLoElement) > 1 )
        {
            int32 iRightEdge = m_pLoElement->lo_cell.x + m_pLoElement->lo_cell.width;

            pElement = lo_GetFirstAndLastCellsInTable(m_pBuffer->m_pContext, m_pLoElement, &pLastElement);
            if( pElement )
                do {
                    if( pElement && pElement->lo_any.type == LO_CELL &&
                        (pElement->lo_cell.x + pElement->lo_cell.width) == iRightEdge &&
                        lo_GetColSpan(pElement) == 1 )
                    {
                        m_pLoElement = pElement;
                        break;
                    }
                    pElement = pElement->lo_any.next;
                }
                while( pElement != pLastElement );
        }
        if( m_iStyle == ED_SIZE_BOTTOM && lo_GetRowSpan(m_pLoElement) > 1 )
        {
            int32 iBottomEdge = m_pLoElement->lo_cell.y + m_pLoElement->lo_cell.height;

            if( !pElement )
                pElement = lo_GetFirstAndLastCellsInTable(m_pBuffer->m_pContext, m_pLoElement, &pLastElement);
            if( pElement ) 
                do {
                    if( pElement && pElement->lo_any.type == LO_CELL &&
                        (pElement->lo_cell.y + pElement->lo_cell.height) == iBottomEdge &&
                        lo_GetRowSpan(pElement) == 1 )
                    {
                        m_pLoElement = pElement;
                        break;
                    }
                    pElement = pElement->lo_any.next;
                }
                while( pElement != pLastElement );
        }
    }

    LO_Any *pAny = &(m_pLoElement->lo_any);
    int32 iSelectX = pAny->x + pAny->x_offset;
    int32 iSelectY = pAny->y + pAny->y_offset;

    m_iParentWidth = 0;
    // Check for sizing of table cell or nested table
    if( pAny->type == LO_TABLE )
    {
        // Check for possible nested table by finding if it has a parent cell
        LO_CellStruct *pParentCell = lo_GetParentCell(m_pBuffer->m_pContext, m_pLoElement); 
        if( pParentCell )
        {
            // Width = that of cell containing table
            m_iParentWidth = pParentCell->width;
            m_iWidthMsgID = XP_EDT_PERCENT_CELL;
        }
        iSelectX = pAny->x;
        iSelectY = pAny->y;
    } 
    else if( pAny->type == LO_CELL )
    {
        // 
        LO_TableStruct *pParentTable = lo_GetParentTable(m_pBuffer->m_pContext, m_pLoElement);
        if( pParentTable )
        {
            // Get the width usable for percent calculation (minus borders and cell spacing)
            m_iParentWidth = lo_CalcTableWidthForPercentMode(m_pLoElement);
            m_iWidthMsgID = XP_EDT_PERCENT_TABLE;
        }
        iSelectX = pAny->x;
        iSelectY = pAny->y;
    } else {
        // If not a table or cell, 
        //   select the item of interest to make it the current element
        // Select at its CENTER to avoid setting caret
        //   if real X,Y is near the left or right edge
        iSelectX += pAny->width/2;
        iSelectY += pAny->height/2;
    }
    
    if( m_iParentWidth == 0 )
    {
        m_iParentWidth = m_iViewWidth;
        m_iWidthMsgID = XP_EDT_PERCENT_WINDOW;
    }
    
    // SHOULD WE DO THIS???
    m_pBuffer->MoveAndHideCaretInTable(m_pLoElement);

    // Calculate the rect in View's coordinates;
    // ASSUMES PIXELS ONLY -- NO CONVERSION DONE
    m_Rect.left = pAny->x + pAny->x_offset - m_iXOrigin;
    m_Rect.top = pAny->y + pAny->y_offset - m_iYOrigin;

    // Recent default value of flag used
    //  for image status string, e.g.: (% of original width)
    m_bPercentOriginal = FALSE;

    // Get percent-mode flag for edit element and adjust table/cell data
    switch ( m_pLoElement->type )
    {
        case LO_IMAGE:
        {
            // Get the edit element
            CEditElement * pElement = m_pLoElement->lo_any.edit_element;
            if( !pElement ) return FALSE;

            if( pElement->IsIcon() )
            {
                // All "unknown" tags are handled here
                pElement->GetWidth(&m_bWidthPercent, &m_iStartWidth);
                pElement->GetHeight(&m_bHeightPercent, &m_iStartHeight);
            } else if( pElement->IsImage() )
            {
                EDT_ImageData * pImageData = pElement->Image()->GetImageData();
                //pBuffer->GetImageData();
                if( pImageData )
                {
                    m_bWidthPercent = pImageData->bWidthPercent;
                    m_bHeightPercent = pImageData->bHeightPercent;

                    // Save the original height and width of the image
                    //  if we have it
                    if( pImageData->iOriginalWidth )
                    {
                        m_iStartWidth = pImageData->iOriginalWidth;
                        m_iStartHeight = pImageData->iOriginalHeight;
                        // Constrain relative to "original" dimensions
                        m_bPercentOriginal = TRUE;
                    }
                    EDT_FreeImageData(pImageData);
                }
                // LO_ImageStruct includes border -- adjust
                m_Rect.left += ((LO_ImageStruct*)pAny)->border_width;
                m_Rect.top += ((LO_ImageStruct*)pAny)->border_width;
            }
            break;
        }
        case LO_HRULE: 
        {
            // Get the edit element
            CEditElement * pElement = m_pLoElement->lo_any.edit_element;
            if( !pElement ) return FALSE;

            EDT_HorizRuleData * pHData = pElement->HorizRule()->GetData(); //pBuffer->GetHorizRuleData();
            if( pHData )
            {
                m_bWidthPercent = pHData->bWidthPercent;
                if(  pHData->align == ED_ALIGN_CENTER )
                    m_bCenterSizing = TRUE;

                //Note: we will get width and "size" (height) in pixels below
                m_iStartHeight = pHData->size;
                EDT_FreeHorizRuleData(pHData);
            }
            break;
        }
        case LO_TABLE:
        {
            CEditTableElement *pTable =
                (CEditTableElement*)edt_GetTableElementFromLO_Element( m_pLoElement, LO_TABLE );
            if( pTable )
            {
                EDT_TableData *pTableData = pTable->GetData();
                if( pTableData )
                {
                    switch( iSizingStyle )
                    {
                        case ED_SIZE_RIGHT:
                            // Use width determined by layout
                            m_bWidthPercent = pTableData->bWidthPercent;
                            if( m_bWidthPercent )
                            {
                                m_iStartWidth = (pAny->width * 100) / m_iParentWidth;
                            } else {
                                m_iStartWidth = pAny->width;
                            }
                            break;
                        case ED_SIZE_BOTTOM:
                            // Use Height determined by layout
                            m_bHeightPercent = pTableData->bHeightPercent;
                            if( m_bHeightPercent )
                            {
                                m_iStartHeight = (pAny->height * 100) / m_iViewHeight;
                            } else {
                                m_iStartHeight = pAny->height;
                            }
                            break;
                        case ED_SIZE_ADD_COLS:
                            // When adding columns, show selection only
                            //  around "new" columns, so rect.left = table->right
                            m_Rect.left += pAny->width;
                            m_Rect.right = m_Rect.left + 1;
                            m_Rect.bottom = m_Rect.top + pAny->height;
                            m_iStartHeight = pAny->height;
                            break;

                        case ED_SIZE_ADD_ROWS:
                            // As with columns, rect is around "new" rows
                            m_Rect.top += pAny->height;
                            m_Rect.bottom = m_Rect.top + 1;
                            m_Rect.right = m_Rect.left + pAny->width;
                            m_iStartWidth = pAny->width;
                            break;
                    }
                    EDT_FreeTableData(pTableData);
                }
            }
            break;
        }
        case LO_CELL:
        {
            CEditTableCellElement *pCell = 
                (CEditTableCellElement*)edt_GetTableElementFromLO_Element( m_pLoElement, LO_CELL );
            if( pCell )
            {
                EDT_TableCellData * pCellData = pCell->GetData(0);
                if( pCellData )
                {
                    m_bWidthPercent = pCellData->bWidthPercent;
                    m_bHeightPercent = pCellData->bHeightPercent;
                    EDT_FreeTableCellData(pCellData);

                    if( iSizingStyle == ED_SIZE_RIGHT )
                    {
                        if( m_bWidthPercent )
                        {
                            // IMPORTANT: The "width" we use for cell sizing INCLUDES the
                            //   border and padding values (as does the LO_Element value),
                            //   This is NOT the same as the HTML "WIDTH" param or the Editor's 
                            //   pCellData->width. Same goes for Height
                            m_iStartWidth = (pAny->width * 100) / m_iParentWidth;
                        } else {
                            // Use width determined by layout
                            m_iStartWidth = pAny->width;
                        }
                    } 
                    else 
                    {
                        if( m_bHeightPercent )
                        {
                            m_iStartHeight = (pAny->height * 100) / m_iViewHeight;
                        } else {
                            m_iStartHeight = pAny->height;
                        }
                    }                

                    // We will select the entire row or column, so get size of table
                    LO_TableStruct * pLoTable = lo_GetParentTable(pBuffer->m_pContext, m_pLoElement);
                    if( pLoTable )
                    {
                        int32 iAdjust = min(pLoTable->border_width, 4 - pLoTable->inter_cell_space);
                        if( iSizingStyle == ED_SIZE_RIGHT )
                        {
                            // Prefered top and bottom is just inside table border
                            m_Rect.top = pLoTable->y + pLoTable->y_offset - m_iYOrigin + pLoTable->border_width;
                            m_Rect.bottom = m_Rect.top + pLoTable->height - 2*pLoTable->border_width;
                    
                            // Back off top and bottom if intercell space is too small
                            if( pLoTable->inter_cell_space < 4 && iAdjust > 0 )
                            {
                                m_Rect.top -= iAdjust;
                                m_Rect.bottom += iAdjust;
                            }
                            m_Rect.right = m_Rect.left + pAny->width;
                        } 
                        else 
                        {
                            // Prefered left and right is just inside table border
                            m_Rect.left = pLoTable->x + pLoTable->x_offset - m_iXOrigin + pLoTable->border_width;
                            m_Rect.right = m_Rect.left + pLoTable->width - 2*pLoTable->border_width;
                    
                            // Back off if intercell space is too small
                            if( pLoTable->inter_cell_space < 4 && iAdjust > 0 )
                            {
                                m_Rect.left -= iAdjust;
                                m_Rect.right += iAdjust;
                            }
                            m_Rect.bottom = m_Rect.top + pAny->height;
                        }
                    }
                }
            }
            break;
        }
    }

    if( !m_bWidthPercent )
    {
        // We aren't doing percent, so set message ID for pixels
        m_iWidthMsgID = XP_EDT_PIXELS;
    }

    // Finish calculating object rect if not set for table sizing above
    if( iSizingStyle != ED_SIZE_ADD_COLS && iSizingStyle != ED_SIZE_ADD_ROWS &&
        m_pLoElement->type != LO_CELL )
    {
        m_Rect.right = m_Rect.left + pAny->width;
        m_Rect.bottom = m_Rect.top + pAny->height;
    }

    // If we didn't set above, use current size or minimum of 1
    if( m_iStartWidth == 0 )
    {
        m_iStartWidth = max(1, pAny->width);
    }
    if( m_iStartHeight == 0 )
    {
        m_iStartHeight = max(1, pAny->height);
    }

    // Call this now primarily to start the 
    //  status line display of sizing information
    GetSizingRect(xVal, yVal, bLockAspect, &m_Rect);

    if( pRect )
        *pRect = m_Rect;
        
    return TRUE;
}

#define  EDT_NEW_ROW_HEIGHT  24
#define  EDT_NEW_COL_WIDTH   24

PRIVATE
void CalcAddRowRect(int32 iRows, XP_Rect *pTableRect, XP_Rect *pRect)
{
    pRect->left = pTableRect->left + 6;
    pRect->right = pTableRect->right - 6;
    pRect->top = pTableRect->top + (iRows * EDT_NEW_ROW_HEIGHT) + 2;
    pRect->bottom = pRect->top;
}

PRIVATE
void CalcAddColRect(int32 iCols, XP_Rect *pTableRect, XP_Rect *pRect)
{
    pRect->top = pTableRect->top + 6;
    pRect->bottom = pTableRect->bottom - 6;
    pRect->left = pTableRect->left + (iCols * EDT_NEW_COL_WIDTH) + 2;
    pRect->right = pRect->left;
}

// Corner must be > any single side value
#define  EDT_IS_SIZING_CORNER(style)   (style == ED_SIZE_TOP_LEFT || \
                                        style == ED_SIZE_TOP_RIGHT || \
                                        style == ED_SIZE_BOTTOM_LEFT || \
                                        style == ED_SIZE_BOTTOM_RIGHT )

XP_Bool CSizingObject::GetSizingRect(int32 xVal, int32 yVal, XP_Bool bLockAspect, XP_Rect *pRect)
{
    XP_ASSERT(pRect);
    int i;
    XP_Bool bLockAspectRatio = EDT_IS_SIZING_CORNER(m_iStyle) && bLockAspect;

    int32 iViewX = xVal - m_iXOrigin;
    int32 iViewY = yVal - m_iYOrigin;

    // Calculate the new rectangle
    // And don't allow dragging past opposite side:
    XP_Rect new_rect = m_Rect;

    int32 iAmountMoved = 0;
    if( m_iStyle == ED_SIZE_RIGHT )
        iAmountMoved = max(m_Rect.left, iViewX) - m_Rect.right;
    else if( m_iStyle == ED_SIZE_LEFT )
        iAmountMoved = m_Rect.left - min(m_Rect.right, iViewX);

    if( m_bWidthPercent )
    {
        // In % mode, limit largest value to get 100% of width or height
        // Calc. the pixel value corresponding to just under 101% width or height,
        //  so roundoff will always allow achieving 100%
        int32 iFullWidth = ((101 * m_iParentWidth) / 100) - 1;
        int32 iFullHeight = ((101 * m_iViewHeight) / 100) - 1;

        if( m_iStyle & ED_SIZE_TOP )
        {
            new_rect.top = min(m_Rect.bottom, max(iViewY,m_Rect.bottom-iFullHeight));
        }
        if( m_iStyle & ED_SIZE_BOTTOM )
        {
            new_rect.bottom = max(m_Rect.top, min(iViewY,m_Rect.top+iFullHeight));
        }
        if( m_iStyle & ED_SIZE_LEFT )
        {
            if( m_bCenterSizing )
            {
                new_rect.left -= iAmountMoved;
                new_rect.right += iAmountMoved;
            }
            else
                new_rect.left = min(m_Rect.right, max(iViewX,m_Rect.right-iFullWidth));

        }
        if( m_iStyle & ED_SIZE_RIGHT )
        {
            if( m_bCenterSizing )
            {
                new_rect.left -= iAmountMoved;
                new_rect.right += iAmountMoved;
            }
            else
                new_rect.right = max(m_Rect.left, iViewX);
        }
    } else {
        if( m_iStyle & ED_SIZE_TOP )
        {
            new_rect.top = min(m_Rect.bottom, iViewY);
        }
        if( m_iStyle & ED_SIZE_BOTTOM || m_iStyle == ED_SIZE_ADD_ROWS )
        {
            new_rect.bottom = max(m_Rect.top, iViewY);
        }
        if( m_iStyle & ED_SIZE_LEFT )
        {
            if( m_bCenterSizing )
            {
                new_rect.left -= iAmountMoved;
                new_rect.right += iAmountMoved;
            }
            else
                new_rect.left = min(m_Rect.right, iViewX);
        }
        if( m_iStyle & ED_SIZE_RIGHT  || m_iStyle == ED_SIZE_ADD_COLS )
        {
            if( m_bCenterSizing )
            {
                new_rect.left -= iAmountMoved;
                new_rect.right += iAmountMoved;
            }
            else
                new_rect.right = max(m_Rect.left, iViewX);
        }
    }
    int iNewWidth = new_rect.right - new_rect.left;
    int iNewHeight = new_rect.bottom - new_rect.top;

    // When sizing both width and height,
    //   fixup coordinates to lock aspect ratio
    //
    if(bLockAspectRatio)
    {
        int iLockedWidth = (iNewHeight * m_iStartWidth) / m_iStartHeight;
        int iLockedHeight = (iNewWidth * m_iStartHeight) / m_iStartWidth;

        if( iNewWidth < iLockedWidth )
        {
            // New width is too small to match new height - adjust
            if( m_iStyle & ED_SIZE_LEFT ){
                new_rect.left = new_rect.right - iLockedWidth;
            } else {
                new_rect.right = new_rect.left + iLockedWidth;
            }
        } else if( iNewHeight < iLockedHeight )
        {
            // New height is too small to match new width
            if( m_iStyle & ED_SIZE_TOP )
            {
                new_rect.top = new_rect.bottom - iLockedHeight;
            } else {
                new_rect.bottom = new_rect.top + iLockedHeight;
            }
        }
    }

    // Return new rect to caller
    *pRect = new_rect;
    XP_Bool bRectChanged = new_rect.top != m_Rect.top ||
                           new_rect.bottom != m_Rect.bottom ||
                           new_rect.left != m_Rect.left ||
                           new_rect.right != m_Rect.right;
    
    // Continue if rect is different from last rect requested
    //    or its the first time here (so status is displayed on initial mouse down)
    if( bRectChanged || m_bFirstTime )
    {
        m_Rect = new_rect;

        // Display status message with data for user:
        char   pMsg[512] = "";
        char   pPercentOfWhat[256] = "";
        int32  iWidth = m_Rect.right - m_Rect.left;
        int32  iHeight = m_Rect.bottom - m_Rect.top;
        int32  iPercent;

        XP_Bool bDoWidth = (m_iStyle & ED_SIZE_LEFT) || (m_iStyle & ED_SIZE_RIGHT);
        XP_Bool bDoHeight = (m_iStyle & ED_SIZE_TOP) || (m_iStyle & ED_SIZE_BOTTOM);

        // Show only dimension(s) for whats changing: (Width vs. Height vs both)
        // First part of status shows: "Width = xx[pixesl|%of window width]"
        //  and/or similar string for Height.
        //
        if( bDoWidth )
        {
            iPercent = (iWidth * 100 ) / m_iStartWidth;
            if(m_bWidthPercent)
            {
                // Convert to % format
                iWidth = (iWidth * 100) / m_iParentWidth;
            }

            // "Width = x"
            PR_snprintf(pMsg, 128, XP_GetString(XP_EDT_WIDTH_EQUALS), iWidth);
            // "pixels" or "% of window"

            strcat(pMsg, XP_GetString(m_iWidthMsgID));

            // The % of original [width] and/or [height] string
            // Current logic assumes constaining aspect ratio when sizing corners,
            //   so separate Width, Height percentages are not shown
            XP_STRCPY(pPercentOfWhat, XP_GetString(XP_EDT_WIDTH));
        }
        if( bDoHeight )
        {
            // Since corners are constrained to aspect ratio,
            //  just use Width's calculation if already done
            if( !bDoWidth )
            {
                iPercent = (iHeight * 100 ) / m_iStartHeight;
            }
            if(m_bHeightPercent)
            {
                iHeight = (iHeight * 100) / m_iViewHeight;
            }

            // "Height = x"
            PR_snprintf(pMsg+XP_STRLEN(pMsg), 128,
                        XP_GetString(XP_EDT_HEIGHT_EQUALS), iHeight);
            // "pixels" or "% of window"
            strcat(pMsg, XP_GetString(m_bHeightPercent ? (int)XP_EDT_PERCENT_WINDOW :
                                                         (int)XP_EDT_PIXELS));

            if( bDoWidth )
            {
                strcat(pPercentOfWhat, XP_GetString(XP_EDT_AND));
            }
            strcat(pPercentOfWhat, XP_GetString(XP_EDT_HEIGHT));
        }
        if( bDoWidth || bDoHeight )
        {
            // Build string to report relative size change:
            //  (% of [original|previous] [width | height | width and height])
            PR_snprintf(pMsg+XP_STRLEN(pMsg), 128,
                        XP_GetString(m_bPercentOriginal ? (int)XP_EDT_PERCENT_ORIGINAL :
                                                          (int)XP_EDT_PERCENT_PREVIOUS),
                        iPercent, pPercentOfWhat);
        } else if( m_iStyle == ED_SIZE_ADD_COLS )
        {
            int iAddCols = iNewWidth / EDT_NEW_COL_WIDTH;
            PR_snprintf(pMsg, 128, XP_GetString(XP_EDT_ADD_COLUMNS), iAddCols);
            XP_Rect rect;

            // Erase all existing lines
            // NOTE: We can't draw/erase just the "new" line because 
            //   we may skip lines if mouse is moved fast, causing a mess
            for( i=1; i< m_iAddCols; i++ )
            {
                CalcAddColRect(i, &new_rect, &rect);
                if( bRectChanged )
                    FE_DisplayAddRowOrColBorder(m_pBuffer->m_pContext, &rect, TRUE);
            }
            // Draw all new lines based on new number of columns
            for( i=1; i< iAddCols; i++ )
            {
                CalcAddColRect(i, &new_rect, &rect);
                if( bRectChanged )
                    FE_DisplayAddRowOrColBorder(m_pBuffer->m_pContext, &rect, FALSE);
            }
            m_iAddCols = iAddCols;
        } else if(  m_iStyle == ED_SIZE_ADD_ROWS )
        {
            int iAddRows = iNewHeight / EDT_NEW_ROW_HEIGHT;
            PR_snprintf(pMsg, 128, XP_GetString(XP_EDT_ADD_ROWS), iAddRows);
            XP_Rect rect;
            
            // Erase all existing lines
            for( i=1; i< m_iAddRows; i++ )
            {
                CalcAddRowRect(i, &new_rect, &rect);
                if( bRectChanged )
                    FE_DisplayAddRowOrColBorder(m_pBuffer->m_pContext, &rect, TRUE);
            }
            // Draw all new lines based on new number of columns
            for( i=1; i< iAddRows; i++ )
            {
                CalcAddRowRect(i, &new_rect, &rect);
                if( bRectChanged )
                   FE_DisplayAddRowOrColBorder(m_pBuffer->m_pContext, &rect, FALSE);
            }
            m_iAddRows = iAddRows;
        }

        FE_Progress(m_pBuffer->m_pContext, pMsg);

        m_bFirstTime = FALSE;

        // Indicate that rect changed
        return bRectChanged;
    }
    // Rect is same as last time
    return FALSE;
}

void CSizingObject::ResizeObject()
{
    // Erase visual feedback when adding rows or columns
    EraseAddRowsOrCols();
    int32 iWidth, iWidthPixels, iHeightPixels, iHeight;

    // Get the element being sized (except table or cell - obtained below)
     CEditLeafElement *pElement = (CEditLeafElement*)(m_pLoElement->lo_any.edit_element);

    if( !(m_iStyle == ED_SIZE_ADD_ROWS || m_iStyle == ED_SIZE_ADD_COLS) )
    {
        iWidthPixels = m_Rect.right - m_Rect.left;
        iHeightPixels = m_Rect.bottom - m_Rect.top;

        // Convert to percent if that's what we are using
        // Note that we do not change that mode when returning new size
        if( m_bWidthPercent )
        {
            iWidth = (iWidthPixels * 100) / m_iParentWidth;
        } else {
            iWidth = iWidthPixels;        
        }
        if( m_bHeightPercent )
        {
            iHeight = (iHeight * 100) / m_iViewHeight;
        } else {
            iHeight = iHeightPixels;
        }
    }

    // Change the appropriate element's width and/or height
    switch ( m_pLoElement->type )
    {
        case LO_IMAGE:
        {
            if( pElement && pElement->IsIcon() )
            {
                // TODO: ALL "UNKNOWN" TAGS ARE HANDLED HERE
				// Only change dimension if we were sizing it
				if( !(m_iStyle & ED_SIZE_LEFT ||
					  m_iStyle & ED_SIZE_RIGHT) )
                {
                    iWidth = 0; // Signal to not change width...
				}
				if( !(m_iStyle & ED_SIZE_TOP ||
					  m_iStyle & ED_SIZE_BOTTOM) )
                {
                    iHeight = 0; // ...or height
                }
                if( iWidth > 0 || iHeight > 0 )
                {
                    m_pBuffer->BeginBatchChanges(kSetUnknownTagDataCommandID);
                    pElement->SetSize(m_bWidthPercent, iWidth, m_bHeightPercent, iHeight);
                    m_pBuffer->EndBatchChanges();
				}
                m_pBuffer->Relayout(pElement, 0);
            } else if( pElement && pElement->IsImage() )
            {
                EDT_ImageData * pImageData = pElement->Image()->GetImageData();
                if( pImageData )
                {

					// Only change dimension if we were sizing it
					if( m_iStyle & ED_SIZE_TOP ||
						m_iStyle & ED_SIZE_BOTTOM )
                    {
	                    pImageData->iHeight = iHeight;
					}
					if( m_iStyle & ED_SIZE_LEFT ||
						m_iStyle & ED_SIZE_RIGHT )
                    {
						pImageData->iWidth = iWidth;
					}
                    m_pBuffer->BeginBatchChanges(kSetImageDataCommandID);
                    // We are not changing image URL, so third param
                    //   (keep-images-with-doc) shouldn't matter
                    pElement->Image()->SetImageData(pImageData);
                    edt_FreeImageData(pImageData);
                    m_pBuffer->Relayout(pElement,0);
                    m_pBuffer->EndBatchChanges();
               }
            }
            break;
        }
        case LO_HRULE:
        {
            if( pElement )
            {
                EDT_HorizRuleData * pHData = pElement->HorizRule()->GetData(); //m_pBuffer->GetHorizRuleData();
                if( pHData )
                {
				    // Only change dimension if we were sizing it
				    if( m_iStyle & ED_SIZE_TOP ||
					    m_iStyle & ED_SIZE_BOTTOM )
                    {
	                    pHData->size = iHeight;
				    }
				    if( m_iStyle & ED_SIZE_LEFT ||
					    m_iStyle & ED_SIZE_RIGHT )
                    {
	                    pHData->iWidth = iWidth;
				    }
                    m_pBuffer->BeginBatchChanges(kSetHorizRuleDataCommandID);
                    pElement->HorizRule()->SetData(pHData);
                    EDT_FreeHorizRuleData(pHData);
                    m_pBuffer->Relayout(pElement,0);
                    m_pBuffer->EndBatchChanges();
                }
            }
            break;
        }
        case LO_TABLE:
        {
            CEditTableElement *pTable = 
                (CEditTableElement*)edt_GetTableElementFromLO_Element( m_pLoElement, LO_TABLE );
            if( pTable )
            {
                if( m_iStyle == ED_SIZE_ADD_ROWS ||
                    m_iStyle == ED_SIZE_ADD_COLS )
                {
                    m_pBuffer->BeginBatchChanges(m_iStyle == ED_SIZE_ADD_COLS ? 
                                                    kInsertTableColumnCommandID : kInsertTableRowCommandID);

                    // Be sure caret is already in the table,
                    //  but don't show caret or scroll window
                    m_pBuffer->MoveAndHideCaretInTable(m_pLoElement);

                    // Move to last cell of table
                    // (1ST param unused, 2nd = Forward, 3rd = NextRow index ptr
                    while(m_pBuffer->NextTableCell(FALSE, TRUE, 0));

                    if( m_iStyle == ED_SIZE_ADD_ROWS )
                        m_pBuffer->InsertTableRows(NULL, TRUE, m_iAddRows);
                    else
                        m_pBuffer->InsertTableColumns(NULL, TRUE, m_iAddCols);
                    
                    // InsertTable... will relayout
                    //m_pBuffer->Relayout(pTable, 0);

                    m_pBuffer->EndBatchChanges();
                }
                else
                {

                    // Sizing table width or height
                    EDT_TableData * pTableData = pTable->GetData();
                    XP_Bool bChangeWidth = FALSE;
                    XP_Bool bChangeHeight = FALSE;
                    if( pTableData )
                    {
                        XP_Bool bChangeTable = FALSE;
                        if( (m_iStyle & ED_SIZE_RIGHT) && iWidth != m_iStartWidth )
                        {
                            pTableData->bWidthDefined = TRUE;
                            pTableData->iWidth = iWidth;
                            pTableData->iWidthPixels = iWidthPixels;
                            bChangeWidth = TRUE;
                        }
                        if( (m_iStyle & ED_SIZE_BOTTOM) && iHeight != m_iStartHeight )
                        {
                            pTableData->bHeightDefined = TRUE;
                            pTableData->iHeight = iHeight;
                            pTableData->iHeightPixels = iHeightPixels;
                            bChangeHeight = TRUE;
                        }
                        if( bChangeWidth || bChangeHeight )
                        {
                            m_pBuffer->BeginBatchChanges(kSetTableDataCommandID);
                            pTable->SetData(pTableData);
                            // Use this instead of Relayout to properly
                            //  adjust sizing params before relayout, then restore after
                            m_pBuffer->ResizeTable(pTable, bChangeWidth, bChangeHeight);
                            m_pBuffer->EndBatchChanges();
                        }
                        EDT_FreeTableData(pTableData);
                    }
                }
            }
            break;
        }
        case LO_CELL:
        {
            CEditTableCellElement *pCell = 
                (CEditTableCellElement*)edt_GetTableElementFromLO_Element( m_pLoElement, LO_CELL );
            if( pCell )
            {
                CEditTableElement *pTable = pCell->GetParentTable();
                // Should never happen!
                if( !pTable )
                    break;

                XP_Bool bChangeWidth = FALSE;
                XP_Bool bChangeHeight = FALSE;

                m_pBuffer->BeginBatchChanges(kSetTableCellDataCommandID);

                if( (m_iStyle & ED_SIZE_RIGHT) && iWidth != m_iStartWidth )
                {
                    bChangeWidth = TRUE;
                    EDT_TableCellData *pData = pCell->GetData();
                    if( pData )
                    {
                        pData->bWidthDefined = TRUE;
                        pData->iWidthPixels = iWidthPixels;
                        // Resize all cells sharing or spanning the right border
                        pCell->SetColumnWidthRight(pTable, m_pLoElement, pData);
                        EDT_FreeTableCellData(pData);
                    }
                }                   

                if( (m_iStyle & ED_SIZE_BOTTOM) && iHeight != m_iStartHeight )
                {
                    bChangeHeight = TRUE;
                    EDT_TableCellData *pData = pCell->GetData();
                    if( pData )
                    {
                        pData->bHeightDefined = TRUE;
                        pData->iHeightPixels = iHeightPixels;
                        // Resize all cells sharing or spanning the bottom border
                        pCell->SetRowHeightBottom(pTable, m_pLoElement, pData);
                        EDT_FreeTableCellData(pData);
                    }
                }
                if( bChangeWidth || bChangeHeight )
                {
                    // Relayout the entire table
                    // (This modifies current table and cell params
                    //  to facilitate resizing, then restores after Relayout)
                    m_pBuffer->ResizeTableCell(pTable, bChangeWidth, bChangeHeight);
                }
                m_pBuffer->EndBatchChanges();
            }
            break;
        }
    }
}

void CSizingObject::EraseAddRowsOrCols()
{
    int i;
    XP_Rect rect;
    if( m_iStyle == ED_SIZE_ADD_COLS )
    {
        // Erase all existing lines
        // NOTE: We can't draw/erase just the "new" line because 
        //   we may skip lines if mouse is moved fast, causing a mess
        for( i=1; i< m_iAddCols; i++ )
        {
            CalcAddColRect(i, &m_Rect, &rect);
            FE_DisplayAddRowOrColBorder(m_pBuffer->m_pContext, &rect, TRUE);
        }
    } else if(  m_iStyle == ED_SIZE_ADD_ROWS )
    {
        // Erase all existing lines
        for( i=1; i< m_iAddRows; i++ )
        {
            CalcAddRowRect(i, &m_Rect, &rect);
            FE_DisplayAddRowOrColBorder(m_pBuffer->m_pContext, &rect, TRUE);
        }
    }
}

CSizingObject::~CSizingObject()
{
    // Assure that next comparison to a new rect
    //   will be different
    m_Rect.left = -1;
    FE_Progress(m_pBuffer->m_pContext, "");

    // Don't leave object in selected state
    m_pBuffer->ClearSelection(TRUE, FALSE);
}
///////////////////////////////////////////////////

// Relayout timer ... doesn't quite work perfectly.

CRelayoutTimer::CRelayoutTimer(){
    m_pBuffer = NULL;
}

void CRelayoutTimer::SetEditBuffer(CEditBuffer* pBuffer){
    m_pBuffer = pBuffer;
}

void CRelayoutTimer::OnCallback() {
    m_pBuffer->Relayout(m_pStartElement, m_iOffset);
}

void CRelayoutTimer::Flush() {
    if (IsTimeoutEnabled() ) {
        Cancel();
        OnCallback();
    }
}

void CRelayoutTimer::Relayout( CEditElement *pStartElement, int iOffset)
{
	const uint32 kRelayoutDelay = 50;
    XP_Bool bSetValues = TRUE;
    XP_Bool bInTable = pStartElement->GetTableIgnoreSubdoc() != NULL;

    if ( ! bInTable ) {
        Flush();
        m_pBuffer->Relayout(pStartElement, iOffset);
        return;
    }
	if ( IsTimeoutEnabled() ) {
		Cancel();
        if ( pStartElement != m_pStartElement ) {
            OnCallback(); // Do saved relayout now.
        }
        else {
            bSetValues = FALSE;
        }
	}
    if ( bSetValues) {
        m_pStartElement = pStartElement;
        m_iOffset = iOffset;
    }

    // Put the character into the LO_Element string so that the two data structures are consistent.
    if (pStartElement->IsText() ){
        CEditTextElement* pTextElement = pStartElement->Text();
        LO_TextStruct* pLoText = NULL;
        int iLoOffset = 0;
        if ( pTextElement->GetLOTextAndOffset(iOffset, FALSE, pLoText, iLoOffset) ) {
            if ( pLoText ) {
                int32 textLen = pLoText->text_len;
		        char *pText;
                if (textLen <= 0 ){
                    PA_FREE(pLoText->text);
                    pLoText->text = (PA_Block) PA_ALLOC(2); // char plus zero terminated.
                }
                else {
                    pLoText->text = (PA_Block) PA_REALLOC(pLoText->text, textLen + 2);
                }
		        PA_LOCK(pText, char *, pLoText->text);
                int32 bytesToMove = textLen - iLoOffset;
                if ( bytesToMove > 0){
                    XP_MEMMOVE(pText + iLoOffset +1, pText + iLoOffset, bytesToMove);
                }
                pText[iLoOffset] = pTextElement->GetText()[iOffset];
                pText[textLen+1] = '\0';
                // XP_TRACE(("pText=%08x %s %d\n", pText, pText, textLen));
		        PA_UNLOCK(pLoText->text);
                textLen++;
                pLoText->text_len = (int16) textLen;
            }
        }
    }
    SetTimeout(kRelayoutDelay);
}

CAutoSaveTimer::CAutoSaveTimer() {
     m_pBuffer = NULL;
     m_iMinutes = 0;
     m_bSuspended = FALSE;
     m_bCalledWhileSuspended = FALSE;
}

void CAutoSaveTimer::SetEditBuffer(CEditBuffer* pBuffer){
    m_pBuffer = pBuffer;
    // Sanity check, is this an OK Buffer?
    XP_ASSERT(m_pBuffer->m_pRoot);
}

void CAutoSaveTimer::OnCallback() {
    if( m_bSuspended ){
        m_bCalledWhileSuspended = TRUE;
    } else {
        // Sanity check -- is this an OK Buffer?
        if ( ! m_pBuffer || ! m_pBuffer->m_pRoot ) {
            XP_ASSERT(FALSE);
            return;
        }
        // Skip autosave if currently suspended
        m_pBuffer->AutoSaveCallback();
        Restart();
    }
}

void CAutoSaveTimer::Restart() {
#ifdef DEBUG_AUTO_SAVE
    const int32 kMillisecondsPerMinute = 6000L; //60000L; // * 1000 / 10;
#else
    const int32 kMillisecondsPerMinute = 60000L;
#endif
    if ( m_iMinutes > 0 ) {
        SetTimeout(m_iMinutes * kMillisecondsPerMinute);
    }
}

void CAutoSaveTimer::SetPeriod(int32 minutes){
    Cancel();
    m_iMinutes = minutes;
    Restart();
}

int32 CAutoSaveTimer::GetPeriod(){
    return m_iMinutes;
}

void CAutoSaveTimer::Suspend(){
    m_bSuspended = TRUE;
}

void CAutoSaveTimer::Resume(){
    m_bSuspended = FALSE;
    // We need to restart the timer ONLY
    //  if we were called during a suspended period
    if( m_bCalledWhileSuspended ){
        m_bCalledWhileSuspended = FALSE;
    } else {
        Restart();
    }
}

CEditDocState::CEditDocState() {
  m_pBuffer = NULL;
  m_version = 0;
}

CEditDocState::~CEditDocState() {
  if (m_pBuffer) {
    XP_HUGE_FREE(m_pBuffer);
  }
}

#ifdef DEBUG
void CEditDocState::Print(IStreamOut& stream) {
  if (!m_pBuffer) return;

  stream.Printf("%d bytes", XP_STRLEN(m_pBuffer));
  // have to dump one line at a time. XP_TRACE has a 512 char limit on Windows.
  char* b = m_pBuffer;
  while ( *b != '\0' ) {
      char* b2 = b;
      while ( *b2 != '\0' && *b2 != '\n'){
          b2++;
      }
      char old = *b2;
      *b2 = '\0';
      stream.Printf("%s\n", b);
      *b2 = old;
      b = b2 + 1;
      if ( old == '\0' ) break;
  }
}
#endif



/* Take the document for current context and change appropriate
 *   params to make it look like a "Untitled" new document
 * Allows loading any document as a "Template"
 */
void EDT_ConvertCurrentDocToNewDoc(MWContext * pMWContext)
{
    XP_ASSERT(pMWContext);
    if( !pMWContext ){
        return;
    }
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pMWContext, pEditBuffer);

    char * pUntitled = XP_GetString(XP_EDIT_NEW_DOC_NAME);

    // Traverse all links and images and change URLs to absolute
    //   since we will be destroying our current base doc URL
    EDT_ImageData *pImageData;
    char *pAbsolute = NULL;
    EDT_PageData *pPageData = pEditBuffer->GetPageData();
    if( !pPageData){
        return;
    }
    
    // Should be the same as pEntry->Address???
    char *pBaseURL = LO_GetBaseURL(pMWContext);

    // Call Java Plugin hook for pages to be closed,
    //  BUT only if it really was an lockable source
    int iType = NET_URL_Type(pBaseURL);
    if( iType == FTP_TYPE_URL ||
        iType == HTTP_TYPE_URL ||
        iType == SECURE_HTTP_TYPE_URL ||
        iType == FILE_TYPE_URL ){
        EDT_PreClose(pMWContext, pBaseURL, NULL, NULL);
    }

    // Walk the tree and find all HREFs.
    CEditElement *pLeaf = pEditBuffer->m_pRoot->FindNextElement( 
                                  &CEditElement::FindLeafAll,0 );
    // First sweep, mark all HREFs as not adjusted.
    while (pLeaf) {
        pEditBuffer->linkManager.SetAdjusted(pLeaf->Leaf()->GetHREF(),FALSE);
        pLeaf = pLeaf->FindNextElement(&CEditElement::FindLeafAll,0 );
    }
    // Second sweep, actually adjust the HREFs.
    pLeaf = pEditBuffer->m_pRoot->FindNextElement( 
            &CEditElement::FindLeafAll,0 );
    while (pLeaf) {
        ED_LinkId linkId = pLeaf->Leaf()->GetHREF();
        if (linkId && !pEditBuffer->linkManager.GetAdjusted(linkId)) {
            pEditBuffer->linkManager.AdjustLink(linkId, pBaseURL, NULL, NULL);          
            pEditBuffer->linkManager.SetAdjusted(linkId,TRUE);
        }
        pLeaf = pLeaf->FindNextElement(&CEditElement::FindLeafAll,0 );
    }

    // Regular images.
    CEditElement *pImage = pEditBuffer->m_pRoot->FindNextElement( 
                                   &CEditElement::FindImage, 0 );
    while( pImage ){
        pImageData = pImage->Image()->GetImageData();
        if( pImageData ){
            if( pImageData->pSrc && *pImageData->pSrc ){
                char * pOld = XP_STRDUP(pImageData->pSrc);
                pAbsolute = NET_MakeAbsoluteURL( pBaseURL, pImageData->pSrc );
                if( pAbsolute ){
                    XP_FREE(pImageData->pSrc);
                    pImageData->pSrc = pAbsolute;
                }
             }
             if( pImageData->pLowSrc && *pImageData->pLowSrc){
                pAbsolute = NET_MakeAbsoluteURL( pBaseURL, pImageData->pLowSrc );
                if( pAbsolute ){
                    XP_FREE(pImageData->pLowSrc);
                    pImageData->pLowSrc = pAbsolute;
                }
            }    
            pImage->Image()->SetImageData( pImageData );
            edt_FreeImageData( pImageData );
        }
        pImage = pImage->FindNextElement( &CEditElement::FindImage, 0 );
    }

    // If there is a background Image, make it absolute also
    if( pPageData->pBackgroundImage && *pPageData->pBackgroundImage){
        pAbsolute = NET_MakeAbsoluteURL( pBaseURL, pPageData->pBackgroundImage );
        if( pAbsolute ){
            XP_FREE(pPageData->pBackgroundImage);
            pPageData->pBackgroundImage = pAbsolute;
        }
    }

    // Change context's "title" string
    XP_FREEIF(pMWContext->title);
    pMWContext->title = NULL;
    pMWContext->is_new_document = TRUE;

    // Change the history entry data
    History_entry * pEntry = SHIST_GetCurrent(&(pMWContext->hist));
	if(pEntry ){
        XP_FREEIF(pEntry->address);
        pEntry->address = XP_STRDUP(pUntitled);
        XP_FREEIF(pEntry->title);
    }
    // Layout uses this as the base URL for all links and images
    LO_SetBaseURL( pMWContext, pUntitled );

    // Cleat the old title in page data
    XP_FREEIF(pPageData->pTitle); 

    // This will set new background image,
    //   call FE_SetDocTitle() with new "file://Untitled" string,
    //   and refresh the layout of entire doc
    pEditBuffer->SetPageData(pPageData);

    pEditBuffer->FreePageData(pPageData);
}

char * EDT_GetFilename(char * pURL, XP_Bool bMustHaveExtension)
{
    // No string or its empty
    if( !pURL || !*pURL ){
        return NULL;
    }
    
    // Isolate just the filename of source URL
	char * pFilename = XP_STRRCHR(pURL, '/');

    // Maybe its a local filename?
	if(!pFilename){
		pFilename = XP_STRRCHR(pURL, '\\');
    }

    // Or just after host or drive letter?
	if(!pFilename){
		pFilename = XP_STRCHR(pURL, ':');
    }

    if(pFilename){
        // Filename starts just after "/"
        pFilename++;
    } else {
        // Use what we have - maybe not a full URL spec?
        pFilename = pURL;
    }
    char * pResult = NULL;

    if( pFilename && *pFilename ){
        // Don't include any stuff after filename
        char * ques = XP_STRCHR(pFilename, '?');
	    char * hash = XP_STRCHR(pFilename, '#');
	    if(ques){
		    *ques = '\0';
        }
	    if(hash){
		    *hash = '\0';
        }

        // Check if we must have a period to be considered a filename
        // This allow text without "." to be considered a subdirectory, not filename
        if( *pFilename ){
            if( !bMustHaveExtension || 
                 (bMustHaveExtension && XP_STRCHR(pFilename, '.')) ){
                // Copy just the filename if we have one
                pResult = XP_STRDUP(pFilename);
            }
        }

        // set the values back 
	    if(ques){
		    *ques = '?';
        }
	    if(hash){
		    *hash = '#';
        }
    }    
    return pResult;
}

 char * EDT_ReplaceFilename(char * pBaseURL, char * pURL, XP_Bool bMustHaveExtension)
{
    char * pResult = NULL;
    if( !pBaseURL || !*pBaseURL ){
        return NULL;
    }
    // Find the start of filename in base - 
    // copy to new string
    // **** We overload the meaning of bMustHaveExtension to 
    //      also cause any filename on the base to have an extension,
    //      else any name without trailing slash, such as //chainsaw/mydir",
    //      will be "corrected" by adding trailing slash
    if( bMustHaveExtension ){
        char * pBaseFilename = EDT_GetFilename(pBaseURL, bMustHaveExtension);
        int iLen = XP_STRLEN(pBaseURL);
        if( !pBaseFilename && pBaseURL[iLen-1] != '/' ){
            // No trailing slash found, copy string and append one
            pResult = (char*)XP_ALLOC(iLen+1);
            if( !pResult ){
                return NULL;
            }
            XP_STRCPY(pResult, pBaseURL);
            pResult[iLen] = '/';
            pResult[iLen+1] = '\0';
        }
        XP_FREEIF(pBaseFilename);
    }
    if( !pResult ){
	    char * pBaseEnd = XP_STRRCHR(pBaseURL, '/');

        if( !pBaseEnd )
		    pBaseEnd = XP_STRCHR(pBaseURL, ':');
	    if( !pBaseEnd )
            return NULL;

        pBaseEnd++;
        char save_char = *pBaseEnd;
        *pBaseEnd = '\0';

        // Allocate result and copy the base part
        StrAllocCat(pResult, pBaseURL);

        // Restore character
        *pBaseEnd = save_char;
    }
    if( pResult && pURL ){
        // Get just the filename of source URL
	    char * pFilename = EDT_GetFilename(pURL, bMustHaveExtension);
        if( pFilename ){
            // Append the filename to the base
            StrAllocCat(pResult, pFilename);
            XP_FREE(pFilename);
        }
    }

    //Note: If no pURL, pBaseURL is returned stripped of its filename
    return pResult;        
}

char * EDT_GetDefaultPublishURL(MWContext * pMWContext, char **ppFilename, char **ppUserName, char **ppPassword)
{
    XP_ASSERT(pMWContext);
    char * pURL = LO_GetBaseURL(pMWContext);
    if( !pURL ){
        return NULL;
    }
    XP_Bool bLastPublishFailed = EDT_IsSameURL(CEditSaveObject::m_pFailedPublishUrl, pURL, NULL, NULL);

    if( ppFilename ){
        if( EDT_IS_NEW_DOCUMENT(pMWContext) ){
            // New page: empty filename
            *ppFilename = NULL;
        } else {
            // Extract current filename part
            *ppFilename = EDT_GetFilename(pURL, TRUE);

            // On win16 force to lower case.
#ifdef XP_WIN16
            char *pConvert = *ppFilename;
            while (pConvert && *pConvert) {
              *pConvert = XP_TO_LOWER(*pConvert);
              pConvert++;
            }
#endif
        }
    }
    // Default: Clear password
    if( ppPassword ){
        *ppPassword = NULL;
    }

    if( !bLastPublishFailed && !EDT_IS_NEW_DOCUMENT(pMWContext) ){
        int iType = NET_URL_Type(pURL);
        if( iType == FTP_TYPE_URL ||
            iType == HTTP_TYPE_URL ||
            iType == SECURE_HTTP_TYPE_URL ) {
            // We are editing a remote page, so try to return it to that location
            // (We don't know the password)
            // Truncate current URL at the filename
            return EDT_ReplaceFilename(pURL, NULL, TRUE);
        }
    }

    char *pPrefURL = NULL;
    XP_Bool bUseDefault = FALSE;

    if( bLastPublishFailed ){
        // The publish destination should be saved to this at start of publishing (thus may not be "correct")
        // The destination should also be saved to the first history pref (editor.publish_history_0) 
        //  but only if publishing succeeded
        PREF_CopyCharPref("editor.publish_last_loc", &pPrefURL);
    } else {
        // Use the last successfully-used location saved in preferences
        PREF_CopyCharPref("editor.publish_history_0", &pPrefURL);
    }
    // Be sure to check for empty pref string
    if( !pPrefURL || !*pPrefURL ){
        XP_FREEIF(pPrefURL);
        // No last-used location -- use the default location
		PREF_CopyCharPref("editor.publish_location", &pPrefURL);
        bUseDefault = TRUE;
    }   

    char * pLocation = 0;
    char * pUserName = ppUserName ? *ppUserName : 0;
    
    // Parse the preference string to extract Username
    //  (password is separate for security munging)
    NET_ParseUploadURL( pPrefURL, &pLocation, &pUserName, NULL );

	if (ppUserName)
		*ppUserName = pUserName;

    if( ppPassword ){
        // Get the corrsponding password saved
        char * pPassword = NULL;
        if( bLastPublishFailed ){
        } else if( bUseDefault ){
            PREF_CopyCharPref("editor.publish_password", &pPassword);
        } else {
            PREF_CopyCharPref("editor.publish_password_0", &pPassword);
        }

        if( pPassword && *pPassword ){
            *ppPassword = SECNAV_UnMungeString(pPassword);
            XP_FREE(pPassword);
        }
    }
    XP_FREEIF(pPrefURL);

    return pLocation;
}

//
//    Keep this localized here. These two functions are the only ones
//    that need to know this value.
//    NOTE: CALLER MUST FREE THE RETURNED STRINGS
#define MAX_PUBLISH_LOCATIONS 20

XP_Bool
EDT_GetPublishingHistory(unsigned n,
						 char** location_r,
						 char** username_r,
						 char** password_r)
{
	char  prefname[32];
	char* prefvalue = NULL;

	if (n >= MAX_PUBLISH_LOCATIONS)
		return FALSE;

	XP_SPRINTF(prefname, "editor.publish_history_%d", n);

	if( PREF_CopyCharPref(prefname, &prefvalue) == -1 ||
        prefvalue == NULL || prefvalue[0] == '\0')
		return FALSE;

	if (location_r != NULL)
        *location_r = NULL; /* else NET_ParseUploadURL() will try to free! */
	if (username_r != NULL)
		*username_r = NULL;

	XP_Bool return_value;
	return_value = NET_ParseUploadURL(prefvalue, location_r, username_r, NULL);
	XP_FREE(prefvalue); // done with this now.

    if (!return_value)
		return FALSE;

	if (password_r != NULL) {
		XP_SPRINTF(prefname, "editor.publish_password_%d", n);

		if (PREF_CopyCharPref(prefname, &prefvalue) != -1 && prefvalue && *prefvalue) {
			*password_r = SECNAV_UnMungeString(prefvalue);
		} else {
			*password_r = NULL;
		}
		if (prefvalue != NULL)
			XP_FREE(prefvalue);
	}
	return TRUE;
}

void
EDT_SyncPublishingHistory()
{
	char  prefname[32];
	char* prefvalue = NULL;
	char* pLastLoc = NULL;
	char* pLastPass = NULL;
	int   i;
	
	/*
	 *    Last location should be set when you start a publish.
	 *    Now copy this to the top (first) of the history.
     *    Abort if pref doesn't exist or is empty
	 */
	if( PREF_CopyCharPref("editor.publish_last_loc", &pLastLoc) == -1 ||
        !pLastLoc || !*pLastLoc )
        return;
	PREF_CopyCharPref("editor.publish_last_pass", &pLastPass);
	
    /*
	 *    First scan the list to find pref that matches new item
	 */
    for (i = 0; i < (MAX_PUBLISH_LOCATIONS - 1); i++) {
		XP_SPRINTF(prefname, "editor.publish_history_%d", i);
        if( PREF_CopyCharPref(prefname, &prefvalue) == -1 ||
            !prefvalue || !*prefvalue ||
			XP_STRCMP(prefvalue, pLastLoc) == 0) {
			//
			//    No history or this history matches the last publish location.
			//
            break;
        }
        XP_FREEIF(prefvalue);
    }
    XP_FREEIF(prefvalue);
	
	/*
	 *    Now shift everything up a slot.
	 */
	for (; i > 0; i--) {
		XP_SPRINTF(prefname, "editor.publish_history_%d", i-1);
		PREF_CopyCharPref(prefname, &prefvalue);
		XP_SPRINTF(prefname, "editor.publish_history_%d", i);
		PREF_SetCharPref(prefname, prefvalue);
        XP_FREEIF(prefvalue);
		
		XP_SPRINTF(prefname, "editor.publish_password_%d", i-1);
		PREF_CopyCharPref(prefname, &prefvalue);
		XP_SPRINTF(prefname, "editor.publish_password_%d", i);
		PREF_SetCharPref(prefname, prefvalue);
        XP_FREEIF(prefvalue);
	}
	
	/*
	 *    Now set the zero'th one.
	 */
	PREF_SetCharPref("editor.publish_history_0", pLastLoc);
	PREF_SetCharPref("editor.publish_password_0", pLastPass);
	
    XP_FREEIF(pLastLoc);
	XP_FREEIF(pLastPass);
}

//
// This was placed in EDT.H so front ends can use it as well
// #define MAX_EDIT_HISTORY_LOCATIONS 10 

// Cache the Edit History data
static char** ppEditHistoryURLs = NULL;
static char** ppEditHistoryTitles = NULL;

//    NOTE: Caller must NOT free the returned strings - they are locally cached
XP_Bool
EDT_GetEditHistory(MWContext * pMWContext, unsigned n,
		           char** ppUrl, char** ppTitle)
{
	char  prefname[32];
    unsigned i;
    
    // if context is NULL, bail!
    if( !pMWContext )
    	return FALSE;
    
    // Initialize list if not done so already
    if( !ppEditHistoryURLs )
        EDT_SyncEditHistory(pMWContext);

    // Check if current URL is matches the requested or more recent items in the list
    if( pMWContext )
    {
        History_entry * pEntry = SHIST_GetCurrent(&(pMWContext->hist));
        if( pEntry && pEntry->address )
        {
            for( i = 0; i <= n; i++ ){
	            XP_SPRINTF(prefname, "editor.url_history.URL_%d", i);
                if( ppEditHistoryURLs[i] && 
                    EDT_IsSameURL(pEntry->address, ppEditHistoryURLs[i], 0, 0) )
                {
                    // The current doc was found, so skip over it
                    n++;
                    break;
                }        
            }
        }
    }
    // Default in case we don't set the location
    if( ppUrl )
        *ppUrl = NULL;
	
    if( n >= MAX_EDIT_HISTORY_LOCATIONS )
		return FALSE;

    if( !ppEditHistoryURLs[n] )
		return FALSE;

    // Return URL and Title to caller
	if( ppUrl )
        *ppUrl = ppEditHistoryURLs[n];

    if( ppTitle )
        *ppTitle = ppEditHistoryTitles[n];

	return TRUE;
}

static char  pEmpty[] = ""; 

void
EDT_SyncEditHistory(MWContext * pMWContext)
{
	char  prefname[32];
	char* prefvalue = NULL;
	char* pTitle = pEmpty;
	int   i;
	
    if(!pMWContext)
        return;

    // Initialize the local arrays from the current preference list
    if( !ppEditHistoryURLs )
    {
        ppEditHistoryURLs = (char**)XP_ALLOC( MAX_EDIT_HISTORY_LOCATIONS * sizeof(char*) );
        if( !ppEditHistoryURLs )
            return;

        ppEditHistoryTitles = (char**)XP_ALLOC( MAX_EDIT_HISTORY_LOCATIONS * sizeof(char*) );
        if( !ppEditHistoryTitles )
        {
            XP_FREEIF(ppEditHistoryURLs);
            return;
        }

        for( i = 0; i < MAX_EDIT_HISTORY_LOCATIONS; i++ )
        {
    		XP_SPRINTF(prefname, "editor.url_history.URL_%d", i);
            if(PREF_CopyCharPref(prefname, &prefvalue) != -1 &&
               prefvalue && *prefvalue )
            {
                ppEditHistoryURLs[i] = prefvalue;
                prefvalue = NULL;

            } else {
                ppEditHistoryURLs[i] = NULL;
                XP_FREEIF(prefvalue);
            }

    		XP_SPRINTF(prefname, "editor.url_history.TITLE_%d", i);
            if(PREF_CopyCharPref(prefname, &prefvalue) != -1 &&
               prefvalue && *prefvalue )
            {
                ppEditHistoryTitles[i] = prefvalue;
                prefvalue = NULL;
            } else {
                ppEditHistoryTitles[i] = NULL;
                XP_FREEIF(prefvalue);
            }
        }
    }

    History_entry * pEntry = SHIST_GetCurrent(&(pMWContext->hist));
	if(pEntry )
    {
        // Must have an address
        if( !pEntry->address || !*pEntry->address )
            return;

        // Test for new document URL: file://Untitled and ignore it
        if( !XP_STRCMP(XP_GetString(XP_EDIT_NEW_DOC_NAME), pEntry->address) ||
            !XP_STRCMP(XP_GetString(XP_EDIT_NEW_DOC_URL), pEntry->address))
        {
            return;
        }

        if( pEntry->title && *pEntry->title ){
            pTitle = pEntry->title;
        }
    }

    /*
	 *    First scan the list to find pref URL that matches current page
	 */
    for (i = 0; i < (MAX_EDIT_HISTORY_LOCATIONS - 1); i++)
    {
		XP_SPRINTF(prefname, "editor.url_history.URL_%d", i);
        if( !ppEditHistoryURLs[i] ||
			EDT_IsSameURL(ppEditHistoryURLs[i], pEntry->address, 0, 0) ) {
			//
			//    No history or this history matches the current last-edited URL
			//
            break;
        }
    }
    // Now i = the index to last-edited URL or last possible URL in the list
    if( i == (MAX_EDIT_HISTORY_LOCATIONS-1) )
    {
        // We will be replacing the last item, so delete it
        //   so delete it
        XP_FREEIF(ppEditHistoryURLs[i]);
        XP_FREEIF(ppEditHistoryTitles[i]);
    }
	
	/*
	 *    Now shift everything one item.
	 */
	for (; i > 0; i--)
    {
        XP_SPRINTF(prefname, "editor.url_history.URL_%d", i);
        // Not sure if this can handle null string, so lets not try!
		PREF_SetCharPref(prefname, ppEditHistoryURLs[i-1] ? ppEditHistoryURLs[i-1] : pEmpty);
        
        ppEditHistoryURLs[i] = ppEditHistoryURLs[i-1];		

		XP_SPRINTF(prefname, "editor.url_history.TITLE_%d", i);
		PREF_SetCharPref(prefname, ppEditHistoryTitles[i-1] ? ppEditHistoryTitles[i-1] : pEmpty);

        ppEditHistoryTitles[i] = ppEditHistoryTitles[i-1];		
	}
	
	/*
	 *    Now set the zero'th one -- the current URL
	 */
	PREF_SetCharPref("editor.url_history.URL_0", pEntry->address);
	PREF_SetCharPref("editor.url_history.TITLE_0", pTitle);

    ppEditHistoryURLs[0] = XP_STRDUP(pEntry->address);		


    if( pTitle && *pTitle )
    {
        ppEditHistoryTitles[0] = XP_STRDUP(pTitle);
    } else {
        ppEditHistoryTitles[0] = NULL;
    }		
}

void edt_UpdateEditHistoryTitle(MWContext * pMWContext, char * pTitle)
{
    if( pMWContext )
    {
        History_entry * pEntry = SHIST_GetCurrent(&(pMWContext->hist));
	    if(pEntry && pEntry->address && !*pEntry->address)
        {
            for (int i = 0; i < MAX_EDIT_HISTORY_LOCATIONS; i++)
            {
            	char  prefname[32];
		        XP_SPRINTF(prefname, "editor.url_history.URL_%d", i);
                if( ppEditHistoryURLs[i] &&
			        EDT_IsSameURL(ppEditHistoryURLs[i], pEntry->address, 0, 0) )
                {
                    XP_FREEIF(ppEditHistoryTitles[i]);
                    if( pTitle )
                        ppEditHistoryTitles[i] = pTitle;

                    // Also update the preference value
	                char  prefname[32];
                    XP_SPRINTF(prefname, "editor.url_history.TITLE_%d", i);
                
                    // Not sure if this can handle null string, so lets not try!
		            PREF_SetCharPref(prefname, pTitle ? pTitle : pEmpty);
                    return;
                }
            }
        }
    }
}

char * EDT_GetPageTitleFromFilename(char * pFilename)
{
    char * pTitle = NULL;
    if( pFilename ){
        // Get Full "filename" and we don't need an extension
        pTitle = EDT_GetFilename(pFilename, FALSE);
    
        if( pTitle ){
            // Don't include extension
            char * period = XP_STRCHR(pTitle, '.');
	        if(period){
                // Truncate at the beginning of extension
		        *period = '\0';
            }
        }
    }
    return pTitle;
}

// True if both urls are the same, ignores any username/password
// information.  Does caseless comparison for file:// URLs 
// on windows and mac.
// url1 and url2 are relative to base1 and base2, respectively.
// If url1 or url2 is already absolute, base1 or base2 can 
// be passed in as NULL.
XP_Bool EDT_IsSameURL(char *url1,char *url2,char *base1,char *base2) {
  // Check for NULL or empty strings.
  if (!url1 || !url2 || !*url1 || !*url2) {
    return FALSE;
  }

  // Used passed in base URLs if provided.
  char *pAbs1 = NULL;
  char *pAbs2 = NULL;
  if (base1) {
    pAbs1 = NET_MakeAbsoluteURL(base1,url1);
  }
  if (base2) {
    pAbs2 = NET_MakeAbsoluteURL(base2,url2);
  }
  // Strip any username/password info from urls in making comparison if
  // they are http, https, or ftp.
  char *pUrl1 = edt_StripUsernamePassword(pAbs1 ? pAbs1 : url1);
  char *pUrl2 = edt_StripUsernamePassword(pAbs2 ? pAbs2 : url2);
  edt_StripAtHashOrQuestionMark(pUrl1);
  edt_StripAtHashOrQuestionMark(pUrl2);

  XP_Bool bRetVal = FALSE;
  if (pUrl1 && pUrl2) {
    char *pBaseUrl1 = NET_ParseURL(pUrl1, GET_PATH_PART);
    char *pBaseUrl2 = NET_ParseURL(pUrl2, GET_PATH_PART);
#if defined(XP_WIN) || defined(XP_MAC) || defined(XP_OS2)
    // Strip off Target (named Anchors)
    // Local file URLs are equivalent if they differ only 
    // by case.
    if (NET_URL_Type(pUrl1) == FILE_TYPE_URL) {
      bRetVal = (XP_STRCASECMP( pUrl1, pUrl2 ) == 0);
    }
    else {
      bRetVal = (XP_STRCMP( pUrl1, pUrl2 ) == 0 );
    }
#else
    // Use regular strcmp.
    bRetVal = (XP_STRCMP( pUrl1, pUrl2 ) == 0 );
#endif
  }
  
  XP_FREEIF(pUrl1);
  XP_FREEIF(pUrl2);
  XP_FREEIF(pAbs1);
  XP_FREEIF(pAbs2);
  return bRetVal;
}

// Replace the current selection with supplied text
void EDT_ReplaceText(MWContext *pContext, char * pReplaceText, XP_Bool bReplaceAll,
					char *pTextToLookFor, XP_Bool bCaseless, XP_Bool bBackward, XP_Bool bDoWrap)
{
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->ReplaceLoop( pReplaceText, bReplaceAll, 
                              pTextToLookFor, bCaseless, bBackward, bDoWrap );
}

// Block of functions moved from EDITOR.CPP for Win16 Build
// NOT USED???
#ifdef FIND_REPLACE

XP_Bool EDT_FindAndReplace(MWContext *pContext,
                EDT_FindAndReplaceData *pData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->FindAndReplace( pData );

}
#endif  // FIND_REPLACE

// Table Sizing, Selection, Add Row/Col interface
ED_HitType EDT_GetTableHitRegion(MWContext *pContext, int32 xVal, int32 yVal, 
                                 LO_Element **ppElement, XP_Bool bModifierKeyPressed)
{
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) ED_HIT_NONE;
    return pEditBuffer->GetTableHitRegion(xVal, yVal, ppElement, bModifierKeyPressed);
}

ED_HitType EDT_GetSelectedTableElement(MWContext *pContext, LO_Element **ppElement){
    return EDT_GetTableHitRegion(pContext, 0, 0, ppElement, FALSE);
}

// Object Sizer
ED_SizeStyle EDT_CanSizeObject(MWContext *pContext, LO_Element *pElement,
                      int32 xVal, int32 yVal){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) 0;
    return pEditBuffer->CanSizeObject(pElement, xVal, yVal);
}

XP_Bool EDT_IsSizing(MWContext *pContext){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->IsSizing();
}

ED_SizeStyle EDT_StartSizing(MWContext *pContext, LO_Element *pElement, int32 xVal, int32 yVal,
			     XP_Bool bLockAspect, XP_Rect *pRect){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) 0;
    return pEditBuffer->StartSizing(pElement, xVal, yVal, bLockAspect, pRect);
}

XP_Bool EDT_GetSizingRect(MWContext *pContext, int32 xVal, int32 yVal,
					   XP_Bool bLockAspect, XP_Rect *pRect){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->GetSizingRect(xVal, yVal, bLockAspect, pRect);
}

void EDT_EndSizing(MWContext *pContext){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->EndSizing();
}

void EDT_CancelSizing(MWContext *pContext){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->CancelSizing();
}

ED_BufferOffset EDT_GetInsertPointOffset( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) 0;
    CPersistentEditInsertPoint p;
    pEditBuffer->GetInsertPoint( p );
    return (ED_BufferOffset) p.m_index;
}

void EDT_SetInsertPointToOffset( MWContext *pContext, ED_BufferOffset i,
        int32 iLen ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    CPersistentEditInsertPoint p((ElementIndex)i);

    if( iLen != 0 ){
        CPersistentEditInsertPoint p1((ElementIndex)i+iLen);
        CPersistentEditSelection perSel;
        perSel = CPersistentEditSelection( p, p1 );
        CEditSelection sel = pEditBuffer->PersistentToEphemeral( perSel );
        pEditBuffer->SetSelection( sel );
    }
    else {
        pEditBuffer->SetInsertPoint( p  );
    }
}

void EDT_OffsetToLayoutElement( MWContext *pContext, ED_BufferOffset i,
		LO_Element * *element, int32 *caretPos){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    CPersistentEditInsertPoint p((ElementIndex)i);

	CEditInsertPoint eP = pEditBuffer->PersistentToEphemeral(p);
	*element = eP.m_pElement->GetLayoutElement();
	*caretPos = eP.m_iPos;
}

ED_BufferOffset EDT_LayoutElementToOffset( MWContext *pContext,
		LO_Element *element, int32 caretPos){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) -1;
    
    if (element->type != LO_TEXT)
    	return -1;
    	
    CEditInsertPoint insertPoint(element->lo_text.edit_element, caretPos);
    
	CPersistentEditInsertPoint p = pEditBuffer->EphemeralToPersistent(insertPoint);
	
	return p.m_index;
}

LO_TextBlock* EDT_GetTextBlock(MWContext *, LO_Element *le)
{
	LO_TextBlock *block;
    CEditTextElement *pText;
    
    block = NULL;
    
    if( ( le->type == LO_TEXT ) && ( le->lo_any.edit_element != NULL )
			&& le->lo_any.edit_element->IsA( P_TEXT ) ){
        pText = le->lo_any.edit_element->Text();
        block = pText->GetTextBlock();
    }
	
	return block;
}

void EDT_GetSelectionOffsets(MWContext *pContext, ED_BufferOffset* pStart, ED_BufferOffset* pEnd){
    if ( ! pStart || !pEnd ) {
        XP_ASSERT(FALSE);
        return;
    }
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    CPersistentEditSelection s;
    pEditBuffer->GetSelection( s );
    *pStart = (ED_BufferOffset) s.m_start.m_index;
    *pEnd = (ED_BufferOffset) s.m_end.m_index;
}

XP_Bool EDT_SelectFirstMisspelledWord( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->FindNextMisspelledWord( TRUE, TRUE, 0 );
}

XP_Bool EDT_SelectNextMisspelledWord( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->FindNextMisspelledWord( FALSE, TRUE, 0 );
}

void EDT_ReplaceMisspelledWord( MWContext *pContext, char* pOldWord, 
        char*pNewWord, XP_Bool bAll ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->BeginBatchChanges(kPasteTextCommandID);
    pEditBuffer->IterateMisspelledWords( CEditBuffer::EMSW_REPLACE, 
            pOldWord, pNewWord, bAll );
    pEditBuffer->EndBatchChanges();
}

void EDT_IgnoreMisspelledWord( MWContext *pContext, char* pOldWord, 
        XP_Bool bAll ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->IterateMisspelledWords( CEditBuffer::EMSW_IGNORE, 
            pOldWord, 0, bAll );
}


XP_HUGE_CHAR_PTR EDT_GetPositionalText( MWContext* pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) 0;
    return pEditBuffer->GetPositionalText();
}

void EDT_SetCharacterDataAtOffset( MWContext *pContext, EDT_CharacterData *pData,
        ED_BufferOffset iBufOffset, int32 iLen ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SetCharacterDataAtOffset(pData, iBufOffset, iLen );
}

void EDT_SetRefresh( MWContext* pContext, XP_Bool bRefreshOn ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SetRefresh( bRefreshOn );
}


// Warning this deletes (and recreates) the CEditBuffer.
XP_Bool EDT_SetEncoding(MWContext* pContext, int16 csid){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    XP_Bool bDoIt = TRUE;
//  if ( pEditBuffer->HasEncoding() ) {
    char* pMessage = XP_GetString(XP_EDT_I18N_HAS_CHARSET);
    if ( pMessage ) {
        bDoIt = FE_Confirm(pContext, pMessage);
    }
    else {
        XP_ASSERT(0);
    }

    if ( bDoIt ) {
        pEditBuffer->ChangeEncoding(csid);
    }
    return bDoIt;
}
// End of Block of functions moved from EDITOR.CPP for Win16 Build

/*
 * Extract the Extra HTML string from the ED_Element pointer in an image struct
 * (ED_Element is a void* to external users)
 * Caller must XP_FREE result
 */
char * EDT_GetExtraHTML_FromImage(LO_ImageStruct *pImage){
    if( pImage ){
        if( pImage->edit_element == 0 ){
            // We don't have an edit element - must be from Navigator,
            // so we have to pick out stuff that would end up in CEditImageElement's extra data
            // BUT the original PA_TAG data is no longer around.
            // The only item we will grab is the USEMAP data
            // TODO: Get other associated data such as JavaScript?
            //       To do that, we need a new function to reconstruct a tag from LO_ImageStruct

            if ( pImage->image_attr && pImage->image_attr->usemap_name ){
                char * pRet = 0;
                pRet = PR_sprintf_append( pRet, "USEMAP=");
                pRet = PR_sprintf_append( pRet, pImage->image_attr->usemap_name);
                return pRet;
            }
        }else if( pImage->edit_element->IsImage() ){
            EDT_ImageData * pImageData = ((CEditImageElement*)pImage->edit_element)->GetImageData();
            if( pImageData->pExtra && *pImageData->pExtra ){
                return XP_STRDUP(pImageData->pExtra);
            }
            EDT_FreeImageData(pImageData);
        }
    }
    return NULL;
}

PRBool EDT_EncryptState( MWContext *pContext ){
    CEditBuffer *pEditBuffer = LO_GetEDBuffer( pContext );

    if (pEditBuffer == NULL) return PR_FALSE;

    return pEditBuffer->m_bEncrypt;
}

void EDT_EncryptToggle( MWContext *pContext ){
    CEditBuffer *pEditBuffer = LO_GetEDBuffer( pContext );

    if (pEditBuffer == NULL) return;

     pEditBuffer->m_bEncrypt = pEditBuffer->m_bEncrypt ? PR_FALSE :  PR_TRUE;
}

void EDT_EncryptSet( MWContext *pContext ){
    CEditBuffer *pEditBuffer = LO_GetEDBuffer( pContext );

    if (pEditBuffer == NULL) return;

    pEditBuffer->m_bEncrypt = PR_TRUE;
}

void EDT_EncryptReset( MWContext *pContext ){
    CEditBuffer *pEditBuffer = LO_GetEDBuffer( pContext );

    if (pEditBuffer == NULL) return;

    pEditBuffer->m_bEncrypt = PR_FALSE;
}

#endif // EDITOR
