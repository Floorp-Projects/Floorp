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
   fonts.c --- managing fonts for layout.
   Created:    Jamie Zawinski     <jwz@netscape.com>, 23-Jun-94.
   Re-written: Erik van der Poel <erik@netscape.com>, 15-Oct-95.
 */


/* Currently, we allow the user to select the size of the font used, and
   the family/foundry.  Within that, the document author specifies
   a size from 1-7, a face (normal, bold, italic, bold-italic) and whether
   the font is fixed or variable width.  Meaning 7*4*2 = 56 possible
   fonts (but we load lazily).
 */


#include "mozilla.h"
#include "xfe.h"
#include "fonts.h"
#include "libi18n.h"
#include "prprf.h"
#include "prmem.h"
#include <xpgetstr.h>
#include "xupfonts.h"
#ifndef NO_WEB_FONTS
#include "nf.h"
#include "Mnfrc.h"     /* before Mnffbc to stop warnings about nfrc */
#include "Mnffmi.h"    /* before Mnffbc to stop warnings about nffmi */
#include "Mnff.h"      /* before Mnfdoer to stop warnings about nff */
#include "Mnfdoer.h"   /* before Mnffbc to stop warnings about nfdoer */
#include "Mnfrf.h"     /* before Mnffbc to stop warnings about nfrf */
#include "Mnffp.h"
#include "Mnffbc.h"    /* font consumer interface definitions */
#include "Mnffbu.h"    /* font utility interface definitions */
#include "Mnffbp.h"    /* font producer interface definitions */
#endif

#include "xp_qsort.h"

/* for XP_GetString() */
extern int XFE_OTHER_LANGUAGE;
extern int XFE_USER_DEFINED;


/* for debugging */
/* #define FE_PRINT_FONTS 1 */

#ifndef NO_WEB_FONTS
/* This is defined in mozilla.c */
extern void fe_GetProgramDirectory(char *path, int len);
#endif /* NO_WEB_FONTS */

#define fe_FONT_PITCH_MONO		0x1
#define fe_FONT_PITCH_PROPORTIONAL	0x2
#define fe_FONT_PITCH_BOTH		0x3


#define fe_TryFont(context, familyName, points, sizeNum, fontmask, charset, pitch, faceNum, dpy) \
	(*fe_CharSetFuncsArray[fe_CharSetInfoArray[charset].info].loadFont) \
(context, familyName, points, sizeNum, fontmask, charset, pitch, faceNum, dpy)


#define FE_MAYBE_FREE(p) do { if (p) { free(p); (p) = NULL; } } while (0)


#define FE_MALLOC_ZAP(type) ((type *) calloc(1, sizeof(type)))


#define FE_FONT_MASK_TO_PITCH(fontmask) (((fontmask) & LO_FONT_FIXED) ? 1 : 0)

#define FE_FONT_MASK_TOGGLE_PITCH(fontmask) ((fontmask) ^ LO_FONT_FIXED)

#define FE_NORMALIZE_SIZE(sizeNum) \
    do { (sizeNum) &= 0x7; if ((sizeNum) < 1) { (sizeNum) = 1; } } while (0)

#define FE_OPPOSITE_PITCH(p) ((p)==0 ? 1 : 0)
#define FE_OPPOSITE_WEIGHT_FACE(f) ((f)^1)
#define FE_OPPOSITE_SLANT_FACE(f) ((f)^2)

#define FE_FONT_MASK_TO_FACE(fontmask) \
	((((fontmask) & LO_FONT_BOLD) && \
	  ((fontmask) & LO_FONT_ITALIC)) ? 3 : \
	  ((fontmask) & LO_FONT_ITALIC) ? 2 : \
	  ((fontmask) & LO_FONT_BOLD) ? 1 : 0);

#define FE_FONT_SELECTED_FAMILY(charset, pitch) \
	fe_FontFindSelectedFamily(&fe_FontCharSets[charset].pitches[pitch])

typedef struct fe_StringProcessTable
{
	unsigned char	fontIndex;
	unsigned char	skip;
	unsigned char	len;
} fe_StringProcessTable;


typedef struct fe_FontGroupFont
{
	unsigned char	mask1;

	unsigned char	mask2;

	XFontStruct	*xFont;

} fe_FontGroupFont;


typedef struct fe_FontSetListEntry fe_FontSetListEntry;

struct fe_FontSetListEntry
{
	fe_FontSetListEntry	*next;
	char			*list;
	XFontSet		fontSet;
};


typedef int (*fe_XDrawStringFunc)(Display *dpy, Drawable d, GC gc,
	int x, int y, char *string, int len);


static fe_Font fe_FixedFont = NULL;

#define FE_DEFAULT_FONT_SIZE_INCREMENT	0.2
static double fe_FontSizeIncrement = FE_DEFAULT_FONT_SIZE_INCREMENT;

/* Allowing an arbitrarily large point sizes enables a condition
 * where viewing a web page which requests fonts in large point sizes
 * could cause the X server to run out of memory and crash.
 * Choose a reasonable maximum point size and normalize requests.
 */
#define FE_DEFAULT_FONT_MAXIMUM_POINTS 200
static int fe_FontMaximumPoints = FE_DEFAULT_FONT_MAXIMUM_POINTS;

static fe_FontFace *fe_FontFaceList = NULL;
static fe_FontSize *fe_FontSizeList = NULL;

static fe_FontSetListEntry *fe_FontSetList = NULL;

#ifndef NO_WEB_FONTS
static struct nffbc * fe_FontBroker  = NULL;
       struct nffbu * fe_FontUtility = NULL;
#endif

fe_FontCharSet fe_FontCharSets[INTL_CHAR_SET_MAX];

unsigned char fe_SortedFontCharSets[INTL_CHAR_SET_MAX];

typedef struct GenericFontFamily {
    char * class_name;
    char * generic_name;
    char * family_name;
} GenericFontFamily;

static GenericFontFamily fe_generics[] = {
    {"Serif",		"serif",      "times"},
    {"Sans-Serif",	"sans-serif", "helvetica"},
    {"Cursive",		"cursive",    "itc zapf chancery"},
    {"Fantasy",		"fantasy",    "new century schoolbook"},
    {"Monospace",	"monospace",  "courier"}
};

#define fe_GenericFamilyCount (sizeof(fe_generics) / sizeof(GenericFontFamily))

static XP_Bool fe_AreNormalFontsAvail(int16 win_csid);

static fe_Font
fe_LoadNormalFont(MWContext *context, char *familyName, 
                  int points, int sizeNum,
		  int fontmask, int charset, int pitch, int faceNum,
		  Display *dpy);

static fe_Font
fe_LoadFont(MWContext *context, int16 *charset, char *familyName,
            int points, int size, int fontmask);

static fe_Font
fe_LoadFontWithoutSubstitution(MWContext *context, int16 *charset, char *familyName,
                               int points, int sizeNum, int fontmask);

static fe_Font WrapPlatformFont(fe_Font platform_font,
                                unsigned char font_type);
#ifndef NO_WEB_FONTS
static fe_Font fe_LoadRenderableFont(MWContext *context, LO_TextAttr *attr,
                                     int16 *charset, char *family);

static double fe_ConvertHTMLSizeToPointSize(LO_TextAttr *attr, int16 charset);
#endif

static fe_FontSize *
fe_FindFontPointSize(fe_FontFamily *family, double decipoints,
                     XP_Bool search_for_fit, XP_Bool scale_to_fit);

static char * fe_FindGenericFamily(int charsetID, char *familyName);

static XP_Bool fe_AreEUCCNFontsAvail(int16 /* win_csid */);

static fe_Font
fe_LoadEUCCNFont(MWContext *context, char *familyName, int points,
                 int sizeNum, int fontmask, int charset, int pitch,
                 int faceNum, Display *dpy);
static void
fe_EUCCNTextExtents(fe_Font font, char *string, int len, int *direction,
	int *fontAscent, int *fontDescent, XCharStruct *overall);
static void
fe_DrawEUCCNString(Display *dpy, Drawable d, fe_Font font, GC gc, int x,
	int y, char *string, int len);
static void
fe_DrawEUCCNImageString(Display *dpy, Drawable d, fe_Font font, GC gc, GC gc2,
	int x, int y, char *string, int len);

static XP_Bool fe_AreEUCJPFontsAvail(int16 /* win_csid */);

static fe_Font
fe_LoadEUCJPFont(MWContext *context, char *familyName, int points,
                 int sizeNum, int fontmask, int charset, int pitch,
                 int faceNum, Display *dpy);
static void
fe_EUCJPTextExtents(fe_Font font, char *string, int len, int *direction,
	int *fontAscent, int *fontDescent, XCharStruct *overall);
static void
fe_DrawEUCJPString(Display *dpy, Drawable d, fe_Font font, GC gc, int x,
	int y, char *string, int len);
static void
fe_DrawEUCJPImageString(Display *dpy, Drawable d, fe_Font font, GC gc, GC gc2,
	int x, int y, char *string, int len);

static XP_Bool fe_AreEUCKRFontsAvail(int16 /* win_csid */);

static fe_Font
fe_LoadEUCKRFont(MWContext *context, char *familyName, int points,
                 int sizeNum, int fontmask, int charset, int pitch,
                 int faceNum, Display *dpy);
static void
fe_EUCKRTextExtents(fe_Font font, char *string, int len, int *direction,
	int *fontAscent, int *fontDescent, XCharStruct *overall);
static void
fe_DrawEUCKRString(Display *dpy, Drawable d, fe_Font font, GC gc, int x,
	int y, char *string, int len);
static void
fe_DrawEUCKRImageString(Display *dpy, Drawable d, fe_Font font, GC gc, GC gc2,
	int x, int y, char *string, int len);

static XP_Bool fe_AreEUCTWFontsAvail(int16 /* win_csid */);

static fe_Font
fe_LoadEUCTWFont(MWContext *context, char *familyName, int points,
                 int sizeNum, int fontmask, int charset, int pitch,
                 int faceNum, Display *dpy);
static void
fe_EUCTWTextExtents(fe_Font font, char *string, int len, int *direction,
	int *fontAscent, int *fontDescent, XCharStruct *overall);
static void
fe_DrawEUCTWString(Display *dpy, Drawable d, fe_Font font, GC gc, int x,
	int y, char *string, int len);
static void
fe_DrawEUCTWImageString(Display *dpy, Drawable d, fe_Font font, GC gc, GC gc2,
	int x, int y, char *string, int len);

static XP_Bool fe_AreBIG5FontsAvail(int16 /* win_csid */);

static fe_Font
fe_LoadBIG5Font(MWContext *context, char *familyName, int points,
                int sizeNum, int fontmask, int charset, int pitch,
                int faceNum, Display *dpy);
static void
fe_BIG5TextExtents(fe_Font font, char *string, int len, int *direction,
	int *fontAscent, int *fontDescent, XCharStruct *overall);
static void
fe_DrawBIG5String(Display *dpy, Drawable d, fe_Font font, GC gc, int x,
	int y, char *string, int len);
static void
fe_DrawBIG5ImageString(Display *dpy, Drawable d, fe_Font font, GC gc, GC gc2,
	int x, int y, char *string, int len);

/* Unicode support */
static XP_Bool fe_AreUnicodeFontsAvail(int16 /* win_csid */);
static void
fe_DrawUTF8StringImage(Display *dpy, Drawable d, fe_Font font, GC gc, GC gc2,
	int x, int y, char *string, int len);
static XFontStruct *
fe_UnicodeInitXfont(fe_UnicodePseudoFont *ufont, uint16 encoding);
static int16 *INTL_GetXCharSetIDs(Display *dpy);

enum
{
	FE_FONT_INFO_X8 = 0,
	FE_FONT_INFO_X16,
	FE_FONT_INFO_EUCCN,
	FE_FONT_INFO_EUCJP,
	FE_FONT_INFO_EUCKR,
	FE_FONT_INFO_EUCTW,
	FE_FONT_INFO_BIG5,
    FE_FONT_INFO_UNICODE
};


fe_CharSetFuncs fe_CharSetFuncsArray[] =
{
	{
						1,
		(fe_AreFontsAvailFunc)	fe_AreNormalFontsAvail,
		(fe_LoadFontFunc)		fe_LoadNormalFont,
		(fe_TextExtentsFunc)		XTextExtents,
		(fe_DrawStringFunc)		XDrawString,
		(fe_DrawImageStringFunc)	XDrawImageString
	},
	{
						1,
		(fe_AreFontsAvailFunc)	fe_AreNormalFontsAvail,
		(fe_LoadFontFunc)		fe_LoadNormalFont,
		(fe_TextExtentsFunc)		XTextExtents16,
		(fe_DrawStringFunc)		XDrawString16,
		(fe_DrawImageStringFunc)	XDrawImageString16
	},
	{
						2,
		(fe_AreFontsAvailFunc)		fe_AreEUCCNFontsAvail,
		(fe_LoadFontFunc)		fe_LoadEUCCNFont,
		(fe_TextExtentsFunc)		fe_EUCCNTextExtents,
		(fe_DrawStringFunc)		fe_DrawEUCCNString,
		(fe_DrawImageStringFunc)	fe_DrawEUCCNImageString
	},
	{
						4,
		(fe_AreFontsAvailFunc)		fe_AreEUCJPFontsAvail,
		(fe_LoadFontFunc)		fe_LoadEUCJPFont,
		(fe_TextExtentsFunc)		fe_EUCJPTextExtents,
		(fe_DrawStringFunc)		fe_DrawEUCJPString,
		(fe_DrawImageStringFunc)	fe_DrawEUCJPImageString
	},
	{
						2,
		(fe_AreFontsAvailFunc)		fe_AreEUCKRFontsAvail,
		(fe_LoadFontFunc)		fe_LoadEUCKRFont,
		(fe_TextExtentsFunc)		fe_EUCKRTextExtents,
		(fe_DrawStringFunc)		fe_DrawEUCKRString,
		(fe_DrawImageStringFunc)	fe_DrawEUCKRImageString
	},
	{
						3,
		(fe_AreFontsAvailFunc)		fe_AreEUCTWFontsAvail,
		(fe_LoadFontFunc)		fe_LoadEUCTWFont,
		(fe_TextExtentsFunc)		fe_EUCTWTextExtents,
		(fe_DrawStringFunc)		fe_DrawEUCTWString,
		(fe_DrawImageStringFunc)	fe_DrawEUCTWImageString
	},
	{
						2,
		(fe_AreFontsAvailFunc)		fe_AreBIG5FontsAvail,
		(fe_LoadFontFunc)		fe_LoadBIG5Font,
		(fe_TextExtentsFunc)		fe_BIG5TextExtents,
		(fe_DrawStringFunc)		fe_DrawBIG5String,
		(fe_DrawImageStringFunc)	fe_DrawBIG5ImageString
	},
	{
						4,
		(fe_AreFontsAvailFunc)		fe_AreUnicodeFontsAvail,
		(fe_LoadFontFunc)		fe_LoadUnicodeFont,
		(fe_TextExtentsFunc)		fe_UTF8TextExtents,
		(fe_DrawStringFunc)		fe_DrawUTF8String,
		(fe_DrawImageStringFunc)	fe_DrawUTF8StringImage
	},
};


