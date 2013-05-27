/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for the HTML <video> element */

#include "nsHTMLParts.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsGkAtoms.h"

#include "nsVideoFrame.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "nsIDOMHTMLVideoElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsDisplayList.h"
#include "nsGenericHTMLElement.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "nsPresContext.h"
#include "nsTransform2D.h"
#include "nsContentCreatorFunctions.h"
#include "nsBoxLayoutState.h"
#include "nsBoxFrame.h"
#include "nsImageFrame.h"
#include "nsIImageLoadingContent.h"
#include "nsCSSRendering.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "mozilla/layers/ShadowLayers.h"
#include "ImageContainer.h"
#include "ImageLayers.h"
#include "nsContentList.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::dom;

nsIFrame*
NS_NewHTMLVideoFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsVideoFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsVideoFrame)

nsVideoFrame::nsVideoFrame(nsStyleContext* aContext) :
  nsContainerFrame(aContext)
{
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
  nsNodeInfoManager *nodeInfoManager = GetContent()->GetCurrentDoc()->NodeInfoManager();
  nsCOMPtr<nsINodeInfo> nodeInfo;
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

    // Push a null JSContext on the stack so that code that runs
    // within the below code doesn't think it's being called by
    // JS. See bug 604262.
    nsCxPusher pusher;
    pusher.PushNull();

    // Set the nsImageLoadingContent::ImageState() to 0. This means that the
    // image will always report its state as 0, so it will never be reframed
    // to show frames for loading or the broken image icon. This is important,
    // as the image is native anonymous, and so can't be reframed (currently).
    nsCOMPtr<nsIImageLoadingContent> imgContent = do_QueryInterface(mPosterImage);
    NS_ENSURE_TRUE(imgContent, NS_ERROR_FAILURE);

    imgContent->ForceImageState(true, 0);
    // And now have it update its internal state
    element->UpdateState(false);

    nsresult res = UpdatePosterSource(false);
    NS_ENSURE_SUCCESS(res,res);

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
nsVideoFrame::AppendAnonymousContentTo(nsBaseContentList& aElements,
                                       uint32_t aFliter)
{
  aElements.MaybeAppendElement(mPosterImage);
  aElements.MaybeAppendElement(mVideoControls);
  aElements.MaybeAppendElement(mCaptionDiv);
}

void
nsVideoFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsContentUtils::DestroyAnonymousContent(&mCaptionDiv);
  nsContentUtils::DestroyAnonymousContent(&mVideoControls);
  nsContentUtils::DestroyAnonymousContent(&mPosterImage);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

bool
nsVideoFrame::IsLeaf() const
{
  return true;
}

// Return the largest rectangle that fits in aRect and has the
// same aspect ratio as aRatio, centered at the center of aRect
static gfxRect
CorrectForAspectRatio(const gfxRect& aRect, const nsIntSize& aRatio)
{
  NS_ASSERTION(aRatio.width > 0 && aRatio.height > 0 && !aRect.IsEmpty(),
               "Nothing to draw");
  // Choose scale factor that scales aRatio to just fit into aRect
  gfxFloat scale =
    std::min(aRect.Width()/aRatio.width, aRect.Height()/aRatio.height);
  gfxSize scaledRatio(scale*aRatio.width, scale*aRatio.height);
  gfxPoint topLeft((aRect.Width() - scaledRatio.width)/2,
                   (aRect.Height() - scaledRatio.height)/2);
  return gfxRect(aRect.TopLeft() + topLeft, scaledRatio);
}

already_AddRefed<Layer>
nsVideoFrame::BuildLayer(nsDisplayListBuilder* aBuilder,
                         LayerManager* aManager,
                         nsDisplayItem* aItem,
                         const ContainerParameters& aContainerParameters)
{
  nsRect area = GetContentRect() - GetPosition() + aItem->ToReferenceFrame();
  HTMLVideoElement* element = static_cast<HTMLVideoElement*>(GetContent());
  nsIntSize videoSize;
  if (NS_FAILED(element->GetVideoSize(&videoSize)) || area.IsEmpty()) {
    return nullptr;
  }

  nsRefPtr<ImageContainer> container = element->GetImageContainer();
  if (!container)
    return nullptr;
  
  // Retrieve the size of the decoded video frame, before being scaled
  // by pixel aspect ratio.
  gfxIntSize frameSize = container->GetCurrentSize();
  if (frameSize.width == 0 || frameSize.height == 0) {
    // No image, or zero-sized image. No point creating a layer.
    return nullptr;
  }

  // Compute the rectangle in which to paint the video. We need to use
  // the largest rectangle that fills our content-box and has the
  // correct aspect ratio.
  nsPresContext* presContext = PresContext();
  gfxRect r = gfxRect(presContext->AppUnitsToGfxUnits(area.x),
                      presContext->AppUnitsToGfxUnits(area.y),
                      presContext->AppUnitsToGfxUnits(area.width),
                      presContext->AppUnitsToGfxUnits(area.height));
  r = CorrectForAspectRatio(r, videoSize);
  r.Round();
  if (r.IsEmpty()) {
    return nullptr;
  }
  gfxIntSize scaleHint(static_cast<int32_t>(r.Width()),
                       static_cast<int32_t>(r.Height()));
  container->SetScaleHint(scaleHint);

  nsRefPtr<ImageLayer> layer = static_cast<ImageLayer*>
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, aItem));
  if (!layer) {
    layer = aManager->CreateImageLayer();
    if (!layer)
      return nullptr;
  }

  layer->SetContainer(container);
  layer->SetFilter(nsLayoutUtils::GetGraphicsFilterForFrame(this));
  layer->SetContentFlags(Layer::CONTENT_OPAQUE);
  // Set a transform on the layer to draw the video in the right place
  gfxMatrix transform;
  transform.Translate(r.TopLeft() + aContainerParameters.mOffset);
  transform.Scale(r.Width()/frameSize.width, r.Height()/frameSize.height);
  layer->SetBaseTransform(gfx3DMatrix::From2D(transform));
  layer->SetVisibleRegion(nsIntRect(0, 0, frameSize.width, frameSize.height));
  nsRefPtr<Layer> result = layer.forget();
  return result.forget();
}

