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
#include "nsDeviceContextUnix.h"

#include "nspr.h"
#include "nsCRT.h"

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

nsFontMetricsUnix :: nsFontMetricsUnix()
{
  NS_INIT_REFCNT();
  mFont = nsnull;
  mFontHandle = nsnull;
}
  
nsFontMetricsUnix :: ~nsFontMetricsUnix()
{
  if (nsnull != mFont)
  {
    delete mFont;
    mFont = nsnull;
  }
}

NS_IMPL_ISUPPORTS(nsFontMetricsUnix, kIFontMetricsIID)

nsresult nsFontMetricsUnix :: Init(const nsFont& aFont, nsIDeviceContext* aCX)
{
  mFont = new nsFont(aFont);
  mContext = aCX ;

  RealizeFont();

  return NS_OK;
}

void nsFontMetricsUnix::RealizeFont()
{

  mFontHandle = ::XLoadFont((Display *)mContext->GetNativeDeviceContext(), "fixed");

  XFontStruct * fs = ::XQueryFont((Display *)mContext->GetNativeDeviceContext(), mFontHandle);
  
  mAscent = fs->ascent ;
  mDescent = fs->descent ;
  mMaxAscent = fs->ascent ;
  mMaxDescent = fs->descent ;
  
  //  ::XSetFont(aRenderingSurface->display, aRenderingSurface->gc, mFontHandle);

  // XXX Temp hardcodes
  mHeight = 15 ;

  PRUint32 i;
  for (i=0;i<256;i++)
    mCharWidths[i] = 9 ;

  mMaxAdvance = 15;
  mLeading = 3;
}

nscoord nsFontMetricsUnix :: GetWidth(char ch)
{
  return mCharWidths[PRUint8(ch)];
}

nscoord nsFontMetricsUnix :: GetWidth(PRUnichar ch)
{
  
  if (ch < 256) {
    return mCharWidths[ch];
  }
  return 0;/* XXX */
}

nscoord nsFontMetricsUnix :: GetWidth(const nsString& aString)
{
  return GetWidth(aString.GetUnicode(), aString.Length());
}

nscoord nsFontMetricsUnix :: GetWidth(const char *aString)
{

  nscoord rc = 0 ;

  mFontHandle = ::XLoadFont((Display *)mContext->GetNativeDeviceContext(), "fixed");
  
  XFontStruct * fs = ::XQueryFont((Display *)mContext->GetNativeDeviceContext(), mFontHandle);
  
  rc = (nscoord) ::XTextWidth(fs, aString, nsCRT::strlen(aString));
  
  return (rc);

}

nscoord nsFontMetricsUnix :: GetWidth(const PRUnichar *aString, PRUint32 aLength)
{

  XChar2b * xstring ;
  XChar2b * thischar ;
  PRUint16 aunichar;
  nscoord width ;
  PRUint32 i ;

  xstring = (XChar2b *) PR_Malloc(sizeof(XChar2b)*aLength);

  for (i=0; i<aLength; i++) {
    thischar = (xstring+i);
    aunichar = (PRUint16)(*(aString+i));
    thischar->byte1 = (aunichar & 0xf);
    thischar->byte2 = (aunichar & 0xff) >> 8;      
  }

  mFontHandle = ::XLoadFont((Display *)mContext->GetNativeDeviceContext(), "fixed");
  
  XFontStruct * fs = ::XQueryFont((Display *)mContext->GetNativeDeviceContext(), mFontHandle);
  
  width = ::XTextWidth16(fs, xstring, aLength);

  PR_Free(xstring);

  return (width);
}

nscoord nsFontMetricsUnix :: GetHeight()
{
  return mHeight;
}

nscoord nsFontMetricsUnix :: GetLeading()
{
  return mLeading;
}

nscoord nsFontMetricsUnix :: GetMaxAscent()
{
  return mMaxAscent;
}

nscoord nsFontMetricsUnix :: GetMaxDescent()
{
  return mMaxDescent;
}

nscoord nsFontMetricsUnix :: GetMaxAdvance()
{
  return mMaxAdvance;
}

const nscoord * nsFontMetricsUnix :: GetWidths()
{
  return mCharWidths;
}

const nsFont& nsFontMetricsUnix :: GetFont()
{
  return *mFont;
}

nsFontHandle nsFontMetricsUnix :: GetFontHandle()
{
  return ((nsFontHandle)mFontHandle);
}


// XXX this function is a hack; the only logical font names we should
// support are the one used by css.
const char* nsFontMetricsUnix::MapFamilyToFont(const nsString& aLogicalFontName)
{
  if (aLogicalFontName.EqualsIgnoreCase("Times Roman")) {
    return "Times New Roman";
  }
  if (aLogicalFontName.EqualsIgnoreCase("Times")) {
    return "Times New Roman";
  }
  if (aLogicalFontName.EqualsIgnoreCase("Unicode")) {
    return "Bitstream Cyberbit";
  }
  if (aLogicalFontName.EqualsIgnoreCase("Courier")) {
    return "Courier New";
  }

  // the CSS generic names
  if (aLogicalFontName.EqualsIgnoreCase("serif")) {
    return "Times New Roman";
  }
  if (aLogicalFontName.EqualsIgnoreCase("sans-serif")) {
    return "Arial";
  }
  if (aLogicalFontName.EqualsIgnoreCase("cursive")) {
//    return "XXX";
  }
  if (aLogicalFontName.EqualsIgnoreCase("fantasy")) {
//    return "XXX";
  }
  if (aLogicalFontName.EqualsIgnoreCase("monospace")) {
    return "Courier New";
  }
  return "Arial";/* XXX for now */
}


