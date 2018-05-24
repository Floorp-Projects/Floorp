/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * rendering object for replaced elements that contain a document, such
 * as <frame>, <iframe>, and some <object>s
 */

#include "nsSubDocumentFrame.h"

#include "gfxPrefs.h"

#include "mozilla/layout/RenderFrameParent.h"

#include "nsCOMPtr.h"
#include "nsGenericHTMLElement.h"
#include "nsGenericHTMLFrameElement.h"
#include "nsAttrValueInlines.h"
#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsIContentInlines.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsFrameSetFrame.h"
#include "nsIScrollable.h"
#include "nsNameSpaceManager.h"
#include "nsDisplayList.h"
#include "nsIScrollableFrame.h"
#include "nsIObjectLoadingContent.h"
#include "nsLayoutUtils.h"
#include "FrameLayerBuilder.h"
#include "nsPluginFrame.h"
#include "nsContentUtils.h"
#include "nsIPermissionManager.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/HTMLFrameElement.h"

using namespace mozilla;
using mozilla::layout::RenderFrameParent;

static bool sShowPreviousPage = true;

static nsIDocument*
GetDocumentFromView(nsView* aView)
{
  NS_PRECONDITION(aView, "");

  nsViewManager* vm = aView->GetViewManager();
  nsIPresShell* ps =  vm ? vm->GetPresShell() : nullptr;
  return ps ? ps->GetDocument() : nullptr;
}

nsSubDocumentFrame::nsSubDocumentFrame(ComputedStyle* aStyle)
  : nsAtomicContainerFrame(aStyle, kClassID)
  , mOuterView(nullptr)
  , mInnerView(nullptr)
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
NS_QUERYFRAME_TAIL_INHERITING(nsAtomicContainerFrame)

class AsyncFrameInit : public Runnable
{
public:
  explicit AsyncFrameInit(nsIFrame* aFrame)
    : mozilla::Runnable("AsyncFrameInit")
    , mFrame(aFrame)
  {
  }
  NS_IMETHOD Run() override
  {
    AUTO_PROFILER_LABEL("AsyncFrameInit::Run", OTHER);
    if (mFrame.IsAlive()) {
      static_cast<nsSubDocumentFrame*>(mFrame.GetFrame())->ShowViewer();
    }
    return NS_OK;
  }
private:
  WeakFrame mFrame;
};

static void
InsertViewsInReverseOrder(nsView* aSibling, nsView* aParent);

static void
EndSwapDocShellsForViews(nsView* aView);

