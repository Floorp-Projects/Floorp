/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PRINTTARGET_H
#define MOZILLA_GFX_PRINTTARGET_H

#include "mozilla/RefPtr.h"
#include "mozilla/gfx/2D.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace gfx {

class DrawEventRecorder;

/**
 * A class that is used to draw output that is to be sent to a printer or print
 * preview.
 *
 * This class wraps a cairo_surface_t* and provides access to it via a
 * DrawTarget.  The various checkpointing methods manage the state of the
 * platform specific cairo_surface_t*.
 */
class PrintTarget {
public:

  NS_INLINE_DECL_REFCOUNTING(PrintTarget);

  /// Must be matched 1:1 by an EndPrinting/AbortPrinting call.
  virtual nsresult BeginPrinting(const nsAString& aTitle,
                                 const nsAString& aPrintToFileName,
                                 int32_t aStartPage,
                                 int32_t aEndPage) {
    return NS_OK;
  }
  virtual nsresult EndPrinting() {
    return NS_OK;
  }
  virtual nsresult AbortPrinting() {
#ifdef DEBUG
    mHasActivePage = false;
#endif
    return NS_OK;
  }
  virtual nsresult BeginPage() {
#ifdef DEBUG
    MOZ_ASSERT(!mHasActivePage, "Missing EndPage() call");
    mHasActivePage = true;
#endif
    return NS_OK;
  }
  virtual nsresult EndPage() {
#ifdef DEBUG
    mHasActivePage = false;
#endif
    return NS_OK;
  }

  /**
   * Releases the resources used by this PrintTarget.  Typically this should be
   * called after calling EndPrinting().  Calling this more than once is
   * allowed, but subsequent calls are a no-op.
   *
   * Note that any DrawTarget obtained from this PrintTarget will no longer be
   * useful after this method has been called.
   */
  virtual void Finish();

  /**
   * Returns true if to print landscape our consumers must apply a 90 degrees
   * rotation to our DrawTarget.
   */
  virtual bool RotateNeededForLandscape() const {
    return false;
  }

  const IntSize& GetSize() const {
    return mSize;
  }

  /**
   * Makes a DrawTarget to draw the printer output to, or returns null on
   * failure.
   *
   * If aRecorder is passed a recording DrawTarget will be created instead of
   * the type of DrawTarget that would normally be returned for a particular
   * subclass of this class.  This argument is only intended to be used in
   * the e10s content process if printing output can't otherwise be transfered
   * over to the parent process using the normal DrawTarget type.
   *
   * NOTE: this should only be called between BeginPage()/EndPage() calls, and
   * the returned DrawTarget should not be drawn to after EndPage() has been
   * called.
   *
   * XXX For consistency with the old code this takes a size parameter even
   * though we already have the size passed to our subclass's CreateOrNull
   * factory methods.  The size passed to the factory method comes from
   * nsIDeviceContextSpec::MakePrintTarget overrides, whereas the size
   * passed to us comes from nsDeviceContext::CreateRenderingContext.  In at
   * least one case (nsDeviceContextSpecAndroid::MakePrintTarget) these are
   * different.  At some point we should align the two sources and get rid of
   * this method's size parameter.
   *
   * XXX For consistency with the old code this returns a new DrawTarget for
   * each call.  Perhaps we can create and cache a DrawTarget in our subclass's
   * CreateOrNull factory methods and return that on each call?  Currently that
   * seems to cause Mochitest failures on Windows though, which coincidentally
   * is the only platform where we get passed an aRecorder.  Probably the
   * issue is that we get called more than once with a different aRecorder, so
   * storing one recording DrawTarget for our lifetime doesn't currently work.
   *
   * XXX Could we pass aRecorder to our subclass's CreateOrNull factory methods?
   * We'd need to check that our consumers always pass the same aRecorder for
   * our entire lifetime.
   *
   * XXX Once PrintTargetThebes is removed this can become non-virtual.
   *
   * XXX In the long run, this class and its sub-classes should be converted to
   * use STL classes and mozilla::RefCounted<> so the can be moved to Moz2D.
   *
   * TODO: Consider adding a SetDPI method that calls
   * cairo_surface_set_fallback_resolution.
   */
  virtual already_AddRefed<DrawTarget>
  MakeDrawTarget(const IntSize& aSize,
                 DrawEventRecorder* aRecorder = nullptr);

  /**
   * Returns a reference DrawTarget. Unlike MakeDrawTarget, this method is not
   * restricted to being called between BeginPage()/EndPage() calls, and the
   * returned DrawTarget it is still valid to use after EndPage() has been
   * called.
   */
  virtual already_AddRefed<DrawTarget> GetReferenceDrawTarget(DrawEventRecorder* aRecorder);

protected:

  // Only created via subclass's constructors
  explicit PrintTarget(cairo_surface_t* aCairoSurface, const IntSize& aSize);

  // Protected because we're refcounted
  virtual ~PrintTarget();

  already_AddRefed<DrawTarget>
  CreateRecordingDrawTarget(DrawEventRecorder* aRecorder,
                            DrawTarget* aDrawTarget);

  cairo_surface_t* mCairoSurface;
  RefPtr<DrawTarget> mRefDT; // reference DT
  IntSize mSize;
  bool mIsFinished;
#ifdef DEBUG
  bool mHasActivePage;
#endif
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_PRINTTARGET_H */
