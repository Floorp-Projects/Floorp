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

#include "nsFontMetricsUnix.h"

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

nsFontMetricsUnix :: nsFontMetricsUnix()
{
  NS_INIT_REFCNT();
}
  
nsFontMetricsUnix :: ~nsFontMetricsUnix()
{
}

NS_IMPL_ISUPPORTS(nsFontMetricsUnix, kIFontMetricsIID)

nsresult nsFontMetricsUnix :: Init(const nsFont& aFont, nsIDeviceContext* aCX)
{
  mFont = new nsFont(aFont);
  mContext = aCX ;

  QueryFont();

  return NS_OK;
}

void nsFontMetricsUnix::QueryFont()
{
}

nscoord nsFontMetricsUnix :: GetWidth(char ch)
{
  return nsnull;
}

nscoord nsFontMetricsUnix :: GetWidth(PRUnichar ch)
{
  return 0;
}

nscoord nsFontMetricsUnix :: GetWidth(const nsString& aString)
{
  return nsnull;
}

nscoord nsFontMetricsUnix :: GetWidth(const char *aString)
{
  return nsnull;
}

nscoord nsFontMetricsUnix :: GetWidth(const PRUnichar *aString, PRUint32 aLength)
{
  return nsnull;
}

nscoord nsFontMetricsUnix :: GetHeight()
{
  return 0;
}

nscoord nsFontMetricsUnix :: GetLeading()
{
  return 0;
}

nscoord nsFontMetricsUnix :: GetMaxAscent()
{
  return 0;
}

nscoord nsFontMetricsUnix :: GetMaxDescent()
{
  return 0;
}

nscoord nsFontMetricsUnix :: GetMaxAdvance()
{
  return 0;
}

const nscoord * nsFontMetricsUnix :: GetWidths()
{
  return nsnull;
}

const nsFont& nsFontMetricsUnix :: GetFont()
{
  return *mFont;
}

nsFontHandle nsFontMetricsUnix :: GetFontHandle()
{
  return ((nsFontHandle)mFontHandle);
}




