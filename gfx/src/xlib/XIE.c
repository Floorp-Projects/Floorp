/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:ts=2:et:sw=2:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Syd Logan <syd@netscape.com>
 *   Xlibified by tim copperfield <timecop@japan.co.jp>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "drawers.h"

#ifdef HAVE_XIE

#include <stdio.h>
#include <string.h>
#include "prenv.h"
#include "xlibrgb.h"
#include <X11/extensions/XIElib.h>

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
#ifdef DEBUG_XIE
  printf("export to %d, %d (%dx%d)\n", (aDX - aSX), (aDY - aSY),
      aDWidth, aDHeight);
#endif
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

  /* run the flo on the image to get a the scaled image */
  DoFlo(display, aDest, aGC, aSrc,
        aSrcWidth, aSrcHeight,
        aSX, aSY, aSWidth, aSHeight,
        aDX, aDY, aDWidth, aDHeight);

  return PR_TRUE;
}
#endif
