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

typedef union {
  css_node binary_node;
} YYSTYPE;
#define	NUMBER	258
#define	STRING	259
#define	PERCENTAGE	260
#define	LENGTH	261
#define	EMS	262
#define	EXS	263
#define	IDENT	264
#define	HEXCOLOR	265
#define	URL	266
#define	RGB	267
#define	CDO	268
#define	CDC	269
#define	IMPORTANT_SYM	270
#define	IMPORT_SYM	271
#define	DOT_AFTER_IDENT	272
#define	DOT	273
#define	LINK_PSCLASS	274
#define	VISITED_PSCLASS	275
#define	ACTIVE_PSCLASS	276
#define	LEADING_LINK_PSCLASS	277
#define	LEADING_VISITED_PSCLASS	278
#define	LEADING_ACTIVE_PSCLASS	279
#define	FIRST_LINE	280
#define	FIRST_LETTER	281
#define	WILD	282
#define	BACKGROUND	283
#define	BG_COLOR	284
#define	BG_IMAGE	285
#define	BG_REPEAT	286
#define	BG_ATTACHMENT	287
#define	BG_POSITION	288
#define	FONT	289
#define	FONT_STYLE	290
#define	FONT_VARIANT	291
#define	FONT_WEIGHT	292
#define	FONT_SIZE	293
#define	FONT_NORMAL	294
#define	LINE_HEIGHT	295
#define	LIST_STYLE	296
#define	LS_TYPE	297
#define	LS_NONE	298
#define	LS_POSITION	299
#define	BORDER	300
#define	BORDER_STYLE	301
#define	BORDER_WIDTH	302
#define	FONT_SIZE_PROPERTY	303
#define	FONTDEF	304


extern YYSTYPE css_lval;
