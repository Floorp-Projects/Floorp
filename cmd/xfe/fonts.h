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
   fonts.h --- fonts, font selector, and related stuff
   Created: Erik van der Poel <erik@netscape.com>, 6-Oct-95.
 */


#ifndef __xfe_fonts_h_
#define __xfe_fonts_h_

#ifdef	__cplusplus
extern "C" {
#endif

#include "xp_core.h"
#include <lo_ele.h>
#include <X11/Xlib.h>

XP_BEGIN_PROTOS

/*
 * defines
 */

#define FE_FONT_TYPE_X8		0x1
#define FE_FONT_TYPE_X16	0x2
#define FE_FONT_TYPE_GROUP	0x4
#ifndef NO_WEB_FONTS
#define FE_FONT_TYPE_RENDERABLE 0x8
#endif


/* the following are macros so that we avoid the overhead of function calls */

#ifndef NO_WEB_FONTS
#define FE_TEXT_EXTENTS(charset, font, string, len, fontAscent, fontDescent,  \
        overall)                                                              \
do                                                                            \
{                                                                             \
        int             direction;                                            \
        fe_Font         platform_font;                                        \
        int             font_type;                                            \
                                                                              \
        platform_font = ((fe_FontWrap *) font)->platform_font;                \
        font_type = ((fe_FontWrap *) font)->distinguisher;                    \
                                                                              \
        if (FE_FONT_TYPE_X8 == font_type)                                     \
        {                                                                     \
                XTextExtents(((XFontStruct *) platform_font),                 \
                             (string), (len), &direction,                     \
                             (fontAscent), (fontDescent), (overall));         \
        }                                                                     \
        else if (FE_FONT_TYPE_X16 == font_type)                               \
        {                                                                     \
                XTextExtents16(((XFontStruct *) platform_font),               \
                               (XChar2b *) (string), (len)/2, &direction,     \
                               (fontAscent), (fontDescent), (overall));       \
        }                                                                     \
        else if (FE_FONT_TYPE_GROUP == font_type)                             \
        {                                                                     \
                (*fe_CharSetFuncsArray[fe_CharSetInfoArray[(charset) & 0xff]. \
                        info].textExtents)(platform_font, (string), (len),    \
                        &direction, (fontAscent), (fontDescent), (overall));  \
        }                                                                     \
        else                                                                  \
        {                                                                     \
            (overall)->width = nfrf_MeasureText(((struct nfrf *)              \
                platform_font), NULL, 0, string, len, NULL, 0, NULL);         \
            *(fontAscent) =                                                   \
                nfrf_GetFontAscent(((struct nfrf *) platform_font), NULL);    \
	    *(fontDescent) =                                                  \
                nfrf_GetFontDescent(((struct nfrf *) platform_font), NULL);   \
            (overall)->rbearing = (overall)->width;                           \
            (overall)->lbearing = 0;                                          \
            (overall)->ascent = *(fontAscent);                                \
   	    (overall)->descent = *(fontDescent);                              \
        }                                                                     \
} while (0)
#else
#define FE_TEXT_EXTENTS(charset, font, string, len, fontAscent, fontDescent,  \
        overall)                                                              \
do                                                                            \
{                                                                             \
        int             direction;                                            \
        int             type;                                                 \
                                                                              \
        type = fe_CharSetInfoArray[(charset) & 0xff].type;                    \
                                                                              \
        if (type == FE_FONT_TYPE_X8)                                          \
        {                                                                     \
                XTextExtents((font), (string), (len), &direction,             \
                        (fontAscent), (fontDescent), (overall));              \
        }                                                                     \
        else if (type == FE_FONT_TYPE_X16)                                    \
        {                                                                     \
                XTextExtents16((font), (XChar2b *) (string), (len)/2,         \
                        &direction, (fontAscent), (fontDescent), (overall));  \
        }                                                                     \
        else                                                                  \
        {                                                                     \
                (*fe_CharSetFuncsArray[fe_CharSetInfoArray[(charset) & 0xff]. \
                        info].textExtents)((font), (string), (len),           \
                        &direction, (fontAscent), (fontDescent), (overall));  \
        }                                                                     \
} while (0)
#endif

