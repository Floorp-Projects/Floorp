/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

 
#ifndef _XPCONTEXT_H_
#define _XPCONTEXT_H_

#include <X11/Xlib.h>
#include <X11/extensions/Print.h>
#include "xp_core.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsIImage.h"
#include "nsGCCache.h"
#include "nsIDeviceContextSpecXPrint.h"
#include "nsDrawingSurfaceXlib.h"
#include "xlibrgb.h"

class nsDeviceContextXp;

class nsXPrintContext : public nsIDrawingSurfaceXlib
{
public:
  nsXPrintContext();
  virtual ~nsXPrintContext();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Lock(PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
                  void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
                  PRUint32 aFlags)  { return NS_OK; };
  NS_IMETHOD Unlock(void)  { return NS_OK; };
  NS_IMETHOD GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight)  { return NS_OK; };
  NS_IMETHOD IsOffscreen(PRBool *aOffScreen) { return NS_OK; };
  NS_IMETHOD IsPixelAddressable(PRBool *aAddressable) { return NS_OK; };
  NS_IMETHOD GetPixelFormat(nsPixelFormat *aFormat) { return NS_OK; };
 
  NS_IMETHOD Init(nsDeviceContextXp *dc, nsIDeviceContextSpecXp *aSpec);
  NS_IMETHOD BeginPage();
  NS_IMETHOD EndPage();
  NS_IMETHOD BeginDocument(PRUnichar *aTitle);
  NS_IMETHOD EndDocument();
 

  int                     GetHeight() { return mHeight; }
  int                     GetWidth() { return mWidth; }
  NS_IMETHOD GetDrawable(Drawable &aDrawable) { aDrawable = mDrawable; return NS_OK; }
  NS_IMETHOD GetXlibRgbHandle(XlibRgbHandle *&aHandle) { aHandle = mXlibRgbHandle; return NS_OK; }
  NS_IMETHOD GetGC(xGC *&aXGC) { mGC->AddRef(); aXGC = mGC; return NS_OK; }
  
  void                    SetGC(xGC *aGC) { mGC = aGC; mGC->AddRef(); }

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
  xGC          *mGC;
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

