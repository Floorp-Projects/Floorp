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

#include "nsFontMetricsXlib.h"

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

nsFontMetricsXlib::nsFontMetricsXlib()
{
  NS_INIT_REFCNT();
}

nsFontMetricsXlib::~nsFontMetricsXlib()
{
}

NS_IMPL_ISUPPORTS(nsFontMetricsXlib, kIFontMetricsIID)

NS_IMETHODIMP nsFontMetricsXlib::Init(const nsFont& aFont, nsIDeviceContext* aContext)
{
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsXlib::Destroy()
{
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsXlib::GetXHeight(nscoord& aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsXlib::GetSuperscriptOffset(nscoord& aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsXlib::GetSubscriptOffset(nscoord& aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsXlib::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsXlib::GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsXlib::GetHeight(nscoord &aHeight)
{
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsXlib::GetLeading(nscoord &aLeading)
{
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsXlib::GetMaxAscent(nscoord &aAscent)
{
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsXlib::GetMaxDescent(nscoord &aDescent)
{
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsXlib::GetMaxAdvance(nscoord &aAdvance)
{
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsXlib::GetFont(const nsFont *&aFont)
{
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsXlib::GetFontHandle(nsFontHandle &aHandle)
{
  return NS_OK;
}
