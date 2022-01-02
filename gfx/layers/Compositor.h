/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITOR_H
#define MOZILLA_GFX_COMPOSITOR_H

#include "Units.h"                           // for ScreenPoint
#include "mozilla/Assertions.h"              // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"                  // for already_AddRefed, RefCounted
#include "mozilla/gfx/2D.h"                  // for DrawTarget
#include "mozilla/gfx/MatrixFwd.h"           // for Matrix, Matrix4x4
#include "mozilla/gfx/Point.h"               // for IntSize, Point
#include "mozilla/gfx/Polygon.h"             // for Polygon
#include "mozilla/gfx/Rect.h"                // for Rect, IntRect
#include "mozilla/gfx/Types.h"               // for Float
#include "mozilla/gfx/Triangle.h"            // for Triangle, TexturedTriangle
#include "mozilla/layers/CompositorTypes.h"  // for DiagnosticTypes, etc
#include "mozilla/layers/LayersTypes.h"      // for LayersBackend
#include "mozilla/layers/SurfacePool.h"      // for SurfacePoolHandle
#include "mozilla/layers/TextureSourceProvider.h"
#include "mozilla/widget/CompositorWidget.h"
#include "nsISupportsImpl.h"  // for MOZ_COUNT_CTOR, etc
#include "nsRegion.h"
#include <vector>
#include "mozilla/WidgetUtils.h"

/**
 * Different elements of a web pages are rendered into separate "layers" before
 * they are flattened into the final image that is brought to the screen.
 * See Layers.h for more informations about layers and why we use retained
 * structures.
 * Most of the documentation for layers is directly in the source code in the
 * form of doc comments. An overview can also be found in the the wiki:
 * https://wiki.mozilla.org/Gecko:Overview#Graphics
 *
 *
 * # Main interfaces and abstractions
 *
 *  - CompositableClient and CompositableHost
 *    (client/CompositableClient.h composite/CompositableHost.h)
 *  - TextureClient and TextureHost
 *    (client/TextureClient.h composite/TextureHost.h)
 *  - TextureSource
 *    (composite/TextureHost.h)
 *  - Forwarders
 *    (ipc/CompositableForwarder.h ipc/ShadowLayers.h)
 *  - Compositor
 *    (this file)
 *  - IPDL protocols
 *    (.ipdl files under the gfx/layers/ipc directory)
 *
 * The *Client and Shadowable* classes are always used on the content thread.
 * Forwarders are always used on the content thread.
 * The *Host and Shadow* classes are always used on the compositor thread.
 * Compositors, TextureSource, and Effects are always used on the compositor
 * thread.
 * Most enums and constants are declared in LayersTypes.h and CompositorTypes.h.
 *
 *
 * # Texture transfer
 *
 * Most layer classes own a Compositable plus some extra information like
 * transforms and clip rects. They are platform independent.
 * Compositable classes manipulate Texture objects and are reponsible for
 * things like tiling, buffer rotation or double buffering. Compositables
 * are also platform-independent. Examples of compositable classes are:
 *  - ImageClient
 *  - CanvasClient
 *  - etc.
 * Texture classes (TextureClient and TextureHost) are thin abstractions over
 * platform-dependent texture memory. They are maniplulated by compositables
 * and don't know about buffer rotations and such. The purposes of TextureClient
 * and TextureHost are to synchronize, serialize and deserialize texture data.
 * TextureHosts provide access to TextureSources that are views on the
 * Texture data providing the necessary api for Compositor backend to composite
 * them.
 *
 * Compositable and Texture clients and hosts are created using factory methods.
 * They should only be created by using their constructor in exceptional
 * circumstances. The factory methods are located:
 *    TextureClient       - CompositableClient::CreateTextureClient
 *    TextureHost         - TextureHost::CreateTextureHost, which calls a
 *                          platform-specific function, e.g.,
 * CreateTextureHostOGL CompositableClient  - in the appropriate subclass, e.g.,
 *                          CanvasClient::CreateCanvasClient
 *    CompositableHost    - CompositableHost::Create
 *
 *
 * # IPDL
 *
 * If off-main-thread compositing (OMTC) is enabled, compositing is performed
 * in a dedicated thread. In some setups compositing happens in a dedicated
 * process. Documentation may refer to either the compositor thread or the
 * compositor process.
 * See explanations in ShadowLayers.h.
 *
 *
 * # Backend implementations
 *
 * Compositor backends like OpenGL or flavours of D3D live in their own
 * directory under gfx/layers/. To add a new backend, implement at least the
 * following interfaces:
 * - Compositor (ex. CompositorOGL)
 * - TextureHost (ex. SurfaceTextureHost)
 * Depending on the type of data that needs to be serialized, you may need to
 * add specific TextureClient implementations.
 */

