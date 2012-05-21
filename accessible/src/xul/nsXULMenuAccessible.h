/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsXULMenuAccessible_H_
#define _nsXULMenuAccessible_H_

#include "nsAccessibleWrap.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "XULSelectControlAccessible.h"

/**
 * Used for XUL menu, menuitem elements.
 */
class nsXULMenuitemAccessible : public nsAccessibleWrap
{
public:
  enum { eAction_Click = 0 };

  nsXULMenuitemAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);

  // nsAccessible
  virtual void Description(nsString& aDescription);
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual PRInt32 GetLevelInternal();

  virtual bool CanHaveAnonChildren();

  // ActionAccessible
  virtual PRUint8 ActionCount();
  virtual KeyBinding AccessKey() const;
  virtual KeyBinding KeyboardShortcut() const;

  // Widgets
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;
  virtual nsAccessible* ContainerWidget() const;
};

/**
 * Used for XUL menuseparator element.
 */
class nsXULMenuSeparatorAccessible : public nsXULMenuitemAccessible
{
public:
  nsXULMenuSeparatorAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);

  // nsAccessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();
};


/**
 * Used for XUL menupopup and panel.
 */
class nsXULMenupopupAccessible : public XULSelectControlAccessible
{
public:
  nsXULMenupopupAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsAccessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // Widgets
  virtual bool IsWidget() const;
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;

  virtual nsAccessible* ContainerWidget() const;
};

/**
 * Used for XUL menubar element.
 */
class nsXULMenubarAccessible : public nsAccessibleWrap
{
public:
  nsXULMenubarAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsAccessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // Widget
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;
  virtual nsAccessible* CurrentItem();
  virtual void SetCurrentItem(nsAccessible* aItem);
};

#endif
