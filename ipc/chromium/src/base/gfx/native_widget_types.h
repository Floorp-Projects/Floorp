// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GFX_NATIVE_WIDGET_TYPES_H_
#define BASE_GFX_NATIVE_WIDGET_TYPES_H_

#include "base/basictypes.h"
#include "build/build_config.h"

// This file provides cross platform typedefs for native widget types.
//   NativeWindow: this is a handle to a native, top-level window
//   NativeView: this is a handle to a native UI element. It may be the
//     same type as a NativeWindow on some platforms.
//   NativeViewId: Often, in our cross process model, we need to pass around a
//     reference to a "window". This reference will, say, be echoed back from a
//     renderer to the browser when it wishes to query it's size. On Windows, a
//     HWND can be used for this. On other platforms, we may wish to pass
//     around X window ids, or maybe abstract identifiers.
//
//     As a rule of thumb - if you're in the renderer, you should be dealing
//     with NativeViewIds. This should remind you that you shouldn't be doing
//     direct operations on platform widgets from the renderer process.
//
//     If you're in the browser, you're probably dealing with NativeViews,
//     unless you're in the IPC layer, which will be translating between
//     NativeViewIds from the renderer and NativeViews.
//
//   NativeEditView: a handle to a native edit-box. The Mac folks wanted this
//     specific typedef.
//
// The name 'View' here meshes with OS X where the UI elements are called
// 'views' and with our Chrome UI code where the elements are also called
// 'views'.

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
struct CGContext;
#ifdef __OBJC__
@class NSView;
@class NSWindow;
@class NSTextField;
#else
class NSView;
class NSWindow;
class NSTextField;
#endif  // __OBJC__
#elif defined(OS_LINUX)
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;
typedef struct _cairo_surface cairo_surface_t;
#endif

namespace gfx {

#if defined(OS_WIN)
typedef HWND NativeView;
typedef HWND NativeWindow;
typedef HWND NativeEditView;
typedef HDC NativeDrawingContext;
#elif defined(OS_MACOSX)
typedef NSView* NativeView;
typedef NSWindow* NativeWindow;
typedef NSTextField* NativeEditView;
typedef CGContext* NativeDrawingContext;
#elif defined(OS_LINUX)
typedef GtkWidget* NativeView;
typedef GtkWindow* NativeWindow;
typedef GtkWidget* NativeEditView;
typedef cairo_surface_t* NativeDrawingContext;
#endif

// Note: for test_shell we're packing a pointer into the NativeViewId. So, if
// you make it a type which is smaller than a pointer, you have to fix
// test_shell.
//
// See comment at the top of the file for usage.
typedef intptr_t NativeViewId;

// Convert a NativeViewId to a NativeView.
// On Windows, these are both HWNDS so it's just a cast.
// On Mac, for now, we pass the NSView pointer into the renderer
// On Linux we use an opaque id
#if defined(OS_WIN) || defined(OS_MACOSX)
static inline NativeView NativeViewFromId(NativeViewId id) {
  return reinterpret_cast<NativeView>(id);
}
#elif defined(OS_LINUX)
// A NativeView on Linux is a GtkWidget*. However, we can't go directly from an
// X window ID to a GtkWidget. Thus, functions which handle NativeViewIds from
// the renderer have to use Xlib. This is fine since these functions are
// generally performed on the BACKGROUND_X thread which can't use GTK anyway.

#define NativeViewFromId(x) NATIVE_VIEW_FROM_ID_NOT_AVAILIBLE_ON_LINUX

#endif  // defined(OS_LINUX)

// Convert a NativeView to a NativeViewId. See the comments above
// NativeViewFromId.
#if defined(OS_WIN) || defined(OS_MACOSX)
static inline NativeViewId IdFromNativeView(NativeView view) {
  return reinterpret_cast<NativeViewId>(view);
}
#elif defined(OS_LINUX)
// Not inlined because it involves pulling too many headers.
NativeViewId IdFromNativeView(NativeView view);
#endif  // defined(OS_LINUX)

}  // namespace gfx

#endif  // BASE_GFX_NATIVE_WIDGET_TYPES_H_