fe_CharSetInfo fe_CharSetInfoArray[] =
{
    { CS_DEFAULT      , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_ASCII        , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_LATIN1       , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_JIS          , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_SJIS         , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_EUCJP        , FE_FONT_TYPE_GROUP , FE_FONT_INFO_EUCJP    },
    { CS_MAC_ROMAN    , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_BIG5         , FE_FONT_TYPE_GROUP , FE_FONT_INFO_BIG5     },
    { CS_GB_8BIT      , FE_FONT_TYPE_GROUP , FE_FONT_INFO_EUCCN    },
    { CS_CNS_8BIT     , FE_FONT_TYPE_GROUP , FE_FONT_INFO_EUCTW    },
    { CS_LATIN2       , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_MAC_CE       , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_KSC_8BIT     , FE_FONT_TYPE_GROUP , FE_FONT_INFO_EUCKR    },
    { CS_2022_KR      , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_8859_3       , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_8859_4       , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_8859_5       , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_8859_6       , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_8859_7       , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_8859_8       , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_8859_9       , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_SYMBOL       , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_DINGBATS     , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_DECTECH      , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_CNS11643_1   , FE_FONT_TYPE_X16   , FE_FONT_INFO_X16      },
    { CS_CNS11643_2   , FE_FONT_TYPE_X16   , FE_FONT_INFO_X16      },
    { CS_JISX0208     , FE_FONT_TYPE_X16   , FE_FONT_INFO_X16      },
    { CS_JISX0201     , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_KSC5601      , FE_FONT_TYPE_X16   , FE_FONT_INFO_X16      },
    { CS_TIS620       , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_JISX0212     , FE_FONT_TYPE_X16   , FE_FONT_INFO_X16      },
    { CS_GB2312       , FE_FONT_TYPE_X16   , FE_FONT_INFO_X16      },
    { CS_UCS2         , FE_FONT_TYPE_X16   , FE_FONT_INFO_X16      },
    { CS_UCS4         , FE_FONT_TYPE_X16   , FE_FONT_INFO_X16      },
    { CS_UTF8         , FE_FONT_TYPE_GROUP , FE_FONT_INFO_UNICODE  },
    { CS_UTF7         , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_NPC          , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_X_BIG5       , FE_FONT_TYPE_X16   , FE_FONT_INFO_X16      },
    { CS_USRDEF2      , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_KOI8_R       , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_MAC_CYRILLIC , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_CP_1251      , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_MAC_GREEK    , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_CP_1253      , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_CP_1250      , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_CP_1254      , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_MAC_TURKISH  , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
    { CS_GB2312_11    , FE_FONT_TYPE_X16   , FE_FONT_INFO_X16      },
    { CS_JISX0208_11  , FE_FONT_TYPE_X16   , FE_FONT_INFO_X16      },
    { CS_KSC5601_11   , FE_FONT_TYPE_X16   , FE_FONT_INFO_X16      },
    { CS_CNS11643_1110, FE_FONT_TYPE_X16   , FE_FONT_INFO_X16      },
    { CS_UCS2_SWAP    , FE_FONT_TYPE_X16   , FE_FONT_INFO_X16      },
    { CS_IMAP4_UTF7   , FE_FONT_TYPE_X8    , FE_FONT_INFO_X8       },
};

static unsigned char *fe_LocaleCharSets = NULL;
static int32 smallestFontFoundSize = 12;

XP_Bool
fe_IsCharSetSupported(int16 doc_csid)
{
/* need to deal with webfonts */
#if 1
	return TRUE;
#else
	int16 win_csid;

	win_csid = INTL_DocToWinCharSetID(doc_csid);
	return(fe_AreFontsAvail(win_csid));
#endif
}

XP_Bool
fe_AreFontsAvail(int16 win_csid)
{
	int result;

	result = (*fe_CharSetFuncsArray[fe_CharSetInfoArray[(win_csid) & 0xff].info
										].areFontsAvail)(win_csid);
	return result;
}

static void
fe_GetLocaleCharSets(Display *dpy)
{
	char		*charset;
	int16		charsetID;
	char		clas[512];
	XrmDatabase	db;
	char		*def;
	XFontSet	fontSet;
	int		i;
	int		j;
	char		*mimeCharSet;
	int		missCount;
	char		**missing;
	char		name[512];
	char		*p;
	char		*type;
	XrmValue	value;

	if (fe_LocaleCharSets)
	{
		return;
	}

#if defined(SCO) || defined(UNIXWARE) && (XlibSpecificationRelease == 5)
	/*
	** XCreateFontSet is very slow in old Xlibs, fixed in X11R6
	** If locale == C, just punt
	*/
	{
		char *locale;
		locale = setlocale(LC_CTYPE, NULL);
		if (*locale == 'C') /* SCO is C_C.C */
		{
			return;
		}
	}
#endif

	missing = NULL;
	missCount = 0;
	def = NULL;
	fontSet = XCreateFontSet(dpy, "*-iso8859-1", &missing, &missCount,
		&def);
	if ((!fontSet) || (missCount < 1) || (!missing))
	{
		goto get_locale_char_sets_fail;
	}

	fe_LocaleCharSets = malloc(missCount + 2);
	if (!fe_LocaleCharSets)
	{
		goto get_locale_char_sets_fail;
	}
	j = 0;
	fe_LocaleCharSets[j++] = ((unsigned char) (CS_LATIN1 & 0xff));

	charset = NULL;

	db = XtDatabase(dpy);

	for (i = 0; i < missCount; i++)
	{
		FE_MAYBE_FREE(charset);
		if (!missing[i])
		{
			continue;
		}
		/*
		(void) fprintf(real_stderr, "locale font %d: \"%s\"\n", i + 1,
			missing[i]);
		*/
		charset = strdup(missing[i]);
		if (!charset)
		{
			goto get_locale_char_sets_fail;
		}
		p = charset;
		while (*p)
		{
			(*p) = XP_TO_LOWER(*p);
			p++;
		}
		(void) PR_snprintf(clas, sizeof(clas),
			"%s.DocumentFonts.CharSet.Name", fe_progclass);
		(void) PR_snprintf(name, sizeof(name),
			"%s.documentFonts.charset.%s", fe_progclass, charset);
		if (XrmGetResource(db, name, clas, &type, &value))
		{
			mimeCharSet = (char *) value.addr;
		}
		else
		{
#ifdef DEBUG
			(void) fprintf(real_stderr, "fontset charset %s\n",
				charset);
#endif /* DEBUG */
			continue;
		}
		charsetID = INTL_CharSetNameToID(mimeCharSet);
		if ((charsetID == CS_UNKNOWN) && strcmp(charset, "x-ignore"))
		{
#ifdef DEBUG
			(void) fprintf(real_stderr, "fontset internal %s %s\n",
				charset, mimeCharSet);
#endif /* DEBUG */
			continue;
		}
		if (!strcmp(charset, "x-ignore"))
		{
			continue;
		}
		charsetID &= 0xff;
		if (charsetID >= INTL_CHAR_SET_MAX)
		{
			continue;
		}
		fe_LocaleCharSets[j++] = (unsigned char) charsetID;
	}

	FE_MAYBE_FREE(charset);

	fe_LocaleCharSets[j] = 0;

	XFreeStringList(missing);
	XFreeFontSet(dpy, fontSet);

	return;

get_locale_char_sets_fail:

	if (fontSet)
	{
		XFreeFontSet(dpy, fontSet);
	}
	if (missing)
	{
		XFreeStringList(missing);
	}
	if (fe_LocaleCharSets)
	{
		free(fe_LocaleCharSets);
		fe_LocaleCharSets = NULL;
	}
}

fe_FontFamily *
fe_FontFindSelectedFamily(fe_FontPitch *pitch)
{
	fe_FontFamily		*family;
	int			i;

	if (!pitch) return(NULL);

	if (pitch->selectedFamily) return(pitch->selectedFamily);

	family = NULL;
	for (i = 0; i < pitch->numberOfFamilies; i++)
	{
		if (pitch->families[i].selected)
		{
			family = &pitch->families[i];
			break;
		}
	}
	if (!family)
	{
		family = FE_MALLOC_ZAP(fe_FontFamily);
	}
	
	pitch->selectedFamily = family;
	return (family);
}

static fe_FontFamily *
fe_GetFontFamily(uint16 charset, int pitch)
{
	fe_FontFamily *family;
	family = FE_FONT_SELECTED_FAMILY((charset&0xFF), pitch);
	if (family == NULL) {
		return NULL;
	}
	if (!family->htmlSizesComputed) {
		fe_ComputeFontSizeTable(family);
	}
	return family;
}

fe_FontFamily *
fe_FontFindFamily(int charsetID, int pitchID, char *familyName)
{
	fe_FontFamily		*family;
	fe_FontPitch		*pitch;
	int			i;
        char *                  genericFamily;

	if (!familyName || !*familyName) return(NULL);

	family = NULL;
	pitch = &fe_FontCharSets[charsetID&0xFF].pitches[pitchID];
	for (i = 0; i < pitch->numberOfFamilies; i++)
	{
		if (!XP_STRCASECMP(pitch->families[i].family, familyName))
		{
			family = &pitch->families[i];
			if (!family->htmlSizesComputed) {
				fe_ComputeFontSizeTable(family);
			}
			break;
		}
	}

        if (family)
            return family;

        genericFamily = fe_FindGenericFamily(charsetID, familyName);
        if (!genericFamily)
            return family;

	for (i = 0; i < pitch->numberOfFamilies; i++) {
            if (!XP_STRCASECMP(pitch->families[i].family, genericFamily)) {
                family = &pitch->families[i];
                if (!family->htmlSizesComputed) {
                    fe_ComputeFontSizeTable(family);
                }
                break;
            }
	}
	return (family);
}

static char * fe_FindGenericFamily(int charsetID, char *familyName)
{
    char * genericFamily;
    int i;

    /* This is the "default font displayer's" mapping of the
     * generic font names, as described by CSS1, into common X fonts.
     */

    if (!familyName || !*familyName) return(NULL);

    /* Generic font names are mapped only for the Latin-1 charset */
    if (CS_LATIN1 != charsetID) return NULL;

    genericFamily = NULL;
    for (i = 0; i < fe_GenericFamilyCount; i++) {
        if (!XP_STRCASECMP(fe_generics[i].generic_name, familyName)) {
            genericFamily = fe_generics[i].family_name;
            break;
        }
    }
    return genericFamily;
}


XtPointer
fe_GetFont(MWContext *context, int sizeNum, int fontmask)
{
        XmFontListEntry flentry;
        XtPointer       fontOrFontSet;
        XmFontType      type;
	XmFontList      fontList;

        fontOrFontSet = fe_GetFontOrFontSet(context, NULL, sizeNum, fontmask,
                &type);
        if (!fontOrFontSet)
        {
                return NULL;
        }
        flentry = XmFontListEntryCreate(XmFONTLIST_DEFAULT_TAG, type,
                fontOrFontSet);
        if (!flentry)
        {
                return NULL;
        }
        fontList = XmFontListAppendEntry(NULL, flentry);
        if (!fontList)
        {
                return NULL;
        }
        XmFontListEntryFree(&flentry);
	return fontList;
}


XtPointer
fe_GetFontOrFontSet(MWContext *context, char *familyName, int sizeNum,
	int fontmask, XmFontType *type)
{
	unsigned char		charset;
	int16			cs;
	char			*def;
	Display			*dpy;
	fe_FontSetListEntry	*f;
	fe_FontFace		*face;
	int			faceNum;
	XtPointer		fontOrFontSet;
	char			*list;
	int			missCount;
	char			**missing;
	char			*newList;
	unsigned char		*p;
	int			pitch;
	fe_FontSize		*size;
	fe_FontFamily		*family;

	def = NULL;
	dpy = XtDisplay(CONTEXT_WIDGET(context));
	faceNum = FE_FONT_MASK_TO_FACE(fontmask);
	list = NULL;
	missCount = 0;
	missing = NULL;
	pitch = FE_FONT_MASK_TO_PITCH(fontmask);
	FE_NORMALIZE_SIZE(sizeNum);

	if (fe_LocaleCharSets)
	{
		p = fe_LocaleCharSets;
		while (*p)
		{
			charset = *p++;
			family = NULL;
			if (familyName && *familyName)
			{
				family = fe_FontFindFamily(charset, pitch, familyName);
				if (!family) return NULL;
			}
			if (!family)
			{
				family = fe_GetFontFamily(charset, pitch);
				if (!family)
				{
					return NULL;
				}
			}
			size = family->htmlSizes[sizeNum-1];
			if (!size)
			{
				continue;
			}
			face = size->faces[faceNum];
			if (!face)
			{
				continue;
			}
			if (!face->longXLFDFontName)
			{
				continue;
			}
			if (list)
			{
				newList = PR_smprintf("%s,%s", list,
					face->longXLFDFontName);
				free(list);
				list = newList;
			}
			else
			{
				list = strdup(face->longXLFDFontName);
			}
			if (!list)
			{
				return NULL;
			}
		}
		f = fe_FontSetList;
		while (f)
		{
			if (!strcmp(f->list, list))
			{
				break;
			}
			f = f->next;
		}
		if (f)
		{
			free(list);
			fontOrFontSet = f->fontSet;
			*type = XmFONT_IS_FONTSET;
		}
		else
		{
			fontOrFontSet = XCreateFontSet(dpy, list, &missing,
				&missCount, &def);
			if (fontOrFontSet)
			{
				/*
				(void) fprintf(real_stderr,
					"font set \"%s\" succeeded\n", list);
				*/
				f = XP_NEW_ZAP(fe_FontSetListEntry);
				if (!f)
				{
					return NULL;
				}
				f->next = fe_FontSetList;
				fe_FontSetList = f;
				f->list = list;
				f->fontSet = fontOrFontSet;
				*type = XmFONT_IS_FONTSET;
			}
			else
			{
				/*
				(void) fprintf(real_stderr,
					"font set \"%s\" failed\n", list);
				*/
				return NULL;
			}
		}
	}
	else
	{
		cs = CS_LATIN1;
		fontOrFontSet = fe_LoadFont(context, &cs, familyName,
					    0 /* no pointsize */,
                                            sizeNum, fontmask);
		*type = XmFONT_IS_FONT;
	}

	return fontOrFontSet;
}


static int
fe_PitchAndFaceToFontMask(int pitch, int face)
{
	int	fontmask;

	if (pitch)
	{
		fontmask = LO_FONT_FIXED;
	}
	else
	{
		fontmask = 0;
	}

	switch (face)
	{
	case 1:
		fontmask |= LO_FONT_BOLD;
		break;
	case 2:
		fontmask |= LO_FONT_ITALIC;
		break;
	case 3:
		fontmask |= (LO_FONT_BOLD | LO_FONT_ITALIC);
		break;
	default:
		break;
	}

	return fontmask;
}


static fe_Font
fe_LoadFontGroup(MWContext *context, char *familyName,
                 int points, int sizeNum,
	int fontmask, int16 charsetID, int pitch, int faceNum, Display *dpy)
{
	fe_FontFace	*face;
	fe_Font		font;
	int		i;
	fe_FontSize	*size;
	fe_FontFamily	*family;

	size = FE_MALLOC_ZAP(fe_FontSize);
	if (!size)
	{
		return NULL;
	}

	for (i = 0; i < 4; i++)
	{
		face = FE_MALLOC_ZAP(fe_FontFace);
		if (!face)
		{
			return NULL;
		}
		size->faces[i] = face;

		/*
		 * @@@ ignore familyName for now
		 * need to modify all the loadFont funcs too
		 */
		font = (*fe_CharSetFuncsArray[fe_CharSetInfoArray[charsetID].
			info].loadFont)(context, NULL, points, sizeNum,
			fe_PitchAndFaceToFontMask(pitch, i), charsetID,
			pitch, i, dpy);

		if (!font)
		{
			return NULL;
		}
		face->font = font;
	}

	family = FE_FONT_SELECTED_FAMILY(charsetID, pitch);
	if (!family)
	{
		XP_FREE(size);
		return NULL;
	}

        /* XXX what is going on here for pointsizes?  -converse */

        if (!points) {
            family->htmlSizes[sizeNum-1] = size;
        }
        return size->faces[faceNum]->font;
}


static fe_Font
fe_LoadFont(MWContext *context, int16 *charset, char *familyName,
	    int points, int sizeNum, int fontmask)
{
	int16		charsetID;
	Display		*dpy;
	fe_FontFace	*face;
	int		faceNum;
	fe_Font		font;
	int		pitch;
	fe_FontSize	*size;
	int		trySize;
	fe_FontFamily	*family;
        double          decipoints;

	/*
	 * paranoia, robustness
	 */

	charsetID = 0;
	if (charset)
	{
		charsetID = *charset;
	}
	else
	{
		charset = &charsetID;
	}

	if (!charsetID)
	{
		charsetID = INTL_DefaultTextAttributeCharSetID(context);
		if (!charsetID)
		{
			charsetID = CS_LATIN1;
		}
		*charset = charsetID;
	}
	charsetID &= 0xff;
	if (charsetID >= INTL_CHAR_SET_MAX)
	{
		charsetID = (CS_LATIN1 & 0xff);
		*charset = CS_LATIN1;
	}

	pitch = FE_FONT_MASK_TO_PITCH(fontmask);

	faceNum = FE_FONT_MASK_TO_FACE(fontmask);

	dpy = XtDisplay(CONTEXT_WIDGET(context));

	family = NULL;
	if (familyName && *familyName)
	{
		family = fe_FontFindFamily(charsetID, pitch, familyName);
		if ((!family) && (fe_CharSetInfoArray[charsetID].type !=
			FE_FONT_TYPE_GROUP))
		{
			return NULL;
		}
	}
	if (!family)
	{
		/* We blew the font lookup, falling back to default. */
		family = fe_GetFontFamily(*charset, pitch);

        /* Invalidate the point size, I can't read it. */
		points = 0;

		if (!family)
		{
			if (*charset == CS_LATIN1)
			{
				return fe_FixedFont;
			}
			else
			{
				*charset = CS_LATIN1;
				return fe_LoadFont(context, charset,
                                                   familyName, points,
                                                   sizeNum, fontmask);
			}
		}
	}
        if (! points)
            size = family->htmlSizes[sizeNum-1];
        else {
            decipoints = (double) (points * 10);
            size = fe_FindFontPointSize(family, decipoints, False, True);
        }
	if (!size)
	{
		if (fe_CharSetInfoArray[charsetID].type == FE_FONT_TYPE_GROUP)
		{
			font = fe_LoadFontGroup(context, familyName, points,
                                                sizeNum, fontmask, charsetID,
                                                pitch, faceNum, dpy);
			if (font)
			{
				return font;
			}
		}

		/*
		 * this charset is not available
		 * use Latin-1 font to display the bytes
		 */
		if (*charset == CS_LATIN1)
		{
			return fe_FixedFont;
		}
		else
		{
			*charset = CS_LATIN1;
			return fe_LoadFont(context, charset, familyName,
					   points, sizeNum, fontmask);
		}
	}

	face = size->faces[faceNum];
	if (!face)
	{
		/*
		 * this shouldn't happen
		 * if it does, we will display these bytes in a Latin-1 font
		 */
		if (*charset == CS_LATIN1)
		{
			return fe_FixedFont;
		}
		else
		{
			*charset = CS_LATIN1;
			return fe_LoadFont(context, charset, familyName,
					   points, sizeNum, fontmask);
		}
	}

	font = face->font;
	if (!font)
	{
		font = fe_TryFont(context, familyName, points, sizeNum,
                                  fontmask, charsetID, pitch, faceNum, dpy);
		if (font)
		{
                    face->font = font;
		}
		else
		{
			for (trySize = sizeNum - 1; trySize > 0; trySize--)
			{
				font = fe_TryFont(context, familyName,
						  points, trySize, fontmask,
						  charsetID, pitch,
						  faceNum, dpy);
				if (font)
				{
					return font;
				}
			}

			for (trySize = sizeNum + 1; trySize < 8; trySize++)
			{
				font = fe_TryFont(context, familyName,
						  points, trySize, fontmask,
						  charsetID, pitch,
						  faceNum, dpy);
				if (font)
				{
					return font;
				}
			}

			*charset = CS_LATIN1;
			font = fe_FixedFont;
			face->font = font;
			return font;

			/* @@@ need to change this */
			/*
			(void) fprintf(real_stderr, "could not load\n%s\n",
				face->longXLFDFontName);
			*/
			/* XP_GetString(XFE_COULD_NOT_LOAD_FONT) */
			/* XP_GetString(XFE_USING_FALLBACK_FONT) */
			/* XP_GetString(XFE_NO_FALLBACK_FONT) */
			/* FE_Alert(context, msg); */
		}
	}

	return font;
}

static fe_Font
fe_LoadFontWithoutSubstitution(MWContext *context, int16 *charset, char *familyName,
                               int points, int sizeNum, int fontmask)
{
	int16		charsetID;
	Display		*dpy;
	fe_FontFace	*face;
	int		faceNum;
	fe_Font		font;
	int		pitch;
	fe_FontSize	*size;
	int		trySize;
	fe_FontFamily	*family;
        double          decipoints;

	/*
	 * paranoia, robustness
	 */

	charsetID = 0;
	if (charset)
	{
		charsetID = *charset;
	}
	else
	{
		charset = &charsetID;
	}

	if (!charsetID)
	{
		charsetID = INTL_DefaultTextAttributeCharSetID(context);
		if (!charsetID)
		{
			charsetID = CS_LATIN1;
		}
		*charset = charsetID;
	}
	charsetID &= 0xff;
	if (charsetID >= INTL_CHAR_SET_MAX)
	{
		charsetID = (CS_LATIN1 & 0xff);
		*charset = CS_LATIN1;
	}

	pitch = FE_FONT_MASK_TO_PITCH(fontmask);

	faceNum = FE_FONT_MASK_TO_FACE(fontmask);

	dpy = XtDisplay(CONTEXT_WIDGET(context));

	family = NULL;
	if (familyName && *familyName)
	{
		family = fe_FontFindFamily(charsetID, pitch, familyName);
		if (!family)
		{
			return NULL;
		}
	}
        else
        {
            return NULL;
        }
        if (! points)
            size = family->htmlSizes[sizeNum-1];
        else {
            decipoints = (double) (points * 10);
            size = fe_FindFontPointSize(family, decipoints, False, True);
        }
	if (!size)
	{
            return NULL;
        }
	face = size->faces[faceNum];
	if (!face)
	{
            return NULL;
	}
	font = face->font;
	if (!font)
	{
		font = fe_TryFont(context, familyName, points, sizeNum,
                                  fontmask, charsetID, pitch, faceNum, dpy);
		if (font)
		{
                    face->font = font;
		}
		else if (points == 0)
		{
			for (trySize = sizeNum - 1; trySize > 0; trySize--)
			{
				font = fe_TryFont(context, familyName,
						  points, trySize, fontmask,
						  charsetID, pitch,
						  faceNum, dpy);
				if (font)
				{
					return font;
				}
			}

			for (trySize = sizeNum + 1; trySize < 8; trySize++)
			{
				font = fe_TryFont(context, familyName,
						  points, trySize, fontmask,
						  charsetID, pitch,
						  faceNum, dpy);
				if (font)
				{
					return font;
				}
			}
		}
	}
	return font;
}


static char * NameFromList(char * namelist)
{
    static char * list;
    static int next;
    static int len;

    int start;
    int end;
    char final = '\0';

    if (namelist) {
        list = namelist;
        len = strlen(namelist);
        next = 0;
    }
    
    start = next;
        
    if (! list || list[start] == '\0')
        return (char *) NULL;
        
    /* skip over leading whitespace and commas to find the start of the name */
    while (list[start] != '\0' && (list[start] == ',' || isspace(list[start])))
        start++;
        
    /* find the end of the name */
    next = start;
    while(list[next] != '\0' && list[next] != ',') {
        if (list[next] == '"' || list[next] == '\'') {
            final = list[next];
                        
            /* find the matching quote */
            next++;
            while (list[next] != '\0' && list[next] != final)
                next++;

            /* if list[next] is null, there was no matching quote, so bail */
            if (list[next] == '\0')
                break;
        }
        next++;
    }
    end = next - 1;

    if (list[next] != '\0' && next < len)
        next++;

    /* strip off trailing whitespace */        
    while (end >= start && isspace(list[end]))
        end--;
        
    /* if it's quoted, strip off the quotes */
    if ((list[start] == '"' || list[start] == '\'') && list[end] == list[start]) {
        start++;
        end--;
    }

    end++;
    list[end] = '\0';
    return &(list[start]);
}

fe_Font
fe_LoadFontFromFace(MWContext *context, LO_TextAttr *attr, int16 *charset, char *net_font_face,
		   int size, int fontmask)
{
	fe_Font font = 0;
	char *local_net_font_face = NULL;
	char *familyname = NULL;
        char *last_familyname = NULL;
        unsigned char font_type = 0;
        int16 charsetID;
        int16 saved_charset;
        int points = 0;

	if (attr)
	{
		font = (fe_Font) attr->FE_Data;
		if (font) return(font);

                points = (int) attr->point_size;
                if (points < 1)
                    points = 0;
                if (points > fe_FontMaximumPoints)
                    points = fe_FontMaximumPoints;

                if (attr->font_weight > 0) {
                    if (attr->font_weight <= 500)
                        fontmask &= ~LO_FONT_BOLD;
                    else 
                        fontmask |= LO_FONT_BOLD;
                }
	}

        charsetID = 0;
	if (charset)
	{
		charsetID = *charset;
	}
	else
	{
		charset = &charsetID;
	}

	if (!charsetID)
	{
		charsetID = INTL_DefaultTextAttributeCharSetID(context);
		if (!charsetID)
		{
			charsetID = CS_LATIN1;
		}
		*charset = charsetID;
	}
	charsetID &= 0xff;
	if (charsetID >= INTL_CHAR_SET_MAX)
	{
		charsetID = (CS_LATIN1 & 0xff);
		*charset = CS_LATIN1;
	}
        saved_charset = *charset;

	FE_NORMALIZE_SIZE(size);

	if (net_font_face && *net_font_face)
	{
		local_net_font_face = strdup(net_font_face);
		if (!local_net_font_face) return (0);
		familyname = NameFromList(local_net_font_face);
		while (familyname)
		{
#if !defined(NO_WEB_FONTS) && !defined(NO_BROKERED_WEB_FONTS)

                    /* Prefer fonts from the broker. */
                    font = fe_LoadRenderableFont(context, attr, charset,
                                                 familyname);
                    if (font)
                        font_type = FE_FONT_TYPE_RENDERABLE;
#endif
                    if (! font) {

                        /* First, determine whether any font is available
                         * in this charset; if not, the charset will be 
                         * reassigned to CS_LATIN1.
                         */
                        (void) fe_LoadFont(context, charset, NULL, 0, 3, 0);

                        font = fe_LoadFontWithoutSubstitution(context, charset,
                                    familyname, points, size, fontmask);
                        if (!font)
                            /* Try the other pitch */
                            font = fe_LoadFontWithoutSubstitution(context,
                                        charset, familyname, points, size,
                                        FE_FONT_MASK_TOGGLE_PITCH(fontmask));
                    }

                    if (font) break;
                    last_familyname = familyname;
                    familyname = NameFromList((char *) NULL);
		}
                if (!font && last_familyname) {
                    font = fe_LoadFont(context, charset, last_familyname,
                                       points, size, fontmask);
                    if (!font)
			{
                            /* Try the other pitch */
                            font = fe_LoadFont(context, charset,
                                               familyname, points, size,
                                               FE_FONT_MASK_TOGGLE_PITCH(fontmask));
			}
                }
                XP_FREE(local_net_font_face);
	}

	/* If we didn't get a font from the face, default to no font_face */
	if (!font)
	{
            font =  fe_LoadFont(context, charset, NULL, points, size,
                                fontmask);
	}
	if (font)
	{
                fe_Font platform_font = font;
                if (FE_FONT_TYPE_RENDERABLE != font_type)
                    font_type = fe_CharSetInfoArray[(*charset) & 0xff].type;
                font = WrapPlatformFont(platform_font, font_type);
		if (attr)
		{
			attr->FE_Data = (void *) font;
		}
	}
        /* Preserve the document charset.  Web fonts may be used to
         * render glyphs in character sets which are not represented
         * in the platform-specific font catalog.  
         */
        *charset = saved_charset;
	return(font);
}


static fe_Font WrapPlatformFont(fe_Font platform_font, unsigned char font_type)
{
    fe_FontWrap *fw;

    fw = XP_NEW(fe_FontWrap);
    if (!fw) return NULL;

    fw->platform_font = platform_font;
    fw->distinguisher = font_type;

    return (fe_Font) fw;
}

/* Delete the wrapper but do not free the font. */
void fe_DoneWithFont(fe_Font font)
{
    XP_DELETE(font);
}

static void FreePlatformFont(Display *dpy, XFontStruct *xfs, LO_TextAttr *attr)
{
    XFontProp *prop;
    int n;
    fe_FontSize *size = 0;
    fe_FontSize **prev = 0;
    fe_FontFace *face = 0;
    fe_FontFamily *family;
    XP_Bool in_use;
    XP_Bool found;
    char * familyName;
    int pitch;
    Atom atom;

    /* Do not free fonts which fulfill requests for HTML sizes */
    if (attr->point_size < 1)
        return;

    /* Take the family name from the FAMILY_NAME property of the font */
    familyName = NULL;
    for (n = xfs->n_properties, prop=xfs->properties; --n >= 0; prop++) {
        if (XA_FAMILY_NAME == prop->name) {
            atom = prop->card32;
            familyName = XGetAtomName(dpy, atom);
            break;
        }
    }
    if (! familyName)
        return;

    /* Use the font family name, the requested charset and the 
     * requested pitch to find the family record.
     */
    pitch = FE_FONT_MASK_TO_PITCH(attr->fontmask);
    family = fe_FontFindFamily(attr->charset, pitch, familyName);
    if (! family) {
        pitch = FE_OPPOSITE_PITCH(pitch);
        family = fe_FontFindFamily(attr->charset, pitch, familyName);
    }
    if (! family && CS_LATIN1 != attr->charset) {
        pitch = FE_FONT_MASK_TO_PITCH(attr->fontmask);
        family = fe_FontFindFamily(CS_LATIN1, pitch, familyName);
        if (! family) {
            pitch = FE_OPPOSITE_PITCH(pitch);
            family = fe_FontFindFamily(CS_LATIN1, pitch, familyName);
        }
    }        
    XFree(familyName);

    /* Search through the family pool for a match on this xfs */
    found = False;
    if (family) {
        prev = &(family->pool);
        for (size = family->pool; size; prev = &(size->next), 
                 size = size->next) {
            for (n=0; n <= 3; n++) {
                face = size->faces[n];
                if (face && face->font == xfs) {
                    found = True;
                    break;
                }
            }
            if (found)
                break;
        }
    }

    /* Free the font and set the font field to NULL */
    if (found && size) {
        XFreeFont(dpy, xfs);
        face->font = NULL;

        /* If all the faces are not loaded, free the faces
         * and free the size record which points to them.
         */
        in_use = False;
        for (n=0; n < 4; n++) {
            if (size->faces[n]->font != NULL)
                in_use = True;
        }
        if ((! in_use) && prev) {
            *prev = size->next;
            for (n=0; n < 4; n++) {
                face = size->faces[n];
                if (face->longXLFDFontName)
                    free(face->longXLFDFontName);
                free(face);
            }
            free(size);
        }
    }
}

void FE_ReleaseTextAttrFeData(MWContext *context, LO_TextAttr *attr)
{
    fe_FontWrap *fw;
    struct nfrf *rf;
    XFontStruct *xfs;
    Display *dpy;

    if (!context || !attr)
        return;

    fw = (fe_FontWrap *) attr->FE_Data;
    if (! fw) return;

    switch (fw->distinguisher) {
    case FE_FONT_TYPE_X8:
    case FE_FONT_TYPE_X16:
        dpy = XtDisplay(CONTEXT_WIDGET(context));
        xfs = (XFontStruct *) fw->platform_font;
        FreePlatformFont(dpy, xfs, attr);
        break;

    case FE_FONT_TYPE_GROUP:
    default:
        break;

    case FE_FONT_TYPE_RENDERABLE:
        rf = (struct nfrf *) fw->platform_font;
        nfrf_release(rf, NULL);
        break;
    }

    XP_DELETE(fw);
    attr->FE_Data = NULL;
}

#ifndef NO_WEB_FONTS
static fe_Font fe_LoadRenderableFont(MWContext *context, LO_TextAttr *attr,
                                     int16 *charset, char *family)

{
    int weight;
    int escapement;
    int style;
    int underline;
    int strikeout;
    int charsetID;
    struct nfrc *  rc;
    struct nffmi * fmi;
    struct nff *   unsized_font = NULL;
    struct nfrf *  renderable_font = NULL;
    void *         rcbuf[4];
    double pointsize;
    char mime_name[128];

    if (!context || !attr || !charset)
        return (fe_Font) NULL;
    
    /* Create a rendering context */

    rcbuf[0] = XtDisplay(CONTEXT_WIDGET(context));
    rcbuf[1] = rcbuf[2] = NULL;
    rc = nffbu_CreateRenderingContext(fe_FontUtility, NF_RC_DIRECT, 0,
                                      rcbuf, 3, NULL);
    if (!rc)
        return (fe_Font) NULL;

    /* Create a font match information object */

    charsetID = *charset;
    if (charsetID < 0 || (charsetID & 0xff) >= INTL_CHAR_SET_MAX)
        return (fe_Font) NULL;
    INTL_CharSetIDToName(charsetID, mime_name);
    weight = attr->font_weight;
    if (0 == weight)
        weight = (LO_FONT_BOLD & attr->attrmask) ? 700 : 400;
    style = (LO_FONT_ITALIC & attr->attrmask)
        ? nfStyleItalic : nfStyleNormal;
    underline = (LO_ATTR_UNDERLINE & attr->attrmask) 
        ? nfUnderlineYes : nfUnderlineNo;
    strikeout = (LO_ATTR_STRIKEOUT & attr->attrmask)
        ? nfStrikeOutYes : nfStrikeOutNo;
    escapement = (LO_FONT_FIXED & attr->attrmask)
        ? nfSpacingMonospaced : nfSpacingProportional;
    fmi = nffbu_CreateFontMatchInfo(fe_FontUtility,
                                    family, NULL, mime_name, weight,
                                    escapement, style, underline, strikeout,
                                    nfResolutionDontCare,
                                    nfResolutionDontCare,
                                    NULL /* exception */);

    /* Create the font, cleanup, and return the font */

    if (fmi) {
		/* Find the url that is loading this font */
		History_entry *he = SHIST_GetCurrent(&context->hist);
		const char *accessing_url_str = NULL;
		if (he)
		{
	 		accessing_url_str = he->address;
		}

        unsized_font = nffbc_LookupFont(fe_FontBroker, rc, fmi,
					accessing_url_str, NULL);
        if (!unsized_font) {
            nffbu_LookupFailed(fe_FontUtility, context, rc, fmi, NULL);
        }
        nffmi_release(fmi, NULL);

        if (unsized_font) {

            pointsize = attr->point_size;
            if (pointsize <= 0)
                pointsize = fe_ConvertHTMLSizeToPointSize(attr, charsetID);

            renderable_font = nff_GetRenderableFont(unsized_font, rc,
                                                    pointsize, NULL);
            nff_release(unsized_font, NULL);
        }
    }
    nfrc_release(rc, NULL);
    return ((fe_Font) renderable_font);
}


int fe_WebfontsNeedReload(MWContext *context)
{
	int ret = 0;
	if (fe_FontUtility)
	{
    	ret = nffbu_WebfontsNeedReload(fe_FontUtility, context, NULL);
	}
	return (ret);
}

void fe_ShutdownWebfonts(void)
{
	char buf[MAXPATHLEN];
	char *home = NULL;

	/* Load Catalog */
	if (home = getenv("HOME"))
	{
		/* Form "$HOME/.netscape/dynfonts/fonts.cat" into buf */
		strncpy(buf, home, sizeof(buf)-1);
		strncat(buf, "/.netscape/dynfonts/fonts.cat",
				sizeof(buf)-1 - strlen(buf));
		buf[sizeof(buf)-1] = '\0';
		nffbu_SaveCatalog(fe_FontUtility, buf, NULL);
	}
}


static double fe_ConvertHTMLSizeToPointSize(LO_TextAttr *attr, int16 charset)
{
    int j;
    int size3;
    double decipoints = 0.0;
    fe_FontFamily *family;
    fe_FontSize *size;
    int pitch;
    double pointsize;

    /* Translate HTML sizes 1-7 to pointsize */

    /* Take the default size of the default family for
     * this charset to be the pointsize of HTML size 3,
     * if it is given; otherwise, use 12 points.
     */
    size3 = 0;
    size = NULL;
    pitch = FE_FONT_MASK_TO_PITCH(attr->attrmask);
    family = FE_FONT_SELECTED_FAMILY(charset, pitch);
    if (family && family->name) {
        for (j = 0; j < family->numberOfPointSizes; j++) {
            size = &family->pointSizes[j];
            if (size->selected) {
                break;
            } else {
                size = NULL;
            }
        }
        if (size)
            size3 = size->size;
        if (!size3)
            size3 = family->scaledSize;
    }
    if (!size3)
        size3 = 120;

    /* Sizing logic from fe_ComputeFontSizeTable */
    if (attr->size < 3) {
        decipoints = ((1 - ((3 - attr->size)
                            * fe_FontSizeIncrement)) * size3);
        if (decipoints < 10)
            decipoints = 10;
    } else if (attr->size == 3) {
        decipoints = size3;
    } else if (attr->size > 3) {
        decipoints = ((1 + ((attr->size - 3)
                            * fe_FontSizeIncrement)) * size3);
    }

    pointsize = decipoints / 10;
    return pointsize;
}

#endif /* ifndef NO_WEB_FONTS */


XtPointer
fe_GetFontOrFontSetFromFace(MWContext *context, LO_TextAttr *attr, char *net_font_face,
			    int size, int fontmask, XmFontType *type)
{
	XtPointer fontOrFontset = 0;
	char *local_net_font_face = NULL;
	char *familyname = NULL;

	if (attr)
	{
		fontOrFontset = (fe_Font) attr->FE_Data;
		if (fontOrFontset) return(fontOrFontset);
	}
	
	if (net_font_face && *net_font_face)
	{
		local_net_font_face = strdup(net_font_face);
		if (!local_net_font_face) return (0);
		familyname = NameFromList(local_net_font_face);
		while (familyname)
		{
			fontOrFontset = fe_GetFontOrFontSet(context, familyname, size,
							    fontmask, type);
			if (!fontOrFontset)
			{
				/* Try the other pitch */
				fontOrFontset = fe_GetFontOrFontSet(context, familyname, size,
								    FE_FONT_MASK_TOGGLE_PITCH(fontmask),
								    type);
			}
			if (fontOrFontset) break;
			familyname = NameFromList((char *) NULL);
		}
		XP_FREE(local_net_font_face);
	}
	
	if (!fontOrFontset)
	{
		fontOrFontset = fe_GetFontOrFontSet(context, NULL, size,
						    fontmask, type);
	}
	if (fontOrFontset)
	{
		if (attr)
		{
			attr->FE_Data = (void *) fontOrFontset;
		}
	}
	return(fontOrFontset);
}


static void *
fe_ReallocZap(void *p, int oldSize, int newSize)
{
	char	*end;
	char	*q;

	p = realloc(p, newSize);
	if (p)
	{
		if (newSize > oldSize)
		{
			end = ((char *) p) + newSize;
			for (q = ((char *) p) + oldSize; q < end; q++)
			{
				*q = 0;
			}
		}
	}

	return p;
}


#define FE_REALLOC_ZAP(p, oldSize, newSize, type) \
  ((type *) fe_ReallocZap((p), (oldSize) * sizeof(type), \
    (newSize) * sizeof(type)))


static char *
fe_CapitalizeFontName(char *name)
{
	char	*p;
	char	*ret;

	ret = strdup(name);
	if (!ret)
	{
		return NULL;
	}

	p = ret;
	while (1)
	{
		while ((*p) && XP_IS_SPACE(*p))
		{
			p++;
		}
		if (!*p)
		{
			break;
		}
		*p = XP_TO_UPPER(*p);
		while ((*p) && (!XP_IS_SPACE(*p)))
		{
			p++;
		}
		if (!*p)
		{
			break;
		}
	}

	return ret;
}


static void
fe_AddFontSize(fe_FontFamily *family, int faceNum, int pixelSizeNum,
	int pointSizeNum, char *pixelSizeFontSpec, char *pointSizeFontSpec)
{
	int		alloc;
	fe_FontFace	*face;
	int		i;
	fe_FontSize	*newSizes;
	fe_FontSize	*size;

	/*
	 * first we do the pixel size
	 */

	size = NULL;

	if (family->pixelSizes)
	{
		for (i = 0; i < family->numberOfPixelSizes; i++)
		{
			size = &family->pixelSizes[i];
			if (size->size == pixelSizeNum)
			{
				break;
			}
			else
			{
				size = NULL;
			}
		}
	}

	if (!size)
	{
		if (family->numberOfPixelSizes >= family->pixelAlloc)
		{
			alloc = (2 * (family->pixelAlloc + 1));
			newSizes = FE_REALLOC_ZAP(family->pixelSizes,
				family->pixelAlloc, alloc, fe_FontSize);
			if (!newSizes)
			{
				return;
			}
			family->pixelSizes = newSizes;
			family->pixelAlloc = alloc;
		}
		size = &family->pixelSizes[family->numberOfPixelSizes++];
		size->size = pixelSizeNum;
	}

	if (!size->faces[faceNum])
	{
		face = FE_MALLOC_ZAP(fe_FontFace);
		if (!face)
		{
			return;
		}
		face->longXLFDFontName = strdup(pixelSizeFontSpec);
		if (!face->longXLFDFontName)
		{
			free(face);
			return;
		}
		face->next = fe_FontFaceList;
		fe_FontFaceList = face;
		size->faces[faceNum] = face;
	}
	else
	{
		/* @@@ fix this -- erik */
		/*
		(void) fprintf(real_stderr, "dup %s\n", pixelSizeFontSpec);
		*/
	}

	/*
	 * now we do the same for the point size
	 */

	size = NULL;

	if (family->pointSizes)
	{
		for (i = 0; i < family->numberOfPointSizes; i++)
		{
			size = &family->pointSizes[i];
			if (size->size == pointSizeNum)
			{
				break;
			}
			else
			{
				size = NULL;
			}
		}
	}

	if (!size)
	{
		if (family->numberOfPointSizes >= family->pointAlloc)
		{
			alloc = (2 * (family->pointAlloc + 1));
			newSizes = FE_REALLOC_ZAP(family->pointSizes,
				family->pointAlloc, alloc, fe_FontSize);
			if (!newSizes)
			{
				return;
			}
			family->pointSizes = newSizes;
			family->pointAlloc = alloc;
		}
		size = &family->pointSizes[family->numberOfPointSizes++];
		size->size = pointSizeNum;
	}

	if (!size->faces[faceNum])
	{
		face = FE_MALLOC_ZAP(fe_FontFace);
		if (!face)
		{
			return;
		}
		face->longXLFDFontName = strdup(pointSizeFontSpec);
		if (!face->longXLFDFontName)
		{
			free(face);
			return;
		}
		face->next = fe_FontFaceList;
		fe_FontFaceList = face;
		size->faces[faceNum] = face;
	}
	else
	{
		/* @@@ fix this -- erik */
		/*
		(void) fprintf(real_stderr, "dup %s\n", pointSizeFontSpec);
		*/
	}
}


static void
fe_AddFontFamily(fe_FontPitch *pitch, char *origFoundry, char *foundry,
	char *origFamily, char *familyName, int face, int pixelSize,
	int pointSize, char *pixelSizeFontSpec, char *pointSizeFontSpec,
	int charsetID, char *origCharset)
{
	int		alloc;
	fe_FontFamily	*family;
	int		i;
	char		*name;
	fe_FontFamily	*newFamilies;

	if (charsetID == CS_USRDEF2)
	{
		if (*foundry)
		{
			name = PR_smprintf("%s (%s, %s)", familyName, foundry,
				origCharset);
		}
		else
		{
			name = PR_smprintf("%s (%s)", familyName, origCharset);
		}
	}
	else
	{
		if (*foundry)
		{
			name = PR_smprintf("%s (%s)", familyName, foundry);
		}
		else
		{
			name = strdup(familyName);
		}
	}
	if (!name)
	{
		return;
	}

	family = NULL;

	if (pitch->families)
	{
		for (i = 0; i < pitch->numberOfFamilies; i++)
		{
			family = &pitch->families[i];
			if (!strcmp(family->name, name))
			{
				break;
			}
			else
			{
				family = NULL;
			}
		}
	}

	if (family)
	{
		free(name);
	}
	else
	{
		if (pitch->numberOfFamilies >= pitch->alloc)
		{
			alloc = (2 * (pitch->alloc + 1));
			newFamilies = FE_REALLOC_ZAP(pitch->families,
				pitch->alloc, alloc, fe_FontFamily);
			if (!newFamilies)
			{
				free(name);
				return;
			}
			pitch->families = newFamilies;
			pitch->alloc = alloc;
		}
		family = &pitch->families[pitch->numberOfFamilies++];
		family->name = name;
		family->family = strdup(origFamily);
		family->foundry = strdup(origFoundry);
	}

	fe_AddFontSize(family, face, pixelSize, pointSize, pixelSizeFontSpec,
		pointSizeFontSpec);
}


static void
fe_AddFontCharSet(char *origFoundry, char *foundry, char *origFamily,
	char *family, int face, int pixelSize, int pointSize, int pitch,
	int charsetID, char *charset, char *charsetlang,
	char *pixelSizeFontSpec, char *pointSizeFontSpec, char *origCharset)
{
	char	mimeName[128];
	char	*name;

	if ((charsetID < 0) || (charsetID >= INTL_CHAR_SET_MAX))
	{
		return;
	}

	if (!fe_FontCharSets[charsetID].name)
	{
		if (charsetID == CS_USRDEF2)
		{
			name = strdup(XP_GetString(XFE_USER_DEFINED));
		}
		else
		{
			name = PR_smprintf("%s (%s)", charsetlang, charset);
		}
		if (!name)
		{
			return;
		}
		fe_FontCharSets[charsetID].name = name;

		INTL_CharSetIDToName(charsetID, mimeName);
		fe_FontCharSets[charsetID].mimeName = strdup(mimeName);
		if (!fe_FontCharSets[charsetID].mimeName)
		{
			return;
		}
	}

	if (pitch & fe_FONT_PITCH_PROPORTIONAL)
	{
		fe_AddFontFamily(&fe_FontCharSets[charsetID].pitches[0],
			origFoundry, foundry, origFamily, family, face,
			pixelSize, pointSize, pixelSizeFontSpec,
			pointSizeFontSpec, charsetID, origCharset);
	}

	if (pitch & fe_FONT_PITCH_MONO)
	{
		fe_AddFontFamily(&fe_FontCharSets[charsetID].pitches[1],
			origFoundry, foundry, origFamily, family, face,
			pixelSize, pointSize, pixelSizeFontSpec,
			pointSizeFontSpec, charsetID, origCharset);
	}
}


static char *
fe_PropOrFixedFont(int i)
{
	return i == 0 ? "prop" : "fixed";
}


#define FE_INDEX_TO_FACE(face) (face == 0 ? "normal" : face == 1 ? "bold" : \
	face == 2 ? "italic" : "boldItalic")


#ifdef FE_PRINT_FONTS
static void
fe_PrintFamily(fe_FontFamily *family)
{
	fe_FontFace	*face;
	int		i;
	int		j;
	fe_FontSize	*size;

        (void) fprintf(real_stderr, "    %s\n", family->name);
	for (i = 0; i < family->numberOfPointSizes; i++)
	{
		size = &family->pointSizes[i];
		(void) fprintf(real_stderr, "      %d\n", size->size / 10);
		for (j = 0; j < 4; j++)
		{
			(void) fprintf(real_stderr, "        %s\n",
				FE_INDEX_TO_FACE(j));
			face = size->faces[j];
			(void) fprintf(real_stderr, "          %s\n",
					face->longXLFDFontName);
		}
	}
}


static void
fe_PrintFonts(void)
{
	fe_FontCharSet	*charset;
	int		i;
	int		j;
	int		k;
	fe_FontPitch	*pitch;

	for (i = 0; i < INTL_CHAR_SET_MAX; i++)
	{
		charset = &fe_FontCharSets[i];
		if (!charset->name)
		{
			continue;
		}
		(void) fprintf(real_stderr, "%s\n", charset->name);
		for (j = 0; j < 2; j++)
		{
			(void) fprintf(real_stderr, "  %s\n",
				fe_PropOrFixedFont(j));
			pitch = &charset->pitches[j];
			for (k = 0; k < pitch->numberOfFamilies; k++)
			{
				fe_PrintFamily(&pitch->families[k]);
			}
		}
	}
}
#endif /* FE_PRINT_FONTS */


static int
fe_CompareFontCharSets(unsigned char *charset1, unsigned char *charset2)
{
	char	*name1;
	char	*name2;

	name1 = fe_FontCharSets[*charset1].name;
	name2 = fe_FontCharSets[*charset2].name;

	if (!name1)
	{
		if (!name2)
		{
			return 0;
		}

		return 1;
	}

	if (!name2)
	{
		return -1;
	}

	return strcmp(name1, name2);
}


static int
fe_CompareFontFamilies(fe_FontFamily *family1, fe_FontFamily *family2)
{
	return strcmp(family1->name, family2->name);
}


static int
fe_CompareFontSizes(fe_FontSize *size1, fe_FontSize *size2)
{
	return size1->size - size2->size;
}


typedef int (*fe_CompFunc)(const void *, const void *);


static void
fe_SortFonts(void)
{
	fe_FontCharSet	*charset;
	fe_FontFamily	*family;
	int		i;
	int		j;
	int		k;
	fe_FontPitch	*pitch;


	for (i = 0; i < INTL_CHAR_SET_MAX; i++)
	{
		fe_SortedFontCharSets[i] = i;
	}
	XP_QSORT(fe_SortedFontCharSets, INTL_CHAR_SET_MAX,
			 sizeof(*fe_SortedFontCharSets),
			 (fe_CompFunc) fe_CompareFontCharSets);


	for (i = 0; i < INTL_CHAR_SET_MAX; i++)
	{
		charset = &fe_FontCharSets[i];
		if (!charset->name)
		{
			continue;
		}
		for (j = 0; j < 2; j++)
		{
			pitch = &charset->pitches[j];
			if (pitch->families)
			{
				XP_QSORT(pitch->families, pitch->numberOfFamilies,
						 sizeof(fe_FontFamily),
						 (fe_CompFunc) fe_CompareFontFamilies);
			}
			for (k = 0; k < pitch->numberOfFamilies; k++)
			{
				family = &pitch->families[k];
				XP_QSORT(family->pixelSizes,
						 family->numberOfPixelSizes,
						 sizeof(fe_FontSize),
						 (fe_CompFunc) fe_CompareFontSizes);
				XP_QSORT(family->pointSizes,
						 family->numberOfPointSizes,
						 sizeof(fe_FontSize),
						 (fe_CompFunc) fe_CompareFontSizes);
			}
		}
	}
}


void
fe_ReadFontCharSet(char *charsetname)
{
	fe_FontCharSet	*charset;
	int16		charsetID;
	int		i;

	charsetID = (INTL_CharSetNameToID(charsetname) & 0xff);
	if ((charsetID < 0) || (charsetID >= INTL_CHAR_SET_MAX))
	{
		charsetID = CS_LATIN1;
	}
	charsetID &= 0xff;

	for (i = 0; i < INTL_CHAR_SET_MAX; i++)
	{
		fe_FontCharSets[i].selected = 0;
	}

	charset = &fe_FontCharSets[charsetID];
	if (charset->name)
	{
		charset->selected = 1;
	}
	else
	{
		charset = NULL;
		for (i = 0; i < INTL_CHAR_SET_MAX; i++)
		{
			charset = &fe_FontCharSets[i];
			if (!charset->name)
			{
				charset = NULL;
				continue;
			}
			charset->selected = 1;
			break;
		}
	}
	if (!charset)
	{
		/* this should never happen */
		(void) fprintf(real_stderr, "no recognized font charsets!\n");
		exit(1);
	}
}


