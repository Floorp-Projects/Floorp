/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* rendering object for the HTML <video> element */

#include "nsHTMLParts.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsGkAtoms.h"

#include "nsVideoFrame.h"
#include "nsHTMLVideoElement.h"
#include "nsIDOMHTMLVideoElement.h"
#include "nsDisplayList.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "nsPresContext.h"
#include "nsTransform2D.h"
#include "nsContentCreatorFunctions.h"
#include "nsBoxLayoutState.h"
#include "nsBoxFrame.h"
#include "nsImageFrame.h"
#include "nsIImageLoadingContent.h"
#include "nsDisplayList.h"
#include "nsCSSRendering.h"
#include "nsContentUtils.h"

#ifdef ACCESSIBILITY
#include "nsIServiceManager.h"
#include "nsAccessibilityService.h"
#endif

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
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

nsresult
nsVideoFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  nsNodeInfoManager *nodeInfoManager = GetContent()->GetCurrentDoc()->NodeInfoManager();
  nsCOMPtr<nsINodeInfo> nodeInfo;
  if (HasVideoElement()) {
    // Create an anonymous image element as a child to hold the poster
    // image. We may not have a poster image now, but one could be added
    // before we load, or on a subsequent load.
    nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::img,
                                            nsnull,
                                            kNameSpaceID_XHTML,
                                            nsIDOMNode::ELEMENT_NODE);
    NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
    Element* element = NS_NewHTMLImageElement(nodeInfo.forget());
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

    imgContent->ForceImageState(PR_TRUE, 0);
    // And now have it update its internal state
    element->UpdateState(false);

    nsresult res = UpdatePosterSource(PR_FALSE);
    NS_ENSURE_SUCCESS(res,res);

    if (!aElements.AppendElement(mPosterImage))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  // Set up "videocontrols" XUL element which will be XBL-bound to the
  // actual controls.
  nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::videocontrols,
                                          nsnull,
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
                                       PRUint32 aFliter)
{
  aElements.MaybeAppendElement(mPosterImage);
  aElements.MaybeAppendElement(mVideoControls);
}

void
nsVideoFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsContentUtils::DestroyAnonymousContent(&mVideoControls);
  nsContentUtils::DestroyAnonymousContent(&mPosterImage);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

bool
nsVideoFrame::IsLeaf() const
{
  return PR_TRUE;
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
    NS_MIN(aRect.Width()/aRatio.width, aRect.Height()/aRatio.height);
  gfxSize scaledRatio(scale*aRatio.width, scale*aRatio.height);
  gfxPoint topLeft((aRect.Width() - scaledRatio.width)/2,
                   (aRect.Height() - scaledRatio.height)/2);
  return gfxRect(aRect.TopLeft() + topLeft, scaledRatio);
}

