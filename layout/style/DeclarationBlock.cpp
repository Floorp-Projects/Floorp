/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DeclarationBlock.h"

#include "mozilla/ServoBindings.h"

#include "nsCSSProps.h"

namespace mozilla {

/* static */ already_AddRefed<DeclarationBlock>
DeclarationBlock::FromCssText(const nsAString& aCssText,
                              URLExtraData* aExtraData,
                              nsCompatibility aMode,
                              css::Loader* aLoader)
{
  NS_ConvertUTF16toUTF8 value(aCssText);
  RefPtr<RawServoDeclarationBlock>
      raw = Servo_ParseStyleAttribute(&value, aExtraData, aMode, aLoader).Consume();
  RefPtr<DeclarationBlock> decl = new DeclarationBlock(raw.forget());
  return decl.forget();
}

// TODO: We can make them inline.
void
DeclarationBlock::GetPropertyValue(const nsAString& aProperty,
                                   nsAString& aValue) const
{
  NS_ConvertUTF16toUTF8 property(aProperty);
  Servo_DeclarationBlock_GetPropertyValue(mRaw, &property, &aValue);
}

void
DeclarationBlock::GetPropertyValueByID(nsCSSPropertyID aPropID,
                                       nsAString& aValue) const
{
  Servo_DeclarationBlock_GetPropertyValueById(mRaw, aPropID, &aValue);
}

bool
DeclarationBlock::GetPropertyIsImportant(const nsAString& aProperty) const
{
  NS_ConvertUTF16toUTF8 property(aProperty);
  return Servo_DeclarationBlock_GetPropertyIsImportant(mRaw, &property);
}

bool
DeclarationBlock::RemoveProperty(const nsAString& aProperty)
{
  AssertMutable();
  NS_ConvertUTF16toUTF8 property(aProperty);
  return Servo_DeclarationBlock_RemoveProperty(mRaw, &property);
}

bool
DeclarationBlock::RemovePropertyByID(nsCSSPropertyID aPropID)
{
  AssertMutable();
  return Servo_DeclarationBlock_RemovePropertyById(mRaw, aPropID);
}

} // namespace mozilla
