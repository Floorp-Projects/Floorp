/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * rendering object for replaced elements that contain a document, such
 * as <frame>, <iframe>, and some <object>s
 */

#include "nsSubDocumentFrame.h"

#include "mozilla/layout/RenderFrameParent.h"

#include "nsCOMPtr.h"
#include "nsGenericHTMLElement.h"
#include "nsGenericHTMLFrameElement.h"
#include "nsAttrValueInlines.h"
#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsFrameSetFrame.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIScrollable.h"
#include "nsNameSpaceManager.h"
#include "nsDisplayList.h"
#include "nsIScrollableFrame.h"
#include "nsIObjectLoadingContent.h"
#include "nsLayoutUtils.h"
#include "FrameLayerBuilder.h"
#include "nsObjectFrame.h"
#include "nsContentUtils.h"
#include "nsIPermissionManager.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla;
using mozilla::layout::RenderFrameParent;

static nsIDocument*
GetDocumentFromView(nsView* aView)
{
  NS_PRECONDITION(aView, "");

  nsIFrame* f = aView->GetFrame();
  nsIPresShell* ps =  f ? f->PresContext()->PresShell() : nullptr;
  return ps ? ps->GetDocument() : nullptr;
}

nsSubDocumentFrame::nsSubDocumentFrame(nsStyleContext* aContext)
  : nsLeafFrame(aContext)
  , mIsInline(false)
  , mPostedReflowCallback(false)
  , mDidCreateDoc(false)
  , mCallingShow(false)
{
}

#ifdef ACCESSIBILITY
a11y::AccType
nsSubDocumentFrame::AccessibleType()
{
  return a11y::eOuterDocType;
}
#endif

NS_QUERYFRAME_HEAD(nsSubDocumentFrame)
  NS_QUERYFRAME_ENTRY(nsSubDocumentFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsLeafFrame)

class AsyncFrameInit : public nsRunnable
{
public:
  AsyncFrameInit(nsIFrame* aFrame) : mFrame(aFrame) {}
  NS_IMETHOD Run()
  {
    if (mFrame.IsAlive()) {
      static_cast<nsSubDocumentFrame*>(mFrame.GetFrame())->ShowViewer();
    }
    return NS_OK;
  }
private:
  nsWeakFrame mFrame;
};

static void
InsertViewsInReverseOrder(nsView* aSibling, nsView* aParent);

static void
EndSwapDocShellsForViews(nsView* aView);

void
nsSubDocumentFrame::Init(nsIContent*     aContent,
                         nsIFrame*       aParent,
                         nsIFrame*       aPrevInFlow)
{
  // determine if we are a <frame> or <iframe>
  if (aContent) {
    nsCOMPtr<nsIDOMHTMLFrameElement> frameElem = do_QueryInterface(aContent);
    mIsInline = frameElem ? false : true;
  }

  nsLeafFrame::Init(aContent, aParent, aPrevInFlow);

  // We are going to create an inner view.  If we need a view for the
  // OuterFrame but we wait for the normal view creation path in
  // nsCSSFrameConstructor, then we will lose because the inner view's
  // parent will already have been set to some outer view (e.g., the
  // canvas) when it really needs to have this frame's view as its
  // parent. So, create this frame's view right away, whether we
  // really need it or not, and the inner view will get it as the
  // parent.
  if (!HasView()) {
    nsContainerFrame::CreateViewForFrame(this, true);
  }
  EnsureInnerView();

  // Set the primary frame now so that nsDocumentViewer::FindContainerView
  // called from within EndSwapDocShellsForViews below can find it if needed.
  aContent->SetPrimaryFrame(this);

  // If we have a detached subdoc's root view on our frame loader, re-insert
  // it into the view tree. This happens when we've been reframed, and
  // ensures the presentation persists across reframes. If the frame element
  // has changed documents however, we blow away the presentation.
  nsRefPtr<nsFrameLoader> frameloader = FrameLoader();
  if (frameloader) {
    nsCOMPtr<nsIDocument> oldContainerDoc;
    nsView* detachedViews =
      frameloader->GetDetachedSubdocView(getter_AddRefs(oldContainerDoc));
    if (detachedViews) {
      if (oldContainerDoc == aContent->OwnerDoc()) {
        // Restore stashed presentation.
        ::InsertViewsInReverseOrder(detachedViews, mInnerView);
        ::EndSwapDocShellsForViews(mInnerView->GetFirstChild());
      } else {
        // Presentation is for a different document, don't restore it.
        frameloader->Hide();
      }
    }
    frameloader->SetDetachedSubdocView(nullptr, nullptr);
  }

  nsContentUtils::AddScriptRunner(new AsyncFrameInit(this));
}

void
nsSubDocumentFrame::ShowViewer()
{
  if (mCallingShow) {
    return;
  }

  if (!PresContext()->IsDynamic()) {
    // We let the printing code take care of loading the document; just
    // create the inner view for it to use.
    (void) EnsureInnerView();
  } else {
    nsRefPtr<nsFrameLoader> frameloader = FrameLoader();
    if (frameloader) {
      nsIntSize margin = GetMarginAttributes();
      nsWeakFrame weakThis(this);
      mCallingShow = true;
      const nsAttrValue* attrValue =
        GetContent()->AsElement()->GetParsedAttr(nsGkAtoms::scrolling);
      int32_t scrolling =
        nsGenericHTMLFrameElement::MapScrollingAttribute(attrValue);
      bool didCreateDoc =
        frameloader->Show(margin.width, margin.height,
                          scrolling, scrolling, this);
      if (!weakThis.IsAlive()) {
        return;
      }
      mCallingShow = false;
      mDidCreateDoc = didCreateDoc;
    }
  }
}

