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

#include "nsFontMetricsPh.h"

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

nsFontMetricsPh :: nsFontMetricsPh()
{
  NS_INIT_REFCNT();
}
  
nsFontMetricsPh :: ~nsFontMetricsPh()
{
}

#ifdef LEAK_DEBUG
nsrefcnt
nsFontMetricsPh :: AddRef()
{
  NS_PRECONDITION(mRefCnt != 0, "resurrecting a dead object");
  return ++mRefCnt;
}

nsrefcnt
nsFontMetricsPh :: Release()
{
  NS_PRECONDITION(mRefCnt != 0, "too many release's");
  if (--mRefCnt == 0) {
    delete this;
  }
  return mRefCnt;
}

nsresult
nsFontMetricsPh :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = NULL;

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, kIFontMetricsIID);
  if (aIID.Equals(kClassIID)) {
    *aInstancePtr = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}
#else
NS_IMPL_ISUPPORTS(nsFontMetricsPh, kIFontMetricsIID)
#endif

NS_IMETHODIMP
nsFontMetricsPh :: Init(const nsFont& aFont, nsIDeviceContext *aContext)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: Destroy()
{
  return NS_OK;
}

static void
MapGenericFamilyToFont(const nsString& aGenericFamily,
                       nsIDeviceContext* aDC,
                       nsString& aFontFace)
{
}

struct FontEnumData
{
  FontEnumData(nsIDeviceContext* aContext, char* aFaceName)
  {
    mContext = aContext;
    mFaceName = aFaceName;
  }
  nsIDeviceContext* mContext;
  char* mFaceName;
};

static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  return PR_TRUE;
}

void
nsFontMetricsPh::RealizeFont()
{
}

NS_IMETHODIMP
nsFontMetricsPh :: GetXHeight(nscoord& aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetSuperscriptOffset(nscoord& aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetSubscriptOffset(nscoord& aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetHeight(nscoord &aHeight)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetLeading(nscoord &aLeading)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetMaxAscent(nscoord &aAscent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetMaxDescent(nscoord &aDescent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetMaxAdvance(nscoord &aAdvance)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh :: GetFont(const nsFont *&aFont)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPh::GetFontHandle(nsFontHandle &aHandle)
{
  return NS_OK;
}