void
fe_ReadFontSpec(char *spec)
{
	fe_FontCharSet	*charset;
	char		*charSetName;
	fe_FontFamily	*family;
	char		*familyName;
	char		*foundry;
	int		i;
	int		j;
	int		k;
	int		m;
	char		*p;
	fe_FontPitch	*pitch;
	char		*pitchName;
	char		*s;
	char		*scaling;
	fe_FontSize	*size;
	char		*sizeName;
	int		sizeNum;

	s = strdup(spec);
	if (!s)
	{
		return;
	}

	p = s;

	foundry = p;
	p = strchr(p, '-');
	if (!p)
	{
		return;
	}
	*p++ = 0;

	familyName = p;
	p = strchr(p, '-');
	if (!p)
	{
		return;
	}
	*p++ = 0;

	sizeName = p;
	p = strchr(p, '-');
	if (!p)
	{
		return;
	}
	*p++ = 0;
	(void) sscanf(sizeName, "%d", &sizeNum);

	scaling = p;
	p = strchr(p, '-');
	if (!p)
	{
		return;
	}
	*p++ = 0;

	pitchName = p;
	p = strchr(p, '-');
	if (!p)
	{
		return;
	}
	*p++ = 0;

	charSetName = p;


	for (i = 0; i < INTL_CHAR_SET_MAX; i++)
	{
		charset = &fe_FontCharSets[i];
		if ((!charset->name) || strcmp(charset->mimeName, charSetName))
		{
			continue;
		}

		j = (*pitchName == 'p' ? 0 : 1);
		pitch = &charset->pitches[j];

		family = NULL;
		for (k = 0; k < pitch->numberOfFamilies; k++)
		{
			family = &pitch->families[k];

			if ((!strcmp(family->foundry, foundry)) &&
				(!strcmp(family->family, familyName)))
			{
				break;
			}
			else
			{
				family = NULL;
			}
		}
		if (family)
		{
			for (k = 0; k < pitch->numberOfFamilies; k++)
			{
				pitch->families[k].selected = 0;
			}
			family->selected = 1;
			pitch->selectedFamily = family;

			family->allowScaling = (((*scaling) == 's') ? 1 : 0);

			/*
			 * sanity check
			 */
			if (family->pointSizes->size)
			{
				family->allowScaling = 0;
			}
			else if (family->numberOfPointSizes == 1)
			{
				family->allowScaling = 1;
			}

			size = NULL;
			for (m = 0; m < family->numberOfPointSizes; m++)
			{
				size = &family->pointSizes[m];
				if (size->size == sizeNum)
				{
					break;
				}
				else
				{
					size = NULL;
				}
			}
			if (size)
			{
				for (m = 0; m < family->numberOfPointSizes; m++)
				{
					family->pointSizes[m].selected = 0;
				}
				size->selected = 1;
			}
			else if (family->allowScaling)
			{
				for (m = 0; m < family->numberOfPointSizes; m++)
				{
					family->pointSizes[m].selected = 0;
				}
				family->pointSizes[0].selected = 1;
				family->scaledSize = sizeNum;
			}
		}

		break;
	}

	free(s);
}


static fe_FontSize *
fe_GetScaledFontSize(fe_FontFamily *family, double idealSize, XP_Bool make_global)
{
	fe_FontFace	*face;
	int		i;
	fe_FontSize	*ret;

	ret = FE_MALLOC_ZAP(fe_FontSize);
	if (!ret)
	{
		return NULL;
	}
        if (make_global) {
            ret->next = fe_FontSizeList;
            fe_FontSizeList = ret;
        }

	ret->size = idealSize;

	for (i = 0; i < 4; i++)
	{
		ret->faces[i] = FE_MALLOC_ZAP(fe_FontFace);
		face = ret->faces[i];
		if (!face)
		{
			return NULL;
		}
                if (make_global) {
                    face->next = fe_FontFaceList;
                    fe_FontFaceList = face;
                }
		face->longXLFDFontName = PR_smprintf(
			family->pointSizes->faces[i]->longXLFDFontName,
			ret->size);
		if (!face->longXLFDFontName)
		{
			return NULL;
		}
	}

	return ret;
}


static fe_FontSize *
fe_FindFontPointSize(fe_FontFamily *family, double idealSize,
                     XP_Bool search_for_fit, XP_Bool scale_to_fit)
{
	int		i;
	double		minDist;
	fe_FontSize	*minSize;
	fe_FontSize	*maxSize;
	double		newDist;
	fe_FontSize	*ret;
	fe_FontSize	*size;

	ret = NULL;

        if (! family->pointSizes)
            return NULL;

	if (family->pointSizes->size == 0)
	{
		minSize = &family->pointSizes[1];
	}
	else
	{
		minSize = family->pointSizes;
	}
	maxSize = &family->pointSizes[family->numberOfPointSizes - 1];

	if ((idealSize < minSize->size) || (idealSize > maxSize->size))
	{
		if (idealSize > maxSize->size)
		{
			ret = maxSize;
		}
		if (idealSize < minSize->size)
		{
			ret = minSize;
		}
	}
	else
	{
                minDist = (1E+36);
		ret = family->pointSizes;
		for (i = 0; i < family->numberOfPointSizes; i++)
		{
			size = &family->pointSizes[i];
			newDist = ((idealSize - size->size) *
				   (idealSize - size->size));
			if (newDist < minDist)
			{
				minDist = newDist;
				ret = size;
			}
			else
			{
				break;
			}
		}
	}

        if (search_for_fit) {

            minDist =  (ret)
                ? ((idealSize - ret->size) * (idealSize - ret->size))
                : (1E+36);

            for (size = family->pool; size; size = size->next) {
                newDist = ((idealSize - size->size) *
                           (idealSize - size->size));
                if (newDist < minDist) {
                    minDist = newDist;
                    ret = size;
                }
            }
        }

        if (scale_to_fit) {

            /* We have deliberately first searched through the array
             * of HTML point sizes for a match within 1/2 point of the
             * requested point size because we prefer hand-tuned bitmaps
             * to scaled outlines, all other things being equal.
             */

            minDist =  (ret)
                ? ((idealSize - ret->size) * (idealSize - ret->size))
                : (1E+36);

            if (0 == family->pointSizes->size && minDist > 0.25) {

                size = fe_GetScaledFontSize(family, idealSize, False);
                if (size) {

                    /* mark to be freed when the context expires */
                    size->in_pool = 1;

                    /* hang off of the family */
                    size->next = family->pool;
                    family->pool = size;

                    ret = size;
                }
            }
        }

	return ret;
}


static fe_FontSize *
fe_ComputeFontSize(fe_FontFamily *family, double idealSize)
{
    if (family->allowScaling)
        return fe_GetScaledFontSize(family, idealSize, True);

    /* Pass False for scale_to_fit: fe_ComputeFontSize
     * is used only to compute the table of HTML sizes.  
     * In integrating the range of sizes allowed by CSS-1, we want
     * no perturbation of font sizes as they are instantiated through
     * HTML size requests.  Therefore, offer to scale outline fonts
     * in the construction of the HTML size table only when the
     * allowScaling flag is set for that particular family, as was
     * the case prior to CSS-1.
     */

    return fe_FindFontPointSize(family, idealSize, False, False);
}


void
fe_FreeFontGroups(void)
{
#if 0
	fe_FontCharSet	*charset;
	fe_FontFace	*face;
	int		i;
	int		j;
	int		k;
	int		m;
	fe_FontPitch	*pitch;
	fe_FontSize	*size;

	for (i = 0; i < INTL_CHAR_SET_MAX; i++)
	{
		charset = &fe_FontCharSets[i];
		if (fe_CharSetInfoArray[i].type == FE_FONT_TYPE_GROUP)
		{
			for (j = 0; j < 2; j++)
			{
				pitch = &charset->pitches[j];
				if (!pitch->selectedFamily)
				{
					continue;
				}
				for (k = 0; k < 7; k++)
				{
					size = pitch->selectedFamily->htmlSizes[k];
					if (size)
					{
						for (m = 0; m < 4; m++)
						{
							face = size->faces[m];
							if (face)
							{
								free(face);
							}
						}
						free(size);
					}
				}
				free(pitch->selectedFamily);
				pitch->selectedFamily = NULL;
			}
		}
	}
#endif
}

void
fe_FreeUnicodeFonts(void)
{
    fe_FontCharSet *charset;
    fe_FontFace *face;
    int i_pitch, i_htmlsize, i_face, i_encodings;
    fe_FontPitch *pitch;
    fe_FontSize *size;
    fe_UnicodePseudoFont *ufont;

    charset = &fe_FontCharSets[CS_UTF8&0xFF];
    for (i_pitch = 0; i_pitch < 2; i_pitch++) {
        pitch = &charset->pitches[i_pitch];
        if (!pitch->selectedFamily)
           continue;
        for (i_htmlsize = 0; i_htmlsize < 7; i_htmlsize++) {
            size = pitch->selectedFamily->htmlSizes[i_htmlsize];
            if (!size)
                continue;
            for (i_face = 0; i_face < 4; i_face++) {
                face = size->faces[i_face];
                if (!face)
                    continue;
                if (!face->font)
                    continue;
                ufont = (fe_UnicodePseudoFont *)face->font;
                if (ufont->tag != UNICODE_PSEUDO_FONT_TAG) {
                    continue;
                }
                for (i_encodings=0; i_encodings<INTL_CHAR_SET_MAX; 
                                                        i_encodings++) {
                    if (ufont->xfont_scaled[i_encodings]) {
                        XFreeFont(ufont->dpy, ufont->xFonts[i_encodings]);
						XP_FREE(ufont->xfont_name[i_encodings]);
                    }
                    ufont->xfont_inited[i_encodings] = FALSE;
                    ufont->xfont_scaled[i_encodings] = FALSE;
                    ufont->xfont_name[i_encodings] = NULL;
                    ufont->larger_fonts_avail[i_encodings] = FALSE;
					if (ufont->xmfontlist) 
						XmFontListFree(ufont->xmfontlist);
					ufont->xmfontlist = NULL;
					if (ufont->xm_fontset) 
						XmFontListFree(ufont->xm_fontset);
					ufont->xm_fontset = NULL;
					if (ufont->xfontset) 
						XFreeFontSet(ufont->dpy, ufont->xfontset);
					ufont->xfontset = NULL;
                }
            }
        }
    }
}

void
fe_FreeFontSizeTable(fe_FontFamily *family)
{
	int		k,m;
	fe_FontSize	*size;
	fe_FontFace	*face;

	if (!family->htmlSizesComputed) return;

	family->htmlSizesComputed = 0;

	/*
	 * Only the scaled fonts were allocated new sizes. Hence we free
	 * only them.
	 */
	if (!family->allowScaling) return;

	for (k = 0; k < 7; k++)
	{
		size = family->htmlSizes[k];
		if (size)
		{
			for (m = 0; m < 4; m++)
			{
				face = size->faces[m];
				if (face)
				{
					free(face);
				}
			}
			free(size);
			family->htmlSizes[k] = NULL;
		}
	}
}

void
fe_ComputeFontSizeTable(fe_FontFamily *family)
{
	int		i;
	double		idealSize;
	int		j;
	fe_FontSize	*size;
	int		size3;

	size = NULL;

	/*
	 * first, find the size selected within the family
	 */
	for (j = 0; j < family->numberOfPointSizes; j++)
	{
		size = &family->pointSizes[j];
		if (size->selected)
		{
			break;
		}
		else
		{
			size = NULL;
		}
	}

	if ((!family) || (!size))
	{
		/* this should not happen */
		return;
	}

	fe_FreeFontGroups();
	fe_FreeFontSizeTable(family);

	/*
	 * set the default size (3)
	 */
	size3 = size->size;
	if (!size3)
	{
		size3 = family->scaledSize;
	}
	if (!size3)
	{
		size3 = 120;
	}

	i = 3;
	idealSize = size3;
	family->htmlSizes[i - 1] = fe_ComputeFontSize(family, idealSize);

	/*
	 * set the higher sizes (4-7)
	 */
	for (i = 4; i <= 7; i++)
	{
		idealSize = ((1 + ((i - 3) * fe_FontSizeIncrement)) * size3);
		family->htmlSizes[i - 1] = fe_ComputeFontSize(family, idealSize);
	}

	/*
	 * set the lower sizes (1-2)
	 */
	for (i = 1; i <= 2; i++)
	{
		idealSize = ((1 - ((3 - i) * fe_FontSizeIncrement)) * size3);
		if (idealSize < 10)
		{
			idealSize = 10;
		}
		family->htmlSizes[i - 1] = fe_ComputeFontSize(family, idealSize);
	}

	/*
	 * mark family that htmlSizes has been computed.
	 */
	family->htmlSizesComputed = 1;

	/* @@@ need to implement this in case any of the scaled
	 * sizes couldn't be malloc'ed -- erik
	 * fe_CheckFonts();
	 */
}


static int
fe_CloneFamily(fe_FontFamily *cloneFamily, fe_FontFamily *origFamily)
{
	int	i;

	cloneFamily->name = strdup(origFamily->name);
	cloneFamily->foundry = strdup(origFamily->foundry);
	cloneFamily->family = strdup(origFamily->family);
	if ((!cloneFamily->name) || (!cloneFamily->foundry) ||
		(!cloneFamily->family))
	{
		return 0;
	}

	cloneFamily->selected = origFamily->selected;
	cloneFamily->allowScaling = origFamily->allowScaling;

	cloneFamily->pointAlloc = origFamily->pointAlloc;
	cloneFamily->numberOfPointSizes = origFamily->numberOfPointSizes;

	cloneFamily->pointSizes = XP_CALLOC(cloneFamily->pointAlloc,
		sizeof(fe_FontSize));
	if (!cloneFamily->pointSizes)
	{
		return 0;
	}

	for (i = 0; i < cloneFamily->numberOfPointSizes; i++)
	{
		cloneFamily->pointSizes[i] = origFamily->pointSizes[i];
	}

	if (origFamily->allowScaling)
	{
		/*
		 * For scaled families, take care to copy the sizes and faces.
		 * Setting this will make the sizes get recomputed.
		 */
		cloneFamily->htmlSizesComputed = 0;
	}
	else 
	{
		cloneFamily->htmlSizesComputed = origFamily->htmlSizesComputed;
		for (i = 0; i < 7; i++)
		{
			cloneFamily->htmlSizes[i] = origFamily->htmlSizes[i];
		}
	}
	return 1;
}


static int
fe_ClonePitch(fe_FontPitch *clonePitch, fe_FontPitch *origPitch)
{
	int	i;

	clonePitch->alloc = origPitch->alloc;
	clonePitch->numberOfFamilies = origPitch->numberOfFamilies;

	clonePitch->families = XP_CALLOC(clonePitch->alloc,
		sizeof(fe_FontFamily));
	if (!clonePitch->families)
	{
		return 0;
	}

	for (i = 0; i < clonePitch->numberOfFamilies; i++)
	{
		if (!fe_CloneFamily(&clonePitch->families[i],
			&origPitch->families[i]))
		{
			return 0;
		}
	}

	clonePitch->selectedFamily = NULL;

	return 1;
}


static void
fe_SetDefaultFontSettings(Display *dpy)
{
	fe_FontCharSet	*charset;
	char		*charsetname;
	char		clas[512];
	XrmDatabase	db;
	fe_FontFace	**faces;
	fe_FontFamily	*family;
	int		i;
	int		j;
	int		k;
	int		m;
	char		name[512];
	fe_FontPitch	*pitch;
	fe_FontSize	*size;
	char		*spec;
	char		*type;
	XrmValue	value;


	db = XtDatabase(dpy);


	/*
	 * try to get the default charset from the resources
	 */
	(void) PR_snprintf(clas, sizeof(clas),
		"%s.DocumentFonts.DefaultCharSet", fe_progclass);
	(void) PR_snprintf(name, sizeof(name),
		"%s.documentFonts.defaultCharSet", fe_progclass);
	if (XrmGetResource(db, name, clas, &type, &value))
	{
		charsetname = (char *) value.addr;
	}
	else
	{
		charsetname = "iso-8859-1";
	}
	fe_ReadFontCharSet(charsetname);


	/*
	 * try to get the default fonts from the resources
	 */
	for (i = 0; i < INTL_CHAR_SET_MAX; i++)
	{
		charset = &fe_FontCharSets[i];
		if (!charset->name)
		{
			continue;
		}
		for (j = 0; j < 2; j++)
		{
			(void) PR_snprintf(clas, sizeof(clas),
				"%s.DocumentFonts.DefaultFont.CharSet.Pitch",
				fe_progclass);
			(void) PR_snprintf(name, sizeof(name),
				"%s.documentFonts.defaultFont.%s.%s",
				fe_progclass, charset->mimeName,
				fe_PropOrFixedFont(j));
			if (XrmGetResource(db, name, clas, &type, &value))
			{
				spec = PR_smprintf("%s-%s-%s",
					(char *) value.addr,
					fe_PropOrFixedFont(j),
					charset->mimeName);
				if (!spec)
				{
					continue;
				}
				fe_ReadFontSpec(spec);
				free(spec);
			}
		}
	}


	/*
	 * make sure we have families set for each charset/pitch,
	 * and sizes set for each family
	 */
	for (i = 0; i < INTL_CHAR_SET_MAX; i++)
	{
		charset = &fe_FontCharSets[i];
		if (!charset->name)
		{
			continue;
		}
		for (j = 0; j < 2; j++)
		{
			pitch = &charset->pitches[j];
			for (k = 0; k < pitch->numberOfFamilies; k++)
			{
				family = &pitch->families[k];
				for (m = 0; m < family->numberOfPointSizes; m++)
				{
					faces = family->pointSizes[m].faces;
					if (!faces[0])
					{
						if (faces[1])
						{
							faces[0] = faces[1];
						}
						else if (faces[2])
						{
							faces[0] = faces[2];
						}
						else
						{
							faces[0] = faces[3];
						}
					}
					if (!faces[1])
					{
						if (faces[3])
						{
							faces[1] = faces[3];
						}
						else if (faces[2])
						{
							faces[1] = faces[2];
						}
						else
						{
							faces[1] = faces[0];
						}
					}
					if (!faces[2])
					{
						if (faces[3])
						{
							faces[2] = faces[3];
						}
						else if (faces[1])
						{
							faces[2] = faces[1];
						}
						else
						{
							faces[2] = faces[0];
						}
					}
					if (!faces[3])
					{
						if (faces[1])
						{
							faces[3] = faces[1];
						}
						else if (faces[2])
						{
							faces[3] = faces[2];
						}
						else
						{
							faces[3] = faces[0];
						}
					}
				}

				/*
				 * make sure a size is selected for each family
				 */
				size = NULL;
				for (m = 0; m < family->numberOfPointSizes; m++)
				{
					size = &family->pointSizes[m];
					if (size->selected)
					{
						break;
					}
					else
					{
						size = NULL;
					}
				}
				if ((!size) && family->numberOfPointSizes)
				{
					size = &family->pointSizes
						[(family->numberOfPointSizes+1)/2-1];
					size->selected = 1;
				}

				/*
				 * determine whether or not we allow scaling
				 * from the available sizes
				 */
				if ((family->numberOfPointSizes == 1) &&
					(family->pointSizes->size == 0))
				{
					family->allowScaling = 1;
				}
			}

			/*
			 * make sure a family is selected for each pitch
			 */
			family = NULL;
			for (k = 0; k < pitch->numberOfFamilies; k++)
			{
				family = &pitch->families[k];
				if (family->selected)
				{
					break;
				}
				else
				{
					family = NULL;
				}
			}
			if ((!family) && pitch->numberOfFamilies)
			{
				family = &pitch->families[0];
				family->selected = 1;
			}
			/* Force recomputation */
			if (family) fe_FreeFontSizeTable(family);
		}

		/*
		 * if some charset has fonts in one pitch, but not the
		 * other, we clone the one that is available, so that
		 * we always have a pitch available
		 */
		if (!charset->pitches[0].families)
		{
			if (!fe_ClonePitch(&charset->pitches[0],
				&charset->pitches[1]))
			{
				/*
				 * if we can't clone the pitch, we
				 * disable the whole charset
				 */
				free(charset->name);
				charset->name = NULL;
			}
		}
		else if (!charset->pitches[1].families)
		{
			if (!fe_ClonePitch(&charset->pitches[1],
				&charset->pitches[0]))
			{
				/*
				 * if we can't clone the pitch, we
				 * disable the whole charset
				 */
				free(charset->name);
				charset->name = NULL;
			}
		}
	}
}


static void
fe_ReportFontCharSets(void)
{
	fe_FontCharSet	*charset;
	int16		*charsets;
	int		i;
	int		j;

	charsets = calloc(INTL_CHAR_SET_MAX, sizeof(*charsets));
	if (!charsets)
	{
		return;
	}

	j = 0;
	for (i = 1; i < INTL_CHAR_SET_MAX; i++)
	{
		charset = &fe_FontCharSets[i];
		if (!charset->name)
		{
			continue;
		}
		charsets[j++] = fe_CharSetInfoArray[i].charsetID;
	}

	INTL_ReportFontCharSets(charsets);
}


static fe_Font
fe_LoadXFont(Display *dpy, char *name)
{
	XFontStruct	*f;

	f = XLoadQueryFont(dpy, name);

	if (f)
	{
		/*
		 * The width check is from xemacs.
		 * The ascent check is from Sun's AWT.
		 */
		if ((!f->max_bounds.width) || (f->ascent < 0))
		{
			XFreeFont(dpy, f);
			f = NULL;
		}
	}

	return (fe_Font) f;
}

static float default_UnicodePseudoFontSizes[] = {
    0.0, 
    8.0,   9.0, 10.0, 11.0, 12.0, 14.0, 
    16.0, 18.0, 20.0, 22.0, 24.0, 28.0, 
    32.0, 36.0,
    48.0, 60.0, 72.0,
};

static float* UnicodePseudoFontSizes = default_UnicodePseudoFontSizes;
static int num_UnicodePseudoFontSizes = 
			sizeof(default_UnicodePseudoFontSizes)/sizeof(float*);

static char *
fe_generatePixelSizeFontSpec(
		int  pixelSizeNum,
		char *origFoundry,
		char *origFamily,
		char *origWeight,
		char *avgwidth,
		char *origCharset)
{
	char *pixelSizeFontSpec;

	/*
	 * Note:
	 * origWeight has 2 values
	 * origCharset has 2 values
	 */
#ifdef SOLARIS
		if (pixelSizeNum)
		{
			pixelSizeFontSpec = PR_smprintf(
				"-%s-%s-%s-*-*-%d-*-*-*-*-%s-%s", origFoundry,
				origFamily, origWeight, pixelSizeNum, avgwidth,
				origCharset);
		}
		else
		{
			pixelSizeFontSpec = PR_smprintf(
				"-%s-%s-%s-*-*-%%d-*-*-*-*-%s-%s", origFoundry,
				origFamily, origWeight, avgwidth, origCharset);
		}
#else /* SOLARIS */
		if (pixelSizeNum)
		{
			pixelSizeFontSpec = PR_smprintf(
				"-%s-%s-%s-*-*-%d-*-*-*-*-*-%s", origFoundry,
				origFamily, origWeight, pixelSizeNum,
				origCharset);
		}
		else
		{
			pixelSizeFontSpec = PR_smprintf(
				"-%s-%s-%s-*-*-%%d-*-*-*-*-*-%s", origFoundry,
				origFamily, origWeight, origCharset);
		}
#endif /* SOLARIS */
	return pixelSizeFontSpec;

}

static char *
fe_generatePointSizeFontSpec(
		int  pointSizeNum,
		char *origFoundry,
		char *origFamily,
		char *origWeight,
		char *xResolution,
		char *yResolution,
		char *avgwidth,
		char *origCharset)
{
	char *pointSizeFontSpec;

	/*
	 * Note:
	 * origWeight has 2 values
	 * origCharset has 2 values
	 */
#ifdef SOLARIS
		if (pointSizeNum)
		{
			pointSizeFontSpec = PR_smprintf(
				"-%s-%s-%s-*-*-*-%d-%s-%s-*-%s-%s",
				origFoundry, origFamily, origWeight,
				pointSizeNum, xResolution, yResolution,
				avgwidth, origCharset);
		}
		else
		{
			pointSizeFontSpec = PR_smprintf(
				"-%s-%s-%s-*-*-*-%%d-%s-%s-*-%s-%s",
				origFoundry, origFamily, origWeight,
				xResolution, yResolution, avgwidth,
				origCharset);
		}
#else /* SOLARIS */
		if (pointSizeNum)
		{
			pointSizeFontSpec = PR_smprintf(
				"-%s-%s-%s-*-*-*-%d-%s-%s-*-*-%s", origFoundry,
				origFamily, origWeight, pointSizeNum,
				xResolution, yResolution, origCharset);
		}
		else
		{
			pointSizeFontSpec = PR_smprintf(
				"-%s-%s-%s-*-*-*-%%d-%s-%s-*-*-%s",
				origFoundry, origFamily, origWeight,
				xResolution, yResolution, origCharset);
		}
#endif /* SOLARIS */
	return pointSizeFontSpec;

}

/*
 * fe_loadUnicodePseudoFonts
 *
 * At this time, Sept. 1996, Unicode fonts are not
 * available for Unix. To display (size) a Unicode string
 * we convert the Unicode string to a series substrings each
 * of which is converted in a locally displayable encoding.
 * We then display (size) each substring using a standard
 * X font from a this Unicode Pseudo font.
 *
 * A Unicode Pseudo font is therefore an array of X fonts
 * each of the given size/pitch/weight for this font with one
 * one X font per encoding.
 *
 * In loading these Unicode Pseudo fonts we pretend to
 * to have these Unicode fonts available and fill in the master 
 * font table. Actually checking for a X font for each
 * for each size/pitch/weight/encoding would require loading 
 * a huge number of fonts.
 *
 *    11 sizes * 2 pitches * 2 weights * 35 encodings = 1540 fonts
 *
 * Because of the large number of fonts and the fact that asian 
 * fonts are large (1M per font) and the fact we might ask for 
 * scaling this loading time could be in the order of 
 * several minutes.  During this font loading time the X server 
 * appears to freeze and this is quite distressing to the user.
 *
 * HENCE BECAUSE OF THE UNACCEPTABLE LOADING DELAYS 
 * WE JUST ASSUME THE UNICODE FONTS ARE AVAILABLE.
 *
 * Even when we begin using a Unicode Pseudo font we only
 * load the X fonts for the encodings that have actually been 
 * seen.  Just loading X fonts for all the encodings for a given
 * size/pitch/weight requires a large number of X fonts.
 * 
 *    11 sizes * 2 pitches * 2 weights = 44 X fonts
 *
 */
static void
fe_loadUnicodePseudoFonts(Display *dpy, char *my_origFamily, int my_pitch)
{
    int i, j;
    char *my_origFoundry = "nsPseudoFont";
    char *my_foundry = my_origFoundry;
    char *my_family = my_origFamily;
    int   my_face;
    int   my_pointSizeNum;
    int   my_pixelSizeNum;
    int16 my_charsetID = CS_UTF8 & 0xFF;
    char *my_origCharset = "unicode-2-0";
    char *my_charset = my_origCharset;
    char *my_charsetlang = "unicode-2-0";
    char *my_pixelSizeFontSpec = NULL; 
    char *my_pointSizeFontSpec = NULL;
    char *my_origWeight;
    char *my_avgwidth = "*";
    char *my_xResolution = "*";
    char *my_yResolution = "*";
    char name[512], clas[512];
    char *type;
    XrmValue value;
    XrmDatabase	db = XtDatabase(dpy);

    /* Family name */
    (void) PR_snprintf(name, sizeof(name),
             "%s.documentFonts.family.%s", fe_progclass, my_origFamily);
    (void) PR_snprintf(clas, sizeof(clas),
                "%s.DocumentFonts.Family.Name", fe_progclass);
    if (XrmGetResource(db, name, clas, &type, &value))
        my_family = (char *) value.addr;

    /* Foundry name */
    (void) PR_snprintf(name, sizeof(name),
             "%s.documentFonts.foundry.%s", fe_progclass, my_origFoundry);
    (void) PR_snprintf(clas, sizeof(clas),
                "%s.DocumentFonts.Foundry.Name", fe_progclass);
    if (XrmGetResource(db, name, clas, &type, &value))
        my_foundry = (char *) value.addr;

    /* CharsetLang name */
    (void) PR_snprintf(name, sizeof(name),
             "%s.documentFonts.charsetlang.%s", fe_progclass, my_charsetlang);
    (void) PR_snprintf(clas, sizeof(clas),
                "%s.DocumentFonts.CharSetLang.Name", fe_progclass);
    if (XrmGetResource(db, name, clas, &type, &value))
        my_charsetlang = (char *) value.addr;

    /*
     * Create the Pseudo font sizes
     */

    for (i=0; i<num_UnicodePseudoFontSizes; i++) {
        /* one of these is not quite right */
        my_pixelSizeNum = (int)(UnicodePseudoFontSizes[i] * 10);
        my_pointSizeNum = (int)(UnicodePseudoFontSizes[i] * 10);
        /*
         * for each size create a plain/bold/italic/boldItalic version
         * (0=plain, 1=bold, 2=italic, 3=boldItalic)
         */
        for (j=0; j<4; j++) {
            my_face = j;
            switch(j) {
                default:
                case 0:
                    my_origWeight = "medium-r";
                    break;
                case 1:
                    my_origWeight = "bold-r";
                    break;
                case 2:
                    my_origWeight = "medium-i";
                    break;
                case 3:
                    my_origWeight = "bold-i";
                    break;
            };

            /*
             * Proportional/Monospaced
             */
            my_pixelSizeFontSpec = fe_generatePixelSizeFontSpec(
                                            my_pixelSizeNum,
                                            my_origFoundry,
                                            my_origFamily,
                                            my_origWeight,
                                            my_avgwidth,
                                            my_origCharset);
            if (!my_pixelSizeFontSpec)
                continue;
            my_pointSizeFontSpec = fe_generatePointSizeFontSpec(
                                        my_pointSizeNum,
                                        my_origFoundry,
                                        my_origFamily,
                                        my_origWeight,
                                        my_xResolution,
                                        my_yResolution,
                                        my_avgwidth,
                                        my_origCharset);
            if (!my_pointSizeFontSpec)
                continue;

            fe_AddFontCharSet(
                    my_origFoundry, 
                    my_foundry,
                    my_origFamily,
                    my_family,
                    my_face,
                    my_pixelSizeNum,
                    my_pointSizeNum,
                    my_pitch,
                    my_charsetID,
                    my_charset,
                    my_charsetlang,
                    my_pixelSizeFontSpec,
                    my_pointSizeFontSpec,
                    my_origCharset);
            FE_MAYBE_FREE(my_pixelSizeFontSpec);
            FE_MAYBE_FREE(my_pointSizeFontSpec);
        }
    }
}

#ifndef NO_WEB_FONTS

#define WF_FONT_DISPLAYER_PATH_1 "~/.netscape/dynfonts"
#define WF_FONT_DISPLAYER_PATH_4 "/usr/local/lib/netscape/dynfonts"

/* This static is outside of fe_InitializeWebfonts() 'cause HPUX has a
 * bug that causes it to become a loaded error with
 *	ld: Invalid loader fixup for symbol
 */
static char *dynfonts_str = "/dynfonts";
static char *dynfonts_str_no_slash = "dynfonts";

static void
fe_InitializeWebfonts(void)
{
	struct nffbp *fbp;
	char buf[MAXPATHLEN];
	char *mozilla_home = NULL;
	char *home = NULL;

	/* Initialize and obtain the font broker and font utility provider. */
	fe_FontBroker = NF_FontBrokerInitialize();
	fe_FontUtility = (struct nffbu *)
		nffbc_getInterface(fe_FontBroker, &nffbu_ID, NULL);

	/* Load Catalog */
	if (home = getenv("HOME"))
	{
		/* Form "$HOME/.netscape/dynfonts/fonts.cat" into buf */
		strncpy(buf, home, sizeof(buf)-1);
		strncat(buf, "/.netscape/dynfonts/fonts.cat",
				sizeof(buf)-1 - strlen(buf));
		buf[sizeof(buf)-1] = '\0';
		nffbu_LoadCatalog(fe_FontUtility, buf, NULL);
	}

	/* Load all font displayers */
	fbp = (struct nffbp *)
		nffbc_getInterface(fe_FontBroker, &nffbp_ID, NULL);

	/* Scan for font displayers. Here is the order and list of places
	 * where we look for font displayers.
	 *	~/.netscape/dynfonts
	 *	$MOZILLA_HOME/dynfonts
	 *	<program directory>/dynfonts
	 *	/usr/local/lib/netscape/dynfonts
	 *
	 * According to our webfont implementation, if a font displayer with
	 * the same name exists in more than one directory, then the
	 * first one takes priority.
	 */
	nffbp_ScanForFontDisplayers(fbp, WF_FONT_DISPLAYER_PATH_1, NULL);

	/* WF_FONT_DISPLAYER_PATH_2 */
	if (mozilla_home = getenv("MOZILLA_HOME"))
	{
		/* Form "$MOZILLA_HOME/dynfonts" into buf */
		strncpy(buf, mozilla_home, sizeof(buf)-1);
		strncat(buf, dynfonts_str, sizeof(buf)-1 - strlen(buf));
		buf[sizeof(buf)-1] = '\0';
		nffbp_ScanForFontDisplayers(fbp, buf, NULL);
	}

	/* WF_FONT_DISPLAYER_PATH_3 */
	/* Form "<program-dir>/dynfonts" into buf */
	buf[0] = '\0';
	fe_GetProgramDirectory(buf, sizeof(buf)-1);
	if (buf[0])
	{
		strncat(buf, dynfonts_str_no_slash, sizeof(buf)-1 - strlen(buf));
		buf[sizeof(buf)-1] = '\0';
		nffbp_ScanForFontDisplayers(fbp, buf, NULL);
	}

	/* WF_FONT_DISPLAYER_PATH_4 */
	nffbp_ScanForFontDisplayers(fbp, WF_FONT_DISPLAYER_PATH_4, NULL);
}
#endif

