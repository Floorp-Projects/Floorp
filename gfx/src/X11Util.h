/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_X11Util_h
#define mozilla_X11Util_h

// Utilities common to all X clients, regardless of UI toolkit.

#if defined(MOZ_WIDGET_GTK)
#  include <gdk/gdk.h>
#  include <gdk/gdkx.h>
#  include "mozilla/WidgetUtilsGtk.h"
#  include "X11UndefineNone.h"
#else
#  error Unknown toolkit
#endif

#include <string.h>          // for memset
#include "mozilla/Scoped.h"  // for SCOPED_TEMPLATE

namespace mozilla {

/**
 * Return the default X Display created and used by the UI toolkit.
 */
inline Display* DefaultXDisplay() {
#if defined(MOZ_WIDGET_GTK)
  GdkDisplay* gdkDisplay = gdk_display_get_default();
  if (mozilla::widget::GdkIsX11Display(gdkDisplay)) {
    return GDK_DISPLAY_XDISPLAY(gdkDisplay);
  }
#endif
  return nullptr;
}

/**
 * Sets *aVisual to point to aDisplay's Visual struct corresponding to
 * aVisualID, and *aDepth to its depth.  When aVisualID is None, these are set
 * to nullptr and 0 respectively.  Both out-parameter pointers are assumed
 * non-nullptr.
 */
void FindVisualAndDepth(Display* aDisplay, VisualID aVisualID, Visual** aVisual,
                        int* aDepth);

/**
 * Ensure that all X requests have been processed.
 *
 * This is similar to XSync, but doesn't need a round trip if the previous
 * request was synchronous or if events have been received since the last
 * request.  Subsequent FinishX calls will be noops if there have been no
 * intermediate requests.
 */

void FinishX(Display* aDisplay);

/**
 * Invoke XFree() on a pointer to memory allocated by Xlib (if the
 * pointer is nonnull) when this class goes out of scope.
 */
template <typename T>
struct ScopedXFreePtrTraits {
  typedef T* type;
  static T* empty() { return nullptr; }
  static void release(T* ptr) {
    if (ptr != nullptr) XFree(ptr);
  }
};
SCOPED_TEMPLATE(ScopedXFree, ScopedXFreePtrTraits)

}  // namespace mozilla

#endif  // mozilla_X11Util_h
