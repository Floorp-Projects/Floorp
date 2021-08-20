/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for the HTML <canvas> element */

#include "nsHTMLCanvasFrame.h"

#include "nsGkAtoms.h"
#include "mozilla/Assertions.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderCanvasRenderer.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "BasicLayers.h"
#include "mozilla/webgpu/CanvasContext.h"
#include "nsDisplayList.h"
#include "nsLayoutUtils.h"
#include "nsStyleUtil.h"
#include "ImageLayers.h"
#include "Layers.h"
#include "ActiveLayerTracker.h"

#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::gfx;

/* Helper for our nsIFrame::GetIntrinsicSize() impl. Takes the result of
 * "GetCanvasSize()" as a parameter, which may help avoid redundant
 * indirect calls to GetCanvasSize().
 *
 * @param aCanvasSizeInPx The canvas's size in CSS pixels, as returned
 *                        by GetCanvasSize().
 * @return The canvas's intrinsic size, as an IntrinsicSize object.
 */
static IntrinsicSize IntrinsicSizeFromCanvasSize(
    const nsIntSize& aCanvasSizeInPx) {
  return IntrinsicSize(
      nsPresContext::CSSPixelsToAppUnits(aCanvasSizeInPx.width),
      nsPresContext::CSSPixelsToAppUnits(aCanvasSizeInPx.height));
}

/* Helper for our nsIFrame::GetIntrinsicRatio() impl. Takes the result of
 * "GetCanvasSize()" as a parameter, which may help avoid redundant
 * indirect calls to GetCanvasSize().
 *
 * @return The canvas's intrinsic ratio.
 */
static AspectRatio IntrinsicRatioFromCanvasSize(
    const nsIntSize& aCanvasSizeInPx) {
  return AspectRatio::FromSize(aCanvasSizeInPx.width, aCanvasSizeInPx.height);
}

