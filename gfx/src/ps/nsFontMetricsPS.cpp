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
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#include "nsFontMetricsPS.h"
#include "nsDeviceContextPS.h"
#include "nsRenderingContextPS.h"
#include "nsIServiceManager.h"
#include "nsGfxCIID.h"

#include "nsIPref.h"
#include "nsVoidArray.h"
#include "nsReadableUtils.h"
#ifdef MOZ_ENABLE_FREETYPE2
#include "nsType8.h"
#endif

#define NS_IS_BOLD(weight) ((weight) > 400 ? 1 : 0)

static NS_DEFINE_CID(kFCSCID, NS_FONTCATALOGSERVICE_CID);
/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
nsFontMetricsPS :: nsFontMetricsPS()
{
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
nsFontMetricsPS :: ~nsFontMetricsPS()
{
  if (nsnull != mFont){
    delete mFont;
    mFont = nsnull;
  }
  
  if (mFontPS) {
    delete mFontPS;
    mFontPS = nsnull;
  }

  if (mDeviceContext) {
    // Notify our device context that owns us so that it can update its font cache
    mDeviceContext->FontMetricsDeleted(this);
    mDeviceContext = nsnull;
  }
}

NS_IMPL_ISUPPORTS1(nsFontMetricsPS, nsIFontMetrics)

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: Init(const nsFont& aFont, nsIAtom* aLangGroup,
  nsIDeviceContext *aContext)
{
  mLangGroup = aLangGroup;

  mFont = new nsFont(aFont);
  //don't addref this to avoid circular refs
  mDeviceContext = (nsDeviceContextPS *)aContext;

  mFontPS = nsFontPS::FindFont(aFont, this);
  NS_ENSURE_TRUE(mFontPS, NS_ERROR_FAILURE);

  RealizeFont();
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: Destroy()
{
  mDeviceContext = nsnull;
  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
void
nsFontMetricsPS::RealizeFont()
{
  if (mFont && mDeviceContext) {
    float dev2app;
    mDeviceContext->GetDevUnitsToAppUnits(dev2app);
    mFontPS->RealizeFont(this, dev2app);
  }
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetXHeight(nscoord& aResult)
{
  aResult = mXHeight;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetSuperscriptOffset(nscoord& aResult)
{
  aResult = mSuperscriptOffset;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetSubscriptOffset(nscoord& aResult)
{
  aResult = mSubscriptOffset;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mStrikeoutOffset;
  aSize = mStrikeoutSize;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mUnderlineOffset;
  aSize = mUnderlineSize;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetHeight(nscoord &aHeight)
{
  aHeight = mHeight;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetNormalLineHeight(nscoord &aHeight)
{
  aHeight = mEmHeight + mLeading;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetEmHeight(nscoord &aHeight)
{
  aHeight = mEmHeight;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetEmAscent(nscoord &aAscent)
{
  aAscent = mEmAscent;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetEmDescent(nscoord &aDescent)
{
  aDescent = mEmDescent;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetMaxHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetMaxAscent(nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetMaxDescent(nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetMaxAdvance(nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: GetAveCharWidth(nscoord &aAveCharWidth)
{
  aAveCharWidth = mAveCharWidth;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetSpaceWidth(nscoord &aSpaceWidth)
{
  aSpaceWidth = mSpaceWidth;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetFont(const nsFont *&aFont)
{
  aFont = mFont;
  return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsPS :: GetLangGroup(nsIAtom** aLangGroup)
{
  if (!aLangGroup) {
    return NS_ERROR_NULL_POINTER;
  }

  *aLangGroup = mLangGroup;
  NS_IF_ADDREF(*aLangGroup);

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS::GetFontHandle(nsFontHandle &aHandle)
{

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetStringWidth(const char *aString,nscoord& aWidth,nscoord aLength)
{
  NS_ENSURE_TRUE(mFontPS, NS_ERROR_NULL_POINTER);
  aWidth = mFontPS->GetWidth(aString, aLength);
  return NS_OK;

}


/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetStringWidth(const PRUnichar *aString,nscoord& aWidth,nscoord aLength)
{
  NS_ENSURE_TRUE(mFontPS, NS_ERROR_NULL_POINTER);
  aWidth = mFontPS->GetWidth(aString, aLength);
  return NS_OK;
}

// nsFontPS
nsFontPS*
nsFontPS::FindFont(const nsFont& aFont, nsFontMetricsPS* aFontMetrics)
{
  nsFontPS* fontPS;

#ifdef MOZ_ENABLE_FREETYPE2
  nsDeviceContextPS* dc = aFontMetrics->GetDeviceContext();
  NS_ENSURE_TRUE(dc, nsnull);
  if (dc->mTTPEnable) {
    fontPS = nsFontPSFreeType::FindFont(aFont, aFontMetrics);
    if (fontPS)
      return fontPS;
  }
#endif

  /* Find in afm font */
  fontPS = nsFontPSAFM::FindFont(aFont, aFontMetrics);
  return fontPS;
}

nsFontPS::nsFontPS(const nsFont& aFont, nsFontMetricsPS* aFontMetrics)
{
  mFont = new nsFont(aFont);
  if (!mFont) return;
  mFontMetrics = aFontMetrics;
}

nsFontPS::~nsFontPS()
{
  if (mFont) {
    delete mFont;
    mFont = nsnull;
  }

  if (mCCMap) {
    FreeCCMap(mCCMap);
  }

  mFontMetrics = nsnull;
}

// nsFontPSAFM
nsFontPS*
nsFontPSAFM::FindFont(const nsFont& aFont, nsFontMetricsPS* aFontMetrics)
{
  nsAFMObject* afmInfo = new nsAFMObject();
  if (!afmInfo) return nsnull;
  afmInfo->Init((PRInt32)(aFont.size / TWIPS_PER_POINT_FLOAT));

  PRInt16 fontIndex = afmInfo->CheckBasicFonts(aFont, PR_TRUE);
  if (fontIndex < 0) {
    if (!(afmInfo->AFM_ReadFile(aFont))) {
      fontIndex = afmInfo->CheckBasicFonts(aFont, PR_FALSE);
      if (fontIndex < 0) {
        fontIndex = afmInfo->CreateSubstituteFont(aFont);
      }
    }
  }

  nsFontPSAFM* fontPSAFM = nsnull;
  if (fontIndex < 0)
    delete afmInfo;
  else
    fontPSAFM = new nsFontPSAFM(aFont, afmInfo, fontIndex, aFontMetrics);
  return fontPSAFM;
}

nsFontPSAFM::nsFontPSAFM(const nsFont& aFont, nsAFMObject* aAFMInfo,
                         PRInt16 fontIndex, nsFontMetricsPS* aFontMetrics) :
nsFontPS(aFont, aFontMetrics), mAFMInfo(aAFMInfo), mFontIndex(fontIndex)
{
  if (!(mFont && mAFMInfo)) return;
  mFamilyName.AssignWithConversion((char*)mAFMInfo->mPSFontInfo->mFamilyName);
}

nsFontPSAFM::~nsFontPSAFM()
{
  if (mAFMInfo) {
    delete mAFMInfo;
    mAFMInfo = nsnull;
  }
}

nscoord
nsFontPSAFM::GetWidth(const char* aString, PRUint32 aLength)
{
  nscoord width;
  if (mAFMInfo) {
    mAFMInfo->GetStringWidth(aString, width, aLength);
  }
  return width;
}

nscoord
nsFontPSAFM::GetWidth(const PRUnichar* aString, PRUint32 aLength)
{
  nscoord width;
  if (mAFMInfo) {
    mAFMInfo->GetStringWidth(aString, width, aLength);
  }
  return width;
}

nscoord
nsFontPSAFM::DrawString(nsRenderingContextPS* aContext,
                        nscoord aX, nscoord aY,
                        const char* aString, PRUint32 aLength)
{
  NS_ENSURE_TRUE(aContext, 0);
  nsPostScriptObj* psObj = aContext->GetPostScriptObj();
  NS_ENSURE_TRUE(psObj, 0);

  psObj->moveto(aX, aY);
  psObj->show(aString, aLength, "");
  return GetWidth(aString, aLength);
}

nscoord
nsFontPSAFM::DrawString(nsRenderingContextPS* aContext,
                        nscoord aX, nscoord aY,
                        const PRUnichar* aString, PRUint32 aLength)
{
  NS_ENSURE_TRUE(aContext, 0);
  nsPostScriptObj* psObj = aContext->GetPostScriptObj();
  NS_ENSURE_TRUE(psObj, 0);

  psObj->moveto(aX, aY);
  psObj->show(aString, aLength, "", 0);
  return GetWidth(aString, aLength);
}

nsresult
nsFontPSAFM::RealizeFont(nsFontMetricsPS* aFontMetrics, float dev2app)
{
  NS_ENSURE_ARG_POINTER(aFontMetrics);

  float fontSize;
  float offset;

  nscoord onePixel = NSToCoordRound(1 * dev2app);

  // convert the font size which is in twips to points
  fontSize = mFont->size / TWIPS_PER_POINT_FLOAT;

  offset = NSFloatPointsToTwips(fontSize * mAFMInfo->mPSFontInfo->mXHeight) / 1000.0f;
  nscoord xHeight = NSToCoordRound(offset);
  aFontMetrics->SetXHeight(xHeight);
  aFontMetrics->SetSuperscriptOffset(xHeight);
  aFontMetrics->SetSubscriptOffset(xHeight);
  aFontMetrics->SetStrikeout((nscoord)(xHeight / TWIPS_PER_POINT_FLOAT), onePixel);

  offset = NSFloatPointsToTwips(fontSize * mAFMInfo->mPSFontInfo->mUnderlinePosition) / 1000.0f;
  aFontMetrics->SetUnderline(NSToCoordRound(offset), onePixel);

  nscoord size = NSToCoordRound(fontSize * dev2app);
  aFontMetrics->SetHeight(size);
  aFontMetrics->SetEmHeight(size);
  aFontMetrics->SetMaxAdvance(size);
  aFontMetrics->SetMaxHeight(size);

  offset = NSFloatPointsToTwips(fontSize * mAFMInfo->mPSFontInfo->mAscender) / 1000.0f;
  nscoord ascent = NSToCoordRound(offset);
  aFontMetrics->SetAscent(ascent);
  aFontMetrics->SetEmAscent(ascent);
  aFontMetrics->SetMaxAscent(ascent);

  offset = NSFloatPointsToTwips(fontSize * mAFMInfo->mPSFontInfo->mDescender) / 1000.0f;
  nscoord descent = -(NSToCoordRound(offset));
  aFontMetrics->SetDescent(descent);
  aFontMetrics->SetEmDescent(descent);
  aFontMetrics->SetMaxDescent(descent);

  aFontMetrics->SetLeading(0);

  nscoord spaceWidth = GetWidth(" ", 1);
  aFontMetrics->SetSpaceWidth(spaceWidth);

  nscoord aveCharWidth = GetWidth("x", 1);
  aFontMetrics->SetAveCharWidth(aveCharWidth);

  return NS_OK;
}

nsresult
nsFontPSAFM::SetupFont(nsRenderingContextPS* aContext)
{
  NS_ENSURE_TRUE(aContext && mFontMetrics, 0);
  nsPostScriptObj* psObj = aContext->GetPostScriptObj();
  NS_ENSURE_TRUE(psObj, 0);

  nscoord fontHeight = 0;
  mFontMetrics->GetHeight(fontHeight);

  psObj->setscriptfont(mFontIndex, mFamilyName,
                       fontHeight, mFont->style, mFont->variant,
                       mFont->weight, mFont->decorations);
  return NS_OK;
}

#ifdef MOZ_MATHML
nsresult
nsFontPSAFM::GetBoundingMetrics(const char*        aString,
                                PRUint32           aLength,
                                nsBoundingMetrics& aBoundingMetrics)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFontPSAFM::GetBoundingMetrics(const PRUnichar*   aString,
                                PRUint32           aLength,
                                nsBoundingMetrics& aBoundingMetrics)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

#ifdef MOZ_ENABLE_FREETYPE2

#define WIDEN_8_TO_16_BUF_SIZE 1024
nsFontPS*
nsFontPSFreeType::FindFont(const nsFont& aFont, nsFontMetricsPS* aFontMetrics)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIAtom> lang;
  aFontMetrics->GetLangGroup(getter_AddRefs(lang));
  NS_ENSURE_TRUE(lang, nsnull);

  nsCOMPtr<nsITrueTypeFontCatalogEntry> entry;
  rv = FindFontEntry(aFont, lang, getter_AddRefs(entry));
  NS_ENSURE_SUCCESS(rv, nsnull);
  NS_ENSURE_TRUE(entry, nsnull);

  nsDeviceContextPS* dc = aFontMetrics->GetDeviceContext();
  NS_ENSURE_TRUE(dc, nsnull);

  nsCAutoString familyName, styleName;
  entry->GetFamilyName(familyName);
  entry->GetStyleName(styleName);
  ToLowerCase(familyName);
  ToLowerCase(styleName);
  
  nsCAutoString fontName;
  fontName.Append(familyName);
  fontName.Append("-");
  fontName.Append(styleName);
  nsCStringKey key(fontName);

  nsHashtable *psFGList = dc->GetPSFontGeneratorList();
  NS_ENSURE_TRUE(psFGList, nsnull);
  
  nsPSFontGenerator* psFontGen = (nsPSFontGenerator*) psFGList->Get(&key);
  if (!psFontGen) {
    psFontGen = new nsFT2Type8Generator;
    NS_ENSURE_TRUE(psFontGen, nsnull);
    rv = ((nsFT2Type8Generator*)psFontGen)->Init(entry);
    if (NS_FAILED(rv)) {
      delete psFontGen;
      return nsnull;
    }
    psFGList->Put(&key, (void *) psFontGen);
  }
  nsFontPSFreeType* fontPS = new nsFontPSFreeType(aFont, aFontMetrics);
  NS_ENSURE_TRUE(fontPS, nsnull);
  rv = fontPS->Init(entry, psFontGen);
  if (NS_FAILED(rv)) {
    delete fontPS;
    return nsnull;
  }
  return (nsFontPS*)fontPS;
}

typedef struct {
  nsVoidArray *fontNames;
  nsVoidArray *isGeneric;
} font_enum_info;

static PRBool PR_CALLBACK
GenericFontEnumCallback(const nsString& aFamily, PRBool aGeneric, void* aData)
{
  font_enum_info* fei = NS_STATIC_CAST(font_enum_info*, aData);
  char* name = ToNewCString(aFamily);
  if (name) {
    fei->fontNames->AppendElement(name);
    fei->isGeneric->AppendElement((void*)aGeneric);
    return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult
nsFontPSFreeType::FindFontEntry(const nsFont& aFont, nsIAtom* aLanguage,
                                nsITrueTypeFontCatalogEntry** aEntry)
{
  font_enum_info fei;
  
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRUint16 weight = NS_IS_BOLD(aFont.weight)
                          ? (PRUint16)nsIFontCatalogService::kFCWeightBold
                          : (PRUint16)nsIFontCatalogService::kFCWeightMedium;
  PRUint16 slant = aFont.style;

  nsVoidArray fontNames;
  nsVoidArray isGeneric;
  fei.fontNames = &fontNames;
  fei.isGeneric = &isGeneric;

  // ignore return value
  aFont.EnumerateFamilies(GenericFontEnumCallback, &fei);
  
  const PRUnichar* langStr = nsnull;
  aLanguage->GetUnicode(&langStr);
  nsCAutoString language;
  language.AppendWithConversion(langStr);
 
  *aEntry = nsnull;
  PRInt32 i;
  nsCAutoString emptyStr;
  for (i = 0; i<fontNames.Count(); i++) {
    nsCAutoString fontName;
    void *tmp = isGeneric[i];
    PRBool isGeneric =  (PRBool)tmp;
    if (isGeneric) {
      //lookup the font pref
      nsXPIDLCString value;
      nsCAutoString fontNameKey("font.name.");
      fontNameKey.Append((char *)fontNames[i]);
      fontNameKey.Append(char('.'));
      fontNameKey.Append(language);
      pref->GetCharPref(fontNameKey.get(), getter_Copies(value));
      free(fontNames[i]);
      if (!value.get())
        continue;
      // strip down to just the family name
      PRUint32 startFamily = value.FindChar('-') + 1;
      PRUint32 endFamily = value.FindChar('-', startFamily + 1);
      fontName.Append(Substring(value, startFamily, endFamily - startFamily));
    }
    else {
      fontName.Append((char *)fontNames[i]);
    }
    ToLowerCase(fontName);
    
    rv = GetFontEntry(fontName, language, weight, 0, slant, 0, aEntry);
    NS_ENSURE_SUCCESS(rv, rv);
    if (*aEntry) return rv;

    rv = GetFontEntry(fontName, emptyStr, weight, 0, slant, 0, aEntry);
    NS_ENSURE_SUCCESS(rv, rv);
    if (*aEntry) return rv;

    rv = GetFontEntry(fontName, emptyStr, 0, 0, 0, 0, aEntry);
    NS_ENSURE_SUCCESS(rv, rv);
    if (*aEntry) return rv;
  }

  rv = GetFontEntry(emptyStr, language, weight, 0, slant, 0, aEntry);
  NS_ENSURE_SUCCESS(rv, rv);
  if (*aEntry) return rv;
  
  rv = GetFontEntry(emptyStr, language, 0, 0, 0, 0, aEntry);
  NS_ENSURE_SUCCESS(rv, rv);
  if (*aEntry) return rv;

  return rv;
}

nsresult
nsFontPSFreeType::GetFontEntry(nsACString& aFamilyName, nsACString& aLanguage,
                               PRUint16 aWeight, PRUint16 aWidth,
                               PRUint16 aSlant, PRUint16 aSpacing,
                               nsITrueTypeFontCatalogEntry** aEntry)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIFontCatalogService> fcs(do_GetService(kFCSCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsArray> entryList = nsnull;
  rv = fcs->GetFontCatalogEntries(aFamilyName, aLanguage,
                                  aWeight, aWidth, aSlant, aSpacing,
                                  getter_AddRefs(entryList));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 count = 0;
  NS_ENSURE_TRUE(entryList, NS_ERROR_FAILURE);

  rv = entryList->Count(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  *aEntry = nsnull;
  if (count > 0) {
    nsCOMPtr<nsISupports> item;
    rv = entryList->GetElementAt(0, getter_AddRefs(item));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsITrueTypeFontCatalogEntry> entry(do_QueryInterface(item, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    *aEntry = entry;
    NS_IF_ADDREF(*aEntry);
  }

  return rv;
}

nsFontPSFreeType::nsFontPSFreeType(const nsFont& aFont,
                                   nsFontMetricsPS* aFontMetrics)
  :nsFontPS(aFont, aFontMetrics)
{
}

nsresult
nsFontPSFreeType::Init(nsITrueTypeFontCatalogEntry* aEntry,
                       nsPSFontGenerator* aPSFontGen)
{
  NS_ENSURE_TRUE(mFont && mFontMetrics, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(aEntry && aPSFontGen, NS_ERROR_FAILURE);
  mEntry = aEntry;
  mPSFontGenerator = aPSFontGen;

  float app2dev;
  nsIDeviceContext* dc = mFontMetrics->GetDeviceContext();
  NS_ENSURE_TRUE(dc, NS_ERROR_NULL_POINTER);
  dc->GetAppUnitsToDevUnits(app2dev);
  
  mPixelSize = NSToIntRound(app2dev * mFont->size);

  mImageDesc.font.face_id    = (void*)mEntry;
  mImageDesc.font.pix_width  = mPixelSize;
  mImageDesc.font.pix_height = mPixelSize;
  mImageDesc.image_type = 0;

  nsresult rv;
  mFt2 = do_GetService(NS_FREETYPE2_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsFontPSFreeType::~nsFontPSFreeType()
{
  mEntry = nsnull;
}

nscoord
nsFontPSFreeType::GetWidth(const char* aString, PRUint32 aLength)
{
  PRUnichar unichars[WIDEN_8_TO_16_BUF_SIZE];
  PRUint32 len, length = 0;
  while ( aLength > 0 ) {
    len = PR_MIN(aLength, WIDEN_8_TO_16_BUF_SIZE);
    for (PRUint32 i=0; i < len; i++) {
      unichars[i] = (PRUnichar)((unsigned char)aString[i]);
    }
    length += GetWidth(unichars, len);
    aString += len;
    aLength -= len;
  }
  return length;
}


nscoord
nsFontPSFreeType::GetWidth(const PRUnichar* aString, PRUint32 aLength)
{
  FT_UInt glyph_index;
  FT_Glyph glyph;
  FT_Pos origin_x = 0;

  // get the face/size from the FreeType cache
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return 0;

  FTC_Image_Cache iCache;
  nsresult rv = mFt2->GetImageCache(&iCache);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to get Image Cache");
    return 0;
  }

  for (PRUint32 i=0; i<aLength; i++) {
    mFt2->GetCharIndex((FT_Face)face, aString[i], &glyph_index);
    nsresult rv = mFt2->ImageCacheLookup(iCache, &mImageDesc,
                                         glyph_index, &glyph);
    if (NS_FAILED(rv)) {
      origin_x += face->size->metrics.x_ppem/2 + 2;
      continue;
    }
    origin_x += FT_16_16_TO_REG(glyph->advance.x);
  }

  NS_ENSURE_TRUE(mFontMetrics, 0);

  nsDeviceContextPS* dc = mFontMetrics->GetDeviceContext();
  NS_ENSURE_TRUE(dc, 0);

  float dev2app;
  dc->GetDevUnitsToAppUnits(dev2app);

  return NSToCoordRound(origin_x * dev2app);
}

FT_Face
nsFontPSFreeType::getFTFace()
{
  FT_Face face = nsnull;
  
  FTC_Manager cManager;
  mFt2->GetFTCacheManager(&cManager);
  nsresult rv = mFt2->ManagerLookupSize(cManager, &mImageDesc.font,
                                        &face, nsnull);
  NS_ASSERTION(rv==0, "failed to get face/size");
  if (rv)
    return nsnull;
  return face;
}

nscoord
nsFontPSFreeType::DrawString(nsRenderingContextPS* aContext,
                             nscoord aX, nscoord aY,
                             const char* aString, PRUint32 aLength)
{
  NS_ENSURE_TRUE(aContext, 0);
  nsPostScriptObj* psObj = aContext->GetPostScriptObj();
  NS_ENSURE_TRUE(psObj, 0);
  
  psObj->moveto(aX, aY);

  PRUnichar unichars[WIDEN_8_TO_16_BUF_SIZE];
  PRUint32 len;
  
  while ( aLength > 0 ) {
    len = PR_MIN(aLength, WIDEN_8_TO_16_BUF_SIZE);
    for (PRUint32 i=0; i < len; i++) {
      unichars[i] = (PRUnichar)((unsigned char)aString[i]);
    }
    psObj->show(unichars, len, "", 1);
    mPSFontGenerator->AddToSubset(unichars, len);
    aString += len;
    aLength -= len;
  }
  return GetWidth(aString, aLength);
}

nscoord
nsFontPSFreeType::DrawString(nsRenderingContextPS* aContext,
                             nscoord aX, nscoord aY,
                             const PRUnichar* aString, PRUint32 aLength)
{
  NS_ENSURE_TRUE(aContext, 0);
  nsPostScriptObj* psObj = aContext->GetPostScriptObj();
  NS_ENSURE_TRUE(psObj, 0);

  psObj->moveto(aX, aY);
  psObj->show(aString, aLength, "", 1);
  
  mPSFontGenerator->AddToSubset(aString, aLength);
  return GetWidth(aString, aLength);
}

int
nsFontPSFreeType::ascent()
{
  int ascent;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  NS_ENSURE_TRUE(face, 0);
  ascent = FT_16_16_TO_REG(face->ascender * face->size->metrics.y_scale);
  ascent = FT_CEIL(ascent);
  ascent = FT_TRUNC(ascent);
  return ascent;
}

int
nsFontPSFreeType::descent()
{
  int descent;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  NS_ENSURE_TRUE(face, 0);
  descent = FT_16_16_TO_REG(-face->descender * face->size->metrics.y_scale);
  descent = FT_CEIL(descent);
  descent = FT_TRUNC(descent);
  return descent;
}

int
nsFontPSFreeType::max_ascent()
{
  int max_ascent;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  NS_ENSURE_TRUE(face, 0);
  void * p;
  mFt2->GetSfntTable(face, ft_sfnt_os2, &p);
  TT_OS2 * tt_os2 = (TT_OS2 *)p;
  NS_ASSERTION(tt_os2, "unable to get OS2 table");
  if (tt_os2)
    max_ascent = FT_16_16_TO_REG(tt_os2->sTypoAscender
                                 * face->size->metrics.y_scale);
  else
    max_ascent = FT_16_16_TO_REG(face->bbox.yMax * face->size->metrics.y_scale);
  max_ascent = FT_CEIL(max_ascent);
  max_ascent = FT_TRUNC(max_ascent);
  return max_ascent;
}

int
nsFontPSFreeType::max_descent()
{
  int max_descent;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  NS_ENSURE_TRUE(face, 0);
  void *p;
  mFt2->GetSfntTable(face, ft_sfnt_os2, &p);
  TT_OS2 *tt_os2 = (TT_OS2 *) p;
  NS_ASSERTION(tt_os2, "unable to get OS2 table");
  if (tt_os2)
    max_descent = FT_16_16_TO_REG(-tt_os2->sTypoDescender *
                                  face->size->metrics.y_scale);
  else
    max_descent = FT_16_16_TO_REG(-face->bbox.yMin *
                                  face->size->metrics.y_scale);
  max_descent = FT_CEIL(max_descent);
  max_descent = FT_TRUNC(max_descent);
  return max_descent;
}

int
nsFontPSFreeType::max_width()
{
  int max_advance_width;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  NS_ENSURE_TRUE(face, 0);
  max_advance_width = FT_16_16_TO_REG(face->max_advance_width *
                                      face->size->metrics.x_scale);
  max_advance_width = FT_CEIL(max_advance_width);
  max_advance_width = FT_TRUNC(max_advance_width);
  return max_advance_width;
}

PRBool
nsFontPSFreeType::getXHeight(unsigned long &aVal)
{
  int height;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face || !aVal)
    return PR_FALSE;
  height = FT_16_16_TO_REG(face->height * face->size->metrics.x_scale);
  height = FT_CEIL(height);
  height = FT_TRUNC(height);

  aVal = height;
  return PR_TRUE;
}

PRBool
nsFontPSFreeType::underlinePosition(long &aVal)
{
  long underline_position;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return PR_FALSE;
  underline_position = FT_16_16_TO_REG(-face->underline_position *
                        face->size->metrics.x_scale);
  underline_position = FT_CEIL(underline_position);
  underline_position = FT_TRUNC(underline_position);
  aVal = underline_position;
  return PR_TRUE;
}

PRBool
nsFontPSFreeType::underline_thickness(unsigned long &aVal)
{
  unsigned long underline_thickness;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return PR_FALSE;
  underline_thickness = FT_16_16_TO_REG(face->underline_thickness *
                         face->size->metrics.x_scale);
  underline_thickness = FT_CEIL(underline_thickness);
  underline_thickness = FT_TRUNC(underline_thickness);
  aVal = underline_thickness;
  return PR_TRUE;
}

PRBool
nsFontPSFreeType::superscript_y(long &aVal)
{
  aVal = 0;
  return PR_FALSE;
}

PRBool
nsFontPSFreeType::subscript_y(long &aVal)
{
  aVal = 0;
  return PR_FALSE;
}

nsresult
nsFontPSFreeType::RealizeFont(nsFontMetricsPS* aFontMetrics, float dev2app)
{
 
  nscoord leading, emHeight, emAscent, emDescent;
  nscoord maxHeight, maxAscent, maxDescent, maxAdvance;
  nscoord xHeight, spaceWidth, aveCharWidth;
  nscoord underlineOffset, underlineSize, superscriptOffset, subscriptOffset;
  nscoord strikeoutOffset, strikeoutSize;

  int lineSpacing = ascent() + descent();
  if (lineSpacing > mPixelSize) {
    leading = nscoord((lineSpacing - mPixelSize) * dev2app);
  }
  else {
    leading = 0;
  }
  emHeight = PR_MAX(1, nscoord(mPixelSize * dev2app));
  emAscent = nscoord(ascent() * mPixelSize * dev2app / lineSpacing);
  emDescent = emHeight - emAscent;

  maxHeight  = nscoord((max_ascent() + max_descent()) * dev2app);
  maxAscent  = nscoord(max_ascent() * dev2app) ;
  maxDescent = nscoord(max_descent() * dev2app);
  maxAdvance = nscoord(max_width() * dev2app);

  // 56% of ascent, best guess for non-true type
  xHeight = NSToCoordRound((float)ascent()* dev2app * 0.56f);

  PRUnichar space = (PRUnichar)' ';
  spaceWidth = NSToCoordRound(GetWidth(&space, 1));
  PRUnichar averageX = (PRUnichar)'x';
  aveCharWidth = NSToCoordRound(GetWidth(&averageX, 1));
  
  unsigned long pr = 0;
  if (getXHeight(pr)) {
    xHeight = (nscoord(pr * dev2app));
  }

  float height;
  long val;
  
  height = ascent() + descent();
  underlineOffset = -NSToIntRound(
                    PR_MAX (1, floor (0.1 * height + 0.5)) * dev2app);

  if (underline_thickness(pr)) {
    /* this will only be provided from adobe .afm fonts */
    underlineSize = nscoord(PR_MAX(dev2app, NSToIntRound(pr * dev2app)));
  }
  else {
    height = ascent() + descent();
    underlineSize = NSToIntRound(
                    PR_MAX(1, floor (0.05 * height + 0.5)) * dev2app);
  }

  if (superscript_y(val)) {
    superscriptOffset = nscoord(PR_MAX(dev2app, NSToIntRound(val * dev2app)));
  }
  else {
    superscriptOffset = xHeight;
  }

  if (subscript_y(val)) {
    subscriptOffset = nscoord(PR_MAX(dev2app, NSToIntRound(val * dev2app)));
  }
  else {
   subscriptOffset = xHeight;
  }

  /* need better way to calculate this */
  strikeoutOffset = NSToCoordRound(xHeight / 2.0);
  strikeoutSize = underlineSize;
  
  // TODO: leading never used, does it equal to "Height"?
  aFontMetrics->SetHeight(emHeight);
  aFontMetrics->SetEmHeight(emHeight);
  aFontMetrics->SetEmAscent(emAscent);
  aFontMetrics->SetEmDescent(emDescent);
  aFontMetrics->SetMaxHeight(maxHeight);
  aFontMetrics->SetMaxAscent(maxAscent);
  aFontMetrics->SetMaxDescent(maxDescent);
  aFontMetrics->SetMaxAdvance(maxAdvance);
  aFontMetrics->SetXHeight(xHeight);
  aFontMetrics->SetSpaceWidth(spaceWidth);
  aFontMetrics->SetAveCharWidth(aveCharWidth);
  aFontMetrics->SetUnderline(underlineOffset, underlineSize);
  aFontMetrics->SetSuperscriptOffset(superscriptOffset);
  aFontMetrics->SetSubscriptOffset(subscriptOffset);
  aFontMetrics->SetStrikeout(strikeoutOffset, strikeoutSize);

  return NS_OK;
}

nsresult
nsFontPSFreeType::SetupFont(nsRenderingContextPS* aContext)
{
  NS_ENSURE_TRUE(aContext, NS_ERROR_FAILURE);
  nsPostScriptObj* psObj = aContext->GetPostScriptObj();
  NS_ENSURE_TRUE(psObj, NS_ERROR_FAILURE);

  nscoord fontHeight = 0;
  mFontMetrics->GetHeight(fontHeight);

  nsCString fontName;
  int wmode = 0;
  FT_Face face = getFTFace();
  NS_ENSURE_TRUE(face, NS_ERROR_NULL_POINTER);
  char *cidFontName = FT2ToType8CidFontName(face, wmode);
  NS_ENSURE_TRUE(cidFontName, NS_ERROR_FAILURE);
  fontName.Assign(cidFontName);
  psObj->setfont(fontName, fontHeight);
  PR_Free(cidFontName);
  
  return NS_OK;
}

#ifdef MOZ_MATHML
nsresult
nsFontPSFreeType::GetBoundingMetrics(const char*        aString,
                                     PRUint32           aLength,
                                     nsBoundingMetrics& aBoundingMetrics)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFontPSFreeType::GetBoundingMetrics(const PRUnichar*   aString,
                                     PRUint32           aLength,
                                     nsBoundingMetrics& aBoundingMetrics)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif //MOZ_MATHML

#endif //MOZ_ENABLE_FREETYPE2

// Implementation of nsPSFontGenerator
nsPSFontGenerator::nsPSFontGenerator()
{
}

nsPSFontGenerator::~nsPSFontGenerator()
{
}

void nsPSFontGenerator::GeneratePSFont(FILE* aFile)
{
  NS_ERROR("should never call nsPSFontGenerator::GeneratePSFont");
}

void nsPSFontGenerator::AddToSubset(const PRUnichar* aString, PRUint32 aLength)
{
  for (PRUint32 i=0; i < aLength; i++) {
    if (mSubset.FindChar(aString[i]) == -1 )
      mSubset.Append(aString[i]);
  }
}

void nsPSFontGenerator::AddToSubset(const char* aString, PRUint32 aLength)
{
  PRUnichar unichar;
  for (PRUint32 i=0; i < aLength; i++) {
    unichar = (PRUnichar)((unsigned char)aString[i]);
    if (mSubset.FindChar(unichar) == -1 )
      mSubset.Append(unichar);
  }
}

#ifdef MOZ_ENABLE_FREETYPE2
nsFT2Type8Generator::nsFT2Type8Generator()
{
}

nsresult
nsFT2Type8Generator::Init(nsITrueTypeFontCatalogEntry* aEntry)
{
  NS_ENSURE_TRUE(aEntry, NS_ERROR_FAILURE);
  mEntry = aEntry;
  nsresult rv;
  mFt2 = do_GetService(NS_FREETYPE2_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsFT2Type8Generator::~nsFT2Type8Generator()
{
  mEntry = nsnull;
}

void nsFT2Type8Generator::GeneratePSFont(FILE* aFile)
{
  nsCAutoString fontName, styleName;
  mEntry->GetFamilyName(fontName);
  mEntry->GetStyleName(styleName);
  
  mImageDesc.font.face_id    = (void*)mEntry;
  // TT glyph has no relation to size
  mImageDesc.font.pix_width  = 16;
  mImageDesc.font.pix_height = 16;
  mImageDesc.image_type = 0;
  FT_Face face = nsnull;
  FTC_Manager cManager;
  mFt2->GetFTCacheManager(&cManager);
  nsresult rv = mFt2->ManagerLookupSize(cManager, &mImageDesc.font,
                                        &face, nsnull);
  if (NS_FAILED(rv))
    return;
 
  int wmode = 0;
  FT2SubsetToType8(face, mSubset.get(), mSubset.Length(), wmode, aFile);
}
#endif //MOZ_ENABLE_FREETYPE2

