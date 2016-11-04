/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoDeclarationBlock.h"

#include "mozilla/ServoBindings.h"

#include "nsCSSProps.h"

namespace mozilla {

/* static */ already_AddRefed<ServoDeclarationBlock>
ServoDeclarationBlock::FromCssText(const nsAString& aCssText)
{
  NS_ConvertUTF16toUTF8 value(aCssText);
  RefPtr<RawServoDeclarationBlock>
    raw = Servo_ParseStyleAttribute(&value).Consume();
  RefPtr<ServoDeclarationBlock> decl = new ServoDeclarationBlock(raw.forget());
  return decl.forget();
}

/**
 * An RAII class holding an atom for the given property.
 */
class MOZ_STACK_CLASS PropertyAtomHolder
{
public:
  explicit PropertyAtomHolder(const nsAString& aProperty)
  {
    nsCSSPropertyID propID =
      nsCSSProps::LookupProperty(aProperty, CSSEnabledState::eForAllContent);
    if (propID == eCSSPropertyExtra_variable) {
      mIsCustomProperty = true;
      mAtom = NS_Atomize(
        Substring(aProperty, CSS_CUSTOM_NAME_PREFIX_LENGTH)).take();
    } else {
      mIsCustomProperty = false;
      if (propID != eCSSProperty_UNKNOWN) {
        mAtom = nsCSSProps::AtomForProperty(propID);
      } else {
        mAtom = nullptr;
      }
    }
  }

  ~PropertyAtomHolder()
  {
    if (mIsCustomProperty) {
      NS_RELEASE(mAtom);
    }
  }

  explicit operator bool() const { return !!mAtom; }
  nsIAtom* Atom() const { MOZ_ASSERT(mAtom); return mAtom; }
  bool IsCustomProperty() const { return mIsCustomProperty; }

private:
  nsIAtom* mAtom;
  bool mIsCustomProperty;
};

void
ServoDeclarationBlock::GetPropertyValue(const nsAString& aProperty,
                                        nsAString& aValue) const
{
  if (PropertyAtomHolder holder{aProperty}) {
    Servo_DeclarationBlock_GetPropertyValue(
      mRaw, holder.Atom(), holder.IsCustomProperty(), &aValue);
  }
}

void
ServoDeclarationBlock::GetPropertyValueByID(nsCSSPropertyID aPropID,
                                            nsAString& aValue) const
{
  nsIAtom* atom = nsCSSProps::AtomForProperty(aPropID);
  Servo_DeclarationBlock_GetPropertyValue(mRaw, atom, false, &aValue);
}

bool
ServoDeclarationBlock::GetPropertyIsImportant(const nsAString& aProperty) const
{
  if (PropertyAtomHolder holder{aProperty}) {
    return Servo_DeclarationBlock_GetPropertyIsImportant(
      mRaw, holder.Atom(), holder.IsCustomProperty());
  }
  return false;
}

void
ServoDeclarationBlock::RemoveProperty(const nsAString& aProperty)
{
  AssertMutable();
  if (PropertyAtomHolder holder{aProperty}) {
    Servo_DeclarationBlock_RemoveProperty(mRaw, holder.Atom(),
                                          holder.IsCustomProperty());
  }
}

void
ServoDeclarationBlock::RemovePropertyByID(nsCSSPropertyID aPropID)
{
  AssertMutable();
  nsIAtom* atom = nsCSSProps::AtomForProperty(aPropID);
  Servo_DeclarationBlock_RemoveProperty(mRaw, atom, false);
}

} // namespace mozilla
