/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerManagerMLGPU.h"
#include "LayerTreeInvalidation.h"
#include "PaintedLayerMLGPU.h"
#include "ImageLayerMLGPU.h"
#include "CanvasLayerMLGPU.h"
#include "GeckoProfiler.h"  // for profiler_*
#include "gfxEnv.h"         // for gfxEnv
#include "MLGDevice.h"
#include "RenderPassMLGPU.h"
#include "RenderViewMLGPU.h"
#include "ShaderDefinitionsMLGPU.h"
#include "SharedBufferMLGPU.h"
#include "UnitTransforms.h"
#include "TextureSourceProviderMLGPU.h"
#include "TreeTraversal.h"
#include "FrameBuilder.h"
#include "LayersLogging.h"
#include "UtilityMLGPU.h"
#include "CompositionRecorder.h"
#include "mozilla/layers/Diagnostics.h"
#include "mozilla/layers/TextRenderer.h"

#ifdef XP_WIN
#  include "mozilla/widget/WinCompositorWidget.h"
#  include "mozilla/gfx/DeviceManagerDx.h"
#endif

using namespace std;

namespace mozilla {
namespace layers {

using namespace gfx;

static const int kDebugOverlayX = 2;
static const int kDebugOverlayY = 5;
static const int kDebugOverlayMaxWidth = 600;
static const int kDebugOverlayMaxHeight = 96;

class RecordedFrameMLGPU : public RecordedFrame {
 public:
  RecordedFrameMLGPU(MLGDevice* aDevice, MLGTexture* aTexture,
                     const TimeStamp& aTimestamp)
      : RecordedFrame(aTimestamp), mDevice(aDevice) {
    mSoftTexture =
        aDevice->CreateTexture(aTexture->GetSize(), SurfaceFormat::B8G8R8A8,
                               MLGUsage::Staging, MLGTextureFlags::None);

    aDevice->CopyTexture(mSoftTexture, IntPoint(), aTexture,
                         IntRect(IntPoint(), aTexture->GetSize()));
  }

  ~RecordedFrameMLGPU() {
    if (mIsMapped) {
      mDevice->Unmap(mSoftTexture);
    }
  }

  virtual already_AddRefed<gfx::DataSourceSurface> GetSourceSurface() override {
    if (mDataSurf) {
      return RefPtr<DataSourceSurface>(mDataSurf).forget();
    }
    MLGMappedResource map;
    if (!mDevice->Map(mSoftTexture, MLGMapType::READ, &map)) {
      return nullptr;
    }

    mIsMapped = true;
    mDataSurf = Factory::CreateWrappingDataSourceSurface(
        map.mData, map.mStride, mSoftTexture->GetSize(),
        SurfaceFormat::B8G8R8A8);
    return RefPtr<DataSourceSurface>(mDataSurf).forget();
  }

