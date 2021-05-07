/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for the HTML <video> element */

#include "nsVideoFrame.h"

#include "nsCOMPtr.h"
#include "nsGkAtoms.h"

#include "mozilla/PresShell.h"
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "nsDisplayList.h"
#include "nsGenericHTMLElement.h"
#include "nsPresContext.h"
#include "nsContentCreatorFunctions.h"
#include "nsBoxLayoutState.h"
#include "nsBoxFrame.h"
#include "nsIContentInlines.h"
#include "nsImageFrame.h"
#include "nsIImageLoadingContent.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "ImageContainer.h"
#include "ImageLayers.h"
#include "nsStyleUtil.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::gfx;

nsIFrame* NS_NewHTMLVideoFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsVideoFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsVideoFrame)

// A matrix to obtain a correct-rotated video frame.
static Matrix ComputeRotationMatrix(gfxFloat aRotatedWidth,
                                    gfxFloat aRotatedHeight,
                                    VideoInfo::Rotation aDegrees) {
  Matrix shiftVideoCenterToOrigin;
  if (aDegrees == VideoInfo::Rotation::kDegree_90 ||
      aDegrees == VideoInfo::Rotation::kDegree_270) {
    shiftVideoCenterToOrigin =
        Matrix::Translation(-aRotatedHeight / 2.0, -aRotatedWidth / 2.0);
  } else {
    shiftVideoCenterToOrigin =
        Matrix::Translation(-aRotatedWidth / 2.0, -aRotatedHeight / 2.0);
  }

  Matrix rotation = Matrix::Rotation(gfx::Float(aDegrees / 180.0 * M_PI));
  Matrix shiftLeftTopToOrigin =
      Matrix::Translation(aRotatedWidth / 2.0, aRotatedHeight / 2.0);
  return shiftVideoCenterToOrigin * rotation * shiftLeftTopToOrigin;
}

static void SwapScaleWidthHeightForRotation(IntSize& aSize,
                                            VideoInfo::Rotation aDegrees) {
  if (aDegrees == VideoInfo::Rotation::kDegree_90 ||
      aDegrees == VideoInfo::Rotation::kDegree_270) {
    int32_t tmpWidth = aSize.width;
    aSize.width = aSize.height;
    aSize.height = tmpWidth;
  }
}

nsVideoFrame::nsVideoFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
    : nsContainerFrame(aStyle, aPresContext, kClassID) {
  EnableVisibilityTracking();
}

nsVideoFrame::~nsVideoFrame() = default;

NS_QUERYFRAME_HEAD(nsVideoFrame)
  NS_QUERYFRAME_ENTRY(nsVideoFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

nsresult nsVideoFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  nsNodeInfoManager* nodeInfoManager =
      GetContent()->GetComposedDoc()->NodeInfoManager();
  RefPtr<NodeInfo> nodeInfo;

  if (HasVideoElement()) {
    // Create an anonymous image element as a child to hold the poster
    // image. We may not have a poster image now, but one could be added
    // before we load, or on a subsequent load.
    nodeInfo = nodeInfoManager->GetNodeInfo(
        nsGkAtoms::img, nullptr, kNameSpaceID_XHTML, nsINode::ELEMENT_NODE);
    NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
    mPosterImage = NS_NewHTMLImageElement(nodeInfo.forget());
    NS_ENSURE_TRUE(mPosterImage, NS_ERROR_OUT_OF_MEMORY);

    // Set the nsImageLoadingContent::ImageState() to 0. This means that the
    // image will always report its state as 0, so it will never be reframed
    // to show frames for loading or the broken image icon. This is important,
    // as the image is native anonymous, and so can't be reframed (currently).
    HTMLImageElement* imgContent = HTMLImageElement::FromNode(mPosterImage);
    NS_ENSURE_TRUE(imgContent, NS_ERROR_FAILURE);

    imgContent->ForceImageState(true, 0);
    // And now have it update its internal state
    mPosterImage->UpdateState(false);

    UpdatePosterSource(false);

    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    aElements.AppendElement(mPosterImage);

    // Set up the caption overlay div for showing any TextTrack data
    nodeInfo = nodeInfoManager->GetNodeInfo(
        nsGkAtoms::div, nullptr, kNameSpaceID_XHTML, nsINode::ELEMENT_NODE);
    NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
    mCaptionDiv = NS_NewHTMLDivElement(nodeInfo.forget());
    NS_ENSURE_TRUE(mCaptionDiv, NS_ERROR_OUT_OF_MEMORY);
    nsGenericHTMLElement* div =
        static_cast<nsGenericHTMLElement*>(mCaptionDiv.get());
    div->SetClassName(u"caption-box"_ns);

    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    aElements.AppendElement(mCaptionDiv);
    UpdateTextTrack();
  }

  return NS_OK;
}

void nsVideoFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                            uint32_t aFliter) {
  if (mPosterImage) {
    aElements.AppendElement(mPosterImage);
  }

  if (mCaptionDiv) {
    aElements.AppendElement(mCaptionDiv);
  }
}

nsIContent* nsVideoFrame::GetVideoControls() const {
  if (!mContent->GetShadowRoot()) {
    return nullptr;
  }

  // The video controls <div> is the only child of the UA Widget Shadow Root
  // if it is present. It is only lazily inserted into the DOM when
  // the controls attribute is set.
  MOZ_ASSERT(mContent->GetShadowRoot()->IsUAWidget());
  MOZ_ASSERT(1 >= mContent->GetShadowRoot()->GetChildCount());
  return mContent->GetShadowRoot()->GetFirstChild();
}

void nsVideoFrame::DestroyFrom(nsIFrame* aDestructRoot,
                               PostDestroyData& aPostDestroyData) {
  aPostDestroyData.AddAnonymousContent(mCaptionDiv.forget());
  aPostDestroyData.AddAnonymousContent(mPosterImage.forget());
  nsContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

already_AddRefed<Layer> nsVideoFrame::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    nsDisplayItem* aItem,
    const ContainerLayerParameters& aContainerParameters) {
  nsRect area = GetContentRectRelativeToSelf() + aItem->ToReferenceFrame();
  HTMLVideoElement* element = static_cast<HTMLVideoElement*>(GetContent());

  Maybe<CSSIntSize> videoSizeInPx = element->GetVideoSize();
  if (videoSizeInPx.isNothing() || area.IsEmpty()) {
    return nullptr;
  }

  RefPtr<ImageContainer> container = element->GetImageContainer();
  if (!container) return nullptr;

  // Retrieve the size of the decoded video frame, before being scaled
  // by pixel aspect ratio.
  mozilla::gfx::IntSize frameSize = container->GetCurrentSize();
  if (frameSize.width == 0 || frameSize.height == 0) {
    // No image, or zero-sized image. No point creating a layer.
    return nullptr;
  }

  const auto aspectRatio = AspectRatio::FromSize(*videoSizeInPx);
  const IntrinsicSize intrinsicSize(CSSPixel::ToAppUnits(*videoSizeInPx));
  nsRect dest = nsLayoutUtils::ComputeObjectDestRect(
      area, intrinsicSize, aspectRatio, StylePosition());

  gfxRect destGFXRect = PresContext()->AppUnitsToGfxUnits(dest);
  destGFXRect.Round();
  if (destGFXRect.IsEmpty()) {
    return nullptr;
  }

  VideoInfo::Rotation rotationDeg = element->RotationDegrees();
  IntSize scaleHint(static_cast<int32_t>(destGFXRect.Width()),
                    static_cast<int32_t>(destGFXRect.Height()));
  // scaleHint is set regardless of rotation, so swap w/h if needed.
  SwapScaleWidthHeightForRotation(scaleHint, rotationDeg);
  container->SetScaleHint(scaleHint);

  RefPtr<ImageLayer> layer = static_cast<ImageLayer*>(
      aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, aItem));
  if (!layer) {
    layer = aManager->CreateImageLayer();
    if (!layer) return nullptr;
  }

  layer->SetContainer(container);
  layer->SetSamplingFilter(nsLayoutUtils::GetSamplingFilterForFrame(this));
  // Set a transform on the layer to draw the video in the right place
  gfxPoint p = destGFXRect.TopLeft() + aContainerParameters.mOffset;

  Matrix preTransform = ComputeRotationMatrix(
      destGFXRect.Width(), destGFXRect.Height(), rotationDeg);

  Matrix transform = preTransform * Matrix::Translation(p.x, p.y);

  layer->SetBaseTransform(gfx::Matrix4x4::From2D(transform));
  layer->SetScaleToSize(scaleHint, ScaleMode::STRETCH);

  uint32_t flags = element->HasAlpha() ? 0 : Layer::CONTENT_OPAQUE;
  layer->SetContentFlags(flags);

  RefPtr<Layer> result = std::move(layer);
  return result.forget();
}

