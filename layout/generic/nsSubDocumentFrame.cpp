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

#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLFrameElement.h"
#include "mozilla/dom/BrowserParent.h"

#include "nsCOMPtr.h"
#include "nsGenericHTMLElement.h"
#include "nsGenericHTMLFrameElement.h"
#include "nsAttrValueInlines.h"
#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsIContentInlines.h"
#include "nsPresContext.h"
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
#include "nsQueryObject.h"
#include "RetainedDisplayListBuilder.h"

#include "Layers.h"
#include "BasicLayers.h"
#include "mozilla/layers/WebRenderUserData.h"
#include "mozilla/layers/WebRenderScrollData.h"
#include "mozilla/layers/RenderRootStateManager.h"

using namespace mozilla;
using namespace mozilla::layers;
using mozilla::dom::Document;

static Document* GetDocumentFromView(nsView* aView) {
  MOZ_ASSERT(aView, "null view");

  nsViewManager* vm = aView->GetViewManager();
  PresShell* presShell = vm ? vm->GetPresShell() : nullptr;
  return presShell ? presShell->GetDocument() : nullptr;
}

nsSubDocumentFrame::nsSubDocumentFrame(ComputedStyle* aStyle,
                                       nsPresContext* aPresContext)
    : nsAtomicContainerFrame(aStyle, aPresContext, kClassID),
      mOuterView(nullptr),
      mInnerView(nullptr),
      mIsInline(false),
      mPostedReflowCallback(false),
      mDidCreateDoc(false),
      mCallingShow(false) {}

#ifdef ACCESSIBILITY
a11y::AccType nsSubDocumentFrame::AccessibleType() {
  return a11y::eOuterDocType;
}
#endif

NS_QUERYFRAME_HEAD(nsSubDocumentFrame)
  NS_QUERYFRAME_ENTRY(nsSubDocumentFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsAtomicContainerFrame)

class AsyncFrameInit : public Runnable {
 public:
  explicit AsyncFrameInit(nsIFrame* aFrame)
      : mozilla::Runnable("AsyncFrameInit"), mFrame(aFrame) {}
  NS_IMETHOD Run() override {
    AUTO_PROFILER_LABEL("AsyncFrameInit::Run", OTHER);
    if (mFrame.IsAlive()) {
      static_cast<nsSubDocumentFrame*>(mFrame.GetFrame())->ShowViewer();
    }
    return NS_OK;
  }

 private:
  WeakFrame mFrame;
};

static void InsertViewsInReverseOrder(nsView* aSibling, nsView* aParent);

static void EndSwapDocShellsForViews(nsView* aView);

void nsSubDocumentFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                              nsIFrame* aPrevInFlow) {
  MOZ_ASSERT(aContent);
  // determine if we are a <frame> or <iframe>
  mIsInline = !aContent->IsHTMLElement(nsGkAtoms::frame);

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
    nsCOMPtr<Document> oldContainerDoc;
    nsIFrame* detachedFrame =
        frameloader->GetDetachedSubdocFrame(getter_AddRefs(oldContainerDoc));
    frameloader->SetDetachedSubdocFrame(nullptr, nullptr);
    MOZ_ASSERT(oldContainerDoc || !detachedFrame);
    if (oldContainerDoc) {
      nsView* detachedView = detachedFrame ? detachedFrame->GetView() : nullptr;
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

  PropagateIsUnderHiddenEmbedderElementToSubView(
      PresShell()->IsUnderHiddenEmbedderElement() ||
      !StyleVisibility()->IsVisible());

  nsContentUtils::AddScriptRunner(new AsyncFrameInit(this));
}

void nsSubDocumentFrame::PropagateIsUnderHiddenEmbedderElementToSubView(
    bool aIsUnderHiddenEmbedderElement) {
  if (mFrameLoader && mFrameLoader->IsRemoteFrame()) {
    mFrameLoader->SendIsUnderHiddenEmbedderElement(
        aIsUnderHiddenEmbedderElement);
    return;
  }

  if (!mInnerView) {
    return;
  }

  nsView* subdocView = mInnerView->GetFirstChild();
  while (subdocView) {
    if (mozilla::PresShell* presShell = subdocView->GetPresShell()) {
      presShell->SetIsUnderHiddenEmbedderElement(aIsUnderHiddenEmbedderElement);
    }
    subdocView = subdocView->GetNextSibling();
  }
}

void nsSubDocumentFrame::ShowViewer() {
  if (mCallingShow) {
    return;
  }

  if (!PresContext()->IsDynamic()) {
    // We let the printing code take care of loading the document; just
    // create the inner view for it to use.
    (void)EnsureInnerView();
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
      bool didCreateDoc = frameloader->Show(margin.width, margin.height,
                                            scrolling, scrolling, this);
      if (!weakThis.IsAlive()) {
        return;
      }
      mCallingShow = false;
      mDidCreateDoc = didCreateDoc;

      if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
        frameloader->UpdatePositionAndSize(this);
      }
    }
  }
}

nsIFrame* nsSubDocumentFrame::GetSubdocumentRootFrame() {
  if (!mInnerView) return nullptr;
  nsView* subdocView = mInnerView->GetFirstChild();
  return subdocView ? subdocView->GetFrame() : nullptr;
}

mozilla::PresShell* nsSubDocumentFrame::GetSubdocumentPresShellForPainting(
    uint32_t aFlags) {
  if (!mInnerView) return nullptr;

  nsView* subdocView = mInnerView->GetFirstChild();
  if (!subdocView) return nullptr;

  mozilla::PresShell* presShell = nullptr;

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
      mozilla::PresShell* presShellForNextView = frame->PresShell();
      if (!presShell || (presShellForNextView &&
                         !presShellForNextView->IsPaintingSuppressed() &&
                         StaticPrefs::layout_show_previous_page())) {
        subdocView = nextView;
        subdocRootFrame = frame;
        presShell = presShellForNextView;
      }
    }
    if (!presShell) {
      // If we don't have a frame we use this roundabout way to get the pres
      // shell.
      if (!mFrameLoader) return nullptr;
      nsIDocShell* docShell = mFrameLoader->GetDocShell(IgnoreErrors());
      if (!docShell) return nullptr;
      presShell = docShell->GetPresShell();
    }
  }

  return presShell;
}

