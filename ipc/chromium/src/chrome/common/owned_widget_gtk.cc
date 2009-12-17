// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/owned_widget_gtk.h"

#include <gtk/gtk.h>

#include "base/logging.h"

OwnedWidgetGtk::~OwnedWidgetGtk() {
  DCHECK(!widget_) << "You must explicitly call OwnerWidgetGtk::Destroy().";
}

void OwnedWidgetGtk::Own(GtkWidget* widget) {
  DCHECK(!widget_);
  // We want to make sure that Own() was called properly, right after the
  // widget was created.  We should have a floating refcount of 1.
  DCHECK(g_object_is_floating(widget));
  // NOTE: Assumes some implementation details about glib internals.
  DCHECK(G_OBJECT(widget)->ref_count == 1);

  // Sink the floating reference, we should now own this reference.
  g_object_ref_sink(widget);
  widget_ = widget;
}

void OwnedWidgetGtk::Destroy() {
  if (!widget_)
    return;

  GtkWidget* widget = widget_;
  widget_ = NULL;
  gtk_widget_destroy(widget);

  DCHECK(!g_object_is_floating(widget));
  // NOTE: Assumes some implementation details about glib internals.
  DCHECK(G_OBJECT(widget)->ref_count == 1);
  g_object_unref(widget);
}
