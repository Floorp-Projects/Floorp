/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * locale.c
 * --------
 * Implement FE functions to support xp_locale and other locale stuff
 */


#include <xplocale.h>

#include "xfe.h"
#include "felocale.h"
#include "csid.h"


int16 fe_LocaleCharSetID = CS_LATIN1;

char fe_LocaleCharSetName[128] = { 0 };

int (*fe_collation_func)(const char *, const char *) = strcoll;


void
fe_InitCollation(void)
{
	/*
	 * Check to see if strcoll() is broken
	 */
	if
	(
		((strcoll("A", "B") < 0) && (strcoll("a", "B") >= 0)) ||

		/*
		 * Unlikely, but just in case...
		 */
		((strcoll("A", "B") > 0) && (strcoll("a", "B") <= 0)) ||
		(strcoll("A", "B") == 0)
	)
	{
		/*
		 * strcoll() is broken, so we use our own routine
		 */
		fe_collation_func = strcasecomp;
	}
}


int
FE_StrColl(const char *s1, const char *s2)
{
	return (*fe_collation_func)(s1, s2);
}


size_t FE_StrfTime(MWContext *context, char *result, size_t maxsize,
	int format, const struct tm *timeptr)

{
	char	*fmt;

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


char *
fe_GetNormalizedLocaleName(void)
{

#ifdef _HPUX_SOURCE

	int	len;
	char	*locale;

	locale = setlocale(LC_CTYPE, NULL);
	if (locale && *locale)
	{
		len = strlen(locale);
	}
	else
	{
		locale = "C";
		len = 1;
	}

	if
	(
		(!strncmp(locale, "/\x03:", 3)) &&
		(!strcmp(&locale[len - 2], ";/"))
	)
	{
		locale += 3;
		len -= 5;
	}

	locale = strdup(locale);
	if (locale)
	{
		locale[len] = 0;
	}

	return locale;

#else

	char	*locale;

	locale = setlocale(LC_CTYPE, NULL);
	if (locale && *locale)
	{
		return strdup(locale);
	}

	return strdup("C");

#endif

}


unsigned char *
fe_ConvertToLocaleEncoding(int16 charset, unsigned char *str)
{
	CCCDataObject	obj;
	unsigned char	*le_string; /* Locally Encoded STRING */

	if ((charset == fe_LocaleCharSetID) || (!str) || (!*str))
	{
		return str;
	}

	if (IS_UTF8_CSID(charset)) {
		int32 le_len;
		uint16 *ucs2_chars;
		int32 num_ucs2_chars;

		/*
		 * Convert utf8 to ucs2
		 */
		ucs2_chars = INTL_UTF8ToUCS2(str, &num_ucs2_chars);

		/*
		 * Convert ucs2 to local encoding
		 */
		le_len = INTL_UnicodeToStrLen(fe_LocaleCharSetID, ucs2_chars, 
															num_ucs2_chars);
		le_string = (unsigned char *)XP_ALLOC(le_len+1);
		INTL_UnicodeToStr(fe_LocaleCharSetID, ucs2_chars, num_ucs2_chars,
					le_string, le_len);
		le_string[le_len] = '\0'; /* null terminate */
		XP_FREE(ucs2_chars);
		return le_string;
	}

	obj = INTL_CreateCharCodeConverter();
	if (!obj)
	{
		return str;
	}
	if (INTL_GetCharCodeConverter(charset, fe_LocaleCharSetID, obj))
	{
		le_string = INTL_CallCharCodeConverter(obj, str,
			strlen((char *) str));
		if (!le_string)
		{
			le_string = str;
		}
	}
	else
	{
		le_string = str;
	}

	INTL_DestroyCharCodeConverter(obj);

	return le_string;
}

/*
 * fe_ConvertToXmString - convert text/encoding to XmString/XmFontList
 * 
 * NOTE: if the calling code does not yet set the XmFontList
 * then pass in font==NULL
 */

XmString
fe_ConvertToXmString(unsigned char *str, int16 charset, 
						fe_Font font, XmFontType type, XmFontList *fontList_p)
{
	unsigned char	*loc;
	XmString	xms;
	XmFontList font_list;
	XmFontListEntry	flentry;

	/*
	 * init default return values
	 */
	*fontList_p = NULL;

	if ((!IS_UNICODE_CSID(charset)) || (!font)) {
		loc = fe_ConvertToLocaleEncoding(charset, str);
		if (loc)
		{
			xms = XmStringCreate((char *) loc, XmFONTLIST_DEFAULT_TAG);
			if (loc != str)
			{
				free(loc);
			}
		}
		else
		{
			xms = NULL;
		}

		if (font) {
			flentry = XmFontListEntryCreate(XmFONTLIST_DEFAULT_TAG, type, font);
			if (flentry) {
				font_list = XmFontListAppendEntry(NULL, flentry);
				if (font_list)
					*fontList_p = font_list;
				XmFontListEntryFree(&flentry);
			}
		}
		return xms;

	}
	else {
		xms = fe_utf8_to_XmString(font, str, strlen(str), fontList_p);
		return xms;
	}

	return xms;
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

/* fe_GetTextSelection() is a direct replacement for XmTextGetSelection --
 * use XtFree() to free the returned string.
 */
char *
fe_GetTextSelection(Widget widget)
{
	char	*loc;
	char	*str;

	XP_ASSERT(XmIsText(widget) || XmIsTextField(widget));

	loc = NULL;
	if (XmIsText(widget)) {
		loc = XmTextGetSelection(widget);
	}
	else {
		loc = XmTextFieldGetSelection(widget);
	}
	if (!loc)
	{
		return NULL;
	}
	str = (char *) fe_ConvertFromLocaleEncoding(
		INTL_DefaultWinCharSetID(NULL), (unsigned char *) loc);
	if (str == loc)
	{
		str = XtNewString(str);
		if (!str)
		{
			return NULL;
		}
	}
	XtFree(loc);

	return str;
}


/* fe_GetTextField() is a direct replacement for XmTextGetString --
 * use XtFree() to free the returned string.
 */
char *
fe_GetTextField(Widget widget)
{
	char	*loc;
	char	*str;

	XP_ASSERT(XmIsText(widget) || XmIsTextField(widget));

	loc = NULL;
	XtVaGetValues(widget, XmNvalue, &loc, 0);
	if (!loc)
	{
		return NULL;
	}
	str = (char *) fe_ConvertFromLocaleEncoding(
		INTL_DefaultWinCharSetID(NULL), (unsigned char *) loc);
	if (str == loc)
	{
		str = XtNewString(str);
		if (!str)
		{
			return NULL;
		}
	}
	XtFree(loc);

	return str;
}


void
fe_SetTextField(Widget widget, const char *str)
{
	unsigned char	*loc;

	XP_ASSERT(XmIsText(widget) || XmIsTextField(widget));

	if ((NULL==str) || ('\0'==*str)) {
		XtVaSetValues(widget, XmNvalue, str, 0);
		return;
	}

	loc = fe_ConvertToLocaleEncoding(INTL_DefaultWinCharSetID(NULL),
		(unsigned char *) str);
	XtVaSetValues(widget, XmNvalue, loc, 0);
	if (loc != ((unsigned char *) str))
	{
		XP_FREE(loc);
	}
}


void
fe_SetTextFieldAndCallBack(Widget widget, const char *str)
{
	unsigned char	*loc = NULL;

	XP_ASSERT(XmIsText(widget) || XmIsTextField(widget));

	if ((NULL != str) && ('\0' != *str)) {

		loc = fe_ConvertToLocaleEncoding(INTL_DefaultWinCharSetID(NULL),
										 (unsigned char *) str);
	}

	/*
	 * Warning: on SGI, XtVaSetValues() doesn't run the
	 * valueChangedCallback, but XmTextFieldSetString() does.
	 */
	if (XmIsText(widget))
		XmTextSetString(widget, (char *) str);
	else if (XmIsTextField(widget))
		XmTextFieldSetString(widget, (char *) str);

	if (loc != ((unsigned char *) str))
	{
		XP_FREE(loc);
	}
}


/*
 * We link statically on AIX to force it to pick up our thread-safe malloc.
 * But AIX has a bug where linking statically results in a broken mbstowcs
 * (and wcstombs) being linked in. So we re-implement these here, so that
 * these are used. See bug # 13574.
 */

#ifdef AIXV3

size_t
mbstowcs(wchar_t *pwcs, const char *s, size_t n)
{
	int	charlen;
	size_t	inlen;
	size_t	ret;
	wchar_t	wc;

	if (!s)
	{
		return 0;
	}

	ret = 0;
	inlen = strlen(s) + 1;

	while (1)
	{
		wc = 0;
		charlen = mbtowc(&wc, s, inlen);
		if (charlen < 0)
		{
			return -1;
		}
		else if (charlen == 0)
		{
			if (pwcs)
			{
				if (n > 0)
				{
					*pwcs = 0;
				}
			}
			break;
		}
		else
		{
			if (pwcs)
			{
				if (n > 0)
				{
					*pwcs++ = wc;
					ret++;
					n--;
					if (n == 0)
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
			else
			{
				ret++;
			}
			inlen -= charlen;
			s += charlen;
		}
	}

	return ret;
}


size_t
wcstombs(char *s, const wchar_t *pwcs, size_t n)
{
	char	buf[MB_LEN_MAX];
	int	charlen;
	int	i;
	size_t	ret;

	if (!pwcs)
	{
		return 0;
	}

	ret = 0;

	while (1)
	{
		buf[0] = 0;
		charlen = wctomb(buf, *pwcs);
		if (charlen <= 0)
		{
			return -1;
		}
		else
		{
			if (s)
			{
				if (n >= charlen)
				{
					for (i = 0; i < charlen; i++)
					{
						*s++ = buf[i];
					}
					if (*pwcs)
					{
						ret += charlen;
					}
					else
					{
						break;
					}
					n -= charlen;
					if (n == 0)
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
			else
			{
				if (*pwcs)
				{
					ret += charlen;
				}
				else
				{
					break;
				}
			}
			pwcs++;
		}
	}

	return ret;
}

#endif /* AIXV3 */
