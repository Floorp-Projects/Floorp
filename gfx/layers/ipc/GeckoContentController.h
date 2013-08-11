/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_GeckoContentController_h
#define mozilla_layers_GeckoContentController_h

#include "FrameMetrics.h"               // for FrameMetrics, etc
#include "Units.h"                      // for CSSIntPoint, CSSRect, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "nsISupportsImpl.h"

class Task;

namespace mozilla {
namespace layers {

class GeckoContentController
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GeckoContentController)

  /**
   * Requests a paint of the given FrameMetrics |aFrameMetrics| from Gecko.
   * Implementations per-platform are responsible for actually handling this.
   */
  virtual void RequestContentRepaint(const FrameMetrics& aFrameMetrics) = 0;

  /**
   * Requests handling of a double tap. |aPoint| is in CSS pixels, relative to
   * the current scroll offset. This should eventually round-trip back to
   * AsyncPanZoomController::ZoomToRect with the dimensions that we want to zoom
   * to.
   */
  virtual void HandleDoubleTap(const CSSIntPoint& aPoint) = 0;

  /**
   * Requests handling a single tap. |aPoint| is in CSS pixels, relative to the
   * current scroll offset. This should simulate and send to content a mouse
   * button down, then mouse button up at |aPoint|.
   */
  virtual void HandleSingleTap(const CSSIntPoint& aPoint) = 0;

  /**
   * Requests handling a long tap. |aPoint| is in CSS pixels, relative to the
   * current scroll offset.
   */
  virtual void HandleLongTap(const CSSIntPoint& aPoint) = 0;

  /**
   * Requests sending a mozbrowserasyncscroll domevent to embedder.
   * |aContentRect| is in CSS pixels, relative to the current cssPage.
   * |aScrollableSize| is the current content width/height in CSS pixels.
   */
  virtual void SendAsyncScrollDOMEvent(FrameMetrics::ViewID aScrollId,
                                       const CSSRect &aContentRect,
                                       const CSSSize &aScrollableSize) = 0;

  /**
   * Schedules a runnable to run on the controller/UI thread at some time
   * in the future.
   */
  virtual void PostDelayedTask(Task* aTask, int aDelayMs) = 0;


  /**
   * Request any special actions be performed when panning starts
   */
  virtual void HandlePanBegin() {}

  /**
   * Request any special actions be performed when panning ends
   */
  virtual void HandlePanEnd() {}

  GeckoContentController() {}
  virtual ~GeckoContentController() {}
};

}
}

#endif // mozilla_layers_GeckoContentController_h
