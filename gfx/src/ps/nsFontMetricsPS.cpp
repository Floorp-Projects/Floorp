/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "gfx-config.h"
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
#include "prlog.h"

#include "nsArray.h"

extern nsIAtom *gUsersLocale;
#define NS_IS_BOLD(weight) ((weight) > 400 ? 1 : 0)

#ifdef MOZ_ENABLE_FREETYPE2
static nsFontPS* CreateFontPS(nsITrueTypeFontCatalogEntry*, const nsFont&,
                              nsFontMetricsPS*);

static NS_DEFINE_CID(kFCSCID, NS_FONTCATALOGSERVICE_CID);
#endif

#ifdef PR_LOGGING
static PRLogModuleInfo *gFontMetricsPSM = PR_NewLogModule("FontMetricsPS");
#endif

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
  
  if (mFontsPS) {
    int i;
    for (i=0; i<mFontsPS->Count(); i++) {
      fontps *fontPS = (fontps *)mFontsPS->ElementAt(i);
      if (!fontPS)
        continue;
      NS_IF_RELEASE(fontPS->entry);
      if (fontPS->fontps)
        delete fontPS->fontps;
      if (fontPS->ccmap)
        FreeCCMap(fontPS->ccmap);
      delete fontPS;
    }
    delete mFontsPS;
  }

  if (mFontsAlreadyLoaded) {
    delete mFontsAlreadyLoaded;
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

  mFontsPS = new nsVoidArray();
  NS_ENSURE_TRUE(mFontsPS, NS_ERROR_OUT_OF_MEMORY);
  mFontsAlreadyLoaded = new nsHashtable();
  NS_ENSURE_TRUE(mFontsAlreadyLoaded, NS_ERROR_OUT_OF_MEMORY);

  // make sure we have at least one font
  nsFontPS *fontPS = nsFontPS::FindFont('a', aFont, this);
  NS_ENSURE_TRUE(fontPS, NS_ERROR_FAILURE);

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
    dev2app = mDeviceContext->DevUnitsToAppUnits();
    fontps *font = (fontps*)mFontsPS->ElementAt(0);
#ifdef MOZ_ENABLE_FREETYPE2
    NS_ASSERTION(font && font->entry, "no font available");
    if (font && !font->fontps && font->entry)
      font->fontps = CreateFontPS(font->entry, *mFont, this);
#endif
    NS_ASSERTION(font && font->fontps, "no font available");
    if (font && font->fontps)
      font->fontps->RealizeFont(this, dev2app);
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
  aWidth = 0;
  if (aLength == 0)
    return NS_OK;
  nsFontPS* fontPS = nsFontPS::FindFont(aString[0], *mFont, this);
  NS_ENSURE_TRUE(fontPS, NS_ERROR_FAILURE);

  nscoord i, start = 0;
  for (i=0; i<aLength; i++) {
    nsFontPS* fontThisChar = nsFontPS::FindFont(aString[i], *mFont, this);
    NS_ASSERTION(fontThisChar,"failed to find a font");
    NS_ENSURE_TRUE(fontThisChar, NS_ERROR_FAILURE);
    if (fontThisChar != fontPS) {
      // measure text up to this point
      aWidth += fontPS->GetWidth(aString+start, i-start);
      start = i;
      fontPS = fontThisChar;
    }
  }
  // measure the last part
  if (aLength-start)
      aWidth += fontPS->GetWidth(aString+start, aLength-start);

  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsFontMetricsPS.h
 *	@update 2/26/99 dwc
 */
NS_IMETHODIMP
nsFontMetricsPS :: GetStringWidth(const PRUnichar *aString,nscoord& aWidth,nscoord aLength)
{
  aWidth = 0;
  if (aLength == 0)
    return NS_OK;
  nsFontPS* fontPS = nsFontPS::FindFont(aString[0], *mFont, this);
  NS_ENSURE_TRUE(fontPS, NS_ERROR_FAILURE);

  nscoord i, start = 0;
  for (i=0; i<aLength; i++) {
    nsFontPS* fontThisChar = nsFontPS::FindFont(aString[i], *mFont, this);
    NS_ASSERTION(fontThisChar,"failed to find a font");
    NS_ENSURE_TRUE(fontThisChar, NS_ERROR_FAILURE);
    if (fontThisChar != fontPS) {
      // measure text up to this point
      aWidth += fontPS->GetWidth(aString+start, i-start);
      start = i;
      fontPS = fontThisChar;
    }
  }
  // measure the last part
  if (aLength-start)
      aWidth += fontPS->GetWidth(aString+start, aLength-start);

  return NS_OK;
}

nsFontPS*
nsFontPS::FindFont(char aChar, const nsFont& aFont, 
                   nsFontMetricsPS* aFontMetrics)
{
  PRUnichar uc = (unsigned char)aChar;
  return FindFont(uc, aFont, aFontMetrics);
}

// nsFontPS
nsFontPS*
nsFontPS::FindFont(PRUnichar aChar, const nsFont& aFont, 
                   nsFontMetricsPS* aFontMetrics)
{
  nsFontPS* fontPS;

#ifdef MOZ_ENABLE_FREETYPE2
  nsDeviceContextPS* dc = aFontMetrics->GetDeviceContext();
  NS_ENSURE_TRUE(dc, nsnull);
  if (dc->mFTPEnable) {
    fontPS = nsFontPSFreeType::FindFont(aChar, aFont, aFontMetrics);
    if (fontPS)
      return fontPS;
  }
#endif

  /* Find in afm font */
  if (aFontMetrics->GetFontsPS()->Count() > 0) {
    fontps *fps = (fontps*)aFontMetrics->GetFontsPS()->ElementAt(0);
    NS_ENSURE_TRUE(fps, nsnull);
    fontPS = fps->fontps;
  }
  else {
    fontPS = nsFontPSAFM::FindFont(aFont, aFontMetrics);
    fontps *fps = new fontps;
    NS_ENSURE_TRUE(fps, nsnull);
    fps->entry  = nsnull;
    fps->fontps = fontPS;
    fps->ccmap  = nsnull;
    aFontMetrics->GetFontsPS()->AppendElement(fps);
  }
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
  afmInfo->Init(aFont.size);

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
  nscoord width = 0;
  if (mAFMInfo) {
    mAFMInfo->GetStringWidth(aString, width, aLength);
  }
  return width;
}

nscoord
nsFontPSAFM::GetWidth(const PRUnichar* aString, PRUint32 aLength)
{
  nscoord width = 0;
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
  aFontMetrics->SetStrikeout((nscoord)(xHeight / 2), onePixel);

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

PRBool
nsFontPSFreeType::CSSFontEnumCallback(const nsString& aFamily, PRBool aGeneric,
                                      void* aFpi)
{
  fontPSInfo* fpi = (fontPSInfo*)aFpi;
  nsCAutoString familyname;
  if (aGeneric) {
    // need lang to lookup generic pref
    if (strlen(fpi->lang.get()) == 0) {
      return PR_TRUE; // keep trying
    }
    nsXPIDLCString value;
    nsresult rv;
    nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, PR_TRUE); // keep trying
    nsCAutoString name("font.name.");
    name.AppendWithConversion(aFamily.get());
    name.Append(char('.'));
    name.Append(fpi->lang);
    pref->CopyCharPref(name.get(), getter_Copies(value));
    if (!value.get())
      return PR_TRUE; // keep trying
    // strip down to just the family name
    PRInt32 startFamily = value.FindChar('-') + 1;
    if (startFamily < 0) // 1st '-' not found. Not FFRE but just familyName.
      familyname = value;
    else { 
      PRInt32 endFamily = value.FindChar('-', startFamily);
      if (endFamily < 0) // 2nd '-' not found
        familyname.Append(Substring(value, startFamily));
      else  // FFRE 
        familyname.Append(Substring(value, startFamily, endFamily - startFamily));
    }
    PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG,
        ("generic font \"%s\" -> \"%s\"\n", name.get(), familyname.get()));
  }
  else
    familyname.AppendWithConversion(aFamily);

  AddFontEntries(familyname, fpi->lang, fpi->weight, 
                 nsIFontCatalogService::kFCWidthAny, fpi->slant,
                 nsIFontCatalogService::kFCSpacingAny, fpi);

  return PR_TRUE;
}