class nsDisplayCanvas final : public nsPaintedDisplayItem {
 public:
  nsDisplayCanvas(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayCanvas);
  }
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayCanvas)

  NS_DISPLAY_DECL_NAME("nsDisplayCanvas", TYPE_CANVAS)

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const override {
    *aSnap = false;
    nsHTMLCanvasFrame* f = static_cast<nsHTMLCanvasFrame*>(Frame());
    HTMLCanvasElement* canvas = HTMLCanvasElement::FromNode(f->GetContent());
    nsRegion result;
    if (canvas->GetIsOpaque()) {
      // OK, the entire region painted by the canvas is opaque. But what is
      // that region? It's the canvas's "dest rect" (controlled by the
      // object-fit/object-position CSS properties), clipped to the container's
      // content box (which is what GetBounds() returns). So, we grab those
      // rects and intersect them.
      nsRect constraintRect = GetBounds(aBuilder, aSnap);

      // Need intrinsic size & ratio, for ComputeObjectDestRect:
      nsIntSize canvasSize = f->GetCanvasSize();
      IntrinsicSize intrinsicSize = IntrinsicSizeFromCanvasSize(canvasSize);
      AspectRatio intrinsicRatio = IntrinsicRatioFromCanvasSize(canvasSize);

      const nsRect destRect = nsLayoutUtils::ComputeObjectDestRect(
          constraintRect, intrinsicSize, intrinsicRatio, f->StylePosition());
      return nsRegion(destRect.Intersect(constraintRect));
    }
    return result;
  }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override {
    *aSnap = true;
    nsHTMLCanvasFrame* f = static_cast<nsHTMLCanvasFrame*>(Frame());
    return f->GetInnerArea() + ToReferenceFrame();
  }

  virtual already_AddRefed<Layer> BuildLayer(
      nsDisplayListBuilder* aBuilder, LayerManager* aManager,
      const ContainerLayerParameters& aContainerParameters) override {
    return static_cast<nsHTMLCanvasFrame*>(mFrame)->BuildLayer(
        aBuilder, aManager, this, aContainerParameters);
  }

  virtual bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      wr::IpcResourceUpdateQueue& aResources, const StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override {
    HTMLCanvasElement* element =
        static_cast<HTMLCanvasElement*>(mFrame->GetContent());
    element->HandlePrintCallback(mFrame->PresContext());

    switch (element->GetCurrentContextType()) {
      case CanvasContextType::Canvas2D:
      case CanvasContextType::WebGL1:
      case CanvasContextType::WebGL2: {
        bool isRecycled;
        RefPtr<WebRenderCanvasData> canvasData =
            aManager->CommandBuilder()
                .CreateOrRecycleWebRenderUserData<WebRenderCanvasData>(
                    this, &isRecycled);
        nsHTMLCanvasFrame* canvasFrame =
            static_cast<nsHTMLCanvasFrame*>(mFrame);
        if (!canvasFrame->UpdateWebRenderCanvasData(aDisplayListBuilder,
                                                    canvasData)) {
          return true;
        }
        WebRenderCanvasRendererAsync* data = canvasData->GetCanvasRenderer();
        MOZ_ASSERT(data);
        data->UpdateCompositableClient();

        // Push IFrame for async image pipeline.
        // XXX Remove this once partial display list update is supported.

        nsIntSize canvasSizeInPx = data->GetSize();
        IntrinsicSize intrinsicSize =
            IntrinsicSizeFromCanvasSize(canvasSizeInPx);
        AspectRatio intrinsicRatio =
            IntrinsicRatioFromCanvasSize(canvasSizeInPx);

        nsRect area =
            mFrame->GetContentRectRelativeToSelf() + ToReferenceFrame();
        nsRect dest = nsLayoutUtils::ComputeObjectDestRect(
            area, intrinsicSize, intrinsicRatio, mFrame->StylePosition());

        LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
            dest, mFrame->PresContext()->AppUnitsPerDevPixel());

        // We don't push a stacking context for this async image pipeline here.
        // Instead, we do it inside the iframe that hosts the image. As a
        // result, a bunch of the calculations normally done as part of that
        // stacking context need to be done manually and pushed over to the
        // parent side, where it will be done when we build the display list for
        // the iframe. That happens in WebRenderCompositableHolder.

        wr::LayoutRect r = wr::ToLayoutRect(bounds);
        aBuilder.PushIFrame(r, !BackfaceIsHidden(), data->GetPipelineId().ref(),
                            /*ignoreMissingPipelines*/ false);

        LayoutDeviceRect scBounds(LayoutDevicePoint(0, 0), bounds.Size());
        wr::ImageRendering filter = wr::ToImageRendering(
            nsLayoutUtils::GetSamplingFilterForFrame(mFrame));
        wr::MixBlendMode mixBlendMode = wr::MixBlendMode::Normal;
        aManager->WrBridge()->AddWebRenderParentCommand(
            OpUpdateAsyncImagePipeline(data->GetPipelineId().value(), scBounds,
                                       VideoInfo::Rotation::kDegree_0, filter,
                                       mixBlendMode));
        break;
      }
      case CanvasContextType::WebGPU: {
        nsHTMLCanvasFrame* canvasFrame =
            static_cast<nsHTMLCanvasFrame*>(mFrame);
        HTMLCanvasElement* canvasElement =
            static_cast<HTMLCanvasElement*>(canvasFrame->GetContent());
        webgpu::CanvasContext* canvasContext =
            canvasElement->GetWebGPUContext();

        if (!canvasContext) {
          return true;
        }

        bool isRecycled;
        RefPtr<WebRenderLocalCanvasData> canvasData =
            aManager->CommandBuilder()
                .CreateOrRecycleWebRenderUserData<WebRenderLocalCanvasData>(
                    this, &isRecycled);
        if (!canvasContext->UpdateWebRenderLocalCanvasData(canvasData)) {
          return true;
        }

        const wr::ImageDescriptor imageDesc =
            canvasContext->MakeImageDescriptor();

        wr::ImageKey imageKey;
        auto imageKeyMaybe = canvasContext->GetImageKey();
        // Check that the key exists, and its namespace matches the active
        // bridge. It will mismatch if there was a GPU reset.
        if (imageKeyMaybe &&
            aManager->WrBridge()->GetNamespace() == imageKeyMaybe->mNamespace) {
          imageKey = imageKeyMaybe.value();
        } else {
          imageKey = canvasContext->CreateImageKey(aManager);
          aResources.AddPrivateExternalImage(canvasContext->mExternalImageId,
                                             imageKey, imageDesc);
        }

        {
          nsIntSize canvasSizeInPx = canvasFrame->GetCanvasSize();
          IntrinsicSize intrinsicSize =
              IntrinsicSizeFromCanvasSize(canvasSizeInPx);
          AspectRatio intrinsicRatio =
              IntrinsicRatioFromCanvasSize(canvasSizeInPx);
          nsRect area =
              mFrame->GetContentRectRelativeToSelf() + ToReferenceFrame();
          nsRect dest = nsLayoutUtils::ComputeObjectDestRect(
              area, intrinsicSize, intrinsicRatio, mFrame->StylePosition());
          LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
              dest, mFrame->PresContext()->AppUnitsPerDevPixel());
          mozilla::wr::ImageRendering rendering = wr::ToImageRendering(
              nsLayoutUtils::GetSamplingFilterForFrame(mFrame));
          aBuilder.PushImage(wr::ToLayoutRect(bounds), wr::ToLayoutRect(bounds),
                             !BackfaceIsHidden(), rendering, imageKey);
        }

        canvasData->mDescriptor = imageDesc;
        canvasData->mImageKey = imageKey;
        canvasData->RequestFrameReadback();
        break;
      }
      case CanvasContextType::ImageBitmap: {
        nsHTMLCanvasFrame* canvasFrame =
            static_cast<nsHTMLCanvasFrame*>(mFrame);
        nsIntSize canvasSizeInPx = canvasFrame->GetCanvasSize();
        if (canvasSizeInPx.width <= 0 || canvasSizeInPx.height <= 0) {
          return true;
        }
        bool isRecycled;
        RefPtr<WebRenderCanvasData> canvasData =
            aManager->CommandBuilder()
                .CreateOrRecycleWebRenderUserData<WebRenderCanvasData>(
                    this, &isRecycled);
        if (!canvasFrame->UpdateWebRenderCanvasData(aDisplayListBuilder,
                                                    canvasData)) {
          canvasData->ClearImageContainer();
          return true;
        }

        IntrinsicSize intrinsicSize =
            IntrinsicSizeFromCanvasSize(canvasSizeInPx);
        AspectRatio intrinsicRatio =
            IntrinsicRatioFromCanvasSize(canvasSizeInPx);

        nsRect area =
            mFrame->GetContentRectRelativeToSelf() + ToReferenceFrame();
        nsRect dest = nsLayoutUtils::ComputeObjectDestRect(
            area, intrinsicSize, intrinsicRatio, mFrame->StylePosition());

        LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
            dest, mFrame->PresContext()->AppUnitsPerDevPixel());

        aManager->CommandBuilder().PushImage(
            this, canvasData->GetImageContainer(), aBuilder, aResources, aSc,
            bounds, bounds);
        break;
      }
      case CanvasContextType::NoContext:
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("unknown canvas context type");
    }
    return true;
  }

  virtual LayerState GetLayerState(
      nsDisplayListBuilder* aBuilder, LayerManager* aManager,
      const ContainerLayerParameters& aParameters) override {
    if (HTMLCanvasElement::FromNode(mFrame->GetContent())
            ->ShouldForceInactiveLayer(aManager))
      return LayerState::LAYER_INACTIVE;

    // If compositing is cheap, just do that
    if (aManager->IsCompositingCheap() ||
        ActiveLayerTracker::IsContentActive(mFrame))
      return mozilla::LayerState::LAYER_ACTIVE;

    return LayerState::LAYER_INACTIVE;
  }

  // FirstContentfulPaint is supposed to ignore "white" canvases.  We use
  // MaybeModified (if GetContext() was called on the canvas) as a standin for
  // "white"
  virtual bool IsContentful() const override {
    nsHTMLCanvasFrame* f = static_cast<nsHTMLCanvasFrame*>(Frame());
    HTMLCanvasElement* canvas = HTMLCanvasElement::FromNode(f->GetContent());
    return canvas->MaybeModified();
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     gfxContext* aCtx) override {
    nsHTMLCanvasFrame* f = static_cast<nsHTMLCanvasFrame*>(Frame());
    HTMLCanvasElement* canvas = HTMLCanvasElement::FromNode(f->GetContent());

    nsRect area = f->GetContentRectRelativeToSelf() + ToReferenceFrame();
    nsIntSize canvasSizeInPx = f->GetCanvasSize();

    nsPresContext* presContext = f->PresContext();
    canvas->HandlePrintCallback(presContext);

    if (canvasSizeInPx.width <= 0 || canvasSizeInPx.height <= 0 ||
        area.IsEmpty()) {
      return;
    }

    IntrinsicSize intrinsicSize = IntrinsicSizeFromCanvasSize(canvasSizeInPx);
    AspectRatio intrinsicRatio = IntrinsicRatioFromCanvasSize(canvasSizeInPx);

    nsRect dest = nsLayoutUtils::ComputeObjectDestRect(
        area, intrinsicSize, intrinsicRatio, f->StylePosition());

    gfxRect destGFXRect = presContext->AppUnitsToGfxUnits(dest);

    // Transform the canvas into the right place
    gfxPoint p = destGFXRect.TopLeft();
    Matrix transform = Matrix::Translation(p.x, p.y);
    transform.PreScale(destGFXRect.Width() / canvasSizeInPx.width,
                       destGFXRect.Height() / canvasSizeInPx.height);
    gfxContextMatrixAutoSaveRestore saveMatrix(aCtx);

    aCtx->SetMatrix(
        gfxUtils::SnapTransformTranslation(aCtx->CurrentMatrix(), nullptr));

    if (RefPtr<layers::Image> image = canvas->GetAsImage()) {
      RefPtr<gfx::SourceSurface> surface = image->GetAsSourceSurface();
      if (!surface || !surface->IsValid()) {
        return;
      }
      gfx::IntSize size = surface->GetSize();

      transform = gfxUtils::SnapTransform(
          transform, gfxRect(0, 0, size.width, size.height), nullptr);
      aCtx->Multiply(transform);

      aCtx->GetDrawTarget()->FillRect(
          Rect(0, 0, size.width, size.height),
          SurfacePattern(surface, ExtendMode::CLAMP, Matrix(),
                         nsLayoutUtils::GetSamplingFilterForFrame(f)));
      return;
    }

    RefPtr<CanvasRenderer> renderer = new CanvasRenderer();
    if (!canvas->InitializeCanvasRenderer(aBuilder, renderer)) {
      return;
    }
    renderer->FirePreTransactionCallback();
    const auto snapshot = renderer->BorrowSnapshot();
    if (!snapshot) return;
    const auto& surface = snapshot->mSurf;

    transform = gfxUtils::SnapTransform(
        transform, gfxRect(0, 0, canvasSizeInPx.width, canvasSizeInPx.height),
        nullptr);

    if (!renderer->YIsDown()) {
      // y-flip
      transform.PreTranslate(0.0f, canvasSizeInPx.height).PreScale(1.0f, -1.0f);
    }
    aCtx->Multiply(transform);

    aCtx->GetDrawTarget()->FillRect(
        Rect(0, 0, canvasSizeInPx.width, canvasSizeInPx.height),
        SurfacePattern(surface, ExtendMode::CLAMP, Matrix(),
                       nsLayoutUtils::GetSamplingFilterForFrame(f)));

    renderer->FireDidTransactionCallback();
    renderer->ResetDirty();
  }
};

