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

#include "nsFontMetricsMac.h"
#include "nsDeviceContextMac.h"

#include "nspr.h"
#include "nsCRT.h"

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

//#define NOISY_FONTS

nsFontMetricsMac :: nsFontMetricsMac()
{
  NS_INIT_REFCNT();
  mFont = nsnull;//еее
  //mFontHandle = nsnull;
}
  
nsFontMetricsMac :: ~nsFontMetricsMac()
{
  if (nsnull != mFont)
  {
    delete mFont;
    mFont = nsnull;
  }
}

NS_IMPL_ISUPPORTS(nsFontMetricsMac, kIFontMetricsIID)

NS_IMETHODIMP nsFontMetricsMac :: Init(const nsFont& aFont, nsIDeviceContext* aCX)
{
  NS_ASSERTION(!(nsnull == aCX), "attempt to init fontmetrics with null device context");

/*
  char        **fnames = nsnull;
  PRInt32     namelen = aFont.name.Length() + 1;
  char	      *wildstring = (char *)PR_Malloc((namelen << 1) + 200);
  int         numnames = 0;
  char        altitalicization = 0;
  XFontStruct *fonts;
  PRInt32     dpi = NSToIntRound(aCX->GetTwipsToDevUnits() * 1440);
  Display     *dpy = XtDisplay((Widget)aCX->GetNativeWidget());

  if (nsnull == wildstring)
    return NS_ERROR_NOT_INITIALIZED;

  mFont = new nsFont(aFont);
  mContext = aCX;
  mFontHandle = nsnull;

  aFont.name.ToCString(wildstring, namelen);

  if (abs(dpi - 75) < abs(dpi - 100))
    dpi = 75;
  else
    dpi = 100;

#ifdef NOISY_FONTS
  fprintf(stderr, "looking for font %s (%d)", wildstring, aFont.size / 20);
#endif

  //font properties we care about:
  //name
  //weight (bold, medium)
  //slant (r = normal, i = italic, o = oblique)
  //size in nscoords >> 1

  PR_snprintf(&wildstring[namelen + 1], namelen + 200,
             "*-%s-%s-%c-normal--*-*-%d-%d-*-*-*",
             wildstring,
             (aFont.weight <= NS_FONT_WEIGHT_NORMAL) ? "medium" : "bold",
             (aFont.style == NS_FONT_STYLE_NORMAL) ? 'r' :
             ((aFont.style == NS_FONT_STYLE_ITALIC) ? 'i' : 'o'), dpi, dpi);

  fnames = XListFontsWithInfo(dpy, &wildstring[namelen + 1], 200, &numnames, &fonts);

  if (aFont.style == NS_FONT_STYLE_ITALIC)
    altitalicization = 'o';
  else if (aFont.style == NS_FONT_STYLE_OBLIQUE)
    altitalicization = 'i';

  if ((numnames <= 0) && altitalicization)
  {
    PR_snprintf(&wildstring[namelen + 1], namelen + 200,
               "*-%s-%s-%c-normal--*-*-%d-%d-*-*-*",
	             wildstring,
               (aFont.weight <= NS_FONT_WEIGHT_NORMAL) ? "medium" : "bold",
               altitalicization, dpi, dpi);

    fnames = XListFontsWithInfo(dpy, &wildstring[namelen + 1], 200, &numnames, &fonts);
  }

  if (numnames <= 0)
  {
    //we were not able to match the font name at all...

    const char *newname = MapFamilyToFont(aFont.name);

    PR_snprintf(&wildstring[namelen + 1], namelen + 200,
               "*-%s-%s-%c-normal--*-*-%d-%d-*-*-*",
               newname,
               (aFont.weight <= NS_FONT_WEIGHT_NORMAL) ? "medium" : "bold",
               (aFont.style == NS_FONT_STYLE_NORMAL) ? 'r' :
               ((aFont.style == NS_FONT_STYLE_ITALIC) ? 'i' : 'o'), dpi, dpi);

    fnames = XListFontsWithInfo(dpy, &wildstring[namelen + 1], 200, &numnames, &fonts);

    if ((numnames <= 0) && altitalicization)
    {
      PR_snprintf(&wildstring[namelen + 1], namelen + 200,
                 "*-%s-%s-%c-normal--*-*-%d-%d-*-*-*",
                 newname,
                 (aFont.weight <= NS_FONT_WEIGHT_NORMAL) ? "medium" : "bold",
                 altitalicization, dpi, dpi);

      fnames = XListFontsWithInfo(dpy, &wildstring[namelen + 1], 200, &numnames, &fonts);
    }
  }

  if (numnames > 0)
  {
    char *nametouse = PickAppropriateSize(fnames, fonts, numnames, aFont.size);
    
    mFontHandle = ::XLoadFont(dpy, nametouse);

#ifdef NOISY_FONTS
    fprintf(stderr, " is: %s\n", nametouse);
#endif
    
    XFreeFontInfo(fnames, fonts, numnames);
  }
  else
  {
    //ack. we're in real trouble, go for fixed...

#ifdef NOISY_FONTS
    fprintf(stderr, " is: %s\n", "fixed (final fallback)");
#endif

    mFontHandle = ::XLoadFont(dpy, "fixed");
  }

  RealizeFont();

  PR_Free(wildstring);*/

  mFont = new nsFont(aFont);//еее
  mContext = aCX;

  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);

  FontInfo fInfo;
  ::GetFontInfo(&fInfo);
 
  mAscent = fInfo.ascent * dev2app;
  mDescent = fInfo.descent * dev2app;
  mLeading = fInfo.leading * dev2app;
  mHeight = mAscent + mDescent + mLeading;
  mMaxAscent = mAscent;
  mMaxDescent = mDescent;
  mMaxAdvance = fInfo.widMax * dev2app;

  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: Destroy()
{
}

