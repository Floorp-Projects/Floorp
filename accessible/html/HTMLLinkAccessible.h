/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLLinkAccessible_h__
#define mozilla_a11y_HTMLLinkAccessible_h__

#include "HyperTextAccessible.h"

namespace mozilla {
namespace a11y {

class HTMLLinkAccessible : public HyperTextAccessible {
 public:
  HTMLLinkAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLLinkAccessible, HyperTextAccessible)

  // LocalAccessible
  virtual void Value(nsString& aValue) const override;
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual uint64_t NativeLinkState() const override;
  virtual uint64_t NativeInteractiveState() const override;

  // ActionAccessible
  virtual bool HasPrimaryAction() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;

  // HyperLinkAccessible
  virtual bool IsLink() const override;

  /**
   * Returns true if the link has href attribute.
   */
  bool IsLinked() const;

 protected:
  virtual ~HTMLLinkAccessible() {}

  virtual bool AttributeChangesState(nsAtom* aAttribute) override;

  virtual void DOMAttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                   int32_t aModType,
                                   const nsAttrValue* aOldValue,
                                   uint64_t aOldState) override;

  enum { eAction_Jump = 0 };
};

inline HTMLLinkAccessible* LocalAccessible::AsHTMLLink() {
  return IsHTMLLink() ? static_cast<HTMLLinkAccessible*>(this) : nullptr;
}

}  // namespace a11y
}  // namespace mozilla

#endif
