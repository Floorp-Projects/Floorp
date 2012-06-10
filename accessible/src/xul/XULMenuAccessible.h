/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULMenuAccessible_h__
#define mozilla_a11y_XULMenuAccessible_h__

#include "AccessibleWrap.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "XULSelectControlAccessible.h"

namespace mozilla {
namespace a11y {

/**
 * Used for XUL menu, menuitem elements.
 */
class XULMenuitemAccessible : public AccessibleWrap
{
public:
  enum { eAction_Click = 0 };

  XULMenuitemAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);

  // Accessible
  virtual void Description(nsString& aDescription);
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual PRUint64 NativeInteractiveState() const;
  virtual PRInt32 GetLevelInternal();

  virtual bool CanHaveAnonChildren();

  // ActionAccessible
  virtual PRUint8 ActionCount();
  virtual KeyBinding AccessKey() const;
  virtual KeyBinding KeyboardShortcut() const;

  // Widgets
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;
  virtual Accessible* ContainerWidget() const;
};

/**
 * Used for XUL menuseparator element.
 */
class XULMenuSeparatorAccessible : public XULMenuitemAccessible
{
public:
  XULMenuSeparatorAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);

  // Accessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();
};


/**
 * Used for XUL menupopup and panel.
 */
class XULMenupopupAccessible : public XULSelectControlAccessible
{
public:
  XULMenupopupAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // Widgets
  virtual bool IsWidget() const;
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;

  virtual Accessible* ContainerWidget() const;
};

/**
 * Used for XUL menubar element.
 */
class XULMenubarAccessible : public AccessibleWrap
{
public:
  XULMenubarAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual a11y::role NativeRole();

  // Widget
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;
  virtual Accessible* CurrentItem();
  virtual void SetCurrentItem(Accessible* aItem);
};

} // namespace a11y
} // namespace mozilla

#endif