class nsIWidget;

namespace mozilla {
namespace gfx {
class DrawTarget;
class DataSourceSurface;
}  // namespace gfx

namespace layers {

struct Effect;
struct EffectChain;
class Image;
class Layer;
class TextureSource;
class DataTextureSource;
class CompositingRenderTarget;
class CompositorBridgeParent;
class NativeLayer;
class CompositorOGL;
class CompositorD3D11;
class TextureReadLock;
struct GPUStats;
class AsyncReadbackBuffer;
class RecordedFrame;

enum SurfaceInitMode { INIT_MODE_NONE, INIT_MODE_CLEAR };

/**
 * Common interface for compositor backends.
 *
 * Compositor provides a cross-platform interface to a set of operations for
 * compositing quads. Compositor knows nothing about the layer tree. It must be
 * told everything about each composited quad - contents, location, transform,
 * opacity, etc.
 *
 * In theory it should be possible for different widgets to use the same
 * compositor. In practice, we use one compositor per window.
 *
 * # Usage
 *
 * For an example of a user of Compositor, see LayerManagerComposite.
 *
 * Initialization: create a Compositor object, call Initialize().
 *
 * Destruction: destroy any resources associated with the compositor, call
 * Destroy(), delete the Compositor object.
 *
 * Composition:
 *  call BeginFrame,
 *  for each quad to be composited:
 *    call MakeCurrent if necessary (not necessary if no other context has been
 *      made current),
 *    take care of any texture upload required to composite the quad, this step
 *      is backend-dependent,
 *    construct an EffectChain for the quad,
 *    call DrawQuad,
 *  call EndFrame.
 *
 * By default, the compositor will render to the screen if BeginFrameForWindow
 * is called. To render to a target, call BeginFrameForTarget or
 * or SetRenderTarget, the latter with a target created
 * by CreateRenderTarget or CreateRenderTargetFromSource.
 *
 * The target and viewport methods can be called before any DrawQuad call and
 * affect any subsequent DrawQuad calls.
 */
class Compositor : public TextureSourceProvider {
 protected:
  virtual ~Compositor();

 public:
  explicit Compositor(widget::CompositorWidget* aWidget);

  bool IsValid() const override { return true; }

  virtual bool Initialize(nsCString* const out_failureReason) = 0;
  void Destroy() override;
  bool IsDestroyed() const { return mIsDestroyed; }

  /**
   * Creates a Surface that can be used as a rendering target by this
   * compositor.
   */
  virtual already_AddRefed<CompositingRenderTarget> CreateRenderTarget(
      const gfx::IntRect& aRect, SurfaceInitMode aInit) = 0;

  /**
   * Grab a snapshot of aSource and store it in aDest, so that the pixels can
   * be read on the CPU by mapping aDest at some point in the future.
   * aSource and aDest must have the same size.
   * If this is a GPU compositor, this call must not block on the GPU.
   * Returns whether the operation was successful.
   */
  virtual bool ReadbackRenderTarget(CompositingRenderTarget* aSource,
                                    AsyncReadbackBuffer* aDest) = 0;

