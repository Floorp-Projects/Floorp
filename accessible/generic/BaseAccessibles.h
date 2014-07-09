/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_BaseAccessibles_h__
#define mozilla_a11y_BaseAccessibles_h__

#include "AccessibleWrap.h"
#include "HyperTextAccessibleWrap.h"

class nsIContent;

/**
  * This file contains a number of classes that are used as base
  *  classes for the different accessibility implementations of
  *  the HTML and XUL widget sets.  --jgaunt
  */

namespace mozilla {
namespace a11y {

/**
  * Leaf version of DOM Accessible -- has no children
  */
class LeafAccessible : public AccessibleWrap
{
public:

  LeafAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual Accessible* ChildAtPoint(int32_t aX, int32_t aY,
                                   EWhichChildAtPoint aWhichChild);
  virtual bool InsertChildAt(uint32_t aIndex, Accessible* aChild) MOZ_OVERRIDE MOZ_FINAL;
  virtual bool RemoveChild(Accessible* aChild) MOZ_OVERRIDE MOZ_FINAL;

protected:
  virtual ~LeafAccessible() {}

  // Accessible
  virtual void CacheChildren();
};

/**
 * Used for text or image accessible nodes contained by link accessibles or
 * accessibles for nodes with registered click event handler. It knows how to
 * report the state of the host link (traveled or not) and can activate (click)
 * the host accessible programmatically.
 */
class LinkableAccessible : public AccessibleWrap
{
public:
  enum { eAction_Jump = 0 };

  LinkableAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString& aName);
  NS_IMETHOD DoAction(uint8_t index);
  NS_IMETHOD TakeFocus();

  // Accessible
  virtual void Shutdown();
  virtual void Value(nsString& aValue);
  virtual uint64_t NativeLinkState() const;

  // ActionAccessible
  virtual uint8_t ActionCount();
  virtual KeyBinding AccessKey() const;

  // HyperLinkAccessible
  virtual already_AddRefed<nsIURI> AnchorURIAt(uint32_t aAnchorIndex);

protected:
  virtual ~LinkableAccessible() {}

  // Accessible
  virtual void BindToParent(Accessible* aParent, uint32_t aIndexInParent);
  virtual void UnbindFromParent();

  /**
   * Parent accessible that provides an action for this linkable accessible.
   */
  Accessible* mActionAcc;
  bool mIsLink;
  bool mIsOnclick;
};

/**
 * A simple accessible that gets its enumerated role passed into constructor.
 */
class EnumRoleAccessible : public AccessibleWrap
{
public:
  EnumRoleAccessible(nsIContent* aContent, DocAccessible* aDoc, 
                     a11y::role aRole);

  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual a11y::role NativeRole();

protected:
  virtual ~EnumRoleAccessible() { }

  a11y::role mRole;
};


/**
 * A wrapper accessible around native accessible to connect it with
 * crossplatform accessible tree.
 */
class DummyAccessible : public AccessibleWrap
{
public:
  DummyAccessible() : AccessibleWrap(nullptr, nullptr) { }
  virtual ~DummyAccessible() { }

  virtual uint64_t NativeState() MOZ_OVERRIDE MOZ_FINAL;
  virtual uint64_t NativeInteractiveState() const MOZ_OVERRIDE MOZ_FINAL;
  virtual uint64_t NativeLinkState() const MOZ_OVERRIDE MOZ_FINAL;
  virtual bool NativelyUnavailable() const MOZ_OVERRIDE MOZ_FINAL;
  virtual void ApplyARIAState(uint64_t* aState) const MOZ_OVERRIDE MOZ_FINAL;
};

} // namespace a11y
} // namespace mozilla

#endif
