// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/gtk_native_view_id_manager.h"

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/rand_util.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

// -----------------------------------------------------------------------------
// Bounce functions for GTK to callback into a C++ object...

static void OnRealize(gfx::NativeView widget, void* arg) {
  GtkNativeViewManager* manager = reinterpret_cast<GtkNativeViewManager*>(arg);
  manager->OnRealize(widget);
}

static void OnUnrealize(gfx::NativeView widget, void *arg) {
  GtkNativeViewManager* manager = reinterpret_cast<GtkNativeViewManager*>(arg);
  manager->OnUnrealize(widget);
}

static void OnDestroy(GtkObject* obj, void* arg) {
  GtkNativeViewManager* manager = reinterpret_cast<GtkNativeViewManager*>(arg);
  manager->OnDestroy(reinterpret_cast<GtkWidget*>(obj));
}

// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Public functions...

GtkNativeViewManager::GtkNativeViewManager() {
}

gfx::NativeViewId GtkNativeViewManager::GetIdForWidget(gfx::NativeView widget) {
  // This is just for unit tests:
  if (!widget)
    return 0;

  AutoLock locked(lock_);

  std::map<gfx::NativeView, gfx::NativeViewId>::const_iterator i =
    native_view_to_id_.find(widget);

  if (i != native_view_to_id_.end())
    return i->second;

  gfx::NativeViewId new_id =
      static_cast<gfx::NativeViewId>(base::RandUint64());
  while (id_to_info_.find(new_id) != id_to_info_.end())
    new_id = static_cast<gfx::NativeViewId>(base::RandUint64());

  NativeViewInfo info;
  if (GTK_WIDGET_REALIZED(widget)) {
    GdkWindow *gdk_window = widget->window;
    CHECK(gdk_window);
    info.x_window_id = GDK_WINDOW_XID(gdk_window);
  }

  native_view_to_id_[widget] = new_id;
  id_to_info_[new_id] = info;

  g_signal_connect(widget, "realize", G_CALLBACK(::OnRealize), this);
  g_signal_connect(widget, "unrealize", G_CALLBACK(::OnUnrealize), this);
  g_signal_connect(widget, "destroy", G_CALLBACK(::OnDestroy), this);

  return new_id;
}

bool GtkNativeViewManager::GetXIDForId(XID* output, gfx::NativeViewId id) {
  AutoLock locked(lock_);

  std::map<gfx::NativeViewId, NativeViewInfo>::const_iterator i =
    id_to_info_.find(id);

  if (i == id_to_info_.end())
    return false;

  *output = i->second.x_window_id;
  return true;
}

// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Private functions...

gfx::NativeViewId GtkNativeViewManager::GetWidgetId(gfx::NativeView widget) {
  lock_.AssertAcquired();

  std::map<gfx::NativeView, gfx::NativeViewId>::const_iterator i =
    native_view_to_id_.find(widget);

  CHECK(i != native_view_to_id_.end());
  return i->second;
}

void GtkNativeViewManager::OnRealize(gfx::NativeView widget) {
  AutoLock locked(lock_);

  const gfx::NativeViewId id = GetWidgetId(widget);
  std::map<gfx::NativeViewId, NativeViewInfo>::iterator i =
    id_to_info_.find(id);

  CHECK(i != id_to_info_.end());
  CHECK(widget->window);

  i->second.x_window_id = GDK_WINDOW_XID(widget->window);
}

void GtkNativeViewManager::OnUnrealize(gfx::NativeView widget) {
  AutoLock locked(lock_);

  const gfx::NativeViewId id = GetWidgetId(widget);
  std::map<gfx::NativeViewId, NativeViewInfo>::iterator i =
    id_to_info_.find(id);

  CHECK(i != id_to_info_.end());

  i->second.x_window_id = 0;
}

void GtkNativeViewManager::OnDestroy(gfx::NativeView widget) {
  AutoLock locked(lock_);

  std::map<gfx::NativeView, gfx::NativeViewId>::iterator i =
    native_view_to_id_.find(widget);
  CHECK(i != native_view_to_id_.end());

  std::map<gfx::NativeViewId, NativeViewInfo>::iterator j =
    id_to_info_.find(i->second);
  CHECK(j != id_to_info_.end());

  native_view_to_id_.erase(i);
  id_to_info_.erase(j);
}

// -----------------------------------------------------------------------------
