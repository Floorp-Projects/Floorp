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

  if (mFont != nsnull)
    nsFontMetricsMac::SetFont(*mFont, mContext);

  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);

  FontInfo fInfo;
  ::GetFontInfo(&fInfo);
 
  mAscent = NSToCoordRound(float(fInfo.ascent) * dev2app);
  mDescent = NSToCoordRound(float(fInfo.descent) * dev2app);
  mLeading = NSToCoordRound(float(fInfo.leading) * dev2app);
  mHeight = mAscent + mDescent + mLeading;
  mMaxAscent = mAscent;
  mMaxDescent = mDescent;
  mMaxAdvance = NSToCoordRound(float(::CharWidth(' ')) * dev2app);	// don't use fInfo.widMax here

  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: Destroy()
{
  return NS_OK;
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
  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);
  aResult = NSToCoordRound(float(mMaxAscent / 2) - dev2app);
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: GetSuperscriptOffset(nscoord& aResult)
{
  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);
  aResult = NSToCoordRound(float(mMaxAscent / 2) - dev2app);
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: GetSubscriptOffset(nscoord& aResult)
{
  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);
  aResult = NSToCoordRound(- float(mMaxDescent / 2) + dev2app);
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);
  aOffset = NSToCoordRound(float(mMaxAscent / 2) - dev2app);
  aSize   = dev2app;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMac :: GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);
  aOffset = NSToCoordRound(- float(mMaxDescent / 2) + dev2app);
  aSize   = dev2app;
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
nsFontMetricsMac :: GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth)
{
  if (nsnull == mContext)
  {
    aWidth = 0;
    return NS_ERROR_NULL_POINTER;
  }

  if (mFont != nsnull)
    nsFontMetricsMac::SetFont(*mFont, mContext);

  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);

  short textWidth = ::TextWidth(aString, 0, aLength);
  aWidth = NSToCoordRound(float(textWidth) * dev2app);

  if (mFont != nsnull) {
	switch (mFont->style)
	{
		case NS_FONT_STYLE_ITALIC:
		case NS_FONT_STYLE_OBLIQUE:
			nscoord aAdvance;
			GetMaxAdvance(aAdvance);
			aWidth += aAdvance;
			break;
	}
  }
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMac :: GetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth)
{
	nsString nsStr;
	nsStr.SetString(aString, aLength);
	char* cStr = nsStr.ToNewCString();
		GetWidth(cStr, aLength, aWidth);
	delete[] cStr;
    return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsMac :: GetWidth(nsIDeviceContext *aContext, const nsString& aString, nscoord &aWidth)
{
	nsIDeviceContext*	saveContext = mContext;
	mContext = aContext;

	char* cStr = aString.ToNewCString();
		nsresult res = GetWidth(cStr, aString.Length(), aWidth);
	delete[] cStr;

	mContext = saveContext;
    return res;
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


//------------------------------------------------------------------------

void nsFontMetricsMac :: SetFont(const nsFont& aFont, nsIDeviceContext* aContext)
{
	short fontNum;
	nsDeviceContextMac::GetMacFontNumber(aFont.name, fontNum);
	::TextFont(fontNum);

	float  dev2app;
	aContext->GetDevUnitsToAppUnits(dev2app);
	::TextSize(short(float(aFont.size) / dev2app));

	Style textFace = normal;
	switch (aFont.style)
	{
		case NS_FONT_STYLE_NORMAL: 								break;
		case NS_FONT_STYLE_ITALIC: 		textFace |= italic;		break;
		case NS_FONT_STYLE_OBLIQUE: 	textFace |= italic;		break;	//XXX
	}
	switch (aFont.variant)
	{
		case NS_FONT_VARIANT_NORMAL: 							break;
		case NS_FONT_VARIANT_SMALL_CAPS: textFace |= condense;	break;	//XXX
	}
	/*switch (aFont.weight)
	{
		case NS_FONT_WEIGHT_NORMAL: 							break;
		case NS_FONT_WEIGHT_BOLD:		textFace |= bold;		break;
	}*/
	if (aFont.weight > NS_FONT_WEIGHT_NORMAL)
		textFace |= bold;

	::TextFace(textFace);
}
