/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "xp_core.h"
#include "nsFontMetricsBeOS.h"
#include "nspr.h"

#undef NOISY_FONTS
#undef REALLY_NOISY_FONTS

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

nsFontMetricsBeOS::nsFontMetricsBeOS()
{
  NS_INIT_REFCNT();
  mDeviceContext = nsnull;
  mFont = nsnull;
//  mFontHandle = nsnull;

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

nsFontMetricsBeOS::~nsFontMetricsBeOS()
{
  if (nsnull != mFont) {
    delete mFont;
    mFont = nsnull;
  }
}

NS_IMPL_ISUPPORTS(nsFontMetricsBeOS, kIFontMetricsIID)

NS_IMETHODIMP nsFontMetricsBeOS::Init(const nsFont& aFont, nsIAtom* aLangGroup,
  nsIDeviceContext* aContext)
{
  NS_ASSERTION(!(nsnull == aContext), "attempt to init fontmetrics with null device context");

  nsAutoString  firstFace;
  mLangGroup = aLangGroup;
  if (NS_OK != aContext->FirstExistingFont(aFont, firstFace)) {
    aFont.GetFirstFamily(firstFace);
  }

  PRInt32     namelen = MAX( firstFace.Length() + 1, sizeof(font_family) );
  font_family wildstring;
  PRInt16	  face = B_REGULAR_FACE;

  mFont = new nsFont(aFont);

  float       app2dev, app2twip;
  aContext->GetAppUnitsToDevUnits(app2dev);
  aContext->GetDevUnitsToTwips(app2twip);

  app2twip *= app2dev;
  float rounded = ((float)NSIntPointsToTwips(NSTwipsToFloorIntPoints(nscoord(mFont->size * app2twip)))) / app2twip;
#if 0
  nsNativeWidget widget = nsnull;
  aContext->GetNativeWidget(widget);
  if( widget ) {
    BView *view = dynamic_cast<BView*>( (BView*)widget );
    view->GetFont( &mFontHandle );
  }

  firstFace.ToCString(wildstring, namelen);

#if 1 
   int32 numFamilies = count_font_families(); 
   for ( int32 i = 0; i < numFamilies; i++ ) { 
       font_family family; 
       uint32 flags; 
       if ( get_font_family(i, &family, &flags) == B_OK ) { 
           int32 numStyles = count_font_styles(family); 
           for ( int32 j = 0; j < numStyles; j++ ) { 
               font_style style; 
               if ( get_font_style(family, j, &style, &flags) 
                                                 == B_OK ) { 
               } 
           } 
       } 
   }
#endif

  if (aFont.style == NS_FONT_STYLE_ITALIC)
    face = B_ITALIC_FACE;

  if( aFont.weight > NS_FONT_WEIGHT_NORMAL )
  	face |= B_BOLD_FACE;
  	
  if( aFont.decorations & NS_FONT_DECORATION_UNDERLINE )
  	face |= B_UNDERSCORE_FACE;
  	
  if( aFont.decorations & NS_FONT_DECORATION_LINE_THROUGH )
  	face |= B_STRIKEOUT_FACE;
#endif
  	
  mFontHandle.SetFamilyAndFace( wildstring, face );
  mFontHandle.SetSize( rounded * app2dev );
 
#ifdef NOISY_FONTS
#ifdef DEBUG
  fprintf(stderr, "looking for font %s (%d)", wildstring, aFont.size / 20);
#endif
#endif

  RealizeFont(aContext);

  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::Destroy()
{
  return NS_OK;
}


void nsFontMetricsBeOS::RealizeFont(nsIDeviceContext* aContext)
{
  float f;
  aContext->GetDevUnitsToAppUnits(f);
  
  struct font_height height;
  mFontHandle.GetHeight( &height );

  mAscent = nscoord(height.ascent * f);
  mDescent = nscoord(height.descent * f);
  mMaxAscent = nscoord(height.ascent * f) ;
  mMaxDescent = nscoord(height.descent * f);
  
  mHeight = nscoord((height.ascent + height.descent) * f) ;
  mMaxAdvance = nscoord(mFontHandle.BoundingBox().Width() * f);

/*  PRUint32 i;

  for (i = 0; i < 256; i++)
  {
    if ((i < mFontInfo->min_char_or_byte2) || (i > mFontInfo->max_char_or_byte2))
      mCharWidths[i] = mMaxAdvance;
    else
      mCharWidths[i] = nscoord((mFontInfo->per_char[i - mFontInfo->min_char_or_byte2].width) * f);
  }
*/
  mLeading = nscoord(height.leading);
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetXHeight(nscoord& aResult)
{
//  aResult = mXHeight;
  aResult = nscoord( mMaxAscent / 2 );     // FIXME temporary code!
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetSuperscriptOffset(nscoord& aResult)
{
//  aResult = mSuperscriptOffset;
  aResult = nscoord( mMaxAscent / 2 );     // FIXME temporary code!
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetSubscriptOffset(nscoord& aResult)
{
//  aResult = mSubscriptOffset;
  aResult = nscoord( mMaxAscent / 2 );     // FIXME temporary code!
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
//  aOffset = mStrikeoutOffset;
//  aSize = mStrikeoutSize;
  aOffset = nscoord( ( mAscent / 2 ) - mDescent );
  aSize = nscoord( 20 );  // FIXME Put 1 pixel which equal 20 twips..
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetUnderline(nscoord& aOffset, nscoord& aSize)
{
//  aOffset = mUnderlineOffset;
//  aSize = mUnderlineSize;
  aOffset = nscoord( 0 ); // FIXME
  aSize = nscoord( 20 );  // FIXME
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetHeight(nscoord &aHeight)
{
  aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetMaxAscent(nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetMaxDescent(nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetMaxAdvance(nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetFont(const nsFont*& aFont)
{
  aFont = mFont;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetLangGroup(nsIAtom** aLangGroup)
{
  if (!aLangGroup) {
    return NS_ERROR_NULL_POINTER;
  }

  *aLangGroup = mLangGroup;
  NS_IF_ADDREF(*aLangGroup);

  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetFontHandle(nsFontHandle &aHandle)
{
  aHandle = (nsFontHandle)&mFontHandle;
  return NS_OK;
}