class DispatchResizeEvent : public Runnable {
 public:
  explicit DispatchResizeEvent(nsIContent* aContent, const nsString& aName)
      : mozilla::Runnable("DispatchResizeEvent"),
        mContent(aContent),
        mName(aName) {}
  NS_IMETHOD Run() override {
    nsContentUtils::DispatchTrustedEvent(mContent->OwnerDoc(), mContent, mName,
                                         CanBubble::eNo, Cancelable::eNo);
    return NS_OK;
  }
  nsCOMPtr<nsIContent> mContent;
  nsString mName;
};

void nsVideoFrame::Reflow(nsPresContext* aPresContext, ReflowOutput& aMetrics,
                          const ReflowInput& aReflowInput,
                          nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsVideoFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aMetrics, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  NS_FRAME_TRACE(
      NS_FRAME_TRACE_CALLS,
      ("enter nsVideoFrame::Reflow: availSize=%d,%d",
       aReflowInput.AvailableWidth(), aReflowInput.AvailableHeight()));

  MOZ_ASSERT(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  const WritingMode myWM = aReflowInput.GetWritingMode();
  nscoord contentBoxBSize = aReflowInput.ComputedBSize();
  const auto logicalBP = aReflowInput.ComputedLogicalBorderPadding(myWM);
  const nscoord borderBoxISize =
      aReflowInput.ComputedISize() + logicalBP.IStartEnd(myWM);
  const bool isBSizeShrinkWrapping = (contentBoxBSize == NS_UNCONSTRAINEDSIZE);

  nscoord borderBoxBSize;
  if (!isBSizeShrinkWrapping) {
    borderBoxBSize = contentBoxBSize + logicalBP.BStartEnd(myWM);
  }

  nsMargin borderPadding = aReflowInput.ComputedPhysicalBorderPadding();

  nsIContent* videoControlsDiv = GetVideoControls();

  // Reflow the child frames. We may have up to three: an image
  // frame (for the poster image), a container frame for the controls,
  // and a container frame for the caption.
  for (nsIFrame* child : mFrames) {
    nsSize oldChildSize = child->GetSize();
    nsReflowStatus childStatus;

    if (child->GetContent() == mPosterImage) {
      // Reflow the poster frame.
      nsImageFrame* imageFrame = static_cast<nsImageFrame*>(child);
      ReflowOutput kidDesiredSize(aReflowInput);
      WritingMode wm = imageFrame->GetWritingMode();
      LogicalSize availableSize = aReflowInput.AvailableSize(wm);
      availableSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;

      LogicalSize cbSize = aMetrics.Size(aMetrics.GetWritingMode())
                               .ConvertTo(wm, aMetrics.GetWritingMode());
      ReflowInput kidReflowInput(aPresContext, aReflowInput, imageFrame,
                                 availableSize, Some(cbSize));

      nsRect posterRenderRect;
      if (ShouldDisplayPoster()) {
        posterRenderRect =
            nsRect(nsPoint(borderPadding.left, borderPadding.top),
                   nsSize(aReflowInput.ComputedWidth(),
                          aReflowInput.ComputedHeight()));
      }
      kidReflowInput.SetComputedWidth(posterRenderRect.width);
      kidReflowInput.SetComputedHeight(posterRenderRect.height);
      ReflowChild(imageFrame, aPresContext, kidDesiredSize, kidReflowInput,
                  posterRenderRect.x, posterRenderRect.y,
                  ReflowChildFlags::Default, childStatus);
      MOZ_ASSERT(childStatus.IsFullyComplete(),
                 "We gave our child unconstrained available block-size, "
                 "so it should be complete!");

      FinishReflowChild(imageFrame, aPresContext, kidDesiredSize,
                        &kidReflowInput, posterRenderRect.x, posterRenderRect.y,
                        ReflowChildFlags::Default);

    } else if (child->GetContent() == mCaptionDiv ||
               child->GetContent() == videoControlsDiv) {
      // Reflow the caption and control bar frames.
      WritingMode wm = child->GetWritingMode();
      LogicalSize availableSize = aReflowInput.ComputedSize(wm);
      availableSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;

      ReflowInput kidReflowInput(aPresContext, aReflowInput, child,
                                 availableSize);
      ReflowOutput kidDesiredSize(kidReflowInput);
      ReflowChild(child, aPresContext, kidDesiredSize, kidReflowInput,
                  borderPadding.left, borderPadding.top,
                  ReflowChildFlags::Default, childStatus);
      MOZ_ASSERT(childStatus.IsFullyComplete(),
                 "We gave our child unconstrained available block-size, "
                 "so it should be complete!");

      if (child->GetContent() == videoControlsDiv && isBSizeShrinkWrapping) {
        // Resolve our own BSize based on the controls' size in the
        // same axis. Unless we're size-contained, in which case we
        // have to behave as if we have an intrinsic size of 0.
        if (aReflowInput.mStyleDisplay->IsContainSize()) {
          contentBoxBSize = 0;
        } else {
          contentBoxBSize = myWM.IsOrthogonalTo(wm) ? kidDesiredSize.ISize(wm)
                                                    : kidDesiredSize.BSize(wm);
        }
      }

      FinishReflowChild(child, aPresContext, kidDesiredSize, &kidReflowInput,
                        borderPadding.left, borderPadding.top,
                        ReflowChildFlags::Default);

      if (child->GetSize() != oldChildSize) {
        const nsString name = child->GetContent() == videoControlsDiv
                                  ? u"resizevideocontrols"_ns
                                  : u"resizecaption"_ns;
        RefPtr<Runnable> event =
            new DispatchResizeEvent(child->GetContent(), name);
        nsContentUtils::AddScriptRunner(event);
      }
    } else {
      NS_ERROR("Unexpected extra child frame in nsVideoFrame; skipping");
    }
  }

  if (isBSizeShrinkWrapping) {
    if (contentBoxBSize == NS_UNCONSTRAINEDSIZE) {
      // We didn't get a BSize from our intrinsic size/ratio, nor did we
      // get one from our controls. Just use BSize of 0.
      contentBoxBSize = 0;
    }
    contentBoxBSize =
        NS_CSS_MINMAX(contentBoxBSize, aReflowInput.ComputedMinBSize(),
                      aReflowInput.ComputedMaxBSize());
    borderBoxBSize = contentBoxBSize + logicalBP.BStartEnd(myWM);
  }

  LogicalSize logicalDesiredSize(myWM, borderBoxISize, borderBoxBSize);
  aMetrics.SetSize(myWM, logicalDesiredSize);

  aMetrics.SetOverflowAreasToDesiredBounds();

  FinishAndStoreOverflow(&aMetrics);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS, ("exit nsVideoFrame::Reflow: size=%d,%d",
                                        aMetrics.Width(), aMetrics.Height()));

  MOZ_ASSERT(aStatus.IsEmpty(), "This type of frame can't be split.");
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aMetrics);
}

