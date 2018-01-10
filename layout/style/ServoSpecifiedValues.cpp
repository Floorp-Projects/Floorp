/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoBindings.h"
#include "mozilla/ServoSpecifiedValues.h"

using namespace mozilla;

bool
ServoSpecifiedValues::PropertyIsSet(nsCSSPropertyID aId)
{
  return Servo_DeclarationBlock_PropertyIsSet(mDecl, aId);
}

void
ServoSpecifiedValues::SetIdentStringValue(nsCSSPropertyID aId,
                                          const nsString& aValue)
{
  RefPtr<nsAtom> atom = NS_Atomize(aValue);
  SetIdentAtomValue(aId, atom);
}

void
ServoSpecifiedValues::SetIdentAtomValue(nsCSSPropertyID aId, nsAtom* aValue)
{
  Servo_DeclarationBlock_SetIdentStringValue(mDecl, aId, aValue);
  if (aId == eCSSProperty__x_lang) {
    // This forces the lang prefs result to be cached so that we can access them
    // off main thread during traversal.
    //
    // FIXME(emilio): Can we move mapped attribute declarations across
    // documents? Isn't this wrong in that case? This is pretty out of place
    // anyway.
    if (nsIPresShell* shell = mDocument->GetShell()) {
      if (nsPresContext* pc = shell->GetPresContext()) {
        pc->ForceCacheLang(aValue);
      }
    }
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
  if (aValue.Type() != nsAttrValue::eURL &&
      aValue.Type() != nsAttrValue::eImage) {
    return;
  }
  nsAutoString str;
  aValue.ToString(str);
  Servo_DeclarationBlock_SetBackgroundImage(
    mDecl, str, mDocument->DefaultStyleAttrURLData());
}