nsIFrame*
nsSubDocumentFrame::GetSubdocumentRootFrame()
{
  if (!mInnerView)
    return nullptr;
  nsView* subdocView = mInnerView->GetFirstChild();
  return subdocView ? subdocView->GetFrame() : nullptr;
}

nsIntSize
nsSubDocumentFrame::GetSubdocumentSize()
{
  if (GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    nsRefPtr<nsFrameLoader> frameloader = FrameLoader();
    if (frameloader) {
      nsCOMPtr<nsIDocument> oldContainerDoc;
      nsView* detachedViews =
        frameloader->GetDetachedSubdocView(getter_AddRefs(oldContainerDoc));
      if (detachedViews) {
        nsSize size = detachedViews->GetBounds().Size();
        nsPresContext* presContext = detachedViews->GetFrame()->PresContext();
        return nsIntSize(presContext->AppUnitsToDevPixels(size.width),
                         presContext->AppUnitsToDevPixels(size.height));
      }
    }
    // Pick some default size for now.  Using 10x10 because that's what the
    // code used to do.
    return nsIntSize(10, 10);
  } else {
    nsSize docSizeAppUnits;
    nsPresContext* presContext = PresContext();
    nsCOMPtr<nsIDOMHTMLFrameElement> frameElem =
      do_QueryInterface(GetContent());
    if (frameElem) {
      docSizeAppUnits = GetSize();
    } else {
      docSizeAppUnits = GetContentRect().Size();
    }
    return nsIntSize(presContext->AppUnitsToDevPixels(docSizeAppUnits.width),
                     presContext->AppUnitsToDevPixels(docSizeAppUnits.height));
  }
}

bool
nsSubDocumentFrame::PassPointerEventsToChildren()
{
  // Limit use of mozpasspointerevents to documents with embedded:apps/chrome
  // permission, because this could be used by the parent document to discover
  // which parts of the subdocument are transparent to events (if subdocument
  // uses pointer-events:none on its root element, which is admittedly
  // unlikely)
  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::mozpasspointerevents)) {
      if (PresContext()->IsChrome()) {
        return true;
      }

      nsCOMPtr<nsIPermissionManager> permMgr =
        do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
      if (permMgr) {
        uint32_t permission = nsIPermissionManager::DENY_ACTION;
        permMgr->TestPermissionFromPrincipal(GetContent()->NodePrincipal(),
                                             "embed-apps", &permission);

        return permission == nsIPermissionManager::ALLOW_ACTION;
      }
  }

  return false;
}

static void
WrapBackgroundColorInOwnLayer(nsDisplayListBuilder* aBuilder,
                              nsIFrame* aFrame,
                              nsDisplayList* aList)
{
  nsDisplayList tempItems;
  nsDisplayItem* item;
  while ((item = aList->RemoveBottom()) != nullptr) {
    if (item->GetType() == nsDisplayItem::TYPE_BACKGROUND_COLOR) {
      nsDisplayList tmpList;
      tmpList.AppendToTop(item);
      item = new (aBuilder) nsDisplayOwnLayer(aBuilder, aFrame, &tmpList);
    }
    tempItems.AppendToTop(item);
  }
  aList->AppendToTop(&tempItems);
}

