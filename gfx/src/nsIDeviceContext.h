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

#ifndef nsIDeviceContext_h___
#define nsIDeviceContext_h___

#include "nsISupports.h"
#include "nsIRenderingContext.h"
#include "nsCoord.h"
#include "nsRect.h"
#include "nsIWidget.h"

class nsIView;
class nsIRenderingContext;
class nsIFontCache;
class nsIFontMetrics;
class nsIWidget;
struct nsFont;

//a cross platform way of specifying a navite device context
typedef void * nsNativeDeviceContext;

#define NS_IDEVICE_CONTEXT_IID   \
{ 0x5931c580, 0xb917, 0x11d1,    \
{ 0xa8, 0x24, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

class nsIDeviceContext : public nsISupports
{
public:
  virtual nsresult Init(nsNativeWidget aWidget) = 0;

  virtual nsIRenderingContext * CreateRenderingContext(nsIView *aView) = 0;
  virtual nsresult InitRenderingContext(nsIRenderingContext *aContext, nsIWidget *aWindow) = 0;

  //these are queries to figure out how large an output unit
  //(i.e. pixel) is in terms of twips (1/20 of a point)
  virtual float GetDevUnitsToTwips() const = 0;
  virtual float GetTwipsToDevUnits() const = 0;

  //these are set by the object that created this
  //device context to define what the scale is
  //between the units used by the app and the
  //device units
  virtual void SetAppUnitsToDevUnits(float aAppUnits) = 0;
  virtual void SetDevUnitsToAppUnits(float aDevUnits) = 0;

  //these are used to query the scale values defined
  //by the above Set*() methods
  virtual float GetAppUnitsToDevUnits() const = 0;
  virtual float GetDevUnitsToAppUnits() const = 0;

  virtual float GetScrollBarWidth() const = 0;
  virtual float GetScrollBarHeight() const = 0;

  //be sure to Relase() after you are done with the Get()
  virtual nsIFontCache * GetFontCache() = 0;
  virtual void FlushFontCache() = 0;

  // Get the font metrics for a given font.
  virtual nsIFontMetrics* GetMetricsFor(const nsFont& aFont) = 0;

  //get and set the document zoom value used for display-time
  //scaling. default is 1.0 (no zoom)
  virtual void SetZoom(float aZoom) = 0;
  virtual float GetZoom() const = 0;

  //get a low level drawing surface for rendering. the rendering context
  //that is passed in is used to create the drawing surface if there isn't
  //already one in the device context. the drawing surface is then cached
  //in the device context for re-use.
  virtual nsDrawingSurface GetDrawingSurface(nsIRenderingContext &aContext) = 0;

  //functions for handling gamma correction of output device
  virtual float GetGamma(void) = 0;
  virtual void SetGamma(float aGamma) = 0;

  //XXX the return from this really needs to be ref counted somehow. MMP
  virtual PRUint8 * GetGammaTable(void) = 0;

  virtual nsNativeWidget GetNativeWidget(void) = 0;
};

#endif /* nsIDeviceContext_h___ */