ScreenIntSize nsSubDocumentFrame::GetSubdocumentSize() {
  if (GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    RefPtr<nsFrameLoader> frameloader = FrameLoader();
    if (frameloader) {
      nsCOMPtr<Document> oldContainerDoc;
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
      nsRect destRect = nsLayoutUtils::ComputeObjectDestRect(
          nsRect(nsPoint(), docSizeAppUnits), subDocRoot->GetIntrinsicSize(),
          subDocRoot->GetIntrinsicRatio(), StylePosition());
      docSizeAppUnits = destRect.Size();
    }

    return ScreenIntSize(
        presContext->AppUnitsToDevPixels(docSizeAppUnits.width),
        presContext->AppUnitsToDevPixels(docSizeAppUnits.height));
  }
}

static void WrapBackgroundColorInOwnLayer(nsDisplayListBuilder* aBuilder,
                                          nsIFrame* aFrame,
                                          nsDisplayList* aList) {
  nsDisplayList tempItems;
  nsDisplayItem* item;
  while ((item = aList->RemoveBottom()) != nullptr) {
    if (item->GetType() == DisplayItemType::TYPE_BACKGROUND_COLOR) {
      nsDisplayList tmpList;
      tmpList.AppendToTop(item);
      item = MakeDisplayItem<nsDisplayOwnLayer>(
          aBuilder, aFrame, &tmpList, aBuilder->CurrentActiveScrolledRoot());
    }
    if (item) {
      tempItems.AppendToTop(item);
    }
  }
  aList->AppendToTop(&tempItems);
}

void nsSubDocumentFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                          const nsDisplayListSet& aLists) {
  if (!IsVisibleForPainting()) return;

  nsFrameLoader* frameLoader = FrameLoader();
  bool isRemoteFrame = frameLoader && frameLoader->IsRemoteFrame();

  // If we are pointer-events:none then we don't need to HitTest background
  bool pointerEventsNone =
      StyleUI()->mPointerEvents == NS_STYLE_POINTER_EVENTS_NONE;
  if (!aBuilder->IsForEventDelivery() || !pointerEventsNone) {
    nsDisplayListCollection decorations(aBuilder);
    DisplayBorderBackgroundOutline(aBuilder, decorations);
    if (isRemoteFrame) {
      // Wrap background colors of <iframe>s with remote subdocuments in their
      // own layer so we generate a ColorLayer. This is helpful for optimizing
      // compositing; we can skip compositing the ColorLayer when the
      // remote content is opaque.
      WrapBackgroundColorInOwnLayer(aBuilder, this,
                                    decorations.BorderBackground());
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

  if (isRemoteFrame) {
    // We're the subdoc for <browser remote="true"> and it has
    // painted content.  Display its shadow layer tree.
    DisplayListClipState::AutoSaveRestore clipState(aBuilder);

    nsPoint offset = aBuilder->ToReferenceFrame(this);
    nsRect bounds = this->EnsureInnerView()->GetBounds() + offset;
    clipState.ClipContentDescendants(bounds);

    aLists.Content()->AppendNewToTop<nsDisplayRemote>(aBuilder, this);
    return;
  }

  RefPtr<mozilla::PresShell> presShell = GetSubdocumentPresShellForPainting(
      aBuilder->IsIgnoringPaintSuppression() ? IGNORE_PAINT_SUPPRESSION : 0);

  if (!presShell) {
    return;
  }

  if (aBuilder->IsInFilter()) {
    Document* outerDoc = PresShell()->GetDocument();
    Document* innerDoc = presShell->GetDocument();
    if (outerDoc && innerDoc) {
      if (!outerDoc->NodePrincipal()->Equals(innerDoc->NodePrincipal())) {
        outerDoc->SetUseCounter(eUseCounter_custom_FilteredCrossOriginIFrame);
      }
    }
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
      nsIScrollableFrame* rootScrollableFrame =
          presShell->GetRootScrollFrameAsScrollable();
      MOZ_ASSERT(rootScrollableFrame);
      // Use a copy, so the rects don't get modified.
      nsRect copyOfDirty = dirty;
      nsRect copyOfVisible = visible;
      haveDisplayPort = rootScrollableFrame->DecideScrollableLayer(
          aBuilder, &copyOfVisible, &copyOfDirty,
          /* aSetBase = */ true);

      if (!StaticPrefs::layout_scroll_root_frame_containers() ||
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
  clipState.ClipContainingBlockDescendantsToContentBox(aBuilder, this);

  nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollable();
  bool constructResolutionItem =
      subdocRootFrame && (presShell->GetResolution() != 1.0);
  bool constructZoomItem = subdocRootFrame && parentAPD != subdocAPD;
  bool needsOwnLayer = false;
  if (constructResolutionItem || constructZoomItem || haveDisplayPort ||
      presContext->IsRootContentDocument() ||
      (sf && sf->IsScrollingActive(aBuilder))) {
    needsOwnLayer = true;
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
    nsDisplayListBuilder::AutoBuildingDisplayList building(aBuilder, frame,
                                                           visible, dirty);

    if (subdocRootFrame) {
      nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
      nsDisplayListBuilder::AutoCurrentScrollParentIdSetter idSetter(
          aBuilder,
          ignoreViewportScrolling && rootScrollFrame &&
                  rootScrollFrame->GetContent()
              ? nsLayoutUtils::FindOrCreateIDFor(rootScrollFrame->GetContent())
              : aBuilder->GetCurrentScrollParentId());

      bool hasDocumentLevelListenersForApzAwareEvents =
          gfxPlatform::AsyncPanZoomEnabled() &&
          nsLayoutUtils::HasDocumentLevelListenersForApzAwareEvents(presShell);

      aBuilder->SetAncestorHasApzAwareEventHandler(
          hasDocumentLevelListenersForApzAwareEvents);
      subdocRootFrame->BuildDisplayListForStackingContext(aBuilder,
                                                          &childItems);
    }

    if (!aBuilder->IsForEventDelivery()) {
      // If we are going to use a displayzoom below then any items we put under
      // it need to have underlying frames from the subdocument. So we need to
      // calculate the bounds based on which frame will be the underlying frame
      // for the canvas background color item.
      nsRect bounds =
          GetContentRectRelativeToSelf() + aBuilder->ToReferenceFrame(this);
      if (subdocRootFrame) {
        bounds = bounds.ScaleToOtherAppUnitsRoundOut(parentAPD, subdocAPD);
      }

      // If we are in print preview/page layout we want to paint the grey
      // background behind the page, not the canvas color. The canvas color gets
      // painted on the page itself.
      if (nsLayoutUtils::NeedsPrintPreviewBackground(presContext)) {
        presShell->AddPrintPreviewBackgroundItem(aBuilder, &childItems, frame,
                                                 bounds);
      } else {
        // Add the canvas background color to the bottom of the list. This
        // happens after we've built the list so that
        // AddCanvasBackgroundColorItem can monkey with the contents if
        // necessary.
        AddCanvasBackgroundColorFlags flags =
            AddCanvasBackgroundColorFlags::ForceDraw |
            AddCanvasBackgroundColorFlags::AddForSubDocument;
        presShell->AddCanvasBackgroundColorItem(
            aBuilder, &childItems, frame, bounds, NS_RGBA(0, 0, 0, 0), flags);
      }
    }
  }

  if (subdocRootFrame) {
    aBuilder->LeavePresShell(subdocRootFrame, &childItems);

    if (ignoreViewportScrolling) {
      aBuilder->SetIgnoreScrollFrame(savedIgnoreScrollFrame);
    }
  }

  // Generate a resolution and/or zoom item if needed. If one or both of those
  // is created, we don't need to create a separate nsDisplaySubDocument.

  nsDisplayOwnLayerFlags flags =
      nsDisplayOwnLayerFlags::GenerateSubdocInvalidations;
  // If ignoreViewportScrolling is true then the top most layer we create here
  // is going to become the scrollable layer for the root scroll frame, so we
  // want to add nsDisplayOwnLayer::GENERATE_SCROLLABLE_LAYER to whatever layer
  // becomes the topmost. We do this below.
  if (constructZoomItem) {
    nsDisplayOwnLayerFlags zoomFlags = flags;
    if (ignoreViewportScrolling && !constructResolutionItem) {
      zoomFlags |= nsDisplayOwnLayerFlags::GenerateScrollableLayer;
    }
    childItems.AppendNewToTop<nsDisplayZoom>(aBuilder, subdocRootFrame, this,
                                             &childItems, subdocAPD, parentAPD,
                                             zoomFlags);

    needsOwnLayer = false;
  }
  // Wrap the zoom item in the resolution item if we have both because we want
  // the resolution scale applied on top of the app units per dev pixel
  // conversion.
  if (ignoreViewportScrolling) {
    flags |= nsDisplayOwnLayerFlags::GenerateScrollableLayer;
  }
  if (constructResolutionItem) {
    childItems.AppendNewToTop<nsDisplayResolution>(aBuilder, subdocRootFrame,
                                                   this, &childItems, flags);

    needsOwnLayer = false;
  }

  // We always want top level content documents to be in their own layer.
  nsDisplaySubDocument* layerItem = MakeDisplayItem<nsDisplaySubDocument>(
      aBuilder, subdocRootFrame ? subdocRootFrame : this, this, &childItems,
      flags);
  if (layerItem) {
    childItems.AppendToTop(layerItem);
    layerItem->SetShouldFlattenAway(!needsOwnLayer);
  }

  // If we're using containers for root frames, then the earlier call
  // to AddCanvasBackgroundColorItem won't have been able to add an
  // unscrolled color item for overscroll. Try again now that we're
  // outside the scrolled ContainerLayer.
  if (!aBuilder->IsForEventDelivery() &&
      StaticPrefs::layout_scroll_root_frame_containers() &&
      !nsLayoutUtils::NeedsPrintPreviewBackground(presContext)) {
    nsRect bounds =
        GetContentRectRelativeToSelf() + aBuilder->ToReferenceFrame(this);

    // Invoke AutoBuildingDisplayList to ensure that the correct dirty rect
    // is used to compute the visible rect if AddCanvasBackgroundColorItem
    // creates a display item.
    nsDisplayListBuilder::AutoBuildingDisplayList building(aBuilder, this,
                                                           visible, dirty);
    // Add the canvas background color to the bottom of the list. This
    // happens after we've built the list so that AddCanvasBackgroundColorItem
    // can monkey with the contents if necessary.
    AddCanvasBackgroundColorFlags flags =
        AddCanvasBackgroundColorFlags::ForceDraw |
        AddCanvasBackgroundColorFlags::AppendUnscrolledOnly;
    presShell->AddCanvasBackgroundColorItem(aBuilder, &childItems, this, bounds,
                                            NS_RGBA(0, 0, 0, 0), flags);
  }

  if (aBuilder->IsForFrameVisibility()) {
    // We don't add the childItems to the return list as we're dealing with them
    // here.
    presShell->RebuildApproximateFrameVisibilityDisplayList(childItems);
    childItems.DeleteAll(aBuilder);
  } else {
    aLists.Content()->AppendToTop(&childItems);
  }
}

nscoord nsSubDocumentFrame::GetIntrinsicISize() {
  if (StyleDisplay()->IsContainSize()) {
    return 0;  // Intrinsic size of 'contain:size' replaced elements is 0,0.
  }

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

nscoord nsSubDocumentFrame::GetIntrinsicBSize() {
  // <frame> processing does not use this routine, only <iframe>
  NS_ASSERTION(IsInline(), "Shouldn't have been called");

  if (StyleDisplay()->IsContainSize()) {
    return 0;  // Intrinsic size of 'contain:size' replaced elements is 0,0.
  }

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
void nsSubDocumentFrame::List(FILE* out, const char* aPrefix,
                              uint32_t aFlags) const {
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

nsresult nsSubDocumentFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(NS_LITERAL_STRING("FrameOuter"), aResult);
}
#endif

/* virtual */
nscoord nsSubDocumentFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_MIN_INLINE_SIZE(this, result);

  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    result = subDocRoot->GetMinISize(aRenderingContext);
  } else {
    result = GetIntrinsicISize();
  }

  return result;
}

/* virtual */
nscoord nsSubDocumentFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_PREF_INLINE_SIZE(this, result);

  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    result = subDocRoot->GetPrefISize(aRenderingContext);
  } else {
    result = GetIntrinsicISize();
  }

  return result;
}

/* virtual */
IntrinsicSize nsSubDocumentFrame::GetIntrinsicSize() {
  if (StyleDisplay()->IsContainSize()) {
    // Intrinsic size of 'contain:size' replaced elements is 0,0.
    return IntrinsicSize(0, 0);
  }

  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    return subDocRoot->GetIntrinsicSize();
  }
  return nsAtomicContainerFrame::GetIntrinsicSize();
}

/* virtual */
AspectRatio nsSubDocumentFrame::GetIntrinsicRatio() {
  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    return subDocRoot->GetIntrinsicRatio();
  }
  return nsAtomicContainerFrame::GetIntrinsicRatio();
}

