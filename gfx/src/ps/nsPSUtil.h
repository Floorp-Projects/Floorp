/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef nsPSUtil_h___
#define nsPSUtil_h___

#include "xp.h"
#include "structs.h"
#include "xlate.h"

//XXX: These should become static methods on a object. 
// They were pulled wholesale from ps.c
extern "C" void xl_begin_document(MWContext *cx);
extern "C" void xl_initialize_translation(MWContext *cx, PrintSetup* ps);
extern "C" void xl_end_document(MWContext *cx);
extern "C" void xl_begin_page(MWContext *cx, int pn);
extern "C" void xl_end_page(MWContext *cx, int pn);
extern "C" void xl_finalize_translation(MWContext *cx);
extern "C" void xl_show(MWContext *cx, char* txt, int len, char *align);
extern "C" void xl_translate(MWContext* cx, int x, int y);
extern "C" void xl_moveto(MWContext* cx, int x, int y);
extern "C" void xl_line(MWContext* cx, int x1, int y1, int x2, int y2, int thick);
extern "C" void xl_box(MWContext* cx, int w, int h);

#endif

/* nsPSUtil_h___ */