void
nsSubDocumentFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return;

  nsFrameLoader* frameLoader = FrameLoader();
  RenderFrameParent* rfp = nullptr;
  if (frameLoader) {
    rfp = frameLoader->GetCurrentRemoteFrame();
  }

  // If we are pointer-events:none then we don't need to HitTest background
  bool pointerEventsNone = StyleVisibility()->mPointerEvents == NS_STYLE_POINTER_EVENTS_NONE;
  if (!aBuilder->IsForEventDelivery() || !pointerEventsNone) {
    nsDisplayListCollection decorations;
    DisplayBorderBackgroundOutline(aBuilder, decorations);
    if (rfp) {
      // Wrap background colors of <iframe>s with remote subdocuments in their
      // own layer so we generate a ColorLayer. This is helpful for optimizing
      // compositing; we can skip compositing the ColorLayer when the
      // remote content is opaque.
      WrapBackgroundColorInOwnLayer(aBuilder, this, decorations.BorderBackground());
    }
    decorations.MoveTo(aLists);
  }

  bool passPointerEventsToChildren = false;
  if (aBuilder->IsForEventDelivery()) {
    passPointerEventsToChildren = PassPointerEventsToChildren();
    // If mozpasspointerevents is set, then we should allow subdocument content
    // to handle events even if we're pointer-events:none.
    if (pointerEventsNone && !passPointerEventsToChildren) {
      return;
    }
  }

  // If we're passing pointer events to children then we have to descend into
  // subdocuments no matter what, to determine which parts are transparent for
  // elementFromPoint.
  if (!mInnerView ||
      (!aBuilder->GetDescendIntoSubdocuments() && !passPointerEventsToChildren)) {
    return;
  }

  if (rfp) {
    rfp->BuildDisplayList(aBuilder, this, aDirtyRect, aLists);
    return;
  }

  nsView* subdocView = mInnerView->GetFirstChild();
  if (!subdocView)
    return;

  nsCOMPtr<nsIPresShell> presShell = nullptr;

  nsIFrame* subdocRootFrame = subdocView->GetFrame();
  if (subdocRootFrame) {
    presShell = subdocRootFrame->PresContext()->PresShell();
  }
  // If painting is suppressed in the presshell, we try to look for a better
  // presshell to use.
  if (!presShell || (presShell->IsPaintingSuppressed() &&
                     !aBuilder->IsIgnoringPaintSuppression())) {
    // During page transition mInnerView will sometimes have two children, the
    // first being the new page that may not have any frame, and the second
    // being the old page that will probably have a frame.
    nsView* nextView = subdocView->GetNextSibling();
    nsIFrame* frame = nullptr;
    if (nextView) {
      frame = nextView->GetFrame();
    }
    if (frame) {
      nsIPresShell* ps = frame->PresContext()->PresShell();
      if (!presShell || (ps && !ps->IsPaintingSuppressed())) {
        subdocView = nextView;
        subdocRootFrame = frame;
        presShell = ps;
      }
    }
    if (!presShell) {
      // If we don't have a frame we use this roundabout way to get the pres shell.
      if (!mFrameLoader)
        return;
      nsCOMPtr<nsIDocShell> docShell;
      mFrameLoader->GetDocShell(getter_AddRefs(docShell));
      if (!docShell)
        return;
      presShell = docShell->GetPresShell();
      if (!presShell)
        return;
    }
  }

  nsPresContext* presContext = presShell->GetPresContext();

  int32_t parentAPD = PresContext()->AppUnitsPerDevPixel();
  int32_t subdocAPD = presContext->AppUnitsPerDevPixel();

  nsRect dirty;
  bool haveDisplayPort = false;
  bool ignoreViewportScrolling = false;
  nsIFrame* savedIgnoreScrollFrame = nullptr;
  if (subdocRootFrame) {
    // get the dirty rect relative to the root frame of the subdoc
    dirty = aDirtyRect + GetOffsetToCrossDoc(subdocRootFrame);
    // and convert into the appunits of the subdoc
    dirty = dirty.ConvertAppUnitsRoundOut(parentAPD, subdocAPD);

    nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
    // for root content documents we want the base to be the composition bounds
    nsRect displayportBase = presContext->IsRootContentDocument() ?
        nsRect(nsPoint(0,0), nsLayoutUtils::CalculateCompositionSizeForFrame(rootScrollFrame)) :
        dirty;
    nsRect displayPort;
    if (nsLayoutUtils::GetOrMaybeCreateDisplayPort(
          *aBuilder, rootScrollFrame, displayportBase, &displayPort)) {
      haveDisplayPort = true;
      dirty = displayPort;
    }

    ignoreViewportScrolling =
      rootScrollFrame && presShell->IgnoringViewportScrolling();
    if (ignoreViewportScrolling) {
      savedIgnoreScrollFrame = aBuilder->GetIgnoreScrollFrame();
      aBuilder->SetIgnoreScrollFrame(rootScrollFrame);
    }

    aBuilder->EnterPresShell(subdocRootFrame, dirty);
  }

  DisplayListClipState::AutoSaveRestore clipState(aBuilder);
  if (ShouldClipSubdocument()) {
    clipState.ClipContainingBlockDescendantsToContentBox(aBuilder, this);
  }

  nsIScrollableFrame *sf = presShell->GetRootScrollFrameAsScrollable();
  bool constructResolutionItem = subdocRootFrame &&
    (presShell->GetXResolution() != 1.0 || presShell->GetYResolution() != 1.0);
  bool constructZoomItem = subdocRootFrame && parentAPD != subdocAPD;
  bool needsOwnLayer = constructResolutionItem || constructZoomItem ||
    haveDisplayPort ||
    presContext->IsRootContentDocument() || (sf && sf->IsScrollingActive());

  // Don't let in fixed pos propagate down to child documents. This makes
  // it a little less effective but doesn't regress an important case of a
  // child document being in a fixed pos element where we would do no occlusion
  // at all if we let it propagate down.
  nsDisplayListBuilder::AutoInFixedPosSetter
    buildingInFixedPos(aBuilder, false);

  nsDisplayList childItems;

  {
    DisplayListClipState::AutoSaveRestore nestedClipState(aBuilder);
    if (needsOwnLayer) {
      // Clear current clip. There's no point in propagating it down, since
      // the layer we will construct will be clipped by the current clip.
      // In fact for nsDisplayZoom propagating it down would be incorrect since
      // nsDisplayZoom changes the meaning of appunits.
      nestedClipState.Clear();
    }

    if (subdocRootFrame) {
      aBuilder->SetAncestorHasTouchEventHandler(false);
      subdocRootFrame->
        BuildDisplayListForStackingContext(aBuilder, dirty, &childItems);
    }

    if (!aBuilder->IsForEventDelivery()) {
      // If we are going to use a displayzoom below then any items we put under
      // it need to have underlying frames from the subdocument. So we need to
      // calculate the bounds based on which frame will be the underlying frame
      // for the canvas background color item.
      nsRect bounds = GetContentRectRelativeToSelf() +
        aBuilder->ToReferenceFrame(this);
      if (subdocRootFrame) {
        bounds = bounds.ConvertAppUnitsRoundOut(parentAPD, subdocAPD);
      }

      // If we are in print preview/page layout we want to paint the grey
      // background behind the page, not the canvas color. The canvas color gets
      // painted on the page itself.
      if (nsLayoutUtils::NeedsPrintPreviewBackground(presContext)) {
        presShell->AddPrintPreviewBackgroundItem(
          *aBuilder, childItems, subdocRootFrame ? subdocRootFrame : this,
          bounds);
      } else {
        // Add the canvas background color to the bottom of the list. This
        // happens after we've built the list so that AddCanvasBackgroundColorItem
        // can monkey with the contents if necessary.
        uint32_t flags = nsIPresShell::FORCE_DRAW;
        presShell->AddCanvasBackgroundColorItem(
          *aBuilder, childItems, subdocRootFrame ? subdocRootFrame : this,
          bounds, NS_RGBA(0,0,0,0), flags);
      }
    }
  }

  // Generate a resolution and/or zoom item if needed. If one or both of those is
  // created, we don't need to create a separate nsDisplaySubDocument.

  uint32_t flags = nsDisplayOwnLayer::GENERATE_SUBDOC_INVALIDATIONS;
  // If ignoreViewportScrolling is true then the top most layer we create here
  // is going to become the scrollable layer for the root scroll frame, so we
  // want to add nsDisplayOwnLayer::GENERATE_SCROLLABLE_LAYER to whatever layer
  // becomes the topmost. We do this below.
  if (constructZoomItem) {
    uint32_t zoomFlags = flags;
    if (ignoreViewportScrolling && !constructResolutionItem) {
      zoomFlags |= nsDisplayOwnLayer::GENERATE_SCROLLABLE_LAYER;
    }
    nsDisplayZoom* zoomItem =
      new (aBuilder) nsDisplayZoom(aBuilder, subdocRootFrame, &childItems,
                                   subdocAPD, parentAPD, zoomFlags);
    childItems.AppendToTop(zoomItem);
    needsOwnLayer = false;
  }
  // Wrap the zoom item in the resolution item if we have both because we want the
  // resolution scale applied on top of the app units per dev pixel conversion.
  if (ignoreViewportScrolling) {
    flags |= nsDisplayOwnLayer::GENERATE_SCROLLABLE_LAYER;
  }
  if (constructResolutionItem) {
    nsDisplayResolution* resolutionItem =
      new (aBuilder) nsDisplayResolution(aBuilder, subdocRootFrame, &childItems,
                                         flags);
    childItems.AppendToTop(resolutionItem);
    needsOwnLayer = false;
  }
  if (needsOwnLayer) {
    // We always want top level content documents to be in their own layer.
    nsDisplaySubDocument* layerItem = new (aBuilder) nsDisplaySubDocument(
      aBuilder, subdocRootFrame ? subdocRootFrame : this,
      &childItems, flags);
    childItems.AppendToTop(layerItem);
  }

  if (subdocRootFrame) {
    aBuilder->LeavePresShell(subdocRootFrame, dirty);

    if (ignoreViewportScrolling) {
      aBuilder->SetIgnoreScrollFrame(savedIgnoreScrollFrame);
    }
  }

  if (aBuilder->IsForImageVisibility()) {
    // We don't add the childItems to the return list as we're dealing with them here.
    presShell->RebuildImageVisibility(childItems);
    childItems.DeleteAll();
  } else {
    aLists.Content()->AppendToTop(&childItems);
  }
}

