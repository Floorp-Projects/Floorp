/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITOR_H
#define MOZILLA_GFX_COMPOSITOR_H

#include "Units.h"                      // for ScreenPoint
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for TemporaryRef, RefCounted
#include "mozilla/gfx/Point.h"          // for IntSize, Point
#include "mozilla/gfx/Rect.h"           // for Rect, IntRect
#include "mozilla/gfx/Types.h"          // for Float
#include "mozilla/layers/CompositorTypes.h"  // for DiagnosticTypes, etc
#include "mozilla/layers/FenceUtils.h"  // for FenceHandle
#include "mozilla/layers/LayersTypes.h"  // for LayersBackend
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
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
 *  - Layer, ShadowableLayer and LayerComposite
 *    (see Layers.h and ipc/ShadowLayers.h)
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
 *  - ContentHost
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
 *                          platform-specific function, e.g., CreateTextureHostOGL
 *    CompositableClient  - in the appropriate subclass, e.g.,
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
 * Compositor backends like OpenGL or flavours of D3D live in their own directory
 * under gfx/layers/. To add a new backend, implement at least the following
 * interfaces:
 * - Compositor (ex. CompositorOGL)
 * - TextureHost (ex. SurfaceTextureHost)
 * Depending on the type of data that needs to be serialized, you may need to
 * add specific TextureClient implementations.
 */

class nsIWidget;
class nsIntRegion;

namespace mozilla {
namespace gfx {
class Matrix;
class Matrix4x4;
class DrawTarget;
}

namespace layers {

struct Effect;
struct EffectChain;
class Image;
class Layer;
class TextureSource;
class DataTextureSource;
class CompositingRenderTarget;
class PCompositorParent;
class LayerManagerComposite;

enum SurfaceInitMode
{
  INIT_MODE_NONE,
  INIT_MODE_CLEAR
};

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
 * If the compositor is usually used for compositing but compositing is
 * temporarily done without the compositor, call EndFrameForExternalComposition
 * after compositing each frame so the compositor can remain internally
 * consistent.
 *
 * By default, the compositor will render to the screen, to render to a target,
 * call SetTargetContext or SetRenderTarget, the latter with a target created
 * by CreateRenderTarget or CreateRenderTargetFromSource.
 *
 * The target and viewport methods can be called before any DrawQuad call and
 * affect any subsequent DrawQuad calls.
 */
class Compositor
{
protected:
  virtual ~Compositor() {}

public:
  NS_INLINE_DECL_REFCOUNTING(Compositor)

  explicit Compositor(PCompositorParent* aParent = nullptr)
    : mCompositorID(0)
    , mDiagnosticTypes(DiagnosticTypes::NO_DIAGNOSTIC)
    , mParent(aParent)
    , mScreenRotation(ROTATION_0)
  {
  }

  virtual TemporaryRef<DataTextureSource> CreateDataTextureSource(TextureFlags aFlags = TextureFlags::NO_FLAGS) = 0;
  virtual bool Initialize() = 0;
  virtual void Destroy() = 0;

  /**
   * Return true if the effect type is supported.
   *
   * By default Compositor implementations should support all effects but in
   * some rare cases it is not possible to support an effect efficiently.
   * This is the case for BasicCompositor with EffectYCbCr.
   */
  virtual bool SupportsEffect(EffectTypes aEffect) { return true; }

  /**
   * Request a texture host identifier that may be used for creating textures
   * across process or thread boundaries that are compatible with this
   * compositor.
   */
  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() = 0;

  /**
   * Properties of the compositor.
   */
  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize& aSize) = 0;
  virtual int32_t GetMaxTextureSize() const = 0;

  /**
   * Set the target for rendering. Results will have been written to aTarget by
   * the time that EndFrame returns.
   *
   * If this method is not used, or we pass in nullptr, we target the compositor's
   * usual swap chain and render to the screen.
   */
  void SetTargetContext(gfx::DrawTarget* aTarget, const gfx::IntRect& aRect)
  {
    mTarget = aTarget;
    mTargetBounds = aRect;
  }
  void ClearTargetContext()
  {
    mTarget = nullptr;
  }

  typedef uint32_t MakeCurrentFlags;
  static const MakeCurrentFlags ForceMakeCurrent = 0x1;
  /**
   * Make this compositor's rendering context the current context for the
   * underlying graphics API. This may be a global operation, depending on the
   * API. Our context will remain the current one until someone else changes it.
   *
   * Clients of the compositor should call this at the start of the compositing
   * process, it might be required by texture uploads etc.
   *
   * If aFlags == ForceMakeCurrent then we will (re-)set our context on the
   * underlying API even if it is already the current context.
   */
  virtual void MakeCurrent(MakeCurrentFlags aFlags = 0) = 0;