nsIFrame* NS_NewHTMLCanvasFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell)
      nsHTMLCanvasFrame(aStyle, aPresShell->GetPresContext());
}

NS_QUERYFRAME_HEAD(nsHTMLCanvasFrame)
  NS_QUERYFRAME_ENTRY(nsHTMLCanvasFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsHTMLCanvasFrame)

void nsHTMLCanvasFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                             nsIFrame* aPrevInFlow) {
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);

  // We can fill in the canvas before the canvas frame is created, in
  // which case we never get around to marking the content as active. Therefore,
  // we mark it active here when we create the frame.
  ActiveLayerTracker::NotifyContentChange(this);
}

void nsHTMLCanvasFrame::DestroyFrom(nsIFrame* aDestroyRoot,
                                    PostDestroyData& aPostDestroyData) {
  if (IsPrimaryFrame()) {
    HTMLCanvasElement::FromNode(*mContent)->ResetPrintCallback();
  }
  nsContainerFrame::DestroyFrom(aDestroyRoot, aPostDestroyData);
}

nsHTMLCanvasFrame::~nsHTMLCanvasFrame() = default;

nsIntSize nsHTMLCanvasFrame::GetCanvasSize() const {
  nsIntSize size(0, 0);
  HTMLCanvasElement* canvas = HTMLCanvasElement::FromNodeOrNull(GetContent());
  if (canvas) {
    size = canvas->GetSize();
    MOZ_ASSERT(size.width >= 0 && size.height >= 0,
               "we should've required <canvas> width/height attrs to be "
               "unsigned (non-negative) values");
  } else {
    MOZ_ASSERT_UNREACHABLE("couldn't get canvas size");
  }

  return size;
}

