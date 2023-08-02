/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULElementAccessibles_h__
#define mozilla_a11y_XULElementAccessibles_h__

#include "HyperTextAccessible.h"
#include "TextLeafAccessible.h"

namespace mozilla {
namespace a11y {

class XULLabelTextLeafAccessible;

/**
 * Used for XUL description and label elements.
 */
class XULLabelAccessible : public HyperTextAccessible {
 public:
  XULLabelAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual void Shutdown() override;
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual Relation RelationByType(RelationType aType) const override;

  void UpdateLabelValue(const nsString& aValue);

 protected:
  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;
  virtual void DispatchClickEvent(nsIContent* aContent,
                                  uint32_t aActionIndex) const override;

 private:
  RefPtr<XULLabelTextLeafAccessible> mValueTextLeaf;
};

inline XULLabelAccessible* LocalAccessible::AsXULLabel() {
  return IsXULLabel() ? static_cast<XULLabelAccessible*>(this) : nullptr;
}

/**
 * Used to implement text interface on XUL label accessible in case when text
 * is provided by @value attribute (no underlying text frame).
 */
class XULLabelTextLeafAccessible final : public TextLeafAccessible {
 public:
  XULLabelTextLeafAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : TextLeafAccessible(aContent, aDoc) {
    mStateFlags |= eSharedNode;
  }

  virtual ~XULLabelTextLeafAccessible() {}

  // LocalAccessible
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
};

/**
 * Used for XUL tooltip element.
 */
class XULTooltipAccessible : public LeafAccessible {
 public:
  XULTooltipAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
};

class XULLinkAccessible : public XULLabelAccessible {
 public:
  XULLinkAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual void Value(nsString& aValue) const override;
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeLinkState() const override;

  // ActionAccessible
  virtual bool HasPrimaryAction() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;

  // HyperLinkAccessible
  virtual bool IsLink() const override;
  virtual uint32_t StartOffset() override;
  virtual uint32_t EndOffset() override;
  virtual already_AddRefed<nsIURI> AnchorURIAt(
      uint32_t aAnchorIndex) const override;

 protected:
  virtual ~XULLinkAccessible();

  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;

  enum { eAction_Jump = 0 };
};

}  // namespace a11y
}  // namespace mozilla

#endif