#ifndef NO_WEB_FONTS
#define FE_FONT_EXTENTS(charset, font, fontAscent, fontDescent)               \
do                                                                            \
{                                                                             \
        fe_Font         platform_font;                                        \
        int             font_type;                                            \
                                                                              \
        platform_font = ((fe_FontWrap *) font)->platform_font;                \
        font_type = ((fe_FontWrap *) font)->distinguisher;                    \
                                                                              \
	if ((FE_FONT_TYPE_X8 == font_type) || (FE_FONT_TYPE_X16 == font_type))\
        {                                                                     \
                *(fontAscent) = ((XFontStruct *) platform_font)->ascent;      \
                *(fontDescent) = ((XFontStruct *) platform_font)->descent;    \
        }                                                                     \
        else if (FE_FONT_TYPE_GROUP == font_type)                             \
        {                                                                     \
                fe_GenericFontExtents(charset, platform_font,                 \
                                        (fontAscent), (fontDescent));         \
        }                                                                     \
        else                                                                  \
        {                                                                     \
                *(fontAscent) =                                               \
                    nfrf_GetFontAscent(((struct nfrf *) platform_font),       \
                                       NULL);                                 \
                *(fontDescent) =                                              \
                    nfrf_GetFontDescent(((struct nfrf *) platform_font),      \
                                        NULL);                                \
         }                                                                    \
} while (0)
#else
#define FE_FONT_EXTENTS(charset, font, fontAscent, fontDescent)               \
do                                                                            \
{                                                                             \
        fe_CharSetInfo  *info;                                                \
                                                                              \
        info = &fe_CharSetInfoArray[(charset) & 0xff];                        \
                                                                              \
        if (info->type == FE_FONT_TYPE_GROUP)                                 \
        {                                                                     \
                fe_GenericFontExtents(charset, (font),                        \
                                        (fontAscent), (fontDescent));         \
        }                                                                     \
        else                                                                  \
        {                                                                     \
                *(fontAscent) = ((XFontStruct *) (font))->ascent;             \
                *(fontDescent) = ((XFontStruct *) (font))->descent;           \
        }                                                                     \
} while (0)
#endif

#ifndef NO_WEB_FONTS
#define FE_DRAW_STRING(charset, dpy, d, font, gc, x, y, string, len)          \
do                                                                            \
{                                                                             \
        int       font_type;                                                  \
        fe_Font   platform_font;                                              \
                                                                              \
        font_type = ((fe_FontWrap *) font)->distinguisher;                    \
        platform_font = ((fe_FontWrap *) font)->platform_font;                \
                                                                              \
        if (FE_FONT_TYPE_X8 == font_type)                                     \
        {                                                                     \
                XDrawString((dpy), (d), (gc), (x), (y), (string),             \
                        (len));                                               \
        }                                                                     \
        else if (FE_FONT_TYPE_X16 == font_type)                               \
        {                                                                     \
                XDrawString16((dpy), (d), (gc), (x), (y),                     \
                        (XChar2b *) (string), (len)/2);                       \
        }                                                                     \
        else if (FE_FONT_TYPE_GROUP == font_type)                             \
        {                                                                     \
                (*fe_CharSetFuncsArray[fe_CharSetInfoArray[(charset) & 0xff]. \
                        info].drawString)((dpy), (d), platform_font,          \
                        (gc), (x), (y), (string), (len));                     \
        }                                                                     \
        else                                                                  \
        {                                                                     \
 	        struct nfrc *   rc;                                           \
                void *          rcbuf[4];                                     \
                                                                              \
                rcbuf[0] = (dpy);                                             \
                rcbuf[1] = (void *) (d);                                      \
                rcbuf[2] = (gc);                                              \
                rc = nffbu_CreateRenderingContext(fe_FontUtility,             \
                                                  NF_RC_DIRECT, 0, rcbuf, 3,  \
                                                  NULL);                      \
 	        (void) nfrf_DrawText(((struct nfrf *) platform_font), rc,     \
                                     (x), (y), 0, (string), (len), NULL);     \
                nfrc_release(rc, NULL);                                       \
        }                                                                     \
} while (0)
#else
#define FE_DRAW_STRING(charset, dpy, d, font, gc, x, y, string, len)          \
do                                                                            \
{                                                                             \
        int  type;                                                            \
                                                                              \
        type = fe_CharSetInfoArray[(charset) & 0xff].type;                    \
                                                                              \
        if (type == FE_FONT_TYPE_X8)                                          \
        {                                                                     \
                XDrawString((dpy), (d), (gc), (x), (y), (string),             \
                        (len));                                               \
        }                                                                     \
        else if (type == FE_FONT_TYPE_X16)                                    \
        {                                                                     \
                XDrawString16((dpy), (d), (gc), (x), (y),                     \
                        (XChar2b *) (string), (len)/2);                       \
        }                                                                     \
        else                                                                  \
        {                                                                     \
                (*fe_CharSetFuncsArray[fe_CharSetInfoArray[(charset) & 0xff]. \
                        info].drawString)((dpy), (d), (font),                 \
                        (gc), (x), (y), (string), (len));                     \
        }                                                                     \
} while (0)
#endif

