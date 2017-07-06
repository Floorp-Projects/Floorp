/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoDeclarationBlock.h"

#include "mozilla/ServoBindings.h"

#include "nsCSSProps.h"

namespace mozilla {

/* static */ already_AddRefed<ServoDeclarationBlock>
ServoDeclarationBlock::FromCssText(const nsAString& aCssText,
                                   URLExtraData* aExtraData,
                                   nsCompatibility aMode,
                                   css::Loader* aLoader)
{
  NS_ConvertUTF16toUTF8 value(aCssText);
  // FIXME (bug 1343964): Figure out a better solution for sending the base uri to servo
  RefPtr<RawServoDeclarationBlock>
      raw = Servo_ParseStyleAttribute(&value, aExtraData, aMode, aLoader).Consume();
  RefPtr<ServoDeclarationBlock> decl = new ServoDeclarationBlock(raw.forget());
  return decl.forget();
}

void
ServoDeclarationBlock::GetPropertyValue(const nsAString& aProperty,
                                        nsAString& aValue) const
{
  NS_ConvertUTF16toUTF8 property(aProperty);
  Servo_DeclarationBlock_GetPropertyValue(mRaw, &property, &aValue);
}

void
ServoDeclarationBlock::GetPropertyValueByID(nsCSSPropertyID aPropID,
                                            nsAString& aValue) const
{
  Servo_DeclarationBlock_GetPropertyValueById(mRaw, aPropID, &aValue);
}

bool
ServoDeclarationBlock::GetPropertyIsImportant(const nsAString& aProperty) const
{
  NS_ConvertUTF16toUTF8 property(aProperty);
  return Servo_DeclarationBlock_GetPropertyIsImportant(mRaw, &property);
}

void
ServoDeclarationBlock::RemoveProperty(const nsAString& aProperty)
{
  AssertMutable();
  NS_ConvertUTF16toUTF8 property(aProperty);
  Servo_DeclarationBlock_RemoveProperty(mRaw, &property);
}

void
ServoDeclarationBlock::RemovePropertyByID(nsCSSPropertyID aPropID)
{
  AssertMutable();
  Servo_DeclarationBlock_RemovePropertyById(mRaw, aPropID);
}

} // namespace mozilla