nscoord
nsSubDocumentFrame::GetIntrinsicWidth()
{
  if (!IsInline()) {
    return 0;  // HTML <frame> has no useful intrinsic width
  }

  if (mContent->IsXUL()) {
    return 0;  // XUL <iframe> and <browser> have no useful intrinsic width
  }

  NS_ASSERTION(ObtainIntrinsicSizeFrame() == nullptr,
               "Intrinsic width should come from the embedded document.");

  // We must be an HTML <iframe>.  Default to a width of 300, for IE
  // compat (and per CSS2.1 draft).
  return nsPresContext::CSSPixelsToAppUnits(300);
}

nscoord
nsSubDocumentFrame::GetIntrinsicHeight()
{
  // <frame> processing does not use this routine, only <iframe>
  NS_ASSERTION(IsInline(), "Shouldn't have been called");

  if (mContent->IsXUL()) {
    return 0;
  }

  NS_ASSERTION(ObtainIntrinsicSizeFrame() == nullptr,
               "Intrinsic height should come from the embedded document.");

  // Use 150px, for compatibility with IE, and per CSS2.1 draft.
  return nsPresContext::CSSPixelsToAppUnits(150);
}

#ifdef DEBUG_FRAME_DUMP
void
nsSubDocumentFrame::List(FILE* out, const char* aPrefix, uint32_t aFlags) const
{
  nsCString str;
  ListGeneric(str, aPrefix, aFlags);
  fprintf_stderr(out, "%s\n", str.get());

  if (aFlags & TRAVERSE_SUBDOCUMENT_FRAMES) {
    nsSubDocumentFrame* f = const_cast<nsSubDocumentFrame*>(this);
    nsIFrame* subdocRootFrame = f->GetSubdocumentRootFrame();
    if (subdocRootFrame) {
      nsCString pfx(aPrefix);
      pfx += "  ";
      subdocRootFrame->List(out, pfx.get(), aFlags);
    }
  }
}