already_AddRefed<Layer>
nsVideoFrame::BuildLayer(nsDisplayListBuilder* aBuilder,
                         LayerManager* aManager,
                         nsDisplayItem* aItem)
{
  nsRect area = GetContentRect() - GetPosition() + aItem->ToReferenceFrame();
  nsHTMLVideoElement* element = static_cast<nsHTMLVideoElement*>(GetContent());
  nsIntSize videoSize = element->GetVideoSize(nsIntSize(0, 0));
  if (videoSize.width <= 0 || videoSize.height <= 0 || area.IsEmpty())
    return nsnull;

  nsRefPtr<ImageContainer> container = element->GetImageContainer();
  // If we have a container with a different layer manager, try to hand
  // off the container to the new one.
  if (container && container->Manager() != aManager) {
    // we don't care about the return type here -- if the set didn't take, it'll
    // be handled when we next check the manager
    container->SetLayerManager(aManager);
  }

  // If we have a container of the correct type already, we don't need
  // to do anything here. Otherwise we need to set up a temporary
  // ImageContainer, capture the video data and store it in the temp
  // container. For now we also check if the manager is equal since not all
  // image containers are manager independent yet.
  if (!container || 
      (container->Manager() && container->Manager() != aManager) ||
      container->GetBackendType() != aManager->GetBackendType())
  {
    nsRefPtr<ImageContainer> tmpContainer = aManager->CreateImageContainer();
    if (!tmpContainer)
      return nsnull;
    
    // We get a reference to the video data as a cairo surface.
    CairoImage::Data cairoData;
    nsRefPtr<gfxASurface> imageSurface;
    if (container) {
      // Get video from the existing container. It was created for a
      // different layer manager, so we do fallback through cairo.
      imageSurface = container->GetCurrentAsSurface(&cairoData.mSize);
      if (!imageSurface) {
        // we couldn't do fallback, so we've got nothing to do here
        return nsnull;
      }
      cairoData.mSurface = imageSurface;
    } else {
      // We're probably printing.
      cairoData.mSurface = element->GetPrintSurface();
      if (!cairoData.mSurface)
        return nsnull;
      cairoData.mSize = gfxIntSize(videoSize.width, videoSize.height);
    }

    // Now create a CairoImage to display the surface.
    Image::Format cairoFormat = Image::CAIRO_SURFACE;
    nsRefPtr<Image> image = tmpContainer->CreateImage(&cairoFormat, 1);
    if (!image)
      return nsnull;

    NS_ASSERTION(image->GetFormat() == cairoFormat, "Wrong format");
    static_cast<CairoImage*>(image.get())->SetData(cairoData);
    tmpContainer->SetCurrentImage(image);
    container = tmpContainer.forget();
  }

  // Retrieve the size of the decoded video frame, before being scaled
  // by pixel aspect ratio.
  gfxIntSize frameSize = container->GetCurrentSize();
  if (frameSize.width == 0 || frameSize.height == 0) {
    // No image, or zero-sized image. No point creating a layer.
    return nsnull;
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
  gfxIntSize scaleHint(static_cast<PRInt32>(r.Width()),
                       static_cast<PRInt32>(r.Height()));
  container->SetScaleHint(scaleHint);

  nsRefPtr<ImageLayer> layer = static_cast<ImageLayer*>
    (aBuilder->LayerBuilder()->GetLeafLayerFor(aBuilder, aManager, aItem));
  if (!layer) {
    layer = aManager->CreateImageLayer();
    if (!layer)
      return nsnull;
  }

  layer->SetContainer(container);
  layer->SetFilter(nsLayoutUtils::GetGraphicsFilterForFrame(this));
  layer->SetContentFlags(Layer::CONTENT_OPAQUE);
  // Set a transform on the layer to draw the video in the right place
  gfxMatrix transform;
  transform.Translate(r.TopLeft());
  transform.Scale(r.Width()/frameSize.width, r.Height()/frameSize.height);
  layer->SetTransform(gfx3DMatrix::From2D(transform));
  layer->SetVisibleRegion(nsIntRect(0, 0, videoSize.width, videoSize.height));
  nsRefPtr<Layer> result = layer.forget();
  return result.forget();
}

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
    if (child->GetType() == nsGkAtoms::imageFrame) {
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
      if (ShouldDisplayPoster()) {
        kidReflowState.SetComputedWidth(aReflowState.ComputedWidth());
        kidReflowState.SetComputedHeight(aReflowState.ComputedHeight());
      } else {
        kidReflowState.SetComputedWidth(0);
        kidReflowState.SetComputedHeight(0);
      }
      ReflowChild(imageFrame, aPresContext, kidDesiredSize, kidReflowState,
                  mBorderPadding.left, mBorderPadding.top, 0, aStatus);
      FinishReflowChild(imageFrame, aPresContext,
                        &kidReflowState, kidDesiredSize,
                        mBorderPadding.left, mBorderPadding.top, 0);
    } else if (child->GetType() == nsGkAtoms::boxFrame) {
      // Reflow the video controls frame.
      nsBoxLayoutState boxState(PresContext(), aReflowState.rendContext);
      nsBoxFrame::LayoutChildAt(boxState,
                                child,
                                nsRect(mBorderPadding.left,
                                       mBorderPadding.top,
                                       aReflowState.ComputedWidth(),
                                       aReflowState.ComputedHeight()));
    }
  }
  aMetrics.SetOverflowAreasToDesiredBounds();

  FinishAndStoreOverflow(&aMetrics);

  if (mRect.width != aMetrics.width || mRect.height != aMetrics.height) {
    Invalidate(nsRect(0, 0, mRect.width, mRect.height));
  }

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

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder)
  {
    nsIFrame* f = GetUnderlyingFrame();
    return f->GetContentRect() - f->GetPosition() + ToReferenceFrame();
  }

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters)
  {
    return static_cast<nsVideoFrame*>(mFrame)->BuildLayer(aBuilder, aManager, this);
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager)
  {
    if (aManager->GetBackendType() != LayerManager::LAYERS_BASIC) {
      // For non-basic layer managers we can assume that compositing
      // layers is very cheap, and since ImageLayers don't require
      // additional memory of the video frames we have to have anyway,
      // we can't save much by making layers inactive. Also, for many
      // accelerated layer managers calling
      // imageContainer->GetCurrentAsSurface can be very expensive. So
      // just always be active for these managers.
      return LAYER_ACTIVE;
    }
    nsHTMLMediaElement* elem =
      static_cast<nsHTMLMediaElement*>(mFrame->GetContent());
    return elem->IsPotentiallyPlaying() ? LAYER_ACTIVE : LAYER_INACTIVE;
  }
};

