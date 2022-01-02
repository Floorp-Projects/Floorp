/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SMILSetAnimationFunction.h"

namespace mozilla {

inline bool SMILSetAnimationFunction::IsDisallowedAttribute(
    const nsAtom* aAttribute) const {
  //
  // A <set> element is similar to <animate> but lacks:
  //   AnimationValue.attrib(calcMode, values, keyTimes, keySplines, from, to,
  //                         by) -- BUT has 'to'
  //   AnimationAddition.attrib(additive, accumulate)
  //
  return aAttribute == nsGkAtoms::calcMode || aAttribute == nsGkAtoms::values ||
         aAttribute == nsGkAtoms::keyTimes ||
         aAttribute == nsGkAtoms::keySplines || aAttribute == nsGkAtoms::from ||
         aAttribute == nsGkAtoms::by || aAttribute == nsGkAtoms::additive ||
         aAttribute == nsGkAtoms::accumulate;
}

bool SMILSetAnimationFunction::SetAttr(nsAtom* aAttribute,
                                       const nsAString& aValue,
                                       nsAttrValue& aResult,
                                       nsresult* aParseResult) {
  if (IsDisallowedAttribute(aAttribute)) {
    aResult.SetTo(aValue);
    if (aParseResult) {
      // SMILANIM 4.2 says:
      //
      //   The additive and accumulate attributes are not allowed, and will be
      //   ignored if specified.
      //
      // So at least for those two attributes we shouldn't report an error even
      // if they're present. For now we'll also just silently ignore other
      // attribute types too.
      *aParseResult = NS_OK;
    }
    return true;
  }

  return SMILAnimationFunction::SetAttr(aAttribute, aValue, aResult,
                                        aParseResult);
}

bool SMILSetAnimationFunction::UnsetAttr(nsAtom* aAttribute) {
  if (IsDisallowedAttribute(aAttribute)) {
    return true;
  }

  return SMILAnimationFunction::UnsetAttr(aAttribute);
}

bool SMILSetAnimationFunction::HasAttr(nsAtom* aAttName) const {
  if (IsDisallowedAttribute(aAttName)) return false;

  return SMILAnimationFunction::HasAttr(aAttName);
}

const nsAttrValue* SMILSetAnimationFunction::GetAttr(nsAtom* aAttName) const {
  if (IsDisallowedAttribute(aAttName)) return nullptr;

  return SMILAnimationFunction::GetAttr(aAttName);
}

bool SMILSetAnimationFunction::GetAttr(nsAtom* aAttName,
                                       nsAString& aResult) const {
  if (IsDisallowedAttribute(aAttName)) return false;

  return SMILAnimationFunction::GetAttr(aAttName, aResult);
}

bool SMILSetAnimationFunction::WillReplace() const { return true; }

}  // namespace mozilla