void
fe_InitFonts(Display *dpy)
{
	char		*avgwidth = NULL;
#ifdef SOLARIS
	int		avgWidthNum;
#endif
	char		*charset;
	int16		charsetID;
	char		*charsetlang;
	char		clas[512];
	int		count;
	XrmDatabase	db;
	int		face;
	char		*family;
	char		*font;
	char		**fonts;
	char		*foundry;
	int		i;
	int		j;
	char		name[512];
	char		*origCharset;
	char		*origFamily;
	char		*origFoundry;
	char		*origWeight; /* note: has weight and slant */
	char		*p;
	int		pitch;
	char		*pixelSize;
	char		*pixelSizeFontSpec;
	int		pixelSizeNum;
	char		*pointSize;
	char		*pointSizeFontSpec;
	int		pointSizeNum;
	char		*resx;
	char		*setwidth;
	char		*spacing;
	char		*type;
	XrmValue	value;
	char		*weight;
	char		*xResolution;
	char		*yResolution;

#ifdef SOLARIS
	avgwidth = NULL;
#endif
	charset = NULL;
	charsetlang = NULL;
	family = NULL;
	font = NULL;
	fonts = NULL;
	foundry = NULL;
	origCharset = NULL;
	origFamily = NULL;
	origFoundry = NULL;
	origWeight = NULL;
	p = NULL;
	pixelSize = NULL;
	pixelSizeFontSpec = NULL;
	pointSize = NULL;
	pointSizeFontSpec = NULL;
	resx = NULL;
	setwidth = NULL;
	spacing = NULL;
	type = NULL;
	weight = NULL;
	xResolution = NULL;
	yResolution = NULL;

#ifndef NO_WEB_FONTS
	/* Initialize font library */
	fe_InitializeWebfonts();
#endif
	/*
	 * do this first so that we are very likely to have this
	 * font available if/when we fail to load any other font
	 */
	fe_FixedFont = fe_LoadXFont(dpy, "fixed");

	fe_GetLocaleCharSets(dpy);

#ifdef DEBUG
	if (sizeof(fe_CharSetInfoArray) / sizeof(*fe_CharSetInfoArray) !=
		INTL_CHAR_SET_MAX)
	{
		(void) fprintf(real_stderr,
			"fe_CharSetInfoArray broken (size)\n");
#if 0 
		/* to fix 93446
		 */
		fe_Exit(1);
#endif
	}
	for (i = 0; i < INTL_CHAR_SET_MAX; i++)
	{
		if ((fe_CharSetInfoArray[i].charsetID & 0xff) != i)
		{
			(void) fprintf(real_stderr,
				"fe_CharSetInfoArray broken (%d)\n", i);
#if 0 
			/* to fix 93446
			 */
			fe_Exit(1);
#endif
		}
	}
#endif /* DEBUG */

	fonts = XListFonts(dpy, "-*-*-*-*-*-*-*-*-*-*-*-*-*-*", INT_MAX,
                           &count);
	if (!fonts)
	{
		return;
	}

	(void) memset(fe_FontCharSets, 0, sizeof(fe_FontCharSets));

        db = XtDatabase(dpy);

	for (i = 0; i < count; i++)
	{
		FE_MAYBE_FREE(font);
		FE_MAYBE_FREE(origCharset);
		FE_MAYBE_FREE(origFamily);
		FE_MAYBE_FREE(origWeight);
		FE_MAYBE_FREE(pixelSizeFontSpec);
		FE_MAYBE_FREE(pointSizeFontSpec);

		/* ignore the non-XLFD short names */
		if ((!fonts[i]) || (fonts[i][0] != '-'))
		{
			continue;
		}

		font = strdup(fonts[i]);
		if (!font)
		{
			continue;
		}

		p = font;
		j = 0;
		while (1)
		{
			p = strchr(p, '-');
			if (!p)
			{
				break;
			}
			p++;
			switch (j)
			{
			case 0:
				foundry = p;
				break;
			case 1:
				family = p;
				break;
			case 2:
				weight = p;
				break;
			case 4:
				setwidth = p;
				break;
			case 6:
				pixelSize = p;
				break;
			case 7:
				pointSize = p;
				break;
			case 8:
				resx = p;
				break;
			case 10:
				spacing = p;
				break;
#ifdef SOLARIS
			case 11:
				avgwidth = p;
				break;
#endif
			case 12:
				charset = p;
				break;
			default:
				break;
			}
			j++;
		}

		if (j < 14)
		{
			/* skip non-XLFD names */
			continue;
		}

                /* Skip Applix fonts -- if these are installed, there are
                   hundreds of them, and they do not have proper XLFD names:
                   the bold/italic/etc versions of the fonts have different
                   *families* as well ("axhvr" for Helvetica-normal,
                   "axhvb" for Helvetica-bold, etc) so all they do is bloat
                   the menus and make the UI unusable.
                 */
                if (family &&
                    strncasecomp(family, "ax", 2) &&
                    XP_STRLEN(family) == 5)
                    continue;

		(void) PR_snprintf(clas, sizeof(clas),
			"%s.DocumentFonts.Foundry.Name", fe_progclass);
		family[-1] = 0;
		(void) PR_snprintf(name, sizeof(name),
			"%s.documentFonts.foundry.%s", fe_progclass, foundry);
		origFoundry = strdup(foundry);
		if (!origFoundry)
		{
			continue;
		}
		if (XrmGetResource(db, name, clas, &type, &value))
		{
			foundry = strdup((char *) value.addr);
		}
		else
		{
			foundry = fe_CapitalizeFontName(foundry);
		}
		if (!foundry)
		{
			continue;
		}

		(void) PR_snprintf(clas, sizeof(clas),
			"%s.DocumentFonts.Family.Name", fe_progclass);
		weight[-1] = 0;
		(void) PR_snprintf(name, sizeof(name),
			"%s.documentFonts.family.%s", fe_progclass, family);
		origFamily = strdup(family);
		if (!origFamily)
		{
			continue;
		}
		if (XrmGetResource(db, name, clas, &type, &value))
		{
			family = strdup((char *) value.addr);
		}
		else
		{
			family = fe_CapitalizeFontName(family);
		}
		if (!family)
		{
			continue;
		}

		(void) PR_snprintf(clas, sizeof(clas),
			"%s.DocumentFonts.Face.Name", fe_progclass);
		setwidth[-1] = 0;
		(void) PR_snprintf(name, sizeof(name),
			"%s.documentFonts.face.%s", fe_progclass, weight);
		origWeight = strdup(weight);
		if (!origWeight)
		{
			continue;
		}
		if (XrmGetResource(db, name, clas, &type, &value))
		{
			weight = (char *) value.addr;
		}
		else
		{
#ifdef DEBUG_xxx
			(void) fprintf(real_stderr, "face %s\n", weight);
#endif /* DEBUG_xxx */
			weight = "";
		}
		if (!strcmp(weight, "bold"))
		{
			face = 1;
		}
		else if (!strcmp(weight, "boldItalic"))
		{
			face = 3;
		}
		else if (!strcmp(weight, "italic"))
		{
			face = 2;
		}
		else
		{
			face = 0;
		}

		pointSize[-1] = 0;
		sscanf(pixelSize, "%d", &pixelSizeNum);
		if (pixelSizeNum < 0)
		{
			continue;
		}

		resx[-1] = 0;
		sscanf(pointSize, "%d", &pointSizeNum);
		if (pointSizeNum < 0)
		{
			continue;
		}

		switch (spacing[0])
		{
		case 'm':
			pitch = fe_FONT_PITCH_MONO;
			break;
		case 'p':
			pitch = fe_FONT_PITCH_PROPORTIONAL;
			break;
		case 'c':
			/* fall through */
		default:
			pitch = fe_FONT_PITCH_BOTH;
			break;
		}

#ifdef SOLARIS
		charset[-1] = 0;
		sscanf(avgwidth, "%d", &avgWidthNum);
		if (avgWidthNum < 0)
		{
			continue;
		}
#endif

		(void) PR_snprintf(clas, sizeof(clas),
			"%s.DocumentFonts.CharSet.Family.Name", fe_progclass);
		(void) PR_snprintf(name, sizeof(name),
			"%s.documentFonts.charset.%s.%s", fe_progclass,
			origFamily, charset);
		origCharset = strdup(charset);
		if (!origCharset)
		{
			continue;
		}
		charsetlang = charset;
		if (XrmGetResource(db, name, clas, &type, &value))
		{
			charset = (char *) value.addr;
		}
		else
		{
			/*
			 * if the user has messed with the app-defaults, we
			 * might not be able to find our mapping, so check for
			 * the following at least
			 */
			if (!strcmp(charset, "iso8859-1"))
			{
				charset = "iso-8859-1";
			}
#ifdef DEBUG
			else
			{
				(void) fprintf(real_stderr, "charset %s %s\n",
					origFamily, charsetlang);
			}
#endif /* DEBUG */
		}
		charsetID = (INTL_CharSetNameToID(charset) & 0xff);

#ifdef SOLARIS
		if (charsetID == (CS_GB2312 & 0xff))
		{
			/*
			 * Simplified Chinese Solaris has broken fonts that
			 * say GB2312 but are actually ASCII fonts. Skip 'em.
			 */
			if (avgWidthNum && (avgWidthNum == (pointSizeNum / 2)))
			{
				continue;
			}
		}
		else
		{
			avgwidth = "*";
		}
#endif /* SOLARIS */

#ifdef DEBUG
		if ((charsetID == CS_UNKNOWN) && strcmp(charset, "x-ignore"))
		{
			(void) fprintf(real_stderr, "internal %s %s %s\n",
				origFamily, charsetlang, charset);
		}
#endif /* DEBUG */
		if (!strcmp(charset, "x-ignore"))
		{
			charsetID = CS_USRDEF2;
		}
		if (charsetID >= INTL_CHAR_SET_MAX)
		{
			charsetID = CS_USRDEF2;
		}
		if (fe_CharSetInfoArray[charsetID].type == FE_FONT_TYPE_GROUP)
		{
			continue;
		}
		(void) PR_snprintf(clas, sizeof(clas),
			"%s.DocumentFonts.CharSetLang.Name", fe_progclass);
		(void) PR_snprintf(name, sizeof(name),
			"%s.documentFonts.charsetlang.%s", fe_progclass,
			charset);
		if (XrmGetResource(db, name, clas, &type, &value))
		{
			charsetlang = (char *) value.addr;
		}
		else
		{
#if 0
			(void) fprintf(real_stderr, "charsetlang %s\n",
				charsetlang);
#endif /* 0 */
			charsetlang = XP_GetString(XFE_OTHER_LANGUAGE);
		}

		(void) PR_snprintf(clas, sizeof(clas),
			"%s.DocumentFonts.XResolution.CharSet", fe_progclass);
		(void) PR_snprintf(name, sizeof(name),
			"%s.documentFonts.xResolution.%s", fe_progclass,
			charset);
		if (XrmGetResource(db, name, clas, &type, &value))
		{
			xResolution = (char *) value.addr;
		}
		else
		{
			xResolution = "*";
		}

		(void) PR_snprintf(clas, sizeof(clas),
			"%s.DocumentFonts.YResolution.CharSet", fe_progclass);
		(void) PR_snprintf(name, sizeof(name),
			"%s.documentFonts.yResolution.%s", fe_progclass,
			charset);
		if (XrmGetResource(db, name, clas, &type, &value))
		{
			yResolution = (char *) value.addr;
		}
		else
		{
			yResolution = "*";
		}

		pixelSizeFontSpec = fe_generatePixelSizeFontSpec(
								pixelSizeNum,
								origFoundry,
								origFamily,
								origWeight,
								avgwidth,
								origCharset);
		if (!pixelSizeFontSpec)
		{
			continue;
		}

		pointSizeFontSpec = fe_generatePointSizeFontSpec(
								pointSizeNum,
								origFoundry,
								origFamily,
								origWeight,
								xResolution,
								yResolution,
								avgwidth,
								origCharset);
		if (!pointSizeFontSpec)
		{
			continue;
		}

		fe_AddFontCharSet(origFoundry, foundry, origFamily, family,
			face, pixelSizeNum, pointSizeNum, pitch, charsetID,
			charset, charsetlang, pixelSizeFontSpec,
			pointSizeFontSpec, origCharset);

#if 0
(void) fprintf
(
	real_stderr,
	"\"%s\" \"%s\" \"%s\" %d \"%s%s\" %d \"%s\" \"%s\"\n\"%s\"\n",
	foundry,
	family,
	FE_INDEX_TO_FACE(face),
	pointSizeNum,
	((pitch & fe_FONT_PITCH_MONO) ? "m" : ""),
	((pitch & fe_FONT_PITCH_PROPORTIONAL) ? "p" : ""),
	charsetID,
	charset,
	charsetlang,
	pointSizeFontSpec
);
#endif /* 0 */

		free(origFoundry);
		free(foundry);
		free(family);
	}

	FE_MAYBE_FREE(font);
	FE_MAYBE_FREE(origCharset);
	FE_MAYBE_FREE(origFamily);
	FE_MAYBE_FREE(origWeight);
	FE_MAYBE_FREE(pixelSizeFontSpec);
	FE_MAYBE_FREE(pointSizeFontSpec);

	XFreeFontNames(fonts);

	(void) PR_snprintf(clas, sizeof(clas),
		"%s.DocumentFonts.SizeIncrement", fe_progclass);
	(void) PR_snprintf(name, sizeof(name),
		"%s.documentFonts.sizeIncrement", fe_progclass);
	if (XrmGetResource(db, name, clas, &type, &value))
	{
		fe_FontSizeIncrement = (((double) atoi(value.addr)) / 100.0);
		if (fe_FontSizeIncrement <= 0)
		{
			fe_FontSizeIncrement = FE_DEFAULT_FONT_SIZE_INCREMENT;
		}
	}

	(void) PR_snprintf(clas, sizeof(clas),
                           "%s.DocumentFonts.MaximumPoints", fe_progclass);
	(void) PR_snprintf(name, sizeof(name),
                           "%s.documentFonts.maximumPoints", fe_progclass);
	if (XrmGetResource(db, name, clas, &type, &value))
	{
            fe_FontMaximumPoints = atoi(value.addr);
            if (fe_FontMaximumPoints <= 0)
                fe_FontMaximumPoints = FE_DEFAULT_FONT_MAXIMUM_POINTS;
	}

        for (i=0; i < fe_GenericFamilyCount; i++) {
            (void) PR_snprintf(clas, sizeof(clas),
                               "%s.DocumentFonts.Generic.%s",
                               fe_progclass, fe_generics[i].class_name);
            (void) PR_snprintf(name, sizeof(name),
                               "%s.documentFonts.generic.%s",
                               fe_progclass, fe_generics[i].generic_name);
            if (XrmGetResource(db, name, clas, &type, &value))
                fe_generics[i].family_name = strdup(value.addr);
        }

	fe_loadUnicodePseudoFonts(dpy, "helvetica", fe_FONT_PITCH_PROPORTIONAL);
	fe_loadUnicodePseudoFonts(dpy, "times",     fe_FONT_PITCH_PROPORTIONAL);
	fe_loadUnicodePseudoFonts(dpy, "courier",   fe_FONT_PITCH_MONO);
	fe_loadUnicodePseudoFonts(dpy, "fixed",     fe_FONT_PITCH_MONO);

	fe_SortFonts();

	fe_SetDefaultFontSettings(dpy);

	fe_ReportFontCharSets();

#ifdef FE_PRINT_FONTS
	fe_PrintFonts();
#endif
}


#ifdef DEBUG
void
fe_FreeFonts(void)
{
	fe_FontCharSet	*charset;
	fe_FontFace	*face;
	fe_FontFamily	*family;
	int		i;
	int		j;
	int		k;
	fe_FontFace	*nextFace;
	fe_FontPitch	*pitch;

	for (i = 0; i < INTL_CHAR_SET_MAX; i++)
	{
		charset = &fe_FontCharSets[i];
		if (!charset->name)
		{
			continue;
		}
		for (j = 0; j < 2; j++)
		{
			pitch = &charset->pitches[j];
			for (k = 0; k < pitch->numberOfFamilies; k++)
			{
				family = &pitch->families[k];
				free(family->name);
				free(family->foundry);
				free(family->family);
				free(family->pixelSizes);
				free(family->pointSizes);
			}
			free(pitch->families);
		}
		free(charset->name);
		free(charset->mimeName);
	}

	(void) memset(fe_FontCharSets, 0, sizeof(fe_FontCharSets));

	face = fe_FontFaceList;
	while (face)
	{
		nextFace = face->next;
		if (face->longXLFDFontName)
		{
			free(face->longXLFDFontName);
		}
		if (face->font)
		{
			XFreeFont(fe_display, face->font);
		}
		face->loaded = FALSE;
		free(face);
		face = nextFace;
	}
	fe_FontFaceList = NULL;

	if (fe_FixedFont)
	{
		XFreeFont(fe_display, fe_FixedFont);
		fe_FixedFont = NULL;
	}
}
#endif /* DEBUG */


static void
fe_AddFontSetting(fe_FontSettings **ret, fe_FontCharSet *charset, int i,
	fe_FontFamily *family, fe_FontSize *size)
{
	fe_FontSettings	*set;
	char		*spec;

	spec = PR_smprintf
	(
		"%s-%s-%d-%s-%s-%s\n",
		family->foundry,
		family->family,
		size->size,
		family->allowScaling ? "scale" : "noscale",
		fe_PropOrFixedFont(i),
		charset->mimeName
	);

	set = FE_MALLOC_ZAP(fe_FontSettings);

	if ((!spec) || (!set))
	{
		return;
	}

	set->spec = spec;
	set->next = *ret;
	*ret = set;
}


fe_FontSettings *
fe_GetFontSettings(void)
{
	fe_FontCharSet	*charset;
	fe_FontFamily	*family;
	int		i;
	int		j;
	int		k;
	int		m;
	fe_FontPitch	*pitch;
	fe_FontSettings	*ret;
	fe_FontSize	*size;

	ret = NULL;

	for (i = 0; i < INTL_CHAR_SET_MAX; i++)
	{
		charset = &fe_FontCharSets[i];
		if (!charset->name)
		{
			continue;
		}
		for (j = 0; j < 2; j++)
		{
			pitch = &charset->pitches[j];
			for (k = 0; k < pitch->numberOfFamilies; k++)
			{
				family = &pitch->families[k];
				if (family->selected)
				{
					for (m = 0; m < family->numberOfPointSizes;
						m++)
					{
						size = &family->pointSizes[m];
						if (size->selected)
						{
							fe_AddFontSetting
							(
								&ret,
								charset,
								j,
								family,
								size
							);
						}
					}
				}
			}
		}
	}

	return ret;
}


void
fe_SetFontSettings(fe_FontSettings *set)
{
}


char *
fe_GetFontCharSetSetting(void)
{
	fe_FontCharSet	*charset;
	int		i;

	for (i = 0; i < INTL_CHAR_SET_MAX; i++)
	{
		charset = &fe_FontCharSets[i];
		if (!charset->name)
		{
			continue;
		}
		if (charset->selected)
		{
			return charset->mimeName;
		}
	}

	return "";
}


void
fe_FreeFontSettings(fe_FontSettings *set)
{
	fe_FontSettings	*next;

	while (set)
	{
		next = set->next;
		free(set->spec);
		free(set);
		set = next;
	}
}


int
fe_GetStrikePosition(int charset, fe_Font font)
{
	int		ascent;
	int		descent;
	XCharStruct	overall;

	/*
	 * strike through the middle of lower case letters
	 */
	FE_TEXT_EXTENTS(charset, font, "a", 1, &ascent, &descent, &overall);

	return -((overall.ascent + 1) / 2);
}


int
fe_GetUnderlinePosition(int charset)
{
	/*
	 * Underline position does not depend on font, otherwise you get
	 * varying positions within a single line, and this looks bad.
	 */

	if ((fe_CharSetInfoArray[charset & 0xff].type == FE_FONT_TYPE_GROUP)
		|| ((charset&0xFF) == (CS_UTF8&0xFF)))
	{
		/*
		 * East Asian fonts have a lower baseline.
		 * Underline looks better like this.
		 */
		return 3;
	}

	return 1;
}


static XP_Bool
fe_AreNormalFontsAvail(int16 win_csid)
{
	if (fe_FontCharSets[win_csid & 0xff].name != NULL)
		return TRUE;
	else
		return FALSE;
}

static fe_Font
fe_LoadNormalFont(MWContext *context, char *familyName, 
                  int points, int sizeNum,
		  int fontmask, int charset,
		  int pitch, int faceNum, Display *dpy)
{
	fe_FontFace	*face;
	fe_Font		font;
	fe_FontFamily	*family = NULL;
        fe_FontSize     *size;
        double          decipoints;

	if (familyName && *familyName)
	{
		family = fe_FontFindFamily(charset, pitch, familyName);
	}
	if (!family)
	{
		family = fe_GetFontFamily(charset, pitch);
		if (!family)
		{
			return NULL;
		}
	}
        if (! points)
            size = family->htmlSizes[sizeNum-1];
        else {
            decipoints = (double) (points * 10);
            size = fe_FindFontPointSize(family, decipoints, True, False);
        }
        if (!size)
            return NULL;
        face = size->faces[faceNum];
        if (!face)
            return NULL;

	font = face->font;
	if (font)
	{
		return font;
	}

	else {
		font = fe_LoadXFont(dpy, face->longXLFDFontName);
	}
	if (font)
	{
		face->font = font;
		return font;
	}

	return NULL;
}


void
fe_GenericFontExtents(int charset, fe_Font font, 
						int *fontAscent, int *fontDescent)
{
	int			ascent;
	int			ascentExtra;
	int			descent;
	int			descentExtra;
	XFontStruct		*f;
	fe_FontGroupFont	*fonts;
	int			i;
	int			num;
    fe_CharSetInfo  *info = &fe_CharSetInfoArray[(charset) & 0xff];

	/*
	 * Handle Unicode Pseudo Fonts
	 */
    if (info->info == FE_FONT_INFO_UNICODE) {
        fe_UnicodePseudoFont *ufont = (fe_UnicodePseudoFont *)font;
        if (ufont->tag != UNICODE_PSEUDO_FONT_TAG) {
#ifdef DEBUG_bstell
            printf("bad ufont %s %d\n", __FILE__, __LINE__);
#endif
            *(fontAscent) = 12;
            *(fontDescent) = 3;
	    }
					
        (void)fe_UnicodeInitXfont(font, CS_LATIN1);
        *(fontAscent) = ufont->ascent;
        *(fontDescent) = ufont->descent;
        return;
    }

	/*
	 * Handle EUC Fonts
	 */
	fonts = (fe_FontGroupFont *) font;

	ascent = 0;
	descent = 0;

	num = fe_CharSetFuncsArray[info->info].numberOfFonts;
	for (i = 0; i < num; i++)
	{
		if (i)
		{
			/*
			 * East Asian text looks better with extra space for
			 * underline.
			 */
			ascentExtra = 1;
			descentExtra = 2;
		}
		else
		{
			ascentExtra = 0;
			descentExtra = 0;
		}
		f = fonts[i].xFont;
		if (f)
		{
			if (f->ascent + ascentExtra > ascent)
			{
				ascent = f->ascent + ascentExtra;
			}
			if (f->descent + descentExtra > descent)
			{
				descent = f->descent + descentExtra;
			}
		}
	}

	*fontAscent = ascent;
	*fontDescent = descent;
}


#define FE_PROCESS_STRING_BUFFER_SIZE	1024


static void
fe_ProcessEUCString(int charset, fe_StringProcessTable *table, 
	fe_XDrawStringFunc draw1, 
	fe_XDrawStringFunc draw2, 
	Display *dpy, Drawable d, fe_Font fontGroup,
	GC gc, int x, int y, unsigned char *in, unsigned int inEnd,
	int *fontAscent, int *fontDescent, XCharStruct *overall,
	unsigned char numberOfFonts)
{
	int			ascent;
	int			descent;
	int			direction;
	fe_StringProcessTable	*entry;
	fe_FontGroupFont	*font;
	unsigned char		fontIndex;
	fe_FontGroupFont	*fonts;
	unsigned int		inIndex;
	unsigned char		len;
	unsigned char		mask1;
	unsigned char		mask2;
	XCharStruct		metrics;
	unsigned char		nextFontIndex;
	unsigned char		out1[FE_PROCESS_STRING_BUFFER_SIZE];
	XChar2b			out2[FE_PROCESS_STRING_BUFFER_SIZE/2];
	unsigned int		out1Index;
	unsigned int		out2Index;
	XChar2b			*p;
	unsigned char		skip;
	XFontStruct		*xFont;

	if (inEnd > FE_PROCESS_STRING_BUFFER_SIZE)
	{
		inEnd = FE_PROCESS_STRING_BUFFER_SIZE;
	}

	fonts = (fe_FontGroupFont *) fontGroup;

	inIndex = 0;

	out1Index = 0;

	out2Index = 0;

	if (overall)
	{
		overall->lbearing = 0;
		overall->rbearing = 0;
		overall->width = 0;
		overall->ascent = 0;
		overall->descent = 0;
	}

	while (inIndex < inEnd)
	{
		entry = &table[in[inIndex]];
		skip = entry->skip;
		len = entry->len;
		fontIndex = entry->fontIndex;
		font = &fonts[fontIndex];
		xFont = font->xFont;
		if (gc && xFont)
		{
			XSetFont(dpy, gc, xFont->fid);
		}
		if (len == 1)
		{
			while (inIndex < inEnd)
			{
				inIndex += skip;
				if (inIndex >= inEnd)
				{
					break;
				}
				out1[out1Index++] = in[inIndex++];
				if (inIndex >= inEnd)
				{
					break;
				}
				nextFontIndex = table[in[inIndex]].fontIndex;
				if (nextFontIndex != fontIndex)
				{
					break;
				}
			}
			if (out1Index && xFont)
			{
				XTextExtents(xFont, out1, out1Index,
					&direction, &ascent, &descent,
					&metrics);
				if (draw1)
				{
					(*draw1)(dpy, d, gc, x, y,
						out1, out1Index);
				}
				x += metrics.width;
				if (overall)
				{
					overall->width += metrics.width;
					if (metrics.ascent > overall->ascent)
					{
						overall->ascent =
							metrics.ascent;
					}
					if (metrics.descent > overall->descent)
					{
						overall->descent =
							metrics.descent;
					}
				}
			}
			out1Index = 0;
		}
		else
		{
			mask1 = font->mask1;
			mask2 = font->mask2;
			while (inIndex < inEnd)
			{
				inIndex += skip;
				if ((inIndex + 1) >= inEnd)
				{
					inIndex++;
					break;
				}
				p = &out2[out2Index++];
				p->byte1 = (in[inIndex++] & mask1);
				p->byte2 = (in[inIndex++] & mask2);
				if (inIndex >= inEnd)
				{
					break;
				}
				nextFontIndex = table[in[inIndex]].fontIndex;
				if (nextFontIndex != fontIndex)
				{
					break;
				}
			}
			if (out2Index && xFont)
			{
				XTextExtents16(xFont, out2, out2Index,
					&direction, &ascent, &descent,
					&metrics);
				if (draw2)
				{
					(*draw2)(dpy, d, gc, x, y,
						(char *) out2, out2Index);
				}
				x += metrics.width;
				if (overall)
				{
					overall->width += metrics.width;
					if (metrics.ascent > overall->ascent)
					{
						overall->ascent =
							metrics.ascent;
					}
					if (metrics.descent > overall->descent)
					{
						overall->descent =
							metrics.descent;
					}
				}
			}
			out2Index = 0;
		}
	}

	if (overall)
	{
		/* @@@ fix */
		overall->rbearing = overall->width;

		fe_GenericFontExtents(charset, fontGroup, fontAscent,
			fontDescent);
	}
}


			/*	fontIndex	skip	len	*/
#define EUCCN_LATIN1	{	0,		0,	1	}
#define EUCCN_GB2312	{	1,		0,	2	}

static fe_StringProcessTable EUCCNTable[] =
{
	/* 0x00 */	EUCCN_LATIN1,
	/* 0x01 */	EUCCN_LATIN1,
	/* 0x02 */	EUCCN_LATIN1,
	/* 0x03 */	EUCCN_LATIN1,
	/* 0x04 */	EUCCN_LATIN1,
	/* 0x05 */	EUCCN_LATIN1,
	/* 0x06 */	EUCCN_LATIN1,
	/* 0x07 */	EUCCN_LATIN1,
	/* 0x08 */	EUCCN_LATIN1,
	/* 0x09 */	EUCCN_LATIN1,
	/* 0x0a */	EUCCN_LATIN1,
	/* 0x0b */	EUCCN_LATIN1,
	/* 0x0c */	EUCCN_LATIN1,
	/* 0x0d */	EUCCN_LATIN1,
	/* 0x0e */	EUCCN_LATIN1,
	/* 0x0f */	EUCCN_LATIN1,
	/* 0x10 */	EUCCN_LATIN1,
	/* 0x11 */	EUCCN_LATIN1,
	/* 0x12 */	EUCCN_LATIN1,
	/* 0x13 */	EUCCN_LATIN1,
	/* 0x14 */	EUCCN_LATIN1,
	/* 0x15 */	EUCCN_LATIN1,
	/* 0x16 */	EUCCN_LATIN1,
	/* 0x17 */	EUCCN_LATIN1,
	/* 0x18 */	EUCCN_LATIN1,
	/* 0x19 */	EUCCN_LATIN1,
	/* 0x1a */	EUCCN_LATIN1,
	/* 0x1b */	EUCCN_LATIN1,
	/* 0x1c */	EUCCN_LATIN1,
	/* 0x1d */	EUCCN_LATIN1,
	/* 0x1e */	EUCCN_LATIN1,
	/* 0x1f */	EUCCN_LATIN1,
	/* 0x20 */	EUCCN_LATIN1,
	/* 0x21 */	EUCCN_LATIN1,
	/* 0x22 */	EUCCN_LATIN1,
	/* 0x23 */	EUCCN_LATIN1,
	/* 0x24 */	EUCCN_LATIN1,
	/* 0x25 */	EUCCN_LATIN1,
	/* 0x26 */	EUCCN_LATIN1,
	/* 0x27 */	EUCCN_LATIN1,
	/* 0x28 */	EUCCN_LATIN1,
	/* 0x29 */	EUCCN_LATIN1,
	/* 0x2a */	EUCCN_LATIN1,
	/* 0x2b */	EUCCN_LATIN1,
	/* 0x2c */	EUCCN_LATIN1,
	/* 0x2d */	EUCCN_LATIN1,
	/* 0x2e */	EUCCN_LATIN1,
	/* 0x2f */	EUCCN_LATIN1,
	/* 0x30 */	EUCCN_LATIN1,
	/* 0x31 */	EUCCN_LATIN1,
	/* 0x32 */	EUCCN_LATIN1,
	/* 0x33 */	EUCCN_LATIN1,
	/* 0x34 */	EUCCN_LATIN1,
	/* 0x35 */	EUCCN_LATIN1,
	/* 0x36 */	EUCCN_LATIN1,
	/* 0x37 */	EUCCN_LATIN1,
	/* 0x38 */	EUCCN_LATIN1,
	/* 0x39 */	EUCCN_LATIN1,
	/* 0x3a */	EUCCN_LATIN1,
	/* 0x3b */	EUCCN_LATIN1,
	/* 0x3c */	EUCCN_LATIN1,
	/* 0x3d */	EUCCN_LATIN1,
	/* 0x3e */	EUCCN_LATIN1,
	/* 0x3f */	EUCCN_LATIN1,
	/* 0x40 */	EUCCN_LATIN1,
	/* 0x41 */	EUCCN_LATIN1,
	/* 0x42 */	EUCCN_LATIN1,
	/* 0x43 */	EUCCN_LATIN1,
	/* 0x44 */	EUCCN_LATIN1,
	/* 0x45 */	EUCCN_LATIN1,
	/* 0x46 */	EUCCN_LATIN1,
	/* 0x47 */	EUCCN_LATIN1,
	/* 0x48 */	EUCCN_LATIN1,
	/* 0x49 */	EUCCN_LATIN1,
	/* 0x4a */	EUCCN_LATIN1,
	/* 0x4b */	EUCCN_LATIN1,
	/* 0x4c */	EUCCN_LATIN1,
	/* 0x4d */	EUCCN_LATIN1,
	/* 0x4e */	EUCCN_LATIN1,
	/* 0x4f */	EUCCN_LATIN1,
	/* 0x50 */	EUCCN_LATIN1,
	/* 0x51 */	EUCCN_LATIN1,
	/* 0x52 */	EUCCN_LATIN1,
	/* 0x53 */	EUCCN_LATIN1,
	/* 0x54 */	EUCCN_LATIN1,
	/* 0x55 */	EUCCN_LATIN1,
	/* 0x56 */	EUCCN_LATIN1,
	/* 0x57 */	EUCCN_LATIN1,
	/* 0x58 */	EUCCN_LATIN1,
	/* 0x59 */	EUCCN_LATIN1,
	/* 0x5a */	EUCCN_LATIN1,
	/* 0x5b */	EUCCN_LATIN1,
	/* 0x5c */	EUCCN_LATIN1,
	/* 0x5d */	EUCCN_LATIN1,
	/* 0x5e */	EUCCN_LATIN1,
	/* 0x5f */	EUCCN_LATIN1,
	/* 0x60 */	EUCCN_LATIN1,
	/* 0x61 */	EUCCN_LATIN1,
	/* 0x62 */	EUCCN_LATIN1,
	/* 0x63 */	EUCCN_LATIN1,
	/* 0x64 */	EUCCN_LATIN1,
	/* 0x65 */	EUCCN_LATIN1,
	/* 0x66 */	EUCCN_LATIN1,
	/* 0x67 */	EUCCN_LATIN1,
	/* 0x68 */	EUCCN_LATIN1,
	/* 0x69 */	EUCCN_LATIN1,
	/* 0x6a */	EUCCN_LATIN1,
	/* 0x6b */	EUCCN_LATIN1,
	/* 0x6c */	EUCCN_LATIN1,
	/* 0x6d */	EUCCN_LATIN1,
	/* 0x6e */	EUCCN_LATIN1,
	/* 0x6f */	EUCCN_LATIN1,
	/* 0x70 */	EUCCN_LATIN1,
	/* 0x71 */	EUCCN_LATIN1,
	/* 0x72 */	EUCCN_LATIN1,
	/* 0x73 */	EUCCN_LATIN1,
	/* 0x74 */	EUCCN_LATIN1,
	/* 0x75 */	EUCCN_LATIN1,
	/* 0x76 */	EUCCN_LATIN1,
	/* 0x77 */	EUCCN_LATIN1,
	/* 0x78 */	EUCCN_LATIN1,
	/* 0x79 */	EUCCN_LATIN1,
	/* 0x7a */	EUCCN_LATIN1,
	/* 0x7b */	EUCCN_LATIN1,
	/* 0x7c */	EUCCN_LATIN1,
	/* 0x7d */	EUCCN_LATIN1,
	/* 0x7e */	EUCCN_LATIN1,
	/* 0x7f */	EUCCN_LATIN1,
	/* 0x80 */	EUCCN_LATIN1,
	/* 0x81 */	EUCCN_LATIN1,
	/* 0x82 */	EUCCN_LATIN1,
	/* 0x83 */	EUCCN_LATIN1,
	/* 0x84 */	EUCCN_LATIN1,
	/* 0x85 */	EUCCN_LATIN1,
	/* 0x86 */	EUCCN_LATIN1,
	/* 0x87 */	EUCCN_LATIN1,
	/* 0x88 */	EUCCN_LATIN1,
	/* 0x89 */	EUCCN_LATIN1,
	/* 0x8a */	EUCCN_LATIN1,
	/* 0x8b */	EUCCN_LATIN1,
	/* 0x8c */	EUCCN_LATIN1,
	/* 0x8d */	EUCCN_LATIN1,
	/* 0x8e */	EUCCN_LATIN1,
	/* 0x8f */	EUCCN_LATIN1,
	/* 0x90 */	EUCCN_LATIN1,
	/* 0x91 */	EUCCN_LATIN1,
	/* 0x92 */	EUCCN_LATIN1,
	/* 0x93 */	EUCCN_LATIN1,
	/* 0x94 */	EUCCN_LATIN1,
	/* 0x95 */	EUCCN_LATIN1,
	/* 0x96 */	EUCCN_LATIN1,
	/* 0x97 */	EUCCN_LATIN1,
	/* 0x98 */	EUCCN_LATIN1,
	/* 0x99 */	EUCCN_LATIN1,
	/* 0x9a */	EUCCN_LATIN1,
	/* 0x9b */	EUCCN_LATIN1,
	/* 0x9c */	EUCCN_LATIN1,
	/* 0x9d */	EUCCN_LATIN1,
	/* 0x9e */	EUCCN_LATIN1,
	/* 0x9f */	EUCCN_LATIN1,
	/* 0xa0 */	EUCCN_LATIN1,
	/* 0xa1 */	EUCCN_GB2312,
	/* 0xa2 */	EUCCN_GB2312,
	/* 0xa3 */	EUCCN_GB2312,
	/* 0xa4 */	EUCCN_GB2312,
	/* 0xa5 */	EUCCN_GB2312,
	/* 0xa6 */	EUCCN_GB2312,
	/* 0xa7 */	EUCCN_GB2312,
	/* 0xa8 */	EUCCN_GB2312,
	/* 0xa9 */	EUCCN_GB2312,
	/* 0xaa */	EUCCN_GB2312,
	/* 0xab */	EUCCN_GB2312,
	/* 0xac */	EUCCN_GB2312,
	/* 0xad */	EUCCN_GB2312,
	/* 0xae */	EUCCN_GB2312,
	/* 0xaf */	EUCCN_GB2312,
	/* 0xb0 */	EUCCN_GB2312,
	/* 0xb1 */	EUCCN_GB2312,
	/* 0xb2 */	EUCCN_GB2312,
	/* 0xb3 */	EUCCN_GB2312,
	/* 0xb4 */	EUCCN_GB2312,
	/* 0xb5 */	EUCCN_GB2312,
	/* 0xb6 */	EUCCN_GB2312,
	/* 0xb7 */	EUCCN_GB2312,
	/* 0xb8 */	EUCCN_GB2312,
	/* 0xb9 */	EUCCN_GB2312,
	/* 0xba */	EUCCN_GB2312,
	/* 0xbb */	EUCCN_GB2312,
	/* 0xbc */	EUCCN_GB2312,
	/* 0xbd */	EUCCN_GB2312,
	/* 0xbe */	EUCCN_GB2312,
	/* 0xbf */	EUCCN_GB2312,
	/* 0xc0 */	EUCCN_GB2312,
	/* 0xc1 */	EUCCN_GB2312,
	/* 0xc2 */	EUCCN_GB2312,
	/* 0xc3 */	EUCCN_GB2312,
	/* 0xc4 */	EUCCN_GB2312,
	/* 0xc5 */	EUCCN_GB2312,
	/* 0xc6 */	EUCCN_GB2312,
	/* 0xc7 */	EUCCN_GB2312,
	/* 0xc8 */	EUCCN_GB2312,
	/* 0xc9 */	EUCCN_GB2312,
	/* 0xca */	EUCCN_GB2312,
	/* 0xcb */	EUCCN_GB2312,
	/* 0xcc */	EUCCN_GB2312,
	/* 0xcd */	EUCCN_GB2312,
	/* 0xce */	EUCCN_GB2312,
	/* 0xcf */	EUCCN_GB2312,
	/* 0xd0 */	EUCCN_GB2312,
	/* 0xd1 */	EUCCN_GB2312,
	/* 0xd2 */	EUCCN_GB2312,
	/* 0xd3 */	EUCCN_GB2312,
	/* 0xd4 */	EUCCN_GB2312,
	/* 0xd5 */	EUCCN_GB2312,
	/* 0xd6 */	EUCCN_GB2312,
	/* 0xd7 */	EUCCN_GB2312,
	/* 0xd8 */	EUCCN_GB2312,
	/* 0xd9 */	EUCCN_GB2312,
	/* 0xda */	EUCCN_GB2312,
	/* 0xdb */	EUCCN_GB2312,
	/* 0xdc */	EUCCN_GB2312,
	/* 0xdd */	EUCCN_GB2312,
	/* 0xde */	EUCCN_GB2312,
	/* 0xdf */	EUCCN_GB2312,
	/* 0xe0 */	EUCCN_GB2312,
	/* 0xe1 */	EUCCN_GB2312,
	/* 0xe2 */	EUCCN_GB2312,
	/* 0xe3 */	EUCCN_GB2312,
	/* 0xe4 */	EUCCN_GB2312,
	/* 0xe5 */	EUCCN_GB2312,
	/* 0xe6 */	EUCCN_GB2312,
	/* 0xe7 */	EUCCN_GB2312,
	/* 0xe8 */	EUCCN_GB2312,
	/* 0xe9 */	EUCCN_GB2312,
	/* 0xea */	EUCCN_GB2312,
	/* 0xeb */	EUCCN_GB2312,
	/* 0xec */	EUCCN_GB2312,
	/* 0xed */	EUCCN_GB2312,
	/* 0xee */	EUCCN_GB2312,
	/* 0xef */	EUCCN_GB2312,
	/* 0xf0 */	EUCCN_GB2312,
	/* 0xf1 */	EUCCN_GB2312,
	/* 0xf2 */	EUCCN_GB2312,
	/* 0xf3 */	EUCCN_GB2312,
	/* 0xf4 */	EUCCN_GB2312,
	/* 0xf5 */	EUCCN_GB2312,
	/* 0xf6 */	EUCCN_GB2312,
	/* 0xf7 */	EUCCN_GB2312,
	/* 0xf8 */	EUCCN_GB2312,
	/* 0xf9 */	EUCCN_GB2312,
	/* 0xfa */	EUCCN_GB2312,
	/* 0xfb */	EUCCN_GB2312,
	/* 0xfc */	EUCCN_GB2312,
	/* 0xfd */	EUCCN_GB2312,
	/* 0xfe */	EUCCN_GB2312,
	/* 0xff */	EUCCN_LATIN1,
};


