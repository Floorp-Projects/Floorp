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

#define M12N

#include "xp.h"
#include "xlate.h"

#define N_FONTS 8

typedef struct {
    short llx, lly, urx, ury;
} PS_BBox;

typedef struct {
	short wx, wy;
	PS_BBox charBBox;
} PS_CharInfo;

typedef struct {
    char *name;
    PS_BBox fontBBox;
    short upos, uthick;
    PS_CharInfo chars[256];
} PS_FontInfo;

#define MAKE_FE_FUNCS_PREFIX(f) TXFE_##f
#define MAKE_FE_FUNCS_EXTERN
#include "mk_cx_fn.h"

extern PS_FontInfo *PSFE_MaskToFI[N_FONTS];

#define LINE_WIDTH 160

#define TEXT_WIDTH 8
#define TEXT_HEIGHT 16

#define MAKE_FE_FUNCS_PREFIX(f) PSFE_##f
#define MAKE_FE_FUNCS_EXTERN
#include "mk_cx_fn.h"

extern void xl_begin_document(MWContext*);
extern void xl_begin_page(MWContext*,int);
extern void xl_end_page(MWContext*,int);
extern void xl_end_document(MWContext*);
extern void xl_show(MWContext *cx, char* txt, int len, char*);
extern void xl_moveto(MWContext* cx, int x, int y);
extern void xl_moveto_loc(MWContext* cx, int x, int y);
extern void xl_circle(MWContext* cx, int w, int h);
extern void xl_box(MWContext* cx, int w, int h);
extern void xl_line(MWContext* cx, int x1, int y1, int x2, int y2, int thick);
extern void xl_stroke(MWContext*);
extern void xl_fill(MWContext*);
extern void xl_colorimage(MWContext *cx, int x, int y, int w, int h,
                          IL_Pixmap *image, IL_Pixmap *mask);
extern void xl_begin_squished_text(MWContext*, float);
extern void xl_end_squished_text(MWContext*);
extern void xl_initialize_translation(MWContext*, PrintSetup*);
extern void xl_finalize_translation(MWContext*);
extern void xl_annotate_page(MWContext*, char*, int, int, int);
extern void xl_draw_border(MWContext *, int , int , int , int , int );
extern void xl_draw_3d_border(MWContext *, int , int , int , int , int, int tl, int br );
extern void xl_draw_3d_radiobox(MWContext *, int , int , int , int , int, int t, int b, int c);
extern void xl_draw_3d_checkbox(MWContext *, int , int , int , int , int, int tl, int br, int c);
extern void xl_draw_3d_arrow(MWContext *, int, int, int, int, int, XP_Bool, int, int, int);
extern XP_Bool xl_item_span(MWContext* cx, int top, int bottom);

extern XP_Bool psfe_init_image_callbacks(MWContext *cx);

struct LineRecord_struct;