PRBool
nsFontPSFreeType::AddUserPref(nsIAtom *aLang, const nsFont& aFont,
                              fontPSInfo *aFpi)
{
  nsCAutoString emptyStr;
  fontPSInfo *fpi = (fontPSInfo*)aFpi;
  nsresult rv = NS_OK;
  nsCAutoString fontName;
  nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  nsXPIDLCString value;
  pref->CopyCharPref("font.default", getter_Copies(value));
  if (!value.get())
    return PR_FALSE;

  nsCAutoString name("font.name.");
  name.Append(value);
  name.Append(char('.'));
  name.Append(fpi->lang);
  pref->CopyCharPref(name.get(), getter_Copies(value));

  if (!value.get())
    return PR_FALSE;

  // strip down to just the family name
  PRInt32 startFamily = value.FindChar('-') + 1;
  if (startFamily < 0) // '-' not found. Not FFRE but just familyName 
    fontName = value;
  else {
    PRInt32 endFamily = value.FindChar('-', startFamily);
    if (endFamily < 0) // 2nd '-' not found
      fontName.Append(Substring(value, startFamily));
    else  // FFRE
      fontName.Append(Substring(value, startFamily, endFamily - startFamily));
  }

  AddFontEntries(fontName, fpi->lang, fpi->weight,
                 nsIFontCatalogService::kFCWidthAny, fpi->slant,
                 nsIFontCatalogService::kFCSpacingAny, fpi);

  // wildcard the language
  AddFontEntries(fontName, emptyStr, fpi->weight,
                 nsIFontCatalogService::kFCWidthAny, fpi->slant,
                 nsIFontCatalogService::kFCSpacingAny, fpi);

  return PR_TRUE;
}

