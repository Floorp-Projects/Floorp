/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for the HTML <video> element */

#include "nsVideoFrame.h"

#include "nsCOMPtr.h"
#include "nsGkAtoms.h"

#include "mozilla/dom/HTMLVideoElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsDisplayList.h"
#include "nsGenericHTMLElement.h"
#include "nsPresContext.h"
#include "nsContentCreatorFunctions.h"
#include "nsBoxLayoutState.h"
#include "nsBoxFrame.h"
#include "nsImageFrame.h"
#include "nsIImageLoadingContent.h"
#include "nsContentUtils.h"
#include "ImageContainer.h"
#include "ImageLayers.h"
#include "nsContentList.h"
#include "nsStyleUtil.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::gfx;

nsIFrame*
NS_NewHTMLVideoFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsVideoFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsVideoFrame)

// A matrix to obtain a correct-rotated video frame.
static Matrix
ComputeRotationMatrix(gfxFloat aRotatedWidth,
                      gfxFloat aRotatedHeight,
                      VideoInfo::Rotation aDegrees)
{
  Matrix shiftVideoCenterToOrigin;
  if (aDegrees == VideoInfo::Rotation::kDegree_90 ||
      aDegrees == VideoInfo::Rotation::kDegree_270) {
    shiftVideoCenterToOrigin = Matrix::Translation(-aRotatedHeight / 2.0,
                                                   -aRotatedWidth / 2.0);
  } else {
    shiftVideoCenterToOrigin = Matrix::Translation(-aRotatedWidth / 2.0,
                                                   -aRotatedHeight / 2.0);
  }

  Matrix rotation = Matrix::Rotation(gfx::Float(aDegrees / 180.0 * M_PI));
  Matrix shiftLeftTopToOrigin = Matrix::Translation(aRotatedWidth / 2.0,
                                                    aRotatedHeight / 2.0);
  return shiftVideoCenterToOrigin * rotation * shiftLeftTopToOrigin;
}

static void
SwapScaleWidthHeightForRotation(IntSize& aSize, VideoInfo::Rotation aDegrees)
{
  if (aDegrees == VideoInfo::Rotation::kDegree_90 ||
      aDegrees == VideoInfo::Rotation::kDegree_270) {
    int32_t tmpWidth = aSize.width;
    aSize.width = aSize.height;
    aSize.height = tmpWidth;
  }
}

nsVideoFrame::nsVideoFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext, kClassID)
{
  EnableVisibilityTracking();
}

nsVideoFrame::~nsVideoFrame()
{
}

NS_QUERYFRAME_HEAD(nsVideoFrame)
  NS_QUERYFRAME_ENTRY(nsVideoFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

nsresult
nsVideoFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  nsNodeInfoManager *nodeInfoManager = GetContent()->GetComposedDoc()->NodeInfoManager();
  RefPtr<NodeInfo> nodeInfo;
  Element *element;

  if (HasVideoElement()) {
    // Create an anonymous image element as a child to hold the poster
    // image. We may not have a poster image now, but one could be added
    // before we load, or on a subsequent load.
    nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::img,
                                            nullptr,
                                            kNameSpaceID_XHTML,
                                            nsIDOMNode::ELEMENT_NODE);
    NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
    element = NS_NewHTMLImageElement(nodeInfo.forget());
    mPosterImage = element;
    NS_ENSURE_TRUE(mPosterImage, NS_ERROR_OUT_OF_MEMORY);

    // Set the nsImageLoadingContent::ImageState() to 0. This means that the
    // image will always report its state as 0, so it will never be reframed
    // to show frames for loading or the broken image icon. This is important,
    // as the image is native anonymous, and so can't be reframed (currently).
    nsCOMPtr<nsIImageLoadingContent> imgContent = do_QueryInterface(mPosterImage);
    NS_ENSURE_TRUE(imgContent, NS_ERROR_FAILURE);

    imgContent->ForceImageState(true, 0);
    // And now have it update its internal state
    element->UpdateState(false);

    UpdatePosterSource(false);

    if (!aElements.AppendElement(mPosterImage))
      return NS_ERROR_OUT_OF_MEMORY;

    // Set up the caption overlay div for showing any TextTrack data
    nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::div,
                                            nullptr,
                                            kNameSpaceID_XHTML,
                                            nsIDOMNode::ELEMENT_NODE);
    NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
    mCaptionDiv = NS_NewHTMLDivElement(nodeInfo.forget());
    NS_ENSURE_TRUE(mCaptionDiv, NS_ERROR_OUT_OF_MEMORY);
    nsGenericHTMLElement* div = static_cast<nsGenericHTMLElement*>(mCaptionDiv.get());
    div->SetClassName(NS_LITERAL_STRING("caption-box"));

    if (!aElements.AppendElement(mCaptionDiv))
      return NS_ERROR_OUT_OF_MEMORY;
    UpdateTextTrack();
  }

  // Set up "videocontrols" XUL element which will be XBL-bound to the
  // actual controls.
  nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::videocontrols,
                                          nullptr,
                                          kNameSpaceID_XUL,
                                          nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  NS_TrustedNewXULElement(getter_AddRefs(mVideoControls), nodeInfo.forget());
  if (!aElements.AppendElement(mVideoControls))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

