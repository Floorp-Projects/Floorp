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

#ifndef nsIDrawingSurfaceGTK_h___
#define nsIDrawingSurfaceGTK_h___

#include "nsIDrawingSurface.h"
#include <gtk/gtk.h>

// windows specific drawing surface method set

#define NS_IDRAWING_SURFACE_GTK_IID   \
{ 0x1ed958b0, 0xcab6, 0x11d2, \
{ 0xa8, 0x49, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

class nsIDrawingSurfaceGTK : public nsISupports
{
public:
  /**
   * Initialize a drawing surface using a windows DC.
   * aDC is "owned" by the drawing surface until the drawing
   * surface is destroyed.
   * @param  aDC HDC to initialize drawing surface with
   * @return error status
   **/
  NS_IMETHOD Init(GdkDrawable *aDrawable, GdkGC *aGC) = 0;

  /**
   * Initialize an offscreen drawing surface using a
   * windows DC. aDC is not "owned" by this drawing surface, instead
   * it is used to create a drawing surface compatible
   * with aDC. if width or height are less than zero, aDC will
   * be created with no offscreen bitmap installed.
   * @param  aDC HDC to initialize drawing surface with
   * @param  aWidth width of drawing surface
   * @param  aHeight height of drawing surface
   * @param  aFlags flags used to control type of drawing
   *         surface created
   * @return error status
   **/
  NS_IMETHOD Init(GdkGC *aGC, PRUint32 aWidth, PRUint32 aHeight,
                  PRUint32 aFlags) = 0;

  /**
   * Get a windows DC that represents the drawing surface.
   * GetDC() must be paired with ReleaseDC(). Getting a DC
   * and Lock()ing are mutually exclusive operations.
   * @param  aDC out parameter for HDC
   * @return error status
   **/
  NS_IMETHOD GetGC(GdkGC *aGC) = 0;

  /**
   * Release a windows DC obtained by GetDC().
   * ReleaseDC() must be preceded by a call to ReleaseDC().
   * @return error status
   **/
  NS_IMETHOD ReleaseGC(void) = 0;

};

#endif  // nsIDrawingSurfaceGTK_h___ 