static XP_Bool
fe_AreEUCCNFontsAvail(int16 win_csid)
{
	/* this needs to match the code in fe_LoadEUCCNFont */
	if ((fe_FontCharSets[CS_GB2312 & 0xff].name == NULL)
		&& (fe_FontCharSets[CS_GB2312_11 & 0xff].name == NULL))
		return FALSE;
	else
		return TRUE;
}

static fe_Font
fe_LoadEUCCNFont(MWContext *context, char *familyName, int points,
                 int sizeNum, int fontmask, int charset, int pitch,
                 int faceNum, Display *dpy)
{
	int16			cs;
	int			len;
	fe_FontGroupFont	*ret;

	len = (2 * sizeof(fe_FontGroupFont));
	ret = calloc(1, len);
	if (!ret)
	{
		return NULL;
	}
	(void) memset(ret, 0, len);

	/* this needs to match the code in fe_AreEUCCNFontsAvail */
	cs = CS_LATIN1;
	ret[0].xFont = (XFontStruct *)
            fe_LoadFont(context, &cs, familyName, points, sizeNum, fontmask);
	if (cs != CS_LATIN1)
	{
		XP_FREE(ret);
		return NULL;
	}

	cs = CS_GB2312;
	ret[1].xFont = (XFontStruct *)
            fe_LoadFont(context, &cs, familyName, points, sizeNum, fontmask);
	if (cs == CS_GB2312)
	{
		ret[1].mask1 = 0x7f;
		ret[1].mask2 = 0x7f;
	}
	else
	{
		cs = CS_GB2312_11;
		ret[1].xFont = (XFontStruct *)
                    fe_LoadFont(context, &cs, familyName, points, sizeNum,
                                fontmask);
		if (cs == CS_GB2312_11)
		{
			ret[1].mask1 = 0xff;
			ret[1].mask2 = 0xff;
		}
		else
		{
			XP_FREE(ret);
			return NULL;
		}
	}

	return (fe_Font) ret;
}


static void
fe_EUCCNTextExtents(fe_Font font, char *string, int len, int *direction,
	int *fontAscent, int *fontDescent, XCharStruct *overall)
{
	fe_ProcessEUCString(CS_GB_8BIT, EUCCNTable, 
		NULL, 
		NULL,
		NULL, None, font, NULL, 0, 0, string, len,
		fontAscent, fontDescent, overall,
		fe_CharSetFuncsArray[FE_FONT_INFO_EUCCN].numberOfFonts);
}


static void
fe_DrawEUCCNString(Display *dpy, Drawable d, fe_Font font, GC gc, int x,
	int y, char *string, int len)
{
	fe_ProcessEUCString(CS_GB_8BIT, EUCCNTable, 
		(fe_XDrawStringFunc) XDrawString, 
		(fe_XDrawStringFunc) XDrawString16,
		dpy, d, font, gc, x, y, string, len,
		NULL, NULL, NULL,
		0);
}


static void
fe_DrawEUCCNImageString(Display *dpy, Drawable d, fe_Font font, GC gc, GC gc2,
	int x, int y, char *string, int len)
{
	fe_ProcessEUCString(CS_GB_8BIT, EUCCNTable, 
		(fe_XDrawStringFunc) XDrawImageString,
		(fe_XDrawStringFunc) XDrawImageString16,
		dpy, d, font, gc, x, y, string, len,
		NULL, NULL, NULL,
		0);
}


			/*	fontIndex	skip	len	*/
#define EUCJP_LATIN1	{	0,		0,	1	}
#define EUCJP_JISX0208	{	1,		0,	2	}
#define EUCJP_JISX0201	{	2,		1,	1	}
#define EUCJP_JISX0212	{	3,		1,	2	}

static fe_StringProcessTable EUCJPTable[] =
{
	/* 0x00 */	EUCJP_LATIN1,
	/* 0x01 */	EUCJP_LATIN1,
	/* 0x02 */	EUCJP_LATIN1,
	/* 0x03 */	EUCJP_LATIN1,
	/* 0x04 */	EUCJP_LATIN1,
	/* 0x05 */	EUCJP_LATIN1,
	/* 0x06 */	EUCJP_LATIN1,
	/* 0x07 */	EUCJP_LATIN1,
	/* 0x08 */	EUCJP_LATIN1,
	/* 0x09 */	EUCJP_LATIN1,
	/* 0x0a */	EUCJP_LATIN1,
	/* 0x0b */	EUCJP_LATIN1,
	/* 0x0c */	EUCJP_LATIN1,
	/* 0x0d */	EUCJP_LATIN1,
	/* 0x0e */	EUCJP_LATIN1,
	/* 0x0f */	EUCJP_LATIN1,
	/* 0x10 */	EUCJP_LATIN1,
	/* 0x11 */	EUCJP_LATIN1,
	/* 0x12 */	EUCJP_LATIN1,
	/* 0x13 */	EUCJP_LATIN1,
	/* 0x14 */	EUCJP_LATIN1,
	/* 0x15 */	EUCJP_LATIN1,
	/* 0x16 */	EUCJP_LATIN1,
	/* 0x17 */	EUCJP_LATIN1,
	/* 0x18 */	EUCJP_LATIN1,
	/* 0x19 */	EUCJP_LATIN1,
	/* 0x1a */	EUCJP_LATIN1,
	/* 0x1b */	EUCJP_LATIN1,
	/* 0x1c */	EUCJP_LATIN1,
	/* 0x1d */	EUCJP_LATIN1,
	/* 0x1e */	EUCJP_LATIN1,
	/* 0x1f */	EUCJP_LATIN1,
	/* 0x20 */	EUCJP_LATIN1,
	/* 0x21 */	EUCJP_LATIN1,
	/* 0x22 */	EUCJP_LATIN1,
	/* 0x23 */	EUCJP_LATIN1,
	/* 0x24 */	EUCJP_LATIN1,
	/* 0x25 */	EUCJP_LATIN1,
	/* 0x26 */	EUCJP_LATIN1,
	/* 0x27 */	EUCJP_LATIN1,
	/* 0x28 */	EUCJP_LATIN1,
	/* 0x29 */	EUCJP_LATIN1,
	/* 0x2a */	EUCJP_LATIN1,
	/* 0x2b */	EUCJP_LATIN1,
	/* 0x2c */	EUCJP_LATIN1,
	/* 0x2d */	EUCJP_LATIN1,
	/* 0x2e */	EUCJP_LATIN1,
	/* 0x2f */	EUCJP_LATIN1,
	/* 0x30 */	EUCJP_LATIN1,
	/* 0x31 */	EUCJP_LATIN1,
	/* 0x32 */	EUCJP_LATIN1,
	/* 0x33 */	EUCJP_LATIN1,
	/* 0x34 */	EUCJP_LATIN1,
	/* 0x35 */	EUCJP_LATIN1,
	/* 0x36 */	EUCJP_LATIN1,
	/* 0x37 */	EUCJP_LATIN1,
	/* 0x38 */	EUCJP_LATIN1,
	/* 0x39 */	EUCJP_LATIN1,
	/* 0x3a */	EUCJP_LATIN1,
	/* 0x3b */	EUCJP_LATIN1,
	/* 0x3c */	EUCJP_LATIN1,
	/* 0x3d */	EUCJP_LATIN1,
	/* 0x3e */	EUCJP_LATIN1,
	/* 0x3f */	EUCJP_LATIN1,
	/* 0x40 */	EUCJP_LATIN1,
	/* 0x41 */	EUCJP_LATIN1,
	/* 0x42 */	EUCJP_LATIN1,
	/* 0x43 */	EUCJP_LATIN1,
	/* 0x44 */	EUCJP_LATIN1,
	/* 0x45 */	EUCJP_LATIN1,
	/* 0x46 */	EUCJP_LATIN1,
	/* 0x47 */	EUCJP_LATIN1,
	/* 0x48 */	EUCJP_LATIN1,
	/* 0x49 */	EUCJP_LATIN1,
	/* 0x4a */	EUCJP_LATIN1,
	/* 0x4b */	EUCJP_LATIN1,
	/* 0x4c */	EUCJP_LATIN1,
	/* 0x4d */	EUCJP_LATIN1,
	/* 0x4e */	EUCJP_LATIN1,
	/* 0x4f */	EUCJP_LATIN1,
	/* 0x50 */	EUCJP_LATIN1,
	/* 0x51 */	EUCJP_LATIN1,
	/* 0x52 */	EUCJP_LATIN1,
	/* 0x53 */	EUCJP_LATIN1,
	/* 0x54 */	EUCJP_LATIN1,
	/* 0x55 */	EUCJP_LATIN1,
	/* 0x56 */	EUCJP_LATIN1,
	/* 0x57 */	EUCJP_LATIN1,
	/* 0x58 */	EUCJP_LATIN1,
	/* 0x59 */	EUCJP_LATIN1,
	/* 0x5a */	EUCJP_LATIN1,
	/* 0x5b */	EUCJP_LATIN1,
	/* 0x5c */	EUCJP_LATIN1,
	/* 0x5d */	EUCJP_LATIN1,
	/* 0x5e */	EUCJP_LATIN1,
	/* 0x5f */	EUCJP_LATIN1,
	/* 0x60 */	EUCJP_LATIN1,
	/* 0x61 */	EUCJP_LATIN1,
	/* 0x62 */	EUCJP_LATIN1,
	/* 0x63 */	EUCJP_LATIN1,
	/* 0x64 */	EUCJP_LATIN1,
	/* 0x65 */	EUCJP_LATIN1,
	/* 0x66 */	EUCJP_LATIN1,
	/* 0x67 */	EUCJP_LATIN1,
	/* 0x68 */	EUCJP_LATIN1,
	/* 0x69 */	EUCJP_LATIN1,
	/* 0x6a */	EUCJP_LATIN1,
	/* 0x6b */	EUCJP_LATIN1,
	/* 0x6c */	EUCJP_LATIN1,
	/* 0x6d */	EUCJP_LATIN1,
	/* 0x6e */	EUCJP_LATIN1,
	/* 0x6f */	EUCJP_LATIN1,
	/* 0x70 */	EUCJP_LATIN1,
	/* 0x71 */	EUCJP_LATIN1,
	/* 0x72 */	EUCJP_LATIN1,
	/* 0x73 */	EUCJP_LATIN1,
	/* 0x74 */	EUCJP_LATIN1,
	/* 0x75 */	EUCJP_LATIN1,
	/* 0x76 */	EUCJP_LATIN1,
	/* 0x77 */	EUCJP_LATIN1,
	/* 0x78 */	EUCJP_LATIN1,
	/* 0x79 */	EUCJP_LATIN1,
	/* 0x7a */	EUCJP_LATIN1,
	/* 0x7b */	EUCJP_LATIN1,
	/* 0x7c */	EUCJP_LATIN1,
	/* 0x7d */	EUCJP_LATIN1,
	/* 0x7e */	EUCJP_LATIN1,
	/* 0x7f */	EUCJP_LATIN1,
	/* 0x80 */	EUCJP_LATIN1,
	/* 0x81 */	EUCJP_LATIN1,
	/* 0x82 */	EUCJP_LATIN1,
	/* 0x83 */	EUCJP_LATIN1,
	/* 0x84 */	EUCJP_LATIN1,
	/* 0x85 */	EUCJP_LATIN1,
	/* 0x86 */	EUCJP_LATIN1,
	/* 0x87 */	EUCJP_LATIN1,
	/* 0x88 */	EUCJP_LATIN1,
	/* 0x89 */	EUCJP_LATIN1,
	/* 0x8a */	EUCJP_LATIN1,
	/* 0x8b */	EUCJP_LATIN1,
	/* 0x8c */	EUCJP_LATIN1,
	/* 0x8d */	EUCJP_LATIN1,
	/* 0x8e */	EUCJP_JISX0201,
	/* 0x8f */	EUCJP_JISX0212,
	/* 0x90 */	EUCJP_LATIN1,
	/* 0x91 */	EUCJP_LATIN1,
	/* 0x92 */	EUCJP_LATIN1,
	/* 0x93 */	EUCJP_LATIN1,
	/* 0x94 */	EUCJP_LATIN1,
	/* 0x95 */	EUCJP_LATIN1,
	/* 0x96 */	EUCJP_LATIN1,
	/* 0x97 */	EUCJP_LATIN1,
	/* 0x98 */	EUCJP_LATIN1,
	/* 0x99 */	EUCJP_LATIN1,
	/* 0x9a */	EUCJP_LATIN1,
	/* 0x9b */	EUCJP_LATIN1,
	/* 0x9c */	EUCJP_LATIN1,
	/* 0x9d */	EUCJP_LATIN1,
	/* 0x9e */	EUCJP_LATIN1,
	/* 0x9f */	EUCJP_LATIN1,
	/* 0xa0 */	EUCJP_LATIN1,
	/* 0xa1 */	EUCJP_JISX0208,
	/* 0xa2 */	EUCJP_JISX0208,
	/* 0xa3 */	EUCJP_JISX0208,
	/* 0xa4 */	EUCJP_JISX0208,
	/* 0xa5 */	EUCJP_JISX0208,
	/* 0xa6 */	EUCJP_JISX0208,
	/* 0xa7 */	EUCJP_JISX0208,
	/* 0xa8 */	EUCJP_JISX0208,
	/* 0xa9 */	EUCJP_JISX0208,
	/* 0xaa */	EUCJP_JISX0208,
	/* 0xab */	EUCJP_JISX0208,
	/* 0xac */	EUCJP_JISX0208,
	/* 0xad */	EUCJP_JISX0208,
	/* 0xae */	EUCJP_JISX0208,
	/* 0xaf */	EUCJP_JISX0208,
	/* 0xb0 */	EUCJP_JISX0208,
	/* 0xb1 */	EUCJP_JISX0208,
	/* 0xb2 */	EUCJP_JISX0208,
	/* 0xb3 */	EUCJP_JISX0208,
	/* 0xb4 */	EUCJP_JISX0208,
	/* 0xb5 */	EUCJP_JISX0208,
	/* 0xb6 */	EUCJP_JISX0208,
	/* 0xb7 */	EUCJP_JISX0208,
	/* 0xb8 */	EUCJP_JISX0208,
	/* 0xb9 */	EUCJP_JISX0208,
	/* 0xba */	EUCJP_JISX0208,
	/* 0xbb */	EUCJP_JISX0208,
	/* 0xbc */	EUCJP_JISX0208,
	/* 0xbd */	EUCJP_JISX0208,
	/* 0xbe */	EUCJP_JISX0208,
	/* 0xbf */	EUCJP_JISX0208,
	/* 0xc0 */	EUCJP_JISX0208,
	/* 0xc1 */	EUCJP_JISX0208,
	/* 0xc2 */	EUCJP_JISX0208,
	/* 0xc3 */	EUCJP_JISX0208,
	/* 0xc4 */	EUCJP_JISX0208,
	/* 0xc5 */	EUCJP_JISX0208,
	/* 0xc6 */	EUCJP_JISX0208,
	/* 0xc7 */	EUCJP_JISX0208,
	/* 0xc8 */	EUCJP_JISX0208,
	/* 0xc9 */	EUCJP_JISX0208,
	/* 0xca */	EUCJP_JISX0208,
	/* 0xcb */	EUCJP_JISX0208,
	/* 0xcc */	EUCJP_JISX0208,
	/* 0xcd */	EUCJP_JISX0208,
	/* 0xce */	EUCJP_JISX0208,
	/* 0xcf */	EUCJP_JISX0208,
	/* 0xd0 */	EUCJP_JISX0208,
	/* 0xd1 */	EUCJP_JISX0208,
	/* 0xd2 */	EUCJP_JISX0208,
	/* 0xd3 */	EUCJP_JISX0208,
	/* 0xd4 */	EUCJP_JISX0208,
	/* 0xd5 */	EUCJP_JISX0208,
	/* 0xd6 */	EUCJP_JISX0208,
	/* 0xd7 */	EUCJP_JISX0208,
	/* 0xd8 */	EUCJP_JISX0208,
	/* 0xd9 */	EUCJP_JISX0208,
	/* 0xda */	EUCJP_JISX0208,
	/* 0xdb */	EUCJP_JISX0208,
	/* 0xdc */	EUCJP_JISX0208,
	/* 0xdd */	EUCJP_JISX0208,
	/* 0xde */	EUCJP_JISX0208,
	/* 0xdf */	EUCJP_JISX0208,
	/* 0xe0 */	EUCJP_JISX0208,
	/* 0xe1 */	EUCJP_JISX0208,
	/* 0xe2 */	EUCJP_JISX0208,
	/* 0xe3 */	EUCJP_JISX0208,
	/* 0xe4 */	EUCJP_JISX0208,
	/* 0xe5 */	EUCJP_JISX0208,
	/* 0xe6 */	EUCJP_JISX0208,
	/* 0xe7 */	EUCJP_JISX0208,
	/* 0xe8 */	EUCJP_JISX0208,
	/* 0xe9 */	EUCJP_JISX0208,
	/* 0xea */	EUCJP_JISX0208,
	/* 0xeb */	EUCJP_JISX0208,
	/* 0xec */	EUCJP_JISX0208,
	/* 0xed */	EUCJP_JISX0208,
	/* 0xee */	EUCJP_JISX0208,
	/* 0xef */	EUCJP_JISX0208,
	/* 0xf0 */	EUCJP_JISX0208,
	/* 0xf1 */	EUCJP_JISX0208,
	/* 0xf2 */	EUCJP_JISX0208,
	/* 0xf3 */	EUCJP_JISX0208,
	/* 0xf4 */	EUCJP_JISX0208,
	/* 0xf5 */	EUCJP_JISX0208,
	/* 0xf6 */	EUCJP_JISX0208,
	/* 0xf7 */	EUCJP_JISX0208,
	/* 0xf8 */	EUCJP_JISX0208,
	/* 0xf9 */	EUCJP_JISX0208,
	/* 0xfa */	EUCJP_JISX0208,
	/* 0xfb */	EUCJP_JISX0208,
	/* 0xfc */	EUCJP_JISX0208,
	/* 0xfd */	EUCJP_JISX0208,
	/* 0xfe */	EUCJP_JISX0208,
	/* 0xff */	EUCJP_LATIN1,
};


static XP_Bool
fe_AreEUCJPFontsAvail(int16 win_csid)
{
	/* this needs to match the code in fe_LoadEUCJPFont */
	if ((fe_FontCharSets[CS_JISX0208 & 0xff].name == NULL)
		&& (fe_FontCharSets[CS_JISX0208_11 & 0xff].name == NULL))
		return FALSE;
	else
		return TRUE;
}

static fe_Font
fe_LoadEUCJPFont(MWContext *context, char *familyName, int points,
                 int sizeNum, int fontmask, int charset, int pitch,
                 int faceNum, Display *dpy)
{
	int16			cs;
	int			len;
	fe_FontGroupFont	*ret;

	len = (4 * sizeof(fe_FontGroupFont));
	ret = calloc(1, len);
	if (!ret)
	{
		return NULL;
	}
	(void) memset(ret, 0, len);

	/* this needs to match the code in fe_AreEUCJPFontsAvail */
	cs = CS_LATIN1;
	ret[0].xFont = (XFontStruct *)
            fe_LoadFont(context, &cs, familyName, points, sizeNum, fontmask);
	if (cs != CS_LATIN1)
	{
		XP_FREE(ret);
		return NULL;
	}

	cs = CS_JISX0208;
	ret[1].xFont = (XFontStruct *)
            fe_LoadFont(context, &cs, familyName, points, sizeNum, fontmask);
	if (cs == CS_JISX0208)
	{
		ret[1].mask1 = 0x7f;
		ret[1].mask2 = 0x7f;
	}
	else
	{
		cs = CS_JISX0208_11;
		ret[1].xFont = (XFontStruct *)
                    fe_LoadFont(context, &cs, familyName, points, sizeNum,
                                fontmask);
		if (cs == CS_JISX0208_11)
		{
			ret[1].mask1 = 0xff;
			ret[1].mask2 = 0xff;
		}
		else
		{
			XP_FREE(ret);
			return NULL;
		}
	}

	cs = CS_JISX0201;
	ret[2].xFont = (XFontStruct *)
            fe_LoadFont(context, &cs, familyName, points, sizeNum, fontmask);
	if (cs != CS_JISX0201)
	{
		ret[2].xFont = NULL;
	}

	cs = CS_JISX0212;
	ret[3].xFont = (XFontStruct *)
            fe_LoadFont(context, &cs, familyName, points, sizeNum, fontmask);
	if (cs == CS_JISX0212)
	{
		ret[3].mask1 = 0x7f;
		ret[3].mask2 = 0x7f;
	}
	else
	{
		ret[3].xFont = NULL;
	}

	return (fe_Font) ret;
}


static void
fe_EUCJPTextExtents(fe_Font font, char *string, int len, int *direction,
	int *fontAscent, int *fontDescent, XCharStruct *overall)
{
	fe_ProcessEUCString(CS_EUCJP, EUCJPTable, 
		NULL, 
		NULL,
		NULL, None, font, NULL, 0, 0, string, len,
		fontAscent, fontDescent, overall,
		fe_CharSetFuncsArray[FE_FONT_INFO_EUCJP].numberOfFonts);
}


static void
fe_DrawEUCJPString(Display *dpy, Drawable d, fe_Font font, GC gc, int x,
	int y, char *string, int len)
{
	fe_ProcessEUCString(CS_EUCJP, EUCJPTable, 
		(fe_XDrawStringFunc) XDrawString, 
		(fe_XDrawStringFunc) XDrawString16,
		dpy, d, font, gc, x, y, string, len,
		NULL, NULL, NULL,
		0);
}


static void
fe_DrawEUCJPImageString(Display *dpy, Drawable d, fe_Font font, GC gc, GC gc2,
	int x, int y, char *string, int len)
{
	fe_ProcessEUCString(CS_EUCJP, EUCJPTable, 
		(fe_XDrawStringFunc) XDrawImageString,
		(fe_XDrawStringFunc) XDrawImageString16,
		dpy, d, font, gc, x, y, string, len,
		NULL, NULL, NULL,
		0);
}


			/*	fontIndex	skip	len	*/
#define EUCKR_LATIN1	{	0,		0,	1	}
#define EUCKR_KSC5601	{	1,		0,	2	}

