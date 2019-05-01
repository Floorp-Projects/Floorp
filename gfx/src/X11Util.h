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
  return GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
#endif
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

/**
 * On construction, set a graceful X error handler that doesn't crash the
 * application and records X errors. On destruction, restore the X error handler
 * to what it was before construction.
 *
 * The SyncAndGetError() method allows to know whether a X error occurred,
 * optionally allows to get the full XErrorEvent, and resets the recorded X
 * error state so that a single X error will be reported only once.
 *
 * Nesting is correctly handled: multiple nested ScopedXErrorHandler's don't
 * interfere with each other's state. However, if SyncAndGetError is not called
 * on the nested ScopedXErrorHandler, then any X errors caused by X calls made
 * while the nested ScopedXErrorHandler was in place may then be caught by the
 * other ScopedXErrorHandler. This is just a result of X being asynchronous and
 * us not doing any implicit syncing: the only method in this class what causes
 * syncing is SyncAndGetError().
 *
 * This class is not thread-safe at all. It is assumed that only one thread is
 * using any ScopedXErrorHandler's. Given that it's not used on Mac, it should
 * be easy to make it thread-safe by using thread-local storage with __thread.
 */
class ScopedXErrorHandler {
 public:
  // trivial wrapper around XErrorEvent, just adding ctor initializing by zero.
  struct ErrorEvent {
    XErrorEvent mError;

    ErrorEvent() { memset(this, 0, sizeof(ErrorEvent)); }
  };

 private:
  // this ScopedXErrorHandler's ErrorEvent object
  ErrorEvent mXError;

  // static pointer for use by the error handler
  static ErrorEvent* sXErrorPtr;

  // what to restore sXErrorPtr to on destruction
  ErrorEvent* mOldXErrorPtr;

  // what to restore the error handler to on destruction
  int (*mOldErrorHandler)(Display*, XErrorEvent*);

 public:
  static int ErrorHandler(Display*, XErrorEvent* ev);

  /**
   * @param aAllowOffMainThread whether to warn if used off main thread
   */
  explicit ScopedXErrorHandler(bool aAllowOffMainThread = false);

  ~ScopedXErrorHandler();

  /** \returns true if a X error occurred since the last time this method was
   * called on this ScopedXErrorHandler object, or since the creation of this
   * ScopedXErrorHandler object if this method was never called on it.
   *
   * \param ev this optional parameter, if set, will be filled with the
   * XErrorEvent object. If multiple errors occurred, the first one will be
   * returned.
   */
  bool SyncAndGetError(Display* dpy, XErrorEvent* ev = nullptr);
};

class OffMainThreadScopedXErrorHandler : public ScopedXErrorHandler {
 public:
  OffMainThreadScopedXErrorHandler() : ScopedXErrorHandler(true) {}
};

}  // namespace mozilla

#endif  // mozilla_X11Util_h
