/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFontFace.h"
#include "nsIDOMCSSFontFaceRule.h"
#include "nsCSSRules.h"
#include "gfxTextRun.h"
#include "gfxUserFontSet.h"
#include "nsFontFaceLoader.h"
#include "mozilla/gfx/2D.h"
#include "zlib.h"
#include "mozilla/dom/FontFaceSet.h"

using namespace mozilla;
using namespace mozilla::dom;

nsFontFace::nsFontFace(gfxFontEntry*      aFontEntry,
                       gfxFontGroup*      aFontGroup,
                       uint8_t            aMatchType)
  : mFontEntry(aFontEntry),
    mFontGroup(aFontGroup),
    mMatchType(aMatchType)
{
}

nsFontFace::~nsFontFace()
{
}

////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS(nsFontFace, nsIDOMFontFace)

////////////////////////////////////////////////////////////////////////
// nsIDOMFontFace

/* readonly attribute boolean fromFontGroup; */
NS_IMETHODIMP
nsFontFace::GetFromFontGroup(bool * aFromFontGroup)
{
  *aFromFontGroup =
    (mMatchType & gfxTextRange::kFontGroup) != 0;
  return NS_OK;
}

/* readonly attribute boolean fromLanguagePrefs; */
NS_IMETHODIMP
nsFontFace::GetFromLanguagePrefs(bool * aFromLanguagePrefs)
{
  *aFromLanguagePrefs =
    (mMatchType & gfxTextRange::kPrefsFallback) != 0;
  return NS_OK;
}

/* readonly attribute boolean fromSystemFallback; */
NS_IMETHODIMP
nsFontFace::GetFromSystemFallback(bool * aFromSystemFallback)
{
  *aFromSystemFallback =
    (mMatchType & gfxTextRange::kSystemFallback) != 0;
  return NS_OK;
}

/* readonly attribute DOMString name; */
NS_IMETHODIMP
nsFontFace::GetName(nsAString & aName)
{
  if (mFontEntry->IsUserFont() && !mFontEntry->IsLocalUserFont()) {
    NS_ASSERTION(mFontEntry->mUserFontData, "missing userFontData");
    aName = mFontEntry->mUserFontData->mRealName;
  } else {
    aName = mFontEntry->RealFaceName();
  }
  return NS_OK;
}

/* readonly attribute DOMString CSSFamilyName; */
NS_IMETHODIMP
nsFontFace::GetCSSFamilyName(nsAString & aCSSFamilyName)
{
  aCSSFamilyName = mFontEntry->FamilyName();
  return NS_OK;
}

/* readonly attribute nsIDOMCSSFontFaceRule rule; */
NS_IMETHODIMP
nsFontFace::GetRule(nsIDOMCSSFontFaceRule **aRule)
{
  // check whether this font entry is associated with an @font-face rule
  // in the relevant font group's user font set
  nsCSSFontFaceRule* rule = nullptr;
  if (mFontEntry->IsUserFont()) {
    FontFaceSet::UserFontSet* fontSet =
      static_cast<FontFaceSet::UserFontSet*>(mFontGroup->GetUserFontSet());
    if (fontSet) {
      FontFaceSet* fontFaceSet = fontSet->GetFontFaceSet();
      if (fontFaceSet) {
        rule = fontFaceSet->FindRuleForEntry(mFontEntry);
      }
    }
  }

  NS_IF_ADDREF(*aRule = rule);
  return NS_OK;
}

/* readonly attribute long srcIndex; */
NS_IMETHODIMP
nsFontFace::GetSrcIndex(int32_t * aSrcIndex)
{
  if (mFontEntry->IsUserFont()) {
    NS_ASSERTION(mFontEntry->mUserFontData, "missing userFontData");
    *aSrcIndex = mFontEntry->mUserFontData->mSrcIndex;
  } else {
    *aSrcIndex = -1;
  }
  return NS_OK;
}

