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

#include "xp_core.h" 
#include "nsQuickSort.h" 
#include "nsFontMetricsBeOS.h" 
#include "nsIServiceManager.h" 
#include "nsICharsetConverterManager.h" 
#include "nsICharsetConverterManager2.h" 
#include "nsISaveAsCharset.h" 
#include "nsIPref.h" 
#include "nsCOMPtr.h" 
#include "nspr.h" 
#include "nsHashtable.h" 
 
#undef USER_DEFINED 
#define USER_DEFINED "x-user-def" 
 
#undef NOISY_FONTS
#undef REALLY_NOISY_FONTS

nsFontMetricsBeOS::nsFontMetricsBeOS()
{
  NS_INIT_REFCNT();
}

nsFontMetricsBeOS::~nsFontMetricsBeOS()
{
  if (nsnull != mFont) {
    delete mFont;
    mFont = nsnull;
  }
}
 
NS_IMPL_ISUPPORTS1(nsFontMetricsBeOS, nsIFontMetrics) 
 
static PRBool 
IsASCIIFontName(const nsString& aName) 
{ 
  PRUint32 len = aName.Length(); 
  const PRUnichar* str = aName.get(); 
  for (PRUint32 i = 0; i < len; i++) { 
    /* 
     * X font names are printable ASCII, ignore others (for now) 
     */ 
    if ((str[i] < 0x20) || (str[i] > 0x7E)) { 
      return PR_FALSE; 
    } 
  } 

  return PR_TRUE; 
} 

