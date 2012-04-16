/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _mozilla_a11y_aria_ARIAStateMap_h_
#define _mozilla_a11y_aria_ARIAStateMap_h_

#include "prtypes.h"

namespace mozilla {

namespace dom {
class Element;
}

namespace a11y {
namespace aria {

/**
 * List of the ARIA state mapping rules.
 */
enum EStateRule
{
  eARIANone,
  eARIAAutoComplete,
  eARIABusy,
  eARIACheckableBool,
  eARIACheckableMixed,
  eARIACheckedMixed,
  eARIADisabled,
  eARIAExpanded,
  eARIAHasPopup,
  eARIAInvalid,
  eARIAMultiline,
  eARIAMultiSelectable,
  eARIAOrientation,
  eARIAPressed,
  eARIAReadonly,
  eARIAReadonlyOrEditable,
  eARIARequired,
  eARIASelectable,
  eReadonlyUntilEditable,
  eIndeterminateIfNoValue
};

/**
 * Expose the accessible states for the given element accordingly to state
 * mapping rule.
 *
 * @param  aRule     [in] state mapping rule ID
 * @param  aElement  [in] node of the accessible
 * @param  aState    [in/out] accessible states
 * @return            true if state map rule ID is valid
 */
bool MapToState(EStateRule aRule, dom::Element* aElement, PRUint64* aState);

} // namespace aria
} // namespace a11y
} // namespace mozilla

#endif