/* virtual */
LogicalSize nsSubDocumentFrame::ComputeAutoSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorder, const LogicalSize& aPadding,
    ComputeSizeFlags aFlags) {
  if (!IsInline()) {
    return nsFrame::ComputeAutoSize(aRenderingContext, aWM, aCBSize,
                                    aAvailableISize, aMargin, aBorder, aPadding,
                                    aFlags);
  }

  const WritingMode wm = GetWritingMode();
  LogicalSize result(wm, GetIntrinsicISize(), GetIntrinsicBSize());
  return result.ConvertTo(aWM, wm);
}

/* virtual */
LogicalSize nsSubDocumentFrame::ComputeSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorder, const LogicalSize& aPadding,
    ComputeSizeFlags aFlags) {
  nsIFrame* subDocRoot = ObtainIntrinsicSizeFrame();
  if (subDocRoot) {
    return ComputeSizeWithIntrinsicDimensions(
        aRenderingContext, aWM, subDocRoot->GetIntrinsicSize(),
        subDocRoot->GetIntrinsicRatio(), aCBSize, aMargin, aBorder, aPadding,
        aFlags);
  }
  return nsAtomicContainerFrame::ComputeSize(aRenderingContext, aWM, aCBSize,
                                             aAvailableISize, aMargin, aBorder,
                                             aPadding, aFlags);
}

void nsSubDocumentFrame::Reflow(nsPresContext* aPresContext,
                                ReflowOutput& aDesiredSize,
                                const ReflowInput& aReflowInput,
                                nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsSubDocumentFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  NS_FRAME_TRACE(
      NS_FRAME_TRACE_CALLS,
      ("enter nsSubDocumentFrame::Reflow: maxSize=%d,%d",
       aReflowInput.AvailableWidth(), aReflowInput.AvailableHeight()));

  NS_ASSERTION(aReflowInput.ComputedWidth() != NS_UNCONSTRAINEDSIZE,
               "Shouldn't have unconstrained stuff here "
               "thanks to the rules of reflow");
  NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aReflowInput.ComputedHeight(),
               "Shouldn't have unconstrained stuff here "
               "thanks to ComputeAutoSize");

  NS_ASSERTION(mContent->GetPrimaryFrame() == this, "Shouldn't happen");

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
    AspectRatio intrinsRatio;
    if (subDocRoot) {
      intrinsSize = subDocRoot->GetIntrinsicSize();
      intrinsRatio = subDocRoot->GetIntrinsicRatio();
    }
    nsRect destRect = nsLayoutUtils::ComputeObjectDestRect(
        nsRect(offset, innerSize), intrinsSize, intrinsRatio, StylePosition());

    nsViewManager* vm = mInnerView->GetViewManager();
    vm->MoveViewTo(mInnerView, destRect.x, destRect.y);
    vm->ResizeView(mInnerView, nsRect(nsPoint(0, 0), destRect.Size()), true);
  }

  aDesiredSize.SetOverflowAreasToDesiredBounds();

  FinishAndStoreOverflow(&aDesiredSize);

  if (!aPresContext->IsPaginated() && !mPostedReflowCallback) {
    PresShell()->PostReflowCallback(this);
    mPostedReflowCallback = true;
  }

  NS_FRAME_TRACE(
      NS_FRAME_TRACE_CALLS,
      ("exit nsSubDocumentFrame::Reflow: size=%d,%d status=%s",
       aDesiredSize.Width(), aDesiredSize.Height(), ToString(aStatus).c_str()));

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

bool nsSubDocumentFrame::ReflowFinished() {
  RefPtr<nsFrameLoader> frameloader = FrameLoader();
  if (frameloader) {
    AutoWeakFrame weakFrame(this);

    frameloader->UpdatePositionAndSize(this);

    if (weakFrame.IsAlive()) {
      // Make sure that we can post a reflow callback in the future.
      mPostedReflowCallback = false;
    }
  } else {
    mPostedReflowCallback = false;
  }
  return false;
}

