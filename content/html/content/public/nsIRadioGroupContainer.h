/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIRadioGroupContainer_h___
#define nsIRadioGroupContainer_h___

#include "nsISupports.h"

class nsIDOMHTMLInputElement;
class nsString;
class nsIRadioVisitor;
class nsIFormControl;

#define NS_IRADIOGROUPCONTAINER_IID   \
{ 0x22924a01, 0x4360, 0x401b, \
  { 0xb1, 0xd1, 0x56, 0x8d, 0xf5, 0xa3, 0xda, 0x71 } }

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
                                     nsIDOMHTMLInputElement* aRadio) = 0;

  /**
   * Get the current radio button in a group
   * @param aName the group name
   * @return the currently selected radio button
   */
  virtual nsIDOMHTMLInputElement* GetCurrentRadioButton(const nsAString& aName) = 0;

  /**
   * Get the next/prev radio button in a group
   * @param aName the group name
   * @param aPrevious, true gets previous radio button, false gets next
   * @param aFocusedRadio the currently focused radio button
   * @param aRadio the currently selected radio button [OUT]
   */
  NS_IMETHOD GetNextRadioButton(const nsAString& aName,
                                const bool aPrevious,
                                nsIDOMHTMLInputElement*  aFocusedRadio,
                                nsIDOMHTMLInputElement** aRadio) = 0;

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

  virtual PRUint32 GetRequiredRadioCount(const nsAString& aName) const = 0;
  virtual void RadioRequiredChanged(const nsAString& aName,
                                    nsIFormControl* aRadio) = 0;
  virtual bool GetValueMissingState(const nsAString& aName) const = 0;
  virtual void SetValueMissingState(const nsAString& aName, bool aValue) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIRadioGroupContainer,
                              NS_IRADIOGROUPCONTAINER_IID)

#endif /* nsIRadioGroupContainer_h__ */
