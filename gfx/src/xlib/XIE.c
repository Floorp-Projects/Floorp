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
 *   Syd Logan <syd@netscape.com>
 *   Xlibified by tim copperfield <timecop@japan.co.jp>
 */

#include "drawers.h"

#ifdef HAVE_XIE

#include <stdio.h>
#include <string.h>
#include <X11/extensions/XIElib.h>
#include "prenv.h"
/* why do I need this define? */
#define USE_MOZILLA_TYPES
#include "xlibrgb.h"

/* #define DEBUG_XIE 1 */
static PRBool useXIE = PR_TRUE;
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

  /* create the pretty flo graph */


  /* import */
  XieFloImportDrawable(&photoElement[idx], aSrc,
                       aSX, aSY, aSWidth, aSHeight,
                       0, PR_FALSE);
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
}

PRBool 
DrawScaledImageXIE(Display *display,
                   Drawable aDest,
                   GC aGC,
                   Drawable aSrc,
                   Drawable aSrcMask,
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
  Pixmap alphaMask = 0;
  GC gc = 0;

#ifdef DEBUG_XIE
  printf("DrawScaledImageXIE\n");
#endif

  if (!useXIE) {
#ifdef DEBUG_XIE
    fprintf(stderr, "useXIE is false.\n");
#endif
    return PR_FALSE;
  }

  if (!inited) {
    XieExtensionInfo *info;

    if (useXIE) {
      char *text = PR_GetEnv("MOZ_DISABLE_XIE");
      if (text) {
#ifdef DEBUG_XIE
        fprintf(stderr, "MOZ_DISABLE_XIE set, disabling use of XIE.\n");
#endif
        useXIE = PR_FALSE;
        return PR_FALSE;
      }
    }

    if (!XieInitialize(display, &info)) {
      useXIE = PR_FALSE;
      return PR_FALSE;
    }

    inited = PR_TRUE;

    /* create the photospace (we only need to do this once) */
    gPhotospace = XieCreatePhotospace(display);

    photoElement = XieAllocatePhotofloGraph(3);

    /* XXX we want to destroy this at shutdown
       XieDestroyPhotospace(display, photospace);
    */
  }

  if (aSrcMask) {
    GC tmpgc;
#ifdef DEBUG_XIE
    fprintf(stderr, "DrawScaledImageXIE with alpha mask\n");
#endif
    alphaMask = XCreatePixmap(display,
                              DefaultRootWindow(xlib_rgb_get_display()),
                              aDWidth, aDHeight, 1);
    
    /* we need to make a worthless depth 1 GC to do the flow
     * because GC's can only be used with the same depth drawables */
    tmpgc = XCreateGC(display, alphaMask, 0, NULL);

    /* run the flo on the alpha mask to get a scaled alpha mask */
    DoFlo(display, alphaMask, tmpgc, aSrcMask,
          aSrcWidth, aSrcHeight,
          aSX, aSY, aSWidth, aSHeight,
          aDX, aDY, aDWidth, aDHeight);

    /* get rid of the worthless depth 1 gc */
    XFreeGC(display, tmpgc);

    /* the real GC needs to be created on aDest since that's the depth
     * we are targeting */
    gc = XCreateGC(display, aDest, 0, NULL);
    XCopyGC(display, aGC, GCFunction, gc);

    /* there is a bug here. The image won't display unless it's at the
     * very top of the page. Something to do with clip mask offset, though
     * I can't really figure it out. If this is a bug, it should be present
     * in the GTK version as well. */

    /* set clip mask + clip origin */
    XSetClipMask(display, gc, alphaMask);
    XSetClipOrigin(display, gc, aDX + aSX, aDY + aSY);
  }
  
  /* run the flo on the image to get a the scaled image
   * we can't destroy the GC from gc cache, so we do the ? thing,
   * gc = our copied GC, aGC = given GC */
  DoFlo(display, aDest, gc ? gc : aGC, aSrc,
        aSrcWidth, aSrcHeight,
        aSX, aSY, aSWidth, aSHeight,
        aDX, aDY, aDWidth, aDHeight);

  if (gc)
    XFreeGC(display, gc);
  if (alphaMask)
    XFreePixmap(display, alphaMask);

  return PR_TRUE;
}
#endif