  /**
   * Create an AsyncReadbackBuffer of the specified size. Can return null.
   */
  virtual already_AddRefed<AsyncReadbackBuffer> CreateAsyncReadbackBuffer(
      const gfx::IntSize& aSize) = 0;

  /**
   * Draw a part of aSource into the current render target.
   * Scaling is done with linear filtering.
   * Returns whether the operation was successful.
   */
  virtual bool BlitRenderTarget(CompositingRenderTarget* aSource,
                                const gfx::IntSize& aSourceSize,
                                const gfx::IntSize& aDestSize) = 0;

  /**
   * Sets the given surface as the target for subsequent calls to DrawQuad.
   * Passing null as aSurface sets the screen as the target.
   */
  virtual void SetRenderTarget(CompositingRenderTarget* aSurface) = 0;

  /**
   * Returns the current target for rendering. Will return null if we are
   * rendering to the screen.
   */
  virtual already_AddRefed<CompositingRenderTarget> GetCurrentRenderTarget()
      const = 0;

  /**
   * Returns a render target which contains the entire window's drawing.
   * On platforms where no such render target is used during compositing (e.g.
   * with buffered BasicCompositor, where only the invalid area is drawn to a
   * render target), this will return null.
   */
  virtual already_AddRefed<CompositingRenderTarget> GetWindowRenderTarget()
      const = 0;

  /**
   * Mostly the compositor will pull the size from a widget and this method will
   * be ignored, but compositor implementations are free to use it if they like.
   */
  virtual void SetDestinationSurfaceSize(const gfx::IntSize& aSize) = 0;

  /**
   * Tell the compositor to draw a quad. What to do draw and how it is
   * drawn is specified by aEffectChain. aRect is the quad to draw, in user
   * space. aTransform transforms from user space to screen space. If texture
   * coords are required, these will be in the primary effect in the effect
   * chain. aVisibleRect is used to determine which edges should be antialiased,
   * without applying the effect to the inner edges of a tiled layer.
   */
  virtual void DrawQuad(const gfx::Rect& aRect, const gfx::IntRect& aClipRect,
                        const EffectChain& aEffectChain, gfx::Float aOpacity,
                        const gfx::Matrix4x4& aTransform,
                        const gfx::Rect& aVisibleRect) = 0;

  void SetClearColor(const gfx::DeviceColor& aColor) { mClearColor = aColor; }

  /**
   * Start a new frame for rendering to the window.
   * Needs to be paired with a call to EndFrame() if the return value is not
   * Nothing().
   *
   * aInvalidRegion is the invalid region of the window.
   * aClipRect is the clip rect for all drawing (optional).
   * aRenderBounds is the bounding rect for rendering.
   * aOpaqueRegion is the area that contains opaque content.
   * All coordinates are in window space.
   *
   * Returns the non-empty render bounds actually used by the compositor in
   * window space, or Nothing() if composition should be aborted.
   */
  virtual Maybe<gfx::IntRect> BeginFrameForWindow(
      const nsIntRegion& aInvalidRegion, const Maybe<gfx::IntRect>& aClipRect,
      const gfx::IntRect& aRenderBounds, const nsIntRegion& aOpaqueRegion) = 0;

  /**
   * Flush the current frame to the screen and tidy up.
   *
   * Derived class overriding this should call Compositor::EndFrame.
   */
  virtual void EndFrame();

  virtual void CancelFrame(bool aNeedFlush = true) {}

#ifdef MOZ_DUMP_PAINTING
  virtual const char* Name() const = 0;
#endif  // MOZ_DUMP_PAINTING

  virtual CompositorD3D11* AsCompositorD3D11() { return nullptr; }

  Compositor* AsCompositor() override { return this; }

  TimeStamp GetLastCompositionEndTime() const override {
    return mLastCompositionEndTime;
  }