void nsSubDocumentFrame::ReflowCallbackCanceled() {
  mPostedReflowCallback = false;
}

nsresult nsSubDocumentFrame::AttributeChanged(int32_t aNameSpaceID,
                                              nsAtom* aAttribute,
                                              int32_t aModType) {
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
  } else if (aAttribute == nsGkAtoms::showresizer) {
    nsIFrame* rootFrame = GetSubdocumentRootFrame();
    if (rootFrame) {
      rootFrame->PresShell()->FrameNeedsReflow(
          rootFrame, IntrinsicDirty::Resize, NS_FRAME_IS_DIRTY);
    }
  } else if (aAttribute == nsGkAtoms::marginwidth ||
             aAttribute == nsGkAtoms::marginheight) {
    // Retrieve the attributes
    CSSIntSize margins = GetMarginAttributes();

    // Notify the frameloader
    RefPtr<nsFrameLoader> frameloader = FrameLoader();
    if (frameloader) frameloader->MarginsChanged(margins.width, margins.height);
  }

  return NS_OK;
}

void nsSubDocumentFrame::DidSetComputedStyle(ComputedStyle* aOldComputedStyle) {
  nsAtomicContainerFrame::DidSetComputedStyle(aOldComputedStyle);

  // If this presshell has invisible ancestors, we don't need to propagate the
  // visibility style change to the subdocument since the subdocument should
  // have already set the IsUnderHiddenEmbedderElement flag in
  // nsSubDocumentFrame::Init.
  if (PresShell()->IsUnderHiddenEmbedderElement()) {
    return;
  }

  const bool isVisible = StyleVisibility()->IsVisible();
  if (!aOldComputedStyle ||
      isVisible != aOldComputedStyle->StyleVisibility()->IsVisible()) {
    PropagateIsUnderHiddenEmbedderElementToSubView(!isVisible);
  }
}

nsIFrame* NS_NewSubDocumentFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell)
      nsSubDocumentFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsSubDocumentFrame)

class nsHideViewer : public Runnable {
 public:
  nsHideViewer(nsIContent* aFrameElement, nsFrameLoader* aFrameLoader,
               PresShell* aPresShell, bool aHideViewerIfFrameless)
      : mozilla::Runnable("nsHideViewer"),
        mFrameElement(aFrameElement),
        mFrameLoader(aFrameLoader),
        mPresShell(aPresShell),
        mHideViewerIfFrameless(aHideViewerIfFrameless) {
    NS_ASSERTION(mFrameElement, "Must have a frame element");
    NS_ASSERTION(mFrameLoader, "Must have a frame loader");
    NS_ASSERTION(mPresShell, "Must have a presshell");
  }

  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override {
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
      MOZ_KnownLive(mPresShell)->FlushPendingNotifications(FlushType::Frames);
    }

    // Either the frame has been constructed by now, or it never will be,
    // either way we want to clear the stashed views.
    mFrameLoader->SetDetachedSubdocFrame(nullptr, nullptr);

    nsSubDocumentFrame* frame = do_QueryFrame(mFrameElement->GetPrimaryFrame());
    if ((!frame && mHideViewerIfFrameless) || mPresShell->IsDestroying()) {
      // Either the frame element has no nsIFrame or the presshell is being
      // destroyed. Hide the nsFrameLoader, which destroys the presentation.
      mFrameLoader->Hide();
    }
    return NS_OK;
  }

 private:
  nsCOMPtr<nsIContent> mFrameElement;
  RefPtr<nsFrameLoader> mFrameLoader;
  RefPtr<PresShell> mPresShell;
  bool mHideViewerIfFrameless;
};

static nsView* BeginSwapDocShellsForViews(nsView* aSibling);

void nsSubDocumentFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                     PostDestroyData& aPostDestroyData) {
  if (mPostedReflowCallback) {
    PresShell()->CancelReflowCallback(this);
    mPostedReflowCallback = false;
  }

  // Detach the subdocument's views and stash them in the frame loader.
  // We can then reattach them if we're being reframed (for example if
  // the frame has been made position:fixed).
  RefPtr<nsFrameLoader> frameloader = FrameLoader();
  if (frameloader) {
    nsView* detachedViews =
        ::BeginSwapDocShellsForViews(mInnerView->GetFirstChild());

    if (detachedViews && detachedViews->GetFrame()) {
      MOZ_ASSERT(mContent->OwnerDoc());
      frameloader->SetDetachedSubdocFrame(detachedViews->GetFrame(),
                                          mContent->OwnerDoc());

      // We call nsFrameLoader::HideViewer() in a script runner so that we can
      // safely determine whether the frame is being reframed or destroyed.
      nsContentUtils::AddScriptRunner(new nsHideViewer(
          mContent, frameloader, PresShell(), (mDidCreateDoc || mCallingShow)));
    } else {
      frameloader->SetDetachedSubdocFrame(nullptr, nullptr);
      if (mDidCreateDoc || mCallingShow) {
        frameloader->Hide();
      }
    }
  }

  nsAtomicContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

CSSIntSize nsSubDocumentFrame::GetMarginAttributes() {
  CSSIntSize result(-1, -1);
  nsGenericHTMLElement* content = nsGenericHTMLElement::FromNode(mContent);
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

nsFrameLoader* nsSubDocumentFrame::FrameLoader() const {
  nsIContent* content = GetContent();
  if (!content) return nullptr;

  if (!mFrameLoader) {
    RefPtr<nsFrameLoaderOwner> loaderOwner = do_QueryObject(content);
    if (loaderOwner) {
      mFrameLoader = loaderOwner->GetFrameLoader();
    }
  }
  return mFrameLoader;
}

void nsSubDocumentFrame::ResetFrameLoader() {
  mFrameLoader = nullptr;
  nsContentUtils::AddScriptRunner(new AsyncFrameInit(this));
}

// XXX this should be called ObtainDocShell or something like that,
// to indicate that it could have side effects
nsIDocShell* nsSubDocumentFrame::GetDocShell() {
  // How can FrameLoader() return null???
  if (NS_WARN_IF(!FrameLoader())) {
    return nullptr;
  }
  return mFrameLoader->GetDocShell(IgnoreErrors());
}

static void DestroyDisplayItemDataForFrames(nsIFrame* aFrame) {
  FrameLayerBuilder::DestroyDisplayItemDataFor(aFrame);

  nsIFrame::ChildListIterator lists(aFrame);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      DestroyDisplayItemDataForFrames(childFrames.get());
    }
  }
}

