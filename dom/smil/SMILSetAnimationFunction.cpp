/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SMILSetAnimationFunction.h"

namespace mozilla {

bool SMILSetAnimationFunction::IsDisallowedAttribute(
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

}  // namespace mozilla
