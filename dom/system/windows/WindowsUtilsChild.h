/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WindowsUtilsChild_h__
#define mozilla_dom_WindowsUtilsChild_h__

#include "mozilla/dom/PWindowsUtilsChild.h"
#include "mozilla/dom/WindowsLocationChild.h"

namespace mozilla::dom {

// Manager for utilities in the WindowsUtils utility process.
class WindowsUtilsChild final : public PWindowsUtilsChild {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WindowsUtilsChild, override);

 public:
  already_AddRefed<PWindowsLocationChild> AllocPWindowsLocationChild() {
    return MakeAndAddRef<WindowsLocationChild>();
  }

 protected:
  ~WindowsUtilsChild() = default;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_WindowsUtilsChild_h__
