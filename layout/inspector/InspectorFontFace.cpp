/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InspectorFontFace.h"

#include "nsCSSRules.h"
#include "gfxTextRun.h"
#include "gfxUserFontSet.h"
#include "nsFontFaceLoader.h"
#include "mozilla/gfx/2D.h"
#include "brotli/decode.h"
#include "zlib.h"
#include "mozilla/dom/FontFaceSet.h"

namespace mozilla {
namespace dom {

bool
InspectorFontFace::FromFontGroup()
{
  return mMatchType & gfxTextRange::kFontGroup;
}

bool
InspectorFontFace::FromLanguagePrefs()
{
  return mMatchType & gfxTextRange::kPrefsFallback;
}

bool
InspectorFontFace::FromSystemFallback()
{
  return mMatchType & gfxTextRange::kSystemFallback;
}

void
InspectorFontFace::GetName(nsAString& aName)
{
  if (mFontEntry->IsUserFont() && !mFontEntry->IsLocalUserFont()) {
    NS_ASSERTION(mFontEntry->mUserFontData, "missing userFontData");
    aName = mFontEntry->mUserFontData->mRealName;
  } else {
    aName = mFontEntry->RealFaceName();
  }
}

void
InspectorFontFace::GetCSSFamilyName(nsAString& aCSSFamilyName)
{
  aCSSFamilyName = mFontEntry->FamilyName();
}

nsCSSFontFaceRule*
InspectorFontFace::GetRule()
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
  return rule;
}

int32_t
InspectorFontFace::SrcIndex()
{
  if (mFontEntry->IsUserFont()) {
    NS_ASSERTION(mFontEntry->mUserFontData, "missing userFontData");
    return mFontEntry->mUserFontData->mSrcIndex;
  }

  return -1;
}

void
InspectorFontFace::GetURI(nsAString& aURI)
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
}

void
InspectorFontFace::GetLocalName(nsAString& aLocalName)
{
  if (mFontEntry->IsLocalUserFont()) {
    NS_ASSERTION(mFontEntry->mUserFontData, "missing userFontData");
    aLocalName = mFontEntry->mUserFontData->mLocalName;
  } else {
    aLocalName.Truncate();
  }
}

static void
AppendToFormat(nsAString& aResult, const char* aFormat)
{
  if (!aResult.IsEmpty()) {
    aResult.Append(',');
  }
  aResult.AppendASCII(aFormat);
}

void
InspectorFontFace::GetFormat(nsAString& aFormat)
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
}

void
InspectorFontFace::GetMetadata(nsAString& aMetadata)
{
  aMetadata.Truncate();
  if (mFontEntry->IsUserFont() && !mFontEntry->IsLocalUserFont()) {
    NS_ASSERTION(mFontEntry->mUserFontData, "missing userFontData");
    const gfxUserFontData* userFontData = mFontEntry->mUserFontData.get();
    if (userFontData->mMetadata.Length() && userFontData->mMetaOrigLen) {
      nsAutoCString str;
      str.SetLength(userFontData->mMetaOrigLen);
      if (str.Length() == userFontData->mMetaOrigLen) {
        switch (userFontData->mCompression) {
        case gfxUserFontData::kZlibCompression:
          {
            uLongf destLen = userFontData->mMetaOrigLen;
            if (uncompress((Bytef *)(str.BeginWriting()), &destLen,
                           (const Bytef *)(userFontData->mMetadata.Elements()),
                           userFontData->mMetadata.Length()) == Z_OK &&
                destLen == userFontData->mMetaOrigLen) {
              AppendUTF8toUTF16(str, aMetadata);
            }
          }
          break;
        case gfxUserFontData::kBrotliCompression:
          {
            size_t decodedSize = userFontData->mMetaOrigLen;
            if (BrotliDecoderDecompress(userFontData->mMetadata.Length(),
                                        userFontData->mMetadata.Elements(),
                                        &decodedSize,
                                        (uint8_t*)str.BeginWriting()) == 1 &&
                decodedSize == userFontData->mMetaOrigLen) {
              AppendUTF8toUTF16(str, aMetadata);
            }
          }
          break;
        }
      }
    }
  }
}

} // namespace dom
} // namespace mozilla