class nsDisplayVideo : public nsPaintedDisplayItem {
 public:
  nsDisplayVideo(nsDisplayListBuilder* aBuilder, nsVideoFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayVideo);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayVideo)
#endif

  NS_DISPLAY_DECL_NAME("Video", TYPE_VIDEO)

  virtual bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const mozilla::layers::StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override {
    nsRect area = Frame()->GetContentRectRelativeToSelf() + ToReferenceFrame();
    HTMLVideoElement* element =
        static_cast<HTMLVideoElement*>(Frame()->GetContent());

    Maybe<CSSIntSize> videoSizeInPx = element->GetVideoSize();
    if (videoSizeInPx.isNothing() || area.IsEmpty()) {
      return true;
    }

    RefPtr<ImageContainer> container = element->GetImageContainer();
    if (!container) {
      return true;
    }

    // Retrieve the size of the decoded video frame, before being scaled
    // by pixel aspect ratio.
    mozilla::gfx::IntSize frameSize = container->GetCurrentSize();
    if (frameSize.width == 0 || frameSize.height == 0) {
      // No image, or zero-sized image. Don't render.
      return true;
    }

    const auto aspectRatio = AspectRatio::FromSize(*videoSizeInPx);
    const IntrinsicSize intrinsicSize(CSSPixel::ToAppUnits(*videoSizeInPx));
    nsRect dest = nsLayoutUtils::ComputeObjectDestRect(
        area, intrinsicSize, aspectRatio, Frame()->StylePosition());

    gfxRect destGFXRect = Frame()->PresContext()->AppUnitsToGfxUnits(dest);
    destGFXRect.Round();
    if (destGFXRect.IsEmpty()) {
      return true;
    }

    container->SetRotation(element->RotationDegrees());

    // If the image container is empty, we don't want to fallback. Any other
    // failure will be due to resource constraints and fallback is unlikely to
    // help us. Hence we can ignore the return value from PushImage.
    LayoutDeviceRect rect(destGFXRect.x, destGFXRect.y, destGFXRect.width,
                          destGFXRect.height);
    aManager->CommandBuilder().PushImage(this, container, aBuilder, aResources,
                                         aSc, rect, rect);
    return true;
  }

  // For opaque videos, we will want to override GetOpaqueRegion here.
  // This is tracked by bug 1545498.

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override {
    *aSnap = true;
    nsIFrame* f = Frame();
    return f->GetContentRectRelativeToSelf() + ToReferenceFrame();
  }

  virtual already_AddRefed<Layer> BuildLayer(
      nsDisplayListBuilder* aBuilder, LayerManager* aManager,
      const ContainerLayerParameters& aContainerParameters) override {
    return static_cast<nsVideoFrame*>(mFrame)->BuildLayer(
        aBuilder, aManager, this, aContainerParameters);
  }

  virtual LayerState GetLayerState(
      nsDisplayListBuilder* aBuilder, LayerManager* aManager,
      const ContainerLayerParameters& aParameters) override {
    if (aManager->IsCompositingCheap()) {
      // Since ImageLayers don't require additional memory of the
      // video frames we have to have anyway, we can't save much by
      // making layers inactive. Also, for many accelerated layer
      // managers calling imageContainer->GetCurrentAsSurface can be
      // very expensive. So just always be active when compositing is
      // cheap (i.e. hardware accelerated).
      return LayerState::LAYER_ACTIVE;
    }
    HTMLMediaElement* elem =
        static_cast<HTMLMediaElement*>(mFrame->GetContent());
    return elem->IsPotentiallyPlaying() ? LayerState::LAYER_ACTIVE_FORCE
                                        : LayerState::LAYER_INACTIVE;
  }

  // Only report FirstContentfulPaint when the video is set
  bool IsContentful() const override {
    nsVideoFrame* f = static_cast<nsVideoFrame*>(Frame());
    HTMLVideoElement* video = HTMLVideoElement::FromNode(f->GetContent());
    return video->VideoWidth() > 0;
  }
};

void nsVideoFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                    const nsDisplayListSet& aLists) {
  if (!IsVisibleForPainting()) return;

  DO_GLOBAL_REFLOW_COUNT_DSP("nsVideoFrame");

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  const bool shouldDisplayPoster = ShouldDisplayPoster();

  // NOTE: If we're displaying a poster image (instead of video data), we can
  // trust the nsImageFrame to constrain its drawing to its content rect
  // (which happens to be the same as our content rect).
  uint32_t clipFlags;
  if (shouldDisplayPoster ||
      !nsStyleUtil::ObjectPropsMightCauseOverflow(StylePosition())) {
    clipFlags = DisplayListClipState::ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT;
  } else {
    clipFlags = 0;
  }

  DisplayListClipState::AutoClipContainingBlockDescendantsToContentBox clip(
      aBuilder, this, clipFlags);

  if (HasVideoElement() && !shouldDisplayPoster) {
    aLists.Content()->AppendNewToTop<nsDisplayVideo>(aBuilder, this);
  }

  // Add child frames to display list. We expect various children,
  // but only want to draw mPosterImage conditionally. Others we
  // always add to the display list.
  for (nsIFrame* child : mFrames) {
    if (child->GetContent() != mPosterImage || shouldDisplayPoster ||
        child->IsBoxFrame()) {
      nsDisplayListBuilder::AutoBuildingDisplayList buildingForChild(
          aBuilder, child,
          aBuilder->GetVisibleRect() - child->GetOffsetTo(this),
          aBuilder->GetDirtyRect() - child->GetOffsetTo(this));

      child->BuildDisplayListForStackingContext(aBuilder, aLists.Content());
    }
  }
}

