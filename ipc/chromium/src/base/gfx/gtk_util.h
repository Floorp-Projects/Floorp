// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GFX_GTK_UTIL_H_
#define BASE_GFX_GTK_UTIL_H_

#include <stdint.h>
#include <vector>

#include <glib-object.h>

#include "base/scoped_ptr.h"

typedef struct _GdkColor GdkColor;
typedef struct _GdkRegion GdkRegion;

// Define a macro for creating GdkColors from RGB values.  This is a macro to
// allow static construction of literals, etc.  Use this like:
//   GdkColor white = GDK_COLOR_RGB(0xff, 0xff, 0xff);
#define GDK_COLOR_RGB(r, g, b) {0, r * 257, g * 257, b * 257}

namespace gfx {

class Rect;

extern const GdkColor kGdkWhite;
extern const GdkColor kGdkBlack;
extern const GdkColor kGdkGreen;

// Modify the given region by subtracting the given rectangles.
void SubtractRectanglesFromRegion(GdkRegion* region,
                                  const std::vector<Rect>& cutouts);

}  // namespace gfx

namespace {
// A helper class that will g_object_unref |p| when it goes out of scope.
// This never adds a ref, it only unrefs.
template <typename Type>
struct GObjectUnrefer {
  void operator()(Type* ptr) const {
    if (ptr)
      g_object_unref(ptr);
  }
};
}  // namespace

// It's not legal C++ to have a templatized typedefs, so we wrap it in a
// struct.  When using this, you need to include ::Type.  E.g.,
// ScopedGObject<GdkPixbufLoader>::Type loader(gdk_pixbuf_loader_new());
template<class T>
struct ScopedGObject {
  typedef scoped_ptr_malloc<T, GObjectUnrefer<T> > Type;
};

#endif  // BASE_GFX_GTK_UTIL_H_