void
nsVideoFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                       uint32_t aFliter)
{
  if (mPosterImage) {
    aElements.AppendElement(mPosterImage);
  }

  if (mVideoControls) {
    aElements.AppendElement(mVideoControls);
  }

  if (mCaptionDiv) {
    aElements.AppendElement(mCaptionDiv);
  }
}

void
nsVideoFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsContentUtils::DestroyAnonymousContent(&mCaptionDiv);
  nsContentUtils::DestroyAnonymousContent(&mVideoControls);
  nsContentUtils::DestroyAnonymousContent(&mPosterImage);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

already_AddRefed<Layer>
nsVideoFrame::BuildLayer(nsDisplayListBuilder* aBuilder,
                         LayerManager* aManager,
                         nsDisplayItem* aItem,
                         const ContainerLayerParameters& aContainerParameters)
{
  nsRect area = GetContentRectRelativeToSelf() + aItem->ToReferenceFrame();
  HTMLVideoElement* element = static_cast<HTMLVideoElement*>(GetContent());

  nsIntSize videoSizeInPx;
  if (NS_FAILED(element->GetVideoSize(&videoSizeInPx)) ||
      area.IsEmpty()) {
    return nullptr;
  }

  RefPtr<ImageContainer> container = element->GetImageContainer();
  if (!container)
    return nullptr;

  // Retrieve the size of the decoded video frame, before being scaled
  // by pixel aspect ratio.
  mozilla::gfx::IntSize frameSize = container->GetCurrentSize();
  if (frameSize.width == 0 || frameSize.height == 0) {
    // No image, or zero-sized image. No point creating a layer.
    return nullptr;
  }

  // Convert video size from pixel units into app units, to get an aspect-ratio
  // (which has to be represented as a nsSize) and an IntrinsicSize that we
  // can pass to ComputeObjectRenderRect.
  nsSize aspectRatio(nsPresContext::CSSPixelsToAppUnits(videoSizeInPx.width),
                     nsPresContext::CSSPixelsToAppUnits(videoSizeInPx.height));
  IntrinsicSize intrinsicSize;
  intrinsicSize.width.SetCoordValue(aspectRatio.width);
  intrinsicSize.height.SetCoordValue(aspectRatio.height);

  nsRect dest = nsLayoutUtils::ComputeObjectDestRect(area,
                                                     intrinsicSize,
                                                     aspectRatio,
                                                     StylePosition());

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

  RefPtr<ImageLayer> layer = static_cast<ImageLayer*>
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, aItem));
  if (!layer) {
    layer = aManager->CreateImageLayer();
    if (!layer)
      return nullptr;
  }

  layer->SetContainer(container);
  layer->SetSamplingFilter(nsLayoutUtils::GetSamplingFilterForFrame(this));
  // Set a transform on the layer to draw the video in the right place
  gfxPoint p = destGFXRect.TopLeft() + aContainerParameters.mOffset;

  Matrix preTransform = ComputeRotationMatrix(destGFXRect.Width(),
                                              destGFXRect.Height(),
                                              rotationDeg);

  Matrix transform = preTransform * Matrix::Translation(p.x, p.y);

  layer->SetBaseTransform(gfx::Matrix4x4::From2D(transform));
  layer->SetScaleToSize(scaleHint, ScaleMode::STRETCH);
  RefPtr<Layer> result = layer.forget();
  return result.forget();
}

