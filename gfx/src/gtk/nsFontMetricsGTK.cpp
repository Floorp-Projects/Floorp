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

#include "nsFontMetricsGTK.h"
#include <gdk/gdkx.h>

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

nsFontMetricsGTK :: nsFontMetricsGTK()
{
  NS_INIT_REFCNT();
}
  
nsFontMetricsGTK :: ~nsFontMetricsGTK()
{
}

NS_IMPL_ISUPPORTS(nsFontMetricsGTK, kIFontMetricsIID)

nsresult nsFontMetricsGTK :: Init(const nsFont& aFont, nsIDeviceContext* aCX)
{
  mFont = new nsFont(aFont);
  mContext = aCX ;

  //  QueryFont();
  return NS_OK;
}

void nsFontMetricsGTK::QueryFont()
{
  const char * str = mFont->name.ToNewCString();
  mGdkFont = gdk_font_load(str);
  delete str;

  // Get character sizes
  float p2t = mContext->GetDevUnitsToAppUnits();
  for (int i = 0; i < 256; i++)
    mCharWidths[i] = nscoord((gdk_char_width(mGdkFont, (gchar)i)), p2t);

}

nscoord nsFontMetricsGTK :: GetWidth(char ch)
{
  return mCharWidths[PRUint8(ch)];
}

nscoord nsFontMetricsGTK :: GetWidth(PRUnichar ch)
{
  if (ch < 256)
    return mCharWidths[ch];
  return 0; /* XXX */
}

nscoord nsFontMetricsGTK :: GetWidth(const nsString& aString)
{
  const char *str = aString.ToNewCString();
  nscoord width = GetWidth(str);
  delete str;
  return width;
}

nscoord nsFontMetricsGTK :: GetWidth(const char *aString)
{
  float p2t = mContext->GetDevUnitsToAppUnits();
  return nscoord((gdk_string_width(mGdkFont, (const gchar *)aString)), p2t);
}

nscoord nsFontMetricsGTK :: GetWidth(const PRUnichar *aString, PRUint32 aLength)
{
  // XXX use native text measurement routine
  nscoord sum = 0;

  PRUint8 ch;
  while ((ch = PRUint8(*aString++)) != 0) {
    sum += mCharWidths[ch];
  }
  return sum;
}

nscoord nsFontMetricsGTK :: GetHeight()
{
  return mGdkFont->descent + mGdkFont->ascent;
}

nscoord nsFontMetricsGTK :: GetLeading()
{
  return 0;
}

nscoord nsFontMetricsGTK :: GetMaxAscent()
{
  return mGdkFont->ascent;
}

nscoord nsFontMetricsGTK :: GetMaxDescent()
{
  return mGdkFont->descent;
}

nscoord nsFontMetricsGTK :: GetMaxAdvance()
{
  return ((XFontStruct *)GDK_FONT_XFONT(mGdkFont))->max_bounds.width;
}

const nscoord * nsFontMetricsGTK :: GetWidths()
{
  return mCharWidths;
}

const nsFont& nsFontMetricsGTK :: GetFont()
{
  return *mFont;
}

nsFontHandle nsFontMetricsGTK :: GetFontHandle()
{
  return ((nsFontHandle)mGdkFont);
}




