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

#ifndef nsDeviceContextWin_h___
#define nsDeviceContextWin_h___

#include "nsDeviceContext.h"
#include <windows.h>

class nsDeviceContextWin : public DeviceContextImpl
{
public:
  nsDeviceContextWin();

  NS_IMETHOD    Init(nsNativeWidget aWidget);

  NS_IMETHOD    GetScrollBarWidth(float &aWidth) const;
  NS_IMETHOD    GetScrollBarHeight(float &aHeight) const;

  //get a low level drawing surface for rendering. the rendering context
  //that is passed in is used to create the drawing surface if there isn't
  //already one in the device context. the drawing surface is then cached
  //in the device context for re-use.
  virtual nsDrawingSurface GetDrawingSurface(nsIRenderingContext &aContext);

  NS_IMETHOD CheckFontExistence(const nsString& aFontName);

  NS_IMETHOD GetDepth(PRUint32& aDepth);

  NS_IMETHOD GetILColorSpace(IL_ColorSpace*& aColorSpace);

  NS_IMETHOD GetPaletteInfo(nsPaletteInfo&);

protected:
  virtual ~nsDeviceContextWin();

  nsDrawingSurface  mSurface;
  PRUint32          mDepth;  // bit depth of device
  nsPaletteInfo     mPaletteInfo;
};

#endif /* nsDeviceContextWin_h___ */