class DispatchResizeToControls : public Runnable
{
public:
  explicit DispatchResizeToControls(nsIContent* aContent)
    : mozilla::Runnable("DispatchResizeToControls")
    , mContent(aContent)
  {
  }
  NS_IMETHOD Run() override {
    nsContentUtils::DispatchTrustedEvent(mContent->OwnerDoc(), mContent,
                                         NS_LITERAL_STRING("resizevideocontrols"),
                                         false, false);
    return NS_OK;
  }
  nsCOMPtr<nsIContent> mContent;
};

void
nsVideoFrame::Reflow(nsPresContext* aPresContext,
                     ReflowOutput& aMetrics,
                     const ReflowInput& aReflowInput,
                     nsReflowStatus& aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsVideoFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aMetrics, aStatus);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsVideoFrame::Reflow: availSize=%d,%d",
                  aReflowInput.AvailableWidth(),
                  aReflowInput.AvailableHeight()));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  aStatus.Reset();

  const WritingMode myWM = aReflowInput.GetWritingMode();
  nscoord contentBoxBSize = aReflowInput.ComputedBSize();
  const nscoord borderBoxISize = aReflowInput.ComputedISize() +
    aReflowInput.ComputedLogicalBorderPadding().IStartEnd(myWM);
  const bool isBSizeShrinkWrapping = (contentBoxBSize == NS_INTRINSICSIZE);

  nscoord borderBoxBSize;
  if (!isBSizeShrinkWrapping) {
    borderBoxBSize = contentBoxBSize +
      aReflowInput.ComputedLogicalBorderPadding().BStartEnd(myWM);
  }

  nsMargin borderPadding = aReflowInput.ComputedPhysicalBorderPadding();

  // Reflow the child frames. We may have up to three: an image
  // frame (for the poster image), a container frame for the controls,
  // and a container frame for the caption.
  for (nsIFrame* child : mFrames) {
    nsSize oldChildSize = child->GetSize();

    if (child->GetContent() == mPosterImage) {
      // Reflow the poster frame.
      nsImageFrame* imageFrame = static_cast<nsImageFrame*>(child);
      ReflowOutput kidDesiredSize(aReflowInput);
      WritingMode wm = imageFrame->GetWritingMode();
      LogicalSize availableSize = aReflowInput.AvailableSize(wm);
      LogicalSize cbSize = aMetrics.Size(aMetrics.GetWritingMode()).
                             ConvertTo(wm, aMetrics.GetWritingMode());
      ReflowInput kidReflowInput(aPresContext,
                                       aReflowInput,
                                       imageFrame,
                                       availableSize,
                                       &cbSize);

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
                  posterRenderRect.x, posterRenderRect.y, 0, aStatus);
      FinishReflowChild(imageFrame, aPresContext,
                        kidDesiredSize, &kidReflowInput,
                        posterRenderRect.x, posterRenderRect.y, 0);

// Android still uses XUL media controls & hence needs this XUL-friendly
// custom reflow code. This will go away in bug 1310907.
#ifdef ANDROID
    } else if (child->GetContent() == mVideoControls) {
      // Reflow the video controls frame.
      nsBoxLayoutState boxState(PresContext(), aReflowInput.mRenderingContext);
      nsBoxFrame::LayoutChildAt(boxState,
                                child,
                                nsRect(borderPadding.left,
                                       borderPadding.top,
                                       aReflowInput.ComputedWidth(),
                                       aReflowInput.ComputedHeight()));

#endif // ANDROID
    } else if (child->GetContent() == mCaptionDiv ||
               child->GetContent() == mVideoControls) {
      // Reflow the caption and control bar frames.
      WritingMode wm = child->GetWritingMode();
      LogicalSize availableSize = aReflowInput.ComputedSize(wm);
      availableSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;

      ReflowInput kidReflowInput(aPresContext,
                                       aReflowInput,
                                       child,
                                       availableSize);
      ReflowOutput kidDesiredSize(kidReflowInput);
      ReflowChild(child, aPresContext, kidDesiredSize, kidReflowInput,
                  borderPadding.left, borderPadding.top, 0, aStatus);

      if (child->GetContent() == mVideoControls && isBSizeShrinkWrapping) {
        // Resolve our own BSize based on the controls' size in the same axis.
        contentBoxBSize = myWM.IsOrthogonalTo(wm) ?
          kidDesiredSize.ISize(wm) : kidDesiredSize.BSize(wm);
      }

      FinishReflowChild(child, aPresContext,
                        kidDesiredSize, &kidReflowInput,
                        borderPadding.left, borderPadding.top, 0);
    }

    if (child->GetContent() == mVideoControls && child->GetSize() != oldChildSize) {
      RefPtr<Runnable> event = new DispatchResizeToControls(child->GetContent());
      nsContentUtils::AddScriptRunner(event);
    }
  }

  if (isBSizeShrinkWrapping) {
    if (contentBoxBSize == NS_INTRINSICSIZE) {
      // We didn't get a BSize from our intrinsic size/ratio, nor did we
      // get one from our controls. Just use BSize of 0.
      contentBoxBSize = 0;
    }
    contentBoxBSize = NS_CSS_MINMAX(contentBoxBSize,
                                    aReflowInput.ComputedMinBSize(),
                                    aReflowInput.ComputedMaxBSize());
    borderBoxBSize = contentBoxBSize +
      aReflowInput.ComputedLogicalBorderPadding().BStartEnd(myWM);
  }

  LogicalSize logicalDesiredSize(myWM, borderBoxISize, borderBoxBSize);
  aMetrics.SetSize(myWM, logicalDesiredSize);

  aMetrics.SetOverflowAreasToDesiredBounds();

  FinishAndStoreOverflow(&aMetrics);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit nsVideoFrame::Reflow: size=%d,%d",
                  aMetrics.Width(), aMetrics.Height()));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aMetrics);
}

