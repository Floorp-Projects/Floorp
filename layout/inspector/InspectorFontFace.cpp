/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InspectorFontFace.h"

#ifdef MOZ_OLD_STYLE
#include "nsCSSRules.h"
#endif
#include "gfxTextRun.h"
#include "gfxUserFontSet.h"
#include "nsFontFaceLoader.h"
#include "mozilla/gfx/2D.h"
#include "brotli/decode.h"
#include "zlib.h"
#include "mozilla/dom/FontFaceSet.h"
#include "mozilla/Unused.h"

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
    if (formatFlags & gfxUserFontSet::FLAG_FORMAT_OPENTYPE_VARIATIONS) {
      AppendToFormat(aFormat, "opentype-variations");
    }
    if (formatFlags & gfxUserFontSet::FLAG_FORMAT_TRUETYPE_VARIATIONS) {
      AppendToFormat(aFormat, "truetype-variations");
    }
    if (formatFlags & gfxUserFontSet::FLAG_FORMAT_WOFF_VARIATIONS) {
      AppendToFormat(aFormat, "woff-variations");
    }
    if (formatFlags & gfxUserFontSet::FLAG_FORMAT_WOFF2_VARIATIONS) {
      AppendToFormat(aFormat, "woff2-variations");
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

// Append an OpenType tag to a string as a 4-ASCII-character code.
static void
AppendTagAsASCII(nsAString& aString, uint32_t aTag)
{
  aString.AppendPrintf("%c%c%c%c", (aTag >> 24) & 0xff,
                                   (aTag >> 16) & 0xff,
                                   (aTag >> 8) & 0xff,
                                   aTag & 0xff);
}

void
InspectorFontFace::GetVariationAxes(nsTArray<InspectorVariationAxis>& aResult,
                                    ErrorResult& aRV)
{
  if (!mFontEntry->HasVariations()) {
    return;
  }
  AutoTArray<gfxFontVariationAxis,4> axes;
  mFontEntry->GetVariationAxes(axes);
  MOZ_ASSERT(!axes.IsEmpty());
  if (!aResult.SetCapacity(axes.Length(), mozilla::fallible)) {
    aRV.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  for (auto a : axes) {
    InspectorVariationAxis& axis = *aResult.AppendElement();
    AppendTagAsASCII(axis.mTag, a.mTag);
    axis.mName = a.mName;
    axis.mMinValue = a.mMinValue;
    axis.mMaxValue = a.mMaxValue;
    axis.mDefaultValue = a.mDefaultValue;
  }
}

void
InspectorFontFace::GetVariationInstances(
  nsTArray<InspectorVariationInstance>& aResult,
  ErrorResult& aRV)
{
  if (!mFontEntry->HasVariations()) {
    return;
  }
  AutoTArray<gfxFontVariationInstance,16> instances;
  mFontEntry->GetVariationInstances(instances);
  if (!aResult.SetCapacity(instances.Length(), mozilla::fallible)) {
    aRV.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  for (auto i : instances) {
    InspectorVariationInstance& inst = *aResult.AppendElement();
    inst.mName = i.mName;
    // inst.mValues is a webidl sequence<>, which is a fallible array,
    // so we are required to use fallible SetCapacity and AppendElement calls,
    // and check the result. In practice we don't expect failure here; the
    // list of values cannot get huge because of limits in the font format.
    if (!inst.mValues.SetCapacity(i.mValues.Length(), mozilla::fallible)) {
      aRV.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    for (auto v : i.mValues) {
      InspectorVariationValue value;
      AppendTagAsASCII(value.mAxis, v.mAxis);
      value.mValue = v.mValue;
      // This won't fail, because of SetCapacity above.
      Unused << inst.mValues.AppendElement(value, mozilla::fallible);
    }
  }
}

void
InspectorFontFace::GetFeatures(nsTArray<InspectorFontFeature>& aResult,
                               ErrorResult& aRV)
{
  AutoTArray<gfxFontFeatureInfo,64> features;
  mFontEntry->GetFeatureInfo(features);
  if (features.IsEmpty()) {
    return;
  }
  if (!aResult.SetCapacity(features.Length(), mozilla::fallible)) {
    aRV.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  for (auto& f : features) {
    InspectorFontFeature& feat = *aResult.AppendElement();
    AppendTagAsASCII(feat.mTag, f.mTag);
    AppendTagAsASCII(feat.mScript, f.mScript);
    AppendTagAsASCII(feat.mLanguageSystem, f.mLangSys);
  }
}

void
InspectorFontFace::GetRanges(nsTArray<RefPtr<nsRange>>& aResult)
{
  aResult = mRanges;
}

void
InspectorFontFace::AddRange(nsRange* aRange)
{
  mRanges.AppendElement(aRange);
}

} // namespace dom
} // namespace mozilla
