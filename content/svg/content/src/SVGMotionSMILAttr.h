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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com>
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

/* representation of a dummy attribute targeted by <animateMotion> element */

#ifndef MOZILLA_SVGMOTIONSMILATTR_H_
#define MOZILLA_SVGMOTIONSMILATTR_H_

#include "nsISMILAttr.h"
class nsSVGElement;

namespace mozilla {

/**
 * SVGMotionSMILAttr: Implements the nsISMILAttr interface for SMIL animations
 * from <animateMotion>.
 *
 * NOTE: Even though there's technically no "motion" attribute, we behave in
 * many ways as if there were, for simplicity.
 */
class SVGMotionSMILAttr : public nsISMILAttr
{
public:
  SVGMotionSMILAttr(nsSVGElement* aSVGElement)
    : mSVGElement(aSVGElement) {}

  // nsISMILAttr methods
  virtual nsresult ValueFromString(const nsAString& aStr,
                                   const nsISMILAnimationElement* aSrcElement,
                                   nsSMILValue& aValue,
                                   bool& aPreventCachingOfSandwich) const;
  virtual nsSMILValue GetBaseValue() const;
  virtual nsresult    SetAnimValue(const nsSMILValue& aValue);
  virtual void        ClearAnimValue();
  virtual const nsIContent* GetTargetNode() const;

protected:
  // Raw pointers are OK here because this SVGMotionSMILAttr is both
  // created & destroyed during a SMIL sample-step, during which time the DOM
  // isn't modified.
  nsSVGElement* mSVGElement;
};

} // namespace mozilla

#endif // MOZILLA_SVGMOTIONSMILATTR_H_
