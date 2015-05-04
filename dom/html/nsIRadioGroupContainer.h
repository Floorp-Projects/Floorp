/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIRadioGroupContainer_h___
#define nsIRadioGroupContainer_h___

#include "nsISupports.h"

class nsIRadioVisitor;
class nsIFormControl;

namespace mozilla {
namespace dom {
class HTMLInputElement;
}
}

#define NS_IRADIOGROUPCONTAINER_IID   \
{ 0x800320a0, 0x733f, 0x11e4, \
  { 0x82, 0xf8, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66 } }

/**
 * A container that has multiple radio groups in it, defined by name.
 */
class nsIRadioGroupContainer : public nsISupports
{
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IRADIOGROUPCONTAINER_IID)

  /**
   * Walk through the radio group, visiting each note with avisitor->Visit()
   * @param aName the group name
   * @param aVisitor the visitor to visit with
   * @param aFlushContent whether to ensure the content model is up to date
   *        before walking.
   */
  NS_IMETHOD WalkRadioGroup(const nsAString& aName,
                            nsIRadioVisitor* aVisitor,
                            bool aFlushContent) = 0;

  /**
   * Set the current radio button in a group
   * @param aName the group name
   * @param aRadio the currently selected radio button
   */
  virtual void SetCurrentRadioButton(const nsAString& aName,
                                     mozilla::dom::HTMLInputElement* aRadio) = 0;

  /**
   * Get the current radio button in a group
   * @param aName the group name
   * @return the currently selected radio button
   */
  virtual mozilla::dom::HTMLInputElement* GetCurrentRadioButton(const nsAString& aName) = 0;

  /**
   * Get the next/prev radio button in a group
   * @param aName the group name
   * @param aPrevious, true gets previous radio button, false gets next
   * @param aFocusedRadio the currently focused radio button
   * @param aRadio the currently selected radio button [OUT]
   */
  NS_IMETHOD GetNextRadioButton(const nsAString& aName,
                                const bool aPrevious,
                                mozilla::dom::HTMLInputElement*  aFocusedRadio,
                                mozilla::dom::HTMLInputElement** aRadio) = 0;

  /**
   * Add radio button to radio group
   *
   * Note that forms do not do anything for this method since they already
   * store radio groups on their own.
   *
   * @param aName radio group's name
   * @param aRadio radio button's pointer
   */
  virtual void AddToRadioGroup(const nsAString& aName, nsIFormControl* aRadio) = 0;

  /**
   * Remove radio button from radio group
   *
   * Note that forms do not do anything for this method since they already
   * store radio groups on their own.
   *
   * @param aName radio group's name
   * @param aRadio radio button's pointer
   */
  virtual void RemoveFromRadioGroup(const nsAString& aName, nsIFormControl* aRadio) = 0;

  virtual uint32_t GetRequiredRadioCount(const nsAString& aName) const = 0;
  virtual void RadioRequiredWillChange(const nsAString& aName,
                                       bool aRequiredAdded) = 0;
  virtual bool GetValueMissingState(const nsAString& aName) const = 0;
  virtual void SetValueMissingState(const nsAString& aName, bool aValue) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIRadioGroupContainer,
                              NS_IRADIOGROUPCONTAINER_IID)

#endif /* nsIRadioGroupContainer_h__ */