class DispatchResizeToControls : public nsRunnable
{
public:
  DispatchResizeToControls(nsIContent* aContent)
    : mContent(aContent) {}
  NS_IMETHOD Run() {
    nsContentUtils::DispatchTrustedEvent(mContent->OwnerDoc(), mContent,
                                         NS_LITERAL_STRING("resizevideocontrols"),
                                         false, false);
    return NS_OK;
  }
  nsCOMPtr<nsIContent> mContent;
};

NS_IMETHODIMP
nsVideoFrame::Reflow(nsPresContext*           aPresContext,
                     nsHTMLReflowMetrics&     aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsVideoFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("enter nsVideoFrame::Reflow: availSize=%d,%d",
                  aReflowState.availableWidth, aReflowState.availableHeight));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  aStatus = NS_FRAME_COMPLETE;

  aMetrics.width = aReflowState.ComputedWidth();
  aMetrics.height = aReflowState.ComputedHeight();

  // stash this away so we can compute our inner area later
  mBorderPadding   = aReflowState.mComputedBorderPadding;

  aMetrics.width += mBorderPadding.left + mBorderPadding.right;
  aMetrics.height += mBorderPadding.top + mBorderPadding.bottom;

  // Reflow the child frames. We may have up to two, an image frame
  // which is the poster, and a box frame, which is the video controls.
  for (nsIFrame *child = mFrames.FirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->GetContent() == mPosterImage) {
      // Reflow the poster frame.
      nsImageFrame* imageFrame = static_cast<nsImageFrame*>(child);
      nsHTMLReflowMetrics kidDesiredSize;
      nsSize availableSize = nsSize(aReflowState.availableWidth,
                                    aReflowState.availableHeight);
      nsHTMLReflowState kidReflowState(aPresContext,
                                       aReflowState,
                                       imageFrame,
                                       availableSize,
                                       aMetrics.width,
                                       aMetrics.height);

      uint32_t posterHeight, posterWidth;
      nsSize scaledPosterSize(0, 0);
      nsSize computedArea(aReflowState.ComputedWidth(), aReflowState.ComputedHeight());
      nsPoint posterTopLeft(0, 0);

      nsCOMPtr<nsIDOMHTMLImageElement> posterImage = do_QueryInterface(mPosterImage);
      NS_ENSURE_TRUE(posterImage, NS_ERROR_FAILURE);
      posterImage->GetNaturalHeight(&posterHeight);
      posterImage->GetNaturalWidth(&posterWidth);

      if (ShouldDisplayPoster() && posterHeight && posterWidth) {
        gfxFloat scale =
          std::min(static_cast<float>(computedArea.width)/nsPresContext::CSSPixelsToAppUnits(static_cast<float>(posterWidth)),
                 static_cast<float>(computedArea.height)/nsPresContext::CSSPixelsToAppUnits(static_cast<float>(posterHeight)));
        gfxSize scaledRatio = gfxSize(scale*posterWidth, scale*posterHeight);
        scaledPosterSize.width = nsPresContext::CSSPixelsToAppUnits(static_cast<float>(scaledRatio.width));
        scaledPosterSize.height = nsPresContext::CSSPixelsToAppUnits(static_cast<int32_t>(scaledRatio.height));
      }
      kidReflowState.SetComputedWidth(scaledPosterSize.width);
      kidReflowState.SetComputedHeight(scaledPosterSize.height);
      posterTopLeft.x = ((computedArea.width - scaledPosterSize.width) / 2) + mBorderPadding.left;
      posterTopLeft.y = ((computedArea.height - scaledPosterSize.height) / 2) + mBorderPadding.top;

      ReflowChild(imageFrame, aPresContext, kidDesiredSize, kidReflowState,
                        posterTopLeft.x, posterTopLeft.y, 0, aStatus);
      FinishReflowChild(imageFrame, aPresContext, &kidReflowState, kidDesiredSize,
                        posterTopLeft.x, posterTopLeft.y, 0);
    } else if (child->GetContent() == mVideoControls) {
      // Reflow the video controls frame.
      nsBoxLayoutState boxState(PresContext(), aReflowState.rendContext);
      nsSize size = child->GetSize();
      nsBoxFrame::LayoutChildAt(boxState,
                                child,
                                nsRect(mBorderPadding.left,
                                       mBorderPadding.top,
                                       aReflowState.ComputedWidth(),
                                       aReflowState.ComputedHeight()));
      if (child->GetSize() != size) {
        nsRefPtr<nsRunnable> event = new DispatchResizeToControls(child->GetContent());
        nsContentUtils::AddScriptRunner(event);
      }
    } else if (child->GetContent() == mCaptionDiv) {
      // Reflow to caption div
      nsHTMLReflowMetrics kidDesiredSize;
      nsSize availableSize = nsSize(aReflowState.availableWidth,
                                    aReflowState.availableHeight);
      nsHTMLReflowState kidReflowState(aPresContext,
                                       aReflowState,
                                       child,
                                       availableSize,
                                       aMetrics.width,
                                       aMetrics.height);
      nsSize size(aReflowState.ComputedWidth(), aReflowState.ComputedHeight());
      size.width -= kidReflowState.mComputedBorderPadding.LeftRight();
      size.height -= kidReflowState.mComputedBorderPadding.TopBottom();

      kidReflowState.SetComputedWidth(std::max(size.width, 0));
      kidReflowState.SetComputedHeight(std::max(size.height, 0));

      ReflowChild(child, aPresContext, kidDesiredSize, kidReflowState,
                  mBorderPadding.left, mBorderPadding.top, 0, aStatus);
      FinishReflowChild(child, aPresContext,
                        &kidReflowState, kidDesiredSize,
                        mBorderPadding.left, mBorderPadding.top, 0);
    }
  }
  aMetrics.SetOverflowAreasToDesiredBounds();

  FinishAndStoreOverflow(&aMetrics);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("exit nsVideoFrame::Reflow: size=%d,%d",
                  aMetrics.width, aMetrics.height));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);

  return NS_OK;
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

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
  {
    *aSnap = true;
    nsIFrame* f = Frame();
    return f->GetContentRect() - f->GetPosition() + ToReferenceFrame();
  }

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters)
  {
    return static_cast<nsVideoFrame*>(mFrame)->BuildLayer(aBuilder, aManager, this, aContainerParameters);
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const FrameLayerBuilder::ContainerParameters& aParameters)
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

  DisplayListClipState::AutoClipContainingBlockDescendantsToContentBox
    clip(aBuilder, this, DisplayListClipState::ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT);

  if (HasVideoElement() && !ShouldDisplayPoster()) {
    aLists.Content()->AppendNewToTop(
      new (aBuilder) nsDisplayVideo(aBuilder, this));
  }

  // Add child frames to display list. We expect various children,
  // but only want to draw mPosterImage conditionally. Others we
  // always add to the display list.
  for (nsIFrame *child = mFrames.FirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->GetContent() != mPosterImage || ShouldDisplayPoster()) {
      child->BuildDisplayListForStackingContext(aBuilder,
                                                aDirtyRect - child->GetOffsetTo(this),
                                                aLists.Content());
    } else if (child->GetType() == nsGkAtoms::boxFrame) {
      child->BuildDisplayListForStackingContext(aBuilder,
                                                aDirtyRect - child->GetOffsetTo(this),
                                                aLists.Content());
    }
  }
}