  /**
   * Creates a Surface that can be used as a rendering target by this
   * compositor.
   */
  virtual TemporaryRef<CompositingRenderTarget>
  CreateRenderTarget(const gfx::IntRect& aRect, SurfaceInitMode aInit) = 0;

  /**
   * Creates a Surface that can be used as a rendering target by this
   * compositor, and initializes the surface by copying from aSource.
   * If aSource is null, then the current screen buffer is used as source.
   *
   * aSourcePoint specifies the point in aSource to copy data from.
   */
  virtual TemporaryRef<CompositingRenderTarget>
  CreateRenderTargetFromSource(const gfx::IntRect& aRect,
                               const CompositingRenderTarget* aSource,
                               const gfx::IntPoint& aSourcePoint) = 0;

  /**
   * Sets the given surface as the target for subsequent calls to DrawQuad.
   * Passing null as aSurface sets the screen as the target.
   */
  virtual void SetRenderTarget(CompositingRenderTarget* aSurface) = 0;

  /**
   * Returns the current target for rendering. Will return null if we are
   * rendering to the screen.
   */
  virtual CompositingRenderTarget* GetCurrentRenderTarget() const = 0;

  /**
   * Mostly the compositor will pull the size from a widget and this method will
   * be ignored, but compositor implementations are free to use it if they like.
   */
  virtual void SetDestinationSurfaceSize(const gfx::IntSize& aSize) = 0;

  /**
   * Declare an offset to use when rendering layers. This will be ignored when
   * rendering to a target instead of the screen.
   */
  virtual void SetScreenRenderOffset(const ScreenPoint& aOffset) = 0;

  /**
   * Tell the compositor to draw a quad. What to do draw and how it is
   * drawn is specified by aEffectChain. aRect is the quad to draw, in user space.
   * aTransform transforms from user space to screen space. If texture coords are
   * required, these will be in the primary effect in the effect chain.
   */
  virtual void DrawQuad(const gfx::Rect& aRect, const gfx::Rect& aClipRect,
                        const EffectChain& aEffectChain,
                        gfx::Float aOpacity, const gfx::Matrix4x4 &aTransform) = 0;

  /*
   * Clear aRect on current render target.
   */
  virtual void ClearRect(const gfx::Rect& aRect) = 0;

  /**
   * Start a new frame.
   *
   * aInvalidRect is the invalid region of the screen; it can be ignored for
   * compositors where the performance for compositing the entire window is
   * sufficient.
   *
   * aClipRectIn is the clip rect for the window in window space (optional).
   * aTransform is the transform from user space to window space.
   * aRenderBounds bounding rect for rendering, in user space.
   *
   * If aClipRectIn is null, this method sets *aClipRectOut to the clip rect
   * actually used for rendering (if aClipRectIn is non-null, we will use that
   * for the clip rect).
   *
   * If aRenderBoundsOut is non-null, it will be set to the render bounds
   * actually used by the compositor in window space. If aRenderBoundsOut
   * is returned empty, composition should be aborted.
   */
  virtual void BeginFrame(const nsIntRegion& aInvalidRegion,
                          const gfx::Rect* aClipRectIn,
                          const gfx::Rect& aRenderBounds,
                          gfx::Rect* aClipRectOut = nullptr,
                          gfx::Rect* aRenderBoundsOut = nullptr) = 0;

  /**
   * Flush the current frame to the screen and tidy up.
   */
  virtual void EndFrame() = 0;

  virtual void SetDispAcquireFence(Layer* aLayer) {}

  virtual FenceHandle GetReleaseFence()
  {
    return FenceHandle();
  }

  /**
   * Post-rendering stuff if the rendering is done outside of this Compositor
   * e.g., by Composer2D.
   * aTransform is the transform from user space to window space.
   */
  virtual void EndFrameForExternalComposition(const gfx::Matrix& aTransform) = 0;

  /**
   * Setup the viewport and projection matrix for rendering to a target of the
   * given dimensions. The size and transform here will override those set in
   * BeginFrame. BeginFrame sets a size and transform for the default render
   * target, usually the screen. Calling this method prepares the compositor to
   * render using a different viewport (that is, size and transform), usually
   * associated with a new render target.
   */
  virtual void PrepareViewport(const gfx::IntSize& aSize) = 0;