#ifndef NO_WEB_FONTS
#define FE_DRAW_IMAGE_STRING(charset, dpy, d, font, gc, gc2, x, y, string, len) \
do                                                                            \
{                                                                             \
        int       font_type;                                                  \
        fe_Font   platform_font;                                              \
                                                                              \
        font_type = ((fe_FontWrap *) font)->distinguisher;                    \
        platform_font = ((fe_FontWrap *) font)->platform_font;                \
                                                                              \
        if (FE_FONT_TYPE_X8 == font_type)                                     \
        {                                                                     \
                XDrawImageString((dpy), (d), (gc), (x), (y),                  \
                        (string), (len));                                     \
        }                                                                     \
        else if (FE_FONT_TYPE_X16 == font_type)                               \
        {                                                                     \
                XDrawImageString16((dpy), (d), (gc), (x), (y),                \
                        (XChar2b *) (string), (len)/2);                       \
        }                                                                     \
        else if (FE_FONT_TYPE_GROUP == font_type)                             \
        {                                                                     \
                (*fe_CharSetFuncsArray[fe_CharSetInfoArray[(charset) & 0xff]. \
                        info].drawImageString)((dpy), (d), platform_font,     \
                        (gc), (gc2), (x), (y), (string), (len));              \
        }                                                                     \
        else                                                                  \
        {                                                                     \
	        struct nfrc *rc;                                              \
	        void * rcbuf[4];                                              \
                                                                              \
	        rcbuf[0] = (dpy);                                             \
	        rcbuf[1] = (void *) (d);                                      \
	        rcbuf[2] = (gc);                                              \
                rcbuf[3] = (void *)(NF_DrawImageTextMask);                            \
	        rc = nffbu_CreateRenderingContext(fe_FontUtility,             \
                                                  NF_RC_DIRECT,               \
					          0, rcbuf, 4, NULL);         \
	        (void) nfrf_DrawText(((struct nfrf *) platform_font), rc,     \
                                     (x), (y), 0, (string), (len), NULL);     \
	        nfrc_release(rc, NULL);                                       \
        }                                                                     \
} while (0)
#else
#define FE_DRAW_IMAGE_STRING(charset, dpy, d, font, gc, gc2, x, y, string, len) \
do                                                                            \
{                                                                             \
        int  type;                                                            \
                                                                              \
        type = fe_CharSetInfoArray[(charset) & 0xff].type;                    \
                                                                              \
        if (type == FE_FONT_TYPE_X8)                                          \
        {                                                                     \
                XDrawImageString((dpy), (d), (gc), (x), (y), (string),        \
                                 (len));                                      \
        }                                                                     \
        else if (type == FE_FONT_TYPE_X16)                                    \
        {                                                                     \
                XDrawImageString16((dpy), (d), (gc), (x), (y),                \
                        (XChar2b *) (string), (len)/2);                       \
        }                                                                     \
        else                                                                  \
        {                                                                     \
                (*fe_CharSetFuncsArray[fe_CharSetInfoArray[(charset) & 0xff]. \
                        info].drawImageString)((dpy), (d),                    \
                        (font), (gc), (gc2), (x), (y), (string), (len));      \
        }                                                                     \
} while (0)
#endif