nsIAtom*
nsVideoFrame::GetType() const
{
  return nsGkAtoms::HTMLVideoFrame;
}

#ifdef ACCESSIBILITY
a11y::AccType
nsVideoFrame::AccessibleType()
{
  return a11y::eHTMLMediaType;
}
#endif

#ifdef DEBUG
NS_IMETHODIMP
nsVideoFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("HTMLVideo"), aResult);
}
#endif

nsSize nsVideoFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                                     nsSize aCBSize,
                                     nscoord aAvailableWidth,
                                     nsSize aMargin,
                                     nsSize aBorder,
                                     nsSize aPadding,
                                     uint32_t aFlags)
{
  nsSize size = GetVideoIntrinsicSize(aRenderingContext);

  IntrinsicSize intrinsicSize;
  intrinsicSize.width.SetCoordValue(size.width);
  intrinsicSize.height.SetCoordValue(size.height);

  nsSize& intrinsicRatio = size; // won't actually be used

  return nsLayoutUtils::ComputeSizeWithIntrinsicDimensions(aRenderingContext,
                                                           this,
                                                           intrinsicSize,
                                                           intrinsicRatio,
                                                           aCBSize,
                                                           aMargin,
                                                           aBorder,
                                                           aPadding);
}

nscoord nsVideoFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result = GetVideoIntrinsicSize(aRenderingContext).width;
  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

