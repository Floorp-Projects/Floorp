/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:ts=2:et:sw=2:
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "prtypes.h"
#include <X11/Xlib.h>

PR_BEGIN_EXTERN_C

#ifdef HAVE_XIE
PRBool 
DrawScaledImageXIE(Display *display,
                   Drawable aDest,
                   GC aGC,
                   Drawable aSrc,
                   PRInt32 aSrcWidth,
                   PRInt32 aSrcHeight,
                   PRInt32 aSX,
                   PRInt32 aSY,
                   PRInt32 aSWidth,
                   PRInt32 aSHeight,
                   PRInt32 aDX,
                   PRInt32 aDY,
                   PRInt32 aDWidth,
                   PRInt32 aDHeight);
#endif

PR_END_EXTERN_C
