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

/* xupfonts.h - X Unicode Pseudo FONTS Header file */

#ifndef XUPFONTS_H
#define XUPFONTS_H

#define UNICODE_PSEUDO_FONT_TAG   0xabadbeef
#define UNICODE_PLACEHOLDER_WIDTH 10
typedef struct fe_UnicodePseudoFont
{
	unsigned int tag;
	char *family;
	XFontStruct	*xFonts[INTL_CHAR_SET_MAX];
	char xfont_inited[INTL_CHAR_SET_MAX];
	char xfont_scaled[INTL_CHAR_SET_MAX];
	char* xfont_name[INTL_CHAR_SET_MAX];
	char *fontFamily[INTL_CHAR_SET_MAX];
	char larger_fonts_avail[INTL_CHAR_SET_MAX];
	XmFontList xmfontlist;
	XFontSet xfontset;
	XmFontList xm_fontset;
	Display *dpy;
	int pitch;
	int sizeNum;
	int fontmask;
	int faceNum;
	int pixelSize;
	int ascent;
	int descent;
} fe_UnicodePseudoFont;

XFontStruct *fe_UnicodeGetXfont(fe_UnicodePseudoFont *ufont, uint16 encoding);

int fe_DrawUCS2String(Display *dpy, Drawable d, fe_Font font, GC gc, int x,
    int y, uint16 *string, int len);

void fe_freeUnicodePseudoFont(fe_Font font);

fe_Font fe_LoadUnicodeFontByPixelSize(void *not_used, char *familyName, 
	int pixelSize, int fontmask, int charset, int pitch, int faceNum,
         Display *dpy);

void fe_UCS2TextExtents(fe_Font font, uint16 *string, int len, int *direction, 
    			int *fontAscent, int *fontDescent, XCharStruct *overall);

void fe_UTF8TextExtents(fe_Font font, char *string, int len, int *direction,
    int *fontAscent, int *fontDescent, XCharStruct *overall);

void fe_DrawUTF8String(Display *dpy, Drawable d, fe_Font font, GC gc, int x,
    int y, char *string, int len);

fe_Font    XUPF_LoadDefaultFont(Display *dpy);
XmFontList XUPF_GetXmFontList(fe_Font font);
XmFontList XUPF_GetXmFontSet(fe_Font font);
XmString   XUPF_UCS2ToXmString(uint16 *uniChars, int32 length, 
						fe_Font ufont, XmFontList *fontList);
int XUPF_JavaPointToPixelSize(Display *dpy, int pointSize);

#endif /* XUPFONTS_H */