/*char * nsFontMetricsMac::PickAppropriateSize(char **names, XFontStruct *fonts, int cnt, nscoord desired)
{
  int         idx;
  PRInt32     desiredpix = NSToIntRound(mContext->GetAppUnitsToDevUnits() * desired);
  XFontStruct *curfont;
  PRInt32     closestmin = -1, minidx;

  //first try an exact or closest smaller match...

  for (idx = 0, curfont = fonts; idx < cnt; idx++, curfont++)
  {
    PRInt32 height = curfont->ascent + curfont->descent;

    if (height == desiredpix)
      break;

    if ((height < desiredpix) && (height > closestmin))
    {
      closestmin = height;
      minidx = idx;
    }
  }

  if (idx < cnt)
    return names[idx];
  else if (closestmin >= 0)
    return names[minidx];
  else
  {
    closestmin = 2000000;

    for (idx = 0, curfont = fonts; idx < cnt; idx++, curfont++)
    {
      PRInt32 height = curfont->ascent + curfont->descent;

      if ((height > desiredpix) && (height < closestmin))
      {
        closestmin = height;
        minidx = idx;
      }
    }

    return names[minidx];
  }
}*/

/*void nsFontMetricsMac::RealizeFont()
{
  XFontStruct * fs = ::XQueryFont(XtDisplay((Widget)mContext->GetNativeWidget()), mFontHandle);

  float f  = mContext->GetDevUnitsToAppUnits();
  
  mAscent = nscoord(fs->ascent * f);
  mDescent = nscoord(fs->descent * f);
  mMaxAscent = nscoord(fs->ascent * f) ;
  mMaxDescent = nscoord(fs->descent * f);
  
  mHeight = nscoord((fs->ascent + fs->descent) * f) ;
  mMaxAdvance = nscoord(fs->max_bounds.width * f);

  PRUint32 i;

  for (i = 0; i < 256; i++)
  {
    if ((i < fs->min_char_or_byte2) || (i > fs->max_char_or_byte2))
      mCharWidths[i] = mMaxAdvance;
    else
      mCharWidths[i] = nscoord((fs->per_char[i - fs->min_char_or_byte2].width) * f);
  }

  mLeading = 0;
}*/

