/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULAlertAccessible_h__
#define mozilla_a11y_XULAlertAccessible_h__

#include "AccessibleWrap.h"

namespace mozilla {
namespace a11y {

/**
 * Accessible for supporting XUL alerts.
 */

class XULAlertAccessible : public AccessibleWrap
{
public:
  XULAlertAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(XULAlertAccessible, AccessibleWrap)

  // Accessible
  virtual mozilla::a11y::ENameValueFlag Name(nsString& aName) override;
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() override;

  // Widgets
  virtual bool IsWidget() const override;
  virtual Accessible* ContainerWidget() const override;

protected:
  ~XULAlertAccessible();
};

} // namespace a11y
} // namespace mozilla

#endif
