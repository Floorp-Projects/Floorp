/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIDrawingSurfaceQT_h___
#define nsIDrawingSurfaceQT_h___

#include "nsIDrawingSurface.h"

class QPainter;
class QPaintDevice;

// windows specific drawing surface method set

#define NS_IDRAWING_SURFACE_QT_IID   \
{ 0x1ed958b0, 0xcab6, 0x11d2, \
{ 0xa8, 0x49, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

class nsIDrawingSurfaceQT : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDRAWING_SURFACE_QT_IID)

    /**
     * Initialize a drawing surface using a QPainter and QPaintDevice.
     * these are "owned" by the drawing surface until the drawing
     * surface is destroyed.
     * @param  aGC QPainter to initialize drawing surface with
     * @param  aPaintDevice QPixmap to initialize drawing surface with
     * @return error status
     **/
    NS_IMETHOD Init(QPaintDevice *aPaintDevice, QPainter *aGC) = 0;

    /**
     * Initialize an offscreen drawing surface using a
     * QPainter. aGC is not "owned" by this drawing surface, instead
     * it is used to create a drawing surface compatible
     * with aGC. if width or height are less than zero, aGC will
     * be created with no offscreen bitmap installed.
     * @param  aGC QPainter to initialize drawing surface with
     * @param  aWidth width of drawing surface
     * @param  aHeight height of drawing surface
     * @param  aFlags flags used to control type of drawing
     *         surface created
     * @return error status
     **/
    NS_IMETHOD Init(QPainter * aGC, 
                    PRUint32 aWidth, 
                    PRUint32 aHeight,
                    PRUint32 aFlags) = 0;
};

#endif  // nsIDrawingSurfaceQT_h___ 
