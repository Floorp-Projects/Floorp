/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_NativeLayer_h
#define mozilla_layers_NativeLayer_h

#include "mozilla/Maybe.h"

#include "GLTypes.h"
#include "nsISupportsImpl.h"
#include "nsRegion.h"

namespace mozilla {

namespace gl {
class GLContext;
}  // namespace gl

namespace layers {

class NativeLayer;
class NativeLayerCA;

// NativeLayerRoot and NativeLayer allow building up a flat layer "tree" of
// sibling layers. These layers provide a cross-platform abstraction for the
// platform's native layers, such as CoreAnimation layers on macOS.
// Every layer has a rectangle that describes its position and size in the
// window. The native layer root is usually be created by the window, and then
// the compositing subsystem uses it to create and place the actual layers.
class NativeLayerRoot {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NativeLayerRoot)

  virtual already_AddRefed<NativeLayer> CreateLayer() = 0;
  virtual void AppendLayer(NativeLayer* aLayer) = 0;
  virtual void RemoveLayer(NativeLayer* aLayer) = 0;

 protected:
  virtual ~NativeLayerRoot() {}
};

// Represents a native layer. Native layers, such as CoreAnimation layers on
// macOS, are used to put pixels on the screen and to refresh and manipulate
// the visual contents of a window efficiently. For example, drawing to a layer
// once and then displaying the layer for multiple frames while moving it to
// different positions will be more efficient than drawing into a window (or a
// non-moving layer) multiple times with different internal offsets.
// There are two sources of "work" for a given composited frame: 1) Our own
// drawing (such as OpenGL compositing into a window or layer) and 2) the
// compositing window manager's work to update the screen. Every pixel we draw
// needs to be copied to the screen by the window manager. This suggests two
// avenues for reducing the work load for a given frame: Drawing fewer pixels
// ourselves, and making the window manager copy fewer pixels to the screen.
// Smart use of native layers allows reducing both work loads: If a visual
// change can be expressed purely as a layer attribute change (such as a change
// in the layer's position), this lets us eliminate our own drawing for that
// change. And secondly, manipulating a small layer rather than a large layer
// will reduce the window manager's work for that frame because it'll only copy
// the pixels of the small layer to the screen.
class NativeLayer {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NativeLayer)

  virtual NativeLayerCA* AsNativeLayerCA() { return nullptr; }

  // The location and size of the layer, in integer device pixels. This also
  // determines the size of the surface that should be returned from the next
  // call to NextSurface.
  virtual void SetRect(const gfx::IntRect& aRect) = 0;
  virtual gfx::IntRect GetRect() = 0;

  // Define which parts of the layer are opaque..
  virtual void SetOpaqueRegion(const gfx::IntRegion& aRegion) = 0;
  virtual gfx::IntRegion OpaqueRegion() = 0;

  // Whether the surface contents are flipped vertically compared to this
  // layer's coordinate system. Can be set on any thread at any time.
  virtual void SetSurfaceIsFlipped(bool aIsFlipped) = 0;
  virtual bool SurfaceIsFlipped() = 0;

  // Invalidates the specified region in all surfaces that are tracked by this
  // layer.
  virtual void InvalidateRegionThroughoutSwapchain(
      const gfx::IntRegion& aRegion) = 0;

  // Returns a DrawTarget. The size of the DrawTarget will be the size of the
  // rect that has been passed to SetRect. The caller should draw to that
  // DrawTarget, then drop its reference to the DrawTarget, and then call
  // NotifySurfaceReady(). It can limit its drawing to
  // CurrentSurfaceInvalidRegion() (which is in the DrawTarget's device space).
  // After a call to NextSurface*, NextSurface* must not be called again until
  // after NotifySurfaceReady has been called. Can be called on any thread. When
  // used from multiple threads, callers need to make sure that they still only
  // call NextSurface and NotifySurfaceReady alternatingly and not in any other
  // order.
  virtual RefPtr<gfx::DrawTarget> NextSurfaceAsDrawTarget(
      gfx::BackendType aBackendType) = 0;

  // Set the GLContext to use for the MozFramebuffer that are returned from
  // NextSurfaceAsFramebuffer. If changed to a different value, all
  // MozFramebuffers tracked by this layer will be discarded.
  // It's a good idea to call SetGLContext(nullptr) before destroying this
  // layer so that GL resource destruction happens at a good time and on the
  // right thread.
  virtual void SetGLContext(gl::GLContext* aGLContext) = 0;
  virtual gl::GLContext* GetGLContext() = 0;

  // Must only be called if a non-null GLContext is set on this layer.
  // Returns a GLuint for a framebuffer that can be used for drawing to the
  // surface. The size of the framebuffer will be the size of the rect that has
  // been passed to SetRect. If aNeedsDepth is true, the framebuffer is created
  // with a depth buffer. The caller should draw to the framebuffer, unbind
  // it, and then call NotifySurfaceReady(). It can limit its drawing to
  // CurrentSurfaceInvalidRegion() (which is in the framebuffer's device space,
  // possibly "upside down" if SurfaceIsFlipped()). The framebuffer will be
  // created using the GLContext that was set on this layer with a call to
  // SetGLContext. The NativeLayer will keep a reference to the MozFramebuffer
  // so that it can reuse the same MozFramebuffer whenever it uses the same
  // underlying surface. Calling SetGLContext with a different context will
  // release that reference. After a call to NextSurface*, NextSurface* must not
  // be called again until after NotifySurfaceReady has been called. Can be
  // called on any thread. When used from multiple threads, callers need to make
  // sure that they still only call NextSurface and NotifySurfaceReady
  // alternatingly and not in any other order.
  virtual Maybe<GLuint> NextSurfaceAsFramebuffer(bool aNeedsDepth) = 0;

  // The invalid region of the surface that has been returned from the most
  // recent call to NextSurface*. Newly-created surfaces are entirely invalid.
  // For surfaces that have been used before, the invalid region is the union of
  // all invalid regions that have been passed to
  // InvalidateRegionThroughoutSwapchain since the last time that
  // NotifySurfaceReady was called for this surface. Can only be called between
  // calls to NextSurface* and NotifySurfaceReady. Can be called on any thread.
  virtual gfx::IntRegion CurrentSurfaceInvalidRegion() = 0;

  // Indicates that the surface which has been returned from the most recent
  // call to NextSurface* is now finished being drawn to and can be displayed on
  // the screen. Resets the invalid region on the surface to the empty region.
  virtual void NotifySurfaceReady() = 0;

 protected:
  virtual ~NativeLayer() {}
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_NativeLayer_h
