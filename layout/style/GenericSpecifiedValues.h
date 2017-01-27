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

#include "nsCSSProps.h"
#include "nsCSSValue.h"
#include "nsPresContext.h"

struct nsRuleData;

// This provides a common interface for attribute mappers (MapAttributesIntoRule)
// to use regardless of the style backend. If the style backend is Gecko,
// this will contain an nsRuleData. If it is Servo, it will be a PropertyDeclarationBlock.
class GenericSpecifiedValues {
public:
    // Check if we already contain a certain longhand
    virtual bool PropertyIsSet(nsCSSPropertyID aId) = 0;
    // Check if we are able to hold longhands from a given
    // style struct. Pass the result of NS_STYLE_INHERIT_BIT to this
    // function. Can accept multiple inherit bits or'd together.
    virtual bool ShouldComputeStyleStruct(uint64_t aInheritBits) = 0;

    virtual nsPresContext* PresContext() = 0;

    // Set a property to an identifier (string)
    virtual void SetIdentStringValue(nsCSSPropertyID aId, const nsString& aValue) = 0;
    virtual void SetIdentStringValueIfUnset(nsCSSPropertyID aId, const nsString& aValue) = 0;

    // Set a property to a keyword (usually NS_STYLE_* or StyleFoo::*)
    virtual void SetKeywordValue(nsCSSPropertyID aId, int32_t aValue) = 0;
    virtual void SetKeywordValueIfUnset(nsCSSPropertyID aId, int32_t aValue) = 0;

    template<typename T,
             typename = typename std::enable_if<std::is_enum<T>::value>::type>
    void SetKeywordValue(nsCSSPropertyID aId, T aValue) {
        static_assert(mozilla::EnumTypeFitsWithin<T, int32_t>::value,
                      "aValue must be an enum that fits within 32 bits");
        SetKeywordValue(aId, static_cast<int32_t>(aValue));
    }
    template<typename T,
             typename = typename std::enable_if<std::is_enum<T>::value>::type>
    void SetKeywordValueIfUnset(nsCSSPropertyID aId, T aValue) {
        static_assert(mozilla::EnumTypeFitsWithin<T, int32_t>::value,
                      "aValue must be an enum that fits within 32 bits");
        SetKeywordValueIfUnset(aId, static_cast<int32_t>(aValue));
    }

    // Set a property to a pixel value
    virtual void SetPixelValue(nsCSSPropertyID aId, float aValue) = 0;
    virtual void SetPixelValueIfUnset(nsCSSPropertyID aId, float aValue) = 0;

    // Set a property to a percent value
    virtual void SetPercentValue(nsCSSPropertyID aId, float aValue) = 0;
    virtual void SetPercentValueIfUnset(nsCSSPropertyID aId, float aValue) = 0;

    // Set a property to `currentcolor`
    virtual void SetCurrentColor(nsCSSPropertyID aId) = 0;
    virtual void SetCurrentColorIfUnset(nsCSSPropertyID aId) = 0;

    // Set a property to an RGBA nscolor value
    virtual void SetColorValue(nsCSSPropertyID aId, nscolor aValue) = 0;

    virtual nsRuleData* AsRuleData() = 0;
};

#endif // mozilla_GenericSpecifiedValues_h