nsresult nsSubDocumentFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("FrameOuter"), aResult);
}
#endif

nsIAtom*
nsSubDocumentFrame::GetType() const
{
  return nsGkAtoms::subDocumentFrame;
}

/* virtual */ nscoord
nsSubDocumentFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    result = subDocRoot->GetMinWidth(aRenderingContext);
  } else {
    result = GetIntrinsicWidth();
  }

  return result;
}

/* virtual */ nscoord
nsSubDocumentFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);

  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    result = subDocRoot->GetPrefWidth(aRenderingContext);
  } else {
    result = GetIntrinsicWidth();
  }

  return result;
}

/* virtual */ IntrinsicSize
nsSubDocumentFrame::GetIntrinsicSize()
{
  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    return subDocRoot->GetIntrinsicSize();
  }
  return nsLeafFrame::GetIntrinsicSize();
}

/* virtual */ nsSize
nsSubDocumentFrame::GetIntrinsicRatio()
{
  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    return subDocRoot->GetIntrinsicRatio();
  }
  return nsLeafFrame::GetIntrinsicRatio();
}

/* virtual */ nsSize
nsSubDocumentFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                    nsSize aCBSize, nscoord aAvailableWidth,
                                    nsSize aMargin, nsSize aBorder,
                                    nsSize aPadding, bool aShrinkWrap)
{
  if (!IsInline()) {
    return nsFrame::ComputeAutoSize(aRenderingContext, aCBSize,
                                    aAvailableWidth, aMargin, aBorder,
                                    aPadding, aShrinkWrap);
  }

  return nsLeafFrame::ComputeAutoSize(aRenderingContext, aCBSize,
                                      aAvailableWidth, aMargin, aBorder,
                                      aPadding, aShrinkWrap);  
}


/* virtual */ nsSize
nsSubDocumentFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                                nsSize aCBSize, nscoord aAvailableWidth,
                                nsSize aMargin, nsSize aBorder, nsSize aPadding,
                                uint32_t aFlags)
{
  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    return nsLayoutUtils::ComputeSizeWithIntrinsicDimensions(
                            aRenderingContext, this,
                            subDocRoot->GetIntrinsicSize(),
                            subDocRoot->GetIntrinsicRatio(),
                            aCBSize, aMargin, aBorder, aPadding);
  }
  return nsLeafFrame::ComputeSize(aRenderingContext, aCBSize, aAvailableWidth,
                                  aMargin, aBorder, aPadding, aFlags);
}

nsresult
nsSubDocumentFrame::Reflow(nsPresContext*           aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsSubDocumentFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  // printf("OuterFrame::Reflow %X (%d,%d) \n", this, aReflowState.AvailableWidth(), aReflowState.AvailableHeight());
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsSubDocumentFrame::Reflow: maxSize=%d,%d",
      aReflowState.AvailableWidth(), aReflowState.AvailableHeight()));

  aStatus = NS_FRAME_COMPLETE;

  NS_ASSERTION(mContent->GetPrimaryFrame() == this,
               "Shouldn't happen");

  // XUL <iframe> or <browser>, or HTML <iframe>, <object> or <embed>
  nsresult rv = nsLeafFrame::DoReflow(aPresContext, aDesiredSize, aReflowState,
                                      aStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  // "offset" is the offset of our content area from our frame's
  // top-left corner.
  nsPoint offset = nsPoint(aReflowState.ComputedPhysicalBorderPadding().left,
                           aReflowState.ComputedPhysicalBorderPadding().top);

  nsSize innerSize(aDesiredSize.Width(), aDesiredSize.Height());
  innerSize.width  -= aReflowState.ComputedPhysicalBorderPadding().LeftRight();
  innerSize.height -= aReflowState.ComputedPhysicalBorderPadding().TopBottom();

  if (mInnerView) {
    nsViewManager* vm = mInnerView->GetViewManager();
    vm->MoveViewTo(mInnerView, offset.x, offset.y);
    vm->ResizeView(mInnerView, nsRect(nsPoint(0, 0), innerSize), true);
  }

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  if (!ShouldClipSubdocument()) {
    nsIFrame* subdocRootFrame = GetSubdocumentRootFrame();
    if (subdocRootFrame) {
      aDesiredSize.mOverflowAreas.UnionWith(subdocRootFrame->GetOverflowAreas() + offset);
    }
  }

  FinishAndStoreOverflow(&aDesiredSize);

  if (!aPresContext->IsPaginated() && !mPostedReflowCallback) {
    PresContext()->PresShell()->PostReflowCallback(this);
    mPostedReflowCallback = true;
  }

  // printf("OuterFrame::Reflow DONE %X (%d,%d)\n", this,
  //        aDesiredSize.Width(), aDesiredSize.Height());

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsSubDocumentFrame::Reflow: size=%d,%d status=%x",
      aDesiredSize.Width(), aDesiredSize.Height(), aStatus));

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

