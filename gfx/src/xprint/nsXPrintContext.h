/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
 */

 
#ifndef _XPCONTEXT_H_
#define _XPCONTEXT_H_

#include <X11/Xlib.h>
#include <X11/extensions/Print.h>
#include "xp_core.h"
#include "xp_file.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsIImage.h"
#include "nsGCCache.h"
#include "nsIDeviceContextSpecXPrint.h"
#include "xlibrgb.h"

class nsDeviceContextXp;

class nsXPrintContext
{
public:
  nsXPrintContext();
  virtual ~nsXPrintContext();
  
  NS_IMETHOD Init(nsDeviceContextXp *dc, nsIDeviceContextSpecXp *aSpec);
  NS_IMETHOD BeginPage();
  NS_IMETHOD EndPage();
  NS_IMETHOD BeginDocument(PRUnichar *aTitle);
  NS_IMETHOD EndDocument();
 
  Drawable   GetDrawable() { return (mDrawable); }
  Screen *   GetScreen() { return mScreen; }
  Visual *   GetVisual() { return mVisual; }
  XlibRgbHandle *GetXlibRgbHandle() { return mXlibRgbHandle; }
  int        GetDepth() { return mDepth; }
  int        GetHeight() { return mHeight; }
  int        GetWidth() { return mWidth; }
  
  Display *  GetDisplay() { return mPDisplay; }
  NS_IMETHOD GetPrintResolution(int &aPrintResolution);

  NS_IMETHOD DrawImage(xGC *gc, nsIImage *aImage,
                PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);

  NS_IMETHOD DrawImage(xGC *gc, nsIImage *aImage,
                 PRInt32 aX, PRInt32 aY,
                 PRInt32 aWidth, PRInt32 aHeight);

private:
  nsresult DrawImageBitsScaled(xGC *gc, nsIImage *aImage,
                PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);
                
  nsresult DrawImageBits(xGC *gc, 
                         PRUint8 *alphaBits, PRInt32  alphaRowBytes, PRUint8 alphaDepth,
                         PRUint8 *image_bits, PRInt32  row_bytes,
                         PRInt32 aX, PRInt32 aY,
                         PRInt32 aWidth, PRInt32 aHeight); 

  XlibRgbHandle *mXlibRgbHandle;
  Display      *mPDisplay;
  Screen       *mScreen;
  Visual       *mVisual;
  Drawable      mDrawable; /* window */
  int           mDepth;
  int           mScreenNumber;
  int           mWidth;
  int           mHeight;
  XPContext     mPContext;
  PRBool        mIsGrayscale; /* color or grayscale ? */
  PRBool        mIsAPrinter;  /* destination: printer or file ? */
  char         *mPrintFile;   /* file to "print" to */
  void         *mXpuPrintToFileHandle; /* handle for XpuPrintToFile/XpuWaitForPrintFileChild when printing to file */
  long          mPrintResolution;
  nsDeviceContextXp *mContext; /* DeviceContext which created this object */

  nsresult SetupWindow(int x, int y, int width, int height);
  nsresult SetupPrintContext(nsIDeviceContextSpecXp *aSpec);
};


#endif /* !_XPCONTEXT_H_ */