 private:
  RefPtr<MLGDevice> mDevice;
  // Software texture in VRAM.
  RefPtr<MLGTexture> mSoftTexture;
  RefPtr<DataSourceSurface> mDataSurf;
  bool mIsMapped = false;
};

LayerManagerMLGPU::LayerManagerMLGPU(widget::CompositorWidget* aWidget)
    : mWidget(aWidget),
      mDrawDiagnostics(false),
      mUsingInvalidation(false),
      mCurrentFrame(nullptr),
      mDebugFrameNumber(0) {
  if (!aWidget) {
    return;
  }

#ifdef WIN32
  mDevice = DeviceManagerDx::Get()->GetMLGDevice();
#endif
  if (!mDevice || !mDevice->IsValid()) {
    gfxWarning() << "Could not acquire an MLGDevice!";
    return;
  }

  mSwapChain = mDevice->CreateSwapChainForWidget(aWidget);
  if (!mSwapChain) {
    gfxWarning() << "Could not acquire an MLGSwapChain!";
    return;
  }

  mDiagnostics = MakeUnique<Diagnostics>();
  mTextRenderer = new TextRenderer();
}

LayerManagerMLGPU::~LayerManagerMLGPU() {
  if (mTextureSourceProvider) {
    mTextureSourceProvider->Destroy();
  }
}

bool LayerManagerMLGPU::Initialize() {
  if (!mDevice || !mSwapChain) {
    return false;
  }

  mTextureSourceProvider = new TextureSourceProviderMLGPU(this, mDevice);
  return true;
}

void LayerManagerMLGPU::Destroy() {
  if (IsDestroyed()) {
    return;
  }

  LayerManager::Destroy();
  mProfilerScreenshotGrabber.Destroy();

  if (mDevice && mDevice->IsValid()) {
    mDevice->Flush();
  }
  if (mSwapChain) {
    mSwapChain->Destroy();
    mSwapChain = nullptr;
  }
  if (mTextureSourceProvider) {
    mTextureSourceProvider->Destroy();
    mTextureSourceProvider = nullptr;
  }
  mWidget = nullptr;
  mDevice = nullptr;
}

void LayerManagerMLGPU::ForcePresent() {
  if (!mDevice->IsValid()) {
    return;
  }

  IntSize windowSize = mWidget->GetClientSize().ToUnknownSize();
  if (mSwapChain->GetSize() != windowSize) {
    return;
  }

  mSwapChain->ForcePresent();
}

already_AddRefed<ContainerLayer> LayerManagerMLGPU::CreateContainerLayer() {
  return MakeAndAddRef<ContainerLayerMLGPU>(this);
}

already_AddRefed<ColorLayer> LayerManagerMLGPU::CreateColorLayer() {
  return MakeAndAddRef<ColorLayerMLGPU>(this);
}

already_AddRefed<RefLayer> LayerManagerMLGPU::CreateRefLayer() {
  return MakeAndAddRef<RefLayerMLGPU>(this);
}

already_AddRefed<PaintedLayer> LayerManagerMLGPU::CreatePaintedLayer() {
  return MakeAndAddRef<PaintedLayerMLGPU>(this);
}

already_AddRefed<ImageLayer> LayerManagerMLGPU::CreateImageLayer() {
  return MakeAndAddRef<ImageLayerMLGPU>(this);
}

already_AddRefed<CanvasLayer> LayerManagerMLGPU::CreateCanvasLayer() {
  return MakeAndAddRef<CanvasLayerMLGPU>(this);
}

TextureFactoryIdentifier LayerManagerMLGPU::GetTextureFactoryIdentifier() {
  TextureFactoryIdentifier ident;
  if (mDevice) {
    ident = mDevice->GetTextureFactoryIdentifier();
  }
  ident.mUsingAdvancedLayers = true;
  return ident;
}

LayersBackend LayerManagerMLGPU::GetBackendType() {
  return mDevice ? mDevice->GetLayersBackend() : LayersBackend::LAYERS_NONE;
}

void LayerManagerMLGPU::SetRoot(Layer* aLayer) { mRoot = aLayer; }

bool LayerManagerMLGPU::BeginTransaction(const nsCString& aURL) { return true; }

void LayerManagerMLGPU::BeginTransactionWithDrawTarget(
    gfx::DrawTarget* aTarget, const gfx::IntRect& aRect) {
  MOZ_ASSERT(!mTarget);

  mTarget = aTarget;
  mTargetRect = aRect;
  return;
}

// Helper class for making sure textures are unlocked.
class MOZ_STACK_CLASS AutoUnlockAllTextures final {
 public:
  explicit AutoUnlockAllTextures(MLGDevice* aDevice) : mDevice(aDevice) {}
  ~AutoUnlockAllTextures() { mDevice->UnlockAllTextures(); }

