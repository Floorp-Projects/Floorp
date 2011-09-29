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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Kew <jfkthame@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#define _IMPL_NS_LAYOUT

#include "nsFontFace.h"
#include "nsIDOMCSSFontFaceRule.h"
#include "nsCSSRules.h"
#include "gfxUserFontSet.h"
#include "zlib.h"

nsFontFace::nsFontFace(gfxFontEntry*      aFontEntry,
                       PRUint8            aMatchType,
                       nsCSSFontFaceRule* aRule)
  : mFontEntry(aFontEntry),
    mRule(aRule),
    mMatchType(aMatchType)
{
}

nsFontFace::~nsFontFace()
{
}

////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS1(nsFontFace, nsIDOMFontFace)

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
  NS_IF_ADDREF(*aRule = mRule.get());
  return NS_OK;
}

/* readonly attribute long srcIndex; */
NS_IMETHODIMP
nsFontFace::GetSrcIndex(PRInt32 * aSrcIndex)
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
      nsCAutoString spec;
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
    aResult.AppendASCII(",");
  }
  aResult.AppendASCII(aFormat);
}

NS_IMETHODIMP
nsFontFace::GetFormat(nsAString & aFormat)
{
  aFormat.Truncate();
  if (mFontEntry->IsUserFont() && !mFontEntry->IsLocalUserFont()) {
    NS_ASSERTION(mFontEntry->mUserFontData, "missing userFontData");
    PRUint32 formatFlags = mFontEntry->mUserFontData->mFormat;
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
      nsCAutoString str;
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
