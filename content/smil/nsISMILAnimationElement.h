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
 * The Initial Developer of the Original Code is
 * the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
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

#ifndef NS_ISMILANIMATIONELEMENT_H_
#define NS_ISMILANIMATIONELEMENT_H_

#include "nsISupports.h"
#include "nsIContent.h"

class nsISMILAttr;

//////////////////////////////////////////////////////////////////////////////
// nsISMILAnimationElement: Interface for elements that control the animation of
// some property of another element, e.g. <animate>, <set>.

#define NS_ISMILANIMATIONELEMENT_IID \
{ 0x70ac6eed, 0x0dba, 0x4c11, { 0xa6, 0xc5, 0x15, 0x73, 0xbc, 0x2f, 0x1a, 0xd8 } }

class nsSMILAnimationFunction;
class nsSMILTimeContainer;
class nsSMILTimedElement;

enum nsSMILTargetAttrType {
  eSMILTargetAttrType_auto,
  eSMILTargetAttrType_CSS,
  eSMILTargetAttrType_XML
};

class nsISMILAnimationElement : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISMILANIMATIONELEMENT_IID)

  /*
   * Returns this element as nsIContent.
   */
  virtual const nsIContent& Content() const = 0;

  /*
   * Non-const version of Content()
   */
  virtual nsIContent& Content() = 0;

  /*
   * Returns the source attribute as an nsAttrValue. The global namespace will
   * be used.
   *
   * (The 'Anim' here and below is largely to avoid conflicts for subclasses
   * that derive from nsGenericElement)
   *
   * @param aName the name of the attr
   * @returns PR_TRUE if the attribute was set (even when set to empty string)
   *          PR_FALSE when not set.
   */
  virtual const nsAttrValue* GetAnimAttr(nsIAtom* aName) const = 0;

  /*
   * Get the current value of an attribute as a string. The global namespace
   * will be used.
   *
   * @param aName the name of the attr
   * @param aResult the value (may legitimately be the empty string) [OUT]
   * @returns PR_TRUE if the attribute was set (even when set to empty string)
   *          PR_FALSE when not set.
   */
  PRBool GetAnimAttr(nsIAtom* aAttName, nsAString &aResult) const
  {
    return Content().GetAttr(kNameSpaceID_None, aAttName, aResult);
  }

  /*
   * Check for the presence of an attribute in the global namespace.
   */
  PRBool HasAnimAttr(nsIAtom* aAttName) const
  {
    return Content().HasAttr(kNameSpaceID_None, aAttName);
  }

  /*
   * Returns the target (animated) element.
   */
  virtual nsIContent* GetTargetElementContent() = 0;

  /*
   * Returns the name of the target (animated) attribute or property.
   */
  virtual nsIAtom* GetTargetAttributeName() const = 0;

  /*
   * Returns the type of the target (animated) attribute or property.
   */
  virtual nsSMILTargetAttrType GetTargetAttributeType() const = 0;

  /*
   * Returns the SMIL animation function associated with this animation element.
   *
   * The animation function is owned by the animation element.
   */
  virtual nsSMILAnimationFunction& AnimationFunction() = 0;

  /*
   * Returns the SMIL timed element associated with this animation element.
   *
   * The timed element is owned by the animation element.
   */
  virtual nsSMILTimedElement& TimedElement() = 0;

  /*
   * Returns the SMIL timed container root with which this animation element is
   * associated (if any).
   */
  virtual nsSMILTimeContainer* GetTimeContainer() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsISMILAnimationElement,
                              NS_ISMILANIMATIONELEMENT_IID)

#endif // NS_ISMILANIMATIONELEMENT_H_