NS_IMETHODIMP
nsFontMetricsMac :: GetXHeight(nscoord& aResult)
{
//  aResult = mMaxAscent / 2;     // XXX temporary code!
  aResult = 0;/* XXX */
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: GetSuperscriptOffset(nscoord& aResult)
{
//  aResult = mMaxAscent / 2;     // XXX temporary code!
  aResult = 0;/* XXX */
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: GetSubscriptOffset(nscoord& aResult)
{
//  aResult = mMaxAscent / 2;     // XXX temporary code!
  aResult = 0;/* XXX */
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetWidth(char ch, nscoord &aWidth)
{
  char buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth);
}

NS_IMETHODIMP nsFontMetricsMac :: GetWidth(PRUnichar ch, nscoord &aWidth)
{
  PRUnichar buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth);
}

NS_IMETHODIMP nsFontMetricsMac :: GetWidth(const nsString& aString, nscoord &aWidth)
{
  return GetWidth(aString.GetUnicode(), aString.Length(), aWidth);
}

NS_IMETHODIMP nsFontMetricsMac :: GetWidth(const char *aString, nscoord &aWidth)
{
  return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP
nsFontMetricsMac :: GetWidth(const char* aString,
                             PRUint32 aLength,
                             nscoord& aWidth)
{
  if (nsnull == mContext) {
    aWidth = 0;
    return NS_ERROR_NULL_POINTER;
  }

/*
  nsNativeWidget widget;
  mDeviceContext->GetNativeWidget(widget);
  HWND win = (HWND)widget;
  HDC  hdc = ::GetDC(win);
  HFONT oldfont = (HFONT)::SelectObject(hdc, (HGDIOBJ) mFontHandle);
  SIZE size;
  BOOL status = GetTextExtentPoint32(hdc, aString, aLength, &size);
  ::ReleaseDC(win, hdc);
*/

  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);

  short textWidth = ::TextWidth(aString, 0, aLength);
  aWidth = nscoord(float(textWidth) * dev2app);
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth)
{
//  XChar2b * xstring ;
//  XChar2b * thischar ;
//  PRUint16 aunichar;
  nscoord width = 0 ;
//  PRUint32 i ;

//  xstring = (XChar2b *) PR_Malloc(sizeof(XChar2b)*aLength);

//  for (i=0; i<aLength; i++) {
//    thischar = (xstring+i);
//    aunichar = (PRUint16)(*(aString+i));
//    thischar->byte2 = (aunichar & 0xff);
//    thischar->byte1 = (aunichar & 0xff00) >> 8;      
//  }
  
  //XFontStruct * fs = ::XQueryFont(XtDisplay((Widget)mContext->GetNativeWidget()), mFontHandle);
  
//  width = ::XTextWidth16(fs, xstring, aLength);
  //width = ::XTextWidth16(fs, (XChar2b *)aString, aLength);

//  PR_Free(xstring);

  //return (nscoord(width * mContext->GetDevUnitsToAppUnits()));
   
  if (nsnull == mContext) {
    aWidth = 0;
    return NS_ERROR_NULL_POINTER;
  }

  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);

	PRUint32 i;//еее
	for (i = 0; i < aLength; i ++)
		width += ::CharWidth(aString[i]);
	aWidth = width*dev2app;
    return NS_OK;//еее
}

NS_IMETHODIMP  nsFontMetricsMac :: GetWidth(nsIDeviceContext *aContext, const nsString& aString, nscoord &aWidth)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsFontMetricsMac :: GetHeight(nscoord &aHeight)
{
  aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetMaxAscent(nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetMaxDescent(nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetMaxAdvance(nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetWidths(const nscoord *&aWidths)
{
  //return mCharWidths;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsFontMetricsMac :: GetFont(const nsFont *&aFont)
{
  aFont = mFont;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetFontHandle(nsFontHandle &aHandle)
{
  //return (nsFontHandle)mFontHandle;
  aHandle = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}


// XXX this function is a hack; the only logical font names we should
// support are the one used by css.

// we'll probably need something similar for the mac
/*const char* nsFontMetricsMac::MapFamilyToFont(const nsString& aLogicalFontName)
{
  if (aLogicalFontName.EqualsIgnoreCase("Times Roman")) {
    return "times";
  }
  if (aLogicalFontName.EqualsIgnoreCase("Times New Roman")) {
    return "times";
  }
  if (aLogicalFontName.EqualsIgnoreCase("Unicode")) {
    return "Bitstream Cyberbit";
  }
  if (aLogicalFontName.EqualsIgnoreCase("Courier New")) {
    return "courier";
  }
  if (aLogicalFontName.EqualsIgnoreCase("Arial")) {
    return "helvetica";
  }

  // the CSS generic names
  if (aLogicalFontName.EqualsIgnoreCase("serif")) {
    return "times";
  }
  if (aLogicalFontName.EqualsIgnoreCase("sans-serif")) {
    return "helvetica";
  }
  if (aLogicalFontName.EqualsIgnoreCase("cursive")) {
//    return "XXX";
  }
  if (aLogicalFontName.EqualsIgnoreCase("fantasy")) {
//    return "XXX";
  }
  if (aLogicalFontName.EqualsIgnoreCase("monospace")) {
    return "fixed";
  }
  return "helvetica";
}
*/

