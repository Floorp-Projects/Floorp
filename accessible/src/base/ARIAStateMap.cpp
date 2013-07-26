/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ARIAMap.h"
#include "nsAccUtils.h"
#include "States.h"

#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::a11y::aria;

/**
 * Used to store state map rule data for ARIA attribute of enum type.
 */
struct EnumTypeData
{
  EnumTypeData(nsIAtom* aAttrName,
               nsIAtom** aValue1, uint64_t aState1,
               nsIAtom** aValue2, uint64_t aState2,
               nsIAtom** aValue3 = 0, uint64_t aState3 = 0) :
    mState1(aState1), mState2(aState2), mState3(aState3), mDefaultState(0),
    mAttrName(aAttrName), mValue1(aValue1), mValue2(aValue2), mValue3(aValue3),
    mNullValue(nullptr)
  { }

  EnumTypeData(nsIAtom* aAttrName, uint64_t aDefaultState,
               nsIAtom** aValue1, uint64_t aState1) :
    mState1(aState1), mState2(0), mState3(0), mDefaultState(aDefaultState),
    mAttrName(aAttrName), mValue1(aValue1), mValue2(nullptr), mValue3(nullptr),
    mNullValue(nullptr)
  { }

  // States applied if corresponding enum values are matched.
  const uint64_t mState1;
  const uint64_t mState2;
  const uint64_t mState3;

  // Default state if no one enum value is matched.
  const uint64_t mDefaultState;

  // ARIA attribute name.
  nsIAtom* const mAttrName;

  // States if the attribute value is matched to the enum value. Used as
  // nsIContent::AttrValuesArray.
  nsIAtom* const* const mValue1;
  nsIAtom* const* const mValue2;
  nsIAtom* const* const mValue3;
  nsIAtom* const* const mNullValue;
};

enum ETokenType
{
  eBoolType = 0,
  eMixedType = 1, // can take 'mixed' value
  eDefinedIfAbsent = 2 // permanent and false state are applied if absent
};

/**
 * Used to store state map rule data for ARIA attribute of token type (including
 * mixed value).
 */
struct TokenTypeData
{
  TokenTypeData(nsIAtom* aAttrName, uint32_t aType,
                uint64_t aPermanentState,
                uint64_t aTrueState,
                uint64_t aFalseState = 0) :
  mAttrName(aAttrName), mType(aType), mPermanentState(aPermanentState),
  mTrueState(aTrueState), mFalseState(aFalseState)
  { }

  // ARIA attribute name.
  nsIAtom* const mAttrName;

  // Type.
  const uint32_t mType;

  // State applied if the attribute is defined or mType doesn't have
  // eDefinedIfAbsent flag set.
  const uint64_t mPermanentState;

  // States applied if the attribute value is true/false.
  const uint64_t mTrueState;
  const uint64_t mFalseState;
};

/**
 * Map enum type attribute value to accessible state.
 */
static void MapEnumType(dom::Element* aElement, uint64_t* aState,
                        const EnumTypeData& aData);

/**
 * Map token type attribute value to states.
 */
static void MapTokenType(dom::Element* aContent, uint64_t* aState,
                         const TokenTypeData& aData);

