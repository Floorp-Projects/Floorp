/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRuleData.h"

#include "nsAttrValueInlines.h"
#include "nsCSSParser.h"
#include "nsPresContext.h"
#include "mozilla/GeckoStyleContext.h"
#include "mozilla/Poison.h"
#include <stdint.h>

using namespace mozilla;

inline size_t
nsRuleData::GetPoisonOffset()
{
  // Fill in mValueOffsets such that mValueStorage + mValueOffsets[i]
  // will yield the frame poison value for all uninitialized value
  // offsets.
  static_assert(sizeof(uintptr_t) == sizeof(size_t),
                "expect uintptr_t and size_t to be the same size");
  static_assert(uintptr_t(-1) > uintptr_t(0),
                "expect uintptr_t to be unsigned");
  static_assert(size_t(-1) > size_t(0), "expect size_t to be unsigned");
  uintptr_t framePoisonValue = mozPoisonValue();
  return size_t(framePoisonValue - uintptr_t(mValueStorage)) /
         sizeof(nsCSSValue);
}

nsRuleData::nsRuleData(uint32_t aSIDs,
                       nsCSSValue* aValueStorage,
                       GeckoStyleContext* aStyleContext)
  : GenericSpecifiedValues(StyleBackendType::Gecko, aStyleContext->PresContext()->Document(), aSIDs)
  , mStyleContext(aStyleContext)
  , mPresContext(aStyleContext->PresContext())
  , mValueStorage(aValueStorage)
{
#ifndef MOZ_VALGRIND
  size_t framePoisonOffset = GetPoisonOffset();
  for (size_t i = 0; i < nsStyleStructID_Length; ++i) {
    mValueOffsets[i] = framePoisonOffset;
  }
#endif
}

void
nsRuleData::SetFontFamily(const nsString& aValue)
{
  nsCSSValue* family = ValueForFontFamily();
  nsCSSParser parser;
  parser.ParseFontFamilyListString(aValue, nullptr, 0, *family);
}

void
nsRuleData::SetTextDecorationColorOverride()
{
  nsCSSValue* decoration = ValueForTextDecorationLine();
  int32_t newValue = NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL;
  if (decoration->GetUnit() == eCSSUnit_Enumerated) {
    newValue |= decoration->GetIntValue();
  }
  decoration->SetIntValue(newValue, eCSSUnit_Enumerated);
}

void
nsRuleData::SetBackgroundImage(nsAttrValue& aValue)
{
  nsCSSValue* backImage = ValueForBackgroundImage();
  // If the value is an image, or it is a URL and we attempted a load,
  // put it in the style tree.
  if (aValue.Type() == nsAttrValue::eURL) {
    aValue.LoadImage(mDocument);
  }
  if (aValue.Type() == nsAttrValue::eImage) {
    nsCSSValueList* list = backImage->SetListValue();
    list->mValue.SetImageValue(aValue.GetImageValue());
  }
}

#ifdef DEBUG
nsRuleData::~nsRuleData()
{
#ifndef MOZ_VALGRIND
  // assert nothing in mSIDs has poison value
  size_t framePoisonOffset = GetPoisonOffset();
  for (size_t i = 0; i < nsStyleStructID_Length; ++i) {
    MOZ_ASSERT(!(mSIDs & (1 << i)) || mValueOffsets[i] != framePoisonOffset,
               "value in SIDs was left with poison offset");
  }
#endif
}
#endif