NS_IMETHODIMP
nsVideoFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                               const nsRect&           aDirtyRect,
                               const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return NS_OK;

  DO_GLOBAL_REFLOW_COUNT_DSP("nsVideoFrame");

  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  nsDisplayList replacedContent;

  if (HasVideoElement() && !ShouldDisplayPoster()) {
    rv = replacedContent.AppendNewToTop(
      new (aBuilder) nsDisplayVideo(aBuilder, this));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Add child frames to display list. We expect up to two children, an image
  // frame for the poster, and the box frame for the video controls.
  for (nsIFrame *child = mFrames.FirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->GetType() == nsGkAtoms::imageFrame && ShouldDisplayPoster()) {
      rv = child->BuildDisplayListForStackingContext(aBuilder,
                                                     aDirtyRect - child->GetOffsetTo(this),
                                                     &replacedContent);
      NS_ENSURE_SUCCESS(rv,rv);
    } else if (child->GetType() == nsGkAtoms::boxFrame) {
      rv = child->BuildDisplayListForStackingContext(aBuilder,
                                                     aDirtyRect - child->GetOffsetTo(this),
                                                     &replacedContent);
      NS_ENSURE_SUCCESS(rv,rv);
    }
  }

  WrapReplacedContentForBorderRadius(aBuilder, &replacedContent, aLists);

  return NS_OK;
}

nsIAtom*
nsVideoFrame::GetType() const
{
  return nsGkAtoms::HTMLVideoFrame;
}

#ifdef ACCESSIBILITY
already_AddRefed<nsAccessible>
nsVideoFrame::CreateAccessible()
{
  nsAccessibilityService* accService = nsIPresShell::AccService();
  return accService ?
    accService->CreateHTMLMediaAccessible(mContent, PresContext()->PresShell()) :
    nsnull;
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
                                     bool aShrinkWrap)
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
  return GetVideoIntrinsicSize(nsnull);
}

bool nsVideoFrame::ShouldDisplayPoster()
{
  if (!HasVideoElement())
    return PR_FALSE;

  nsHTMLVideoElement* element = static_cast<nsHTMLVideoElement*>(GetContent());
  if (element->GetPlayedOrSeeked() && HasVideoData())
    return PR_FALSE;

  nsCOMPtr<nsIImageLoadingContent> imgContent = do_QueryInterface(mPosterImage);
  NS_ENSURE_TRUE(imgContent, PR_FALSE);

  nsCOMPtr<imgIRequest> request;
  nsresult res = imgContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                                        getter_AddRefs(request));
  if (NS_FAILED(res) || !request) {
    return PR_FALSE;
  }

  PRUint32 status = 0;
  res = request->GetImageStatus(&status);
  if (NS_FAILED(res) || (status & imgIRequest::STATUS_ERROR))
    return PR_FALSE;

  return PR_TRUE;
}

nsSize
nsVideoFrame::GetVideoIntrinsicSize(nsRenderingContext *aRenderingContext)
{
  // Defaulting size to 300x150 if no size given.
  nsIntSize size(300, 150);

  if (ShouldDisplayPoster()) {
    // Use the poster image frame's size.
    nsIFrame *child = mFrames.FirstChild();
    if (child && child->GetType() == nsGkAtoms::imageFrame) {
      nsImageFrame* imageFrame = static_cast<nsImageFrame*>(child);
      nsSize imgsize;
      if (NS_SUCCEEDED(imageFrame->GetIntrinsicImageSize(imgsize))) {
        return imgsize;
      }
    }
  }

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

  nsHTMLVideoElement* element = static_cast<nsHTMLVideoElement*>(GetContent());
  size = element->GetVideoSize(size);

  return nsSize(nsPresContext::CSSPixelsToAppUnits(size.width),
                nsPresContext::CSSPixelsToAppUnits(size.height));
}

nsresult
nsVideoFrame::UpdatePosterSource(bool aNotify)
{
  NS_ASSERTION(HasVideoElement(), "Only call this on <video> elements.");
  nsHTMLVideoElement* element = static_cast<nsHTMLVideoElement*>(GetContent());

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
nsVideoFrame::AttributeChanged(PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aModType)
{
  if (aAttribute == nsGkAtoms::poster && HasVideoElement()) {
    nsresult res = UpdatePosterSource(PR_TRUE);
    NS_ENSURE_SUCCESS(res,res);
  }
  return nsContainerFrame::AttributeChanged(aNameSpaceID,
                                            aAttribute,
                                            aModType);
}

bool nsVideoFrame::HasVideoElement() {
  nsCOMPtr<nsIDOMHTMLVideoElement> videoDomElement = do_QueryInterface(mContent);
  return videoDomElement != nsnull;
}

bool nsVideoFrame::HasVideoData()
{
  if (!HasVideoElement())
    return PR_FALSE;
  nsHTMLVideoElement* element = static_cast<nsHTMLVideoElement*>(GetContent());
  nsIntSize size = element->GetVideoSize(nsIntSize(0,0));
  return size != nsIntSize(0,0);
}
