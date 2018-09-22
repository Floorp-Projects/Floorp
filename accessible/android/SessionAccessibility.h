/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_SessionAccessibility_h_
#define mozilla_a11y_SessionAccessibility_h_

#include "GeneratedJNINatives.h"
#include "nsWindow.h"

namespace mozilla {
namespace a11y {

class SessionAccessibility final
  : public java::SessionAccessibility::NativeProvider::Natives<SessionAccessibility>
{
public:
  typedef java::SessionAccessibility::NativeProvider::Natives<SessionAccessibility> Base;

  SessionAccessibility(
    nsWindow::NativePtr<SessionAccessibility>* aPtr,
    nsWindow* aWindow,
    java::SessionAccessibility::NativeProvider::Param aSessionAccessibility)
    : mWindow(aPtr, aWindow)
    , mSessionAccessibility(aSessionAccessibility)
  {
    SetAttached(true);
  }

  void OnDetach() { SetAttached(false); }

  // Native implementations
  using Base::AttachNative;
  using Base::DisposeNative;

  NS_INLINE_DECL_REFCOUNTING(SessionAccessibility)

private:
  ~SessionAccessibility() {}

  void SetAttached(bool aAttached);

  nsWindow::WindowPtr<SessionAccessibility> mWindow; // Parent only
  java::SessionAccessibility::NativeProvider::GlobalRef mSessionAccessibility;
};

} // namespace a11y
} // namespace mozilla

#endif
