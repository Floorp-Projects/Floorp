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

//////////////////////////////////////////////////////////////////////////////
// nsISMILAnimationElement: Interface for elements that control the animation of
// some property of another element, e.g. <animate>, <set>.

#define NS_ISMILANIMATIONELEMENT_IID \
{ 0xaf92584b, 0x75b0, 0x4584,        \
  { 0x87, 0xd2, 0xa8, 0x3, 0x34, 0xf0, 0x5, 0xaf } }

class nsISMILAttr;
class nsSMILAnimationFunction;
class nsSMILTimeContainer;
class nsSMILTimedElement;
class nsIAtom;
class nsAttrValue;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

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
   * Returns this element as a mozilla::dom::Element.
   */
  virtual const mozilla::dom::Element& AsElement() const = 0;

  /*
   * Non-const version of Element()
   */
  virtual mozilla::dom::Element& AsElement() = 0;

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
  virtual bool GetAnimAttr(nsIAtom* aAttName, nsAString& aResult) const = 0;

  /*
   * Check for the presence of an attribute in the global namespace.
   */
  virtual bool HasAnimAttr(nsIAtom* aAttName) const = 0;

  /*
   * Returns the target (animated) element.
   */
  virtual mozilla::dom::Element* GetTargetElementContent() = 0;

  /*
   * Returns the name of the target (animated) attribute or property.
   */
  virtual bool GetTargetAttributeName(PRInt32* aNamespaceID,
                                        nsIAtom** aLocalName) const = 0;

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