static fe_StringProcessTable EUCKRTable[] =
{
	/* 0x00 */	EUCKR_LATIN1,
	/* 0x01 */	EUCKR_LATIN1,
	/* 0x02 */	EUCKR_LATIN1,
	/* 0x03 */	EUCKR_LATIN1,
	/* 0x04 */	EUCKR_LATIN1,
	/* 0x05 */	EUCKR_LATIN1,
	/* 0x06 */	EUCKR_LATIN1,
	/* 0x07 */	EUCKR_LATIN1,
	/* 0x08 */	EUCKR_LATIN1,
	/* 0x09 */	EUCKR_LATIN1,
	/* 0x0a */	EUCKR_LATIN1,
	/* 0x0b */	EUCKR_LATIN1,
	/* 0x0c */	EUCKR_LATIN1,
	/* 0x0d */	EUCKR_LATIN1,
	/* 0x0e */	EUCKR_LATIN1,
	/* 0x0f */	EUCKR_LATIN1,
	/* 0x10 */	EUCKR_LATIN1,
	/* 0x11 */	EUCKR_LATIN1,
	/* 0x12 */	EUCKR_LATIN1,
	/* 0x13 */	EUCKR_LATIN1,
	/* 0x14 */	EUCKR_LATIN1,
	/* 0x15 */	EUCKR_LATIN1,
	/* 0x16 */	EUCKR_LATIN1,
	/* 0x17 */	EUCKR_LATIN1,
	/* 0x18 */	EUCKR_LATIN1,
	/* 0x19 */	EUCKR_LATIN1,
	/* 0x1a */	EUCKR_LATIN1,
	/* 0x1b */	EUCKR_LATIN1,
	/* 0x1c */	EUCKR_LATIN1,
	/* 0x1d */	EUCKR_LATIN1,
	/* 0x1e */	EUCKR_LATIN1,
	/* 0x1f */	EUCKR_LATIN1,
	/* 0x20 */	EUCKR_LATIN1,
	/* 0x21 */	EUCKR_LATIN1,
	/* 0x22 */	EUCKR_LATIN1,
	/* 0x23 */	EUCKR_LATIN1,
	/* 0x24 */	EUCKR_LATIN1,
	/* 0x25 */	EUCKR_LATIN1,
	/* 0x26 */	EUCKR_LATIN1,
	/* 0x27 */	EUCKR_LATIN1,
	/* 0x28 */	EUCKR_LATIN1,
	/* 0x29 */	EUCKR_LATIN1,
	/* 0x2a */	EUCKR_LATIN1,
	/* 0x2b */	EUCKR_LATIN1,
	/* 0x2c */	EUCKR_LATIN1,
	/* 0x2d */	EUCKR_LATIN1,
	/* 0x2e */	EUCKR_LATIN1,
	/* 0x2f */	EUCKR_LATIN1,
	/* 0x30 */	EUCKR_LATIN1,
	/* 0x31 */	EUCKR_LATIN1,
	/* 0x32 */	EUCKR_LATIN1,
	/* 0x33 */	EUCKR_LATIN1,
	/* 0x34 */	EUCKR_LATIN1,
	/* 0x35 */	EUCKR_LATIN1,
	/* 0x36 */	EUCKR_LATIN1,
	/* 0x37 */	EUCKR_LATIN1,
	/* 0x38 */	EUCKR_LATIN1,
	/* 0x39 */	EUCKR_LATIN1,
	/* 0x3a */	EUCKR_LATIN1,
	/* 0x3b */	EUCKR_LATIN1,
	/* 0x3c */	EUCKR_LATIN1,
	/* 0x3d */	EUCKR_LATIN1,
	/* 0x3e */	EUCKR_LATIN1,
	/* 0x3f */	EUCKR_LATIN1,
	/* 0x40 */	EUCKR_LATIN1,
	/* 0x41 */	EUCKR_LATIN1,
	/* 0x42 */	EUCKR_LATIN1,
	/* 0x43 */	EUCKR_LATIN1,
	/* 0x44 */	EUCKR_LATIN1,
	/* 0x45 */	EUCKR_LATIN1,
	/* 0x46 */	EUCKR_LATIN1,
	/* 0x47 */	EUCKR_LATIN1,
	/* 0x48 */	EUCKR_LATIN1,
	/* 0x49 */	EUCKR_LATIN1,
	/* 0x4a */	EUCKR_LATIN1,
	/* 0x4b */	EUCKR_LATIN1,
	/* 0x4c */	EUCKR_LATIN1,
	/* 0x4d */	EUCKR_LATIN1,
	/* 0x4e */	EUCKR_LATIN1,
	/* 0x4f */	EUCKR_LATIN1,
	/* 0x50 */	EUCKR_LATIN1,
	/* 0x51 */	EUCKR_LATIN1,
	/* 0x52 */	EUCKR_LATIN1,
	/* 0x53 */	EUCKR_LATIN1,
	/* 0x54 */	EUCKR_LATIN1,
	/* 0x55 */	EUCKR_LATIN1,
	/* 0x56 */	EUCKR_LATIN1,
	/* 0x57 */	EUCKR_LATIN1,
	/* 0x58 */	EUCKR_LATIN1,
	/* 0x59 */	EUCKR_LATIN1,
	/* 0x5a */	EUCKR_LATIN1,
	/* 0x5b */	EUCKR_LATIN1,
	/* 0x5c */	EUCKR_LATIN1,
	/* 0x5d */	EUCKR_LATIN1,
	/* 0x5e */	EUCKR_LATIN1,
	/* 0x5f */	EUCKR_LATIN1,
	/* 0x60 */	EUCKR_LATIN1,
	/* 0x61 */	EUCKR_LATIN1,
	/* 0x62 */	EUCKR_LATIN1,
	/* 0x63 */	EUCKR_LATIN1,
	/* 0x64 */	EUCKR_LATIN1,
	/* 0x65 */	EUCKR_LATIN1,
	/* 0x66 */	EUCKR_LATIN1,
	/* 0x67 */	EUCKR_LATIN1,
	/* 0x68 */	EUCKR_LATIN1,
	/* 0x69 */	EUCKR_LATIN1,
	/* 0x6a */	EUCKR_LATIN1,
	/* 0x6b */	EUCKR_LATIN1,
	/* 0x6c */	EUCKR_LATIN1,
	/* 0x6d */	EUCKR_LATIN1,
	/* 0x6e */	EUCKR_LATIN1,
	/* 0x6f */	EUCKR_LATIN1,
	/* 0x70 */	EUCKR_LATIN1,
	/* 0x71 */	EUCKR_LATIN1,
	/* 0x72 */	EUCKR_LATIN1,
	/* 0x73 */	EUCKR_LATIN1,
	/* 0x74 */	EUCKR_LATIN1,
	/* 0x75 */	EUCKR_LATIN1,
	/* 0x76 */	EUCKR_LATIN1,
	/* 0x77 */	EUCKR_LATIN1,
	/* 0x78 */	EUCKR_LATIN1,
	/* 0x79 */	EUCKR_LATIN1,
	/* 0x7a */	EUCKR_LATIN1,
	/* 0x7b */	EUCKR_LATIN1,
	/* 0x7c */	EUCKR_LATIN1,
	/* 0x7d */	EUCKR_LATIN1,
	/* 0x7e */	EUCKR_LATIN1,
	/* 0x7f */	EUCKR_LATIN1,
	/* 0x80 */	EUCKR_LATIN1,
	/* 0x81 */	EUCKR_LATIN1,
	/* 0x82 */	EUCKR_LATIN1,
	/* 0x83 */	EUCKR_LATIN1,
	/* 0x84 */	EUCKR_LATIN1,
	/* 0x85 */	EUCKR_LATIN1,
	/* 0x86 */	EUCKR_LATIN1,
	/* 0x87 */	EUCKR_LATIN1,
	/* 0x88 */	EUCKR_LATIN1,
	/* 0x89 */	EUCKR_LATIN1,
	/* 0x8a */	EUCKR_LATIN1,
	/* 0x8b */	EUCKR_LATIN1,
	/* 0x8c */	EUCKR_LATIN1,
	/* 0x8d */	EUCKR_LATIN1,
	/* 0x8e */	EUCKR_LATIN1,
	/* 0x8f */	EUCKR_LATIN1,
	/* 0x90 */	EUCKR_LATIN1,
	/* 0x91 */	EUCKR_LATIN1,
	/* 0x92 */	EUCKR_LATIN1,
	/* 0x93 */	EUCKR_LATIN1,
	/* 0x94 */	EUCKR_LATIN1,
	/* 0x95 */	EUCKR_LATIN1,
	/* 0x96 */	EUCKR_LATIN1,
	/* 0x97 */	EUCKR_LATIN1,
	/* 0x98 */	EUCKR_LATIN1,
	/* 0x99 */	EUCKR_LATIN1,
	/* 0x9a */	EUCKR_LATIN1,
	/* 0x9b */	EUCKR_LATIN1,
	/* 0x9c */	EUCKR_LATIN1,
	/* 0x9d */	EUCKR_LATIN1,
	/* 0x9e */	EUCKR_LATIN1,
	/* 0x9f */	EUCKR_LATIN1,
	/* 0xa0 */	EUCKR_LATIN1,
	/* 0xa1 */	EUCKR_KSC5601,
	/* 0xa2 */	EUCKR_KSC5601,
	/* 0xa3 */	EUCKR_KSC5601,
	/* 0xa4 */	EUCKR_KSC5601,
	/* 0xa5 */	EUCKR_KSC5601,
	/* 0xa6 */	EUCKR_KSC5601,
	/* 0xa7 */	EUCKR_KSC5601,
	/* 0xa8 */	EUCKR_KSC5601,
	/* 0xa9 */	EUCKR_KSC5601,
	/* 0xaa */	EUCKR_KSC5601,
	/* 0xab */	EUCKR_KSC5601,
	/* 0xac */	EUCKR_KSC5601,
	/* 0xad */	EUCKR_KSC5601,
	/* 0xae */	EUCKR_KSC5601,
	/* 0xaf */	EUCKR_KSC5601,
	/* 0xb0 */	EUCKR_KSC5601,
	/* 0xb1 */	EUCKR_KSC5601,
	/* 0xb2 */	EUCKR_KSC5601,
	/* 0xb3 */	EUCKR_KSC5601,
	/* 0xb4 */	EUCKR_KSC5601,
	/* 0xb5 */	EUCKR_KSC5601,
	/* 0xb6 */	EUCKR_KSC5601,
	/* 0xb7 */	EUCKR_KSC5601,
	/* 0xb8 */	EUCKR_KSC5601,
	/* 0xb9 */	EUCKR_KSC5601,
	/* 0xba */	EUCKR_KSC5601,
	/* 0xbb */	EUCKR_KSC5601,
	/* 0xbc */	EUCKR_KSC5601,
	/* 0xbd */	EUCKR_KSC5601,
	/* 0xbe */	EUCKR_KSC5601,
	/* 0xbf */	EUCKR_KSC5601,
	/* 0xc0 */	EUCKR_KSC5601,
	/* 0xc1 */	EUCKR_KSC5601,
	/* 0xc2 */	EUCKR_KSC5601,
	/* 0xc3 */	EUCKR_KSC5601,
	/* 0xc4 */	EUCKR_KSC5601,
	/* 0xc5 */	EUCKR_KSC5601,
	/* 0xc6 */	EUCKR_KSC5601,
	/* 0xc7 */	EUCKR_KSC5601,
	/* 0xc8 */	EUCKR_KSC5601,
	/* 0xc9 */	EUCKR_KSC5601,
	/* 0xca */	EUCKR_KSC5601,
	/* 0xcb */	EUCKR_KSC5601,
	/* 0xcc */	EUCKR_KSC5601,
	/* 0xcd */	EUCKR_KSC5601,
	/* 0xce */	EUCKR_KSC5601,
	/* 0xcf */	EUCKR_KSC5601,
	/* 0xd0 */	EUCKR_KSC5601,
	/* 0xd1 */	EUCKR_KSC5601,
	/* 0xd2 */	EUCKR_KSC5601,
	/* 0xd3 */	EUCKR_KSC5601,
	/* 0xd4 */	EUCKR_KSC5601,
	/* 0xd5 */	EUCKR_KSC5601,
	/* 0xd6 */	EUCKR_KSC5601,
	/* 0xd7 */	EUCKR_KSC5601,
	/* 0xd8 */	EUCKR_KSC5601,
	/* 0xd9 */	EUCKR_KSC5601,
	/* 0xda */	EUCKR_KSC5601,
	/* 0xdb */	EUCKR_KSC5601,
	/* 0xdc */	EUCKR_KSC5601,
	/* 0xdd */	EUCKR_KSC5601,
	/* 0xde */	EUCKR_KSC5601,
	/* 0xdf */	EUCKR_KSC5601,
	/* 0xe0 */	EUCKR_KSC5601,
	/* 0xe1 */	EUCKR_KSC5601,
	/* 0xe2 */	EUCKR_KSC5601,
	/* 0xe3 */	EUCKR_KSC5601,
	/* 0xe4 */	EUCKR_KSC5601,
	/* 0xe5 */	EUCKR_KSC5601,
	/* 0xe6 */	EUCKR_KSC5601,
	/* 0xe7 */	EUCKR_KSC5601,
	/* 0xe8 */	EUCKR_KSC5601,
	/* 0xe9 */	EUCKR_KSC5601,
	/* 0xea */	EUCKR_KSC5601,
	/* 0xeb */	EUCKR_KSC5601,
	/* 0xec */	EUCKR_KSC5601,
	/* 0xed */	EUCKR_KSC5601,
	/* 0xee */	EUCKR_KSC5601,
	/* 0xef */	EUCKR_KSC5601,
	/* 0xf0 */	EUCKR_KSC5601,
	/* 0xf1 */	EUCKR_KSC5601,
	/* 0xf2 */	EUCKR_KSC5601,
	/* 0xf3 */	EUCKR_KSC5601,
	/* 0xf4 */	EUCKR_KSC5601,
	/* 0xf5 */	EUCKR_KSC5601,
	/* 0xf6 */	EUCKR_KSC5601,
	/* 0xf7 */	EUCKR_KSC5601,
	/* 0xf8 */	EUCKR_KSC5601,
	/* 0xf9 */	EUCKR_KSC5601,
	/* 0xfa */	EUCKR_KSC5601,
	/* 0xfb */	EUCKR_KSC5601,
	/* 0xfc */	EUCKR_KSC5601,
	/* 0xfd */	EUCKR_KSC5601,
	/* 0xfe */	EUCKR_KSC5601,
	/* 0xff */	EUCKR_LATIN1,
};


static XP_Bool
fe_AreEUCKRFontsAvail(int16 win_csid)
{
	/* this needs to match the code in fe_LoadEUCKRFont */
	if ((fe_FontCharSets[CS_KSC5601 & 0xff].name == NULL)
		&& (fe_FontCharSets[CS_KSC5601_11 & 0xff].name == NULL))
		return FALSE;
	else
		return TRUE;
}

static fe_Font
fe_LoadEUCKRFont(MWContext *context, char *familyName, int points,
                 int sizeNum, int fontmask, int charset, int pitch,
                 int faceNum, Display *dpy)
{
	int16			cs;
	int			len;
	fe_FontGroupFont	*ret;

	len = (2 * sizeof(fe_FontGroupFont));
	ret = calloc(1, len);
	if (!ret)
	{
		return NULL;
	}
	(void) memset(ret, 0, len);

	cs = CS_LATIN1;
	ret[0].xFont = (XFontStruct *)
            fe_LoadFont(context, &cs, familyName, points, sizeNum, fontmask);
	if (cs != CS_LATIN1)
	{
		XP_FREE(ret);
		return NULL;
	}

	/* this needs to match the code in fe_AreEUCKRFontsAvail */
	cs = CS_KSC5601;
	ret[1].xFont = (XFontStruct *)
            fe_LoadFont(context, &cs, familyName, points, sizeNum, fontmask);
	if (cs == CS_KSC5601)
	{
		ret[1].mask1 = 0x7f;
		ret[1].mask2 = 0x7f;
	}
	else
	{
		cs = CS_KSC5601_11;
		ret[1].xFont = (XFontStruct *)
                    fe_LoadFont(context, &cs, familyName, points, sizeNum,
                                fontmask);
		if (cs == CS_KSC5601_11)
		{
			ret[1].mask1 = 0xff;
			ret[1].mask2 = 0xff;
		}
		else
		{
			XP_FREE(ret);
			return NULL;
		}
	}

	return (fe_Font) ret;
}


static void
fe_EUCKRTextExtents(fe_Font font, char *string, int len, int *direction,
	int *fontAscent, int *fontDescent, XCharStruct *overall)
{
	fe_ProcessEUCString(CS_KSC_8BIT, EUCKRTable, 
		NULL, 
		NULL,
		NULL, None, font, NULL, 0, 0, string, len,
		fontAscent, fontDescent, overall,
		fe_CharSetFuncsArray[FE_FONT_INFO_EUCKR].numberOfFonts);
}


static void
fe_DrawEUCKRString(Display *dpy, Drawable d, fe_Font font, GC gc, int x,
	int y, char *string, int len)
{
	fe_ProcessEUCString(CS_KSC_8BIT, EUCKRTable, 
		(fe_XDrawStringFunc) XDrawString, 
		(fe_XDrawStringFunc) XDrawString16,
		dpy, d, font, gc, x, y, string, len,
		NULL, NULL, NULL,
		0);
}


static void
fe_DrawEUCKRImageString(Display *dpy, Drawable d, fe_Font font, GC gc, GC gc2,
	int x, int y, char *string, int len)
{
	fe_ProcessEUCString(CS_KSC_8BIT, EUCKRTable, 
		(fe_XDrawStringFunc) XDrawImageString,
		(fe_XDrawStringFunc) XDrawImageString16,
		dpy, d, font, gc, x, y, string, len,
		NULL, NULL, NULL,
		0);
}


				/*	fontIndex	skip	len	*/
#define EUCTW_LATIN1		{	0,		0,	1	}
#define EUCTW_CNS11643_1	{	1,		0,	2	}
#define EUCTW_CNS11643_2	{	2,		2,	2	}

static fe_StringProcessTable EUCTWTable[] =
{
	/* 0x00 */	EUCTW_LATIN1,
	/* 0x01 */	EUCTW_LATIN1,
	/* 0x02 */	EUCTW_LATIN1,
	/* 0x03 */	EUCTW_LATIN1,
	/* 0x04 */	EUCTW_LATIN1,
	/* 0x05 */	EUCTW_LATIN1,
	/* 0x06 */	EUCTW_LATIN1,
	/* 0x07 */	EUCTW_LATIN1,
	/* 0x08 */	EUCTW_LATIN1,
	/* 0x09 */	EUCTW_LATIN1,
	/* 0x0a */	EUCTW_LATIN1,
	/* 0x0b */	EUCTW_LATIN1,
	/* 0x0c */	EUCTW_LATIN1,
	/* 0x0d */	EUCTW_LATIN1,
	/* 0x0e */	EUCTW_LATIN1,
	/* 0x0f */	EUCTW_LATIN1,
	/* 0x10 */	EUCTW_LATIN1,
	/* 0x11 */	EUCTW_LATIN1,
	/* 0x12 */	EUCTW_LATIN1,
	/* 0x13 */	EUCTW_LATIN1,
	/* 0x14 */	EUCTW_LATIN1,
	/* 0x15 */	EUCTW_LATIN1,
	/* 0x16 */	EUCTW_LATIN1,
	/* 0x17 */	EUCTW_LATIN1,
	/* 0x18 */	EUCTW_LATIN1,
	/* 0x19 */	EUCTW_LATIN1,
	/* 0x1a */	EUCTW_LATIN1,
	/* 0x1b */	EUCTW_LATIN1,
	/* 0x1c */	EUCTW_LATIN1,
	/* 0x1d */	EUCTW_LATIN1,
	/* 0x1e */	EUCTW_LATIN1,
	/* 0x1f */	EUCTW_LATIN1,
	/* 0x20 */	EUCTW_LATIN1,
	/* 0x21 */	EUCTW_LATIN1,
	/* 0x22 */	EUCTW_LATIN1,
	/* 0x23 */	EUCTW_LATIN1,
	/* 0x24 */	EUCTW_LATIN1,
	/* 0x25 */	EUCTW_LATIN1,
	/* 0x26 */	EUCTW_LATIN1,
	/* 0x27 */	EUCTW_LATIN1,
	/* 0x28 */	EUCTW_LATIN1,
	/* 0x29 */	EUCTW_LATIN1,
	/* 0x2a */	EUCTW_LATIN1,
	/* 0x2b */	EUCTW_LATIN1,
	/* 0x2c */	EUCTW_LATIN1,
	/* 0x2d */	EUCTW_LATIN1,
	/* 0x2e */	EUCTW_LATIN1,
	/* 0x2f */	EUCTW_LATIN1,
	/* 0x30 */	EUCTW_LATIN1,
	/* 0x31 */	EUCTW_LATIN1,
	/* 0x32 */	EUCTW_LATIN1,
	/* 0x33 */	EUCTW_LATIN1,
	/* 0x34 */	EUCTW_LATIN1,
	/* 0x35 */	EUCTW_LATIN1,
	/* 0x36 */	EUCTW_LATIN1,
	/* 0x37 */	EUCTW_LATIN1,
	/* 0x38 */	EUCTW_LATIN1,
	/* 0x39 */	EUCTW_LATIN1,
	/* 0x3a */	EUCTW_LATIN1,
	/* 0x3b */	EUCTW_LATIN1,
	/* 0x3c */	EUCTW_LATIN1,
	/* 0x3d */	EUCTW_LATIN1,
	/* 0x3e */	EUCTW_LATIN1,
	/* 0x3f */	EUCTW_LATIN1,
	/* 0x40 */	EUCTW_LATIN1,
	/* 0x41 */	EUCTW_LATIN1,
	/* 0x42 */	EUCTW_LATIN1,
	/* 0x43 */	EUCTW_LATIN1,
	/* 0x44 */	EUCTW_LATIN1,
	/* 0x45 */	EUCTW_LATIN1,
	/* 0x46 */	EUCTW_LATIN1,
	/* 0x47 */	EUCTW_LATIN1,
	/* 0x48 */	EUCTW_LATIN1,
	/* 0x49 */	EUCTW_LATIN1,
	/* 0x4a */	EUCTW_LATIN1,
	/* 0x4b */	EUCTW_LATIN1,
	/* 0x4c */	EUCTW_LATIN1,
	/* 0x4d */	EUCTW_LATIN1,
	/* 0x4e */	EUCTW_LATIN1,
	/* 0x4f */	EUCTW_LATIN1,
	/* 0x50 */	EUCTW_LATIN1,
	/* 0x51 */	EUCTW_LATIN1,
	/* 0x52 */	EUCTW_LATIN1,
	/* 0x53 */	EUCTW_LATIN1,
	/* 0x54 */	EUCTW_LATIN1,
	/* 0x55 */	EUCTW_LATIN1,
	/* 0x56 */	EUCTW_LATIN1,
	/* 0x57 */	EUCTW_LATIN1,
	/* 0x58 */	EUCTW_LATIN1,
	/* 0x59 */	EUCTW_LATIN1,
	/* 0x5a */	EUCTW_LATIN1,
	/* 0x5b */	EUCTW_LATIN1,
	/* 0x5c */	EUCTW_LATIN1,
	/* 0x5d */	EUCTW_LATIN1,
	/* 0x5e */	EUCTW_LATIN1,
	/* 0x5f */	EUCTW_LATIN1,
	/* 0x60 */	EUCTW_LATIN1,
	/* 0x61 */	EUCTW_LATIN1,
	/* 0x62 */	EUCTW_LATIN1,
	/* 0x63 */	EUCTW_LATIN1,
	/* 0x64 */	EUCTW_LATIN1,
	/* 0x65 */	EUCTW_LATIN1,
	/* 0x66 */	EUCTW_LATIN1,
	/* 0x67 */	EUCTW_LATIN1,
	/* 0x68 */	EUCTW_LATIN1,
	/* 0x69 */	EUCTW_LATIN1,
	/* 0x6a */	EUCTW_LATIN1,
	/* 0x6b */	EUCTW_LATIN1,
	/* 0x6c */	EUCTW_LATIN1,
	/* 0x6d */	EUCTW_LATIN1,
	/* 0x6e */	EUCTW_LATIN1,
	/* 0x6f */	EUCTW_LATIN1,
	/* 0x70 */	EUCTW_LATIN1,
	/* 0x71 */	EUCTW_LATIN1,
	/* 0x72 */	EUCTW_LATIN1,
	/* 0x73 */	EUCTW_LATIN1,
	/* 0x74 */	EUCTW_LATIN1,
	/* 0x75 */	EUCTW_LATIN1,
	/* 0x76 */	EUCTW_LATIN1,
	/* 0x77 */	EUCTW_LATIN1,
	/* 0x78 */	EUCTW_LATIN1,
	/* 0x79 */	EUCTW_LATIN1,
	/* 0x7a */	EUCTW_LATIN1,
	/* 0x7b */	EUCTW_LATIN1,
	/* 0x7c */	EUCTW_LATIN1,
	/* 0x7d */	EUCTW_LATIN1,
	/* 0x7e */	EUCTW_LATIN1,
	/* 0x7f */	EUCTW_LATIN1,
	/* 0x80 */	EUCTW_LATIN1,
	/* 0x81 */	EUCTW_LATIN1,
	/* 0x82 */	EUCTW_LATIN1,
	/* 0x83 */	EUCTW_LATIN1,
	/* 0x84 */	EUCTW_LATIN1,
	/* 0x85 */	EUCTW_LATIN1,
	/* 0x86 */	EUCTW_LATIN1,
	/* 0x87 */	EUCTW_LATIN1,
	/* 0x88 */	EUCTW_LATIN1,
	/* 0x89 */	EUCTW_LATIN1,
	/* 0x8a */	EUCTW_LATIN1,
	/* 0x8b */	EUCTW_LATIN1,
	/* 0x8c */	EUCTW_LATIN1,
	/* 0x8d */	EUCTW_LATIN1,
	/* 0x8e */	EUCTW_CNS11643_2,
	/* 0x8f */	EUCTW_LATIN1,
	/* 0x90 */	EUCTW_LATIN1,
	/* 0x91 */	EUCTW_LATIN1,
	/* 0x92 */	EUCTW_LATIN1,
	/* 0x93 */	EUCTW_LATIN1,
	/* 0x94 */	EUCTW_LATIN1,
	/* 0x95 */	EUCTW_LATIN1,
	/* 0x96 */	EUCTW_LATIN1,
	/* 0x97 */	EUCTW_LATIN1,
	/* 0x98 */	EUCTW_LATIN1,
	/* 0x99 */	EUCTW_LATIN1,
	/* 0x9a */	EUCTW_LATIN1,
	/* 0x9b */	EUCTW_LATIN1,
	/* 0x9c */	EUCTW_LATIN1,
	/* 0x9d */	EUCTW_LATIN1,
	/* 0x9e */	EUCTW_LATIN1,
	/* 0x9f */	EUCTW_LATIN1,
	/* 0xa0 */	EUCTW_LATIN1,
	/* 0xa1 */	EUCTW_CNS11643_1,
	/* 0xa2 */	EUCTW_CNS11643_1,
	/* 0xa3 */	EUCTW_CNS11643_1,
	/* 0xa4 */	EUCTW_CNS11643_1,
	/* 0xa5 */	EUCTW_CNS11643_1,
	/* 0xa6 */	EUCTW_CNS11643_1,
	/* 0xa7 */	EUCTW_CNS11643_1,
	/* 0xa8 */	EUCTW_CNS11643_1,
	/* 0xa9 */	EUCTW_CNS11643_1,
	/* 0xaa */	EUCTW_CNS11643_1,
	/* 0xab */	EUCTW_CNS11643_1,
	/* 0xac */	EUCTW_CNS11643_1,
	/* 0xad */	EUCTW_CNS11643_1,
	/* 0xae */	EUCTW_CNS11643_1,
	/* 0xaf */	EUCTW_CNS11643_1,
	/* 0xb0 */	EUCTW_CNS11643_1,
	/* 0xb1 */	EUCTW_CNS11643_1,
	/* 0xb2 */	EUCTW_CNS11643_1,
	/* 0xb3 */	EUCTW_CNS11643_1,
	/* 0xb4 */	EUCTW_CNS11643_1,
	/* 0xb5 */	EUCTW_CNS11643_1,
	/* 0xb6 */	EUCTW_CNS11643_1,
	/* 0xb7 */	EUCTW_CNS11643_1,
	/* 0xb8 */	EUCTW_CNS11643_1,
	/* 0xb9 */	EUCTW_CNS11643_1,
	/* 0xba */	EUCTW_CNS11643_1,
	/* 0xbb */	EUCTW_CNS11643_1,
	/* 0xbc */	EUCTW_CNS11643_1,
	/* 0xbd */	EUCTW_CNS11643_1,
	/* 0xbe */	EUCTW_CNS11643_1,
	/* 0xbf */	EUCTW_CNS11643_1,
	/* 0xc0 */	EUCTW_CNS11643_1,
	/* 0xc1 */	EUCTW_CNS11643_1,
	/* 0xc2 */	EUCTW_CNS11643_1,
	/* 0xc3 */	EUCTW_CNS11643_1,
	/* 0xc4 */	EUCTW_CNS11643_1,
	/* 0xc5 */	EUCTW_CNS11643_1,
	/* 0xc6 */	EUCTW_CNS11643_1,
	/* 0xc7 */	EUCTW_CNS11643_1,
	/* 0xc8 */	EUCTW_CNS11643_1,
	/* 0xc9 */	EUCTW_CNS11643_1,
	/* 0xca */	EUCTW_CNS11643_1,
	/* 0xcb */	EUCTW_CNS11643_1,
	/* 0xcc */	EUCTW_CNS11643_1,
	/* 0xcd */	EUCTW_CNS11643_1,
	/* 0xce */	EUCTW_CNS11643_1,
	/* 0xcf */	EUCTW_CNS11643_1,
	/* 0xd0 */	EUCTW_CNS11643_1,
	/* 0xd1 */	EUCTW_CNS11643_1,
	/* 0xd2 */	EUCTW_CNS11643_1,
	/* 0xd3 */	EUCTW_CNS11643_1,
	/* 0xd4 */	EUCTW_CNS11643_1,
	/* 0xd5 */	EUCTW_CNS11643_1,
	/* 0xd6 */	EUCTW_CNS11643_1,
	/* 0xd7 */	EUCTW_CNS11643_1,
	/* 0xd8 */	EUCTW_CNS11643_1,
	/* 0xd9 */	EUCTW_CNS11643_1,
	/* 0xda */	EUCTW_CNS11643_1,
	/* 0xdb */	EUCTW_CNS11643_1,
	/* 0xdc */	EUCTW_CNS11643_1,
	/* 0xdd */	EUCTW_CNS11643_1,
	/* 0xde */	EUCTW_CNS11643_1,
	/* 0xdf */	EUCTW_CNS11643_1,
	/* 0xe0 */	EUCTW_CNS11643_1,
	/* 0xe1 */	EUCTW_CNS11643_1,
	/* 0xe2 */	EUCTW_CNS11643_1,
	/* 0xe3 */	EUCTW_CNS11643_1,
	/* 0xe4 */	EUCTW_CNS11643_1,
	/* 0xe5 */	EUCTW_CNS11643_1,
	/* 0xe6 */	EUCTW_CNS11643_1,
	/* 0xe7 */	EUCTW_CNS11643_1,
	/* 0xe8 */	EUCTW_CNS11643_1,
	/* 0xe9 */	EUCTW_CNS11643_1,
	/* 0xea */	EUCTW_CNS11643_1,
	/* 0xeb */	EUCTW_CNS11643_1,
	/* 0xec */	EUCTW_CNS11643_1,
	/* 0xed */	EUCTW_CNS11643_1,
	/* 0xee */	EUCTW_CNS11643_1,
	/* 0xef */	EUCTW_CNS11643_1,
	/* 0xf0 */	EUCTW_CNS11643_1,
	/* 0xf1 */	EUCTW_CNS11643_1,
	/* 0xf2 */	EUCTW_CNS11643_1,
	/* 0xf3 */	EUCTW_CNS11643_1,
	/* 0xf4 */	EUCTW_CNS11643_1,
	/* 0xf5 */	EUCTW_CNS11643_1,
	/* 0xf6 */	EUCTW_CNS11643_1,
	/* 0xf7 */	EUCTW_CNS11643_1,
	/* 0xf8 */	EUCTW_CNS11643_1,
	/* 0xf9 */	EUCTW_CNS11643_1,
	/* 0xfa */	EUCTW_CNS11643_1,
	/* 0xfb */	EUCTW_CNS11643_1,
	/* 0xfc */	EUCTW_CNS11643_1,
	/* 0xfd */	EUCTW_CNS11643_1,
	/* 0xfe */	EUCTW_CNS11643_1,
	/* 0xff */	EUCTW_LATIN1,
};


static XP_Bool
fe_AreEUCTWFontsAvail(int16 win_csid)
{
	/* this needs to match the code in fe_LoadEUCTWFont */
	if (((fe_FontCharSets[CS_CNS11643_1 & 0xff].name == NULL)
		|| (fe_FontCharSets[CS_CNS11643_2 & 0xff].name == NULL))
		&& (fe_FontCharSets[CS_CNS11643_1110 & 0xff].name == NULL))
		return FALSE;
	else
		return TRUE;
}

static fe_Font
fe_LoadEUCTWFont(MWContext *context, char *familyName, int points,
                 int sizeNum, int fontmask, int charset, int pitch,
                 int faceNum, Display *dpy)
{
	int16			cs;
	int			len;
	fe_FontGroupFont	*ret;

	len = (3 * sizeof(fe_FontGroupFont));
	ret = calloc(1, len);
	if (!ret)
	{
		return NULL;
	}
	(void) memset(ret, 0, len);

	cs = CS_LATIN1;
	ret[0].xFont = (XFontStruct *)
            fe_LoadFont(context, &cs, familyName, points, sizeNum, fontmask);
	if (cs != CS_LATIN1)
	{
		XP_FREE(ret);
		return NULL;
	}

	/* this needs to match the code in fe_AreEUCTWFontsAvail */
	cs = CS_CNS11643_1;
	ret[1].xFont = (XFontStruct *)
            fe_LoadFont(context, &cs, familyName, points, sizeNum, fontmask);
	if (cs == CS_CNS11643_1)
	{
		ret[1].mask1 = 0x7f;
		ret[1].mask2 = 0x7f;
	}
	else
	{
		ret[1].xFont = NULL;
	}

	cs = CS_CNS11643_2;
	ret[2].xFont = (XFontStruct *)
            fe_LoadFont(context, &cs, familyName, points, sizeNum, fontmask);
	if (cs == CS_CNS11643_2)
	{
		ret[2].mask1 = 0x7f;
		ret[2].mask2 = 0x7f;
	}
	else
	{
		ret[2].xFont = NULL;
	}

	if ((!ret[1].xFont) || (!ret[2].xFont))
	{
		cs = CS_CNS11643_1110;
		ret[1].xFont = (XFontStruct *)
                    fe_LoadFont(context, &cs, familyName, points, sizeNum,
                                fontmask);
		if (cs == CS_CNS11643_1110)
		{
			ret[1].mask1 = 0xff;
			ret[1].mask2 = 0xff;

			ret[2].xFont = ret[1].xFont;
			ret[2].mask1 = 0xff;
			ret[2].mask2 = 0x7f;
		}
		else
		{
			XP_FREE(ret);
			return NULL;
		}
	}

	return (fe_Font) ret;
}


static void
fe_EUCTWTextExtents(fe_Font font, char *string, int len, int *direction,
	int *fontAscent, int *fontDescent, XCharStruct *overall)
{
	fe_ProcessEUCString(CS_CNS_8BIT, EUCTWTable, 
		NULL, 
		NULL,
		NULL, None, font, NULL, 0, 0, string, len,
		fontAscent, fontDescent, overall,
		fe_CharSetFuncsArray[FE_FONT_INFO_EUCTW].numberOfFonts);
}


static void
fe_DrawEUCTWString(Display *dpy, Drawable d, fe_Font font, GC gc, int x,
	int y, char *string, int len)
{
	fe_ProcessEUCString(CS_CNS_8BIT, EUCTWTable, 
		(fe_XDrawStringFunc) XDrawString,
		(fe_XDrawStringFunc) XDrawString16,
		dpy, d, font, gc, x, y, string, len,
		NULL, NULL, NULL,
		0);
}


static void
fe_DrawEUCTWImageString(Display *dpy, Drawable d, fe_Font font, GC gc, GC gc2,
	int x, int y, char *string, int len)
{
	fe_ProcessEUCString(CS_CNS_8BIT, EUCTWTable, 
		(fe_XDrawStringFunc) XDrawImageString,
		(fe_XDrawStringFunc) XDrawImageString16,
		dpy, d, font, gc, x, y, string, len,
		NULL, NULL, NULL,
		0);
}


				/*	fontIndex	skip	len	*/
#define BIG5_LATIN1		{	0,		0,	1	}
#define BIG5_BIG5		{	1,		0,	2	}

