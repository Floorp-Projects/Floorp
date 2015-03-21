/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLLinkAccessible_h__
#define mozilla_a11y_HTMLLinkAccessible_h__

#include "HyperTextAccessibleWrap.h"

namespace mozilla {
namespace a11y {

class HTMLLinkAccessible : public HyperTextAccessibleWrap
{
public:
  HTMLLinkAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual void Value(nsString& aValue) override;
  virtual a11y::role NativeRole() override;
  virtual uint64_t NativeState() override;
  virtual uint64_t NativeLinkState() const override;
  virtual uint64_t NativeInteractiveState() const override;

  // ActionAccessible
  virtual uint8_t ActionCount() override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) override;

  // HyperLinkAccessible
  virtual bool IsLink() override;
  virtual already_AddRefed<nsIURI> AnchorURIAt(uint32_t aAnchorIndex) override;

protected:
  virtual ~HTMLLinkAccessible() {}

  enum { eAction_Jump = 0 };

  /**
   * Returns true if the link has href attribute.
   */
  bool IsLinked() const;
};

} // namespace a11y
} // namespace mozilla

#endif
