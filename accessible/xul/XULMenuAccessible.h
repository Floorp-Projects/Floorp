/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULMenuAccessible_h__
#define mozilla_a11y_XULMenuAccessible_h__

#include "AccessibleWrap.h"
#include "XULSelectControlAccessible.h"

namespace mozilla {
namespace a11y {

/**
 * Used for XUL menu, menuitem elements.
 */
class XULMenuitemAccessible : public AccessibleWrap {
 public:
  enum { eAction_Click = 0 };

  XULMenuitemAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual void Description(nsString& aDescription) const override;
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual uint64_t NativeInteractiveState() const override;

  // ActionAccessible
  virtual bool HasPrimaryAction() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual KeyBinding AccessKey() const override;
  virtual KeyBinding KeyboardShortcut() const override;

  // Widgets
  virtual bool IsActiveWidget() const override;
  virtual bool AreItemsOperable() const override;
  virtual LocalAccessible* ContainerWidget() const override;

 protected:
  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;
  virtual int32_t GetLevel(bool aFast) const override;
};

/**
 * Used for XUL menuseparator element.
 */
class XULMenuSeparatorAccessible : public XULMenuitemAccessible {
 public:
  XULMenuSeparatorAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;

  // ActionAccessible
  virtual bool HasPrimaryAction() const override;

 protected:
  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;
};

/**
 * Used for XUL menupopup and panel.
 */
class XULMenupopupAccessible : public XULSelectControlAccessible {
 public:
  XULMenupopupAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;

  // Widgets
  virtual bool IsWidget() const override;
  virtual bool IsActiveWidget() const override;
  virtual bool AreItemsOperable() const override;

  virtual LocalAccessible* ContainerWidget() const override;

 protected:
  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;
};

/**
 * Used for XUL menubar element.
 */
class XULMenubarAccessible : public AccessibleWrap {
 public:
  XULMenubarAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual a11y::role NativeRole() const override;

  // Widget
  virtual bool IsActiveWidget() const override;
  virtual bool AreItemsOperable() const override;
  virtual LocalAccessible* CurrentItem() const override;
  virtual void SetCurrentItem(const LocalAccessible* aItem) override;

 protected:
  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;
};

}  // namespace a11y
}  // namespace mozilla

#endif