void
nsSubDocumentFrame::Init(nsIContent*       aContent,
                         nsContainerFrame* aParent,
                         nsIFrame*         aPrevInFlow)
{
  MOZ_ASSERT(aContent);
  // determine if we are a <frame> or <iframe>
  mIsInline = !aContent->IsHTMLElement(nsGkAtoms::frame);

  static bool addedShowPreviousPage = false;
  if (!addedShowPreviousPage) {
    // If layout.show_previous_page is true then during loading of a new page we
    // will draw the previous page if the new page has painting suppressed.
    Preferences::AddBoolVarCache(&sShowPreviousPage, "layout.show_previous_page", true);
    addedShowPreviousPage = true;
  }

  nsAtomicContainerFrame::Init(aContent, aParent, aPrevInFlow);

  // CreateView() creates this frame's view, stored in mOuterView.  It needs to
  // be created first since it's the parent of the inner view, stored in
  // mInnerView.
  CreateView();
  EnsureInnerView();

  // Set the primary frame now so that nsDocumentViewer::FindContainerView
  // called from within EndSwapDocShellsForViews below can find it if needed.
  aContent->SetPrimaryFrame(this);

  // If we have a detached subdoc's root view on our frame loader, re-insert
  // it into the view tree. This happens when we've been reframed, and
  // ensures the presentation persists across reframes. If the frame element
  // has changed documents however, we blow away the presentation.
  RefPtr<nsFrameLoader> frameloader = FrameLoader();
  if (frameloader) {
    nsCOMPtr<nsIDocument> oldContainerDoc;
    nsIFrame* detachedFrame =
      frameloader->GetDetachedSubdocFrame(getter_AddRefs(oldContainerDoc));
    frameloader->SetDetachedSubdocFrame(nullptr, nullptr);
    MOZ_ASSERT(oldContainerDoc || !detachedFrame);
    if (oldContainerDoc) {
      nsView* detachedView =
        detachedFrame ? detachedFrame->GetView() : nullptr;
      if (detachedView && oldContainerDoc == aContent->OwnerDoc()) {
        // Restore stashed presentation.
        ::InsertViewsInReverseOrder(detachedView, mInnerView);
        ::EndSwapDocShellsForViews(mInnerView->GetFirstChild());
      } else {
        // Presentation is for a different document, don't restore it.
        frameloader->Hide();
      }
    }
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
    RefPtr<nsFrameLoader> frameloader = FrameLoader();
    if (frameloader) {
      CSSIntSize margin = GetMarginAttributes();
      AutoWeakFrame weakThis(this);
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

nsIPresShell*
nsSubDocumentFrame::GetSubdocumentPresShellForPainting(uint32_t aFlags)
{
  if (!mInnerView)
    return nullptr;

  nsView* subdocView = mInnerView->GetFirstChild();
  if (!subdocView)
    return nullptr;

  nsIPresShell* presShell = nullptr;

  nsIFrame* subdocRootFrame = subdocView->GetFrame();
  if (subdocRootFrame) {
    presShell = subdocRootFrame->PresShell();
  }

  // If painting is suppressed in the presshell, we try to look for a better
  // presshell to use.
  if (!presShell || (presShell->IsPaintingSuppressed() &&
                     !(aFlags & IGNORE_PAINT_SUPPRESSION))) {
    // During page transition mInnerView will sometimes have two children, the
    // first being the new page that may not have any frame, and the second
    // being the old page that will probably have a frame.
    nsView* nextView = subdocView->GetNextSibling();
    nsIFrame* frame = nullptr;
    if (nextView) {
      frame = nextView->GetFrame();
    }
    if (frame) {
      nsIPresShell* ps = frame->PresShell();
      if (!presShell || (ps && !ps->IsPaintingSuppressed() && sShowPreviousPage)) {
        subdocView = nextView;
        subdocRootFrame = frame;
        presShell = ps;
      }
    }
    if (!presShell) {
      // If we don't have a frame we use this roundabout way to get the pres shell.
      if (!mFrameLoader)
        return nullptr;
      nsIDocShell* docShell = mFrameLoader->GetDocShell(IgnoreErrors());
      if (!docShell)
        return nullptr;
      presShell = docShell->GetPresShell();
    }
  }

  return presShell;
}




ScreenIntSize
nsSubDocumentFrame::GetSubdocumentSize()
{
  if (GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    RefPtr<nsFrameLoader> frameloader = FrameLoader();
    if (frameloader) {
      nsCOMPtr<nsIDocument> oldContainerDoc;
      nsIFrame* detachedFrame =
        frameloader->GetDetachedSubdocFrame(getter_AddRefs(oldContainerDoc));
      nsView* view = detachedFrame ? detachedFrame->GetView() : nullptr;
      if (view) {
        nsSize size = view->GetBounds().Size();
        nsPresContext* presContext = detachedFrame->PresContext();
        return ScreenIntSize(presContext->AppUnitsToDevPixels(size.width),
                             presContext->AppUnitsToDevPixels(size.height));
      }
    }
    // Pick some default size for now.  Using 10x10 because that's what the
    // code used to do.
    return ScreenIntSize(10, 10);
  } else {
    nsSize docSizeAppUnits;
    nsPresContext* presContext = PresContext();
    if (GetContent()->IsHTMLElement(nsGkAtoms::frame)) {
      docSizeAppUnits = GetSize();
    } else {
      docSizeAppUnits = GetContentRect().Size();
    }
    // Adjust subdocument size, according to 'object-fit' and the
    // subdocument's intrinsic size and ratio.
    nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
    if (subDocRoot) {
      nsRect destRect =
        nsLayoutUtils::ComputeObjectDestRect(nsRect(nsPoint(), docSizeAppUnits),
                                             subDocRoot->GetIntrinsicSize(),
                                             subDocRoot->GetIntrinsicRatio(),
                                             StylePosition());
      docSizeAppUnits = destRect.Size();
    }

    return ScreenIntSize(presContext->AppUnitsToDevPixels(docSizeAppUnits.width),
                         presContext->AppUnitsToDevPixels(docSizeAppUnits.height));
  }
}

static void
WrapBackgroundColorInOwnLayer(nsDisplayListBuilder* aBuilder,
                              nsIFrame* aFrame,
                              nsDisplayList* aList)
{
  nsDisplayList tempItems;
  nsDisplayItem* item;
  while ((item = aList->RemoveBottom()) != nullptr) {
    if (item->GetType() == DisplayItemType::TYPE_BACKGROUND_COLOR) {
      nsDisplayList tmpList;
      tmpList.AppendToTop(item);
      item = MakeDisplayItem<nsDisplayOwnLayer>(aBuilder, aFrame, &tmpList, aBuilder->CurrentActiveScrolledRoot());
    }
    tempItems.AppendToTop(item);
  }
  aList->AppendToTop(&tempItems);
}

void
nsSubDocumentFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return;

  nsFrameLoader* frameLoader = FrameLoader();
  RenderFrameParent* rfp = nullptr;
  if (frameLoader) {
    rfp = frameLoader->GetCurrentRenderFrame();
  }

  // If we are pointer-events:none then we don't need to HitTest background
  bool pointerEventsNone =
    StyleUserInterface()->mPointerEvents == NS_STYLE_POINTER_EVENTS_NONE;
  if (!aBuilder->IsForEventDelivery() || !pointerEventsNone) {
    nsDisplayListCollection decorations(aBuilder);
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

  if (aBuilder->IsForEventDelivery() && pointerEventsNone) {
    return;
  }

  // If we're passing pointer events to children then we have to descend into
  // subdocuments no matter what, to determine which parts are transparent for
  // hit-testing or event regions.
  bool needToDescend = aBuilder->GetDescendIntoSubdocuments();
  if (!mInnerView || !needToDescend) {
    return;
  }

  if (rfp) {
    rfp->BuildDisplayList(aBuilder, this, aLists);
    return;
  }

  nsCOMPtr<nsIPresShell> presShell =
    GetSubdocumentPresShellForPainting(
      aBuilder->IsIgnoringPaintSuppression() ? IGNORE_PAINT_SUPPRESSION : 0);

  if (!presShell) {
    return;
  }

  nsIFrame* subdocRootFrame = presShell->GetRootFrame();

  nsPresContext* presContext = presShell->GetPresContext();

  int32_t parentAPD = PresContext()->AppUnitsPerDevPixel();
  int32_t subdocAPD = presContext->AppUnitsPerDevPixel();

  nsRect visible;
  nsRect dirty;
  bool haveDisplayPort = false;
  bool ignoreViewportScrolling = false;
  nsIFrame* savedIgnoreScrollFrame = nullptr;
  if (subdocRootFrame) {
    // get the dirty rect relative to the root frame of the subdoc
    visible = aBuilder->GetVisibleRect() + GetOffsetToCrossDoc(subdocRootFrame);
    dirty = aBuilder->GetDirtyRect() + GetOffsetToCrossDoc(subdocRootFrame);
    // and convert into the appunits of the subdoc
    visible = visible.ScaleToOtherAppUnitsRoundOut(parentAPD, subdocAPD);
    dirty = dirty.ScaleToOtherAppUnitsRoundOut(parentAPD, subdocAPD);

    if (nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame()) {
      nsIScrollableFrame* rootScrollableFrame = presShell->GetRootScrollFrameAsScrollable();
      MOZ_ASSERT(rootScrollableFrame);
      // Use a copy, so the rects don't get modified.
      nsRect copyOfDirty = dirty;
      nsRect copyOfVisible = visible;
      haveDisplayPort = rootScrollableFrame->DecideScrollableLayer(aBuilder,
                          &copyOfVisible, &copyOfDirty,
                          /* aSetBase = */ true);

      if (!gfxPrefs::LayoutUseContainersForRootFrames() ||
          !aBuilder->IsPaintingToWindow()) {
        haveDisplayPort = false;
      }

      ignoreViewportScrolling = presShell->IgnoringViewportScrolling();
      if (ignoreViewportScrolling) {
        savedIgnoreScrollFrame = aBuilder->GetIgnoreScrollFrame();
        aBuilder->SetIgnoreScrollFrame(rootScrollFrame);
      }
    }

    aBuilder->EnterPresShell(subdocRootFrame, pointerEventsNone);
    aBuilder->IncrementPresShellPaintCount(presShell);
  } else {
    visible = aBuilder->GetVisibleRect();
    dirty = aBuilder->GetDirtyRect();
  }

  DisplayListClipState::AutoSaveRestore clipState(aBuilder);
  if (ShouldClipSubdocument()) {
    clipState.ClipContainingBlockDescendantsToContentBox(aBuilder, this);
  }

  nsIScrollableFrame *sf = presShell->GetRootScrollFrameAsScrollable();
  bool constructResolutionItem = subdocRootFrame &&
    (presShell->GetResolution() != 1.0);
  bool constructZoomItem = subdocRootFrame && parentAPD != subdocAPD;
  bool needsOwnLayer = false;
  if (constructResolutionItem ||
      constructZoomItem ||
      haveDisplayPort ||
      presContext->IsRootContentDocument() ||
      (sf && sf->IsScrollingActive(aBuilder)))
  {
    needsOwnLayer = true;
  }

  if (aBuilder->IsRetainingDisplayList()) {
    // Caret frame changed, rebuild the entire subdoc.
    // We could just invalidate the old and new frame
    // areas and save some work here. RetainedDisplayListBuilder
    // does this, so we could teach it to find and check all
    // subdocs in advance.
    if (mPreviousCaret != aBuilder->GetCaretFrame()) {
      dirty = visible;
      aBuilder->RebuildAllItemsInCurrentSubtree();
      // Mark the old caret frame as invalid so that we remove the
      // old nsDisplayCaret. We don't mark the current frame as invalid
      // since we want the nsDisplaySubdocument to retain it's place
      // in the retained display list.
      if (mPreviousCaret) {
        aBuilder->MarkFrameModifiedDuringBuilding(mPreviousCaret);
      }
    }
    mPreviousCaret = aBuilder->GetCaretFrame();
  }

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

    // Invoke AutoBuildingDisplayList to ensure that the correct dirty rect
    // is used to compute the visible rect if AddCanvasBackgroundColorItem
    // creates a display item.
    nsIFrame* frame = subdocRootFrame ? subdocRootFrame : this;
    nsDisplayListBuilder::AutoBuildingDisplayList
      building(aBuilder, frame, visible, dirty, true);

    if (subdocRootFrame) {
      nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
      nsDisplayListBuilder::AutoCurrentScrollParentIdSetter idSetter(
          aBuilder,
          ignoreViewportScrolling && rootScrollFrame && rootScrollFrame->GetContent()
              ? nsLayoutUtils::FindOrCreateIDFor(rootScrollFrame->GetContent())
              : aBuilder->GetCurrentScrollParentId());

      bool hasDocumentLevelListenersForApzAwareEvents =
          nsDisplayListBuilder::LayerEventRegionsEnabled() &&
          nsLayoutUtils::HasDocumentLevelListenersForApzAwareEvents(presShell);

      aBuilder->SetAncestorHasApzAwareEventHandler(hasDocumentLevelListenersForApzAwareEvents);
      subdocRootFrame->
        BuildDisplayListForStackingContext(aBuilder, &childItems);
    }

    if (!aBuilder->IsForEventDelivery()) {
      // If we are going to use a displayzoom below then any items we put under
      // it need to have underlying frames from the subdocument. So we need to
      // calculate the bounds based on which frame will be the underlying frame
      // for the canvas background color item.
      nsRect bounds = GetContentRectRelativeToSelf() +
        aBuilder->ToReferenceFrame(this);
      if (subdocRootFrame) {
        bounds = bounds.ScaleToOtherAppUnitsRoundOut(parentAPD, subdocAPD);
      }

      // If we are in print preview/page layout we want to paint the grey
      // background behind the page, not the canvas color. The canvas color gets
      // painted on the page itself.
      if (nsLayoutUtils::NeedsPrintPreviewBackground(presContext)) {
        presShell->AddPrintPreviewBackgroundItem(
          *aBuilder, childItems, frame, bounds);
      } else {
        // Add the canvas background color to the bottom of the list. This
        // happens after we've built the list so that AddCanvasBackgroundColorItem
        // can monkey with the contents if necessary.
        uint32_t flags = nsIPresShell::FORCE_DRAW | nsIPresShell::ADD_FOR_SUBDOC;
        presShell->AddCanvasBackgroundColorItem(
          *aBuilder, childItems, frame, bounds, NS_RGBA(0,0,0,0), flags);
      }
    }
  }

  if (subdocRootFrame) {
    aBuilder->LeavePresShell(subdocRootFrame, &childItems);

    if (ignoreViewportScrolling) {
      aBuilder->SetIgnoreScrollFrame(savedIgnoreScrollFrame);
    }
  }

  // Generate a resolution and/or zoom item if needed. If one or both of those is
  // created, we don't need to create a separate nsDisplaySubDocument.

  nsDisplayOwnLayerFlags flags = nsDisplayOwnLayerFlags::eGenerateSubdocInvalidations;
  // If ignoreViewportScrolling is true then the top most layer we create here
  // is going to become the scrollable layer for the root scroll frame, so we
  // want to add nsDisplayOwnLayer::GENERATE_SCROLLABLE_LAYER to whatever layer
  // becomes the topmost. We do this below.
  if (constructZoomItem) {
    nsDisplayOwnLayerFlags zoomFlags = flags;
    if (ignoreViewportScrolling && !constructResolutionItem) {
      zoomFlags |= nsDisplayOwnLayerFlags::eGenerateScrollableLayer;
    }
    nsDisplayZoom* zoomItem =
      MakeDisplayItem<nsDisplayZoom>(aBuilder, subdocRootFrame, &childItems,
                                   subdocAPD, parentAPD, zoomFlags);
    childItems.AppendToTop(zoomItem);
    needsOwnLayer = false;
  }
  // Wrap the zoom item in the resolution item if we have both because we want the
  // resolution scale applied on top of the app units per dev pixel conversion.
  if (ignoreViewportScrolling) {
    flags |= nsDisplayOwnLayerFlags::eGenerateScrollableLayer;
  }
  if (constructResolutionItem) {
    nsDisplayResolution* resolutionItem =
      MakeDisplayItem<nsDisplayResolution>(aBuilder, subdocRootFrame, &childItems,
                                           flags);
    childItems.AppendToTop(resolutionItem);
    needsOwnLayer = false;
  }

  // We always want top level content documents to be in their own layer.
  nsDisplaySubDocument* layerItem = MakeDisplayItem<nsDisplaySubDocument>(
    aBuilder, subdocRootFrame ? subdocRootFrame : this, this,
    &childItems, flags);
  childItems.AppendToTop(layerItem);
  layerItem->SetShouldFlattenAway(!needsOwnLayer);

  // If we're using containers for root frames, then the earlier call
  // to AddCanvasBackgroundColorItem won't have been able to add an
  // unscrolled color item for overscroll. Try again now that we're
  // outside the scrolled ContainerLayer.
  if (!aBuilder->IsForEventDelivery() &&
      gfxPrefs::LayoutUseContainersForRootFrames() &&
      !nsLayoutUtils::NeedsPrintPreviewBackground(presContext)) {
     nsRect bounds = GetContentRectRelativeToSelf() +
       aBuilder->ToReferenceFrame(this);

    // Invoke AutoBuildingDisplayList to ensure that the correct dirty rect
    // is used to compute the visible rect if AddCanvasBackgroundColorItem
    // creates a display item.
    nsDisplayListBuilder::AutoBuildingDisplayList
      building(aBuilder, this, visible, dirty, true);
    // Add the canvas background color to the bottom of the list. This
    // happens after we've built the list so that AddCanvasBackgroundColorItem
    // can monkey with the contents if necessary.
    uint32_t flags = nsIPresShell::FORCE_DRAW | nsIPresShell::APPEND_UNSCROLLED_ONLY;
    presShell->AddCanvasBackgroundColorItem(
      *aBuilder, childItems, this, bounds, NS_RGBA(0,0,0,0), flags);
   }

  if (aBuilder->IsForFrameVisibility()) {
    // We don't add the childItems to the return list as we're dealing with them here.
    presShell->RebuildApproximateFrameVisibilityDisplayList(childItems);
    childItems.DeleteAll(aBuilder);
  } else {
    aLists.Content()->AppendToTop(&childItems);
  }
}

nscoord
nsSubDocumentFrame::GetIntrinsicISize()
{
  if (!IsInline()) {
    return 0;  // HTML <frame> has no useful intrinsic isize
  }

  if (mContent->IsXULElement()) {
    return 0;  // XUL <iframe> and <browser> have no useful intrinsic isize
  }

  NS_ASSERTION(ObtainIntrinsicSizeFrame() == nullptr,
               "Intrinsic isize should come from the embedded document.");

  // We must be an HTML <iframe>.  Default to size of 300px x 150px, for IE
  // compat (and per CSS2.1 draft).
  WritingMode wm = GetWritingMode();
  return nsPresContext::CSSPixelsToAppUnits(wm.IsVertical() ? 150 : 300);
}

nscoord
nsSubDocumentFrame::GetIntrinsicBSize()
{
  // <frame> processing does not use this routine, only <iframe>
  NS_ASSERTION(IsInline(), "Shouldn't have been called");

  if (mContent->IsXULElement()) {
    return 0;
  }

  NS_ASSERTION(ObtainIntrinsicSizeFrame() == nullptr,
               "Intrinsic bsize should come from the embedded document.");

  // Use size of 300px x 150px, for compatibility with IE, and per CSS2.1 draft.
  WritingMode wm = GetWritingMode();
  return nsPresContext::CSSPixelsToAppUnits(wm.IsVertical() ? 300 : 150);
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

/* virtual */ nscoord
nsSubDocumentFrame::GetMinISize(gfxContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    result = subDocRoot->GetMinISize(aRenderingContext);
  } else {
    result = GetIntrinsicISize();
  }

  return result;
}

/* virtual */ nscoord
nsSubDocumentFrame::GetPrefISize(gfxContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);

  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    result = subDocRoot->GetPrefISize(aRenderingContext);
  } else {
    result = GetIntrinsicISize();
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
  return nsAtomicContainerFrame::GetIntrinsicSize();
}

/* virtual */ nsSize
nsSubDocumentFrame::GetIntrinsicRatio()
{
  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    return subDocRoot->GetIntrinsicRatio();
  }
  return nsAtomicContainerFrame::GetIntrinsicRatio();
}

