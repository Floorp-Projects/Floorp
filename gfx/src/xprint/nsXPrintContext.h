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
 * Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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
#include "nsString.h"
#include "nsIImage.h"

#include "nsIDeviceContextSpecXPrint.h"

class nsXPrintContext
{
public:
  nsXPrintContext();
  virtual ~nsXPrintContext();
  
  NS_IMETHOD Init(nsIDeviceContextSpecXP *aSpec);
  NS_IMETHOD BeginPage();
  NS_IMETHOD EndPage();
  NS_IMETHOD BeginDocument(PRUnichar * aTitle);
  NS_IMETHOD EndDocument();
 
  GC         GetGC(void) { return mGC; }
  Drawable   GetDrawable(void) { return (mDrawable); }
  Screen *   GetScreen() { return mScreen; }
  Visual *   GetVisual() { return mVisual; }
  int        GetDepth() { return mDepth; }
  int        GetHeight() { return mHeight; }
  int        GetWidth() { return mWidth; }
  int        GetScreenNumber() { return XScreenNumberOfScreen(mScreen); }
  
  Display *  GetDisplay() { return mPDisplay; }
  NS_IMETHOD GetPrintResolution(int &aPrintResolution) const;
  NS_IMETHOD GetTextZoom(float &aTextZoom) const { aTextZoom = mTextZoom; return NS_OK; }

  NS_IMETHOD DrawImage(nsIImage *aImage,
                PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);

  NS_IMETHOD DrawImage(nsIImage *aImage,
                 PRInt32 aX, PRInt32 aY,
                 PRInt32 aWidth, PRInt32 aHeight);

  
  NS_IMETHOD SetForegroundColor(nscolor aColor); 
 
private:
  Display      *mPDisplay;
  Screen       *mScreen;
  Visual       *mVisual;
  GC            mGC;
  Drawable      mDrawable;
  XImage       *mImage;
  int           mDepth;
  int           mScreenNumber;
  Pixmap        mAlphaPixmap;
  Pixmap        mImagePixmap;
  int           mWidth;
  int           mHeight;
  XPContext     mPContext;
  PRBool        mIsAPrinter;
  char         *mPrintFile; /* file to "print" to */
  void         *mXpuPrintToFileHandle; /* handle for XpuPrintToFile/XpuWaitForPrintFileChild when printing to file */
  long          mPrintResolution;
  float         mTextZoom;

  NS_IMETHOD SetupWindow(int x, int y, int width, int height);
  NS_IMETHOD SetupPrintContext(nsIDeviceContextSpecXP *aSpec);
};


#endif /* !_XPCONTEXT_H_ */