static fe_StringProcessTable BIG5Table[] =
{
	/* 0x00 */	BIG5_LATIN1,
	/* 0x01 */	BIG5_LATIN1,
	/* 0x02 */	BIG5_LATIN1,
	/* 0x03 */	BIG5_LATIN1,
	/* 0x04 */	BIG5_LATIN1,
	/* 0x05 */	BIG5_LATIN1,
	/* 0x06 */	BIG5_LATIN1,
	/* 0x07 */	BIG5_LATIN1,
	/* 0x08 */	BIG5_LATIN1,
	/* 0x09 */	BIG5_LATIN1,
	/* 0x0a */	BIG5_LATIN1,
	/* 0x0b */	BIG5_LATIN1,
	/* 0x0c */	BIG5_LATIN1,
	/* 0x0d */	BIG5_LATIN1,
	/* 0x0e */	BIG5_LATIN1,
	/* 0x0f */	BIG5_LATIN1,
	/* 0x10 */	BIG5_LATIN1,
	/* 0x11 */	BIG5_LATIN1,
	/* 0x12 */	BIG5_LATIN1,
	/* 0x13 */	BIG5_LATIN1,
	/* 0x14 */	BIG5_LATIN1,
	/* 0x15 */	BIG5_LATIN1,
	/* 0x16 */	BIG5_LATIN1,
	/* 0x17 */	BIG5_LATIN1,
	/* 0x18 */	BIG5_LATIN1,
	/* 0x19 */	BIG5_LATIN1,
	/* 0x1a */	BIG5_LATIN1,
	/* 0x1b */	BIG5_LATIN1,
	/* 0x1c */	BIG5_LATIN1,
	/* 0x1d */	BIG5_LATIN1,
	/* 0x1e */	BIG5_LATIN1,
	/* 0x1f */	BIG5_LATIN1,
	/* 0x20 */	BIG5_LATIN1,
	/* 0x21 */	BIG5_LATIN1,
	/* 0x22 */	BIG5_LATIN1,
	/* 0x23 */	BIG5_LATIN1,
	/* 0x24 */	BIG5_LATIN1,
	/* 0x25 */	BIG5_LATIN1,
	/* 0x26 */	BIG5_LATIN1,
	/* 0x27 */	BIG5_LATIN1,
	/* 0x28 */	BIG5_LATIN1,
	/* 0x29 */	BIG5_LATIN1,
	/* 0x2a */	BIG5_LATIN1,
	/* 0x2b */	BIG5_LATIN1,
	/* 0x2c */	BIG5_LATIN1,
	/* 0x2d */	BIG5_LATIN1,
	/* 0x2e */	BIG5_LATIN1,
	/* 0x2f */	BIG5_LATIN1,
	/* 0x30 */	BIG5_LATIN1,
	/* 0x31 */	BIG5_LATIN1,
	/* 0x32 */	BIG5_LATIN1,
	/* 0x33 */	BIG5_LATIN1,
	/* 0x34 */	BIG5_LATIN1,
	/* 0x35 */	BIG5_LATIN1,
	/* 0x36 */	BIG5_LATIN1,
	/* 0x37 */	BIG5_LATIN1,
	/* 0x38 */	BIG5_LATIN1,
	/* 0x39 */	BIG5_LATIN1,
	/* 0x3a */	BIG5_LATIN1,
	/* 0x3b */	BIG5_LATIN1,
	/* 0x3c */	BIG5_LATIN1,
	/* 0x3d */	BIG5_LATIN1,
	/* 0x3e */	BIG5_LATIN1,
	/* 0x3f */	BIG5_LATIN1,
	/* 0x40 */	BIG5_LATIN1,
	/* 0x41 */	BIG5_LATIN1,
	/* 0x42 */	BIG5_LATIN1,
	/* 0x43 */	BIG5_LATIN1,
	/* 0x44 */	BIG5_LATIN1,
	/* 0x45 */	BIG5_LATIN1,
	/* 0x46 */	BIG5_LATIN1,
	/* 0x47 */	BIG5_LATIN1,
	/* 0x48 */	BIG5_LATIN1,
	/* 0x49 */	BIG5_LATIN1,
	/* 0x4a */	BIG5_LATIN1,
	/* 0x4b */	BIG5_LATIN1,
	/* 0x4c */	BIG5_LATIN1,
	/* 0x4d */	BIG5_LATIN1,
	/* 0x4e */	BIG5_LATIN1,
	/* 0x4f */	BIG5_LATIN1,
	/* 0x50 */	BIG5_LATIN1,
	/* 0x51 */	BIG5_LATIN1,
	/* 0x52 */	BIG5_LATIN1,
	/* 0x53 */	BIG5_LATIN1,
	/* 0x54 */	BIG5_LATIN1,
	/* 0x55 */	BIG5_LATIN1,
	/* 0x56 */	BIG5_LATIN1,
	/* 0x57 */	BIG5_LATIN1,
	/* 0x58 */	BIG5_LATIN1,
	/* 0x59 */	BIG5_LATIN1,
	/* 0x5a */	BIG5_LATIN1,
	/* 0x5b */	BIG5_LATIN1,
	/* 0x5c */	BIG5_LATIN1,
	/* 0x5d */	BIG5_LATIN1,
	/* 0x5e */	BIG5_LATIN1,
	/* 0x5f */	BIG5_LATIN1,
	/* 0x60 */	BIG5_LATIN1,
	/* 0x61 */	BIG5_LATIN1,
	/* 0x62 */	BIG5_LATIN1,
	/* 0x63 */	BIG5_LATIN1,
	/* 0x64 */	BIG5_LATIN1,
	/* 0x65 */	BIG5_LATIN1,
	/* 0x66 */	BIG5_LATIN1,
	/* 0x67 */	BIG5_LATIN1,
	/* 0x68 */	BIG5_LATIN1,
	/* 0x69 */	BIG5_LATIN1,
	/* 0x6a */	BIG5_LATIN1,
	/* 0x6b */	BIG5_LATIN1,
	/* 0x6c */	BIG5_LATIN1,
	/* 0x6d */	BIG5_LATIN1,
	/* 0x6e */	BIG5_LATIN1,
	/* 0x6f */	BIG5_LATIN1,
	/* 0x70 */	BIG5_LATIN1,
	/* 0x71 */	BIG5_LATIN1,
	/* 0x72 */	BIG5_LATIN1,
	/* 0x73 */	BIG5_LATIN1,
	/* 0x74 */	BIG5_LATIN1,
	/* 0x75 */	BIG5_LATIN1,
	/* 0x76 */	BIG5_LATIN1,
	/* 0x77 */	BIG5_LATIN1,
	/* 0x78 */	BIG5_LATIN1,
	/* 0x79 */	BIG5_LATIN1,
	/* 0x7a */	BIG5_LATIN1,
	/* 0x7b */	BIG5_LATIN1,
	/* 0x7c */	BIG5_LATIN1,
	/* 0x7d */	BIG5_LATIN1,
	/* 0x7e */	BIG5_LATIN1,
	/* 0x7f */	BIG5_LATIN1,
	/* 0x80 */	BIG5_LATIN1,
	/* 0x81 */	BIG5_LATIN1,
	/* 0x82 */	BIG5_LATIN1,
	/* 0x83 */	BIG5_LATIN1,
	/* 0x84 */	BIG5_LATIN1,
	/* 0x85 */	BIG5_LATIN1,
	/* 0x86 */	BIG5_LATIN1,
	/* 0x87 */	BIG5_LATIN1,
	/* 0x88 */	BIG5_LATIN1,
	/* 0x89 */	BIG5_LATIN1,
	/* 0x8a */	BIG5_LATIN1,
	/* 0x8b */	BIG5_LATIN1,
	/* 0x8c */	BIG5_LATIN1,
	/* 0x8d */	BIG5_LATIN1,
	/* 0x8e */	BIG5_LATIN1,
	/* 0x8f */	BIG5_LATIN1,
	/* 0x90 */	BIG5_LATIN1,
	/* 0x91 */	BIG5_LATIN1,
	/* 0x92 */	BIG5_LATIN1,
	/* 0x93 */	BIG5_LATIN1,
	/* 0x94 */	BIG5_LATIN1,
	/* 0x95 */	BIG5_LATIN1,
	/* 0x96 */	BIG5_LATIN1,
	/* 0x97 */	BIG5_LATIN1,
	/* 0x98 */	BIG5_LATIN1,
	/* 0x99 */	BIG5_LATIN1,
	/* 0x9a */	BIG5_LATIN1,
	/* 0x9b */	BIG5_LATIN1,
	/* 0x9c */	BIG5_LATIN1,
	/* 0x9d */	BIG5_LATIN1,
	/* 0x9e */	BIG5_LATIN1,
	/* 0x9f */	BIG5_LATIN1,
	/* 0xa0 */	BIG5_LATIN1,
	/* 0xa1 */	BIG5_BIG5,
	/* 0xa2 */	BIG5_BIG5,
	/* 0xa3 */	BIG5_BIG5,
	/* 0xa4 */	BIG5_BIG5,
	/* 0xa5 */	BIG5_BIG5,
	/* 0xa6 */	BIG5_BIG5,
	/* 0xa7 */	BIG5_BIG5,
	/* 0xa8 */	BIG5_BIG5,
	/* 0xa9 */	BIG5_BIG5,
	/* 0xaa */	BIG5_BIG5,
	/* 0xab */	BIG5_BIG5,
	/* 0xac */	BIG5_BIG5,
	/* 0xad */	BIG5_BIG5,
	/* 0xae */	BIG5_BIG5,
	/* 0xaf */	BIG5_BIG5,
	/* 0xb0 */	BIG5_BIG5,
	/* 0xb1 */	BIG5_BIG5,
	/* 0xb2 */	BIG5_BIG5,
	/* 0xb3 */	BIG5_BIG5,
	/* 0xb4 */	BIG5_BIG5,
	/* 0xb5 */	BIG5_BIG5,
	/* 0xb6 */	BIG5_BIG5,
	/* 0xb7 */	BIG5_BIG5,
	/* 0xb8 */	BIG5_BIG5,
	/* 0xb9 */	BIG5_BIG5,
	/* 0xba */	BIG5_BIG5,
	/* 0xbb */	BIG5_BIG5,
	/* 0xbc */	BIG5_BIG5,
	/* 0xbd */	BIG5_BIG5,
	/* 0xbe */	BIG5_BIG5,
	/* 0xbf */	BIG5_BIG5,
	/* 0xc0 */	BIG5_BIG5,
	/* 0xc1 */	BIG5_BIG5,
	/* 0xc2 */	BIG5_BIG5,
	/* 0xc3 */	BIG5_BIG5,
	/* 0xc4 */	BIG5_BIG5,
	/* 0xc5 */	BIG5_BIG5,
	/* 0xc6 */	BIG5_BIG5,
	/* 0xc7 */	BIG5_BIG5,
	/* 0xc8 */	BIG5_BIG5,
	/* 0xc9 */	BIG5_BIG5,
	/* 0xca */	BIG5_BIG5,
	/* 0xcb */	BIG5_BIG5,
	/* 0xcc */	BIG5_BIG5,
	/* 0xcd */	BIG5_BIG5,
	/* 0xce */	BIG5_BIG5,
	/* 0xcf */	BIG5_BIG5,
	/* 0xd0 */	BIG5_BIG5,
	/* 0xd1 */	BIG5_BIG5,
	/* 0xd2 */	BIG5_BIG5,
	/* 0xd3 */	BIG5_BIG5,
	/* 0xd4 */	BIG5_BIG5,
	/* 0xd5 */	BIG5_BIG5,
	/* 0xd6 */	BIG5_BIG5,
	/* 0xd7 */	BIG5_BIG5,
	/* 0xd8 */	BIG5_BIG5,
	/* 0xd9 */	BIG5_BIG5,
	/* 0xda */	BIG5_BIG5,
	/* 0xdb */	BIG5_BIG5,
	/* 0xdc */	BIG5_BIG5,
	/* 0xdd */	BIG5_BIG5,
	/* 0xde */	BIG5_BIG5,
	/* 0xdf */	BIG5_BIG5,
	/* 0xe0 */	BIG5_BIG5,
	/* 0xe1 */	BIG5_BIG5,
	/* 0xe2 */	BIG5_BIG5,
	/* 0xe3 */	BIG5_BIG5,
	/* 0xe4 */	BIG5_BIG5,
	/* 0xe5 */	BIG5_BIG5,
	/* 0xe6 */	BIG5_BIG5,
	/* 0xe7 */	BIG5_BIG5,
	/* 0xe8 */	BIG5_BIG5,
	/* 0xe9 */	BIG5_BIG5,
	/* 0xea */	BIG5_BIG5,
	/* 0xeb */	BIG5_BIG5,
	/* 0xec */	BIG5_BIG5,
	/* 0xed */	BIG5_BIG5,
	/* 0xee */	BIG5_BIG5,
	/* 0xef */	BIG5_BIG5,
	/* 0xf0 */	BIG5_BIG5,
	/* 0xf1 */	BIG5_BIG5,
	/* 0xf2 */	BIG5_BIG5,
	/* 0xf3 */	BIG5_BIG5,
	/* 0xf4 */	BIG5_BIG5,
	/* 0xf5 */	BIG5_BIG5,
	/* 0xf6 */	BIG5_BIG5,
	/* 0xf7 */	BIG5_BIG5,
	/* 0xf8 */	BIG5_BIG5,
	/* 0xf9 */	BIG5_BIG5,
	/* 0xfa */	BIG5_BIG5,
	/* 0xfb */	BIG5_BIG5,
	/* 0xfc */	BIG5_BIG5,
	/* 0xfd */	BIG5_BIG5,
	/* 0xfe */	BIG5_BIG5,
	/* 0xff */	BIG5_LATIN1,
};


static XP_Bool
fe_AreBIG5FontsAvail(int16 win_csid)
{
	/* this needs to match the code in fe_LoadBIG5Font */
	if (fe_FontCharSets[CS_X_BIG5 & 0xff].name == NULL)
		return FALSE;
	else
		return TRUE;
}

static fe_Font
fe_LoadBIG5Font(MWContext *context, char *familyName, int points,
                int sizeNum, int fontmask, int charset, int pitch,
                int faceNum, Display *dpy)
{
	int16			cs;
	int			len;
	fe_FontGroupFont	*ret;

	len = (2 * sizeof(fe_FontGroupFont));
	ret = calloc(1, len);
	if (!ret)
	{
		return NULL;
	}
	(void) memset(ret, 0, len);

	cs = CS_LATIN1;
	ret[0].xFont = (XFontStruct *)
            fe_LoadFont(context, &cs, familyName, points, sizeNum, fontmask);
	if (cs != CS_LATIN1)
	{
		XP_FREE(ret);
		return NULL;
	}

	/* this needs to match the code in fe_AreBIG5FontsAvail */
	cs = CS_X_BIG5;
	ret[1].xFont = (XFontStruct *)
            fe_LoadFont(context, &cs, familyName, points, sizeNum, fontmask);
	if (cs == CS_X_BIG5)
	{
		ret[1].mask1 = 0xff;
		ret[1].mask2 = 0xff;
	}
	else
	{
		XP_FREE(ret);
		return NULL;
	}

	return (fe_Font) ret;
}


static void
fe_BIG5TextExtents(fe_Font font, char *string, int len, int *direction,
	int *fontAscent, int *fontDescent, XCharStruct *overall)
{
	fe_ProcessEUCString(CS_BIG5, BIG5Table, 
		NULL,
		NULL,
		NULL, None, font, NULL, 0, 0, string, len,
		fontAscent, fontDescent, overall,
		fe_CharSetFuncsArray[FE_FONT_INFO_BIG5].numberOfFonts);
}


static void
fe_DrawBIG5String(Display *dpy, Drawable d, fe_Font font, GC gc, int x,
	int y, char *string, int len)
{
	fe_ProcessEUCString(CS_BIG5, BIG5Table, 
		(fe_XDrawStringFunc) XDrawString,
		(fe_XDrawStringFunc) XDrawString16,
		dpy, d, font, gc, x, y, string, len,
		NULL, NULL, NULL,
		0);
}


static void
fe_DrawBIG5ImageString(Display *dpy, Drawable d, fe_Font font, GC gc, GC gc2,
	int x, int y, char *string, int len)
{
	fe_ProcessEUCString(CS_BIG5, BIG5Table, 
		(fe_XDrawStringFunc) XDrawImageString,
		(fe_XDrawStringFunc) XDrawImageString16,
		dpy, d, font, gc, x, y, string, len,
		NULL, NULL, NULL,
		0);
}

int 
fe_UnicodePointToPixelSize(Display *dpy, int pointSize)
{
	int pixelSize;
	static double pointToPixelFactor;
	static XP_Bool inited = FALSE;
    char name[512], class[512];
    char *type;
    XrmValue value;

	if (!inited) {
		inited = TRUE;
		pointToPixelFactor = ((WidthOfScreen(DefaultScreenOfDisplay(dpy)) /
                (WidthMMOfScreen(DefaultScreenOfDisplay(dpy)) / 25.4)) /
				72.27) / 10;

        /*
         * Get the users resource setting
         */
        (void) PR_snprintf(name, sizeof(name), "%s.unicodeFont.screenDpi", 
                        fe_progclass);
        (void) PR_snprintf(class, sizeof(class),"%s.UnicodeFont.ScreenDpi",
                        fe_progclass);
        if (XrmGetResource(XtDatabase(dpy), name, class, &type, &value)){
			float userScreenDpi;
			if (sscanf((char *)value.addr, "%e", &userScreenDpi)) {
				pointToPixelFactor = (userScreenDpi/72.27) / 10;
			}
        }
	}
	pointSize += 4; /* round up */
	pixelSize = (int)(pointSize * pointToPixelFactor);
	return pixelSize;
}

static XP_Bool
fe_AreUnicodeFontsAvail(int16 win_csid)
{
	/*
	 * since we build unicode fonts from other fonts
	 * there is not a good way to tell what will be
	 * avail so just assume we can do it
	 */
	return TRUE;
}

/*
 * fe_LoadUnicodeFont - load (create) a Unicode Pseudo Font
 *
 * see fe_loadUnicodePseudoFonts() for information on Unicode Pseudo Fonts
 *
 */
fe_Font
fe_LoadUnicodeFont(void *not_used, char *familyName,
                   int points, int sizeNum,
         int fontmask, int charset, int pitch, int faceNum,
         Display *dpy)
{
    fe_UnicodePseudoFont *ufont;
    fe_FontFamily *unicodeFamily = NULL;
    fe_FontSize *unicodeSize;
	int pixelSize;

	/*
	 * If the familyName is specified then find it
	 * else use the selected family
	 */
	if (familyName) {
        unicodeFamily = fe_FontFindFamily(CS_UTF8, pitch, familyName);
	}
	/*
	 * If the familyName is not specified or that family not found
	 * then use the selected family
	 */
	if (!unicodeFamily) {
        unicodeFamily = fe_GetFontFamily(CS_UTF8, pitch);
		familyName = unicodeFamily->family;
	}
    unicodeSize = unicodeFamily->htmlSizes[sizeNum-1];
	pixelSize = fe_UnicodePointToPixelSize(dpy, unicodeSize->size);
    ufont = fe_LoadUnicodeFontByPixelSize(not_used, familyName, 
    				pixelSize, fontmask, charset, pitch, faceNum, dpy);

    return ufont;
}

/*
 * fe_LoadUnicodeFontByPixelSize - load (create) a Unicode Pseudo Font
 *
 * see fe_loadUnicodePseudoFonts() for information on Unicode Pseudo Fonts
 *
 */
fe_Font
fe_LoadUnicodeFontByPixelSize(void *not_used, char *familyName, int pixelSize,
         int fontmask, int charset, int pitch, int faceNum,
         Display *dpy)
{
    fe_UnicodePseudoFont *ufont;

    ufont = (fe_UnicodePseudoFont*)calloc(1, sizeof(fe_UnicodePseudoFont));
    if (!ufont) {
        return NULL;
    }
    ufont->tag = UNICODE_PSEUDO_FONT_TAG;
    ufont->family = strdup(familyName);
    ufont->dpy = dpy;
    ufont->pixelSize = pixelSize;
    ufont->fontmask = fontmask;
    ufont->pitch = pitch;
    ufont->faceNum = faceNum;
    ufont->ascent = 0;
    ufont->descent = 0;

    return ufont;
}

void
fe_freeUnicodePseudoFont(fe_Font font)
{
    fe_UnicodePseudoFont *ufont = (fe_UnicodePseudoFont *)font;
	int i_encodings;

    if (ufont->tag != UNICODE_PSEUDO_FONT_TAG) {
#ifdef DEBUG_bstell
        printf("bad ufont %s %d\n", __FILE__, __LINE__);
#endif
        return;
    }

	if (ufont->family)
		free(ufont->family);
	/*
	 * Free any scaled fonts
	 */
    for (i_encodings=0; i_encodings<INTL_CHAR_SET_MAX; i_encodings++) {
        if (ufont->xfont_scaled[i_encodings]) {
            XFreeFont(ufont->dpy, ufont->xFonts[i_encodings]);
            XP_FREE(ufont->xfont_name[i_encodings]);
		}
    }
	if (ufont->xmfontlist)
		XmFontListFree(ufont->xmfontlist);
	if (ufont->xm_fontset)
		XmFontListFree(ufont->xm_fontset);
	if (ufont->xfontset) 
		XFreeFontSet(ufont->dpy, ufont->xfontset);
	free(ufont);
}

static char *
fe_UnicodeEquivFontName(fe_UnicodePseudoFont *ufont, uint16 charsetID)
{
    XrmDatabase db;
    char name[512], class[512];
    char *csid_name, *type;
    XrmValue value;

    charsetID &= 0xFF;

    if (ufont->fontFamily[charsetID&0xFF])
        return (ufont->fontFamily[charsetID]);

    db = XtDatabase(ufont->dpy);

    csid_name = INTL_CsidToCharsetNamePt(charsetID);
    (void) PR_snprintf(name, sizeof(name), "%s.unicodeFont.%s.%s", 
                        fe_progclass, ufont->family, csid_name);
    (void) PR_snprintf(class, sizeof(class), "%s.UnicodeFont.%s.%s", 
                        fe_progclass, ufont->family, "Name");
    if (XrmGetResource(db, name, class, &type, &value))
        ufont->fontFamily[charsetID] = strdup(value.addr);
    else
        ufont->fontFamily[charsetID] = strdup(ufont->family);
    return (ufont->fontFamily[charsetID]);
}

/*
 * Rate the  Quality of the font match
 *
 * a return range 0.0 to 1.0
 * 1.0 means perfect match
 * lower numbers mean poorer match
 */
static int
fe_UnicodeFontQ(fe_UnicodePseudoFont *ufont, 
				int32 target_height, int32 actual_height,
				XP_Bool family_match, XP_Bool weight_match, XP_Bool slant_match,
				int target_pitch, XP_Bool pitch_match
				)
{
	float q = 1.0;

	/*
	 * Look at size match
	 */
	if (actual_height <= target_height)
		q *= (float)actual_height / (float)target_height;
	else
		q *= (float)target_height / (float)actual_height;

	/*
	 * Look at family match (correct font family)
	 */
	if (!family_match)
		q *= 0.8;
#ifdef DEBUG_bstell
	else
		q *= 1.0;
#endif

	/*
	 * Look at weight match (regular vs bold)
	 */
	if (!weight_match)
		q *= 0.75;

	/*
	 * Look at slant match (regular vs italic)
	 */
	if (!slant_match)
		q *= 0.5;

	/*
	 * Look at pitch match (monospaced vs proportional)
	 *
	 * Pitch doesn't mean much to most users but:
	 *    many (older) applications assume a monospaced font 
	 *    and won't work correctly with a proportional font 
	 * Hence if the target font is monospaced
	 *    we strongly want the pitch to be right
	 */
	if (!pitch_match) {
		if (target_pitch == 0) /* proportional */
			q *= 0.8;
		else /* MONO */
			q *= 0.0;
	}

	return (int)(q * 100);
}

#define IS_ORIG_PITCH(pitch) ((pitch)==0)
#define IS_ORIG_WEIGHT_FACE(face)   (((face)&1)==0)
#define IS_ORIG_SLANT_FACE(face)  (((face)&2)==0)

/*
 * fe_UnicodeFindFontInRange
 *
 * find the font for this encoding with the UnicodePseudoFont's 
 *   specified font name / slant / weight
 *   no scaling
 *   size range = minPixelSize to maxPixelSize
 *   either pitch
 */
static XFontStruct *
fe_UnicodeFindFontInRange(fe_UnicodePseudoFont *ufont, uint16 charsetID,
                    int32 maxPixelSize, int32 minPixelSize, XP_Bool any_weight,
					char **name)
{
    int pitch, i_pitch, i_pnt;
    fe_FontSize *size;
    fe_FontFamily *fontFamily;
    char *family;
	int bestFoundQ = 0;
	int thisFontQ;
	int bestFoundFaceNum;
    fe_FontSize *bestFoundSize = NULL;

    /*
     * Get the equivalent font name for this encoding
     */
    family = fe_UnicodeEquivFontName(ufont, charsetID);

    /*
     * Find the font
     */
    for (i_pitch=0; i_pitch<2; i_pitch++) {
        if (IS_ORIG_PITCH(i_pitch))
            pitch = ufont->pitch;
        else
            pitch = FE_OPPOSITE_PITCH(ufont->pitch);
        fontFamily = fe_FontFindFamily(charsetID, pitch, family);
        if (fontFamily == NULL)
            continue;
        /* scan pixel sizes for largest size that is not too big */
        for (i_pnt=fontFamily->numberOfPixelSizes-1; i_pnt>=0; i_pnt--) {
            size = &fontFamily->pixelSizes[i_pnt];
            /* stop when sizes are too small */
            if (size->size<minPixelSize)
                break;
            if (size->size>=maxPixelSize) 
                ufont->larger_fonts_avail[charsetID] = TRUE;
            if ((size->size>=minPixelSize) && (size->size<=maxPixelSize)) {
				int i_face, faceNum;
				for (i_face=0; i_face<2; i_face++) {
					if (IS_ORIG_WEIGHT_FACE(i_face))
						faceNum = ufont->faceNum;
					else {
						if (!any_weight)
							continue;
						faceNum = FE_OPPOSITE_WEIGHT_FACE(ufont->faceNum);
					}
                	/* load font if necessary */
                	if (!size->faces[faceNum]) 
				   		continue;
					thisFontQ = fe_UnicodeFontQ(ufont, 
										maxPixelSize, size->size,
										/* family_match */
										TRUE,
										IS_ORIG_WEIGHT_FACE(i_face),
										TRUE,
										ufont->pitch, IS_ORIG_PITCH(i_pitch));
					if (thisFontQ > bestFoundQ) {
						bestFoundQ = thisFontQ;
						bestFoundSize = size;
						bestFoundFaceNum = faceNum;
					}
				}
            }
        }
    }
	if (bestFoundSize != NULL) {
        if (!bestFoundSize->faces[bestFoundFaceNum]->loaded) {
            bestFoundSize->faces[bestFoundFaceNum]->loaded = TRUE;
            bestFoundSize->faces[bestFoundFaceNum]->font = 
				fe_LoadXFont(ufont->dpy,
                      bestFoundSize->faces[bestFoundFaceNum]->longXLFDFontName);
        }
        if (bestFoundSize->faces[bestFoundFaceNum]->font) {
			*name = bestFoundSize->faces[bestFoundFaceNum]->longXLFDFontName;
            return bestFoundSize->faces[bestFoundFaceNum]->font;
		}
	}
	*name = "";
    return NULL;
}


/*
 * fe_UnicodeFindFontSloppy
 *
 * find the font for this encoding with the UnicodePseudoFont's 
 *   ANY FAMILY
 */
static XFontStruct *
fe_UnicodeFindFontSloppy(fe_UnicodePseudoFont *ufont, uint16 charsetID,
                          int32 maxPixelSize, int32 minPixelSize,
                          XP_Bool allow_scaling, 
			  XP_Bool any_weight, XP_Bool any_slant,
			  char **name)
{
    int i_family, i_pitch, i_pnt, i_face;
    fe_FontPitch *fontPitch;
    fe_FontSize *size = NULL;
    fe_FontFamily *fontFamily = NULL;
    fe_FontCharSet    *fontCharset;
    XFontStruct *scaled_font;
    fe_FontSize *bestFoundSize = NULL;
    fe_FontFamily *bestFontFamily = NULL;
	int bestFoundQ = 0;
	int thisFontQ;
	int bestFoundFaceNum = 0;
    fe_FontFamily *targetFontFamily;
    char *targetFamily;

	if (minPixelSize < 1)
		minPixelSize = 1;

    fontCharset = &fe_FontCharSets[charsetID];
    /*
     * Get the equivalent font name for this encoding
     */
    targetFamily = fe_UnicodeEquivFontName(ufont, charsetID);

    /*
     * Find the font
     */
    for (i_pitch=0; i_pitch<2; i_pitch++) {
        if IS_ORIG_PITCH(i_pitch) {
            fontPitch = &fontCharset->pitches[ufont->pitch];
    		targetFontFamily = fe_FontFindFamily(charsetID, 
										ufont->pitch, targetFamily);
		}
        else {
            fontPitch = &fontCharset->pitches[FE_OPPOSITE_PITCH(ufont->pitch)];
    		targetFontFamily = fe_FontFindFamily(charsetID, 
								FE_OPPOSITE_PITCH(ufont->pitch), targetFamily);
		}
        for (i_family = 0; i_family < fontPitch->numberOfFamilies; i_family++) {
            fontFamily = &fontPitch->families[i_family];
            for (i_pnt=fontFamily->numberOfPixelSizes-1; i_pnt>=0; i_pnt--) {
                size = &fontFamily->pixelSizes[i_pnt];
				if ((charsetID==CS_LATIN1) 
					&& (size->size) && (size->size < smallestFontFoundSize))
					smallestFontFoundSize = size->size;
                /* skip sizes that are too small */
                if ((size->size) && (size->size<minPixelSize))
                    continue;
                if (size->size>=maxPixelSize) 
                    ufont->larger_fonts_avail[charsetID] = TRUE;
                if (((size->size>=minPixelSize) && (size->size<=maxPixelSize))
                    || (allow_scaling && (size->size == 0))) {
					int32 target_size;
                    for (i_face=0; i_face<4; i_face++) {
                        int faceNum = ufont->faceNum;
                        if (!IS_ORIG_WEIGHT_FACE(i_face)) {
                            if (!any_weight)
                                continue;
                            faceNum = FE_OPPOSITE_WEIGHT_FACE(faceNum);
                        }
                        if (!IS_ORIG_SLANT_FACE(i_face)) {
                            if (!any_slant)
                                continue;
                            faceNum = FE_OPPOSITE_SLANT_FACE(faceNum);
                        }
                        if (!size->faces[faceNum])
							continue;
						if (size->size) /* not scaled */
							target_size = size->size;
						else
							target_size = maxPixelSize;
						thisFontQ = fe_UnicodeFontQ(ufont, 
										maxPixelSize, target_size,
										/* family_match */
										(targetFontFamily==fontFamily),
										IS_ORIG_WEIGHT_FACE(i_face),
										IS_ORIG_SLANT_FACE(i_face),
										ufont->pitch, IS_ORIG_PITCH(i_pitch));
						if (thisFontQ > bestFoundQ) {
							bestFoundQ = thisFontQ;
							bestFoundSize = size;
							bestFoundFaceNum = faceNum;
							bestFontFamily = fontFamily;
						}
                    }
                }
            }
        }
    }
	if (bestFoundSize != NULL) {
        /* load font if necessary */
        if (bestFoundSize->size) {
            if (!bestFoundSize->faces[bestFoundFaceNum]->loaded) {
                bestFoundSize->faces[bestFoundFaceNum]->loaded = TRUE;
                bestFoundSize->faces[bestFoundFaceNum]->font = 
							fe_LoadXFont(ufont->dpy, 
                    bestFoundSize->faces[bestFoundFaceNum]->longXLFDFontName);
            }
            if (bestFoundSize->faces[bestFoundFaceNum]->font)
				*name = bestFoundSize->faces[bestFoundFaceNum]->longXLFDFontName;
                return bestFoundSize->faces[bestFoundFaceNum]->font;
            }
        else if (allow_scaling) {
            *name = PR_smprintf(bestFontFamily->pixelSizes->faces[
                      bestFoundFaceNum]->longXLFDFontName, maxPixelSize);
            scaled_font = fe_LoadXFont(ufont->dpy, *name);
            if (scaled_font)
                return scaled_font;
			else
				XP_FREE(*name);
        }
	}

	*name = "";
    return NULL;
}

/*
 * fe_UnicodeFindFontScaled
 *
 * find the font for this encoding with the UnicodePseudoFont's 
 *   specified font name / slant / weight
 *   scaled font
 *   either pitch
 */
static XFontStruct *
fe_UnicodeFindFontScaled(fe_UnicodePseudoFont *ufont, uint16 charsetID,
                        int32 maxPixelSize, char **name)
{
    int pitch, i_pitch, i_face;
    char *family;
    fe_FontFamily *fontFamily;
    XFontStruct *scaled_font;
    fe_FontFamily *bestFontFamily = NULL;
	int bestFoundQ = 0;
	int thisFontQ;
	int bestFoundFaceNum = 0;

    /*
     * Get the equivalent font name for this encoding
     */
    family = fe_UnicodeEquivFontName(ufont, charsetID);
    for (i_pitch=0; i_pitch<2; i_pitch++) {
        if (IS_ORIG_PITCH(i_pitch))
            pitch = ufont->pitch;
        else
            pitch = FE_OPPOSITE_PITCH(ufont->pitch);
        fontFamily = fe_FontFindFamily(charsetID, pitch, family);
        if (fontFamily == NULL)
            continue;
        if (fontFamily->pixelSizes->size == 0)  {
			int faceNum;
			for (i_face=0; i_face<2; i_face++) {
				if (IS_ORIG_WEIGHT_FACE(i_face))
					faceNum = ufont->faceNum;
				else
					faceNum = FE_OPPOSITE_WEIGHT_FACE(ufont->faceNum);
				if (!fontFamily->pixelSizes->faces[faceNum])
					continue;
				thisFontQ = fe_UnicodeFontQ(ufont, 
										maxPixelSize, maxPixelSize,
										TRUE, /* family_match */
										IS_ORIG_WEIGHT_FACE(i_face),
										TRUE, /* slant_match */
										ufont->pitch, IS_ORIG_PITCH(i_pitch));
				if (thisFontQ > bestFoundQ) {
					bestFoundQ = thisFontQ;
					bestFoundFaceNum = faceNum;
					bestFontFamily = fontFamily;
				}
			}
        }
    }
	if (bestFontFamily != NULL) {
        *name = PR_smprintf(bestFontFamily->pixelSizes->faces[
                              bestFoundFaceNum]->longXLFDFontName,
                              maxPixelSize);
        scaled_font = fe_LoadXFont(ufont->dpy, *name);
        if (scaled_font)
            return scaled_font;
		else
			XP_FREE(*name);
	}
	*name = "";
    return NULL;
}

static XP_Bool
fe_UnicodeCheckAllowScaling(fe_UnicodePseudoFont *ufont)
{
    XP_Bool allow_scaling;
    fe_FontFamily *unicodeFamily;
    char *server_vendor;
    static XP_Bool inited_server_info = FALSE;
    static XP_Bool is_hp_server = FALSE;
    static XP_Bool allow_hp_scaling = FALSE;
    char name[512], class[512];
    char *type;
    XrmValue value;

    /*
     * When displaying on a HP system do not scale.
     *
     * HP scaling causes the X server to "freeze"
     * which makes the system unusable.
     * Mouse delays of 20+ seconds make it very 
     * difficult to even exit the application.
     */
    if (!inited_server_info) {
        inited_server_info = TRUE;
        server_vendor = ServerVendor(ufont->dpy);
        if (strstr(server_vendor, "Hewlett-Packard")) {
            is_hp_server = TRUE;
        }
        /*
         * Get the users resource setting
         */
        (void) PR_snprintf(name, sizeof(name), "%s.unicodeFont.allowHPScaling", 
                        fe_progclass);
        (void) PR_snprintf(class, sizeof(class),"%s.UnicodeFont.AllowHPScaling",
                        fe_progclass);
        if (XrmGetResource(XtDatabase(ufont->dpy), name, class, &type, &value)){
            char *string = (char *)value.addr;
            if ((string[0]=='y') || (string[0]=='Y')
                || (string[0]=='t') || (string[0]=='T')
                || (string[0]=='1'))
                allow_hp_scaling = TRUE;
            else
                allow_hp_scaling = FALSE;
        }
    }
    if ((is_hp_server) && (!allow_hp_scaling))
        return FALSE;

    unicodeFamily = fe_GetFontFamily(CS_UTF8, ufont->pitch);
    allow_scaling = unicodeFamily->allowScaling;
    return allow_scaling;
}

