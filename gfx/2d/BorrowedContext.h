/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_BORROWED_CONTEXT_H
#define _MOZILLA_GFX_BORROWED_CONTEXT_H

#include "2D.h"

#ifdef MOZ_X11
#include <X11/extensions/Xrender.h>
#include <X11/Xlib.h>
#include "X11UndefineNone.h"
#endif

struct _cairo;
typedef struct _cairo cairo_t;

namespace mozilla {

namespace gfx {

/* This is a helper class that let's you borrow a cairo_t from a
 * DrawTargetCairo. This is used for drawing themed widgets.
 *
 * Callers should check the cr member after constructing the object
 * to see if it succeeded. The DrawTarget should not be used while
 * the context is borrowed. */
class BorrowedCairoContext
{
public:
  BorrowedCairoContext()
    : mCairo(nullptr)
    , mDT(nullptr)
  { }

  explicit BorrowedCairoContext(DrawTarget *aDT)
    : mDT(aDT)
  {
    mCairo = BorrowCairoContextFromDrawTarget(aDT);
  }

  // We can optionally Init after construction in
  // case we don't know what the DT will be at construction
  // time.
  cairo_t *Init(DrawTarget *aDT)
  {
    MOZ_ASSERT(!mDT, "Can't initialize twice!");
    mDT = aDT;
    return mCairo = BorrowCairoContextFromDrawTarget(aDT);
  }

  // The caller needs to call Finish if cr is non-null when
  // they are done with the context. This is currently explicit
  // instead of happening implicitly in the destructor to make
  // what's happening in the caller more clear. It also
  // let's you resume using the DrawTarget in the same scope.
  void Finish()
  {
    if (mCairo) {
      ReturnCairoContextToDrawTarget(mDT, mCairo);
      mCairo = nullptr;
    }
  }

  ~BorrowedCairoContext() {
    MOZ_ASSERT(!mCairo);
  }

  cairo_t *mCairo;
private:
  static cairo_t* BorrowCairoContextFromDrawTarget(DrawTarget *aDT);
  static void ReturnCairoContextToDrawTarget(DrawTarget *aDT, cairo_t *aCairo);
  DrawTarget *mDT;
};

#ifdef MOZ_X11
/* This is a helper class that let's you borrow an Xlib drawable from
 * a DrawTarget. This is used for drawing themed widgets.
 *
 * Callers should check the Xlib drawable after constructing the object
 * to see if it succeeded. The DrawTarget should not be used while
 * the drawable is borrowed. */
class BorrowedXlibDrawable
{
public:
  BorrowedXlibDrawable()
    : mDT(nullptr),
      mDisplay(nullptr),
      mDrawable(X11None),
      mScreen(nullptr),
      mVisual(nullptr),
      mXRenderFormat(nullptr)
  {}

  explicit BorrowedXlibDrawable(DrawTarget *aDT)
    : mDT(nullptr),
      mDisplay(nullptr),
      mDrawable(X11None),
      mScreen(nullptr),
      mVisual(nullptr),
      mXRenderFormat(nullptr)
  {
    Init(aDT);
  }

  // We can optionally Init after construction in
  // case we don't know what the DT will be at construction
  // time.
  bool Init(DrawTarget *aDT);

  // The caller needs to call Finish if drawable is non-zero when
  // they are done with the context. This is currently explicit
  // instead of happening implicitly in the destructor to make
  // what's happening in the caller more clear. It also
  // let's you resume using the DrawTarget in the same scope.
  void Finish();

  ~BorrowedXlibDrawable() {
    MOZ_ASSERT(!mDrawable);
  }

  Display *GetDisplay() const { return mDisplay; }
  Drawable GetDrawable() const { return mDrawable; }
  Screen *GetScreen() const { return mScreen; }
  Visual *GetVisual() const { return mVisual; }
  IntSize GetSize() const { return mSize; }
  Point GetOffset() const { return mOffset; }

  XRenderPictFormat* GetXRenderFormat() const { return mXRenderFormat; }

private:
  DrawTarget *mDT;
  Display *mDisplay;
  Drawable mDrawable;
  Screen *mScreen;
  Visual *mVisual;
  XRenderPictFormat *mXRenderFormat;
  IntSize mSize;
  Point mOffset;
};
#endif

#ifdef XP_DARWIN
/* This is a helper class that let's you borrow a CGContextRef from a
 * DrawTargetCG. This is used for drawing themed widgets.
 *
 * Callers should check the cg member after constructing the object
 * to see if it succeeded. The DrawTarget should not be used while
 * the context is borrowed. */
class BorrowedCGContext
{
public:
  BorrowedCGContext()
    : cg(nullptr)
    , mDT(nullptr)
  { }

  explicit BorrowedCGContext(DrawTarget *aDT)
    : mDT(aDT)
  {
    MOZ_ASSERT(aDT, "Caller should check for nullptr");
    cg = BorrowCGContextFromDrawTarget(aDT);
  }

  // We can optionally Init after construction in
  // case we don't know what the DT will be at construction
  // time.
  CGContextRef Init(DrawTarget *aDT)
  {
    MOZ_ASSERT(aDT, "Caller should check for nullptr");
    MOZ_ASSERT(!mDT, "Can't initialize twice!");
    mDT = aDT;
    cg = BorrowCGContextFromDrawTarget(aDT);
    return cg;
  }

  // The caller needs to call Finish if cg is non-null when
  // they are done with the context. This is currently explicit
  // instead of happening implicitly in the destructor to make
  // what's happening in the caller more clear. It also
  // let's you resume using the DrawTarget in the same scope.
  void Finish()
  {
    if (cg) {
      ReturnCGContextToDrawTarget(mDT, cg);
      cg = nullptr;
    }
  }

  ~BorrowedCGContext() {
    MOZ_ASSERT(!cg);
  }

  CGContextRef cg;
private:
#ifdef USE_SKIA
  static CGContextRef BorrowCGContextFromDrawTarget(DrawTarget *aDT);
  static void ReturnCGContextToDrawTarget(DrawTarget *aDT, CGContextRef cg);
#else
  static CGContextRef BorrowCGContextFromDrawTarget(DrawTarget *aDT) {
    MOZ_CRASH("Not supported without Skia");
  }

  static void ReturnCGContextToDrawTarget(DrawTarget *aDT, CGContextRef cg) {
    MOZ_CRASH("not supported without Skia");
  }
#endif
  DrawTarget *mDT;
};
#endif

} // namespace gfx
} // namespace mozilla

#endif // _MOZILLA_GFX_BORROWED_CONTEXT_H
