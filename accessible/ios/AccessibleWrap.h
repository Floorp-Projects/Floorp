/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* For documentation of the accessibility architecture,
 * see https://firefox-source-docs.mozilla.org/accessible/index.html
 */

#ifndef mozilla_a11y_AccessibleWrap_h_
#define mozilla_a11y_AccessibleWrap_h_

#include <objc/objc.h>

#include "nsCOMPtr.h"
#include "LocalAccessible.h"

namespace mozilla {
namespace a11y {

class AccessibleWrap : public LocalAccessible {
 public:  // construction, destruction
  AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~AccessibleWrap() = default;

  virtual void Shutdown() override;

  /**
   * Get the native Obj-C object (MUIAccessible).
   */
  virtual void GetNativeInterface(void** aOutAccessible) override;

 protected:
  id GetNativeObject();

 private:
  id mNativeObject;

  bool mNativeInited;
};

}  // namespace a11y
}  // namespace mozilla

#endif