#ifndef NO_WEB_FONTS
#define FE_SET_GC_FONT(charset, gc, font, flags)                              \
do                                                                            \
{                                                                             \
        int       font_type;                                                  \
        fe_Font   platform_font;                                              \
                                                                              \
        font_type = ((fe_FontWrap *) font)->distinguisher;                    \
        platform_font = ((fe_FontWrap *) font)->platform_font;                \
                                                                              \
        if ((FE_FONT_TYPE_X8 == font_type) ||                                 \
            (FE_FONT_TYPE_X16 == font_type))                                  \
        {                                                                     \
                (gc)->font = ((XFontStruct *) platform_font)->fid;            \
                *(flags) |= GCFont;                                           \
        }                                                                     \
} while (0)
#else
#define FE_SET_GC_FONT(charset, gc, font, flags)                              \
do                                                                            \
{                                                                             \
        fe_CharSetInfo  *info;                                                \
                                                                              \
        info = &fe_CharSetInfoArray[(charset) & 0xff];                        \
                                                                              \
        if (info->type != FE_FONT_TYPE_GROUP)                                 \
        {                                                                     \
                (gc)->font = ((XFontStruct *) (font))->fid;                   \
                *(flags) |= GCFont;                                           \
        }                                                                     \
} while (0)
#endif


/*
 * typedefs and structs
 */

#ifndef NO_WEB_FONTS
typedef struct fe_FontWrap
{
        unsigned char    distinguisher;
        void *           platform_font;
} fe_FontWrap;
#endif

typedef void *fe_Font;

typedef struct fe_FontFace fe_FontFace;

struct fe_FontFace
{
	fe_FontFace	*next;

	char		*longXLFDFontName;

	fe_Font		font;
	fe_Font		javafont;
	XP_Bool         loaded;
};


typedef struct fe_FontSize fe_FontSize;

struct fe_FontSize
{
	fe_FontSize	*next;

	int		size;

	/* whether the user has selected this size in preferences */
	unsigned int	selected : 1;

        /* whether this record is in the css pool */
        unsigned int    in_pool: 1;

	/* 0 = normal, 1 = bold, 2 = italic, 3 = boldItalic */
	fe_FontFace	*faces[4];

};


typedef struct fe_FontFamily
{
	/* displayed name of this family/foundry */
	char		*name;

	/* XLFD name of foundry */
	char		*foundry;

	/* XLFD name of family */
	char		*family;

	/* whether the user has selected this family in preferences */
	unsigned int	selected : 1;

	/* whether not to allow scaling for this family */
	unsigned int	allowScaling : 1;

	/* whether the htmlSizes have been computer or not */
	unsigned int	htmlSizesComputed : 1;

	/* one for each document size, 1-7 */
	fe_FontSize	*htmlSizes[7];

	/* selected size if scaled font */
	int		scaledSize;

	/* pointer to all pixel sizes within this family */
	fe_FontSize	*pixelSizes;

	/* allocated size of pixelSizes array */
	short		pixelAlloc;

	/* actual number of pixel sizes */
	short		numberOfPixelSizes;

	/* pointer to all point sizes within this family */
	fe_FontSize	*pointSizes;

	/* allocated size of pointSizes array */
	short		pointAlloc;

	/* actual number of point sizes */
	short		numberOfPointSizes;

        /* pointer to list of temporarily instantiated point sizes */
        fe_FontSize     *pool;
} fe_FontFamily;


/* one for each pitch: proportional and fixed */
typedef struct fe_FontPitch
{
	/* pointer to size menu for this pitch */
	Widget		sizeMenu;

	/* pointer to all families of this charset/pitch */
	fe_FontFamily	*families;

	/* allocated size of families array */
	short		alloc;

	/* actual number of families */
	short		numberOfFamilies;

	/* selected family */
	fe_FontFamily	*selectedFamily;

} fe_FontPitch;


/* we have one of these per charset */
typedef struct fe_FontCharSet
{
	/* displayed name of this charset */
	char		*name;

	/* official MIME name of this charset */
	char		*mimeName;

	/* whether the user has selected this one in the preferences */
	unsigned int	selected : 1;

	/* the two pitches: 0 = proportional and 1 = fixed */
	fe_FontPitch	pitches[2];

} fe_FontCharSet;


