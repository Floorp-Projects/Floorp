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

protected:
  ~nsDeviceContextUnix();
  nsresult CreateFontCache();

  nsIFontCache      *mFontCache;
  nsIWidget         *mWidget;

};

#endif /* nsDeviceContextUnix_h___ */