/* readonly attribute DOMString URI; */
NS_IMETHODIMP
nsFontFace::GetURI(nsAString & aURI)
{
  aURI.Truncate();
  if (mFontEntry->IsUserFont() && !mFontEntry->IsLocalUserFont()) {
    NS_ASSERTION(mFontEntry->mUserFontData, "missing userFontData");
    if (mFontEntry->mUserFontData->mURI) {
      nsAutoCString spec;
      mFontEntry->mUserFontData->mURI->GetSpec(spec);
      AppendUTF8toUTF16(spec, aURI);
    }
  }
  return NS_OK;
}

/* readonly attribute DOMString localName; */
NS_IMETHODIMP
nsFontFace::GetLocalName(nsAString & aLocalName)
{
  if (mFontEntry->IsLocalUserFont()) {
    NS_ASSERTION(mFontEntry->mUserFontData, "missing userFontData");
    aLocalName = mFontEntry->mUserFontData->mLocalName;
  } else {
    aLocalName.Truncate();
  }
  return NS_OK;
}

/* readonly attribute DOMString format; */
static void
AppendToFormat(nsAString & aResult, const char* aFormat)
{
  if (!aResult.IsEmpty()) {
    aResult.Append(',');
  }
  aResult.AppendASCII(aFormat);
}

NS_IMETHODIMP
nsFontFace::GetFormat(nsAString & aFormat)
{
  aFormat.Truncate();
  if (mFontEntry->IsUserFont() && !mFontEntry->IsLocalUserFont()) {
    NS_ASSERTION(mFontEntry->mUserFontData, "missing userFontData");
    uint32_t formatFlags = mFontEntry->mUserFontData->mFormat;
    if (formatFlags & gfxUserFontSet::FLAG_FORMAT_OPENTYPE) {
      AppendToFormat(aFormat, "opentype");
    }
    if (formatFlags & gfxUserFontSet::FLAG_FORMAT_TRUETYPE) {
      AppendToFormat(aFormat, "truetype");
    }
    if (formatFlags & gfxUserFontSet::FLAG_FORMAT_TRUETYPE_AAT) {
      AppendToFormat(aFormat, "truetype-aat");
    }
    if (formatFlags & gfxUserFontSet::FLAG_FORMAT_EOT) {
      AppendToFormat(aFormat, "embedded-opentype");
    }
    if (formatFlags & gfxUserFontSet::FLAG_FORMAT_SVG) {
      AppendToFormat(aFormat, "svg");
    }
    if (formatFlags & gfxUserFontSet::FLAG_FORMAT_WOFF) {
      AppendToFormat(aFormat, "woff");
    }
    if (formatFlags & gfxUserFontSet::FLAG_FORMAT_WOFF2) {
      AppendToFormat(aFormat, "woff2");
    }
  }
  return NS_OK;
}

/* readonly attribute DOMString metadata; */
NS_IMETHODIMP
nsFontFace::GetMetadata(nsAString & aMetadata)
{
  aMetadata.Truncate();
  if (mFontEntry->IsUserFont() && !mFontEntry->IsLocalUserFont()) {
    NS_ASSERTION(mFontEntry->mUserFontData, "missing userFontData");
    const gfxUserFontData* userFontData = mFontEntry->mUserFontData;
    if (userFontData->mMetadata.Length() && userFontData->mMetaOrigLen) {
      nsAutoCString str;
      str.SetLength(userFontData->mMetaOrigLen);
      if (str.Length() == userFontData->mMetaOrigLen) {
        uLongf destLen = userFontData->mMetaOrigLen;
        if (uncompress((Bytef *)(str.BeginWriting()), &destLen,
                       (const Bytef *)(userFontData->mMetadata.Elements()),
                       userFontData->mMetadata.Length()) == Z_OK &&
            destLen == userFontData->mMetaOrigLen)
        {
          AppendUTF8toUTF16(str, aMetadata);
        }
      }
    }
  }
  return NS_OK;
}