class nsDisplayVideo : public nsDisplayItem {
public:
  nsDisplayVideo(nsDisplayListBuilder* aBuilder, nsVideoFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  {
    MOZ_COUNT_CTOR(nsDisplayVideo);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayVideo() {
    MOZ_COUNT_DTOR(nsDisplayVideo);
  }
#endif

  NS_DISPLAY_DECL_NAME("Video", TYPE_VIDEO)

  // It would be great if we could override GetOpaqueRegion to return nonempty here,
  // but it's probably not safe to do so in general. Video frames are
  // updated asynchronously from decoder threads, and it's possible that
  // we might have an opaque video frame when GetOpaqueRegion is called, but
  // when we come to paint, the video frame is transparent or has gone
  // away completely (e.g. because of a decoder error). The problem would
  // be especially acute if we have off-main-thread rendering.

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override
  {
    *aSnap = true;
    nsIFrame* f = Frame();
    return f->GetContentRectRelativeToSelf() + ToReferenceFrame();
  }

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override
  {
    return static_cast<nsVideoFrame*>(mFrame)->BuildLayer(aBuilder, aManager, this, aContainerParameters);
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override
  {
    if (aManager->IsCompositingCheap()) {
      // Since ImageLayers don't require additional memory of the
      // video frames we have to have anyway, we can't save much by
      // making layers inactive. Also, for many accelerated layer
      // managers calling imageContainer->GetCurrentAsSurface can be
      // very expensive. So just always be active when compositing is
      // cheap (i.e. hardware accelerated).
      return LAYER_ACTIVE;
    }
    HTMLMediaElement* elem =
      static_cast<HTMLMediaElement*>(mFrame->GetContent());
    return elem->IsPotentiallyPlaying() ? LAYER_ACTIVE_FORCE : LAYER_INACTIVE;
  }
};

void
nsVideoFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                               const nsRect&           aDirtyRect,
                               const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return;

  DO_GLOBAL_REFLOW_COUNT_DSP("nsVideoFrame");

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  const bool shouldDisplayPoster = ShouldDisplayPoster();

  // NOTE: If we're displaying a poster image (instead of video data), we can
  // trust the nsImageFrame to constrain its drawing to its content rect
  // (which happens to be the same as our content rect).
  uint32_t clipFlags;
  if (shouldDisplayPoster ||
      !nsStyleUtil::ObjectPropsMightCauseOverflow(StylePosition())) {
    clipFlags =
      DisplayListClipState::ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT;
  } else {
    clipFlags = 0;
  }

  DisplayListClipState::AutoClipContainingBlockDescendantsToContentBox
    clip(aBuilder, this, clipFlags);

  if (HasVideoElement() && !shouldDisplayPoster) {
    aLists.Content()->AppendNewToTop(
      new (aBuilder) nsDisplayVideo(aBuilder, this));
  }

  // Add child frames to display list. We expect various children,
  // but only want to draw mPosterImage conditionally. Others we
  // always add to the display list.
  for (nsIFrame* child : mFrames) {
    if (child->GetContent() != mPosterImage || shouldDisplayPoster) {
      child->BuildDisplayListForStackingContext(aBuilder,
                                                aDirtyRect - child->GetOffsetTo(this),
                                                aLists.Content());
    } else if (child->IsBoxFrame()) {
      child->BuildDisplayListForStackingContext(aBuilder,
                                                aDirtyRect - child->GetOffsetTo(this),
                                                aLists.Content());
    }
  }
}

#ifdef ACCESSIBILITY
a11y::AccType
nsVideoFrame::AccessibleType()
{
  return a11y::eHTMLMediaType;
}
#endif

#ifdef DEBUG_FRAME_DUMP
nsresult
nsVideoFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("HTMLVideo"), aResult);
}
#endif