static bool BeginSwapDocShellsForDocument(Document* aDocument, void*) {
  MOZ_ASSERT(aDocument, "null document");

  mozilla::PresShell* presShell = aDocument->GetPresShell();
  if (presShell) {
    // Disable painting while the views are detached, see bug 946929.
    presShell->SetNeverPainting(true);

    nsIFrame* rootFrame = presShell->GetRootFrame();
    if (rootFrame) {
      ::DestroyDisplayItemDataForFrames(rootFrame);
    }
  }
  aDocument->EnumerateActivityObservers(nsPluginFrame::BeginSwapDocShells,
                                        nullptr);
  aDocument->EnumerateSubDocuments(BeginSwapDocShellsForDocument, nullptr);
  return true;
}

static nsView* BeginSwapDocShellsForViews(nsView* aSibling) {
  // Collect the removed sibling views in reverse order in 'removedViews'.
  nsView* removedViews = nullptr;
  while (aSibling) {
    Document* doc = ::GetDocumentFromView(aSibling);
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

static void InsertViewsInReverseOrder(nsView* aSibling, nsView* aParent) {
  MOZ_ASSERT(aParent, "null view");
  MOZ_ASSERT(!aParent->GetFirstChild(), "inserting into non-empty list");

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

nsresult nsSubDocumentFrame::BeginSwapDocShells(nsIFrame* aOther) {
  if (!aOther || !aOther->IsSubDocumentFrame()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsSubDocumentFrame* other = static_cast<nsSubDocumentFrame*>(aOther);
  if (!mFrameLoader || !mDidCreateDoc || mCallingShow || !other->mFrameLoader ||
      !other->mDidCreateDoc) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  ClearDisplayItems();
  other->ClearDisplayItems();

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

static bool EndSwapDocShellsForDocument(Document* aDocument, void*) {
  MOZ_ASSERT(aDocument, "null document");

  // Our docshell and view trees have been updated for the new hierarchy.
  // Now also update all nsDeviceContext::mWidget to that of the
  // container view in the new hierarchy.
  nsCOMPtr<nsIDocShell> ds = aDocument->GetDocShell();
  if (ds) {
    nsCOMPtr<nsIContentViewer> cv;
    ds->GetContentViewer(getter_AddRefs(cv));
    while (cv) {
      RefPtr<nsPresContext> pc = cv->GetPresContext();
      if (pc && pc->GetPresShell()) {
        pc->GetPresShell()->SetNeverPainting(ds->IsInvisible());
      }
      nsDeviceContext* dc = pc ? pc->DeviceContext() : nullptr;
      if (dc) {
        nsView* v = cv->FindContainerView();
        dc->Init(v ? v->GetNearestWidget(nullptr) : nullptr);
      }
      cv = cv->GetPreviousViewer();
    }
  }

  aDocument->EnumerateActivityObservers(nsPluginFrame::EndSwapDocShells,
                                        nullptr);
  aDocument->EnumerateSubDocuments(EndSwapDocShellsForDocument, nullptr);
  return true;
}

static void EndSwapDocShellsForViews(nsView* aSibling) {
  for (; aSibling; aSibling = aSibling->GetNextSibling()) {
    Document* doc = ::GetDocumentFromView(aSibling);
    if (doc) {
      ::EndSwapDocShellsForDocument(doc, nullptr);
    }
    nsIFrame* frame = aSibling->GetFrame();
    if (frame) {
      nsIFrame* parent = nsLayoutUtils::GetCrossDocParentFrame(frame);
      if (parent->HasAnyStateBits(NS_FRAME_IN_POPUP)) {
        nsIFrame::AddInPopupStateBitToDescendants(frame);
      } else {
        nsIFrame::RemoveInPopupStateBitFromDescendants(frame);
      }
      if (frame->HasInvalidFrameInSubtree()) {
        while (parent &&
               !parent->HasAnyStateBits(NS_FRAME_DESCENDANT_NEEDS_PAINT |
                                        NS_FRAME_IS_NONDISPLAY)) {
          parent->AddStateBits(NS_FRAME_DESCENDANT_NEEDS_PAINT);
          parent = nsLayoutUtils::GetCrossDocParentFrame(parent);
        }
      }
    }
  }
}

void nsSubDocumentFrame::EndSwapDocShells(nsIFrame* aOther) {
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
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::TreeChange,
                                  NS_FRAME_IS_DIRTY);
    InvalidateFrameSubtree();
    PropagateIsUnderHiddenEmbedderElementToSubView(
        PresShell()->IsUnderHiddenEmbedderElement() ||
        !StyleVisibility()->IsVisible());
  }
  if (weakOther.IsAlive()) {
    other->PresShell()->FrameNeedsReflow(other, IntrinsicDirty::TreeChange,
                                         NS_FRAME_IS_DIRTY);
    other->InvalidateFrameSubtree();
    other->PropagateIsUnderHiddenEmbedderElementToSubView(
        other->PresShell()->IsUnderHiddenEmbedderElement() ||
        !other->StyleVisibility()->IsVisible());
  }
}

void nsSubDocumentFrame::ClearDisplayItems() {
  DisplayItemArray* items = GetProperty(DisplayItems());
  if (!items) {
    return;
  }

  nsIFrame* displayRoot = nsLayoutUtils::GetDisplayRootFrame(this);
  MOZ_ASSERT(displayRoot);

  RetainedDisplayListBuilder* retainedBuilder =
      displayRoot->GetProperty(RetainedDisplayListBuilder::Cached());
  MOZ_ASSERT(retainedBuilder);

  for (nsDisplayItemBase* i : *items) {
    if (i->GetType() == DisplayItemType::TYPE_SUBDOCUMENT) {
      auto* item = static_cast<nsDisplaySubDocument*>(i);
      item->GetChildren()->DeleteAll(retainedBuilder->Builder());
      item->Disown();
      break;
    }
  }
}

nsView* nsSubDocumentFrame::EnsureInnerView() {
  if (mInnerView) {
    return mInnerView;
  }

  // create, init, set the parent of the view
  nsView* outerView = GetView();
  NS_ASSERTION(outerView, "Must have an outer view already");
  nsRect viewBounds(0, 0, 0, 0);  // size will be fixed during reflow

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

nsPoint nsSubDocumentFrame::GetExtraOffset() const {
  MOZ_ASSERT(mInnerView);
  return mInnerView->GetPosition();
}

nsIFrame* nsSubDocumentFrame::ObtainIntrinsicSizeFrame() {
  if (StyleDisplay()->IsContainSize()) {
    // Intrinsic size of 'contain:size' replaced elements is 0,0. So, don't use
    // internal frames to provide intrinsic size at all.
    return nullptr;
  }

  nsCOMPtr<nsIObjectLoadingContent> olc = do_QueryInterface(GetContent());
  if (olc) {
    // We are an HTML <object> or <embed> (a replaced element).

    // Try to get an nsIFrame for our sub-document's document element
    nsIFrame* subDocRoot = nullptr;

    if (nsIDocShell* docShell = GetDocShell()) {
      if (mozilla::PresShell* presShell = docShell->GetPresShell()) {
        nsIScrollableFrame* scrollable =
            presShell->GetRootScrollFrameAsScrollable();
        if (scrollable) {
          nsIFrame* scrolled = scrollable->GetScrolledFrame();
          if (scrolled) {
            subDocRoot = scrolled->PrincipalChildList().FirstChild();
          }
        }
      }
    }

    if (subDocRoot && subDocRoot->GetContent() &&
        subDocRoot->GetContent()->NodeInfo()->Equals(nsGkAtoms::svg,
                                                     kNameSpaceID_SVG)) {
      return subDocRoot;  // SVG documents have an intrinsic size
    }
  }
  return nullptr;
}

/**
 * Gets the layer-pixel offset of aContainerFrame's content rect top-left
 * from the nearest display item reference frame (which we assume will be
 * inducing a ContainerLayer).
 */
static mozilla::LayoutDeviceIntPoint GetContentRectLayerOffset(
    nsIFrame* aContainerFrame, nsDisplayListBuilder* aBuilder) {
  nscoord auPerDevPixel = aContainerFrame->PresContext()->AppUnitsPerDevPixel();

  // Offset to the content rect in case we have borders or padding
  // Note that aContainerFrame could be a reference frame itself, so
  // we need to be careful here to ensure that we call ToReferenceFrame
  // on aContainerFrame and not its parent.
  nsPoint frameOffset =
      aBuilder->ToReferenceFrame(aContainerFrame) +
      aContainerFrame->GetContentRectRelativeToSelf().TopLeft();

  return mozilla::LayoutDeviceIntPoint::FromAppUnitsToNearest(frameOffset,
                                                              auPerDevPixel);
}

// Return true iff |aManager| is a "temporary layer manager".  They're
// used for small software rendering tasks, like drawWindow.  That's
// currently implemented by a BasicLayerManager without a backing
// widget, and hence in non-retained mode.
inline static bool IsTempLayerManager(LayerManager* aManager) {
  return (LayersBackend::LAYERS_BASIC == aManager->GetBackendType() &&
          !static_cast<BasicLayerManager*>(aManager)->IsRetained());
}

nsDisplayRemote::nsDisplayRemote(nsDisplayListBuilder* aBuilder,
                                 nsSubDocumentFrame* aFrame)
    : nsPaintedDisplayItem(aBuilder, aFrame),
      mTabId{0},
      mEventRegionsOverride(EventRegionsOverride::NoOverride) {
  bool frameIsPointerEventsNone = aFrame->StyleUI()->GetEffectivePointerEvents(
                                      aFrame) == NS_STYLE_POINTER_EVENTS_NONE;
  if (aBuilder->IsInsidePointerEventsNoneDoc() || frameIsPointerEventsNone) {
    mEventRegionsOverride |= EventRegionsOverride::ForceEmptyHitRegion;
  }
  if (nsLayoutUtils::HasDocumentLevelListenersForApzAwareEvents(
          aFrame->PresShell())) {
    mEventRegionsOverride |= EventRegionsOverride::ForceDispatchToContent;
  }

  nsFrameLoader* frameLoader = GetFrameLoader();
  MOZ_ASSERT(frameLoader && frameLoader->IsRemoteFrame());
  if (frameLoader->GetRemoteBrowser()) {
    mLayersId = frameLoader->GetLayersId();
    mTabId = frameLoader->GetRemoteBrowser()->GetTabId();
  }
}

mozilla::LayerState nsDisplayRemote::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  if (IsTempLayerManager(aManager)) {
    return mozilla::LayerState::LAYER_NONE;
  }
  return mozilla::LayerState::LAYER_ACTIVE_FORCE;
}

LayerIntRect GetFrameRect(const nsIFrame* aFrame) {
  LayoutDeviceRect rect = LayoutDeviceRect::FromAppUnits(
      aFrame->GetContentRectRelativeToSelf(),
      aFrame->PresContext()->AppUnitsPerDevPixel());
  return RoundedOut(rect * LayoutDeviceToLayerScale(
                               aFrame->PresShell()->GetCumulativeResolution()));
}

already_AddRefed<mozilla::layers::Layer> nsDisplayRemote::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  MOZ_ASSERT(mFrame, "Makes no sense to have a shadow tree without a frame");

  if (IsTempLayerManager(aManager)) {
    // This can happen if aManager is a "temporary" manager, or if the
    // widget's layer manager changed out from under us.  We need to
    // FIXME handle the former case somehow, probably with an API to
    // draw a manager's subtree.  The latter is bad bad bad, but the the
    // MOZ_ASSERT() above will flag it.  Returning nullptr here will just
    // cause the shadow subtree not to be rendered.
    NS_WARNING("Remote iframe not rendered");
    return nullptr;
  }

  if (!mLayersId.IsValid()) {
    return nullptr;
  }

  if (RefPtr<RemoteBrowser> remoteBrowser =
          GetFrameLoader()->GetRemoteBrowser()) {
    // Adjust mItemVisibleRect, which is relative to the reference frame, to be
    // relative to this frame
    nsRect visibleRect;
    if (aContainerParameters.mItemVisibleRect) {
      visibleRect = *aContainerParameters.mItemVisibleRect;
    } else {
      visibleRect = GetBuildingRect();
    }
    visibleRect -= ToReferenceFrame();

    // Generate an effects update notifying the browser it is visible
    aBuilder->AddEffectUpdate(remoteBrowser,
                              EffectsInfo::VisibleWithinRect(
                                  visibleRect, aContainerParameters.mXScale,
                                  aContainerParameters.mYScale));
    // FrameLayerBuilder will take care of notifying the browser when it is no
    // longer visible
  }

  RefPtr<Layer> layer =
      aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this);

  if (!layer) {
    layer = aManager->CreateRefLayer();
  }
  if (!layer || !layer->AsRefLayer()) {
    // Probably a temporary layer manager that doesn't know how to
    // use ref layers.
    return nullptr;
  }
  RefLayer* refLayer = layer->AsRefLayer();

  LayoutDeviceIntPoint offset = GetContentRectLayerOffset(Frame(), aBuilder);
  // We can only have an offset if we're a child of an inactive
  // container, but our display item is LAYER_ACTIVE_FORCE which
  // forces all layers above to be active.
  MOZ_ASSERT(aContainerParameters.mOffset == nsIntPoint());
  Matrix4x4 m = Matrix4x4::Translation(offset.x, offset.y, 0.0);
  // Remote content can't be repainted by us, so we multiply down
  // the resolution that our container expects onto our container.
  m.PreScale(aContainerParameters.mXScale, aContainerParameters.mYScale, 1.0);
  refLayer->SetBaseTransform(m);
  refLayer->SetEventRegionsOverride(mEventRegionsOverride);
  refLayer->SetReferentId(mLayersId);
  refLayer->SetRemoteDocumentRect(GetFrameRect(mFrame));

  return layer.forget();
}

void nsDisplayRemote::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  DrawTarget* target = aCtx->GetDrawTarget();
  if (!target->IsRecording() || mTabId == 0) {
    NS_WARNING("Remote iframe not rendered");
    return;
  }

  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  Rect destRect = mozilla::NSRectToSnappedRect(GetContentRect(),
                                               appUnitsPerDevPixel, *target);
  target->DrawDependentSurface(mTabId, destRect);
}

