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

#ifndef nsIImage_h___
#define nsIImage_h___

#include "nsISupports.h"
#include "nsRect.h"
#include "nsIRenderingContext.h"

typedef struct
{
  //I lifted this from the image lib. The difference is that
  //this uses nscolor instead of NI_RGB. Multiple color pollution
  //is a bad thing. MMP
  PRInt32 NumColors;  // Number of colors in the colormap.
                      // A negative value can be used to denote a
                      // possibly non-unique set.
  //nscolor *Map;       // Colormap colors.
  PRUint8 *Index;     // NULL, if map is in index order, otherwise
                      // specifies the indices of the map entries. */
} nsColorMap;

typedef enum {
    nsMaskRequirements_kNoMask,
    nsMaskRequirements_kNeeds1Bit,
    nsMaskRequirements_kNeeds8Bit
} nsMaskRequirements;


#define  nsImageUpdateFlags_kColorMapChanged 0x1
#define  nsImageUpdateFlags_kBitsChanged     0x2
 
// IID for the nsIImage interface
#define NS_IIMAGE_IID          \
{ 0x0b4faaa0, 0xaa3a, 0x11d1, \
{ 0xa8, 0x24, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

// Interface to Images
class nsIImage : public nsISupports
{
public:
  //initialize the image. aDepth is either 8 or 24. if the image has an alpha
  //channel, aNeedAlpha will be true
  // had nsqresult for return, need to fix                                                fix
  virtual nsresult Init(PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth, nsMaskRequirements aMaskRequirements) = 0;

  //dimensioning in PIXELS
  virtual PRInt32 GetWidth() = 0;
  virtual PRInt32 GetHeight() = 0;

  //if the image is not optimzed, get a pointer to the bits
  virtual PRUint8 * GetBits() = 0;

  //get the number of bytes to jump from scanline to scanline
  virtual PRInt32 GetLineStride() = 0;

  //if the image is not optimzed, get a pointer to the bits
  virtual PRUint8 * GetAlphaBits() = 0;

  //get the number of bytes to jump from scanline to scanline
  virtual PRInt32 GetAlphaLineStride() = 0;

  virtual void ImageUpdated(PRUint8 aFlags, nsRect *aUpdateRect) = 0;

  //has this image been optimized?
  virtual PRBool IsOptimized() = 0;

  //convert this image to a version optimized for display
  virtual nsresult Optimize(nsDrawingSurface aSurface) = 0;

  //if this returns non-null, this image is color mapped
  virtual nsColorMap * GetColorMap() = 0;

  virtual PRBool Draw(nsDrawingSurface aSurface, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight) = 0;
  virtual PRBool Draw(nsDrawingSurface aSurface, PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                                  PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight) = 0;
  
  //get the color space metrics for this image
  //virtual NI_ColorSpec * GetColorSpec() = 0;                       fix

  //get the color which should be considered transparent.
  //if this image is color mapped, this value will be an
  //index into the color map. hokey? yes, but it avoids
  //another silly api or struct.
  //virtual nscolor GetTransparentColor() = 0;                              fix.
};

//change notification API flag bits
#define NS_IMAGE_UPDATE_COLORMAP  1
#define NS_IMAGE_UPDATE_PIXELS    2
#define NS_IMAGE_UPDATE_ALPHA     4

#endif