LogicalSize
nsVideoFrame::ComputeSize(gfxContext *aRenderingContext,
                          WritingMode aWM,
                          const LogicalSize& aCBSize,
                          nscoord aAvailableISize,
                          const LogicalSize& aMargin,
                          const LogicalSize& aBorder,
                          const LogicalSize& aPadding,
                          ComputeSizeFlags aFlags)
{
// When in no video scenario, it should fall back to inherited method.
// We keep old codepath here since Android still uses XUL media controls.
// This will go away in bug 1310907.
#ifndef ANDROID
  if (!HasVideoElement()) {
    return nsContainerFrame::ComputeSize(aRenderingContext,
                                         aWM,
                                         aCBSize,
                                         aAvailableISize,
                                         aMargin,
                                         aBorder,
                                         aPadding,
                                         aFlags);
  }
#endif // ANDROID

  nsSize size = GetVideoIntrinsicSize(aRenderingContext);

  IntrinsicSize intrinsicSize;
  intrinsicSize.width.SetCoordValue(size.width);
  intrinsicSize.height.SetCoordValue(size.height);

  // Only video elements have an intrinsic ratio.
  nsSize intrinsicRatio = HasVideoElement() ? size : nsSize(0, 0);

  return ComputeSizeWithIntrinsicDimensions(aRenderingContext, aWM,
                                            intrinsicSize, intrinsicRatio,
                                            aCBSize, aMargin, aBorder, aPadding,
                                            aFlags);
}

nscoord nsVideoFrame::GetMinISize(gfxContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  if (HasVideoElement()) {
    nsSize size = GetVideoIntrinsicSize(aRenderingContext);
    result = GetWritingMode().IsVertical() ? size.height : size.width;
  } else {
    // We expect last and only child of audio elements to be control if
    // "controls" attribute is present.
    nsIFrame* kid = mFrames.LastChild();
    if (kid) {
      result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                                    kid,
                                                    nsLayoutUtils::MIN_ISIZE);
    } else {
      result = 0;
    }
  }

  return result;
}

nscoord nsVideoFrame::GetPrefISize(gfxContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);

  if (HasVideoElement()) {
    nsSize size = GetVideoIntrinsicSize(aRenderingContext);
    result = GetWritingMode().IsVertical() ? size.height : size.width;
  } else {
    // We expect last and only child of audio elements to be control if
    // "controls" attribute is present.
    nsIFrame* kid = mFrames.LastChild();
    if (kid) {
      result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                                    kid,
                                                    nsLayoutUtils::PREF_ISIZE);
    } else {
      result = 0;
    }
  }

  return result;
}

nsSize nsVideoFrame::GetIntrinsicRatio()
{
  if (!HasVideoElement()) {
    // Audio elements have no intrinsic ratio.
    return nsSize(0, 0);
  }

  return GetVideoIntrinsicSize(nullptr);
}

bool nsVideoFrame::ShouldDisplayPoster()
{
  if (!HasVideoElement())
    return false;

  HTMLVideoElement* element = static_cast<HTMLVideoElement*>(GetContent());
  if (element->GetPlayedOrSeeked() && HasVideoData())
    return false;

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
  if (NS_FAILED(res) || (status & imgIRequest::STATUS_ERROR))
    return false;

  return true;
}