#ifdef ACCESSIBILITY
a11y::AccType nsVideoFrame::AccessibleType() { return a11y::eHTMLMediaType; }
#endif

#ifdef DEBUG_FRAME_DUMP
nsresult nsVideoFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"HTMLVideo"_ns, aResult);
}
#endif

nsIFrame::SizeComputationResult nsVideoFrame::ComputeSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorderPadding, const StyleSizeOverrides& aSizeOverrides,
    ComputeSizeFlags aFlags) {
  if (!HasVideoElement()) {
    return nsContainerFrame::ComputeSize(
        aRenderingContext, aWM, aCBSize, aAvailableISize, aMargin,
        aBorderPadding, aSizeOverrides, aFlags);
  }

  return {ComputeSizeWithIntrinsicDimensions(
              aRenderingContext, aWM, GetIntrinsicSize(), GetAspectRatio(),
              aCBSize, aMargin, aBorderPadding, aSizeOverrides, aFlags),
          AspectRatioUsage::None};
}

nscoord nsVideoFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_MIN_INLINE_SIZE(this, result);

  nsSize size = kFallbackIntrinsicSize;
  if (HasVideoElement()) {
    size = GetVideoIntrinsicSize();
  } else {
    // We expect last and only child of audio elements to be control if
    // "controls" attribute is present.
    if (StyleDisplay()->IsContainSize() || !mFrames.LastChild()) {
      size = nsSize();
    }
  }

  result = GetWritingMode().IsVertical() ? size.height : size.width;
  return result;
}

nscoord nsVideoFrame::GetPrefISize(gfxContext* aRenderingContext) {
  // <audio> / <video> has the same min / pref ISize.
  return GetMinISize(aRenderingContext);
}

Maybe<nsSize> nsVideoFrame::PosterImageSize() const {
  // Use the poster image frame's size.
  nsIFrame* child = GetPosterImage()->GetPrimaryFrame();
  return child->GetIntrinsicSize().ToSize();
}

AspectRatio nsVideoFrame::GetIntrinsicRatio() const {
  if (!HasVideoElement()) {
    // Audio elements have no intrinsic ratio.
    return AspectRatio();
  }

  // 'contain:size' replaced elements have no intrinsic ratio.
  if (StyleDisplay()->IsContainSize()) {
    return AspectRatio();
  }

  HTMLVideoElement* element = static_cast<HTMLVideoElement*>(GetContent());
  if (Maybe<CSSIntSize> size = element->GetVideoSize()) {
    return AspectRatio::FromSize(*size);
  }

  if (ShouldDisplayPoster()) {
    if (Maybe<nsSize> imgSize = PosterImageSize()) {
      return AspectRatio::FromSize(*imgSize);
    }
  }

  return AspectRatio::FromSize(kFallbackIntrinsicSizeInPixels);
}

