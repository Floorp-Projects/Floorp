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

#ifndef nsDeviceContextUnix_h___
#define nsDeviceContextUnix_h___

#include "nsIDeviceContext.h"
#include "nsUnitConversion.h"
#include "nsIFontCache.h"
#include "nsIWidget.h"
#include "nsIView.h"
#include "nsIRenderingContext.h"

#include "X11/Xlib.h"

/* nsDrawingSurface is actually the following struct */
typedef struct nsDrawingSurfaceUnix {
  Display *display ;
  Drawable drawable ;
  GC       gc ;
};

class nsDeviceContextUnix : public nsIDeviceContext
{
public:
  nsDeviceContextUnix();

  NS_DECL_ISUPPORTS

  virtual nsresult Init();

  virtual nsIRenderingContext * CreateRenderingContext(nsIView *aView);
  virtual void InitRenderingContext(nsIRenderingContext *aContext, nsIWidget *aWidget);

  virtual float GetTwipsToDevUnits() const;
  virtual float GetDevUnitsToTwips() const;

  virtual void SetAppUnitsToDevUnits(float aAppUnits);
  virtual void SetDevUnitsToAppUnits(float aDevUnits);

  virtual float GetAppUnitsToDevUnits() const;
  virtual float GetDevUnitsToAppUnits() const;

  virtual float GetScrollBarWidth() const;
  virtual float GetScrollBarHeight() const;

  virtual nsIFontCache * GetFontCache();
  virtual void FlushFontCache();

  virtual nsIFontMetrics* GetMetricsFor(const nsFont& aFont);

  virtual void SetZoom(float aZoom);
  virtual float GetZoom() const;

  virtual nsDrawingSurface GetDrawingSurface(nsIRenderingContext &aContext);
  virtual nsDrawingSurface GetDrawingSurface();

  //functions for handling gamma correction of output device
  virtual float GetGamma(void);
  virtual void SetGamma(float aGamma);

  //XXX the return from this really needs to be ref counted somehow. MMP
  virtual PRUint8 * GetGammaTable(void);

  virtual PRUint32 ConvertPixel(nscolor aColor);

protected:
  ~nsDeviceContextUnix();
  nsresult CreateFontCache();

  nsIFontCache      *mFontCache;
  nsDrawingSurfaceUnix * mSurface ;

  PRUint32 mDepth;
  Visual * mVisual;
  PRBool   mWriteable;
  PRUint32 mNumCells;
  Colormap mColormap;
  // XXX There should be a nsIColormap interface

  float             mTwipsToPixels;
  float             mPixelsToTwips;
  float             mAppUnitsToDevUnits;
  float             mDevUnitsToAppUnits;
  float             mZoom;
  float             mGammaValue;
  PRUint8           *mGammaTable;

public:
  void InstallColormap(void);
  void SetDrawingSurface(nsDrawingSurfaceUnix * aSurface) { mSurface = aSurface; }
  void SetGammaTable(PRUint8 * aTable, float aCurrentGamma, float aNewGamma);
};

#endif /* nsDeviceContextUnix_h___ */
