// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_X11_UTIL_INTERNAL_H_
#define CHROME_COMMON_X11_UTIL_INTERNAL_H_

// This file declares utility functions for X11 (Linux only).
//
// These functions require the inclusion of the Xlib headers. Since the Xlib
// headers pollute so much of the namespace, this should only be included
// when needed.

extern "C" {
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xrender.h>
}

namespace x11_util {
  // These functions cache their results and must be called from the UI thread.
  // Currently they don't support multiple screens/displays.

  // Get the XRENDER format id for ARGB32 (Skia's format).
  XRenderPictFormat* GetRenderARGB32Format(Display* dpy);
  // Get the XRENDER format id for the default visual on the first screen. This
  // is the format which our GTK window will have.
  XRenderPictFormat* GetRenderVisualFormat(Display* dpy, Visual* visual);
};

#endif  // CHROME_COMMON_X11_UTIL_H_