NS_IMETHODIMP nsFontMetricsBeOS::Init(const nsFont& aFont, nsIAtom* aLangGroup,
  nsIDeviceContext* aContext)
{
  NS_ASSERTION(!(nsnull == aContext), "attempt to init fontmetrics with null device context");

  nsAutoString  firstFace;
  mLangGroup = aLangGroup;
  if (NS_OK != aContext->FirstExistingFont(aFont, firstFace)) {
    aFont.GetFirstFamily(firstFace);
  }

//  PRInt32     namelen = MAX( firstFace.Length() + 1, sizeof(font_family) );
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
  fprintf(stderr, "looking for font %s (%d)", wildstring, aFont.size / app2twip);
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
 
  struct font_height emHeight; 
  be_plain_font->GetHeight(&emHeight); 
 
  int lineSpacing = nscoord(height.ascent + height.descent); 
  if (lineSpacing > (emHeight.ascent + emHeight.descent)) { 
    mLeading = nscoord((lineSpacing - (emHeight.ascent + emHeight.descent)) * f); 
  } 
  else { 
    mLeading = 0; 
  } 
  mEmHeight = PR_MAX(1, nscoord((emHeight.ascent + emHeight.descent) * f)); 
  mEmAscent = nscoord(height.ascent * (emHeight.ascent + emHeight.descent) * f / lineSpacing); 
  mEmDescent = mEmHeight - mEmAscent; 

  mMaxHeight = nscoord((height.ascent + 
                        height.descent) * f) ; 
  mMaxAscent = nscoord(height.ascent * f) ;
  mMaxDescent = nscoord(height.descent * f);
  
  mMaxAdvance = nscoord(mFontHandle.BoundingBox().Width() * f);

  // 56% of ascent, best guess for non-true type 
  mXHeight = NSToCoordRound((float) height.ascent* f * 0.56f); 

  float rawWidth = mFontHandle.StringWidth(" "); 
  mSpaceWidth = NSToCoordRound(rawWidth * f); 
 
/* Temp */ 
  mUnderlineOffset = -NSToIntRound(MAX (1, floor (0.1 * (height.ascent + height.descent + height.leading) + 0.5)) * f); 
  
  mUnderlineSize = NSToIntRound(MAX(1, floor (0.05 * (height.ascent + height.descent + height.leading) + 0.5)) * f); 
 
  mSuperscriptOffset = mXHeight; 
 
  mSubscriptOffset = mXHeight; 
 
  /* need better way to calculate this */ 
  mStrikeoutOffset = NSToCoordRound(mXHeight / 2.0); 
  mStrikeoutSize = mUnderlineSize; 
 
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetXHeight(nscoord& aResult)
{
  aResult = mXHeight;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetSuperscriptOffset(nscoord& aResult)
{
  aResult = mSuperscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetSubscriptOffset(nscoord& aResult)
{
  aResult = mSubscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mStrikeoutOffset; 
  aSize = mStrikeoutSize; 
//  aOffset = nscoord( ( mAscent / 2 ) - mDescent ); 
//  aSize = nscoord( 20 );  // FIXME Put 1 pixel which equal 20 twips.. 
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
  aHeight = mMaxHeight; 
  return NS_OK; 
} 
 
NS_IMETHODIMP  nsFontMetricsBeOS::GetNormalLineHeight(nscoord &aHeight) 
{
  aHeight = mEmHeight + mLeading; 
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsBeOS::GetEmHeight(nscoord &aHeight) 
{ 
  aHeight = mEmHeight; 
  return NS_OK; 
} 
 
NS_IMETHODIMP  nsFontMetricsBeOS::GetEmAscent(nscoord &aAscent) 
{ 
  aAscent = mEmAscent; 
  return NS_OK; 
} 
 
NS_IMETHODIMP  nsFontMetricsBeOS::GetEmDescent(nscoord &aDescent) 
{ 
  aDescent = mEmDescent; 
  return NS_OK; 
} 
 
NS_IMETHODIMP  nsFontMetricsBeOS::GetMaxHeight(nscoord &aHeight) 
{ 
  aHeight = mMaxHeight; 
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
 
nsresult 
nsFontMetricsBeOS::FamilyExists(const nsString& aName) 
{ 
  if (!IsASCIIFontName(aName)) { 
    return NS_ERROR_FAILURE; 
  } 
 
  nsCAutoString name; 
  name.AssignWithConversion(aName.get()); 
  name.ToLowerCase(); 
  PRBool  isthere = PR_FALSE; 
 
  char* cStr = name.ToNewCString(); 
 
       int32 numFamilies = count_font_families(); 
       for(int32 i = 0; i < numFamilies; i++) 
       { 
               font_family family; 
               uint32 flags; 
               if(get_font_family(i, &family, &flags) == B_OK) 
               { 
                       if(strcmp(family, cStr) == 0) 
                       { 
                               isthere = PR_TRUE; 
                               break; 
                       } 
               } 
       } 
 
       //printf("%s there? %s\n", cStr, isthere?"Yes":"No" ); 
       
  delete[] cStr; 
 
  if (PR_TRUE == isthere) 
    return NS_OK; 
  else 
    return NS_ERROR_FAILURE; 
} 
 
nsresult 
nsFontMetricsBeOS::GetSpaceWidth(nscoord &aSpaceWidth) 
{ 
  aSpaceWidth = mSpaceWidth; 
  return NS_OK; 
} 
 
// The Font Enumerator 
 
nsFontEnumeratorBeOS::nsFontEnumeratorBeOS() 
{ 
  NS_INIT_REFCNT(); 
} 
 
NS_IMPL_ISUPPORTS1(nsFontEnumeratorBeOS, nsIFontEnumerator)
 
static int 
CompareFontNames(const void* aArg1, const void* aArg2, void* aClosure) 
{ 
  const PRUnichar* str1 = *((const PRUnichar**) aArg1); 
  const PRUnichar* str2 = *((const PRUnichar**) aArg2); 
 
  // XXX add nsICollation stuff 
 
  return nsCRT::strcmp(str1, str2); 
} 
 
static nsresult 
EnumFonts(nsIAtom* aLangGroup, const char* aGeneric, PRUint32* aCount, 
  PRUnichar*** aResult) 
{ 
  nsString font_name; 
    
  int32 numFamilies = count_font_families(); 
 
  PRUnichar** array = 
    (PRUnichar**) nsMemory::Alloc(numFamilies * sizeof(PRUnichar*)); 
  if (!array) { 
    return NS_ERROR_OUT_OF_MEMORY; 
  } 
 
       for(int32 i = 0; i < numFamilies; i++) 
       { 
               font_family family; 
               uint32 flags; 
               if(get_font_family(i, &family, &flags) == B_OK) 
               { 
                       font_name.AssignWithConversion(family); 
                       array[i] = font_name.ToNewUnicode(); 
               } 
       } 
 
  NS_QuickSort(array, numFamilies, sizeof(PRUnichar*), CompareFontNames, 
               nsnull); 
 
  *aCount = numFamilies; 
  if (*aCount) { 
    *aResult = array; 
  } 
  else { 
    nsMemory::Free(array); 
  } 
 
  return NS_OK; 
} 
 
NS_IMETHODIMP 
nsFontEnumeratorBeOS::EnumerateAllFonts(PRUint32* aCount, PRUnichar*** aResult) 
{ 
  NS_ENSURE_ARG_POINTER(aResult); 
  *aResult = nsnull; 
  NS_ENSURE_ARG_POINTER(aCount); 
  *aCount = 0; 
 
  return EnumFonts(nsnull, nsnull, aCount, aResult); 
} 
 
NS_IMETHODIMP 
nsFontEnumeratorBeOS::EnumerateFonts(const char* aLangGroup, 
  const char* aGeneric, PRUint32* aCount, PRUnichar*** aResult) 
{ 
  NS_ENSURE_ARG_POINTER(aResult); 
  *aResult = nsnull; 
  NS_ENSURE_ARG_POINTER(aCount); 
  *aCount = 0; 
  NS_ENSURE_ARG_POINTER(aGeneric); 
  NS_ENSURE_ARG_POINTER(aLangGroup); 
 
  nsCOMPtr<nsIAtom> langGroup = getter_AddRefs(NS_NewAtom(aLangGroup)); 
 
  // XXX still need to implement aLangGroup and aGeneric 
  return EnumFonts(langGroup, aGeneric, aCount, aResult); 
}

NS_IMETHODIMP
nsFontEnumeratorBeOS::HaveFontFor(const char* aLangGroup, PRBool* aResult)
{
  NS_ENSURE_ARG_POINTER(aLangGroup); 
  NS_ENSURE_ARG_POINTER(aResult); 
  *aResult = PR_FALSE; 
  // XXX stub
  return NS_OK;
}

NS_IMETHODIMP
nsFontEnumeratorBeOS::UpdateFontList(PRBool *updateFontList)
{
  *updateFontList = PR_FALSE; // always return false for now
  return NS_OK;
}