bool
aria::MapToState(EStateRule aRule, dom::Element* aElement, uint64_t* aState)
{
  switch (aRule) {
    case eARIAAutoComplete:
    {
      static const EnumTypeData data(
        nsGkAtoms::aria_autocomplete,
        &nsGkAtoms::inlinevalue, states::SUPPORTS_AUTOCOMPLETION,
        &nsGkAtoms::list, states::HASPOPUP | states::SUPPORTS_AUTOCOMPLETION,
        &nsGkAtoms::both, states::HASPOPUP | states::SUPPORTS_AUTOCOMPLETION);

      MapEnumType(aElement, aState, data);
      return true;
    }

    case eARIABusy:
    {
      static const EnumTypeData data(
        nsGkAtoms::aria_busy,
        &nsGkAtoms::_true, states::BUSY,
        &nsGkAtoms::error, states::INVALID);

      MapEnumType(aElement, aState, data);
      return true;
    }

    case eARIACheckableBool:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_checked, eBoolType | eDefinedIfAbsent,
        states::CHECKABLE, states::CHECKED);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eARIACheckableMixed:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_checked, eMixedType | eDefinedIfAbsent,
        states::CHECKABLE, states::CHECKED);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eARIACheckedMixed:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_checked, eMixedType,
        states::CHECKABLE, states::CHECKED);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eARIADisabled:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_disabled, eBoolType,
        0, states::UNAVAILABLE);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eARIAExpanded:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_expanded, eBoolType,
        0, states::EXPANDED, states::COLLAPSED);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eARIAHasPopup:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_haspopup, eBoolType,
        0, states::HASPOPUP);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eARIAInvalid:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_invalid, eBoolType,
        0, states::INVALID);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eARIAMultiline:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_multiline, eBoolType | eDefinedIfAbsent,
        0, states::MULTI_LINE, states::SINGLE_LINE);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eARIAMultiSelectable:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_multiselectable, eBoolType,
        0, states::MULTISELECTABLE | states::EXTSELECTABLE);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eARIAOrientation:
    {
      if (aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_orientation,
                                NS_LITERAL_STRING("horizontal"), eCaseMatters)) {
        *aState &= ~states::VERTICAL;
        *aState |= states::HORIZONTAL;
      } else if (aElement->AttrValueIs(kNameSpaceID_None,
                                       nsGkAtoms::aria_orientation,
                                       NS_LITERAL_STRING("vertical"),
                                       eCaseMatters)) {
        *aState &= ~states::HORIZONTAL;
        *aState |= states::VERTICAL;
      } else {
        NS_ASSERTION(!(*aState & (states::HORIZONTAL | states::VERTICAL)),
                     "orientation state on role with default aria-orientation!");
        *aState |= GetRoleMap(aElement)->Is(nsGkAtoms::scrollbar) ?
          states::VERTICAL : states::HORIZONTAL;
      }

      return true;
    }

    case eARIAPressed:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_pressed, eMixedType,
        states::CHECKABLE, states::PRESSED);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eARIAReadonly:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_readonly, eBoolType,
        0, states::READONLY);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eARIAReadonlyOrEditable:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_readonly, eBoolType | eDefinedIfAbsent,
        0, states::READONLY, states::EDITABLE);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eARIAReadonlyOrEditableIfDefined:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_readonly, eBoolType,
        0, states::READONLY, states::EDITABLE);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eARIARequired:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_required, eBoolType,
        0, states::REQUIRED);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eARIASelectable:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_selected, eBoolType | eDefinedIfAbsent,
        states::SELECTABLE, states::SELECTED);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eARIASelectableIfDefined:
    {
      static const TokenTypeData data(
        nsGkAtoms::aria_selected, eBoolType,
        states::SELECTABLE, states::SELECTED);

      MapTokenType(aElement, aState, data);
      return true;
    }

    case eReadonlyUntilEditable:
    {
      if (!(*aState & states::EDITABLE))
        *aState |= states::READONLY;

      return true;
    }

    case eIndeterminateIfNoValue:
    {
      if (!aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_valuenow) &&
          !aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_valuetext))
        *aState |= states::MIXED;

      return true;
    }

    case eFocusableUntilDisabled:
    {
      if (!nsAccUtils::HasDefinedARIAToken(aElement, nsGkAtoms::aria_disabled) ||
          aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_disabled,
                                nsGkAtoms::_false, eCaseMatters))
        *aState |= states::FOCUSABLE;

      return true;
    }

    default:
      return false;
  }
}

static void
MapEnumType(dom::Element* aElement, uint64_t* aState, const EnumTypeData& aData)
{
  switch (aElement->FindAttrValueIn(kNameSpaceID_None, aData.mAttrName,
                                    &aData.mValue1, eCaseMatters)) {
    case 0:
      *aState |= aData.mState1;
      return;
    case 1:
      *aState |= aData.mState2;
      return;
    case 2:
      *aState |= aData.mState3;
      return;
  }

  *aState |= aData.mDefaultState;
}

static void
MapTokenType(dom::Element* aElement, uint64_t* aState,
             const TokenTypeData& aData)
{
  if (aElement->HasAttr(kNameSpaceID_None, aData.mAttrName)) {
    if ((aData.mType & eMixedType) &&
        aElement->AttrValueIs(kNameSpaceID_None, aData.mAttrName,
                              nsGkAtoms::mixed, eCaseMatters)) {
      *aState |= aData.mPermanentState | states::MIXED;
      return;
    }

    if (aElement->AttrValueIs(kNameSpaceID_None, aData.mAttrName,
                              nsGkAtoms::_false, eCaseMatters)) {
      *aState |= aData.mPermanentState | aData.mFalseState;
      return;
    }

    if (!aElement->AttrValueIs(kNameSpaceID_None, aData.mAttrName,
                               nsGkAtoms::_undefined, eCaseMatters) &&
        !aElement->AttrValueIs(kNameSpaceID_None, aData.mAttrName,
                               nsGkAtoms::_empty, eCaseMatters)) {
      *aState |= aData.mPermanentState | aData.mTrueState;
    }
    return;
  }

  if (aData.mType & eDefinedIfAbsent)
    *aState |= aData.mPermanentState | aData.mFalseState;
}
