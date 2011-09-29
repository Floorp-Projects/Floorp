/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SMIL module.
 *
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    return PR_TRUE;
  }

  return PR_FALSE;
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
    return PR_TRUE;
  }

  return nsSMILAnimationFunction::SetAttr(aAttribute, aValue,
                                          aResult, aParseResult);
}

bool
nsSMILSetAnimationFunction::UnsetAttr(nsIAtom* aAttribute)
{
  if (IsDisallowedAttribute(aAttribute)) {
    return PR_TRUE;
  }

  return nsSMILAnimationFunction::UnsetAttr(aAttribute);
}

bool
nsSMILSetAnimationFunction::HasAttr(nsIAtom* aAttName) const
{
  if (IsDisallowedAttribute(aAttName))
    return PR_FALSE;

  return nsSMILAnimationFunction::HasAttr(aAttName);
}

const nsAttrValue*
nsSMILSetAnimationFunction::GetAttr(nsIAtom* aAttName) const
{
  if (IsDisallowedAttribute(aAttName))
    return nsnull;

  return nsSMILAnimationFunction::GetAttr(aAttName);
}

bool
nsSMILSetAnimationFunction::GetAttr(nsIAtom* aAttName,
                                    nsAString& aResult) const
{
  if (IsDisallowedAttribute(aAttName))
    return nsnull;

  return nsSMILAnimationFunction::GetAttr(aAttName, aResult);
}

bool
nsSMILSetAnimationFunction::WillReplace() const
{
  return PR_TRUE;
}