  /**
   * Whether textures created by this compositor can receive partial updates.
   */
  virtual bool SupportsPartialTextureUpdate() = 0;

  void SetDiagnosticTypes(DiagnosticTypes aDiagnostics)
  {
    mDiagnosticTypes = aDiagnostics;
  }

  DiagnosticTypes GetDiagnosticTypes() const
  {
    return mDiagnosticTypes;
  }

  void DrawDiagnostics(DiagnosticFlags aFlags,
                       const gfx::Rect& visibleRect,
                       const gfx::Rect& aClipRect,
                       const gfx::Matrix4x4& transform,
                       uint32_t aFlashCounter = DIAGNOSTIC_FLASH_COUNTER_MAX);

  void DrawDiagnostics(DiagnosticFlags aFlags,
                       const nsIntRegion& visibleRegion,
                       const gfx::Rect& aClipRect,
                       const gfx::Matrix4x4& transform,
                       uint32_t aFlashCounter = DIAGNOSTIC_FLASH_COUNTER_MAX);

#ifdef MOZ_DUMP_PAINTING
  virtual const char* Name() const = 0;
#endif // MOZ_DUMP_PAINTING

  virtual LayersBackend GetBackendType() const = 0;

  /**
   * Each Compositor has a unique ID.
   * This ID is used to keep references to each Compositor in a map accessed
   * from the compositor thread only, so that async compositables can find
   * the right compositor parent and schedule compositing even if the compositor
   * changed.
   */
  uint32_t GetCompositorID() const
  {
    return mCompositorID;
  }
  void SetCompositorID(uint32_t aID)
  {
    MOZ_ASSERT(mCompositorID == 0, "The compositor ID must be set only once.");
    mCompositorID = aID;
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

  /**
   * Call before rendering begins to ensure the compositor is ready to
   * composite. Returns false if rendering should be aborted.
   */
  virtual bool Ready() { return true; }

  // XXX I expect we will want to move mWidget into this class and implement
  // these methods properly.
  virtual nsIWidget* GetWidget() const { return nullptr; }

  /**
   * Debug-build assertion that can be called to ensure code is running on the
   * compositor thread.
   */
  static void AssertOnCompositorThread();

  /**
   * We enforce that there can only be one Compositor backend type off the main
   * thread at the same time. The backend type in use can be checked with this
   * static method. We need this for creating texture clients/hosts etc. when we
   * don't have a reference to a Compositor.
   *
   * This can only be used from the compositor thread!
   */
  static LayersBackend GetBackend();

  size_t GetFillRatio() {
    float fillRatio = 0;
    if (mPixelsFilled > 0 && mPixelsPerFrame > 0) {
      fillRatio = 100.0f * float(mPixelsFilled) / float(mPixelsPerFrame);
      if (fillRatio > 999.0f) {
        fillRatio = 999.0f;
      }
    }
    return fillRatio;
  }

  ScreenRotation GetScreenRotation() const {
    return mScreenRotation;
  }

  void SetScreenRotation(ScreenRotation aRotation) {
    mScreenRotation = aRotation;
  }

protected:
  void DrawDiagnosticsInternal(DiagnosticFlags aFlags,
                               const gfx::Rect& aVisibleRect,
                               const gfx::Rect& aClipRect,
                               const gfx::Matrix4x4& transform,
                               uint32_t aFlashCounter);

  bool ShouldDrawDiagnostics(DiagnosticFlags);

  /**
   * Set the global Compositor backend, checking that one isn't already set.
   */
  static void SetBackend(LayersBackend backend);

  uint32_t mCompositorID;
  DiagnosticTypes mDiagnosticTypes;
  PCompositorParent* mParent;

  /**
   * We keep track of the total number of pixels filled as we composite the
   * current frame. This value is an approximation and is not accurate,
   * especially in the presence of transforms.
   */
  size_t mPixelsPerFrame;
  size_t mPixelsFilled;

  ScreenRotation mScreenRotation;

  virtual gfx::IntSize GetWidgetSize() const = 0;

  RefPtr<gfx::DrawTarget> mTarget;
  gfx::IntRect mTargetBounds;

private:
  static LayersBackend sBackend;

};

// Returns the number of rects. (Up to 4)
typedef gfx::Rect decomposedRectArrayT[4];
size_t DecomposeIntoNoRepeatRects(const gfx::Rect& aRect,
                                  const gfx::Rect& aTexCoordRect,
                                  decomposedRectArrayT* aLayerRects,
                                  decomposedRectArrayT* aTextureRects);

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_COMPOSITOR_H */
