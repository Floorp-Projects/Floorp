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
   scroller.c --- defines a subclass of XmScrolledWindow
   Created: Jamie Zawinski <jwz@netscape.com>, 23-Jul-94.
 */


#ifndef _FE_SCROLLER_H_
#define _FE_SCROLLER_H_

#include <Xm/Xm.h>

#if XmVersion >= 2000

/*
 * There is a backward compatibility bug in motif 2.0 that prevents
 * the XmScrolledWindow class from behaving just as the motif 1.2
 * one.  For whatever reasons, the mozilla scrolling code depends on
 * the 1.2 behavior.  Fixing the problem in Mozilla should not be
 * hard.  The logic in scroll.c should probably be updated to work
 * with both motif 1.2 and 2.0.
 *
 * The current hackery solution (yes its disgusting) is to use the
 * XmScrolledWindow code from 1.2.  In defense of Netscape:  The only
 * reason whay such a sickening solution was used, was to make the
 * free mozilla date with building motif 2.0 code.
 *
 * -ramiro
 *
 */
#ifdef FEATURE_MOTIF20_SCROLLED_WINDOW_HACKING
#include <XmPatches/HackedW.h>
#define SCROLLER_SET_AREAS XmHackedWindowSetAreas
#else
#include <Xm/ScrolledW.h>
#define SCROLLER_SET_AREAS XmScrolledWindowSetAreas
#endif /* FEATURE_MOTIF20_SCROLLED_WINDOW_HACKING */

#else
#include <Xm/ScrolledW.h>
#define SCROLLER_SET_AREAS XmScrolledWindowSetAreas

#endif /* XmVersion >= 2000 */

extern WidgetClass scrollerClass;
typedef struct _ScrollerClassRec *ScrollerClass;
typedef struct _ScrollerRec      *Scroller;


#endif /* _FE_SCROLLER_H_ */