static nsFontPS*
CreateFontPS(nsITrueTypeFontCatalogEntry *aEntry, const nsFont& aFont,
             nsFontMetricsPS* aFontMetrics)
{
  nsresult rv;
  nsDeviceContextPS* dc = aFontMetrics->GetDeviceContext();
  NS_ENSURE_TRUE(dc, nsnull);

  nsCAutoString familyName, styleName;
  aEntry->GetFamilyName(familyName);
  aEntry->GetStyleName(styleName);
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
    rv = ((nsFT2Type8Generator*)psFontGen)->Init(aEntry);
    if (NS_FAILED(rv)) {
      delete psFontGen;
      return nsnull;
    }
    psFGList->Put(&key, (void *) psFontGen);
  }
  nsFontPSFreeType* font = new nsFontPSFreeType(aFont, aFontMetrics);
  NS_ENSURE_TRUE(font, nsnull);
  rv = font->Init(aEntry, psFontGen);
  if (NS_FAILED(rv)) {
    delete font;
    return nsnull;
  }
  return font;
}

nsFontPS*
nsFontPSFreeType::FindFont(PRUnichar aChar, const nsFont& aFont, 
                           nsFontMetricsPS* aFontMetrics)
{
  PRBool inited = PR_FALSE;
  int anyWeight  = nsIFontCatalogService::kFCWeightAny;
  int anyWidth   = nsIFontCatalogService::kFCWidthAny;
  int anySlant   = nsIFontCatalogService::kFCSlantAny;
  int anySpacing = nsIFontCatalogService::kFCSpacingAny;
  nsCOMPtr<nsIAtom> lang;
  nsCAutoString emptyStr;
  nsCAutoString locale;
  fontPSInfo fpi, fpi2;
  fpi.fontps = aFontMetrics->GetFontsPS();

  int i = 0;
  while (1) {
    //
    // see if it is already in the list of fonts
    //
    for (; i<fpi.fontps->Count(); i++) {
      fontps *fi = (fontps *)fpi.fontps->ElementAt(i);
      if (!fi->entry || !fi->ccmap) {
        NS_ASSERTION(fi->entry, "invalid entry");
        NS_ASSERTION(fi->ccmap, "invalid ccmap");
        continue;
      }
      if (CCMAP_HAS_CHAR(fi->ccmap, aChar)) {
        if (!fi->fontps) {
#ifdef PR_LOGGING
          if (PR_LOG_TEST(gFontMetricsPSM, PR_LOG_DEBUG)) {
            nsCAutoString familyName, styleName;
            fi->entry->GetFamilyName(familyName);
            fi->entry->GetStyleName(styleName);
            PR_LogPrint("CreateFontPS %s/%s\n",
                familyName.get(), styleName.get());
          }
#endif
          fi->fontps = CreateFontPS(fi->entry, aFont, aFontMetrics);
        }
        if (fi->fontps)
          return fi->fontps;
      }
    }

    //
    // it is not already in the list of fonts
    // so add more fonts to the list
    //

    if (!inited) {
      fpi.nsfont = &aFont;
      fpi.alreadyLoaded = aFontMetrics->GetFontsAlreadyLoadedList();
      aFontMetrics->GetLangGroup(getter_AddRefs(lang));
      if (!lang)
        lang = NS_NewAtom("x-western");
      const char *langStr;
      lang->GetUTF8String(&langStr);
      if (langStr)
        fpi.lang.Append(langStr);
      gUsersLocale->GetUTF8String(&langStr);
      if (langStr)
        locale.Append(langStr);
      if (NS_IS_BOLD(fpi.nsfont->weight))
        fpi.weight = nsIFontCatalogService::kFCWeightBold;
      else
        fpi.weight = nsIFontCatalogService::kFCWeightMedium;
      if (fpi.nsfont->style == NS_FONT_STYLE_NORMAL)
        fpi.slant = nsIFontCatalogService::kFCSlantRoman;
      else
        fpi.slant = nsIFontCatalogService::kFCSlantItalic;
      inited = PR_TRUE;
    }

    //
    // Add fonts to the list following the CSS spec, user pref
    // After that slowly loosen the spec to enlarge the list
    //
    int state = aFontMetrics->GetFontPSState();
    aFontMetrics->IncrementFontPSState();

    switch (state) {
      case 0:
        PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("%s\n",
            "get the CSS specified entries for the element's language"));
        aFont.EnumerateFamilies(nsFontPSFreeType::CSSFontEnumCallback, &fpi);
        break;

      case 1:
        PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("%s\n",
            "get the CSS specified entries for the user's locale"));
        fpi2 = fpi;
        fpi2.lang = locale;
        aFont.EnumerateFamilies(nsFontPSFreeType::CSSFontEnumCallback, &fpi2);
        break;

      case 2:
        PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("%s\n",
            "get the CSS specified entries for any language"));
        fpi2 = fpi;
        fpi2.lang = emptyStr;
        aFont.EnumerateFamilies(nsFontPSFreeType::CSSFontEnumCallback, &fpi2);
        break;

      case 3:
        PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("%s\n",
            "get the user pref for the element's language"));
        AddUserPref(lang, aFont, &fpi);
        break;

      case 4:
        PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("%s\n",
            "get the user pref for the user's locale"));
        fpi2 = fpi;
        fpi2.lang = locale;
        aFont.EnumerateFamilies(nsFontPSFreeType::CSSFontEnumCallback, &fpi2);
        break;

      case 5:
        PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("%s\n",
            "get all the entries for this language/style"));
        AddFontEntries(emptyStr, fpi.lang, fpi.weight, anyWidth, fpi.slant,
                       anySpacing, &fpi);
        break;

      case 6:
        PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("%s\n",
            "get all the entries for the locale/style"));
        AddFontEntries(emptyStr, locale, fpi.weight, anyWidth, fpi.slant,
                       anySpacing, &fpi);
        break;

      case 7:
        PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("%s\n",
            "wildcard the slant/weight variations of CSS "
            "specified entries for the element's language"));
        fpi2 = fpi;
        fpi2.weight = anyWeight;
        fpi2.slant = 0;
        aFont.EnumerateFamilies(nsFontPSFreeType::CSSFontEnumCallback, &fpi2);
        break;

      case 8:
        PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("%s\n",
            "wildcard the slant/weight variations of CSS "
            "specified entries for the user's locale"));
        fpi2 = fpi;
        fpi2.lang = locale;
        fpi2.weight = anyWeight;
        fpi2.slant = 0;
        aFont.EnumerateFamilies(nsFontPSFreeType::CSSFontEnumCallback, &fpi2);
        break;

      case 9:
        PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("%s\n",
            "wildcard the slant/weight variations of CSS "
            "specified entries for any language"));
        fpi2 = fpi;
        fpi2.lang = emptyStr;
        fpi2.weight = anyWeight;
        fpi2.slant = 0;
        aFont.EnumerateFamilies(nsFontPSFreeType::CSSFontEnumCallback, &fpi2);
        break;

      case 10:
        PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("%s\n",
            "wildcard the slant/weight variations of the user pref"));
        fpi2 = fpi;
        fpi2.lang   = emptyStr;
        fpi2.weight = anyWeight;
        fpi2.slant  = 0;
        AddUserPref(lang, aFont, &fpi2);
        break;

      case 11:
        PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("%s\n",
            "wildcard the slant/weight variations for this language"));
        AddFontEntries(emptyStr, fpi.lang, anyWeight, anyWidth, anySlant,
                       anySpacing, &fpi);
        break;

      case 12:
        PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("%s\n",
            "wildcard the slant/weight variations of the locale"));
        AddFontEntries(emptyStr, locale, anyWeight, anyWidth, anySlant,
                       anySpacing, &fpi);
        break;

      case 13:
        PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("%s\n", "get ALL font entries"));
        AddFontEntries(emptyStr, emptyStr, anyWeight, anyWidth, anySlant,
                       anySpacing, &fpi);
        break;

      default:
        // try to always return a font even if no font supports this char
        if (fpi.fontps->Count()) {
          PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG,
              ("failed to find a font supporting 0x%04x so "
              "returning 1st font in list\n", aChar));
          fontps *fi = (fontps *)fpi.fontps->ElementAt(0);
          if (!fi->fontps)
            fi->fontps = CreateFontPS(fi->entry, aFont, aFontMetrics);
          return fi->fontps;
        }
        PR_LOG(gFontMetricsPSM, PR_LOG_WARNING,
            ("failed to find a font supporting 0x%04x\n", aChar));
        return (nsnull);
    }
  }

  return nsnull;
}