bool
nsSubDocumentFrame::ReflowFinished()
{
  if (mFrameLoader) {
    nsWeakFrame weakFrame(this);

    mFrameLoader->UpdatePositionAndSize(this);

    if (weakFrame.IsAlive()) {
      // Make sure that we can post a reflow callback in the future.
      mPostedReflowCallback = false;
    }
  } else {
    mPostedReflowCallback = false;
  }
  return false;
}

void
nsSubDocumentFrame::ReflowCallbackCanceled()
{
  mPostedReflowCallback = false;
}

nsresult
nsSubDocumentFrame::AttributeChanged(int32_t aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     int32_t aModType)
{
  if (aNameSpaceID != kNameSpaceID_None) {
    return NS_OK;
  }
  
  // If the noResize attribute changes, dis/allow frame to be resized
  if (aAttribute == nsGkAtoms::noresize) {
    // Note that we're not doing content type checks, but that's ok -- if
    // they'd fail we will just end up with a null framesetFrame.
    if (mContent->GetParent()->Tag() == nsGkAtoms::frameset) {
      nsIFrame* parentFrame = GetParent();

      if (parentFrame) {
        // There is no interface for nsHTMLFramesetFrame so QI'ing to
        // concrete class, yay!
        nsHTMLFramesetFrame* framesetFrame = do_QueryFrame(parentFrame);
        if (framesetFrame) {
          framesetFrame->RecalculateBorderResize();
        }
      }
    }
  }
  else if (aAttribute == nsGkAtoms::showresizer) {
    nsIFrame* rootFrame = GetSubdocumentRootFrame();
    if (rootFrame) {
      rootFrame->PresContext()->PresShell()->
        FrameNeedsReflow(rootFrame, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
    }
  }
  else if (aAttribute == nsGkAtoms::marginwidth ||
           aAttribute == nsGkAtoms::marginheight) {

    // Retrieve the attributes
    nsIntSize margins = GetMarginAttributes();

    // Notify the frameloader
    nsRefPtr<nsFrameLoader> frameloader = FrameLoader();
    if (frameloader)
      frameloader->MarginsChanged(margins.width, margins.height);
  }

  return NS_OK;
}

nsIFrame*
NS_NewSubDocumentFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSubDocumentFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSubDocumentFrame)

class nsHideViewer : public nsRunnable {
public:
  nsHideViewer(nsIContent* aFrameElement,
               nsFrameLoader* aFrameLoader,
               nsIPresShell* aPresShell,
               bool aHideViewerIfFrameless)
    : mFrameElement(aFrameElement),
      mFrameLoader(aFrameLoader),
      mPresShell(aPresShell),
      mHideViewerIfFrameless(aHideViewerIfFrameless)
  {
    NS_ASSERTION(mFrameElement, "Must have a frame element");
    NS_ASSERTION(mFrameLoader, "Must have a frame loader");
    NS_ASSERTION(mPresShell, "Must have a presshell");
  }

  NS_IMETHOD Run()
  {
    // Flush frames, to ensure any pending display:none changes are made.
    // Note it can be unsafe to flush if we've destroyed the presentation
    // for some other reason, like if we're shutting down.
    if (!mPresShell->IsDestroying()) {
      mPresShell->FlushPendingNotifications(Flush_Frames);
    }
    nsIFrame* frame = mFrameElement->GetPrimaryFrame();
    if ((!frame && mHideViewerIfFrameless) ||
        mPresShell->IsDestroying()) {
      // Either the frame element has no nsIFrame or the presshell is being
      // destroyed. Hide the nsFrameLoader, which destroys the presentation,
      // and clear our references to the stashed presentation.
      mFrameLoader->SetDetachedSubdocView(nullptr, nullptr);
      mFrameLoader->Hide();
    }
    return NS_OK;
  }
private:
  nsCOMPtr<nsIContent> mFrameElement;
  nsRefPtr<nsFrameLoader> mFrameLoader;
  nsCOMPtr<nsIPresShell> mPresShell;
  bool mHideViewerIfFrameless;
};

static nsView*
BeginSwapDocShellsForViews(nsView* aSibling);

void
nsSubDocumentFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  if (mPostedReflowCallback) {
    PresContext()->PresShell()->CancelReflowCallback(this);
    mPostedReflowCallback = false;
  }

  // Detach the subdocument's views and stash them in the frame loader.
  // We can then reattach them if we're being reframed (for example if
  // the frame has been made position:fixed).
  nsFrameLoader* frameloader = FrameLoader();
  if (frameloader) {
    nsView* detachedViews = ::BeginSwapDocShellsForViews(mInnerView->GetFirstChild());
    frameloader->SetDetachedSubdocView(detachedViews, mContent->OwnerDoc());

    // We call nsFrameLoader::HideViewer() in a script runner so that we can
    // safely determine whether the frame is being reframed or destroyed.
    nsContentUtils::AddScriptRunner(
      new nsHideViewer(mContent,
                       mFrameLoader,
                       PresContext()->PresShell(),
                       (mDidCreateDoc || mCallingShow)));
  }

  nsLeafFrame::DestroyFrom(aDestructRoot);
}

