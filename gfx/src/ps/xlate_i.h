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

//#define M12N

#include "xp.h"
#include "xlate.h"
#include "nsPSStructs.h"

extern PS_FontInfo *PSFE_MaskToFI[N_FONTS];

#define LINE_WIDTH 160
#define TEXT_WIDTH 8
#define TEXT_HEIGHT 16

// this should be in the devicecontext now.
extern void xl_begin_document(PSContext*);
extern void xl_begin_page(PSContext*,int);
extern void xl_end_page(PSContext*,int);
extern void xl_end_document(PSContext*);
extern void xl_show(PSContext *cx, char* txt, int len, char*);
extern void xl_moveto(PSContext* cx, int x, int y);
extern void xl_moveto_loc(PSContext* cx, int x, int y);
extern void xl_lineto(PSContext* cx, int x1, int y1);
extern void xl_circle(PSContext* cx, int w, int h);
extern void xl_box(PSContext* cx, int w, int h);
extern void xl_line(PSContext* cx, int x1, int y1, int x2, int y2, int thick);
extern void xl_stroke(PSContext*);
extern void xl_fill(PSContext*);
extern void xl_colorimage(PSContext *cx, int x, int y, int w, int h,IL_Pixmap *image, IL_Pixmap *mask);
extern void xl_begin_squished_text(PSContext*, float);
extern void xl_end_squished_text(PSContext*);
extern void xl_initialize_translation(PSContext*, PrintSetup*);
extern void xl_finalize_translation(PSContext*);
extern void xl_annotate_page(PSContext*, char*, int, int, int);
extern void xl_draw_border(PSContext *, int , int , int , int , int );
extern void xl_draw_3d_border(PSContext *, int , int , int , int , int, int tl, int br );
extern void xl_draw_3d_radiobox(PSContext *, int , int , int , int , int, int t, int b, int c);
extern void xl_draw_3d_checkbox(PSContext *, int , int , int , int , int, int tl, int br, int c);
extern void xl_draw_3d_arrow(PSContext *, int, int, int, int, int, XP_Bool, int, int, int);
extern XP_Bool xl_item_span(PSContext* cx, int top, int bottom);

extern XP_Bool psfe_init_image_callbacks(PSContext *cx);

struct LineRecord_struct;