nsSize
nsVideoFrame::GetVideoIntrinsicSize(gfxContext *aRenderingContext)
{
  // Defaulting size to 300x150 if no size given.
  nsIntSize size(300, 150);

// All media controls have been converted to HTML except Android. Hence
// we keep this codepath for Android until removal in bug 1310907.
#ifdef ANDROID
  if (!HasVideoElement()) {
    if (!mFrames.FirstChild()) {
      return nsSize(0, 0);
    }

    // Ask the controls frame what its preferred height is
    nsBoxLayoutState boxState(PresContext(), aRenderingContext, 0);
    nscoord prefHeight = mFrames.LastChild()->GetXULPrefSize(boxState).height;
    return nsSize(nsPresContext::CSSPixelsToAppUnits(size.width), prefHeight);
  }
#endif // ANDROID

  HTMLVideoElement* element = static_cast<HTMLVideoElement*>(GetContent());
  if (NS_FAILED(element->GetVideoSize(&size)) && ShouldDisplayPoster()) {
    // Use the poster image frame's size.
    nsIFrame *child = mPosterImage->GetPrimaryFrame();
    nsImageFrame* imageFrame = do_QueryFrame(child);
    nsSize imgsize;
    if (NS_SUCCEEDED(imageFrame->GetIntrinsicImageSize(imgsize))) {
      return imgsize;
    }
  }

  return nsSize(nsPresContext::CSSPixelsToAppUnits(size.width),
                nsPresContext::CSSPixelsToAppUnits(size.height));
}

void
nsVideoFrame::UpdatePosterSource(bool aNotify)
{
  NS_ASSERTION(HasVideoElement(), "Only call this on <video> elements.");
  HTMLVideoElement* element = static_cast<HTMLVideoElement*>(GetContent());

  if (element->HasAttr(kNameSpaceID_None, nsGkAtoms::poster) &&
      !element->AttrValueIs(kNameSpaceID_None,
                            nsGkAtoms::poster,
                            nsGkAtoms::_empty,
                            eIgnoreCase)) {
    nsAutoString posterStr;
    element->GetPoster(posterStr);
    mPosterImage->SetAttr(kNameSpaceID_None,
                          nsGkAtoms::src,
                          posterStr,
                          aNotify);
  } else {
    mPosterImage->UnsetAttr(kNameSpaceID_None, nsGkAtoms::src, aNotify);
  }
}

nsresult
nsVideoFrame::AttributeChanged(int32_t aNameSpaceID,
                               nsIAtom* aAttribute,
                               int32_t aModType)
{
  if (aAttribute == nsGkAtoms::poster && HasVideoElement()) {
    UpdatePosterSource(true);
  }
  return nsContainerFrame::AttributeChanged(aNameSpaceID,
                                            aAttribute,
                                            aModType);
}

void
nsVideoFrame::OnVisibilityChange(Visibility aNewVisibility,
                                 const Maybe<OnNonvisible>& aNonvisibleAction)
{
  if (HasVideoElement()) {
    nsCOMPtr<nsIDOMHTMLMediaElement> mediaDomElement = do_QueryInterface(mContent);
    mediaDomElement->OnVisibilityChange(aNewVisibility);
  }

  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mPosterImage);
  if (imageLoader) {
    imageLoader->OnVisibilityChange(aNewVisibility,
                                    aNonvisibleAction);
  }

  nsContainerFrame::OnVisibilityChange(aNewVisibility, aNonvisibleAction);
}

bool nsVideoFrame::HasVideoElement() {
  nsCOMPtr<nsIDOMHTMLMediaElement> mediaDomElement = do_QueryInterface(mContent);
  return mediaDomElement->IsVideo();
}

bool nsVideoFrame::HasVideoData()
{
  if (!HasVideoElement())
    return false;
  HTMLVideoElement* element = static_cast<HTMLVideoElement*>(GetContent());
  nsIntSize size(0, 0);
  element->GetVideoSize(&size);
  return size != nsIntSize(0,0);
}

void nsVideoFrame::UpdateTextTrack()
{
  HTMLMediaElement* element = static_cast<HTMLMediaElement*>(GetContent());
  if (element) {
    element->NotifyCueDisplayStatesChanged();
  }
}
