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

#ifndef nsImageXlib_h__
#define nsImageXlib_h__

#include "nsIImage.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

// these are defined in the Xlib widget classes but I need access
// to them here.  fun without an external library.

extern Display         *gDisplay;
extern Screen          *gScreen;
extern int              gScreenNum;
extern int              gDepth;
extern Visual          *gVisual;

class nsImageXlib : public nsIImage {
public:
  nsImageXlib();
  virtual ~nsImageXlib();

  NS_DECL_ISUPPORTS

  virtual PRInt32     GetBytesPix() { return mNumBytesPixel; }
  virtual PRInt32     GetHeight();
  virtual PRInt32     GetWidth();
  virtual PRUint8*    GetBits();
  virtual PRInt32     GetLineStride();
  NS_IMETHOD          Draw(nsIRenderingContext &aContext,
                           nsDrawingSurface aSurface,
                           PRInt32 aX, PRInt32 aY,
                           PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD          Draw(nsIRenderingContext &aContext,
                           nsDrawingSurface aSurface,
                           PRInt32 aSX, PRInt32 aSY,
                           PRInt32 aSWidth, PRInt32 aSHeight,
                           PRInt32 aDX, PRInt32 aDY,
                           PRInt32 aDWidth, PRInt32 aDHeight);
  virtual nsColorMap* GetColorMap();
  virtual void        ImageUpdated(nsIDeviceContext *aContext,
                                   PRUint8 aFlags, nsRect *aUpdateRect);
  virtual nsresult    Init(PRInt32 aWidth, PRInt32 aHeight,
                           PRInt32 aDepth, nsMaskRequirements aMaskRequirements);
  virtual PRBool      IsOptimized();
  virtual nsresult    Optimize(nsIDeviceContext* aContext);
  virtual PRUint8*    GetAlphaBits();
  virtual PRInt32     GetAlphaWidth();
  virtual PRInt32     GetAlphaHeight();
  virtual PRInt32     GetAlphaLineStride();

  PRIntn              GetSizeImage();
  void                ComputeMetrics();
  PRInt32             CalcBytesSpan(PRUint32  aWidth);

  virtual void        SetAlphaLevel(PRInt32 aAlphaLevel);

  virtual PRInt32     GetAlphaLevel();

  void*               GetBitInfo();
  virtual PRBool      GetIsRowOrderTopToBottom() { return PR_TRUE; }
private:
  PRInt32   mWidth;
  PRInt32   mHeight;
  PRInt32   mDepth;
  PRInt32   mRowBytes;
  PRUint8  *mImageBits;
  PRUint32  mSizeImage;
  PRInt8    mNumBytesPixel;
  Pixmap    mImagePixmap;

  // for alpha mask
  PRUint8  *mAlphaBits;
  Pixmap    mAlphaPixmap;
  PRUint8   mAlphaDepth;
  PRUint16  mAlphaRowBytes;
  PRUint16  mAlphaWidth;
  PRUint16  mAlphaHeight;
  nsPoint   mLocation;
  
};

#endif