nsresult
nsFontPSFreeType::AddFontEntries(nsACString& aFamilyName, nsACString& aLanguage,
                                 PRUint16 aWeight, PRUint16 aWidth,
                                 PRUint16 aSlant, PRUint16 aSpacing,
                                 fontPSInfo* aFpi)
{
  nsresult rv = NS_OK;
  nsCAutoString name(aFamilyName);
  nsCAutoString lang(aLanguage);
  PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("    family   = '%s'", name.get()));
  PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("    lang     = '%s'", lang.get()));
  PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("    aWeight  = %d", aWeight));
  PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("    aWidth   = %d", aWidth));
  PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("    aSlant   = %d", aSlant));
  PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("    aSpacing = %d", aSpacing));

  nsCOMPtr<nsIFontCatalogService> fcs(do_GetService(kFCSCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIArray> entryList;
  rv = fcs->GetFontCatalogEntries(aFamilyName, aLanguage,
                                  aWeight, aWidth, aSlant, aSpacing,
                                  getter_AddRefs(entryList));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 i, count = 0;
  NS_ENSURE_TRUE(entryList, NS_ERROR_FAILURE);

  rv = entryList->GetLength(&count);
  NS_ENSURE_SUCCESS(rv, rv);
  PR_LOG(gFontMetricsPSM, PR_LOG_DEBUG, ("    count    = %d", count));

  for (i=0; i<count; i++) {
    nsCOMPtr<nsITrueTypeFontCatalogEntry> entry = do_QueryElementAt(entryList,
                                                                    i, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // check if already in list
    nsVoidKey key((void*)entry);
    PRBool loaded = aFpi->alreadyLoaded->Exists(&key);
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gFontMetricsPSM, PR_LOG_DEBUG)) {
      nsCAutoString fontname, stylename;
      entry->GetFamilyName(fontname);
      entry->GetStyleName(stylename);
      PR_LogPrint("    -- %s '%s/%s'\n", (loaded ? "already loaded" : "load"),
          fontname.get(), stylename.get());
    }
#endif
    if (loaded)
      continue;

    PRUint16 *ccmap;
    PRUint32 size;
    entry->GetCCMap(&size, &ccmap);
    nsITrueTypeFontCatalogEntry *e = entry;
    NS_IF_ADDREF(e);
    fontps *fps = new fontps;
    NS_ENSURE_TRUE(fps, NS_ERROR_OUT_OF_MEMORY);
    fps->entry  = entry;
    fps->fontps = nsnull;
    fps->ccmap  = ccmap;
    aFpi->fontps->AppendElement(fps);
    aFpi->alreadyLoaded->Put(&key, (void*)1);
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
  app2dev = dc->AppUnitsToDevUnits();
  
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
  PRUint32 len, width = 0;
  while ( aLength > 0 ) {
    len = PR_MIN(aLength, WIDEN_8_TO_16_BUF_SIZE);
    for (PRUint32 i=0; i < len; i++) {
      unichars[i] = (PRUnichar)((unsigned char)aString[i]);
    }
    width += GetWidth(unichars, len);
    aString += len;
    aLength -= len;
  }
  return width;
}


nscoord
nsFontPSFreeType::GetWidth(const PRUnichar* aString, PRUint32 aLength)
{
  FT_UInt glyph_index;
  FT_Glyph glyph;
  double origin_x = 0;

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
      origin_x += FT_REG_TO_16_16(face->size->metrics.x_ppem/2 + 2);
      continue;
    }
    origin_x += glyph->advance.x;
  }

  NS_ENSURE_TRUE(mFontMetrics, 0);

  nsDeviceContextPS* dc = mFontMetrics->GetDeviceContext();
  NS_ENSURE_TRUE(dc, 0);

  float dev2app;
  dev2app = dc->DevUnitsToAppUnits();
  origin_x *= dev2app;
  origin_x /= FT_REG_TO_16_16(1);

  return NSToCoordRound((nscoord)origin_x);
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
  nscoord width = 0;
  
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
    width += GetWidth(unichars, len);
    aString += len;
    aLength -= len;
  }
  return width;
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
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  NS_ENSURE_TRUE(face, 0);
  return FT_DESIGN_UNITS_TO_PIXELS(face->ascender, face->size->metrics.y_scale);
}

