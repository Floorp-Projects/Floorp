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
  NS_INLINE_DECL_REFCOUNTING_INHERITED(LeafAccessible, AccessibleWrap)

  // Accessible
  virtual Accessible* ChildAtPoint(int32_t aX, int32_t aY,
                                   EWhichChildAtPoint aWhichChild) override;
  bool InsertChildAt(uint32_t aIndex, Accessible* aChild) final;
  bool RemoveChild(Accessible* aChild) final;

  virtual bool IsAcceptableChild(nsIContent* aEl) const override;

protected:
  virtual ~LeafAccessible() {}
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

  LinkableAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    AccessibleWrap(aContent, aDoc)
  {
  }

  NS_INLINE_DECL_REFCOUNTING_INHERITED(LinkableAccessible, AccessibleWrap)

  // Accessible
  virtual void Value(nsString& aValue) const override;
  virtual uint64_t NativeLinkState() const override;
  virtual void TakeFocus() const override;

  // ActionAccessible
  virtual uint8_t ActionCount() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t index) const override;
  virtual KeyBinding AccessKey() const override;

  // ActionAccessible helpers
  const Accessible* ActionWalk(bool* aIsLink = nullptr,
                               bool* aIsOnclick = nullptr,
                               bool* aIsLabelWithControl = nullptr) const;
  // HyperLinkAccessible
  virtual already_AddRefed<nsIURI> AnchorURIAt(uint32_t aAnchorIndex) const override;

protected:
  virtual ~LinkableAccessible() {}

};

/**
 * A simple accessible that gets its enumerated role.
 */
template<a11y::role R>
class EnumRoleAccessible : public AccessibleWrap
{
public:
  EnumRoleAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    AccessibleWrap(aContent, aDoc) { }

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aPtr) override
    { return Accessible::QueryInterface(aIID, aPtr); }

  // Accessible
  virtual a11y::role NativeRole() const override { return R; }

protected:
  virtual ~EnumRoleAccessible() { }
};


/**
 * A wrapper accessible around native accessible to connect it with
 * crossplatform accessible tree.
 */
class DummyAccessible : public AccessibleWrap
{
public:
  explicit DummyAccessible(DocAccessible* aDocument = nullptr) :
    AccessibleWrap(nullptr, aDocument) { }

  uint64_t NativeState() const final;
  uint64_t NativeInteractiveState() const final;
  uint64_t NativeLinkState() const final;
  bool NativelyUnavailable() const final;
  void ApplyARIAState(uint64_t* aState) const final;

protected:
  virtual ~DummyAccessible() { }
};

} // namespace a11y
} // namespace mozilla

#endif