bool nsVideoFrame::ShouldDisplayPoster() const {
  if (!HasVideoElement()) return false;

  HTMLVideoElement* element = static_cast<HTMLVideoElement*>(GetContent());
  if (element->GetPlayedOrSeeked() && HasVideoData()) return false;

  nsCOMPtr<nsIImageLoadingContent> imgContent = do_QueryInterface(mPosterImage);
  NS_ENSURE_TRUE(imgContent, false);

  nsCOMPtr<imgIRequest> request;
  nsresult res = imgContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                                        getter_AddRefs(request));
  if (NS_FAILED(res) || !request) {
    return false;
  }

  uint32_t status = 0;
  res = request->GetImageStatus(&status);
  if (NS_FAILED(res) || (status & imgIRequest::STATUS_ERROR)) return false;

  return true;
}

nsSize nsVideoFrame::GetVideoIntrinsicSize() const {
  // 'contain:size' replaced elements have intrinsic size 0,0.
  if (StyleDisplay()->IsContainSize()) {
    return nsSize(0, 0);
  }

  HTMLVideoElement* element = static_cast<HTMLVideoElement*>(GetContent());
  if (Maybe<CSSIntSize> size = element->GetVideoSize()) {
    return CSSPixel::ToAppUnits(*size);
  }

  if (ShouldDisplayPoster()) {
    if (Maybe<nsSize> imgSize = PosterImageSize()) {
      return *imgSize;
    }
  }

  return kFallbackIntrinsicSize;
}

IntrinsicSize nsVideoFrame::GetIntrinsicSize() {
  return IntrinsicSize(GetVideoIntrinsicSize());
}

void nsVideoFrame::UpdatePosterSource(bool aNotify) {
  NS_ASSERTION(HasVideoElement(), "Only call this on <video> elements.");
  HTMLVideoElement* element = static_cast<HTMLVideoElement*>(GetContent());

  if (element->HasAttr(kNameSpaceID_None, nsGkAtoms::poster) &&
      !element->AttrValueIs(kNameSpaceID_None, nsGkAtoms::poster,
                            nsGkAtoms::_empty, eIgnoreCase)) {
    nsAutoString posterStr;
    element->GetPoster(posterStr);
    mPosterImage->SetAttr(kNameSpaceID_None, nsGkAtoms::src, posterStr,
                          aNotify);
  } else {
    mPosterImage->UnsetAttr(kNameSpaceID_None, nsGkAtoms::src, aNotify);
  }
}

nsresult nsVideoFrame::AttributeChanged(int32_t aNameSpaceID,
                                        nsAtom* aAttribute, int32_t aModType) {
  if (aAttribute == nsGkAtoms::poster && HasVideoElement()) {
    UpdatePosterSource(true);
  }
  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

void nsVideoFrame::OnVisibilityChange(
    Visibility aNewVisibility, const Maybe<OnNonvisible>& aNonvisibleAction) {
  if (HasVideoElement()) {
    static_cast<HTMLMediaElement*>(GetContent())
        ->OnVisibilityChange(aNewVisibility);
  }

  nsCOMPtr<nsIImageLoadingContent> imageLoader =
      do_QueryInterface(mPosterImage);
  if (imageLoader) {
    imageLoader->OnVisibilityChange(aNewVisibility, aNonvisibleAction);
  }

  nsContainerFrame::OnVisibilityChange(aNewVisibility, aNonvisibleAction);
}

bool nsVideoFrame::HasVideoElement() const {
  return static_cast<HTMLMediaElement*>(GetContent())->IsVideo();
}

bool nsVideoFrame::HasVideoData() const {
  if (!HasVideoElement()) {
    return false;
  }
  auto* element = static_cast<HTMLVideoElement*>(GetContent());
  return element->GetVideoSize().isSome();
}

void nsVideoFrame::UpdateTextTrack() {
  static_cast<HTMLMediaElement*>(GetContent())->NotifyCueDisplayStatesChanged();
}