/* virtual */
LogicalSize
nsSubDocumentFrame::ComputeAutoSize(gfxContext*         aRenderingContext,
                                    WritingMode         aWM,
                                    const LogicalSize&  aCBSize,
                                    nscoord             aAvailableISize,
                                    const LogicalSize&  aMargin,
                                    const LogicalSize&  aBorder,
                                    const LogicalSize&  aPadding,
                                    ComputeSizeFlags    aFlags)
{
  if (!IsInline()) {
    return nsFrame::ComputeAutoSize(aRenderingContext, aWM, aCBSize,
                                    aAvailableISize, aMargin, aBorder,
                                    aPadding, aFlags);
  }

  const WritingMode wm = GetWritingMode();
  LogicalSize result(wm, GetIntrinsicISize(), GetIntrinsicBSize());
  return result.ConvertTo(aWM, wm);
}


/* virtual */
LogicalSize
nsSubDocumentFrame::ComputeSize(gfxContext*         aRenderingContext,
                                WritingMode         aWM,
                                const LogicalSize&  aCBSize,
                                nscoord             aAvailableISize,
                                const LogicalSize&  aMargin,
                                const LogicalSize&  aBorder,
                                const LogicalSize&  aPadding,
                                ComputeSizeFlags    aFlags)
{
  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    return ComputeSizeWithIntrinsicDimensions(aRenderingContext, aWM,
                                              subDocRoot->GetIntrinsicSize(),
                                              subDocRoot->GetIntrinsicRatio(),
                                              aCBSize, aMargin, aBorder,
                                              aPadding, aFlags);
  }
  return nsAtomicContainerFrame::ComputeSize(aRenderingContext, aWM,
                                             aCBSize, aAvailableISize,
                                             aMargin, aBorder, aPadding,
                                             aFlags);
}

