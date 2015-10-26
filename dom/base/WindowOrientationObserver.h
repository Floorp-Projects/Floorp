/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WindowOrientationObserver_h
#define mozilla_dom_WindowOrientationObserver_h

#include "mozilla/HalScreenConfiguration.h"

class nsGlobalWindow;

namespace mozilla {
namespace dom {

class WindowOrientationObserver final :
  public mozilla::hal::ScreenConfigurationObserver
{
public:
  explicit WindowOrientationObserver(nsGlobalWindow* aGlobalWindow);
  ~WindowOrientationObserver();
  void Notify(const mozilla::hal::ScreenConfiguration& aConfiguration) override;
  static int16_t OrientationAngle();

private:
  // Weak pointer, instance is owned by mWindow.
  nsGlobalWindow* MOZ_NON_OWNING_REF mWindow;
  uint16_t mAngle;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WindowOrientationObserver_h