/* virtual */
nscoord nsHTMLCanvasFrame::GetMinISize(gfxContext* aRenderingContext) {
  // XXX The caller doesn't account for constraints of the height,
  // min-height, and max-height properties.
  bool vertical = GetWritingMode().IsVertical();
  nscoord result;
  if (StyleDisplay()->IsContainSize()) {
    result = 0;
  } else {
    result = nsPresContext::CSSPixelsToAppUnits(
        vertical ? GetCanvasSize().height : GetCanvasSize().width);
  }
  DISPLAY_MIN_INLINE_SIZE(this, result);
  return result;
}

/* virtual */
nscoord nsHTMLCanvasFrame::GetPrefISize(gfxContext* aRenderingContext) {
  // XXX The caller doesn't account for constraints of the height,
  // min-height, and max-height properties.
  bool vertical = GetWritingMode().IsVertical();
  nscoord result;
  if (StyleDisplay()->IsContainSize()) {
    result = 0;
  } else {
    result = nsPresContext::CSSPixelsToAppUnits(
        vertical ? GetCanvasSize().height : GetCanvasSize().width);
  }
  DISPLAY_PREF_INLINE_SIZE(this, result);
  return result;
}

/* virtual */
IntrinsicSize nsHTMLCanvasFrame::GetIntrinsicSize() {
  if (StyleDisplay()->IsContainSize()) {
    return IntrinsicSize(0, 0);
  }
  return IntrinsicSizeFromCanvasSize(GetCanvasSize());
}

