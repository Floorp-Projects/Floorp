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
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#ifndef NS_SVGTRANSFORMSMILATTR_H_
#define NS_SVGTRANSFORMSMILATTR_H_

#include "nsISMILAttr.h"
#include "nsIAtom.h"
#include "nsString.h"

class nsSVGElement;
class nsSVGAnimatedTransformList;
class nsISMILType;
class nsIDOMSVGTransform;
class nsIDOMSVGTransformList;
class nsSVGSMILTransform;

class nsSVGTransformSMILAttr : public nsISMILAttr
{
public:
  nsSVGTransformSMILAttr(nsSVGAnimatedTransformList* aTransform,
                         nsSVGElement* aSVGElement)
    : mVal(aTransform),
      mSVGElement(aSVGElement) {}

  // nsISMILAttr methods
  virtual nsISMILType* GetSMILType() const;
  virtual nsresult     ValueFromString(const nsAString& aStr,
                                   const nsISMILAnimationElement* aSrcElement,
                                   nsSMILValue& aValue) const;
  virtual nsSMILValue  GetBaseValue() const;
  virtual nsresult     SetAnimValue(const nsSMILValue& aValue);

protected:
  nsresult ParseValue(const nsAString& aSpec,
                      const nsIAtom* aTransformType,
                      nsSMILValue& aResult) const;
  PRInt32  ParseParameterList(const nsAString& aSpec, float* aVars,
                              PRInt32 aNVars) const;
  PRBool   IsSpace(const char c) const;
  void     SkipWsp(nsACString::const_iterator& aIter,
                   const nsACString::const_iterator& aIterEnd) const;
  nsresult AppendSVGTransformToSMILValue(nsIDOMSVGTransform* transform,
                                         nsSMILValue& aValue) const;
  nsresult UpdateFromSMILValue(nsIDOMSVGTransformList* aTransformList,
                               const nsSMILValue& aValue);
  nsresult GetSVGTransformFromSMILValue(
                        const nsSVGSMILTransform& aSMILTransform,
                        nsIDOMSVGTransform* aSVGTransform) const;
  already_AddRefed<nsIDOMSVGTransform> GetSVGTransformFromSMILValue(
                                        const nsSMILValue& aValue) const;

private:
  // Raw pointers are OK here because this nsSVGTransformSMILAttr is both
  // created & destroyed during a SMIL sample-step, during which time the DOM
  // isn't modified.
  nsSVGAnimatedTransformList* mVal;
  nsSVGElement* mSVGElement;
};

#endif // NS_SVGTRANSFORMSMILATTR_H_
