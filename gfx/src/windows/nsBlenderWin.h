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

#ifndef nsBlenderWin_h___
#define nsBlenderWin_h___

#include "nsIBlender.h"
#include "nsPoint.h"
#include "nsRect.h"

//----------------------------------------------------------------------

// Blender interface
class nsBlenderWin : public nsIBlender
{
public:

    NS_DECL_ISUPPORTS
  
      nsBlenderWin();
      ~nsBlenderWin();

  /**
   * Initialize the Blender
   * @result The result of the initialization, NS_OK if no errors
   */
  virtual nsresult Init();

  virtual void Blend(nsDrawingSurface aSrc,
                     PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight,
                     nsDrawingSurface aDest, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity);

  PRBool CalcAlphaMetrics(BITMAP *aSrcInfo,BITMAP *aDestInfo,nsPoint *ASrcUL,
                              BITMAP  *aMapInfo,nsPoint *aMaskUL,
                              PRInt32 *aNumlines,
                              PRInt32 *aNumbytes,PRUint8 **aSImage,PRUint8 **aDImage,
                              PRUint8 **aMImage,PRInt32 *aSLSpan,PRInt32 *aDLSpan,PRInt32 *aMLSpan);

  PRInt32  CalcBytesSpan(PRUint32  aWidth,PRUint32  aBitsPixel);

private:
  nsresult BuildDIB(LPBITMAPINFOHEADER  *aBHead,unsigned char **aBits,PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth);

};

#endif