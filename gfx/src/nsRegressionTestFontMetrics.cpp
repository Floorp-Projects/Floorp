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

#include "nsRegressionTestFontMetrics.h"

#define MAPPING_FACTOR_FOR_SPACE 0.02f
#define MAPPING_FACTOR_FOR_LOWER_CASE  0.50f
#define MAPPING_FACTOR_FOR_OTHERS  0.70f


nsresult
NS_NewRegressionTestFontMetrics(nsIFontMetrics** aMetrics)
{
  if (aMetrics == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  nsRegressionTestFontMetrics  *fm = new nsRegressionTestFontMetrics();

  if (nsnull == fm) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(fm, aMetrics);
}

nsRegressionTestFontMetrics:: nsRegressionTestFontMetrics()
{
  NS_INIT_REFCNT(); 
  mFont = nsnull; 
  mDeviceContext = nsnull;
  
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
  
NS_IMPL_ISUPPORTS1(nsRegressionTestFontMetrics, nsIFontMetrics)

nsRegressionTestFontMetrics::~nsRegressionTestFontMetrics()
{
  if (nsnull != mFont)
  {
    delete mFont;
    mFont = nsnull;
  }
  mDeviceContext = nsnull;
}


NS_IMETHODIMP
nsRegressionTestFontMetrics::Init(const nsFont& aFont, nsIDeviceContext *aContext)
{
  mFont = new nsFont(aFont);
  mDeviceContext = aContext;
  RealizeFont();
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTestFontMetrics::Destroy()
{
  mDeviceContext = nsnull;
  return NS_OK;
}

void
nsRegressionTestFontMetrics::RealizeFont()
{
  float dev2app;
  mDeviceContext->GetDevUnitsToAppUnits(dev2app);
  nscoord onepixel = NSToCoordRound(1 * dev2app);
  PRUint32 fontsize = mFont->size;
 
  // Most of the numbers are just made up....
  // feel free to play around.

  mHeight = fontsize;
  mXHeight = NSToCoordRound((float)mHeight * 0.50f);
  mSuperscriptOffset = mXHeight;
  mSubscriptOffset = mXHeight;
  mStrikeoutSize = onepixel;
  mStrikeoutOffset = NSToCoordRound(mXHeight / 3.5f);

  
  mAscent = NSToCoordRound((float)fontsize * 0.60f);
  mDescent = NSToCoordRound((float)fontsize * 0.20f);

  mLeading = NSToCoordRound((float)fontsize * 0.10f);
  mMaxAscent = mAscent;
  mMaxDescent = mDescent;
  mMaxAdvance = NSToCoordRound((float)fontsize * 0.80f);
  mUnderlineSize = onepixel;
  mUnderlineOffset =  NSToCoordRound(fontsize / 100.0f);
  
}


/**
  * This routine determines a character width without relying on the
  * underlying OS.
  * 
  * @update  harishd 02/07/1999
  * @param   aChar  -- A valid character whose width needs to be computed
  * @param   aWidth -- Holds the computed character width
  */

NS_METHOD
nsRegressionTestFontMetrics::GetWidth(const char aChar, nscoord& aWidth)
{
  float size = (float)mFont->size;
  aWidth = 0;

  if(aChar == ' ')
    size *= MAPPING_FACTOR_FOR_SPACE;
  else if(aChar >= 'a' && aChar <= 'z')
    size *= MAPPING_FACTOR_FOR_LOWER_CASE;
  else
    size *= MAPPING_FACTOR_FOR_OTHERS;
    
  aWidth = NSToCoordRound(size);

  return NS_OK;
}

/**
  * This routine determines a character width without relying on the
  * underlying OS.
  * 
  * @update  harishd 02/07/1999
  * @param   aChar  -- A valid character whose width needs to be computed
  * @param   aWidth -- Holds the computed character width
  */

NS_METHOD
nsRegressionTestFontMetrics::GetWidth(const PRUnichar aChar,nscoord& aWidth)
{
  float size = (float)mFont->size;
  aWidth = 0;

  if(aChar == ' ')
    size *= MAPPING_FACTOR_FOR_SPACE;
  else if(aChar >= 'a' && aChar <= 'z')
    size *= MAPPING_FACTOR_FOR_LOWER_CASE;
  else 
    size *= MAPPING_FACTOR_FOR_OTHERS;
      
  aWidth = NSToCoordRound(size);

  return NS_OK;
}

/**
  * This routine determines width of a string without relying on the
  * underlying OS.
  * 
  * @update  harishd 02/07/1999
  * @param   aString -- A string whose width needs to be computed
  * @param   aLength -- Length of the string
  * @param   aWidth  -- Holds the computed string width
  *
  * @return  error code -- 0 if success, non-zero if error
  */

NS_METHOD
nsRegressionTestFontMetrics::GetWidth(const PRUnichar* aString, PRUint32 aLength, nscoord& aWidth)
{
  aWidth = 0;
  
  if(aString == nsnull)
    return NS_ERROR_NULL_POINTER;
  
  float size;
  float totalsize = 0;
  
  for(PRUint32 index = 0; index < aLength; index++){
    size = (float)mFont->size;
    if(aString[index] == ' ')
      size *= MAPPING_FACTOR_FOR_SPACE;
    else if(aString[index] >= 'a' && aString[index] <= 'z')
      size *= MAPPING_FACTOR_FOR_LOWER_CASE;
    else 
      size *= MAPPING_FACTOR_FOR_OTHERS;
    totalsize += size;    
  }
  aWidth = NSToCoordRound(totalsize);
  return NS_OK;
}

/**
  * This routine determines width of a string without relying on the
  * underlying OS.
  * 
  * @update  harishd 02/07/1999
  * @param   aString -- A string whose width needs to be computed
  * @param   aLength -- Length of the string
  * @param   aWidth  -- Holds the computed string width
  *
  * @return  error code -- 0 if success, non-zero if error
  */

NS_METHOD
nsRegressionTestFontMetrics::GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth)
{
  aWidth = 0;

  if(aString == nsnull)
    return NS_ERROR_NULL_POINTER;
  
  float size;
  float totalsize = 0;
 
  for(PRUint32 index=0; index < aLength; index++){
    size = (float)mFont->size;
    if(aString[index] == ' ')
      size *= MAPPING_FACTOR_FOR_SPACE;
    else if(aString[index] >= 'a' && aString[index] <= 'z')
      size *= MAPPING_FACTOR_FOR_LOWER_CASE;
    else
      size *= MAPPING_FACTOR_FOR_OTHERS;
    totalsize += size;    
  }
  aWidth += NSToCoordRound(totalsize);
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTestFontMetrics::GetXHeight(nscoord& aResult)
{
  aResult = mXHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTestFontMetrics::GetSuperscriptOffset(nscoord& aResult)
{
  aResult = mSuperscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTestFontMetrics::GetSubscriptOffset(nscoord& aResult)
{
  aResult = mSubscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTestFontMetrics::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mStrikeoutOffset;
  aSize = mStrikeoutSize;
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTestFontMetrics::GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mUnderlineOffset;
  aSize = mUnderlineSize;
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTestFontMetrics::GetHeight(nscoord &aHeight)
{
  aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTestFontMetrics::GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTestFontMetrics::GetMaxAscent(nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTestFontMetrics::GetMaxDescent(nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTestFontMetrics::GetMaxAdvance(nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTestFontMetrics::GetFont(const nsFont *&aFont)
{
  aFont = mFont;
  return NS_OK;
}
NS_IMETHODIMP
nsRegressionTestFontMetrics::GetFontHandle(nsFontHandle &aHandle)
{
  //We don't have a font handler
  aHandle = NULL;
  return NS_OK;
}