nsIntSize
nsSubDocumentFrame::GetMarginAttributes()
{
  nsIntSize result(-1, -1);
  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(mContent);
  if (content) {
    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::marginwidth);
    if (attr && attr->Type() == nsAttrValue::eInteger)
      result.width = attr->GetIntegerValue();
    attr = content->GetParsedAttr(nsGkAtoms::marginheight);
    if (attr && attr->Type() == nsAttrValue::eInteger)
      result.height = attr->GetIntegerValue();
  }
  return result;
}

nsFrameLoader*
nsSubDocumentFrame::FrameLoader()
{
  nsIContent* content = GetContent();
  if (!content)
    return nullptr;

  if (!mFrameLoader) {
    nsCOMPtr<nsIFrameLoaderOwner> loaderOwner = do_QueryInterface(content);
    if (loaderOwner) {
      nsCOMPtr<nsIFrameLoader> loader;
      loaderOwner->GetFrameLoader(getter_AddRefs(loader));
      mFrameLoader = static_cast<nsFrameLoader*>(loader.get());
    }
  }
  return mFrameLoader;
}

// XXX this should be called ObtainDocShell or something like that,
// to indicate that it could have side effects
nsresult
nsSubDocumentFrame::GetDocShell(nsIDocShell **aDocShell)
{
  *aDocShell = nullptr;

  NS_ENSURE_STATE(FrameLoader());
  return mFrameLoader->GetDocShell(aDocShell);
}

static void
DestroyDisplayItemDataForFrames(nsIFrame* aFrame)
{
  FrameLayerBuilder::DestroyDisplayItemDataFor(aFrame);

  nsIFrame::ChildListIterator lists(aFrame);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      DestroyDisplayItemDataForFrames(childFrames.get());
    }
  }
}

static bool
BeginSwapDocShellsForDocument(nsIDocument* aDocument, void*)
{
  NS_PRECONDITION(aDocument, "");

  nsIPresShell* shell = aDocument->GetShell();
  if (shell) {
    // Disable painting while the views are detached, see bug 946929.
    shell->SetNeverPainting(true);

    nsIFrame* rootFrame = shell->GetRootFrame();
    if (rootFrame) {
      ::DestroyDisplayItemDataForFrames(rootFrame);
    }
  }
  aDocument->EnumerateFreezableElements(
    nsObjectFrame::BeginSwapDocShells, nullptr);
  aDocument->EnumerateSubDocuments(BeginSwapDocShellsForDocument, nullptr);
  return true;
}

static nsView*
BeginSwapDocShellsForViews(nsView* aSibling)
{
  // Collect the removed sibling views in reverse order in 'removedViews'.
  nsView* removedViews = nullptr;
  while (aSibling) {
    nsIDocument* doc = ::GetDocumentFromView(aSibling);
    if (doc) {
      ::BeginSwapDocShellsForDocument(doc, nullptr);
    }
    nsView* next = aSibling->GetNextSibling();
    aSibling->GetViewManager()->RemoveChild(aSibling);
    aSibling->SetNextSibling(removedViews);
    removedViews = aSibling;
    aSibling = next;
  }
  return removedViews;
}

static void
InsertViewsInReverseOrder(nsView* aSibling, nsView* aParent)
{
  NS_PRECONDITION(aParent, "");
  NS_PRECONDITION(!aParent->GetFirstChild(), "inserting into non-empty list");

  nsViewManager* vm = aParent->GetViewManager();
  while (aSibling) {
    nsView* next = aSibling->GetNextSibling();
    aSibling->SetNextSibling(nullptr);
    // true means 'after' in document order which is 'before' in view order,
    // so this call prepends the child, thus reversing the siblings as we go.
    vm->InsertChild(aParent, aSibling, nullptr, true);
    aSibling = next;
  }
}

nsresult
nsSubDocumentFrame::BeginSwapDocShells(nsIFrame* aOther)
{
  if (!aOther || aOther->GetType() != nsGkAtoms::subDocumentFrame) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsSubDocumentFrame* other = static_cast<nsSubDocumentFrame*>(aOther);
  if (!mFrameLoader || !mDidCreateDoc || mCallingShow ||
      !other->mFrameLoader || !other->mDidCreateDoc) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (mInnerView && other->mInnerView) {
    nsView* ourSubdocViews = mInnerView->GetFirstChild();
    nsView* ourRemovedViews = ::BeginSwapDocShellsForViews(ourSubdocViews);
    nsView* otherSubdocViews = other->mInnerView->GetFirstChild();
    nsView* otherRemovedViews = ::BeginSwapDocShellsForViews(otherSubdocViews);

    ::InsertViewsInReverseOrder(ourRemovedViews, other->mInnerView);
    ::InsertViewsInReverseOrder(otherRemovedViews, mInnerView);
  }
  mFrameLoader.swap(other->mFrameLoader);
  return NS_OK;
}

