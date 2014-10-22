/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_ApplicationAccessibleWrap_h__
#define mozilla_a11y_ApplicationAccessibleWrap_h__

#include "ApplicationAccessible.h"

namespace mozilla {
namespace a11y {

class ApplicationAccessibleWrap: public ApplicationAccessible
{
public:
  ApplicationAccessibleWrap();
  virtual ~ApplicationAccessibleWrap();

  // Accessible
  virtual mozilla::a11y::ENameValueFlag Name(nsString& aName);
  virtual bool InsertChildAt(uint32_t aIdx, Accessible* aChild) MOZ_OVERRIDE;
  virtual bool RemoveChild(Accessible* aChild);

  /**
   * Return the atk object for app root accessible.
   */
  virtual void GetNativeInterface(void** aOutAccessible) MOZ_OVERRIDE;
};

} // namespace a11y
} // namespace mozilla

#endif   /* __NS_APP_ROOT_ACCESSIBLE_H__ */