nscoord nsVideoFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result = GetVideoIntrinsicSize(aRenderingContext).width;
  DISPLAY_PREF_WIDTH(this, result);
  return result;
}

nsSize nsVideoFrame::GetIntrinsicRatio()
{
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
nsVideoFrame::GetVideoIntrinsicSize(nsRenderingContext *aRenderingContext)
{
  // Defaulting size to 300x150 if no size given.
  nsIntSize size(300, 150);
  
  if (!HasVideoElement()) {
    if (!aRenderingContext || !mFrames.FirstChild()) {
      // We just want our intrinsic ratio, but audio elements need no
      // intrinsic ratio, so just return "no ratio". Also, if there's
      // no controls frame, we prefer to be zero-sized.
      return nsSize(0, 0);
    }

    // Ask the controls frame what its preferred height is
    nsBoxLayoutState boxState(PresContext(), aRenderingContext, 0);
    nscoord prefHeight = mFrames.LastChild()->GetPrefSize(boxState).height;
    return nsSize(nsPresContext::CSSPixelsToAppUnits(size.width), prefHeight);
  }

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

nsresult
nsVideoFrame::UpdatePosterSource(bool aNotify)
{
  NS_ASSERTION(HasVideoElement(), "Only call this on <video> elements.");
  HTMLVideoElement* element = static_cast<HTMLVideoElement*>(GetContent());

  nsAutoString posterStr;
  element->GetPoster(posterStr);
  nsresult res = mPosterImage->SetAttr(kNameSpaceID_None,
                                       nsGkAtoms::src,
                                       posterStr,
                                       aNotify);
  NS_ENSURE_SUCCESS(res,res);
  return NS_OK;
}

NS_IMETHODIMP
nsVideoFrame::AttributeChanged(int32_t aNameSpaceID,
                               nsIAtom* aAttribute,
                               int32_t aModType)
{
  if (aAttribute == nsGkAtoms::poster && HasVideoElement()) {
    nsresult res = UpdatePosterSource(true);
    NS_ENSURE_SUCCESS(res,res);
  }
  return nsContainerFrame::AttributeChanged(aNameSpaceID,
                                            aAttribute,
                                            aModType);
}

bool nsVideoFrame::HasVideoElement() {
  nsCOMPtr<nsIDOMHTMLVideoElement> videoDomElement = do_QueryInterface(mContent);
  return videoDomElement != nullptr;
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