void
nsSubDocumentFrame::Reflow(nsPresContext*           aPresContext,
                           ReflowOutput&     aDesiredSize,
                           const ReflowInput& aReflowInput,
                           nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsSubDocumentFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsSubDocumentFrame::Reflow: maxSize=%d,%d",
      aReflowInput.AvailableWidth(), aReflowInput.AvailableHeight()));

  NS_ASSERTION(aReflowInput.ComputedWidth() != NS_UNCONSTRAINEDSIZE,
               "Shouldn't have unconstrained stuff here "
               "thanks to the rules of reflow");
  NS_ASSERTION(NS_INTRINSICSIZE != aReflowInput.ComputedHeight(),
               "Shouldn't have unconstrained stuff here "
               "thanks to ComputeAutoSize");

  NS_ASSERTION(mContent->GetPrimaryFrame() == this,
               "Shouldn't happen");

  // XUL <iframe> or <browser>, or HTML <iframe>, <object> or <embed>
  aDesiredSize.SetSize(aReflowInput.GetWritingMode(),
                       aReflowInput.ComputedSizeWithBorderPadding());

  // "offset" is the offset of our content area from our frame's
  // top-left corner.
  nsPoint offset = nsPoint(aReflowInput.ComputedPhysicalBorderPadding().left,
                           aReflowInput.ComputedPhysicalBorderPadding().top);

  if (mInnerView) {
    const nsMargin& bp = aReflowInput.ComputedPhysicalBorderPadding();
    nsSize innerSize(aDesiredSize.Width() - bp.LeftRight(),
                     aDesiredSize.Height() - bp.TopBottom());

    // Size & position the view according to 'object-fit' & 'object-position'.
    nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
    IntrinsicSize intrinsSize;
    nsSize intrinsRatio;
    if (subDocRoot) {
      intrinsSize = subDocRoot->GetIntrinsicSize();
      intrinsRatio = subDocRoot->GetIntrinsicRatio();
    }
    nsRect destRect =
      nsLayoutUtils::ComputeObjectDestRect(nsRect(offset, innerSize),
                                           intrinsSize, intrinsRatio,
                                           StylePosition());

    nsViewManager* vm = mInnerView->GetViewManager();
    vm->MoveViewTo(mInnerView, destRect.x, destRect.y);
    vm->ResizeView(mInnerView, nsRect(nsPoint(0, 0), destRect.Size()), true);
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
    PresShell()->PostReflowCallback(this);
    mPostedReflowCallback = true;
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsSubDocumentFrame::Reflow: size=%d,%d status=%s",
      aDesiredSize.Width(), aDesiredSize.Height(), ToString(aStatus).c_str()));

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