static XFontStruct *
fe_UnicodeFindFont(fe_UnicodePseudoFont *ufont, uint16 encoding, 
                                                       XP_Bool *is_scaled,
													   char **name)
{
    uint16 charsetID;
    fe_FontCharSet    *fontCharset;
    int loop_timeout;
    int32 maxPixelSize;
    XFontStruct *ret_font, *scaled_font = NULL;
    XP_Bool allow_scaling;

    /* printf("encoding = %d (%d), pixelSize = %d, pitch = %d, faceNum = %d\n",
                encoding, encoding&0xFF, ufont->pixelSize, 
                ufont->pitch, ufont->faceNum); */
    /*
     * Init return value
     */
    *is_scaled = FALSE;

    /*
     * Check the encoding
     */
    charsetID = encoding & 0xFF;
    fontCharset = &fe_FontCharSets[charsetID];
    if (!fontCharset->name) {
        return NULL;
    }

    maxPixelSize = ufont->pixelSize;
    allow_scaling = fe_UnicodeCheckAllowScaling(ufont);

    /*
     * Get the best font for this size and encoding
     *
     * We loop looking for a font that will fit
     *  The reason for the loop is that after we load the font it is
     *  possible that the ascent or descent is too large.
     *  If this happens we reduce the size by that amount and try again.
     *
     * The loop count is large because:
     *  If we can only find a bestfit that is small
     *  and this bestfit is still larger than the desired size
     *  then we end up running thru the loop while maxPixelSize is reduced
     *  to the pixel that bestfit is small enough.
     *  This tends to not be efficient since the bestfit font
     *  is only loaded once.
     */
#define LOOP_TIMEOUT 100
    scaled_font = NULL;
    for (loop_timeout=0; loop_timeout<LOOP_TIMEOUT; loop_timeout++) {
        if (scaled_font != NULL) {
            XFreeFont(ufont->dpy, scaled_font);
			XP_FREE(*name);
			*name = "";
            scaled_font = NULL;
        }

        /*
         * Exact Fit (non scaled)
         * try to find:
         * the family with this size range: 100% - 95%
         * no scaling
         * correct pitch/weight/slant
         * then try the other pitch
         */
        ret_font = fe_UnicodeFindFontInRange(ufont, charsetID, 
                                        maxPixelSize, 0.95*maxPixelSize, FALSE,
										name);
        if (ret_font) {
            loop_timeout += LOOP_TIMEOUT/10 - 1;
            goto UnicodeFindFont_foundOne;
        }

        /*
         * Exact Fit (scaled)
         * try to find:
         * the family with the SCALED SIZE
         * (X bitmap scaled fonts typically look bad)
         * correct slant
         * ANY PITCH / ANY WEIGHT
         */
        if (allow_scaling) {
            scaled_font = fe_UnicodeFindFontScaled(ufont, charsetID, 
                                                maxPixelSize, name);
            if (scaled_font) {
                loop_timeout += LOOP_TIMEOUT/5 - 1;
                ret_font = scaled_font;
                goto UnicodeFindFont_foundOne;
            }
        }
    
        /*
         * Close Fit (non scaled)
         * try to find:
         * the family with this size range: 100% - 75%
         * no scaling
         * correct slant
         * ANY_WEIGHT / ANY PITCH
         */
        ret_font = fe_UnicodeFindFontInRange(ufont, charsetID, 
                                        maxPixelSize, 0.75*maxPixelSize, TRUE,
										name);
        if (ret_font) {
            loop_timeout += LOOP_TIMEOUT/10 - 1;
            goto UnicodeFindFont_foundOne;
        }

        /*
         * Okay Fit (any family)
         * try to find:
         * size range: 100% - 75%
         * correct slant
         * ANY FAMILY / ANY WEIGHT / ANY PITCH
         */
        ret_font = fe_UnicodeFindFontSloppy(ufont, charsetID, 
                         maxPixelSize, 0.75*maxPixelSize, 
                         allow_scaling, TRUE, FALSE,
						 name);
        if (ret_font) {
            loop_timeout += LOOP_TIMEOUT/10 - 1;
            goto UnicodeFindFont_foundOne;
        }
    
        /*
         * Loose Fit (non scaled)
         * try to find:
         * size range: 100% - 50%
         * ANY FAMILY / ANY WEIGHT / ANY PITCH / ANY SLANT
         */
        ret_font = fe_UnicodeFindFontSloppy(ufont, charsetID, 
                                maxPixelSize, 0.50*maxPixelSize, 
                                allow_scaling, TRUE, TRUE,
								name);
        if (ret_font) {
            loop_timeout += LOOP_TIMEOUT/10 - 1;
            goto UnicodeFindFont_foundOne;
        }
    
        /*
         * Any fit
         * try to find:
         * ANY SIZE / ANY FAMILY / ANY WEIGHT / ANY PITCH / ANY SLANT
         */
        ret_font = fe_UnicodeFindFontSloppy(ufont, charsetID, 
                                maxPixelSize, 0.0*maxPixelSize,
                                allow_scaling, TRUE, TRUE,
								name);
        if (ret_font) {
            loop_timeout += LOOP_TIMEOUT/10 - 1;
            goto UnicodeFindFont_foundOne;
        }

		/* 
		 * HUM ..., 
		 * must not be a font small enough
		 * in desperation we give them a real small font.
		 */
        ret_font = fe_UnicodeFindFontSloppy(ufont, CS_LATIN1, 
								smallestFontFoundSize, 0.0*maxPixelSize,
                                allow_scaling, TRUE, TRUE,
								name);
        if (ret_font) {
            loop_timeout += LOOP_TIMEOUT/10 - 1;
            goto UnicodeFindFont_foundOne;
        }
    
        /*
         * Gave up trying to find a font that will fit
         */
        goto UnicodeFindFont_failed;


UnicodeFindFont_foundOne:
        /*
         * if encoding is CS_LATIN1 use the font
         * else check that the font will fit
         */
        if ((encoding==CS_LATIN1) && (ufont->ascent==0) && (ufont->descent==0)){
            if ((ufont->pixelSize/10) > (ret_font->ascent+ret_font->descent))
                ret_font->ascent = (ufont->pixelSize/10) - ret_font->descent;
            ufont->ascent  = ret_font->ascent;
            ufont->descent = ret_font->descent;
        }
        else {
            if (ret_font->ascent > ufont->ascent) {
                /* printf("ascent too big (%d), reduce by %d\n",
                        ufont->pixelSize, ret_font->ascent - ufont->ascent); */
                maxPixelSize -= (ret_font->ascent - ufont->ascent) * 10;
                if (maxPixelSize > 20) /* continue unless something wrong */
                    continue;
            }
            if (ret_font->descent > ufont->descent) {
                /* printf("descent too big (%d), reduce by %d\n",
                                    ret_font->descent - ufont->descent); */
                maxPixelSize -= (ret_font->descent - ufont->descent) * 10;
                if (maxPixelSize > 20) /* continue unless something wrong */
                    continue;
            }
        }
        if (scaled_font != NULL)
            *is_scaled = TRUE;
        return ret_font;
    }
UnicodeFindFont_failed:
    /*
     * did not find any fit
     */
    if (encoding == CS_LATIN1) {
        /* if we got here something is wrong with fonts */
        XFontStruct *xfont = (XFontStruct *)fe_FixedFont;
        ufont->ascent = xfont->ascent;
        ufont->descent = xfont->descent;
        return fe_FixedFont;
    }
    return NULL;
}

static XFontStruct *
fe_UnicodeInitXfont(fe_UnicodePseudoFont *ufont, uint16 encoding)
{
    XmFontListEntry flentry;
    uint16 charset = encoding&0xFF;

    if (ufont->tag != UNICODE_PSEUDO_FONT_TAG) {
#ifdef DEBUG_bstell
        printf("bad ufont %s %d\n", __FILE__, __LINE__);
#endif
                return fe_FixedFont; /* fallback */
    }

   /*
    * Time to load the font
    */
    ufont->xfont_inited[charset] = TRUE;
    ufont->xFonts[charset] = fe_UnicodeFindFont(ufont, encoding, 
									&ufont->xfont_scaled[charset],
									&ufont->xfont_name[charset]);
    /*
     * Add this font to the XmFontList
     */
    if (ufont->xFonts[charset]) {
        flentry = XmFontListEntryCreate(INTL_CsidToCharsetNamePt(encoding), 
                                       XmFONT_IS_FONT, ufont->xFonts[charset]);
        ufont->xmfontlist = XmFontListAppendEntry(ufont->xmfontlist, flentry);
        XmFontListEntryFree(&flentry);
    }
    return ufont->xFonts[charset];
}

XFontStruct *
fe_UnicodeGetXfont(fe_UnicodePseudoFont *ufont, uint16 encoding)
{
    uint16 charset = encoding&0xFF;

    if (ufont->tag != UNICODE_PSEUDO_FONT_TAG) {
#ifdef DEBUG_bstell
        printf("bad ufont %s %d\n", __FILE__, __LINE__);
#endif
                return fe_FixedFont; /* fallback */
    }

    /*
     * If the xfont is already there return it
     */
    if (ufont->xfont_inited[charset])
        return ufont->xFonts[charset];

    /*
     * Make sure CS_LATIN1 loaded so the
     * ascender/descender info is set
     */
    if (encoding != CS_LATIN1) {
        if (ufont->xfont_inited[CS_LATIN1] == FALSE)
                        fe_UnicodeInitXfont(ufont, CS_LATIN1);
    }

    /*
     * Load the font
     */
    fe_UnicodeInitXfont(ufont, encoding);
    return ufont->xFonts[charset];
}

XmFontList 
XUPF_GetXmFontList(fe_Font font)
{
    fe_UnicodePseudoFont *ufont = (fe_UnicodePseudoFont *)font;

    if ((ufont==NULL) || (ufont->tag != UNICODE_PSEUDO_FONT_TAG)) {
        return NULL;
    }
    return ufont->xmfontlist;
}

/*
 * create a XmFontList with one XFontSet for XmText widgets
 * since they can only handle a single locale.
 */
XmFontList 
XUPF_GetXmFontSet(fe_Font font)
{
    fe_UnicodePseudoFont *ufont = (fe_UnicodePseudoFont *)font;
	int16 *csidList;
	int i;
	char *fontNameList = NULL;
	char **missingCharSets;
	int missingCharSetCount;
	char *missingCharString;
	XmFontListEntry flentry;


	/* check for bad inputs */
    if ((ufont==NULL) || (ufont->tag != UNICODE_PSEUDO_FONT_TAG)) {
        return NULL;
    }

	/* if it has already been created just use it */
	if (ufont->xm_fontset)
		return ufont->xm_fontset;

   /*
	* get the list of charset we need
	*/
	csidList = INTL_GetXCharSetIDs(ufont->dpy);
	if (csidList == NULL)
		return NULL;

   /*
	* build the list of fontSet names
	*/
	for( i=0; csidList[i] != 0; i++ ) {
		char *fontName;
		char *newFontNameList;
		(void)fe_UnicodeGetXfont(ufont, csidList[i]);
		fontName = ufont->xfont_name[csidList[i]&0xff];

		if ((fontName==NULL) || (strlen(fontName)==0)) {
			continue;
		}
 
		if (fontNameList == NULL)
			newFontNameList = strdup(fontName);
		else
			newFontNameList = PR_smprintf("%s,\n%s", fontNameList, fontName);

		if (!newFontNameList) {
			continue;
		}
		if (fontNameList != NULL)
			free(fontNameList);
		fontNameList = newFontNameList;
	}
	free(csidList);

   /*
	* create the XFontSet
	*/
	ufont->xfontset = XCreateFontSet(ufont->dpy, fontNameList, &missingCharSets,
                          &missingCharSetCount, &missingCharString);

	flentry = XmFontListEntryCreate(XmFONTLIST_DEFAULT_TAG, XmFONT_IS_FONTSET,
										ufont->xfontset);
	ufont->xm_fontset = XmFontListAppendEntry(NULL, flentry);
	XmFontListEntryFree(&flentry);
	return ufont->xm_fontset;

}

int
XUPF_JavaPointToPixelSize(Display *dpy, int pointSize)
{
	static XP_Bool inited = FALSE;
	static XP_Bool use_point_sizing;
	static double pointToPixelFactor = (72.27/72.27)/10;
    char name[512], class[512];
    char *type;
    XrmValue value;

	if (!inited) {
		inited = TRUE;
        /*
         * Get the users resource setting
         */
        (void) PR_snprintf(name, sizeof(name), "%s.javaFont.usePointSizing", 
                        fe_progclass);
        (void) PR_snprintf(class, sizeof(class),"%s.JavaFont.UsePointSizing",
                        fe_progclass);
        if (XrmGetResource(XtDatabase(dpy), name, class, &type, &value)){
            char *string = (char *)value.addr;
            if ((string[0]=='y') || (string[0]=='Y')
                || (string[0]=='t') || (string[0]=='T')
                || (string[0]=='1'))
                use_point_sizing = TRUE;
            else
                use_point_sizing = FALSE;
        }
        (void) PR_snprintf(name, sizeof(name), "%s.javaFont.screenDpi", 
                        fe_progclass);
        (void) PR_snprintf(class, sizeof(class),"%s.JavaFont.ScreenDpi",
                        fe_progclass);
        if (XrmGetResource(XtDatabase(dpy), name, class, &type, &value)){
			float userScreenDpi;
			if (sscanf((char *)value.addr, "%e", &userScreenDpi)) {
				pointToPixelFactor = (userScreenDpi/72.27)/10;
			}
        }
	}
	if (use_point_sizing)
		return (fe_UnicodePointToPixelSize(dpy, pointSize));
	else {
	    pointSize += 4; /* round up */
		return (int)(pointSize * pointToPixelFactor);
	}
}

void
fe_UnicodeConvertToPlaceHolder(fe_UnicodePseudoFont *ufont, 
                        int16 *encoding, XFontStruct **xfont,
                        unsigned char *segment, int seg_data_len)
{
	/* unsigned char c; */
	int i;

	/*
	 * Handle errors
	 */
#if 0
	if (*encoding == -1) /* not utf8 */
		c = '?';
	else if (!*xfont) /* only bigger fonts available */
		c = '?';
	else /* no font at all for this encoding */
		c = '?';
#endif /* 0 */
	for (i=0; i< seg_data_len; i++) {
		segment[i] = ((unsigned char) '?') /* c */;
	}
	*encoding = CS_LATIN1;
	*xfont = fe_UnicodeGetXfont(ufont, CS_LATIN1);
}

/*
 * fe_UTF8TextExtents - Get the Text Extents for a UTF8 Unicode string
 *
 * Parse the string into locally displayable encoded segments
 * and sum up the extents for the segments
 *
 * If there are errors dummy up some space for those characters
 * so we can display something.
 * hopefully the user will see the garbage and try a different encoding
 */
void
fe_UTF8TextExtents(fe_Font font, char *string, int len, int *direction,
    int *fontAscent, int *fontDescent, XCharStruct *overall)
{
    fe_UnicodePseudoFont *ufont = (fe_UnicodePseudoFont *)font;
    unsigned char segment[X11STRING_LIMIT*6];
    int16 encoding;
    int parsed_cnt = 0;
    int this_parsed_cnt, seg_data_len;
    int ascent, descent, this_direction;
    Dimension width;
    XFontStruct *xfont;
    XCharStruct this_overall;


    if ((ufont==NULL) || (ufont->tag != UNICODE_PSEUDO_FONT_TAG)) {
        /* something is wrong. I will just return a guess */
        overall->ascent = *fontAscent = 14;
        overall->descent = *fontDescent = 3;
        overall->lbearing = 0;
        overall->rbearing = 0;
        overall->width = len*9;
        return;
    }

    /*
     * initialize the values
     */
    *fontAscent = 0;
    *fontDescent = 0;
    overall->lbearing = 0;
    overall->rbearing = 0;
    overall->width = 0;
    overall->ascent = 0;
    overall->descent = 0;

    /*
     * Break up the unicode string into locally sizeable encoded segments
     */
    ascent = 0;
    descent = 0;
    width = 0;
    while (1) {
        /* get a single encoding string */
        this_parsed_cnt = utf8_to_local_encoding(string+parsed_cnt, 
                                        len-parsed_cnt, 
                                        segment, sizeof(segment),
                                        &seg_data_len, &encoding);
        if (this_parsed_cnt < 1) /* no more input data */
            break;

        /*
         * Get the font for this segment
         */
        if (encoding >= 0)
            xfont = fe_UnicodeGetXfont(ufont, encoding);
		else
			xfont = NULL;

		/*
		 * Handle errors
		 */
		if ((encoding < 0) || (!xfont)) {
            fe_UnicodeConvertToPlaceHolder(ufont, &encoding, &xfont,
                            segment, seg_data_len);
		}

        /*
         * Get the size metrics for this segment
         */
        if (IS_16BIT_ENCODING(encoding)) {
            XTextExtents16(xfont, (XChar2b*)segment, seg_data_len/2,
                &this_direction, &ascent, &descent, &this_overall);
        }
        else {
            XTextExtents(xfont, segment, seg_data_len,
                &this_direction, &ascent, &descent, &this_overall);
        }

        /*
         * sum up the width so far
         * height (ascent/descent) comes from the theoritical size
         */
        parsed_cnt += this_parsed_cnt;
        *direction = this_direction; /* which segment should I use ? */
        if ((this_overall.lbearing+overall->width) < overall->lbearing)
            overall->lbearing = this_overall.lbearing + overall->width;
        overall->width += this_overall.width;
        overall->rbearing += this_overall.rbearing;
    }

    /*
     * Set the height from the theoritical size
     */
    *fontAscent  = ufont->ascent;
    *fontDescent = ufont->descent + 1;
    overall->ascent = ufont->ascent;
    overall->descent = ufont->descent + 1;
}


static void
fe_DisplayUTF8String(XP_Bool imageString, Display *dpy, Drawable d, 
              fe_Font font, GC gc, GC gc2, int x, int y, char *string, int len)
{
    fe_UnicodePseudoFont *ufont = (fe_UnicodePseudoFont *)font;
    unsigned char segment[X11STRING_LIMIT*6];
    int parsed_cnt = 0;
    int16 encoding, charset;
    int this_parsed_cnt, seg_data_len;
    int ascent, descent, this_direction;
    XCharStruct this_overall;
    XFontStruct *xfont;

    if ((ufont==NULL) || (ufont->tag != UNICODE_PSEUDO_FONT_TAG)) {
        return;
    }
    if (!gc) {
        return;
    }
    while (1) {
        /*
         * get a single encoding string
         */
        this_parsed_cnt = utf8_to_local_encoding(string+parsed_cnt, 
                                        len-parsed_cnt, 
                                        segment, sizeof(segment),
                                        &seg_data_len, &encoding);
        if (this_parsed_cnt < 1)
            break;

        parsed_cnt += this_parsed_cnt;

        /*
         * Set the font for this encoding
         */
        if (encoding>0)
            xfont = fe_UnicodeGetXfont(ufont, encoding);
		else
			xfont = NULL;

		/*
		 * Handle errors
		 */
		if ((encoding < 0) || (!xfont)) {
            fe_UnicodeConvertToPlaceHolder(ufont, &encoding, &xfont,
                            segment, seg_data_len);
		}

        if (IS_16BIT_ENCODING(encoding))
            seg_data_len /= 2;
        XSetFont(dpy, gc, xfont->fid);
        charset = encoding &0xFF;
        if (IS_16BIT_ENCODING(encoding)) {
            /* get the real base line / descent */
            XTextExtents16(ufont->xFonts[charset], (XChar2b*)segment, 
                seg_data_len, &this_direction, &ascent, &descent,
                &this_overall);
            if (imageString)
                XFillRectangle(dpy, d, gc2, x, y-ufont->ascent, 
                        this_overall.width, ufont->ascent+ufont->descent);
            XDrawString16(dpy, d, gc, x, y, (XChar2b*)segment,seg_data_len);
        }
        else {
            XTextExtents(ufont->xFonts[charset], segment, seg_data_len,
                        &this_direction, &ascent, &descent, &this_overall);
            if (imageString)
                XFillRectangle(dpy, d, gc2, x, y-ufont->ascent, 
                        this_overall.width, ufont->ascent+ufont->descent);
            XDrawString(dpy, d, gc, x, y, segment, seg_data_len);
        }
        x += this_overall.width;
    }
}


void
fe_DrawUTF8String(Display *dpy, Drawable d, fe_Font font, GC gc, int x,
    int y, char *string, int len)
{
    fe_DisplayUTF8String(FALSE, dpy, d, font, gc, NULL, x, y, string, len);
}

static void
fe_DrawUTF8StringImage(Display *dpy, Drawable d, fe_Font font, GC gc, GC gc2,
    int x, int y, char *string, int len)
{
    fe_DisplayUTF8String(TRUE, dpy, d, font, gc, gc2, x, y, string, len);
}

int
fe_DrawUCS2String(Display *dpy, Drawable d, fe_Font font, GC gc, int x,
    int y, uint16 *string, int len)
{
    unsigned char *utf8p;
    int fontAscent, fontDescent, direction; 
    XCharStruct overall;
 
   /*
    * Convert from UCS2 to UTF8
    */
    utf8p = INTL_UCS2ToUTF8(string, len);
    if (utf8p == NULL) {
        return -1;
    }
    fe_DisplayUTF8String(FALSE, dpy, d, font, gc, NULL, x, y, 
						utf8p, strlen(utf8p));
    fe_UTF8TextExtents(font, utf8p, strlen(utf8p), &direction,
                            &fontAscent, &fontDescent, &overall);
    free(utf8p);
    return overall.width;
}

void
fe_UCS2TextExtents(fe_Font font, uint16 *string, int len, int *direction, 
    			int *fontAscent, int *fontDescent, XCharStruct *overall)
{
    unsigned char *utf8p;
 
   /*
    * Convert from UCS2 to UTF8
    */
    utf8p = INTL_UCS2ToUTF8(string, len);
    if (utf8p == NULL) {
	*direction = 0;
	*fontAscent = 0;
	*fontDescent = 0;
    	overall->width = 0;
        return;
    }
    fe_UTF8TextExtents(font, utf8p, strlen(utf8p), direction,
                            fontAscent, fontDescent, overall);
    free(utf8p);
}


XmString
XUPF_UCS2ToXmString(uint16 *uniChars, int32 length, fe_Font font, 
						XmFontList *fontList)
{
	fe_UnicodePseudoFont *ufont = (fe_UnicodePseudoFont *)font;
    unsigned char *utf8p;
	XmString xmstring;
 
	if ((ufont==NULL) || (ufont->tag != UNICODE_PSEUDO_FONT_TAG)) {
		/* something is wrong. */
		return NULL;
	}
   /*
    * Convert from UCS2 to UTF8
    */
    if (length) {
		utf8p = INTL_UCS2ToUTF8(uniChars, length);
    	if (utf8p == NULL) {
        	return NULL;
    	}
	}
	else
		utf8p = XP_STRDUP("");
	xmstring = fe_utf8_to_XmString(ufont, utf8p, strlen(utf8p), fontList);
    free(utf8p);
	return xmstring;
}

fe_Font
XUPF_LoadDefaultFont(Display *dpy)
{
    fe_UnicodePseudoFont *ufont;

	ufont = fe_LoadUnicodeFont(NULL, "helvetica", 0, 4, 0, CS_LATIN1,
                                   0, 0, dpy);
	return ufont;
}

/*
 * fe_utf8_to_XmString - Convert a utf8 string to a XmString
 *
 * Parse the utf8 string into locally displayable encoded segments
 * and add each segment onto a XmString
 *
 * If there are errors display something.
 * hopefully the user will see the garbage and try a different encoding
 */
XmString
fe_utf8_to_XmString(fe_Font font, char *string, int len, XmFontList *fontList)
{
	fe_UnicodePseudoFont *ufont = (fe_UnicodePseudoFont *)font;
	unsigned char segment[X11STRING_LIMIT*6];
	int16 encoding;
	char *encoding_name;
	int parsed_cnt = 0;
	int this_parsed_cnt, seg_data_len;
	XmString xm_string = NULL;
	XmString this_xm_seg, old_xm_string;
	XFontStruct *xfont;


	if ((ufont==NULL) || (ufont->tag != UNICODE_PSEUDO_FONT_TAG)) {
		/* something is wrong. I will just return a guess */
		return NULL;
	}

	/*
	 * Break up the unicode string into locally sizeable encoded segments
	 */
	while (1) {
		/* get a single encoding string */
		this_parsed_cnt = utf8_to_local_encoding(string+parsed_cnt, 
										len-parsed_cnt, 
										segment, sizeof(segment),
										&seg_data_len, &encoding);
		if (this_parsed_cnt < 1) /* no more input data */
			break;

		parsed_cnt += this_parsed_cnt;

		/*
		 * Get update the XmFontList
		 */
		if (encoding >= 0)
			xfont = fe_UnicodeGetXfont(ufont, encoding);
		else
			xfont = NULL;

		/*
		 * Handle errors
		 */
		if ((encoding < 0) || (!xfont)) {
            fe_UnicodeConvertToPlaceHolder(ufont, &encoding, &xfont,
                            segment, seg_data_len);
		}

		/*
		* Add this component to the XmString
		*/
		encoding_name = INTL_CsidToCharsetNamePt(encoding);
		this_xm_seg = XmStringCreateLtoR(segment, encoding_name);
		if (xm_string) {
			old_xm_string = xm_string;
			xm_string = XmStringConcat(old_xm_string, this_xm_seg);
			XmStringFree(old_xm_string);
			XmStringFree(this_xm_seg);
		}
		else
			xm_string = this_xm_seg;
	}

	/*
	 * handle empty string
	 */
	if (xm_string==NULL) {
		(void)fe_UnicodeGetXfont(ufont, CS_LATIN1);
		xm_string = XmStringCreateSimple("");
	}

	*fontList = ufont->xmfontlist;
	return xm_string;
}

/********************************************************************
  New Code
 ********************************************************************/
/*
 * these values can be changed to whatever is convenient for Java
 */
enum
{
	INTL_FontStyleNormal,
	INTL_FontStyleBold,
	INTL_FontStyleItalic,
	INTL_FontStyleBoldItalic
};


static XFontStruct *
intl_GetFontFace(Display *display, int sizeNum, fe_FontSize *size, int faceNum,
	char **fontName)
{
	fe_FontFace	*face;
	int		i;
	char		*name;
	XFontStruct	*ret;

	for (i = 0; i < 2; i++, faceNum = 0)
	{
		face = size->faces[faceNum];
		if (!face)
		{
			continue;
		}
		if (face->font)
		{
			if (fontName)
			{
				if (size->size)
				{
					*fontName =
						strdup(face->longXLFDFontName);
				}
				else
				{
					*fontName = PR_smprintf(
						face->longXLFDFontName,
						sizeNum);
				}
			}
			return face->font;
		}
		else
		{
			if (size->size)
			{
				name = strdup(face->longXLFDFontName);
			}
			else
			{
				name = PR_smprintf(face->longXLFDFontName,
					sizeNum);
			}
			if (!name)
			{
				return NULL;
			}
			ret = fe_LoadXFont(display, name);
			if (ret)
			{
				face->font = ret;
				if (fontName)
				{
					*fontName = name;
				}
				return ret;
			}
		}
	}

	return NULL;
}


static XFontStruct *
intl_GetFontSize(Display *display, fe_FontFamily *family, int sizeNum,
	int faceNum, char **fontName)
{
	int		closestSize = 0;
	int		firstSize;
	int		i;
	double		minDist;
	double		newDist;
	XFontStruct	*ret;
	fe_FontSize	*size;

	if (!family->pixelSizes)
	{
		return NULL;
	}

	if (family->pixelSizes->size)
	{
		firstSize = 0;
	}
	else
	{
		if (family->numberOfPixelSizes == 1)
		{
			return intl_GetFontFace(display, sizeNum,
				family->pixelSizes, faceNum, fontName);
		}
		firstSize = 1;
	}

	minDist = (1E+36);
	for (i = firstSize; i < family->numberOfPixelSizes; i++)
	{
		size = &family->pixelSizes[i];
		newDist = ((sizeNum - size->size) * (sizeNum - size->size));
		if (newDist < minDist)
		{
			minDist = newDist;
			closestSize = i;
		}
		else
		{
			break;
		}
	}

	for (i = 0; i < family->numberOfPixelSizes; i++)
	{
		if (closestSize - i >= firstSize)
		{
			size = &family->pixelSizes[closestSize - i];
			ret = intl_GetFontFace(display, size->size, size,
				faceNum, fontName);
			if (ret)
			{
				return ret;
			}
		}
		if (closestSize + i < family->numberOfPixelSizes)
		{
			size = &family->pixelSizes[closestSize + i];
			ret = intl_GetFontFace(display, size->size, size,
				faceNum, fontName);
			if (ret)
			{
				return ret;
			}
		}
	}

	if (!family->pixelSizes->size)
	{
		return intl_GetFontFace(display, sizeNum, family->pixelSizes,
			faceNum, fontName);
	}

	return NULL;
}


XFontStruct *
intl_GetFont(Display *display, int16 characterSet, char *familyName,
	char *foundry, int sizeNum, int style, char **fontName)
{
	fe_FontCharSet	*charset;
	int		faceNum;
	fe_FontFamily	*family = NULL;
	int		i;
	int		j;
	fe_FontPitch	*pitch;
	XFontStruct	*ret;


	/*
	 * check the charset
	 */
        characterSet &= 0x00ff;
	if ((0 > characterSet) || (characterSet >= INTL_CHAR_SET_MAX))
	{
		return NULL;
	}
	if (fe_CharSetInfoArray[characterSet].type == FE_FONT_TYPE_GROUP)
	{
		return NULL;
	}
	if (!fe_FontCharSets[characterSet].name)
	{
		return NULL;
	}


	/*
	 * check the family
	 */
	if (!familyName)
	{
		familyName = "";
	}


	/*
	 * check the foundry
	 */
	if (!foundry)
	{
		foundry = "";
	}


	/*
	 * check the size
	 */
	if (sizeNum <= 0)
	{
		return NULL;
	}


	/*
	 * check the style
	 */
	switch (style)
	{
	case INTL_FontStyleNormal:
		faceNum = 0;
		break;
	case INTL_FontStyleBold:
		faceNum = 1;
		break;
	case INTL_FontStyleItalic:
		faceNum = 2;
		break;
	case INTL_FontStyleBoldItalic:
		faceNum = 3;
		break;
	default:
		return NULL;
	}


	charset = &fe_FontCharSets[characterSet];

	/*
	 * iterate over the pitches (proportional and fixed)
	 * and for each pitch, look for the requested family and foundry
	 */
	for (i = 0; i < 2; i++)
	{
		pitch = &charset->pitches[i];
		for (j = 0; j < pitch->numberOfFamilies; j++)
		{
			family = &pitch->families[j];
			if ((!strcmp(family->family, familyName)) &&
				(!strcmp(family->foundry, foundry)))
			{
				ret = intl_GetFontSize(display, family,
					sizeNum, faceNum, fontName);
				if (ret)
				{
					return ret;
				}
			}
		}
	}

	/*
	 * couldn't find the requested family/foundry pair
	 * so try matching just the family
	 */
	for (i = 0; i < 2; i++)
	{
		pitch = &charset->pitches[i];
		for (j = 0; j < pitch->numberOfFamilies; j++)
		{
			family = &pitch->families[j];
			if (!strcmp(family->family, familyName))
			{
				ret = intl_GetFontSize(display, family,
					sizeNum, faceNum, fontName);
				if (ret)
				{
					return ret;
				}
			}
		}
	}

	/*
	 * couldn't find the requested family
	 * so let's try the one selected in the UI
	 */
	for (i = 0; i < 2; i++)
	{
		pitch = &charset->pitches[i];
		for (j = 0; j < pitch->numberOfFamilies; j++)
		{
			family = &pitch->families[j];
			if (family->selected)
			{
				break;
			}
			else
			{
				family = NULL;
			}
		}
		if (family)
		{
			ret = intl_GetFontSize(display, family, sizeNum,
				faceNum, fontName);
			if (ret)
			{
				return ret;
			}
		}
	}

	/*
	 * if we couldn't load the selected family
	 * we try all of the families in turn
	 */
	for (i = 0; i < 2; i++)
	{
		pitch = &charset->pitches[i];
		for (j = 0; j < pitch->numberOfFamilies; j++)
		{
			family = &pitch->families[j];
			ret = intl_GetFontSize(display, family, sizeNum,
				faceNum, fontName);
			if (ret)
			{
				return ret;
			}
		}
	}

	return NULL;
}


XFontStruct *
INTL_GetFont(Display *display, int16 characterSet, char *familyName,
	char *foundry, int sizeNum, int style)
{
	return intl_GetFont(display, characterSet, familyName, foundry,
		sizeNum, style, NULL);
}


static int16 *
INTL_GetXCharSetIDs(Display *dpy)
{
	unsigned char	*charset;
	int		len;
	int16		*p;
	int16		*ret;

	ret = NULL;

	if (!fe_LocaleCharSets)
	{
		fe_GetLocaleCharSets(dpy);
		if (!fe_LocaleCharSets)
		{
			ret = (int16 *) malloc(2 * sizeof(int16));
			if (!ret)
			{
				return NULL;
			}
			ret[0] = CS_LATIN1;
			ret[1] = 0;
			return ret;
		}
	}

	len = 0;
	charset = fe_LocaleCharSets;
	while (*charset)
	{
		len++;
		charset++;
	}

	ret = (int16 *) malloc((len + 1) * sizeof(int16));
	if (!ret)
	{
		return NULL;
	}

	p = ret;
	charset = fe_LocaleCharSets;
	while (*charset)
	{
		*p++ = fe_CharSetInfoArray[*charset++].charsetID;
	}
	*p = 0;

	return ret;
}


char *
INTL_GetXFontName(Display *display, int16 characterSet, char *familyName,
	char *foundry, int sizeNum, int style)
{
	char	*fontName;

	fontName = NULL;

	(void) intl_GetFont(display, characterSet, familyName, foundry,
		sizeNum, style, &fontName);

	return fontName;
}