  /**
   * Notify the compositor that composition is being paused. This allows the
   * compositor to temporarily release any resources.
   * Between calling Pause and Resume, compositing may fail.
   */
  virtual void Pause() {}
  /**
   * Notify the compositor that composition is being resumed. The compositor
   * regain any resources it requires for compositing.
   * Returns true if succeeded.
   */
  virtual bool Resume() { return true; }

  widget::CompositorWidget* GetWidget() const { return mWidget; }

  /**
   * Request the compositor to allow recording its frames.
   *
   * This is a noop on |CompositorOGL|.
   */
  virtual void RequestAllowFrameRecording(bool aWillRecord) {
    mRecordFrames = aWillRecord;
  }

  /**
   * Record the current frame for readback by the |CompositionRecorder|.
   *
   * If this compositor does not support this feature, a null pointer is
   * returned instead.
   */
  already_AddRefed<RecordedFrame> RecordFrame(const TimeStamp& aTimeStamp);

 protected:
  /**
   * Whether or not the compositor should be prepared to record frames. While
   * this returns true, compositors are expected to maintain a full window
   * render target that they return from GetWindowRenderTarget() between
   * NormalDrawingDone() and EndFrame().
   *
   * This will be true when either we are recording a profile with screenshots
   * enabled or the |LayerManagerComposite| has requested us to record frames
   * for the |CompositionRecorder|.
   */
  bool ShouldRecordFrames() const;

  /**
   * Last Composition end time.
   */
  TimeStamp mLastCompositionEndTime;

  widget::CompositorWidget* mWidget;

  bool mIsDestroyed;

  gfx::DeviceColor mClearColor;

  bool mRecordFrames = false;

 private:
  static LayersBackend sBackend;
};

// Returns the number of rects. (Up to 4)
typedef gfx::Rect decomposedRectArrayT[4];
size_t DecomposeIntoNoRepeatRects(const gfx::Rect& aRect,
                                  const gfx::Rect& aTexCoordRect,
                                  decomposedRectArrayT* aLayerRects,
                                  decomposedRectArrayT* aTextureRects);

static inline bool BlendOpIsMixBlendMode(gfx::CompositionOp aOp) {
  switch (aOp) {
    case gfx::CompositionOp::OP_MULTIPLY:
    case gfx::CompositionOp::OP_SCREEN:
    case gfx::CompositionOp::OP_OVERLAY:
    case gfx::CompositionOp::OP_DARKEN:
    case gfx::CompositionOp::OP_LIGHTEN:
    case gfx::CompositionOp::OP_COLOR_DODGE:
    case gfx::CompositionOp::OP_COLOR_BURN:
    case gfx::CompositionOp::OP_HARD_LIGHT:
    case gfx::CompositionOp::OP_SOFT_LIGHT:
    case gfx::CompositionOp::OP_DIFFERENCE:
    case gfx::CompositionOp::OP_EXCLUSION:
    case gfx::CompositionOp::OP_HUE:
    case gfx::CompositionOp::OP_SATURATION:
    case gfx::CompositionOp::OP_COLOR:
    case gfx::CompositionOp::OP_LUMINOSITY:
      return true;
    default:
      return false;
  }
}

class AsyncReadbackBuffer {
 public:
  NS_INLINE_DECL_REFCOUNTING(AsyncReadbackBuffer)

  gfx::IntSize GetSize() const { return mSize; }
  virtual bool MapAndCopyInto(gfx::DataSourceSurface* aSurface,
                              const gfx::IntSize& aReadSize) const = 0;

 protected:
  explicit AsyncReadbackBuffer(const gfx::IntSize& aSize) : mSize(aSize) {}
  virtual ~AsyncReadbackBuffer() = default;

  gfx::IntSize mSize;
};

struct TexturedVertex {
  float position[2];
  float texCoords[2];
};

nsTArray<TexturedVertex> TexturedTrianglesToVertexArray(
    const nsTArray<gfx::TexturedTriangle>& aTriangles);

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_COMPOSITOR_H */