typedef struct fe_CharSetInfo
{
	int16			charsetID;
	unsigned char		type;
	unsigned char		info;
} fe_CharSetInfo;


typedef XP_Bool (*fe_AreFontsAvailFunc)(int16 win_csid);
typedef fe_Font (*fe_LoadFontFunc)(MWContext *context, char *familyName,
	int points, int sizeNum, int fontmask, int charset, int pitch,
        int faceNum, Display *dpy);
typedef void (*fe_TextExtentsFunc)(fe_Font font, char *string, int len,
	int *dir, int *fontAscent, int *fontDescent, XCharStruct *overall);
typedef void (*fe_DrawStringFunc)(Display *dpy, Drawable d, fe_Font font,
	GC gc, int x, int y, char *string, int len);
typedef void (*fe_DrawImageStringFunc)(Display *dpy, Drawable d, fe_Font font,
	GC gc, GC gc2, int x, int y, char *string, int len);

typedef struct fe_CharSetFuncs
{
	unsigned char			numberOfFonts;
	fe_AreFontsAvailFunc	areFontsAvail;
	fe_LoadFontFunc			loadFont;
	fe_TextExtentsFunc		textExtents;
	fe_DrawStringFunc		drawString;
	fe_DrawImageStringFunc	drawImageString;
} fe_CharSetFuncs;


typedef struct fe_FontSettings fe_FontSettings;

struct fe_FontSettings
{
	fe_FontSettings	*next;
	char		*spec;
};


/*
 * public data
 */

extern fe_FontCharSet fe_FontCharSets[];

extern unsigned char fe_SortedFontCharSets[];

extern fe_CharSetInfo fe_CharSetInfoArray[];

extern fe_CharSetFuncs fe_CharSetFuncsArray[];

#ifndef NO_WEB_FONTS
extern struct nffbu * fe_FontUtility;
#endif

/*
 * public functions
 */
XP_Bool fe_IsCharSetSupported(int16 doc_csid);
XP_Bool fe_AreFontsAvail(int16 win_csid);

fe_Font fe_LoadUnicodeFont(void *, char *familyName, int points,
                           int sizeNum, int fontmask, int charset,
                           int pitch, int faceNum, Display *dpy);
void fe_FreeUnicodeFonts(void);

XmString fe_utf8_to_XmString(fe_Font, char *, int, XmFontList *);

void fe_ComputeFontSizeTable(fe_FontFamily *family);

void fe_FreeFontGroups(void);

void fe_FreeFonts(void);

void fe_FreeFontSettings(fe_FontSettings *set);

void fe_FreeFontSizeTable(fe_FontFamily *family);

void fe_GenericFontExtents(int num, fe_Font font, int *fontAscent,
	int *fontDescent);

char *fe_GetFontCharSetSetting(void);

XtPointer fe_GetFont(MWContext *context, int sizeNum, int fontmask);

XtPointer fe_GetFontOrFontSet(MWContext *context, char *familyName,
			      int sizeNum, int fontmask, XmFontType *type);

XtPointer fe_GetFontOrFontSetFromFace(MWContext *context, LO_TextAttr *attr,
				      char *net_font_face, int sizeNum,
				      int fontmask, XmFontType *type);

fe_FontSettings *fe_GetFontSettings(void);

int fe_GetStrikePosition(int charset, fe_Font font);

int fe_GetUnderlinePosition(int charset);

void fe_InitFonts(Display *dpy);

#ifndef NO_WEB_FONTS
extern void fe_DoneWithFont(fe_Font font);
#endif

fe_Font fe_LoadFontFromFace(MWContext *context, LO_TextAttr *attr, int16 *charset,
			    char *net_font_face, int size, int fontmask);

void fe_ReadFontCharSet(char *charset);

void fe_ReadFontSpec(char *spec);

void fe_SetFontSettings(fe_FontSettings *set);

XP_END_PROTOS
int fe_WebfontsNeedReload(MWContext *context);


#ifdef	__cplusplus
}
#endif

#endif /* __xfe_fonts_h_ */