bool
nsSubDocumentFrame::ReflowFinished()
{
  if (mFrameLoader) {
    AutoWeakFrame weakFrame(this);

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
                                     nsAtom* aAttribute,
                                     int32_t aModType)
{
  if (aNameSpaceID != kNameSpaceID_None) {
    return NS_OK;
  }

  // If the noResize attribute changes, dis/allow frame to be resized
  if (aAttribute == nsGkAtoms::noresize) {
    // Note that we're not doing content type checks, but that's ok -- if
    // they'd fail we will just end up with a null framesetFrame.
    if (mContent->GetParent()->IsHTMLElement(nsGkAtoms::frameset)) {
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
      rootFrame->PresShell()->
        FrameNeedsReflow(rootFrame, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
    }
  }
  else if (aAttribute == nsGkAtoms::marginwidth ||
           aAttribute == nsGkAtoms::marginheight) {

    // Retrieve the attributes
    CSSIntSize margins = GetMarginAttributes();

    // Notify the frameloader
    RefPtr<nsFrameLoader> frameloader = FrameLoader();
    if (frameloader)
      frameloader->MarginsChanged(margins.width, margins.height);
  }

  return NS_OK;
}

nsIFrame*
NS_NewSubDocumentFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle)
{
  return new (aPresShell) nsSubDocumentFrame(aStyle);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSubDocumentFrame)

class nsHideViewer : public Runnable {
public:
  nsHideViewer(nsIContent* aFrameElement,
               nsFrameLoader* aFrameLoader,
               nsIPresShell* aPresShell,
               bool aHideViewerIfFrameless)
    : mozilla::Runnable("nsHideViewer")
    , mFrameElement(aFrameElement)
    , mFrameLoader(aFrameLoader)
    , mPresShell(aPresShell)
    , mHideViewerIfFrameless(aHideViewerIfFrameless)
  {
    NS_ASSERTION(mFrameElement, "Must have a frame element");
    NS_ASSERTION(mFrameLoader, "Must have a frame loader");
    NS_ASSERTION(mPresShell, "Must have a presshell");
  }

  NS_IMETHOD Run() override
  {
    // Flush frames, to ensure any pending display:none changes are made.
    // Note it can be unsafe to flush if we've destroyed the presentation
    // for some other reason, like if we're shutting down.
    //
    // But avoid the flush if we know for sure we're away, like when we're out
    // of the document already.
    //
    // FIXME(emilio): This could still be a perf footgun when removing lots of
    // siblings where each of them cause the reframe of an ancestor which happen
    // to contain a subdocument.
    //
    // We should find some way to avoid that!
    if (!mPresShell->IsDestroying() && mFrameElement->IsInComposedDoc()) {
      mPresShell->FlushPendingNotifications(FlushType::Frames);
    }

    // Either the frame has been constructed by now, or it never will be,
    // either way we want to clear the stashed views.
    mFrameLoader->SetDetachedSubdocFrame(nullptr, nullptr);

    nsSubDocumentFrame* frame = do_QueryFrame(mFrameElement->GetPrimaryFrame());
    if ((!frame && mHideViewerIfFrameless) ||
        mPresShell->IsDestroying()) {
      // Either the frame element has no nsIFrame or the presshell is being
      // destroyed. Hide the nsFrameLoader, which destroys the presentation.
      mFrameLoader->Hide();
    }
    return NS_OK;
  }
private:
  nsCOMPtr<nsIContent> mFrameElement;
  RefPtr<nsFrameLoader> mFrameLoader;
  nsCOMPtr<nsIPresShell> mPresShell;
  bool mHideViewerIfFrameless;
};

static nsView*
BeginSwapDocShellsForViews(nsView* aSibling);

void
nsSubDocumentFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  if (mPostedReflowCallback) {
    PresShell()->CancelReflowCallback(this);
    mPostedReflowCallback = false;
  }

  // Detach the subdocument's views and stash them in the frame loader.
  // We can then reattach them if we're being reframed (for example if
  // the frame has been made position:fixed).
  RefPtr<nsFrameLoader> frameloader = FrameLoader();
  if (frameloader) {
    nsView* detachedViews = ::BeginSwapDocShellsForViews(mInnerView->GetFirstChild());

    if (detachedViews && detachedViews->GetFrame()) {
      MOZ_ASSERT(mContent->OwnerDoc());
      frameloader->SetDetachedSubdocFrame(
        detachedViews->GetFrame(), mContent->OwnerDoc());

      // We call nsFrameLoader::HideViewer() in a script runner so that we can
      // safely determine whether the frame is being reframed or destroyed.
      nsContentUtils::AddScriptRunner(
        new nsHideViewer(mContent,
                         frameloader,
                         PresShell(),
                         (mDidCreateDoc || mCallingShow)));
    } else {
      frameloader->SetDetachedSubdocFrame(nullptr, nullptr);
      if (mDidCreateDoc || mCallingShow) {
        frameloader->Hide();
      }
    }
  }

  nsAtomicContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

CSSIntSize
nsSubDocumentFrame::GetMarginAttributes()
{
  CSSIntSize result(-1, -1);
  nsGenericHTMLElement *content = nsGenericHTMLElement::FromNode(mContent);
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
      mFrameLoader = loaderOwner->GetFrameLoader();
    }
  }
  return mFrameLoader;
}

