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

#ifndef nsBlender_h___
#define nsBlender_h___

#include "nsIBlender.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsIImage.h"
#include "libimg.h"


typedef enum{
  nsLowQual=0,
  nsLowMedQual,
  nsMedQual,
  nsHighMedQual,
  nsHighQual
}nsBlendQuality;

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

//----------------------------------------------------------------------

// Blender interface
class nsBlender : public nsIBlender
{
public:

  NS_DECL_ISUPPORTS
  
 /** --------------------------------------------------------------------------
  * General constructor for a nsBlender object
  * @update dc - 10/29/98
  */
  nsBlender();

 /** --------------------------------------------------------------------------
  * Destructor for a nsBlender object
  * @update dwc - 10/29/98
  */
  virtual ~nsBlender();

  NS_IMETHOD Init(nsIDeviceContext *aDeviceContext);

 protected:

  /** --------------------------------------------------------------------------
   * Calculate how many bytes per span for a given depth
   * @update dwc - 10/29/98
   * @param aWidth -- width of the line
   * @param aBitsPixel -- how many bytes per pixel in the bitmap
   * @result The number of bytes per line
   */
  PRInt32  CalcBytesSpan(PRUint32  aWidth,PRUint32  aBitsPixel);

  /** --------------------------------------------------------------------------
   * Blend two 32 bit image arrays
   * @update dwc - 10/29/98
   * @param aNumlines  Number of lines to blend
   * @param aNumberBytes Number of bytes per line to blend
   * @param aSImage Pointer to beginning of the source bytes
   * @param aDImage Pointer to beginning of the destination bytes
   * @param aMImage Pointer to beginning of the mask bytes
   * @param aSLSpan number of bytes per line for the source bytes
   * @param aDLSpan number of bytes per line for the destination bytes
   * @param aMLSpan number of bytes per line for the Mask bytes
   * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
   * @param aPixelFormat nsPixelFormat struct filled out to describe data format
   */
  void Do32Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,
                 PRUint8 *aSecondSImage,PRInt32 aSLSpan,PRInt32 aDLSpan,nsBlendQuality aTheQual,
                 nscolor aSrcBackColor, nscolor aSecondSrcBackColor, nsPixelFormat &aPixelFormat);

  /** --------------------------------------------------------------------------
   * Blend two 24 bit image arrays using an 8 bit alpha mask
   * @update dwc - 10/29/98
   * @param aNumlines  Number of lines to blend
   * @param aNumberBytes Number of bytes per line to blend
   * @param aSImage Pointer to beginning of the source bytes
   * @param aDImage Pointer to beginning of the destination bytes
   * @param aMImage Pointer to beginning of the mask bytes
   * @param aSLSpan number of bytes per line for the source bytes
   * @param aDLSpan number of bytes per line for the destination bytes
   * @param aMLSpan number of bytes per line for the Mask bytes
   * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
   */
  void Do24BlendWithMask(PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,
                PRUint8 *aMImage,PRInt32 aSLSpan,PRInt32 aDLSpan,PRInt32 aMLSpan,nsBlendQuality aBlendQuality);

 /** --------------------------------------------------------------------------
  * Blend two 24 bit image arrays using a passed in blend value
  * @update dwc - 10/29/98
  * @param aNumlines  Number of lines to blend
  * @param aNumberBytes Number of bytes per line to blend
  * @param aSImage Pointer to beginning of the source bytes
  * @param aDImage Pointer to beginning of the destination bytes
  * @param aMImage Pointer to beginning of the mask bytes
  * @param aSLSpan number of bytes per line for the source bytes
  * @param aDLSpan number of bytes per line for the destination bytes
  * @param aMLSpan number of bytes per line for the Mask bytes
  * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
  */
  void Do24Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,
                 PRUint8 *aSecondSImage,PRInt32 aSLSpan,PRInt32 aDLSpan,nsBlendQuality aBlendQuality,
                 nscolor aSrcBackColor, nscolor aSecondSrcBackColor, nsPixelFormat &aPixelFormat);


 /** --------------------------------------------------------------------------
  * Blend two 16 bit image arrays using a passed in blend value
  * @update dwc - 10/29/98
  * @param aNumlines  Number of lines to blend
  * @param aNumberBytes Number of bytes per line to blend
  * @param aSImage Pointer to beginning of the source bytes
  * @param aDImage Pointer to beginning of the destination bytes
  * @param aMImage Pointer to beginning of the mask bytes
  * @param aSLSpan number of bytes per line for the source bytes
  * @param aDLSpan number of bytes per line for the destination bytes
  * @param aMLSpan number of bytes per line for the Mask bytes
  * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
  */
  void Do16Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,
                 PRUint8 *aSecondSImage,PRInt32 aSLSpan,PRInt32 aDLSpan,nsBlendQuality aBlendQuality,
                 nscolor aSrcBackColor, nscolor aSecondSrcBackColor, nsPixelFormat &aPixelFormat);

 /** --------------------------------------------------------------------------
  * Blend two 8 bit image arrays using an 8 bit alpha mask
  * @update dwc - 10/29/98
  * @param aNumlines  Number of lines to blend
  * @param aNumberBytes Number of bytes per line to blend
  * @param aSImage Pointer to beginning of the source bytes
  * @param aDImage Pointer to beginning of the destination bytes
  * @param aMImage Pointer to beginning of the mask bytes
  * @param aSLSpan number of bytes per line for the source bytes
  * @param aDLSpan number of bytes per line for the destination bytes
  * @param aMLSpan number of bytes per line for the Mask bytes
  * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
  */
  void Do8BlendWithMask(PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,
                PRUint8 *aMImage,PRInt32 aSLSpan,PRInt32 aDLSpan,PRInt32 aMLSpan,nsBlendQuality aBlendQuality);

 /** --------------------------------------------------------------------------
  * @update dwc - 10/29/98
  * Blend two 8 bit image arrays using a passed in blend value
  * @param aNumlines  Number of lines to blend
  * @param aNumberBytes Number of bytes per line to blend
  * @param aSImage Pointer to beginning of the source bytes
  * @param aDImage Pointer to beginning of the destination bytes
  * @param aMImage Pointer to beginning of the mask bytes
  * @param aSLSpan number of bytes per line for the source bytes
  * @param aDLSpan number of bytes per line for the destination bytes
  * @param aMLSpan number of bytes per line for the Mask bytes
  * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
  */
  void Do8Blend(PRUint8 aBlendVal,PRInt32 aNumlines,PRInt32 aNumbytes,PRUint8 *aSImage,PRUint8 *aDImage,
                PRUint8 *aSecondSImage,PRInt32 aSLSpan,PRInt32 aDLSpan,IL_ColorSpace *aColorMap,nsBlendQuality aBlendQuality,
                nscolor aSrcBackColor, nscolor aSecondSrcBackColor);


  nsIDeviceContext    *mTheDeviceCon;
};

#endif