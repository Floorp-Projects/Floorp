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

  virtual nsresult Init(nsNativeWidget aWidget);

  virtual float GetScrollBarWidth() const;
  virtual float GetScrollBarHeight() const;

  //get a low level drawing surface for rendering. the rendering context
  //that is passed in is used to create the drawing surface if there isn't
  //already one in the device context. the drawing surface is then cached
  //in the device context for re-use.
  virtual nsDrawingSurface GetDrawingSurface(nsIRenderingContext &aContext);

  NS_IMETHOD CheckFontExistence(const char * aFontName);

  NS_IMETHOD GetDepth(PRUint32& aDepth);

  NS_IMETHOD CreateILColorSpace(IL_ColorSpace*& aColorSpace);

protected:
  virtual ~nsDeviceContextWin();

  HDC      mSurface;
  PRUint32 mDepth;  // bit depth of device
};

#endif /* nsDeviceContextWin_h___ */
