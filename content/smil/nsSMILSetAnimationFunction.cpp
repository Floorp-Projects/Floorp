/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSMILSetAnimationFunction.h"

inline bool
nsSMILSetAnimationFunction::IsDisallowedAttribute(
    const nsIAtom* aAttribute) const
{
  //
  // A <set> element is similar to <animate> but lacks:
  //   AnimationValue.attrib(calcMode, values, keyTimes, keySplines, from, to,
  //                         by) -- BUT has 'to'
  //   AnimationAddition.attrib(additive, accumulate)
  //
  if (aAttribute == nsGkAtoms::calcMode ||
      aAttribute == nsGkAtoms::values ||
      aAttribute == nsGkAtoms::keyTimes ||
      aAttribute == nsGkAtoms::keySplines ||
      aAttribute == nsGkAtoms::from ||
      aAttribute == nsGkAtoms::by ||
      aAttribute == nsGkAtoms::additive ||
      aAttribute == nsGkAtoms::accumulate) {
    return true;
  }

  return false;
}

bool
nsSMILSetAnimationFunction::SetAttr(nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult,
                                    nsresult* aParseResult)
{
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

  return nsSMILAnimationFunction::SetAttr(aAttribute, aValue,
                                          aResult, aParseResult);
}

bool
nsSMILSetAnimationFunction::UnsetAttr(nsIAtom* aAttribute)
{
  if (IsDisallowedAttribute(aAttribute)) {
    return true;
  }

  return nsSMILAnimationFunction::UnsetAttr(aAttribute);
}

bool
nsSMILSetAnimationFunction::HasAttr(nsIAtom* aAttName) const
{
  if (IsDisallowedAttribute(aAttName))
    return false;

  return nsSMILAnimationFunction::HasAttr(aAttName);
}

const nsAttrValue*
nsSMILSetAnimationFunction::GetAttr(nsIAtom* aAttName) const
{
  if (IsDisallowedAttribute(aAttName))
    return nullptr;

  return nsSMILAnimationFunction::GetAttr(aAttName);
}

bool
nsSMILSetAnimationFunction::GetAttr(nsIAtom* aAttName,
                                    nsAString& aResult) const
{
  if (IsDisallowedAttribute(aAttName))
    return nullptr;

  return nsSMILAnimationFunction::GetAttr(aAttName, aResult);
}

bool
nsSMILSetAnimationFunction::WillReplace() const
{
  return true;
}
