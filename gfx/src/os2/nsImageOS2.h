/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifndef _nsImageOS2_h_
#define _nsImageOS2_h_

#include "nsIImage.h"

struct nsDrawingSurfaceOS2;

class nsImageOS2 : public nsIImage
{
 public:
   nsImageOS2();
   virtual ~nsImageOS2();

   NS_DECL_ISUPPORTS

   nsresult Init( PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth,
                  nsMaskRequirements aMaskRequirements);
   PRInt32  GetWidth()        { return mInfo ? mInfo->cx : 0; }
   PRInt32  GetHeight()       { return mInfo ? mInfo->cy : 0; }
   PRInt32  GetLineStride()   { return mStride; }
   PRUint8 *GetBits()         { return mImageBits; }
   void    *GetBitInfo()      { return mInfo; }
   nsColorMap *GetColorMap()  { return mColorMap; }
   PRInt32  GetBytesPix()     { return mInfo ? mInfo->cBitCount : 0; }

   PRBool   GetIsRowOrderTopToBottom() { return PR_FALSE; }

   // These may require more sensible returns...
   PRInt32  GetAlphaWidth()      { return mInfo ? mInfo->cx : 0; }
   PRInt32  GetAlphaHeight()     { return mInfo ? mInfo->cy : 0; }
   PRInt32  GetAlphaLineStride() { return mAStride; }
   PRBool   GetHasAlphaMask()     { return mAImageBits != nsnull; }        
   PRUint8 *GetAlphaBits()       { return mAImageBits; }

   void    SetAlphaLevel(PRInt32 aAlphaLevel)      {}
   PRInt32 GetAlphaLevel()                         { return 0; }

   nsresult Optimize( nsIDeviceContext* aContext);
   PRBool   IsOptimized() { return mOptimized; }

   NS_IMETHOD Draw( nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                    PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
   NS_IMETHOD Draw( nsIRenderingContext &aContext, nsDrawingSurface aSurface,
                    PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                    PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);

   void ImageUpdated( nsIDeviceContext *aContext,
                      PRUint8 aFlags, nsRect *aUpdateRect);

  NS_IMETHOD   LockImagePixels(PRBool aMaskPixels);
  NS_IMETHOD   UnlockImagePixels(PRBool aMaskPixels);    

 private:
   BITMAPINFO2 *mInfo;
   PRInt32      mStride;
   PRInt32      mAStride;
   PRUint8     *mImageBits;
   PRUint8     *mAImageBits;
   nsColorMap  *mColorMap;
   HBITMAP      mBitmap;
   HBITMAP      mABitmap;
   PRBool       mOptimized;
   PRInt32      mAlphaDepth;
   PRUint32     mDeviceDepth;

   void Cleanup();
   void CreateBitmaps( nsDrawingSurfaceOS2 *surf);
   void DrawBitmap( HPS hps, LONG cPts, PPOINTL pPts, LONG lRop, PRBool bMsk);
};

#endif
