/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

#include <stdio.h>
#include <string.h>

#ifndef HAVE_LIBNLS

#ifdef __cplusplus
extern "C" {
#endif

extern char	*ldaptool_charset;
char		*ldaptool_convdir = NULL;
static		int charsetset = 0;
char		*ldaptool_local2UTF8( const char *src );

char *
ldaptool_local2UTF8( const char *src )
{
    char *utf8;

    charsetset = 0;

    if (src == NULL)
    {
	return NULL;
    }
    utf8 = strdup(src);

    return ( utf8 );
}

#else /* HAVE_LIBNLS */

#define NSPR20

#ifdef XP_WIN32
#define  VC_EXTRALEAN
#include <afxwin.h>
#include <winnls.h>
#endif

extern char *ldaptool_charset;
static int charsetset = 0;

extern "C" {
char *ldaptool_convdir = NULL;
char *ldaptool_local2UTF8( const char * );
}

#ifndef XP_WIN32
char * GetNormalizedLocaleName(void);

#include "unistring.h"
#include "nlsenc.h"

extern NLS_StaticConverterRegistry _STATICLINK_NSJPN_;
extern NLS_StaticConverterRegistry _STATICLINK_NSCCK_;
extern NLS_StaticConverterRegistry _STATICLINK_NSSB_;

char *
GetNormalizedLocaleName(void)
{
#ifdef _HPUX_SOURCE
 
    int    len;
    char    *locale;
 
    locale = setlocale(LC_CTYPE, "");
    if (locale && *locale) {
        len = strlen(locale);
    } else {
        locale = "C";
        len = 1;
    }
 
    if ((!strncmp(locale, "/\x03:", 3)) &&
        (!strcmp(&locale[len - 2], ";/"))) {
        locale += 3;
        len -= 5;
    }
 
    locale = strdup(locale);
    if (locale) {
        locale[len] = 0;
    }
 
    return locale;
 
#else
 
    char    *locale;
 
    locale = setlocale(LC_CTYPE, "");
    if (locale && *locale) {
        return strdup(locale);
    }
 
    return strdup("C");
 
#endif
}

#if defined(IRIX)
const char *CHARCONVTABLE[] =
{
"! This table maps the host's locale names to LIBNLS charsets",
"!",
"C:             ISO_8859-1:1987",
"cs:            ISO_8859-2:1987",
"da:            ISO_8859-1:1987",
"de:            ISO_8859-1:1987",
"de_AT:         ISO_8859-1:1987",
"de_CH:         ISO_8859-1:1987",
"en:            ISO_8859-1:1987",
"en_AU:         ISO_8859-1:1987",
"en_CA:         ISO_8859-1:1987",
"en_TH:         ISO_8859-1:1987",
"en_US:         ISO_8859-1:1987",
"es:            ISO_8859-1:1987",
"fi:            ISO_8859-1:1987",
"fr:            ISO_8859-1:1987",
"fr_BE:         ISO_8859-1:1987",
"fr_CA:         ISO_8859-1:1987",
"fr_CH:         ISO_8859-1:1987",
"is:            ISO_8859-1:1987",
"it:            ISO_8859-1:1987",
"it_CH:         ISO_8859-1:1987",
"ja_JP.EUC:     Extended_UNIX_Code_Packed_Format_for_Japanese",
"ko_KR.euc:     EUC-KR",
"nl:            ISO_8859-1:1987",
"nl_BE:         ISO_8859-1:1987",
"no:            ISO_8859-1:1987",
"pl:            ISO_8859-2:1987",
"pt:            ISO_8859-1:1987",
"sh:            ISO_8859-2:1987",
"sk:            ISO_8859-2:1987",
"sv:            ISO_8859-1:1987",
"zh_CN.ugb:     GB2312",
"zh_TW.ucns:    cns11643_1",
NULL
};
#elif defined(SOLARIS)
const char *CHARCONVTABLE[] =
{
"! This table maps the host's locale names to LIBNLS charsets",
"!",
"C:             ISO_8859-1:1987",
"ja:            Extended_UNIX_Code_Packed_Format_for_Japanese",
"ja_JP.EUC:     Extended_UNIX_Code_Packed_Format_for_Japanese",
"ja_JP.PCK:     Shift_JIS",
"en:		ISO_8859-1:1987",
"en_AU:		ISO_8859-1:1987",
"en_CA:		ISO_8859-1:1987",
"en_UK:		ISO_8859-1:1987",
"en_US:		ISO_8859-1:1987",
"es:		ISO_8859-1:1987",
"es_AR:		ISO_8859-1:1987",
"es_BO:		ISO_8859-1:1987",
"es_CL:		ISO_8859-1:1987",
"es_CO:		ISO_8859-1:1987",
"es_CR:		ISO_8859-1:1987",
"es_EC:		ISO_8859-1:1987",
"es_GT:		ISO_8859-1:1987",
"es_MX:		ISO_8859-1:1987",
"es_NI:		ISO_8859-1:1987",
"es_PA:		ISO_8859-1:1987",
"es_PE:		ISO_8859-1:1987",
"es_PY:		ISO_8859-1:1987",
"es_SV:		ISO_8859-1:1987",
"es_UY:		ISO_8859-1:1987",
"es_VE:		ISO_8859-1:1987",
"fr:		ISO_8859-1:1987",
"fr_BE:		ISO_8859-1:1987",
"fr_CA:		ISO_8859-1:1987",
"fr_CH:		ISO_8859-1:1987",
"de:		ISO_8859-1:1987",
"de_AT:		ISO_8859-1:1987",
"de_CH:		ISO_8859-1:1987",
"nl:		ISO_8859-1:1987",
"nl_BE:		ISO_8859-1:1987",
"it:		ISO_8859-1:1987",
"sv:		ISO_8859-1:1987",
"no:		ISO_8859-1:1987",
"da:		ISO_8859-1:1987",
"iso_8859_1:    ISO_8859-1:1987",
"japanese:      Extended_UNIX_Code_Packed_Format_for_Japanese",
"ko:            EUC-KR",
"zh:            GB2312",
"zh_TW:         cns11643_1",
NULL
};
#elif defined(OSF1)
const char *CHARCONVTABLE[] =
{
"! This table maps the host's locale names to LIBNLS charsets",
"!",
"C:                     ISO_8859-1:1987",
"cs_CZ.ISO8859-2:       ISO_8859-2:1987",
"cs_CZ:                 ISO_8859-2:1987",
"da_DK.ISO8859-1:       ISO_8859-1:1987",
"de_CH.ISO8859-1:       ISO_8859-1:1987",
"de_DE.ISO8859-1:       ISO_8859-1:1987",
"en_GB.ISO8859-1:       ISO_8859-1:1987",
"en_US.ISO8859-1:       ISO_8859-1:1987",
"es_ES.ISO8859-1:       ISO_8859-1:1987",
"fi_FI.ISO8859-1:       ISO_8859-1:1987",
"fr_BE.ISO8859-1:       ISO_8859-1:1987",
"fr_CA.ISO8859-1:       ISO_8859-1:1987",
"fr_CH.ISO8859-1:       ISO_8859-1:1987",
"fr_FR.ISO8859-1:       ISO_8859-1:1987",
"hu_HU.ISO8859-2:       ISO_8859-2:1987",
"hu_HU:                 ISO_8859-2:1987",
"is_IS.ISO8859-1:       ISO_8859-1:1987",
"it_IT.ISO8859-1:       ISO_8859-1:1987",
"ja_JP.SJIS:            Shift_JIS",
"ja_JP.eucJP:           Extended_UNIX_Code_Packed_Format_for_Japanese",
"ja_JP:                 Extended_UNIX_Code_Packed_Format_for_Japanese",
"ko_KR.eucKR:           EUC-KR",
"ko_KR:                 EUC-KR",
"nl_BE.ISO8859-1:       ISO_8859-1:1987",
"nl_NL.ISO8859-1:       ISO_8859-1:1987",
"no_NO.ISO8859-1:       ISO_8859-1:1987",
"pl_PL.ISO8859-2:       ISO_8859-2:1987",
"pl_PL:                 ISO_8859-2:1987",
"pt_PT.ISO8859-1:       ISO_8859-1:1987",
"sk_SK.ISO8859-2:       ISO_8859-2:1987",
"sk_SK:                 ISO_8859-2:1987",
"sv_SE.ISO8859-1:       ISO_8859-1:1987",
"zh_CN:                 GB2312",
"zh_HK.big5:            Big5",
"zh_HK.eucTW:           cns11643_1",
"zh_TW.big5:            Big5",
"zh_TW.big5@chuyin:     Big5",
"zh_TW.big5@radical:    Big5",
"zh_TW.big5@stroke:     Big5",
"zh_TW.eucTW:           cns11643_1",
"zh_TW.eucTW@chuyin:    cns11643_1",
"zh_TW.eucTW@radical:   cns11643_1",
"zh_TW.eucTW@stroke:    cns11643_1",
"zh_TW:                 cns11643_1",
NULL
};
#elif defined(HPUX)
const char *CHARCONVTABLE[] =
{
"! This table maps the host's locale names to LIBNLS charsets",
"!",
"C:			ISO_8859-1:1987",
"ja_JP:			Extended_UNIX_Code_Packed_Format_for_Japanese",
"ja_JP.SJIS:		Shift_JIS",
"ja_JP.eucJP:		Extended_UNIX_Code_Packed_Format_for_Japanese",
"es_ES:			ISO_8859-1:1987",
"es_ES.iso88591:	ISO_8859-1:1987",
"sv_SE:			ISO_8859-1:1987",
"sv_SE.iso88591:	ISO_8859-1:1987",
"da_DK:			ISO_8859-1:1987",
"da_DK.iso88591:	ISO_8859-1:1987",
"nl_NL:			ISO_8859-1:1987",
"nl_NL.iso88591:	ISO_8859-1:1987",
"en:			ISO_8859-1:1987",
"en_GB:			ISO_8859-1:1987",
"en_GB.iso88591:	ISO_8859-1:1987",
"en_US:			ISO_8859-1:1987",
"en_US.iso88591:	ISO_8859-1:1987",
"fi_FI:			ISO_8859-1:1987",
"fi_FI.iso88591:	ISO_8859-1:1987",
"fr_CA:			ISO_8859-1:1987",
"fr_CA.iso88591:	ISO_8859-1:1987",
"fr_FR:			ISO_8859-1:1987",
"fr_FR.iso88591:	ISO_8859-1:1987",
"de_DE:			ISO_8859-1:1987",
"de_DE.iso88591:	ISO_8859-1:1987",
"is_IS:			ISO_8859-1:1987",
"is_IS.iso88591:	ISO_8859-1:1987",
"it_IT:			ISO_8859-1:1987",
"it_IT.iso88591:	ISO_8859-1:1987",
"no_NO:			ISO_8859-1:1987",
"no_NO.iso88591:	ISO_8859-1:1987",
"pt_PT:			ISO_8859-1:1987",
"pt_PT.iso88591:	ISO_8859-1:1987",
"hu_HU:			ISO_8859-2:1987",
"hu_HU.iso88592:	ISO_8859-2:1987",
"cs_CZ:			ISO_8859-2:1987",
"cs_CZ.iso88592:	ISO_8859-2:1987",
"pl_PL:			ISO_8859-2:1987",
"pl_PL.iso88592:	ISO_8859-2:1987",
"ro_RO:			ISO_8859-2:1987",
"ro_RO.iso88592:	ISO_8859-2:1987",
"hr_HR:			ISO_8859-2:1987",
"hr_HR.iso88592:	ISO_8859-2:1987",
"sk_SK:			ISO_8859-2:1987",
"sk_SK.iso88592:	ISO_8859-2:1987",
"sl_SI:			ISO_8859-2:1987",
"sl_SI.iso88592:	ISO_8859-2:1987",
"american.iso88591:     ISO_8859-1:1987",
"bulgarian:             ISO_8859-2:1987",
"c-french.iso88591:     ISO_8859-1:1987",
"chinese-s:             GB2312",
"chinese-t.big5:                Big5",
"czech:                 ISO_8859-2:1987",
"danish.iso88591:       ISO_8859-1:1987",
"dutch.iso88591:                ISO_8859-1:1987",
"english.iso88591:      ISO_8859-1:1987",
"finnish.iso88591:      ISO_8859-1:1987",
"french.iso88591:       ISO_8859-1:1987",
"german.iso88591:       ISO_8859-1:1987",
"hungarian:             ISO_8859-2:1987",
"icelandic.iso88591:    ISO_8859-1:1987",
"italian.iso88591:      ISO_8859-1:1987",
"japanese.euc:          Extended_UNIX_Code_Packed_Format_for_Japanese",
"japanese:              Shift_JIS",
"katakana:              Shift_JIS",
"korean:                        EUC-KR",
"norwegian.iso88591:    ISO_8859-1:1987",
"polish:                        ISO_8859-2:1987",
"portuguese.iso88591:   ISO_8859-1:1987",
"rumanian:              ISO_8859-2:1987",
"serbocroatian:         ISO_8859-2:1987",
"slovene:               ISO_8859-2:1987",
"spanish.iso88591:      ISO_8859-1:1987",
"swedish.iso88591:      ISO_8859-1:1987",
NULL
};
#elif defined(AIX)
const char *CHARCONVTABLE[] =
{
"! This table maps the host's locale names to LIBNLS charsets",
"!",
"C:                     ISO_8859-1:1987",
"En_JP.IBM-932:         Shift_JIS",
"En_JP:                 Shift_JIS",
"Ja_JP.IBM-932:         Shift_JIS",
"Ja_JP:                 Shift_JIS",
"da_DK.ISO8859-1:       ISO_8859-1:1987",
"da_DK:                 ISO_8859-1:1987",
"de_CH.ISO8859-1:       ISO_8859-1:1987",
"de_CH:                 ISO_8859-1:1987",
"de_DE.ISO8859-1:       ISO_8859-1:1987",
"de_DE:                 ISO_8859-1:1987",
"en_GB.ISO8859-1:       ISO_8859-1:1987",
"en_GB:                 ISO_8859-1:1987",
"en_JP.IBM-eucJP:       Extended_UNIX_Code_Packed_Format_for_Japanese",
"en_JP:                 Extended_UNIX_Code_Packed_Format_for_Japanese",
"en_KR.IBM-eucKR:       EUC-KR",
"en_KR:                 EUC-KR",
"en_TW.IBM-eucTW:       cns11643_1",
"en_TW:                 cns11643_1",
"en_US.ISO8859-1:       ISO_8859-1:1987",
"en_US:                 ISO_8859-1:1987",
"es_ES.ISO8859-1:       ISO_8859-1:1987",
"es_ES:                 ISO_8859-1:1987",
"fi_FI.ISO8859-1:       ISO_8859-1:1987",
"fi_FI:                 ISO_8859-1:1987",
"fr_BE.ISO8859-1:       ISO_8859-1:1987",
"fr_BE:                 ISO_8859-1:1987",
"fr_CA.ISO8859-1:       ISO_8859-1:1987",
"fr_CA:                 ISO_8859-1:1987",
"fr_CH.ISO8859-1:       ISO_8859-1:1987",
"fr_CH:                 ISO_8859-1:1987",
"fr_FR.ISO8859-1:       ISO_8859-1:1987",
"fr_FR:                 ISO_8859-1:1987",
"is_IS.ISO8859-1:       ISO_8859-1:1987",
"is_IS:                 ISO_8859-1:1987",
"it_IT.ISO8859-1:       ISO_8859-1:1987",
"it_IT:                 ISO_8859-1:1987",
"ja_JP.IBM-eucJP:       Extended_UNIX_Code_Packed_Format_for_Japanese",
"ja_JP:                 Extended_UNIX_Code_Packed_Format_for_Japanese",
"ko_KR.IBM-eucKR:       EUC-KR",
"ko_KR:                 EUC-KR",
"nl_BE.ISO8859-1:       ISO_8859-1:1987",
"nl_BE:                 ISO_8859-1:1987",
"nl_NL.ISO8859-1:       ISO_8859-1:1987",
"nl_NL:                 ISO_8859-1:1987",
"no_NO.ISO8859-1:       ISO_8859-1:1987",
"no_NO:                 ISO_8859-1:1987",
"pt_PT.ISO8859-1:       ISO_8859-1:1987",
"pt_PT:                 ISO_8859-1:1987",
"sv_SE.ISO8859-1:       ISO_8859-1:1987",
"sv_SE:                 ISO_8859-1:1987",
"zh_TW.IBM-eucTW:       cns11643_1",
"zh_TW:                 cns11643_1",
NULL
};
#else   // sunos by default
const char *CHARCONVTABLE[] =
{
"! This table maps the host's locale names to LIBNLS charsets",
"!",
"C:             ISO_8859-1:1987",
"de:            ISO_8859-1:1987",
"en_US:         ISO_8859-1:1987",
"es:            ISO_8859-1:1987",
"fr:            ISO_8859-1:1987",
"iso_8859_1:    ISO_8859-1:1987",
"it:            ISO_8859-1:1987",
"ja:            Extended_UNIX_Code_Packed_Format_for_Japanese",
"ja_JP.EUC:     Extended_UNIX_Code_Packed_Format_for_Japanese",
"japanese:      Extended_UNIX_Code_Packed_Format_for_Japanese",
"ko:            EUC-KR",
"sv:            ISO_8859-1:1987",
"zh:            GB2312",
"zh_TW:         cns11643_1",
NULL
};
#endif
 
#define BSZ     256
 
char *
GetCharsetFromLocale(char *locale)
{
    char *tmpcharset = NULL;
    char buf[BSZ];
    char *p;
    const char *line;
    int i=0;
 
    line = CHARCONVTABLE[i];
    while (line != NULL)
    {
       if (*line == 0)
       { 
          break;
       } 
 
       strcpy(buf, line);
       line = CHARCONVTABLE[++i];
 
       if (strlen(buf) == 0 || buf[0] == '!')
       { 
          continue;
       } 
       p = strchr(buf, ':');
       if (p == NULL)
       { 
          tmpcharset = NULL;
          break;
       } 
       *p = 0;
       if (strcmp(buf, locale) == 0) {
          while (*++p == ' ' || *p == '\t')
             ;
          if (isalpha(*p)) {
             tmpcharset = strdup(p);
          } else
             tmpcharset = NULL;
 
          break;
       }
    }
    return tmpcharset;
}
 
#endif /* Not defined XP_WIN32 */

#ifdef XP_WIN32
char *_convertor(const char *instr, int bFromUTF8)
{
    char  *outstr = NULL;
    int    inlen, wclen, outlen;
    LPWSTR wcstr;
         
    if (instr == NULL)
            return NULL;

    if ((inlen = strlen(instr)) <= 0)
            return NULL;
 
    /* output never becomes longer than input,
    ** thus we don't have to ask for the length
    */
    wcstr = (LPWSTR) malloc( sizeof( WCHAR ) * (inlen+1) );
    if (!wcstr)
        return NULL;
 
    wclen = MultiByteToWideChar(bFromUTF8 ? CP_UTF8 : CP_ACP, 0, instr,
                                 inlen, wcstr, inlen);
    outlen = WideCharToMultiByte(bFromUTF8 ? CP_ACP : CP_UTF8, 0, wcstr,
                                  wclen, NULL, 0, NULL, NULL);
 
    if (outlen > 0) {
        outstr = (char *) malloc(outlen + 2);
        outlen = WideCharToMultiByte(bFromUTF8 ? CP_ACP : CP_UTF8, 0, wcstr,
                                      wclen, outstr, outlen, NULL, NULL);
        if (outlen > 0)
            *(outstr+outlen) = _T('\0');
        else
            return NULL;
    }
    free( wcstr );
    return outstr;
}
#endif

char *
ldaptool_local2UTF8( const char *src )
{
    char *utf8;
#ifndef XP_WIN32
    char *locale, *newcharset;
    size_t outLen, resultLen;
    NLS_ErrorCode err;
 
    if (src == NULL)
    {
      return NULL;
    }
    else if (*src == 0)
    {
        utf8 = strdup(src);
        return utf8;
    }
 
    if( (ldaptool_charset != NULL) && (!strcmp( ldaptool_charset, "" ))
	    && (!charsetset) )
    {
        locale = GetNormalizedLocaleName();
        ldaptool_charset = GetCharsetFromLocale(locale);
        free( locale );
        charsetset = 1;
    }
    else
    if( (ldaptool_charset != NULL) && strcmp( ldaptool_charset, "" )
	    && (!charsetset) )
    {
        newcharset = GetCharsetFromLocale( ldaptool_charset );
        free( ldaptool_charset );
        ldaptool_charset = newcharset;
        charsetset = 1;
    }

    if (ldaptool_charset == NULL) {
        return strdup(src);
    }

    if (NLS_EncInitialize(NULL, ldaptool_convdir) != NLS_SUCCESS ||
	NLS_RegisterStaticLibrary(_STATICLINK_NSJPN_) != NLS_SUCCESS ||
	NLS_RegisterStaticLibrary(_STATICLINK_NSCCK_) != NLS_SUCCESS ||
	NLS_RegisterStaticLibrary(_STATICLINK_NSSB_) != NLS_SUCCESS) {
        return strdup(src);
    }

    outLen = NLS_GetResultBufferSize( (byte *) src,
                                      strlen( src ) * sizeof(char), 
                                      ldaptool_charset,
                                      NLS_ENCODING_UTF_8 ); 

    utf8 =  (char *) malloc( outLen/sizeof(UniChar) ); 
    if( utf8 == NULL )
       return strdup(src);

    err = NLS_ConvertBuffer( ldaptool_charset,
                             NLS_ENCODING_UTF_8, 
                             (byte*)src,
                             strlen(src) * sizeof(char),
                             (byte*)utf8,
                             outLen, 
                             &resultLen ); 

    NLS_EncTerminate();
 
#else
    utf8 = _convertor(src, FALSE);
    if( utf8 == NULL )
        utf8 = strdup(src);
#endif
 
    return utf8;
}
#endif /* HAVE_LIBNLS */

#ifndef HAVE_LIBNLS
#ifdef __cplusplus
}
#endif
#endif