/* virtual */
AspectRatio nsHTMLCanvasFrame::GetIntrinsicRatio() const {
  if (StyleDisplay()->IsContainSize()) {
    return AspectRatio();
  }

  return IntrinsicRatioFromCanvasSize(GetCanvasSize());
}

/* virtual */
nsIFrame::SizeComputationResult nsHTMLCanvasFrame::ComputeSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorderPadding, const StyleSizeOverrides& aSizeOverrides,
    ComputeSizeFlags aFlags) {
  return {ComputeSizeWithIntrinsicDimensions(
              aRenderingContext, aWM, GetIntrinsicSize(), GetAspectRatio(),
              aCBSize, aMargin, aBorderPadding, aSizeOverrides, aFlags),
          AspectRatioUsage::None};
}

void nsHTMLCanvasFrame::Reflow(nsPresContext* aPresContext,
                               ReflowOutput& aMetrics,
                               const ReflowInput& aReflowInput,
                               nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsHTMLCanvasFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aMetrics, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  NS_FRAME_TRACE(
      NS_FRAME_TRACE_CALLS,
      ("enter nsHTMLCanvasFrame::Reflow: availSize=%d,%d",
       aReflowInput.AvailableWidth(), aReflowInput.AvailableHeight()));

  MOZ_ASSERT(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  WritingMode wm = aReflowInput.GetWritingMode();
  LogicalSize finalSize = aReflowInput.ComputedSize();

  // stash this away so we can compute our inner area later
  mBorderPadding = aReflowInput.ComputedLogicalBorderPadding(wm);

  finalSize.ISize(wm) += mBorderPadding.IStartEnd(wm);
  finalSize.BSize(wm) += mBorderPadding.BStartEnd(wm);

  if (GetPrevInFlow()) {
    nscoord y = GetContinuationOffset(&finalSize.ISize(wm));
    finalSize.BSize(wm) -= y + mBorderPadding.BStart(wm);
    finalSize.BSize(wm) = std::max(0, finalSize.BSize(wm));
  }

  aMetrics.SetSize(wm, finalSize);
  aMetrics.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aMetrics);

  // Reflow the single anon block child.
  nsReflowStatus childStatus;
  nsIFrame* childFrame = mFrames.FirstChild();
  WritingMode childWM = childFrame->GetWritingMode();
  LogicalSize availSize = aReflowInput.ComputedSize(childWM);
  availSize.BSize(childWM) = NS_UNCONSTRAINEDSIZE;
  NS_ASSERTION(!childFrame->GetNextSibling(), "HTML canvas should have 1 kid");
  ReflowOutput childDesiredSize(aReflowInput.GetWritingMode());
  ReflowInput childReflowInput(aPresContext, aReflowInput, childFrame,
                               availSize);
  ReflowChild(childFrame, aPresContext, childDesiredSize, childReflowInput, 0,
              0, ReflowChildFlags::Default, childStatus, nullptr);
  FinishReflowChild(childFrame, aPresContext, childDesiredSize,
                    &childReflowInput, 0, 0, ReflowChildFlags::Default);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit nsHTMLCanvasFrame::Reflow: size=%d,%d",
                  aMetrics.ISize(wm), aMetrics.BSize(wm)));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aMetrics);
}

