/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "xp_core.h"			//this is a hack to get it to build. MMP
#include "nsFontMetricsMotif.h"
#include "nsDeviceContextMotif.h"

#include "nspr.h"
#include "nsCRT.h"

//#define NOISY_FONTS

nsFontMetricsMotif :: nsFontMetricsMotif()
{
  NS_INIT_REFCNT();
  mFont = nsnull;
  mFontHandle = nsnull;
  mFontInfo = nsnull;
}
  
nsFontMetricsMotif :: ~nsFontMetricsMotif()
{
  if (nsnull != mFont) {    
    delete mFont;
    mFont = nsnull;
  }
  
  if (nsnull != mFontHandle) {
    ::XUnloadFont(((nsDeviceContextMotif *)mContext)->GetDisplay(), mFontHandle);  
  }
}

NS_IMPL_ISUPPORTS1(nsFontMetricsMotif, nsIFontMetrics)

NS_IMETHODIMP nsFontMetricsMotif :: Init(const nsFont& aFont, nsIAtom* aLangGroup, nsIDeviceContext* aCX)
{
  NS_ASSERTION(!(nsnull == aCX), "attempt to init fontmetrics with null device context");

  nsAutoString  firstFace;
  if (NS_OK != aCX->FirstExistingFont(aFont, firstFace)) {
    aFont.GetFirstFamily(firstFace);
  }

  char        **fnames = nsnull;
  PRInt32     namelen = firstFace.Length() + 1;
  char	      *wildstring = (char *)PR_Malloc((namelen << 1) + 200);
  int         numnames = 0;
  char        altitalicization = 0;
  XFontStruct *fonts;
  float       t2d;
  aCX->GetTwipsToDevUnits(t2d);
  PRInt32     dpi = NSToIntRound(t2d * 1440);
  Display     *dpy;
  dpy = ((nsDeviceContextMotif *)aCX)->GetDisplay();

  if (nsnull == wildstring)
    return NS_ERROR_NOT_INITIALIZED;

  mFont = new nsFont(aFont);
  mContext = aCX;
  mFontHandle = nsnull;
  mLangGroup = aLangGroup;
  
  firstFace.ToCString(wildstring, namelen);

  if (abs(dpi - 75) < abs(dpi - 100))
    dpi = 75;
  else
    dpi = 100;

#ifdef NOISY_FONTS
#ifdef DEBUG
  fprintf(stderr, "looking for font %s (%d)", wildstring, aFont.size / 20);
#endif
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

  fnames = ::XListFontsWithInfo(dpy, &wildstring[namelen + 1], 200, &numnames, &fonts);

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

    fnames = ::XListFontsWithInfo(dpy, &wildstring[namelen + 1], 200, &numnames, &fonts);
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

    fnames = ::XListFontsWithInfo(dpy, &wildstring[namelen + 1], 200, &numnames, &fonts);

    if ((numnames <= 0) && altitalicization)
    {
      PR_snprintf(&wildstring[namelen + 1], namelen + 200,
                 "*-%s-%s-%c-normal--*-*-%d-%d-*-*-*",
                 newname,
                 (aFont.weight <= NS_FONT_WEIGHT_NORMAL) ? "medium" : "bold",
                 altitalicization, dpi, dpi);

      fnames = ::XListFontsWithInfo(dpy, &wildstring[namelen + 1], 200, &numnames, &fonts);
    }

    delete [] newname;
  }

  if (numnames > 0)
  {
    char *nametouse = PickAppropriateSize(fnames, fonts, numnames, aFont.size);
    
    mFontHandle = ::XLoadFont(dpy, nametouse);

#ifdef NOISY_FONTS
#ifdef DEBUG
    fprintf(stderr, " is: %s\n", nametouse);
#endif
#endif

    ::XFreeFontInfo(fnames, fonts, numnames);
  }
  else
  {
    //ack. we're in real trouble, go for fixed...

#ifdef NOISY_FONTS
#ifdef DEBUG
    fprintf(stderr, " is: %s\n", "fixed (final fallback)");
#endif
#endif

    mFontHandle = ::XLoadFont(dpy, "fixed");
  }

  RealizeFont();

  PR_Free(wildstring);

  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMotif :: Destroy()
{
//  NS_IF_RELEASE(mDeviceContext);
  return NS_OK;
}

