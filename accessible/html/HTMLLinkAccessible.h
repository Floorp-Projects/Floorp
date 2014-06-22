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

  // nsIAccessible
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString& aName);
  NS_IMETHOD DoAction(uint8_t aIndex);

  // Accessible
  virtual void Value(nsString& aValue);
  virtual a11y::role NativeRole();
  virtual uint64_t NativeState();
  virtual uint64_t NativeLinkState() const;
  virtual uint64_t NativeInteractiveState() const;

  // ActionAccessible
  virtual uint8_t ActionCount();

  // HyperLinkAccessible
  virtual bool IsLink();
  virtual already_AddRefed<nsIURI> AnchorURIAt(uint32_t aAnchorIndex);

protected:
  enum { eAction_Jump = 0 };

  /**
   * Returns true if the link has href attribute.
   */
  bool IsLinked();
};

} // namespace a11y
} // namespace mozilla

#endif