// FIXME taken from nsImageFrame, but then had splittable frame stuff
// removed.  That needs to be fixed.
// XXXdholbert As in nsImageFrame, this function's clients should probably
// just be calling GetContentRectRelativeToSelf().
nsRect nsHTMLCanvasFrame::GetInnerArea() const {
  nsMargin bp = mBorderPadding.GetPhysicalMargin(GetWritingMode());
  nsRect r;
  r.x = bp.left;
  r.y = bp.top;
  r.width = mRect.width - bp.left - bp.right;
  r.height = mRect.height - bp.top - bp.bottom;
  return r;
}

already_AddRefed<Layer> nsHTMLCanvasFrame::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    nsDisplayItem* aItem,
    const ContainerLayerParameters& aContainerParameters) {
  nsRect area = GetContentRectRelativeToSelf() + aItem->ToReferenceFrame();
  HTMLCanvasElement* element = static_cast<HTMLCanvasElement*>(GetContent());
  nsIntSize canvasSizeInPx = GetCanvasSize();

  nsPresContext* presContext = PresContext();
  element->HandlePrintCallback(presContext);

  if (canvasSizeInPx.width <= 0 || canvasSizeInPx.height <= 0 || area.IsEmpty())
    return nullptr;

  Layer* oldLayer =
      aManager->GetLayerBuilder()
          ? aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, aItem)
          : nullptr;
  RefPtr<Layer> layer = element->GetCanvasLayer(aBuilder, oldLayer, aManager);
  if (!layer) return nullptr;

  IntrinsicSize intrinsicSize = IntrinsicSizeFromCanvasSize(canvasSizeInPx);
  AspectRatio intrinsicRatio = IntrinsicRatioFromCanvasSize(canvasSizeInPx);

  nsRect dest = nsLayoutUtils::ComputeObjectDestRect(
      area, intrinsicSize, intrinsicRatio, StylePosition());

  gfxRect destGFXRect = presContext->AppUnitsToGfxUnits(dest);

  // Transform the canvas into the right place
  gfxPoint p = destGFXRect.TopLeft() + aContainerParameters.mOffset;
  Matrix transform = Matrix::Translation(p.x, p.y);
  transform.PreScale(destGFXRect.Width() / canvasSizeInPx.width,
                     destGFXRect.Height() / canvasSizeInPx.height);
  layer->SetBaseTransform(gfx::Matrix4x4::From2D(transform));
  if (layer->GetType() == layers::Layer::TYPE_CANVAS) {
    RefPtr<CanvasLayer> canvasLayer = static_cast<CanvasLayer*>(layer.get());
    canvasLayer->SetSamplingFilter(
        nsLayoutUtils::GetSamplingFilterForFrame(this));
    nsIntRect bounds;
    bounds.SetRect(0, 0, canvasSizeInPx.width, canvasSizeInPx.height);
    canvasLayer->SetBounds(bounds);
  } else if (layer->GetType() == layers::Layer::TYPE_IMAGE) {
    RefPtr<ImageLayer> imageLayer = static_cast<ImageLayer*>(layer.get());
    imageLayer->SetSamplingFilter(
        nsLayoutUtils::GetSamplingFilterForFrame(this));
  }

  return layer.forget();
}

