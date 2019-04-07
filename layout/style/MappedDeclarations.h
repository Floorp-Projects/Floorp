/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Representation of a declaration block used for attribute mapping */

#ifndef mozilla_MappedDeclarations_h
#define mozilla_MappedDeclarations_h

#include "mozilla/FontPropertyTypes.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/ServoBindings.h"
#include "nsCSSPropertyID.h"
#include "nsCSSValue.h"
#include "nsColor.h"

class nsAttrValue;

namespace mozilla {

// This provides a convenient interface for attribute mappers
// (MapAttributesIntoRule) to modify the presentation attribute declaration
// block for a given element.
class MappedDeclarations final {
 public:
  explicit MappedDeclarations(dom::Document* aDoc,
                              already_AddRefed<RawServoDeclarationBlock> aDecls)
      : mDocument(aDoc), mDecl(aDecls) {
    MOZ_ASSERT(mDecl);
  }

  ~MappedDeclarations() { MOZ_ASSERT(!mDecl, "Forgot to take the block?"); }

  dom::Document* Document() { return mDocument; }

  already_AddRefed<RawServoDeclarationBlock> TakeDeclarationBlock() {
    MOZ_ASSERT(mDecl);
    return mDecl.forget();
  }

  // Check if we already contain a certain longhand
  bool PropertyIsSet(nsCSSPropertyID aId) const {
    return Servo_DeclarationBlock_PropertyIsSet(mDecl, aId);
  }

  // Set a property to an identifier (string)
  void SetIdentStringValue(nsCSSPropertyID aId, const nsString& aValue) {
    RefPtr<nsAtom> atom = NS_AtomizeMainThread(aValue);
    SetIdentAtomValue(aId, atom);
  }

  void SetIdentStringValueIfUnset(nsCSSPropertyID aId, const nsString& aValue) {
    if (!PropertyIsSet(aId)) {
      SetIdentStringValue(aId, aValue);
    }
  }

  void SetIdentAtomValue(nsCSSPropertyID aId, nsAtom* aValue);

  void SetIdentAtomValueIfUnset(nsCSSPropertyID aId, nsAtom* aValue) {
    if (!PropertyIsSet(aId)) {
      SetIdentAtomValue(aId, aValue);
    }
  }

  // Set a property to a keyword (usually NS_STYLE_* or StyleFoo::*)
  void SetKeywordValue(nsCSSPropertyID aId, int32_t aValue) {
    Servo_DeclarationBlock_SetKeywordValue(mDecl, aId, aValue);
  }

  void SetKeywordValueIfUnset(nsCSSPropertyID aId, int32_t aValue) {
    if (!PropertyIsSet(aId)) {
      SetKeywordValue(aId, aValue);
    }
  }

  template <typename T,
            typename = typename std::enable_if<std::is_enum<T>::value>::type>
  void SetKeywordValue(nsCSSPropertyID aId, T aValue) {
    static_assert(mozilla::EnumTypeFitsWithin<T, int32_t>::value,
                  "aValue must be an enum that fits within 32 bits");
    SetKeywordValue(aId, static_cast<int32_t>(aValue));
  }
  template <typename T,
            typename = typename std::enable_if<std::is_enum<T>::value>::type>
  void SetKeywordValueIfUnset(nsCSSPropertyID aId, T aValue) {
    static_assert(mozilla::EnumTypeFitsWithin<T, int32_t>::value,
                  "aValue must be an enum that fits within 32 bits");
    SetKeywordValueIfUnset(aId, static_cast<int32_t>(aValue));
  }

  // Set a property to an integer value
  void SetIntValue(nsCSSPropertyID aId, int32_t aValue) {
    Servo_DeclarationBlock_SetIntValue(mDecl, aId, aValue);
  }

  // Set "counter-reset: list-item <integer>".
  void SetCounterResetListItem(int32_t aValue) {
    Servo_DeclarationBlock_SetCounterResetListItem(mDecl, aValue);
  }

  // Set "counter-set: list-item <integer>".
  void SetCounterSetListItem(int32_t aValue) {
    Servo_DeclarationBlock_SetCounterSetListItem(mDecl, aValue);
  }

  // Set a property to a pixel value
  void SetPixelValue(nsCSSPropertyID aId, float aValue) {
    Servo_DeclarationBlock_SetPixelValue(mDecl, aId, aValue);
  }

  void SetPixelValueIfUnset(nsCSSPropertyID aId, float aValue) {
    if (!PropertyIsSet(aId)) {
      SetPixelValue(aId, aValue);
    }
  }

  void SetLengthValue(nsCSSPropertyID aId, const nsCSSValue& aValue) {
    MOZ_ASSERT(aValue.IsLengthUnit());
    Servo_DeclarationBlock_SetLengthValue(mDecl, aId, aValue.GetFloatValue(),
                                          aValue.GetUnit());
  }

  // Set a property to a number value
  void SetNumberValue(nsCSSPropertyID aId, float aValue) {
    Servo_DeclarationBlock_SetNumberValue(mDecl, aId, aValue);
  }

  // Set a property to a percent value
  void SetPercentValue(nsCSSPropertyID aId, float aValue) {
    Servo_DeclarationBlock_SetPercentValue(mDecl, aId, aValue);
  }

  void SetPercentValueIfUnset(nsCSSPropertyID aId, float aValue) {
    if (!PropertyIsSet(aId)) {
      SetPercentValue(aId, aValue);
    }
  }

  // Set a property to `auto`
  void SetAutoValue(nsCSSPropertyID aId) {
    Servo_DeclarationBlock_SetAutoValue(mDecl, aId);
  }

  void SetAutoValueIfUnset(nsCSSPropertyID aId) {
    if (!PropertyIsSet(aId)) {
      SetAutoValue(aId);
    }
  }

  // Set a property to `currentcolor`
  void SetCurrentColor(nsCSSPropertyID aId) {
    Servo_DeclarationBlock_SetCurrentColor(mDecl, aId);
  }

  void SetCurrentColorIfUnset(nsCSSPropertyID aId) {
    if (!PropertyIsSet(aId)) {
      SetCurrentColor(aId);
    }
  }

  // Set a property to an RGBA nscolor value
  void SetColorValue(nsCSSPropertyID aId, nscolor aValue) {
    Servo_DeclarationBlock_SetColorValue(mDecl, aId, aValue);
  }

  void SetColorValueIfUnset(nsCSSPropertyID aId, nscolor aValue) {
    if (!PropertyIsSet(aId)) {
      SetColorValue(aId, aValue);
    }
  }

  // Set font-family to a string
  void SetFontFamily(const nsString& aValue) {
    Servo_DeclarationBlock_SetFontFamily(mDecl, &aValue);
  }

  // Add a quirks-mode override to the decoration color of elements nested in
  // <a>
  void SetTextDecorationColorOverride() {
    Servo_DeclarationBlock_SetTextDecorationColorOverride(mDecl);
  }

  void SetBackgroundImage(const nsAttrValue& value);

 private:
  dom::Document* const mDocument;
  RefPtr<RawServoDeclarationBlock> mDecl;
};

}  // namespace mozilla

#endif  // mozilla_MappedDeclarations_h