// XXX this should be called ObtainDocShell or something like that,
// to indicate that it could have side effects
nsIDocShell*
nsSubDocumentFrame::GetDocShell()
{
  // How can FrameLoader() return null???
  if (NS_WARN_IF(!FrameLoader())) {
    return nullptr;
  }
  return mFrameLoader->GetDocShell(IgnoreErrors());
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
  aDocument->EnumerateActivityObservers(
    nsPluginFrame::BeginSwapDocShells, nullptr);
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
  if (!aOther || !aOther->IsSubDocumentFrame()) {
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
      RefPtr<nsPresContext> pc;
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

  aDocument->EnumerateActivityObservers(
    nsPluginFrame::EndSwapDocShells, nullptr);
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
        while (parent && !parent->HasAnyStateBits(NS_FRAME_DESCENDANT_NEEDS_PAINT | NS_FRAME_IS_NONDISPLAY)) {
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
  AutoWeakFrame weakThis(this);
  AutoWeakFrame weakOther(aOther);

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
    PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange, NS_FRAME_IS_DIRTY);
    InvalidateFrameSubtree();
  }
  if (weakOther.IsAlive()) {
    other->PresShell()->
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
    // We are an HTML <object> or <embed> (a replaced element).

    // Try to get an nsIFrame for our sub-document's document element
    nsIFrame* subDocRoot = nullptr;

    nsIDocShell* docShell = GetDocShell();
    if (docShell) {
      nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
      if (presShell) {
        nsIScrollableFrame* scrollable = presShell->GetRootScrollFrameAsScrollable();
        if (scrollable) {
          nsIFrame* scrolled = scrollable->GetScrolledFrame();
          if (scrolled) {
            subDocRoot = scrolled->PrincipalChildList().FirstChild();
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
