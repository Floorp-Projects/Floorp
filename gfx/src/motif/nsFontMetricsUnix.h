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

#ifndef nsFontMetricsUnix_h__
#define nsFontMetricsUnix_h__

#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"
#include "nsCRT.h"

#include "X11/Xlib.h"

class nsFontMetricsUnix : public nsIFontMetrics
{
public:
  nsFontMetricsUnix();
  ~nsFontMetricsUnix();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  virtual nsresult Init(const nsFont& aFont, nsIDeviceContext* aContext);
  virtual nscoord GetWidth(char aC);
  virtual nscoord GetWidth(PRUnichar aC);
  virtual nscoord GetWidth(const nsString& aString);
  virtual nscoord GetWidth(const char *aString);
  virtual nscoord GetWidth(const PRUnichar *aString, PRUint32 aLength);
  virtual nscoord GetHeight();
  virtual nscoord GetLeading();
  virtual nscoord GetMaxAscent();
  virtual nscoord GetMaxDescent();
  virtual nscoord GetMaxAdvance();
  virtual const nscoord *GetWidths();
  virtual const nsFont& GetFont();
  virtual nsFontHandle GetFontHandle();

protected:
  void RealizeFont();
  static const char* MapFamilyToFont(const nsString& aLogicalFontName);

protected:
  void QueryFont();

  nsFont            *mFont;
  nsIDeviceContext  *mContext;
  XFontStruct       *mFontInfo;
  Font              mFontHandle;
  nscoord           mCharWidths[256];
  nscoord           mHeight;
  nscoord           mAscent;
  nscoord           mDescent;
  nscoord           mLeading;
  nscoord           mMaxAscent;
  nscoord           mMaxDescent;
  nscoord           mMaxAdvance;
};

#endif




