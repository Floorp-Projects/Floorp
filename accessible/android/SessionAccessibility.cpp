/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionAccessibility.h"
#include "AndroidUiThread.h"
#include "nsThreadUtils.h"

#ifdef DEBUG
#include <android/log.h>
#define AALOG(args...)                                                         \
  __android_log_print(ANDROID_LOG_INFO, "GeckoAccessibilityNative", ##args)
#else
#define AALOG(args...)                                                         \
  do {                                                                         \
  } while (0)
#endif

template<>
const char nsWindow::NativePtr<mozilla::a11y::SessionAccessibility>::sName[] =
  "SessionAccessibility";

using namespace mozilla::a11y;

void
SessionAccessibility::SetAttached(bool aAttached)
{
  if (RefPtr<nsThread> uiThread = GetAndroidUiThread()) {
    uiThread->Dispatch(NS_NewRunnableFunction(
      "SessionAccessibility::Attach",
      [aAttached,
       sa = java::SessionAccessibility::NativeProvider::GlobalRef(
         mSessionAccessibility)] { sa->SetAttached(aAttached); }));
  }
}