static bool
EndSwapDocShellsForDocument(nsIDocument* aDocument, void*)
{
  NS_PRECONDITION(aDocument, "");

  // Our docshell and view trees have been updated for the new hierarchy.
  // Now also update all nsDeviceContext::mWidget to that of the
  // container view in the new hierarchy.
  nsCOMPtr<nsIDocShell> ds = aDocument->GetDocShell();
  if (ds) {
    nsCOMPtr<nsIContentViewer> cv;
    ds->GetContentViewer(getter_AddRefs(cv));
    while (cv) {
      nsRefPtr<nsPresContext> pc;
      cv->GetPresContext(getter_AddRefs(pc));
      if (pc && pc->GetPresShell()) {
        pc->GetPresShell()->SetNeverPainting(ds->IsInvisible());
      }
      nsDeviceContext* dc = pc ? pc->DeviceContext() : nullptr;
      if (dc) {
        nsView* v = cv->FindContainerView();
        dc->Init(v ? v->GetNearestWidget(nullptr) : nullptr);
      }
      nsCOMPtr<nsIContentViewer> prev;
      cv->GetPreviousViewer(getter_AddRefs(prev));
      cv = prev;
    }
  }

  aDocument->EnumerateFreezableElements(
    nsObjectFrame::EndSwapDocShells, nullptr);
  aDocument->EnumerateSubDocuments(EndSwapDocShellsForDocument, nullptr);
  return true;
}

static void
EndSwapDocShellsForViews(nsView* aSibling)
{
  for ( ; aSibling; aSibling = aSibling->GetNextSibling()) {
    nsIDocument* doc = ::GetDocumentFromView(aSibling);
    if (doc) {
      ::EndSwapDocShellsForDocument(doc, nullptr);
    }
    nsIFrame *frame = aSibling->GetFrame();
    if (frame) {
      nsIFrame* parent = nsLayoutUtils::GetCrossDocParentFrame(frame);
      if (parent->HasAnyStateBits(NS_FRAME_IN_POPUP)) {
        nsIFrame::AddInPopupStateBitToDescendants(frame);
      } else {
        nsIFrame::RemoveInPopupStateBitFromDescendants(frame);
      }
      if (frame->HasInvalidFrameInSubtree()) {
        while (parent && !parent->HasAnyStateBits(NS_FRAME_DESCENDANT_NEEDS_PAINT)) {
          parent->AddStateBits(NS_FRAME_DESCENDANT_NEEDS_PAINT);
          parent = nsLayoutUtils::GetCrossDocParentFrame(parent);
        }
      }
    }
  }
}

void
nsSubDocumentFrame::EndSwapDocShells(nsIFrame* aOther)
{
  nsSubDocumentFrame* other = static_cast<nsSubDocumentFrame*>(aOther);
  nsWeakFrame weakThis(this);
  nsWeakFrame weakOther(aOther);

  if (mInnerView) {
    ::EndSwapDocShellsForViews(mInnerView->GetFirstChild());
  }
  if (other->mInnerView) {
    ::EndSwapDocShellsForViews(other->mInnerView->GetFirstChild());
  }

  // Now make sure we reflow both frames, in case their contents
  // determine their size.
  // And repaint them, for good measure, in case there's nothing
  // interesting that happens during reflow.
  if (weakThis.IsAlive()) {
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange, NS_FRAME_IS_DIRTY);
    InvalidateFrameSubtree();
  }
  if (weakOther.IsAlive()) {
    other->PresContext()->PresShell()->
      FrameNeedsReflow(other, nsIPresShell::eTreeChange, NS_FRAME_IS_DIRTY);
    other->InvalidateFrameSubtree();
  }
}

nsView*
nsSubDocumentFrame::EnsureInnerView()
{
  if (mInnerView) {
    return mInnerView;
  }

  // create, init, set the parent of the view
  nsView* outerView = GetView();
  NS_ASSERTION(outerView, "Must have an outer view already");
  nsRect viewBounds(0, 0, 0, 0); // size will be fixed during reflow

  nsViewManager* viewMan = outerView->GetViewManager();
  nsView* innerView = viewMan->CreateView(viewBounds, outerView);
  if (!innerView) {
    NS_ERROR("Could not create inner view");
    return nullptr;
  }
  mInnerView = innerView;
  viewMan->InsertChild(outerView, innerView, nullptr, true);

  return mInnerView;
}

nsIFrame*
nsSubDocumentFrame::ObtainIntrinsicSizeFrame()
{
  nsCOMPtr<nsIObjectLoadingContent> olc = do_QueryInterface(GetContent());
  if (olc) {
    // We are an HTML <object>, <embed> or <applet> (a replaced element).

    // Try to get an nsIFrame for our sub-document's document element
    nsIFrame* subDocRoot = nullptr;

    nsCOMPtr<nsIDocShell> docShell;
    GetDocShell(getter_AddRefs(docShell));
    if (docShell) {
      nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
      if (presShell) {
        nsIScrollableFrame* scrollable = presShell->GetRootScrollFrameAsScrollable();
        if (scrollable) {
          nsIFrame* scrolled = scrollable->GetScrolledFrame();
          if (scrolled) {
            subDocRoot = scrolled->GetFirstPrincipalChild();
          }
        }
      }
    }

    if (subDocRoot && subDocRoot->GetContent() &&
        subDocRoot->GetContent()->NodeInfo()->Equals(nsGkAtoms::svg, kNameSpaceID_SVG)) {
      return subDocRoot; // SVG documents have an intrinsic size
    }
  }
  return nullptr;
}