char * nsFontMetricsMotif::PickAppropriateSize(char **names, XFontStruct *fonts, int cnt, nscoord desired)
{
  int         idx;
  float       app2dev;
  mContext->GetAppUnitsToDevUnits(app2dev);
  PRInt32     desiredpix = NSToIntRound(app2dev * desired);
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
}

void nsFontMetricsMotif::RealizeFont()
{
  mFontInfo = ::XQueryFont(((nsDeviceContextMotif *)mContext)->GetDisplay(), mFontHandle);

  float f;
  mContext->GetDevUnitsToAppUnits(f);
  
  mAscent = nscoord(mFontInfo->ascent * f);
  mDescent = nscoord(mFontInfo->descent * f);
  mMaxAscent = nscoord(mFontInfo->ascent * f) ;
  mMaxDescent = nscoord(mFontInfo->descent * f);
  
  mHeight = nscoord((mFontInfo->ascent + mFontInfo->descent) * f) ;
  mMaxAdvance = nscoord(mFontInfo->max_bounds.width * f);

  PRUint32 i;

  for (i = 0; i < 256; i++)
  {
    if ((i < mFontInfo->min_char_or_byte2) || (i > mFontInfo->max_char_or_byte2))
      mCharWidths[i] = mMaxAdvance;
    else
      mCharWidths[i] = nscoord((mFontInfo->per_char[i - mFontInfo->min_char_or_byte2].width) * f);
  }

  mLeading = 0;
}

NS_IMETHODIMP
nsFontMetricsMotif :: GetXHeight(nscoord& aResult)
{
  aResult = mMaxAscent / 2;     // XXX temporary code!
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMotif :: GetSuperscriptOffset(nscoord& aResult)
{
  aResult = mMaxAscent / 2;     // XXX temporary code!
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMotif :: GetSubscriptOffset(nscoord& aResult)
{
  aResult = mMaxAscent / 2;     // XXX temporary code!
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMotif :: GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  aOffset = 0; /* XXX */
  aSize = 0;  /* XXX */
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsMotif :: GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  aOffset = 0; /* XXX */
  aSize = 0;  /* XXX */
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMotif :: GetHeight(nscoord &aHeight)
{
  aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMotif :: GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMotif :: GetMaxAscent(nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMotif :: GetMaxDescent(nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMotif :: GetMaxAdvance(nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMotif :: GetFont(const nsFont*& aFont)
{
  aFont = mFont;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMotif :: GetFontHandle(nsFontHandle &aHandle)
{
  aHandle = (nsFontHandle)mFontHandle;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsMotif :: GetLangGroup(nsIAtom** aLangGroup)
{
  if (!aLangGroup) {
    return NS_ERROR_NULL_POINTER;
  }

  *aLangGroup = mLangGroup;
  NS_IF_ADDREF(*aLangGroup);

  return NS_OK;
}

static void MapGenericFamilyToFont(const nsString& aGenericFamily, nsIDeviceContext* aDC,
                                   nsString& aFontFace)
{
  // the CSS generic names (conversions from Nav for now)
  // XXX this  need to check availability with the dc
  PRBool  aliased;
  if (aGenericFamily.EqualsIgnoreCase("serif")) {
    aDC->GetLocalFontName("times", aFontFace, aliased);
  }
  else if (aGenericFamily.EqualsIgnoreCase("sans-serif")) {
    aDC->GetLocalFontName("helvetica", aFontFace, aliased);
  }
  else if (aGenericFamily.EqualsIgnoreCase("cursive")) {
    aDC->GetLocalFontName("script", aFontFace, aliased);  // XXX ???
  }
  else if (aGenericFamily.EqualsIgnoreCase("fantasy")) {
    aDC->GetLocalFontName("helvetica", aFontFace, aliased);
  }
  else if (aGenericFamily.EqualsIgnoreCase("monospace")) {
    aDC->GetLocalFontName("fixed", aFontFace, aliased);
  }
  else if (aGenericFamily.EqualsIgnoreCase("-moz-fixed")) {
    aDC->GetLocalFontName("fixed", aFontFace, aliased);
  }
  else {
    aFontFace.Truncate();
  }
}


