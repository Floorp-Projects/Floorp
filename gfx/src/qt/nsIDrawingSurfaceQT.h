/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *		John C. Griggs <johng@corel.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    NS_IMETHOD Init(QPainter * aGC,PRUint32 aWidth,PRUint32 aHeight,
                    PRUint32 aFlags) = 0;
};

#endif  // nsIDrawingSurfaceQT_h___ 
