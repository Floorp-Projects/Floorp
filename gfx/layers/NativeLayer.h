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
class SurfacePoolHandle;

// NativeLayerRoot and NativeLayer allow building up a flat layer "tree" of
// sibling layers. These layers provide a cross-platform abstraction for the
// platform's native layers, such as CoreAnimation layers on macOS.
// Every layer has a rectangle that describes its position and size in the
// window. The native layer root is usually be created by the window, and then
// the compositing subsystem uses it to create and place the actual layers.
class NativeLayerRoot {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NativeLayerRoot)

  virtual already_AddRefed<NativeLayer> CreateLayer(
      const gfx::IntSize& aSize, bool aIsOpaque,
      SurfacePoolHandle* aSurfacePoolHandle) = 0;
  virtual void AppendLayer(NativeLayer* aLayer) = 0;
  virtual void RemoveLayer(NativeLayer* aLayer) = 0;
  virtual void SetLayers(const nsTArray<RefPtr<NativeLayer>>& aLayers) = 0;

  // Publish the layer changes to the screen. Returns whether the commit was
  // successful.
  virtual bool CommitToScreen() = 0;

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

  // The size and opaqueness of a layer are supplied during layer creation and
  // never change.
  virtual gfx::IntSize GetSize() = 0;
  virtual bool IsOpaque() = 0;

  // The location of the layer, in integer device pixels.
  virtual void SetPosition(const gfx::IntPoint& aPosition) = 0;
  virtual gfx::IntPoint GetPosition() = 0;

  virtual gfx::IntRect GetRect() = 0;

  // Set an optional clip rect on the layer. The clip rect is in the same
  // coordinate space as the layer rect.
  virtual void SetClipRect(const Maybe<gfx::IntRect>& aClipRect) = 0;
  virtual Maybe<gfx::IntRect> ClipRect() = 0;

  // Whether the surface contents are flipped vertically compared to this
  // layer's coordinate system. Can be set on any thread at any time.
  virtual void SetSurfaceIsFlipped(bool aIsFlipped) = 0;
  virtual bool SurfaceIsFlipped() = 0;

  // Returns a DrawTarget. The size of the DrawTarget will be the same as the
  // size of this layer. The caller should draw to that DrawTarget, then drop
  // its reference to the DrawTarget, and then call NotifySurfaceReady(). It can
  // limit its drawing to aUpdateRegion (which is in the DrawTarget's device
  // space). After a call to NextSurface*, NextSurface* must not be called again
  // until after NotifySurfaceReady has been called. Can be called on any
  // thread. When used from multiple threads, callers need to make sure that
  // they still only call NextSurface* and NotifySurfaceReady alternatingly and
  // not in any other order. aUpdateRegion must not extend beyond the layer
  // size.
  virtual RefPtr<gfx::DrawTarget> NextSurfaceAsDrawTarget(
      const gfx::IntRegion& aUpdateRegion, gfx::BackendType aBackendType) = 0;

  // Returns a GLuint for a framebuffer that can be used for drawing to the
  // surface. The size of the framebuffer will be the same as the size of this
  // layer. If aNeedsDepth is true, the framebuffer is created with a depth
  // buffer.
  // The framebuffer's depth buffer (if present) may be shared with other
  // framebuffers of the same size, even from entirely different NativeLayer
  // objects. The caller should not assume anything about the depth buffer's
  // existing contents (i.e. it should clear it at the beginning of the draw).
  // Callers should draw to one layer at a time, such that there is no
  // interleaved drawing to different framebuffers that could be tripped up by
  // the sharing.
  // The caller should draw to the framebuffer, unbind it, and then call
  // NotifySurfaceReady(). It can limit its drawing to aUpdateRegion (which is
  // in the framebuffer's device space, possibly "upside down" if
  // SurfaceIsFlipped()).
  // The framebuffer will be created in the GLContext that this layer's
  // SurfacePoolHandle was created for.
  // After a call to NextSurface*, NextSurface* must not be called again until
  // after NotifySurfaceReady has been called. Can be called on any thread. When
  // used from multiple threads, callers need to make sure that they still only
  // call NextSurface and NotifySurfaceReady alternatingly and not in any other
  // order.
  // aUpdateRegion must not extend beyond the layer size.
  virtual Maybe<GLuint> NextSurfaceAsFramebuffer(
      const gfx::IntRegion& aUpdateRegion, bool aNeedsDepth) = 0;

  // Indicates that the surface which has been returned from the most recent
  // call to NextSurface* is now finished being drawn to and can be displayed on
  // the screen. Resets the invalid region on the surface to the empty region.
  virtual void NotifySurfaceReady() = 0;

  // If you know that this layer will likely not draw any more frames, then it's
  // good to call DiscardBackbuffers in order to save memory and allow other
  // layer's to pick up the released surfaces from the pool.
  virtual void DiscardBackbuffers() = 0;

 protected:
  virtual ~NativeLayer() {}
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_NativeLayer_h
