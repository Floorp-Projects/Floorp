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

#include "xp_core.h"
#include "nsFontMetricsGTK.h"
#include "nspr.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <X11/Xatom.h>

//#define NOISY_FONTS 1

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

nsFontMetricsGTK::nsFontMetricsGTK()
{
  NS_INIT_REFCNT();
  mDeviceContext = nsnull;
  mFont = nsnull;
  mFontHandle = nsnull;

  mHeight = 0;
  mAscent = 0;
  mDescent = 0;
  mLeading = 0;
  mMaxAscent = 0;
  mMaxDescent = 0;
  mMaxAdvance = 0;
  mXHeight = 0;
  mSuperscriptOffset = 0;
  mSubscriptOffset = 0;
  mStrikeoutSize = 0;
  mStrikeoutOffset = 0;
  mUnderlineSize = 0;
  mUnderlineOffset = 0;
}

nsFontMetricsGTK::~nsFontMetricsGTK()
{
  if (nsnull != mFont) {
    delete mFont;
    mFont = nsnull;
  }

  if (nsnull != mFontHandle) {
    ::gdk_font_unref (mFontHandle);
  }
}

NS_IMPL_ISUPPORTS(nsFontMetricsGTK, kIFontMetricsIID)

NS_IMETHODIMP nsFontMetricsGTK::Init(const nsFont& aFont, nsIDeviceContext* aContext)
{
  NS_ASSERTION(!(nsnull == aContext), "attempt to init fontmetrics with null device context");

  nsAutoString  firstFace;
  if (NS_OK != aContext->FirstExistingFont(aFont, firstFace)) {
    aFont.GetFirstFamily(firstFace);
  }

  char        **fnames = nsnull;
  PRInt32     namelen = firstFace.Length() + 1;
  char	      *wildstring = (char *)PR_Malloc((namelen << 1) + 200);
  int         numnames = 0;
  char        altitalicization = 0;
  XFontStruct *fonts;
  float       t2d;
  aContext->GetTwipsToDevUnits(t2d);
  PRInt32     dpi = NSToIntRound(t2d * 1440);

  if (nsnull == wildstring)
    return NS_ERROR_NOT_INITIALIZED;

  mFont = new nsFont(aFont);
  mDeviceContext = aContext;
  mFontHandle = nsnull;

  firstFace.ToCString(wildstring, namelen);

  if (abs(dpi - 75) < abs(dpi - 100))
    dpi = 75;
  else
    dpi = 100;

#ifdef NOISY_FONTS
  g_print("looking for font %s (%d)", wildstring, aFont.size / 20);
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

  fnames = ::XListFontsWithInfo(GDK_DISPLAY(), &wildstring[namelen + 1], 200, &numnames, &fonts);

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

    fnames = ::XListFontsWithInfo(GDK_DISPLAY(), &wildstring[namelen + 1], 200, &numnames, &fonts);
  }


  if (numnames <= 0)
  {
    //we were not able to match the font name at all...

    char *newname = firstFace.ToNewCString();

    PR_snprintf(&wildstring[namelen + 1], namelen + 200,
               "*-%s-%s-%c-normal--*-*-%d-%d-*-*-*",
               newname,
               (aFont.weight <= NS_FONT_WEIGHT_NORMAL) ? "medium" : "bold",
               (aFont.style == NS_FONT_STYLE_NORMAL) ? 'r' :
               ((aFont.style == NS_FONT_STYLE_ITALIC) ? 'i' : 'o'), dpi, dpi);

    fnames = ::XListFontsWithInfo(GDK_DISPLAY(), &wildstring[namelen + 1], 200, &numnames, &fonts);

    if ((numnames <= 0) && altitalicization)
    {
      PR_snprintf(&wildstring[namelen + 1], namelen + 200,
                 "*-%s-%s-%c-normal--*-*-%d-%d-*-*-*",
                 newname,
                 (aFont.weight <= NS_FONT_WEIGHT_NORMAL) ? "medium" : "bold",
                 altitalicization, dpi, dpi);

      fnames = ::XListFontsWithInfo(GDK_DISPLAY(), &wildstring[namelen + 1], 200, &numnames, &fonts);
    }

    delete [] newname;
  }

  if (numnames > 0)
  {
    char *nametouse = PickAppropriateSize(fnames, fonts, numnames, aFont.size);

    mFontHandle = ::gdk_font_load(nametouse);

#ifdef NOISY_FONTS
    g_print(" is: %s\n", nametouse);
#endif

    ::XFreeFontInfo(fnames, fonts, numnames);
  }
  else
  {
    //ack. we're in real trouble, go for fixed...

#ifdef NOISY_FONTS
    g_print(" is: %s\n", "fixed (final fallback)");
#endif

    mFontHandle = ::gdk_font_load("fixed");
  }

  RealizeFont();

  PR_Free(wildstring);

  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::Destroy()
{
//  NS_IF_RELEASE(mDeviceContext);
  return NS_OK;
}

