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

#ifndef nsDeviceContextMac_h___
#define nsDeviceContextMac_h___

#include "nsDeviceContext.h"
#include "nsUnitConversion.h"
#include "nsIFontCache.h"
#include "nsIWidget.h"
#include "nsIView.h"
#include "nsIRenderingContext.h"

class nsDeviceContextMac : public DeviceContextImpl
{
public:
  nsDeviceContextMac();

	NS_DECL_ISUPPORTS

  virtual nsresult Init(nsNativeWidget aNativeWidget);

  virtual float GetScrollBarWidth() const;
  virtual float GetScrollBarHeight() const;

  virtual nsDrawingSurface GetDrawingSurface(nsIRenderingContext &aContext);


  NS_IMETHOD CheckFontExistence(const nsString& aFontName);
  NS_IMETHOD CreateILColorSpace(IL_ColorSpace*& aColorSpace);
  NS_IMETHOD GetDepth(PRUint32& aDepth);


protected:
  virtual ~nsDeviceContextMac();
  nsresult CreateFontCache();

  nsIFontCache      		*mFontCache;
  nsDrawingSurface 			mSurface ;
  PRUint32 							mDepth;

};

#endif /* nsDeviceContextMac_h___ */
