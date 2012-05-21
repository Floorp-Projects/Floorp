/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_ISMILANIMATIONELEMENT_H_
#define NS_ISMILANIMATIONELEMENT_H_

#include "nsISupports.h"

//////////////////////////////////////////////////////////////////////////////
// nsISMILAnimationElement: Interface for elements that control the animation of
// some property of another element, e.g. <animate>, <set>.

#define NS_ISMILANIMATIONELEMENT_IID \
{ 0x29792cd9, 0x0f96, 0x4ba6,        \
  { 0xad, 0xea, 0x03, 0x0e, 0x0b, 0xfe, 0x1e, 0xb7 } }

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
   * Returns true if the element passes conditional processing
   */
  virtual bool PassesConditionalProcessingTests() = 0;

  /*
   * Returns the source attribute as an nsAttrValue. The global namespace will
   * be used.
   *
   * (The 'Anim' here and below is largely to avoid conflicts for subclasses
   * that derive from nsGenericElement)
   *
   * @param aName the name of the attr
   * @returns true if the attribute was set (even when set to empty string)
   *          false when not set.
   */
  virtual const nsAttrValue* GetAnimAttr(nsIAtom* aName) const = 0;

  /*
   * Get the current value of an attribute as a string. The global namespace
   * will be used.
   *
   * @param aName the name of the attr
   * @param aResult the value (may legitimately be the empty string) [OUT]
   * @returns true if the attribute was set (even when set to empty string)
   *          false when not set.
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