char * nsFontMetricsGTK::PickAppropriateSize(char **names, XFontStruct *fonts, int cnt, nscoord desired)
{
  int         idx;
  float       app2dev;
  mDeviceContext->GetAppUnitsToDevUnits(app2dev);
//  XXX FIX ME
  PRInt32     desiredpix = NSToIntRound(app2dev * desired);
  XFontStruct *curfont;
  PRInt32     closestmin = -1, minidx;

  //first try an exact or closest smaller match...
  
  for (idx = 0, curfont = fonts; idx < cnt; idx++, curfont++)
  {
    PRInt32 height = NSToIntRound(0.75 * (curfont->ascent + curfont->descent));

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
      PRInt32 height = NSToIntRound(0.75 * (curfont->ascent + curfont->descent));

      if ((height > desiredpix) && (height < closestmin))
      {
        closestmin = height;
        minidx = idx;
      }
    }

    return names[minidx];
  }
}

void nsFontMetricsGTK::RealizeFont()
{
//XXX this API is dead... MMP
//  nsNativeWidget  widget;
//  mDeviceContext->GetNativeWidget(widget);
  XFontStruct *fontInfo;
  nscoord CharWidths[256];
  
  fontInfo = (XFontStruct *)GDK_FONT_XFONT(mFontHandle);

  float f;
  mDeviceContext->GetDevUnitsToAppUnits(f);

  mAscent = nscoord(fontInfo->ascent * f);
  mDescent = nscoord(fontInfo->descent * f);
  mMaxAscent = nscoord(fontInfo->ascent * f) ;
  mMaxDescent = nscoord(fontInfo->descent * f);

  mHeight = nscoord((fontInfo->ascent + fontInfo->descent) * f);
  mMaxAdvance = nscoord(fontInfo->max_bounds.width * f);

  unsigned long pr = 0;

  if (::XGetFontProperty(fontInfo, XA_X_HEIGHT, &pr))
  {
    mXHeight = nscoord(pr * f);
  }

  if (::XGetFontProperty(fontInfo, XA_UNDERLINE_POSITION, &pr))
  {
    /* this will only be provided from adobe .afm fonts */
    mUnderlineOffset = NSToIntRound(pr * f);
  }
  else
  {
    /* this may need to be different than one for those weird asian fonts */
    /* mHeight is already multipled by f */
    float height;
    height = fontInfo->ascent + fontInfo->descent;
    mUnderlineOffset = -NSToIntRound(MAX (1, floor (0.1 * height + 0.5)) * f);
  }



  if (::XGetFontProperty(fontInfo, XA_UNDERLINE_THICKNESS, &pr))
  {
    /* this will only be provided from adobe .afm fonts */
    mUnderlineSize = nscoord(MAX(f, NSToIntRound(pr * f)));
  }
  else
  {
    /* mHeight is already multipled by f */
    float height;
    height = fontInfo->ascent + fontInfo->descent;
    mUnderlineSize = NSToIntRound(MAX(1, floor (0.05 * height + 0.5)) * f);
  }


  /* need better way to calculate this */
  mStrikeoutOffset = NSToIntRound((mAscent + 1) / 2);
  mStrikeoutSize = mUnderlineSize;

  PRUint32 i;

  for (i = 0; i < 256; i++)
  {
    if ((i < fontInfo->min_char_or_byte2) || (i > fontInfo->max_char_or_byte2))
      CharWidths[i] = mMaxAdvance;
    else
      CharWidths[i] = nscoord((fontInfo->per_char[i - fontInfo->min_char_or_byte2].width) * f);
  }

  mLeading = 0;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetXHeight(nscoord& aResult)
{
  aResult = mXHeight;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetSuperscriptOffset(nscoord& aResult)
{
  aResult = mSuperscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetSubscriptOffset(nscoord& aResult)
{
  aResult = mSubscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mStrikeoutOffset;
  aSize = mStrikeoutSize;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mUnderlineOffset;
  aSize = mUnderlineSize;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetHeight(nscoord &aHeight)
{
  aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetMaxAscent(nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetMaxDescent(nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetMaxAdvance(nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetFont(const nsFont*& aFont)
{
  aFont = mFont;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetFontHandle(nsFontHandle &aHandle)
{
  aHandle = (nsFontHandle)mFontHandle;
  return NS_OK;
}

