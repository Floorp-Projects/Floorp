/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Generic representation of a container of specified CSS values, which
 * could potentially be Servo- or Gecko- format. Used to make attribute mapping
 * code generic over style backends.
 */

#ifndef mozilla_GenericSpecifiedValues_h
#define mozilla_GenericSpecifiedValues_h

#include "mozilla/ServoUtils.h"
#include "mozilla/FontPropertyTypes.h"
#include "nsCSSPropertyID.h"
#include "nsCSSValue.h"
#include "nsColor.h"

class nsAttrValue;

namespace mozilla {

class ServoSpecifiedValues;

// This provides a common interface for attribute mappers
// (MapAttributesIntoRule) to use regardless of the style backend. If the style
// backend is Gecko, this will contain an nsRuleData. If it is Servo, it will be
// a PropertyDeclarationBlock.
class GenericSpecifiedValues
{
protected:
  explicit GenericSpecifiedValues(nsIDocument* aDoc)
    : mDocument(aDoc)
  {}

public:
  MOZ_DECL_STYLO_METHODS(nsRuleData, ServoSpecifiedValues)

  nsIDocument* Document()
  {
    return mDocument;
  }

  // Check if we already contain a certain longhand
  inline bool PropertyIsSet(nsCSSPropertyID aId);

  // Set a property to an identifier (string)
  inline void SetIdentStringValue(nsCSSPropertyID aId, const nsString& aValue);
  inline void SetIdentStringValueIfUnset(nsCSSPropertyID aId,
                                         const nsString& aValue);

  inline void SetIdentAtomValue(nsCSSPropertyID aId, nsAtom* aValue);
  inline void SetIdentAtomValueIfUnset(nsCSSPropertyID aId, nsAtom* aValue);

  // Set a property to a keyword (usually NS_STYLE_* or StyleFoo::*)
  inline void SetKeywordValue(nsCSSPropertyID aId, int32_t aValue);
  inline void SetKeywordValueIfUnset(nsCSSPropertyID aId, int32_t aValue);

  template<typename T,
           typename = typename std::enable_if<std::is_enum<T>::value>::type>
  void SetKeywordValue(nsCSSPropertyID aId, T aValue)
  {
    static_assert(mozilla::EnumTypeFitsWithin<T, int32_t>::value,
                  "aValue must be an enum that fits within 32 bits");
    SetKeywordValue(aId, static_cast<int32_t>(aValue));
  }
  template<typename T,
           typename = typename std::enable_if<std::is_enum<T>::value>::type>
  void SetKeywordValueIfUnset(nsCSSPropertyID aId, T aValue)
  {
    static_assert(mozilla::EnumTypeFitsWithin<T, int32_t>::value,
                  "aValue must be an enum that fits within 32 bits");
    SetKeywordValueIfUnset(aId, static_cast<int32_t>(aValue));
  }

  // Set a property to an integer value
  inline void SetIntValue(nsCSSPropertyID aId, int32_t aValue);
  // Set a property to a pixel value
  inline void SetPixelValue(nsCSSPropertyID aId, float aValue);
  inline void SetPixelValueIfUnset(nsCSSPropertyID aId, float aValue);

  inline void SetLengthValue(nsCSSPropertyID aId, nsCSSValue aValue);

  // Set a property to a number value
  inline void SetNumberValue(nsCSSPropertyID aId, float aValue);

  // Set a property to a percent value
  inline void SetPercentValue(nsCSSPropertyID aId, float aValue);
  inline void SetPercentValueIfUnset(nsCSSPropertyID aId, float aValue);

  // Set a property to `auto`
  inline void SetAutoValue(nsCSSPropertyID aId);
  inline void SetAutoValueIfUnset(nsCSSPropertyID aId);

  // Set a property to `currentcolor`
  inline void SetCurrentColor(nsCSSPropertyID aId);
  inline void SetCurrentColorIfUnset(nsCSSPropertyID aId);

  // Set a property to an RGBA nscolor value
  inline void SetColorValue(nsCSSPropertyID aId, nscolor aValue);
  inline void SetColorValueIfUnset(nsCSSPropertyID aId, nscolor aValue);

  // Set font-family to a string
  inline void SetFontFamily(const nsString& aValue);
  // Add a quirks-mode override to the decoration color of elements nested in <a>
  inline void SetTextDecorationColorOverride();
  inline void SetBackgroundImage(nsAttrValue& value);

  nsIDocument* const mDocument;
};

} // namespace mozilla

#endif // mozilla_GenericSpecifiedValues_h
