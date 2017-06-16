/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoBindings.h"
#include "mozilla/ServoSpecifiedValues.h"

namespace {

#define STYLE_STRUCT(name, checkdata_cb) | NS_STYLE_INHERIT_BIT(name)
const uint64_t ALL_SIDS = 0
#include "nsStyleStructList.h"
  ;
#undef STYLE_STRUCT

} // anonymous namespace

using namespace mozilla;

ServoSpecifiedValues::ServoSpecifiedValues(nsPresContext* aContext,
                                           RawServoDeclarationBlock* aDecl)

  : GenericSpecifiedValues(StyleBackendType::Servo, aContext, ALL_SIDS)
  , mDecl(aDecl)
{}

bool
ServoSpecifiedValues::PropertyIsSet(nsCSSPropertyID aId)
{
  // We always create fresh ServoSpecifiedValues for each property
  // mapping, so unlike Gecko there aren't existing properties from
  // the cascade that we wish to avoid overwriting.
  //
  // If a property is being overwritten, that's a bug. Check for it
  // in debug mode (this is O(n^2) behavior since Servo will traverse
  // the array each time you add a new property)
  MOZ_ASSERT(!Servo_DeclarationBlock_PropertyIsSet(mDecl, aId),
             "Presentation attribute mappers should never attempt to set the "
             "same property twice");
  return false;
}

void
ServoSpecifiedValues::SetIdentStringValue(nsCSSPropertyID aId,
                                          const nsString& aValue)
{
  nsCOMPtr<nsIAtom> atom = NS_Atomize(aValue);
  SetIdentAtomValue(aId, atom);
}

void
ServoSpecifiedValues::SetIdentAtomValue(nsCSSPropertyID aId, nsIAtom* aValue)
{
  Servo_DeclarationBlock_SetIdentStringValue(mDecl, aId, aValue);
  if (aId == eCSSProperty__x_lang) {
    // This forces the lang prefs result to be cached
    // so that we can access them off main thread during traversal
    mPresContext->ForceCacheLang(aValue);
  }
}

void
ServoSpecifiedValues::SetKeywordValue(nsCSSPropertyID aId, int32_t aValue)
{
  Servo_DeclarationBlock_SetKeywordValue(mDecl, aId, aValue);
}

void
ServoSpecifiedValues::SetIntValue(nsCSSPropertyID aId, int32_t aValue)
{
  Servo_DeclarationBlock_SetIntValue(mDecl, aId, aValue);
}

void
ServoSpecifiedValues::SetPixelValue(nsCSSPropertyID aId, float aValue)
{
  Servo_DeclarationBlock_SetPixelValue(mDecl, aId, aValue);
}

void
ServoSpecifiedValues::SetLengthValue(nsCSSPropertyID aId, nsCSSValue aValue)
{
  MOZ_ASSERT(aValue.IsLengthUnit());
  Servo_DeclarationBlock_SetLengthValue(
    mDecl, aId, aValue.GetFloatValue(), aValue.GetUnit());
}

void
ServoSpecifiedValues::SetNumberValue(nsCSSPropertyID aId, float aValue)
{
  Servo_DeclarationBlock_SetNumberValue(mDecl, aId, aValue);
}

void
ServoSpecifiedValues::SetPercentValue(nsCSSPropertyID aId, float aValue)
{
  Servo_DeclarationBlock_SetPercentValue(mDecl, aId, aValue);
}

void
ServoSpecifiedValues::SetAutoValue(nsCSSPropertyID aId)
{
  Servo_DeclarationBlock_SetAutoValue(mDecl, aId);
}

void
ServoSpecifiedValues::SetCurrentColor(nsCSSPropertyID aId)
{
  Servo_DeclarationBlock_SetCurrentColor(mDecl, aId);
}

void
ServoSpecifiedValues::SetColorValue(nsCSSPropertyID aId, nscolor aColor)
{
  Servo_DeclarationBlock_SetColorValue(mDecl, aId, aColor);
}

void
ServoSpecifiedValues::SetFontFamily(const nsString& aValue)
{
  Servo_DeclarationBlock_SetFontFamily(mDecl, aValue);
}

void
ServoSpecifiedValues::SetTextDecorationColorOverride()
{
  Servo_DeclarationBlock_SetTextDecorationColorOverride(mDecl);
}

void
ServoSpecifiedValues::SetBackgroundImage(nsAttrValue& aValue)
{
  nsAutoString str;
  aValue.ToString(str);
  Servo_DeclarationBlock_SetBackgroundImage(
    mDecl, str, mPresContext->Document()->DefaultStyleAttrURLData());
}