int
nsFontPSFreeType::descent()
{
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  NS_ENSURE_TRUE(face, 0);
  return FT_DESIGN_UNITS_TO_PIXELS(-face->descender, face->size->metrics.y_scale);
}

int
nsFontPSFreeType::max_ascent()
{
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  NS_ENSURE_TRUE(face, 0);
  void * p;
  mFt2->GetSfntTable(face, ft_sfnt_os2, &p);
  TT_OS2 * tt_os2 = (TT_OS2 *)p;
  NS_ASSERTION(tt_os2, "unable to get OS2 table");
  if (tt_os2)
     return FT_DESIGN_UNITS_TO_PIXELS(tt_os2->sTypoAscender,
                                      face->size->metrics.y_scale);
  else
     return FT_DESIGN_UNITS_TO_PIXELS(face->bbox.yMax,
                                      face->size->metrics.y_scale);
}

int
nsFontPSFreeType::max_descent()
{
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  NS_ENSURE_TRUE(face, 0);
  void *p;
  mFt2->GetSfntTable(face, ft_sfnt_os2, &p);
  TT_OS2 *tt_os2 = (TT_OS2 *) p;
  NS_ASSERTION(tt_os2, "unable to get OS2 table");
  if (tt_os2)
     return FT_DESIGN_UNITS_TO_PIXELS(-tt_os2->sTypoDescender,
                                      face->size->metrics.y_scale);
  else
     return FT_DESIGN_UNITS_TO_PIXELS(-face->bbox.yMin,
                                      face->size->metrics.y_scale);
}