 private:
  RefPtr<MLGDevice> mDevice;
};

void LayerManagerMLGPU::EndTransaction(const TimeStamp& aTimeStamp,
                                       EndTransactionFlags aFlags) {
  AUTO_PROFILER_LABEL("LayerManager::EndTransaction", GRAPHICS);

  SetCompositionTime(aTimeStamp);

  TextureSourceProvider::AutoReadUnlockTextures unlock(mTextureSourceProvider);

  if (!mRoot || (aFlags & END_NO_IMMEDIATE_REDRAW) || !mWidget) {
    return;
  }

  if (!mDevice->IsValid()) {
    // Waiting device reset handling.
    return;
  }

  mCompositionStartTime = TimeStamp::Now();

  IntSize windowSize = mWidget->GetClientSize().ToUnknownSize();
  if (windowSize.IsEmpty()) {
    return;
  }

  // Resize the window if needed.
  if (mSwapChain->GetSize() != windowSize) {
    // Note: all references to the backbuffer must be cleared.
    mDevice->SetRenderTarget(nullptr);
    if (!mSwapChain->ResizeBuffers(windowSize)) {
      gfxCriticalNote << "Could not resize the swapchain ("
                      << hexa(windowSize.width) << ","
                      << hexa(windowSize.height) << ")";
      return;
    }
  }

  // Don't draw the diagnostic overlay if we want to snapshot the output.
  mDrawDiagnostics = gfxPrefs::LayersDrawFPS() && !mTarget;
  mUsingInvalidation = gfxPrefs::AdvancedLayersUseInvalidation();
  mDebugFrameNumber++;

  AL_LOG("--- Compositing frame %d ---\n", mDebugFrameNumber);

  // Compute transforms - and the changed area, if enabled.
  mRoot->ComputeEffectiveTransforms(Matrix4x4());
  ComputeInvalidRegion();

  // Build and execute draw commands, and present.
  if (PreRender()) {
    Composite();
    PostRender();
  }

  mTextureSourceProvider->FlushPendingNotifyNotUsed();

  // Finish composition.
  mLastCompositionEndTime = TimeStamp::Now();
}

void LayerManagerMLGPU::Composite() {
  if (gfxEnv::SkipComposition()) {
    return;
  }

  AUTO_PROFILER_LABEL("LayerManagerMLGPU::Composite", GRAPHICS);

  // Don't composite if we're minimized/hidden, or if there is nothing to draw.
  if (mWidget->IsHidden()) {
    return;
  }

  // Make sure the diagnostic area gets invalidated. We do this now, rather than
  // earlier, so we don't accidentally cause extra composites.
  Maybe<IntRect> diagnosticRect;
  if (mDrawDiagnostics) {
    diagnosticRect =
        Some(IntRect(kDebugOverlayX, kDebugOverlayY, kDebugOverlayMaxWidth,
                     kDebugOverlayMaxHeight));
  }

  AL_LOG("Computed invalid region: %s\n", Stringify(mInvalidRegion).c_str());

  // Now that we have the final invalid region, give it to the swap chain which
  // will tell us if we still need to render.
  if (!mSwapChain->ApplyNewInvalidRegion(std::move(mInvalidRegion),
                                         diagnosticRect)) {
    mProfilerScreenshotGrabber.NotifyEmptyFrame();
    return;
  }

  AutoUnlockAllTextures autoUnlock(mDevice);

  mDevice->BeginFrame();

  RenderLayers();

  if (mDrawDiagnostics) {
    DrawDebugOverlay();
  }

  if (mTarget) {
    mSwapChain->CopyBackbuffer(mTarget, mTargetRect);
    mTarget = nullptr;
    mTargetRect = IntRect();
  }
  mSwapChain->Present();

  // We call this here to mimic the behavior in LayerManagerComposite, as to
  // not change what Talos measures. That is, we do not record an empty frame
  // as a frame, since we short-circuit at the top of this function.
  RecordFrame();

  mDevice->EndFrame();

  // Free the old cloned property tree, then clone a new one. Note that we do
  // this after compositing, since layer preparation actually mutates the layer
  // tree (for example, ImageHost::mLastFrameID). We want the property tree to
  // pick up these changes. Similarly, we are careful to not mutate the tree
  // in any way that we *don't* want LayerProperties to catch, lest we cause
  // extra invalidation.
  //
  // Note that the old Compositor performs occlusion culling directly on the
  // shadow visible region, and does this *before* cloning layer tree
  // properties. Advanced Layers keeps the occlusion region separate and
  // performs invalidation against the clean layer tree.
  mClonedLayerTreeProperties = nullptr;
  mClonedLayerTreeProperties = LayerProperties::CloneFrom(mRoot);

  PayloadPresented();

  mPayload.Clear();
}

void LayerManagerMLGPU::RenderLayers() {
  AUTO_PROFILER_LABEL("LayerManagerMLGPU::RenderLayers", GRAPHICS);

  // Traverse the layer tree and assign each layer to a render target.
  FrameBuilder builder(this, mSwapChain);
  mCurrentFrame = &builder;

  if (!builder.Build()) {
    return;
  }

  if (mDrawDiagnostics) {
    mDiagnostics->RecordPrepareTime(
        (TimeStamp::Now() - mCompositionStartTime).ToMilliseconds());
  }

  // Make sure we acquire/release the sync object.
  if (!mDevice->Synchronize()) {
    // Catastrophic failure - probably a device reset.
    return;
  }

  TimeStamp start = TimeStamp::Now();

  // Upload shared buffers.
  mDevice->FinishSharedBufferUse();

  // Prepare the pipeline.
  if (mDrawDiagnostics) {
    IntSize size = mSwapChain->GetBackBufferInvalidRegion().GetBounds().Size();
    uint32_t numPixels = size.width * size.height;
    mDevice->StartDiagnostics(numPixels);
  }

  // Execute all render passes.
  builder.Render();

  mProfilerScreenshotGrabber.MaybeGrabScreenshot(
      mDevice, builder.GetWidgetRT()->GetTexture());

  if (mCompositionRecorder) {
    bool hasContentPaint = false;
    for (CompositionPayload& payload : mPayload) {
      if (payload.mType == CompositionPayloadType::eContentPaint) {
        hasContentPaint = true;
        break;
      }
    }

    if (hasContentPaint) {
      RefPtr<RecordedFrame> frame = new RecordedFrameMLGPU(
          mDevice, builder.GetWidgetRT()->GetTexture(), TimeStamp::Now());
      mCompositionRecorder->RecordFrame(frame);
    }
  }
  mCurrentFrame = nullptr;

  if (mDrawDiagnostics) {
    mDiagnostics->RecordCompositeTime(
        (TimeStamp::Now() - start).ToMilliseconds());
    mDevice->EndDiagnostics();
  }
}

void LayerManagerMLGPU::DrawDebugOverlay() {
  IntSize windowSize = mSwapChain->GetSize();

  GPUStats stats;
  mDevice->GetDiagnostics(&stats);
  stats.mScreenPixels = windowSize.width * windowSize.height;

  std::string text = mDiagnostics->GetFrameOverlayString(stats);
  RefPtr<TextureSource> texture =
      mTextRenderer->RenderText(mTextureSourceProvider, text, 30, 600,
                                TextRenderer::FontType::FixedWidth);
  if (!texture) {
    return;
  }

  if (mUsingInvalidation &&
      (texture->GetSize().width > kDebugOverlayMaxWidth ||
       texture->GetSize().height > kDebugOverlayMaxHeight)) {
    gfxCriticalNote << "Diagnostic overlay exceeds invalidation area: %s"
                    << Stringify(texture->GetSize()).c_str();
  }

  struct DebugRect {
    Rect bounds;
    Rect texCoords;
  };

  if (!mDiagnosticVertices) {
    DebugRect rect;
    rect.bounds =
        Rect(Point(kDebugOverlayX, kDebugOverlayY), Size(texture->GetSize()));
    rect.texCoords = Rect(0.0, 0.0, 1.0, 1.0);

    VertexStagingBuffer instances;
    if (!instances.AppendItem(rect)) {
      return;
    }

    mDiagnosticVertices = mDevice->CreateBuffer(
        MLGBufferType::Vertex, instances.NumItems() * instances.SizeOfItem(),
        MLGUsage::Immutable, instances.GetBufferStart());
    if (!mDiagnosticVertices) {
      return;
    }
  }

  // Note: we rely on the world transform being correctly left bound by the
  // outermost render view.
  mDevice->SetScissorRect(Nothing());
  mDevice->SetDepthTestMode(MLGDepthTestMode::Disabled);
  mDevice->SetTopology(MLGPrimitiveTopology::UnitQuad);
  mDevice->SetVertexShader(VertexShaderID::DiagnosticText);
  mDevice->SetVertexBuffer(1, mDiagnosticVertices, sizeof(DebugRect));
  mDevice->SetPixelShader(PixelShaderID::DiagnosticText);
  mDevice->SetBlendState(MLGBlendState::Over);
  mDevice->SetPSTexture(0, texture);
  mDevice->SetSamplerMode(0, SamplerMode::Point);
  mDevice->DrawInstanced(4, 1, 0, 0);
}

void LayerManagerMLGPU::ComputeInvalidRegion() {
  // If invalidation is disabled, throw away cloned properties and redraw the
  // whole target area.
  if (!mUsingInvalidation) {
    mInvalidRegion = mTarget ? mTargetRect : mRenderBounds;
    mNextFrameInvalidRegion.SetEmpty();
    return;
  }

  nsIntRegion changed;
  if (mClonedLayerTreeProperties) {
    if (!mClonedLayerTreeProperties->ComputeDifferences(mRoot, changed,
                                                        nullptr)) {
      changed = mRenderBounds;
    }
  } else {
    changed = mRenderBounds;
  }

  // We compute the change region, but if we're painting to a target, we save
  // it for the next frame instead.
  if (mTarget) {
    mInvalidRegion = mTargetRect;
    mNextFrameInvalidRegion.OrWith(changed);
  } else {
    mInvalidRegion = std::move(mNextFrameInvalidRegion);
    mInvalidRegion.OrWith(changed);
  }
}

void LayerManagerMLGPU::AddInvalidRegion(const nsIntRegion& aRegion) {
  mNextFrameInvalidRegion.OrWith(aRegion);
}

TextureSourceProvider* LayerManagerMLGPU::GetTextureSourceProvider() const {
  return mTextureSourceProvider;
}

bool LayerManagerMLGPU::IsCompositingToScreen() const { return !mTarget; }

bool LayerManagerMLGPU::AreComponentAlphaLayersEnabled() {
  return LayerManager::AreComponentAlphaLayersEnabled();
}

bool LayerManagerMLGPU::BlendingRequiresIntermediateSurface() { return true; }

void LayerManagerMLGPU::EndTransaction(DrawPaintedLayerCallback aCallback,
                                       void* aCallbackData,
                                       EndTransactionFlags aFlags) {
  MOZ_CRASH("GFX: Use EndTransaction(aTimeStamp)");
}

void LayerManagerMLGPU::ClearCachedResources(Layer* aSubtree) {
  Layer* root = aSubtree ? aSubtree : mRoot.get();
  if (!root) {
    return;
  }

  ForEachNode<ForwardIterator>(root, [](Layer* aLayer) {
    LayerMLGPU* layer = aLayer->AsHostLayer()->AsLayerMLGPU();
    if (!layer) {
      return;
    }
    layer->ClearCachedResources();
  });
}

void LayerManagerMLGPU::NotifyShadowTreeTransaction() {
  if (gfxPrefs::LayersDrawFPS()) {
    mDiagnostics->AddTxnFrame();
  }
}

void LayerManagerMLGPU::UpdateRenderBounds(const gfx::IntRect& aRect) {
  mRenderBounds = aRect;
}

bool LayerManagerMLGPU::PreRender() {
  AUTO_PROFILER_LABEL("LayerManagerMLGPU::PreRender", GRAPHICS);

  widget::WidgetRenderingContext context;
  if (!mWidget->PreRender(&context)) {
    return false;
  }
  mWidgetContext = Some(context);
  return true;
}

void LayerManagerMLGPU::PostRender() {
  mWidget->PostRender(mWidgetContext.ptr());
  mProfilerScreenshotGrabber.MaybeProcessQueue();
  mWidgetContext = Nothing();
}

}  // namespace layers
}  // namespace mozilla
