/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIDrawingSurface_h___
#define nsIDrawingSurface_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsRect.h"

// a memory area that can be rendered to

typedef struct
{
  PRUint32  mRedZeroMask;     //red color mask in zero position
  PRUint32  mGreenZeroMask;   //green color mask in zero position
  PRUint32  mBlueZeroMask;    //blue color mask in zero position
  PRUint32  mAlphaZeroMask;   //alpha data mask in zero position
  PRUint32  mRedMask;         //red color mask
  PRUint32  mGreenMask;       //green color mask
  PRUint32  mBlueMask;        //blue color mask
  PRUint32  mAlphaMask;       //alpha data mask
  PRUint8   mRedCount;        //number of red color bits
  PRUint8   mGreenCount;      //number of green color bits
  PRUint8   mBlueCount;       //number of blue color bits
  PRUint8   mAlphaCount;      //number of alpha data bits
  PRUint8   mRedShift;        //number to shift value into red position
  PRUint8   mGreenShift;      //number to shift value into green position
  PRUint8   mBlueShift;       //number to shift value into blue position
  PRUint8   mAlphaShift;      //number to shift value into alpha position
} nsPixelFormat;

#define RASWIDTH(width, bpp) ((((width) * (bpp) + 31) >> 5) << 2)

#define NS_IDRAWING_SURFACE_IID   \
{ 0x61cc77e0, 0xcaac, 0x11d2, \
{ 0xa8, 0x49, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

class nsIDrawingSurface : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDRAWING_SURFACE_IID)
  /**
   * Lock a rect of a drawing surface and return a
   * pointer to the upper left hand corner of the
   * bitmap.
   * @param  aX x position of subrect of bitmap
   * @param  aY y position of subrect of bitmap
   * @param  aWidth width of subrect of bitmap
   * @param  aHeight height of subrect of bitmap
   * @param  aBits out parameter for upper left hand
   *         corner of bitmap
   * @param  aStride out parameter for number of bytes
   *         to add to aBits to go from scanline to scanline
   * @param  aWidthBytes out parameter for number of
   *         bytes per line in aBits to process aWidth pixels
   * @return error status
   *
   **/
  NS_IMETHOD Lock(PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
                  void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
                  PRUint32 aFlags) = 0;

  /**
   * Unlock a rect of a drawing surface. must be preceded
   * by a call to Lock(). Lock()/Unlock() pairs do not nest.
   * @return error status
   *
   **/
  NS_IMETHOD Unlock(void) = 0;

  /**
   * Get the dimensions of a drawing surface
   * @param  aWidth out parameter for width of drawing surface
   * @param  aHeight out parameter for height of drawing surface
   * @return error status
   *
   **/
  NS_IMETHOD GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight) = 0;

  /**
   * Get the offscreen status of the drawing surface
   * @param  aOffscreen out parameter for offscreen status of
   *         drawing surface. if PR_TRUE, then modifying the
   *         drawing surface does not immediately reflect the
   *         changes on the output device
   * @return error status
   *
   **/
  NS_IMETHOD IsOffscreen(PRBool *aOffScreen) = 0;

  /**
   * Get the pixel addressability status of the drawing surface
   * @param  aAddressable out parameter for pixel addressability
   *         status of drawing surface. if PR_TRUE, then the
   *         drawing surface is optimized for pixel addressability
   *         (i.e. the Lock() method has very low overhead). All
   *         drawing surfaces support Lock()ing, but doing so on
   *         drawing surfaces that do not return PR_TRUE here may
   *         impose significant overhead.
   * @return error status
   *
   **/
  NS_IMETHOD IsPixelAddressable(PRBool *aAddressable) = 0;

  /**
   * Get the pixel format of a drawing surface
   * @param  aOffscreen out parameter filled in with pixel
   *         format information.
   * @return error status
   *
   **/
  NS_IMETHOD GetPixelFormat(nsPixelFormat *aFormat) = 0;
};

//when creating a drawing surface, you can use this
//to tell the drawing surface that you anticipate
//the need to get to the actual bits of the drawing
//surface at some point during it's lifetime. typically
//used when creating bitmaps to be operated on by the
//nsIBlender implementations.
#define NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS  0x0001

//flag to say that this drawing surface is shortlived,
//which may affect how the OS allocates it. Used for
//tiling, grouting etc.
#define NS_CREATEDRAWINGSURFACE_SHORTLIVED        0x0002

//when locking a drawing surface, use these flags to
//control how the data in the surface should be accessed
#define NS_LOCK_SURFACE_READ_ONLY       0x0001
#define NS_LOCK_SURFACE_WRITE_ONLY      0x0002

#endif  // nsIDrawingSurface_h___ 