bool nsHTMLCanvasFrame::UpdateWebRenderCanvasData(
    nsDisplayListBuilder* aBuilder, WebRenderCanvasData* aCanvasData) {
  HTMLCanvasElement* element = static_cast<HTMLCanvasElement*>(GetContent());
  return element->UpdateWebRenderCanvasData(aBuilder, aCanvasData);
}

void nsHTMLCanvasFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayListSet& aLists) {
  if (!IsVisibleForPainting()) return;

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  uint32_t clipFlags =
      nsStyleUtil::ObjectPropsMightCauseOverflow(StylePosition())
          ? 0
          : DisplayListClipState::ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT;

  DisplayListClipState::AutoClipContainingBlockDescendantsToContentBox clip(
      aBuilder, this, clipFlags);

  aLists.Content()->AppendNewToTop<nsDisplayCanvas>(aBuilder, this);

  DisplaySelectionOverlay(aBuilder, aLists.Content(),
                          nsISelectionDisplay::DISPLAY_IMAGES);
}

// get the offset into the content area of the image where aImg starts if it is
// a continuation. from nsImageFrame
nscoord nsHTMLCanvasFrame::GetContinuationOffset(nscoord* aWidth) const {
  nscoord offset = 0;
  if (aWidth) {
    *aWidth = 0;
  }

  if (GetPrevInFlow()) {
    for (nsIFrame* prevInFlow = GetPrevInFlow(); prevInFlow;
         prevInFlow = prevInFlow->GetPrevInFlow()) {
      nsRect rect = prevInFlow->GetRect();
      if (aWidth) {
        *aWidth = rect.width;
      }
      offset += rect.height;
    }
    offset -= mBorderPadding.GetPhysicalMargin(GetWritingMode()).top;
    offset = std::max(0, offset);
  }
  return offset;
}

void nsHTMLCanvasFrame::AppendDirectlyOwnedAnonBoxes(
    nsTArray<OwnedAnonBox>& aResult) {
  MOZ_ASSERT(mFrames.FirstChild(), "Must have our canvas content anon box");
  MOZ_ASSERT(!mFrames.FirstChild()->GetNextSibling(),
             "Must only have our canvas content anon box");
  aResult.AppendElement(OwnedAnonBox(mFrames.FirstChild()));
}

#ifdef ACCESSIBILITY
a11y::AccType nsHTMLCanvasFrame::AccessibleType() {
  return a11y::eHTMLCanvasType;
}
#endif

#ifdef DEBUG_FRAME_DUMP
nsresult nsHTMLCanvasFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"HTMLCanvas"_ns, aResult);
}
#endif