int
nsFontPSFreeType::max_width()
{
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  NS_ENSURE_TRUE(face, 0);
  return FT_DESIGN_UNITS_TO_PIXELS(face->max_advance_width,
                                   face->size->metrics.x_scale);
}

PRBool
nsFontPSFreeType::getXHeight(unsigned long &aVal)
{
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face || !aVal)
    return PR_FALSE;
  aVal = FT_DESIGN_UNITS_TO_PIXELS(face->height, face->size->metrics.y_scale);

  return PR_TRUE;
}

PRBool
nsFontPSFreeType::underlinePosition(long &aVal)
{
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return PR_FALSE;
  aVal = FT_DESIGN_UNITS_TO_PIXELS(-face->underline_position,
                                   face->size->metrics.y_scale);
  return PR_TRUE;
}

PRBool
nsFontPSFreeType::underline_thickness(unsigned long &aVal)
{
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return PR_FALSE;
  aVal = FT_DESIGN_UNITS_TO_PIXELS(face->underline_thickness,
                                   face->size->metrics.x_scale);
  return PR_TRUE;
}

PRBool
nsFontPSFreeType::superscript_y(long &aVal)
{
  aVal = 0;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return PR_FALSE;

  TT_OS2 *tt_os2;
  mFt2->GetSfntTable(face, ft_sfnt_os2, (void**)&tt_os2);
  NS_ASSERTION(tt_os2, "unable to get OS2 table");
  if (!tt_os2)
    return PR_FALSE;

  aVal = FT_DESIGN_UNITS_TO_PIXELS(tt_os2->ySuperscriptYOffset,
                                  face->size->metrics.y_scale);
  return PR_TRUE;
}

PRBool
nsFontPSFreeType::subscript_y(long &aVal)
{
  aVal = 0;
  FT_Face face = getFTFace();
  NS_ASSERTION(face, "failed to get face/size");
  if (!face)
    return PR_FALSE;

  TT_OS2 *tt_os2;
  mFt2->GetSfntTable(face, ft_sfnt_os2, (void**)&tt_os2);
  NS_ASSERTION(tt_os2, "unable to get OS2 table");
  if (!tt_os2)
    return PR_FALSE;

  aVal = FT_DESIGN_UNITS_TO_PIXELS(tt_os2->ySubscriptYOffset,
                                  face->size->metrics.y_scale);

  // some fonts have the sign wrong. it should be always positive.
  aVal = (aVal < 0) ? -aVal : aVal;
  return PR_TRUE;
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
  // Add a small set of characters to the subset of the user
  // defined font to produce to make sure the font ends up
  // being larger than 2000 bytes, a threshold under which
  // some printers will consider the font invalid.  (bug 253219)
  AddToSubset("1234567890", 10);
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
  if (!mSubset.IsEmpty())
    FT2SubsetToType8(face, mSubset.get(), mSubset.Length(), wmode, aFile);
}
#endif //MOZ_ENABLE_FREETYPE2

