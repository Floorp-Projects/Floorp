// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_X11_UTIL_H_
#define CHROME_COMMON_X11_UTIL_H_

// This file declares utility functions for X11 (Linux only).
//
// These functions do not require the Xlib headers to be included (which is why
// we use a void* for Visual*). The Xlib headers are highly polluting so we try
// hard to limit their spread into the rest of the code.

#if (MOZ_WIDGET_GTK == 2)
typedef struct _GdkDrawable GdkWindow;
#else
typedef struct _GdkWindow GdkWindow;
#endif
typedef struct _GtkWidget GtkWidget;
typedef unsigned long XID;
typedef struct _XDisplay Display;

namespace base {
class Thread;
}
namespace x11_util {

// These functions use the GDK default display and this /must/ be called from
// the UI thread. Thus, they don't support multiple displays.

// These functions cache their results.

// Return an X11 connection for the current, primary display.
Display* GetXDisplay();
// Return true iff the connection supports X shared memory
bool QuerySharedMemorySupport(Display* dpy);
// Return true iff the display supports Xrender
bool QueryRenderSupport(Display* dpy);
// Return the default screen number for the display
int GetDefaultScreen(Display* display);

// These functions do not cache their results

// Get the X window id for the default root window
XID GetX11RootWindow();
// Get the X window id for the given GTK widget.
XID GetX11WindowFromGtkWidget(GtkWidget*);
XID GetX11WindowFromGdkWindow(GdkWindow*);
// Get a Visual from the given widget. Since we don't include the Xlib
// headers, this is returned as a void*.
void* GetVisualFromGtkWidget(GtkWidget*);
// Return the number of bits-per-pixel for a pixmap of the given depth
int BitsPerPixelForPixmapDepth(Display*, int depth);

// Return a handle to a server side pixmap. |shared_memory_key| is a SysV
// IPC key. The shared memory region must contain 32-bit pixels.
XID AttachSharedMemory(Display* display, int shared_memory_support);
void DetachSharedMemory(Display* display, XID shmseg);

// Return a handle to an XRender picture where |pixmap| is a handle to a
// pixmap containing Skia ARGB data.
XID CreatePictureFromSkiaPixmap(Display* display, XID pixmap);

void FreePicture(Display* display, XID picture);
void FreePixmap(Display* display, XID pixmap);

// These functions are for performing X opertions outside of the UI thread.

// Return the Display for the secondary X connection. We keep a second
// connection around for making X requests outside of the UI thread.
// This function may only be called from the BACKGROUND_X11 thread.
Display* GetSecondaryDisplay();

// Since one cannot include both WebKit header and Xlib headers in the same
// file (due to collisions), we wrap all the Xlib functions that we need here.
// These functions must be called on the BACKGROUND_X11 thread since they
// reference GetSecondaryDisplay().

// Get the position of the given window in screen coordinates as well as its
// current size.
bool GetWindowGeometry(int* x, int* y, unsigned* width, unsigned* height,
                       XID window);

// Find the immediate parent of an X window.
//
// parent_window: (output) the parent window of |window|, or 0.
// parent_is_root: (output) true iff the parent of |window| is the root window.
bool GetWindowParent(XID* parent_window, bool* parent_is_root, XID window);

}  // namespace x11_util

#endif  // CHROME_COMMON_X11_UTIL_H_
