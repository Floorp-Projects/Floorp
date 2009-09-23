// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GTK_UTIL_H_
#define CHROME_COMMON_GTK_UTIL_H_

#include <gtk/gtk.h>

#include "webkit/glue/window_open_disposition.h"

typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GtkWidget GtkWidget;
class SkBitmap;

namespace event_utils {

// Translates event flags into what kind of disposition they represent.
// For example, a middle click would mean to open a background tab.
// event_flags are the state in the GdkEvent structure.
WindowOpenDisposition DispositionFromEventFlags(guint state);

}  // namespace event_utils

namespace gfx {

// Convert and copy a SkBitmap to a GdkPixbuf. NOTE: this uses BGRAToRGBA, so
// it is an expensive operation.
GdkPixbuf* GdkPixbufFromSkBitmap(const SkBitmap* bitmap);

// Create a GtkBin with |child| as its child widget.  This bin will paint a
// border of color |color| with the sizes specified in pixels.
GtkWidget* CreateGtkBorderBin(GtkWidget* child, const GdkColor* color,
                              int top, int bottom, int left, int right);

// Remove all children from this container.
void RemoveAllChildren(GtkWidget* container);

}  // namespace gfx

#endif  // CHROME_COMMON_GTK_UTIL_H_
