// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GFX_GTK_NATIVE_VIEW_ID_MANAGER_H_
#define BASE_GFX_GTK_NATIVE_VIEW_ID_MANAGER_H_

#include <map>

#include "base/singleton.h"
#include "base/gfx/native_widget_types.h"

typedef unsigned long XID;

// NativeViewIds are the opaque values which the renderer holds as a reference
// to a window. These ids are often used in sync calls from the renderer and
// one cannot terminate sync calls on the UI thread as that can lead to
// deadlocks.
//
// Because of this, we have the BACKGROUND_X11 thread for these calls and this
// thread has a separate X connection in order to answer them. But one cannot
// use GTK on multiple threads, so the BACKGROUND_X11 thread deals only in Xlib
// calls and, thus, XIDs.
//
// So we could make NativeViewIds be the X id of the window. However, at the
// time when we need to tell the renderer about its NativeViewId, an XID isn't
// availible and it goes very much against the grain of the code to make it so.
// Also, we worry that GTK might choose to change the underlying X window id
// when, say, the widget is hidden or repacked. Finally, if we used XIDs then a
// compromised renderer could start asking questions about any X windows on the
// system.
//
// Thus, we have this object. It produces random NativeViewIds from GtkWidget
// pointers and observes the various signals from the widget for when an X
// window is created, destroyed etc. Thus it provides a thread safe mapping
// from NativeViewIds to the current XID for that widget.
//
// You get a reference to the global instance with:
//   Singleton<GtkNativeViewManager>()
class GtkNativeViewManager {
 public:
  // Must be called from the UI thread:
  //
  // Return a NativeViewId for the given widget and attach to the various
  // signals emitted by that widget. The NativeViewId is pseudo-randomly
  // allocated so that a compromised renderer trying to guess values will fail
  // with high probability. The NativeViewId will not be reused for the
  // lifetime of the GtkWidget.
  gfx::NativeViewId GetIdForWidget(gfx::NativeView widget);

  // May be called from any thread:
  //
  // xid: (output) the resulting X window ID, or 0
  // id: a value previously returned from GetIdForWidget
  // returns: true if |id| is a valid id, false otherwise.
  //
  // If the widget referenced by |id| does not current have an X window id,
  // |*xid| is set to 0.
  bool GetXIDForId(XID* xid, gfx::NativeViewId id);

  // These are actually private functions, but need to be called from statics.
  void OnRealize(gfx::NativeView widget);
  void OnUnrealize(gfx::NativeView widget);
  void OnDestroy(gfx::NativeView widget);

 private:
  // This object is a singleton:
  GtkNativeViewManager();
  friend struct DefaultSingletonTraits<GtkNativeViewManager>;

  struct NativeViewInfo {
    NativeViewInfo()
        : x_window_id(0) {
    }

    XID x_window_id;
  };

  gfx::NativeViewId GetWidgetId(gfx::NativeView id);

  // protects native_view_to_id_ and id_to_info_
  Lock lock_;
    // If asked for an id for the same widget twice, we want to return the same
    // id. So this records the current mapping.
    std::map<gfx::NativeView, gfx::NativeViewId> native_view_to_id_;
    std::map<gfx::NativeViewId, NativeViewInfo> id_to_info_;

  DISALLOW_COPY_AND_ASSIGN(GtkNativeViewManager);
};

#endif  // BASE_GFX_GTK_NATIVE_VIEW_ID_MANAGER_H_
