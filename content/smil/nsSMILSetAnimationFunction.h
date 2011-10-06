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

#ifndef NS_SMILSETANIMATIONFUNCTION_H_
#define NS_SMILSETANIMATIONFUNCTION_H_

#include "nsSMILAnimationFunction.h"

//----------------------------------------------------------------------
// nsSMILSetAnimationFunction
//
// Subclass of nsSMILAnimationFunction that limits the behaviour to that offered
// by a <set> element.
//
class nsSMILSetAnimationFunction : public nsSMILAnimationFunction
{
public:
  /*
   * Sets animation-specific attributes (or marks them dirty, in the case
   * of from/to/by/values).
   *
   * @param aAttribute The attribute being set
   * @param aValue     The updated value of the attribute.
   * @param aResult    The nsAttrValue object that may be used for storing the
   *                   parsed result.
   * @param aParseResult  Outparam used for reporting parse errors. Will be set
   *                      to NS_OK if everything succeeds.
   * @returns PR_TRUE if aAttribute is a recognized animation-related
   *          attribute; PR_FALSE otherwise.
   */
  virtual bool SetAttr(nsIAtom* aAttribute, const nsAString& aValue,
                         nsAttrValue& aResult, nsresult* aParseResult = nsnull);

  /*
   * Unsets the given attribute.
   *
   * @returns PR_TRUE if aAttribute is a recognized animation-related
   *          attribute; PR_FALSE otherwise.
   */
  NS_OVERRIDE virtual bool UnsetAttr(nsIAtom* aAttribute);

protected:
  // Although <set> animation might look like to-animation, unlike to-animation,
  // it never interpolates values.
  // Returning PR_FALSE here will mean this animation function gets treated as
  // a single-valued function and no interpolation will be attempted.
  NS_OVERRIDE virtual bool IsToAnimation() const {
    return PR_FALSE;
  }

  // <set> applies the exact same value across the simple duration.
  NS_OVERRIDE virtual bool IsValueFixedForSimpleDuration() const {
    return PR_TRUE;
  }
  NS_OVERRIDE virtual bool               HasAttr(nsIAtom* aAttName) const;
  NS_OVERRIDE virtual const nsAttrValue* GetAttr(nsIAtom* aAttName) const;
  NS_OVERRIDE virtual bool               GetAttr(nsIAtom* aAttName,
                                                 nsAString& aResult) const;
  NS_OVERRIDE virtual bool WillReplace() const;

  bool IsDisallowedAttribute(const nsIAtom* aAttribute) const;
};

#endif // NS_SMILSETANIMATIONFUNCTION_H_
