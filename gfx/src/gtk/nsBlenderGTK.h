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

#ifndef nsBlenderGTK_h___
#define nsBlenderGTK_h___

#include "nsBlender.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsIImage.h"
#include "nsRenderingContextGTK.h"

//----------------------------------------------------------------------

// Blender interface
class nsBlenderGTK : public nsBlender
{
public:
  
  /**
   * Construct and set the initial values for this windows specific blender
   * @update dc - 10/29/98
   */
  nsBlenderGTK();

  NS_IMETHOD Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight,nsDrawingSurface aSrc,
                   nsDrawingSurface aDest, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                   nsDrawingSurface aSecondSrc = nsnull, nscolor aSrcBackColor = NS_RGB(0, 0, 0),
                   nscolor aSecondSrcBackColor = NS_RGB(0, 0, 0));

  NS_IMETHOD Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight, nsIRenderingContext *aSrc,
                   nsIRenderingContext *aDest, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                   nsIRenderingContext *aSecondSrc = nsnull, nscolor aSrcBackColor = NS_RGB(0, 0, 0),
                   nscolor aSecondSrcBackColor = NS_RGB(0, 0, 0));

protected:
  /**
   * Release and cleanup all the windows specific information for this blender
   * @update dc - 10/29/98
   */
  ~nsBlenderGTK();

  nsresult Blend(PRUint8 *aSrcBits, PRInt32 aSrcStride, PRInt32 aSrcBytes,
                 PRUint8 *aDestBits, PRInt32 aDestStride, PRInt32 aDestBytes,
                 PRUint8 *aSecondSrcBits, PRInt32 aSecondSrcStride, PRInt32 aSecondSrcBytes,
                 PRInt32 aLines, PRInt32 aAlpha, nsPixelFormat &aPixFormat,
                 nscolor aSrcBackColor, nscolor aSecondSrcBackColor);

 private:
#if 0
  /** --------------------------------------------------------------------------
   * Calculate the metrics for the alpha layer before the blend
   * @update dc - 10/29/98
   * @param aSrcInfo -- a pointer to a source bitmap
   * @param aDestInfo -- a pointer to the destination bitmap
   * @param aSrcUL -- upperleft for the source blend
   * @param aMaskInfo -- a pointer to the mask bitmap
   * @param aMaskUL -- upperleft for the mask bitmap
   * @param aWidth -- width of the blend
   * @param aHeight -- heigth of the blend
   * @param aNumLines -- a pointer to number of lines to do for the blend
   * @param aNumbytes -- a pointer to the number of bytes per line for the blend
   * @param aSImage -- a pointer to a the bit pointer for the source
   * @param aDImage -- a pointer to a the bit pointer for the destination 
   * @param aMImage -- a pointer to a the bit pointer for the mask 
   * @param aSLSpan -- number of bytes per span for the source
   * @param aDLSpan -- number of bytes per span for the destination
   * @param aMLSpan -- number of bytes per span for the mask
   * @result PR_TRUE if calculation was succesful
   */
  PRBool CalcAlphaMetrics(BITMAP *aSrcInfo,BITMAP *aDestInfo,
                          BITMAP *aSecondSrcInfo, nsPoint *ASrcUL,
                          BITMAP  *aMapInfo,nsPoint *aMaskUL,
                          PRInt32 aWidth,PRInt32 aHeight,
                          PRInt32 *aNumlines,
                          PRInt32 *aNumbytes,PRUint8 **aSImage,PRUint8 **aDImage,
                          PRUint8 **aSecondSImage,
                          PRUint8 **aMImage,PRInt32 *aSLSpan,PRInt32 *aDLSpan,PRInt32 *aMLSpan);
#endif
private:
  PRUint8             *mSrcBytes;
  PRUint8             *mSecondSrcBytes;
  PRUint8             *mDestBytes;

  PRInt32             mSrcRowBytes;
  PRInt32             mSecondSrcRowBytes;
  PRInt32             mDestRowBytes;

  PRInt32             mSrcSpan;
  PRInt32             mSecondSrcSpan;
  PRInt32             mDestSpan;
};

#endif
