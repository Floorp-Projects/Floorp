/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULSliderAccessible_h__
#define mozilla_a11y_XULSliderAccessible_h__

#include "AccessibleWrap.h"

#include "nsIDOMElement.h"

namespace mozilla {
namespace a11y {

/**
 * Used for XUL slider and scale elements.
 */
class XULSliderAccessible : public AccessibleWrap
{
public:
  XULSliderAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsIAccessibleValue
  NS_DECL_NSIACCESSIBLEVALUE

  // Accessible
  virtual void Value(nsString& aValue);
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeInteractiveState() const;
  virtual bool NativelyUnavailable() const;
  virtual bool CanHaveAnonChildren();

  // ActionAccessible
  virtual PRUint8 ActionCount();

protected:
  /**
   * Return anonymous slider element.
   */
  nsIContent* GetSliderElement() const;

  nsresult GetSliderAttr(nsIAtom *aName, nsAString& aValue);
  nsresult SetSliderAttr(nsIAtom *aName, const nsAString& aValue);

  nsresult GetSliderAttr(nsIAtom *aName, double *aValue);
  nsresult SetSliderAttr(nsIAtom *aName, double aValue);

private:
  mutable nsCOMPtr<nsIContent> mSliderNode;
};


/**
 * Used for slider's thumb element.
 */
class XULThumbAccessible : public AccessibleWrap
{
public:
  XULThumbAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual a11y::role NativeRole();
};

} // namespace a11y
} // namespace mozilla

#endif