bool nsDisplayRemote::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (!mLayersId.IsValid()) {
    return true;
  }

  if (RefPtr<RemoteBrowser> remoteBrowser =
          GetFrameLoader()->GetRemoteBrowser()) {
    // Adjust mItemVisibleRect, which is relative to the reference frame, to be
    // relative to this frame
    nsRect visibleRect = GetBuildingRect() - ToReferenceFrame();

    // Generate an effects update notifying the browser it is visible
    // TODO - Gather scaling factors
    aDisplayListBuilder->AddEffectUpdate(
        remoteBrowser, EffectsInfo::VisibleWithinRect(visibleRect, 1.0f, 1.0f));

    // Create a WebRenderRemoteData to notify the RemoteBrowser when it is no
    // longer visible
    RefPtr<WebRenderRemoteData> userData =
        aManager->CommandBuilder()
            .CreateOrRecycleWebRenderUserData<WebRenderRemoteData>(
                this, aBuilder.GetRenderRoot(), nullptr);
    userData->SetRemoteBrowser(remoteBrowser);
  }

  mOffset = GetContentRectLayerOffset(mFrame, aDisplayListBuilder);

  LayoutDeviceRect rect = LayoutDeviceRect::FromAppUnits(
      mFrame->GetContentRectRelativeToSelf(),
      mFrame->PresContext()->AppUnitsPerDevPixel());
  rect += mOffset;

  aBuilder.PushIFrame(mozilla::wr::ToLayoutRect(rect), !BackfaceIsHidden(),
                      mozilla::wr::AsPipelineId(mLayersId),
                      /*ignoreMissingPipelines*/ true);

  return true;
}

bool nsDisplayRemote::UpdateScrollData(
    mozilla::layers::WebRenderScrollData* aData,
    mozilla::layers::WebRenderLayerScrollData* aLayerData) {
  if (!mLayersId.IsValid()) {
    return true;
  }

  if (aLayerData) {
    aLayerData->SetReferentId(mLayersId);
    aLayerData->SetTransform(
        mozilla::gfx::Matrix4x4::Translation(mOffset.x, mOffset.y, 0.0));
    aLayerData->SetEventRegionsOverride(mEventRegionsOverride);
    aLayerData->SetRemoteDocumentRect(GetFrameRect(mFrame));
  }
  return true;
}

nsFrameLoader* nsDisplayRemote::GetFrameLoader() const {
  return mFrame ? static_cast<nsSubDocumentFrame*>(mFrame)->FrameLoader()
                : nullptr;
}
