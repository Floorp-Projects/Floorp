/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_NativeLayer_h
#define mozilla_layers_NativeLayer_h

#include "nsISupportsImpl.h"
#include "nsRegion.h"

namespace mozilla {
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

 protected:
  virtual ~NativeLayer() {}
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_NativeLayer_h
