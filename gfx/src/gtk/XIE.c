/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Syd Logan <syd@netscape.com>
 */

#include "drawers.h"

#ifdef HAVE_XIE

#include <stdio.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <X11/extensions/XIElib.h>

/*#define DEBUG_XIE 1*/

static PRBool inited = PR_FALSE;
static XiePhotospace gPhotospace;
static XiePhotoElement *photoElement;

static void
DoFlo(Display *display,
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
      PRInt32 aDHeight)
{
  XieExtensionInfo *info;
  float coeffs[6];
  XieConstant constant;
  XiePhototag idx = 0, src;
  /*  static PRBool firsttime = PR_TRUE;
      static XiePhotomap pmap;
  */

  /* create the pretty flo graph */



  /* import */
  XieFloImportDrawable(&photoElement[idx], aSrc, aSX, aSY, aSWidth, aSHeight, 0, PR_FALSE);
  ++idx;
  src = idx;

  /* do the scaling stuff */
  coeffs[0] = (float)aSrcWidth / (float)aDWidth;
  coeffs[1] = 0.0;
  coeffs[2] = 0.0;
  coeffs[3] = (float)aSrcHeight / (float)aDHeight;
  coeffs[4] = 0.0;
  coeffs[5] = 0.0;

  constant[0] = 128.0;
  constant[1] = 128.0;
  constant[2] = 128.0;

  XieFloGeometry(&photoElement[idx], src, aDWidth, aDHeight,
                 coeffs,
                 constant,
                 0x07,
                 xieValGeomNearestNeighbor,
                 NULL);
  ++idx;

  /* export */
  XieFloExportDrawable(&photoElement[idx], idx, aDest, aGC,
                       (aDX - aSX),
                       (aDY - aSY));
  ++idx;


  /* do the scale thing baby */
  XieExecuteImmediate(display, gPhotospace, 1, PR_FALSE, photoElement, idx);


  /*
    XieFreePhotofloGraph(photoElement, 3);
  */

#ifdef DEBUG_XIE
  gdk_flush();
#endif
}




PRBool 
DrawScaledImageXIE(Display *display,
                   GdkDrawable *aDest,
                   GdkGC *aGC,
                   GdkDrawable *aSrc,
                   GdkDrawable *aSrcMask,
                   PRInt32 aSrcWidth,
                   PRInt32 aSrcHeight,
                   PRInt32 aSX,
                   PRInt32 aSY,
                   PRInt32 aSWidth,
                   PRInt32 aSHeight,
                   PRInt32 aDX,
                   PRInt32 aDY,
                   PRInt32 aDWidth,
                   PRInt32 aDHeight)
{
  Drawable importDrawable = GDK_WINDOW_XWINDOW(aSrc);
  Drawable exportDrawable = GDK_WINDOW_XWINDOW(aDest);

  GdkPixmap *alphaMask = NULL;

  GdkGC *gc = NULL;

#ifdef DEBUG_XIE
  printf("DrawScaledImageXIE\n");
#endif

  if (!inited) {
    XieExtensionInfo *info;
    inited = PR_TRUE;
    if (!XieInitialize(display, &info))
      return PR_FALSE;

    /* create the photospace (we only need to do this once) */
    gPhotospace = XieCreatePhotospace(display);

    photoElement = XieAllocatePhotofloGraph(3);

    /* we want to destroy this at shutdown
       XieDestroyPhotospace(display, photospace);
    */
  }

  if (aSrcMask) {
    Drawable destMask;

#ifdef DEBUG_XIE    
    fprintf(stderr, "DrawScaledImageXIE with alpha mask\n");
#endif

    alphaMask = gdk_pixmap_new(aSrcMask, aDWidth, aDHeight, 1);
    destMask = GDK_WINDOW_XWINDOW(alphaMask);
    gc = gdk_gc_new(alphaMask);
    DoFlo(display, destMask, GDK_GC_XGC(gc), GDK_WINDOW_XWINDOW(aSrcMask), aSrcWidth, aSrcHeight,
	  aSX, aSY, aSWidth, aSHeight,
	  aDX, aDY, aDWidth, aDHeight);
    gdk_gc_unref(gc);

    gc = gdk_gc_new(aDest);

    gdk_gc_copy(gc, aGC);
    gdk_gc_set_clip_mask(gc, alphaMask);
    gdk_gc_set_clip_origin(gc, aDX + aSX, aDY + aSY);
  }

  if (!gc) {
    gc = aGC;
    gdk_gc_ref(gc);
  }

  DoFlo(display, exportDrawable, GDK_GC_XGC(gc), importDrawable, aSrcWidth, aSrcHeight,
	aSX, aSY, aSWidth, aSHeight,
	aDX, aDY, aDWidth, aDHeight);

  if (alphaMask)
    gdk_pixmap_unref(alphaMask);

  gdk_gc_unref(gc);

  return PR_TRUE;
}

#endif
