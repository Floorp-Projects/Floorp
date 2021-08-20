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
#include "nsStyleStruct.h"
#include "nsStyleStructInlines.h"
#include "nsFrameSetFrame.h"
#include "nsNameSpaceManager.h"
#include "nsDisplayList.h"
#include "nsIScrollableFrame.h"
#include "nsIObjectLoadingContent.h"
#include "nsLayoutUtils.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsQueryObject.h"
#include "RetainedDisplayListBuilder.h"
#include "nsObjectLoadingContent.h"

#include "Layers.h"
#include "BasicLayers.h"
#include "mozilla/layers/WebRenderUserData.h"
#include "mozilla/layers/WebRenderScrollData.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/StackingContextHelper.h"  // for StackingContextHelper
#include "mozilla/ProfilerLabels.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;

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

  RefPtr<nsFrameLoader> frameloader = FrameLoader();
  if (!frameloader) {
    return;
  }

  if (!frameloader->IsRemoteFrame() && !PresContext()->IsDynamic()) {
    // We let the printing code take care of loading the document and
    // initializing the shell; just create the inner view for it to use.
    (void)EnsureInnerView();
  } else {
    AutoWeakFrame weakThis(this);
    mCallingShow = true;
    bool didCreateDoc = frameloader->Show(this);
    if (!weakThis.IsAlive()) {
      return;
    }
    mCallingShow = false;
    mDidCreateDoc = didCreateDoc;

    if (!HasAnyStateBits(NS_FRAME_FIRST_REFLOW)) {
      frameloader->UpdatePositionAndSize(this);
    }

    if (!weakThis.IsAlive()) {
      return;
    }
    InvalidateFrame();
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
  if (HasAnyStateBits(NS_FRAME_FIRST_REFLOW)) {
    if (RefPtr<nsFrameLoader> frameloader = FrameLoader()) {
      nsCOMPtr<Document> oldContainerDoc;
      nsIFrame* detachedFrame =
          frameloader->GetDetachedSubdocFrame(getter_AddRefs(oldContainerDoc));
      if (nsView* view = detachedFrame ? detachedFrame->GetView() : nullptr) {
        nsSize size = view->GetBounds().Size();
        nsPresContext* presContext = detachedFrame->PresContext();
        return ScreenIntSize(presContext->AppUnitsToDevPixels(size.width),
                             presContext->AppUnitsToDevPixels(size.height));
      }
    }
    // Pick some default size for now.  Using 10x10 because that's what the
    // code used to do.
    return ScreenIntSize(10, 10);
  }

  nsSize docSizeAppUnits;
  nsPresContext* presContext = PresContext();
  if (GetContent()->IsHTMLElement(nsGkAtoms::frame)) {
    docSizeAppUnits = GetSize();
  } else {
    docSizeAppUnits = GetContentRect().Size();
  }

  // Adjust subdocument size, according to 'object-fit' and the subdocument's
  // intrinsic size and ratio.
  docSizeAppUnits = nsLayoutUtils::ComputeObjectDestRect(
                        nsRect(nsPoint(), docSizeAppUnits), GetIntrinsicSize(),
                        GetIntrinsicRatio(), StylePosition())
                        .Size();

  return ScreenIntSize(
      presContext->AppUnitsToDevPixels(docSizeAppUnits.width),
      presContext->AppUnitsToDevPixels(docSizeAppUnits.height));
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
      item = MakeDisplayItemWithIndex<nsDisplayOwnLayer>(
          aBuilder, aFrame, /* aIndex = */ nsDisplayOwnLayer::OwnLayerForSubdoc,
          &tmpList, aBuilder->CurrentActiveScrolledRoot(),
          nsDisplayOwnLayerFlags::None, ScrollbarData{}, true, false);
    }
    if (item) {
      tempItems.AppendToTop(item);
    }
  }
  aList->AppendToTop(&tempItems);
}

void nsSubDocumentFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                          const nsDisplayListSet& aLists) {
  if (!IsVisibleForPainting()) {
    return;
  }

  nsFrameLoader* frameLoader = FrameLoader();
  bool isRemoteFrame = frameLoader && frameLoader->IsRemoteFrame();

  // If we are pointer-events:none then we don't need to HitTest background
  bool pointerEventsNone =
      StyleUI()->mPointerEvents == StylePointerEvents::None;
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
    clipState.ClipContainingBlockDescendantsToContentBox(aBuilder, this);

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
  bool ignoreViewportScrolling = false;
  if (subdocRootFrame) {
    // get the dirty rect relative to the root frame of the subdoc
    visible = aBuilder->GetVisibleRect() + GetOffsetToCrossDoc(subdocRootFrame);
    dirty = aBuilder->GetDirtyRect() + GetOffsetToCrossDoc(subdocRootFrame);
    // and convert into the appunits of the subdoc
    visible = visible.ScaleToOtherAppUnitsRoundOut(parentAPD, subdocAPD);
    dirty = dirty.ScaleToOtherAppUnitsRoundOut(parentAPD, subdocAPD);

    if (nsIScrollableFrame* rootScrollableFrame =
            presShell->GetRootScrollFrameAsScrollable()) {
      // Use a copy, so the rects don't get modified.
      nsRect copyOfDirty = dirty;
      nsRect copyOfVisible = visible;
      // TODO(botond): Can we just axe this DecideScrollableLayer call?
      rootScrollableFrame->DecideScrollableLayer(aBuilder, &copyOfVisible,
                                                 &copyOfDirty,
                                                 /* aSetBase = */ true);

      ignoreViewportScrolling = presShell->IgnoringViewportScrolling();
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
  bool constructZoomItem = subdocRootFrame && parentAPD != subdocAPD;
  bool needsOwnLayer = false;
  if (constructZoomItem || presContext->IsRootContentDocument() ||
      (sf && sf->IsScrollingActive())) {
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
    if (ignoreViewportScrolling) {
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

  // We always want top level content documents to be in their own layer.
  nsDisplaySubDocument* layerItem = MakeDisplayItem<nsDisplaySubDocument>(
      aBuilder, subdocRootFrame ? subdocRootFrame : this, this, &childItems,
      flags);
  if (layerItem) {
    childItems.AppendToTop(layerItem);
    layerItem->SetShouldFlattenAway(!needsOwnLayer);
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

#ifdef DEBUG_FRAME_DUMP
void nsSubDocumentFrame::List(FILE* out, const char* aPrefix,
                              ListFlags aFlags) const {
  nsCString str;
  ListGeneric(str, aPrefix, aFlags);
  fprintf_stderr(out, "%s\n", str.get());

  if (aFlags.contains(ListFlag::TraverseSubdocumentFrames)) {
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
  return MakeFrameName(u"FrameOuter"_ns, aResult);
}
#endif

/* virtual */
nscoord nsSubDocumentFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_MIN_INLINE_SIZE(this, result);

  nsCOMPtr<nsIObjectLoadingContent> iolc = do_QueryInterface(mContent);
  auto olc = static_cast<nsObjectLoadingContent*>(iolc.get());

  if (olc && olc->GetSubdocumentIntrinsicSize()) {
    // The subdocument is an SVG document, so technically we should call
    // SVGOuterSVGFrame::GetMinISize() on its root frame.  That method always
    // returns 0, though, so we can just do that & don't need to bother with
    // the cross-doc communication.
    result = 0;
  } else {
    result = GetIntrinsicISize();
  }

  return result;
}

/* virtual */
nscoord nsSubDocumentFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_PREF_INLINE_SIZE(this, result);

  // If the subdocument is an SVG document, then in theory we want to return
  // the same thing that SVGOuterSVGFrame::GetPrefISize does.  That method
  // has some special handling of percentage values to avoid unhelpful zero
  // sizing in the presence of orthogonal writing modes.  We don't bother
  // with that for SVG documents in <embed> and <object>, since that special
  // handling doesn't look up across document boundaries anyway.
  result = GetIntrinsicISize();

  return result;
}

/* virtual */
IntrinsicSize nsSubDocumentFrame::GetIntrinsicSize() {
  if (StyleDisplay()->IsContainSize()) {
    // Intrinsic size of 'contain:size' replaced elements is 0,0.
    return IntrinsicSize(0, 0);
  }

  if (nsCOMPtr<nsIObjectLoadingContent> iolc = do_QueryInterface(mContent)) {
    auto olc = static_cast<nsObjectLoadingContent*>(iolc.get());

    if (auto size = olc->GetSubdocumentIntrinsicSize()) {
      // Use the intrinsic size from the child SVG document, if available.
      return *size;
    }
  }

  if (!IsInline()) {
    return {};  // <frame> elements have no useful intrinsic size.
  }

  if (mContent->IsXULElement()) {
    return {};  // XUL <iframe> and <browser> have no useful intrinsic size
  }

  // We must be an HTML <iframe>. Return fallback size.
  return IntrinsicSize(kFallbackIntrinsicSize);
}

/* virtual */
AspectRatio nsSubDocumentFrame::GetIntrinsicRatio() const {
  // FIXME(emilio): This should probably respect contain: size and return no
  // ratio in the case subDocRoot is non-null. Otherwise we do it by virtue of
  // using a zero-size below and reusing GetIntrinsicSize().
  if (nsCOMPtr<nsIObjectLoadingContent> iolc = do_QueryInterface(mContent)) {
    auto olc = static_cast<nsObjectLoadingContent*>(iolc.get());

    auto ratio = olc->GetSubdocumentIntrinsicRatio();
    if (ratio && *ratio) {
      // Use the intrinsic aspect ratio from the child SVG document, if
      // available.
      return *ratio;
    }
  }

  // NOTE(emilio): Even though we have an intrinsic size, we may not have an
  // intrinsic ratio. For example `<iframe style="width: 100px">` should not
  // shrink in the vertical axis to preserve the 300x150 ratio.
  return nsAtomicContainerFrame::GetIntrinsicRatio();
}

/* virtual */
LogicalSize nsSubDocumentFrame::ComputeAutoSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorderPadding, const StyleSizeOverrides& aSizeOverrides,
    ComputeSizeFlags aFlags) {
  if (!IsInline()) {
    return nsIFrame::ComputeAutoSize(aRenderingContext, aWM, aCBSize,
                                     aAvailableISize, aMargin, aBorderPadding,
                                     aSizeOverrides, aFlags);
  }

  const WritingMode wm = GetWritingMode();
  LogicalSize result(wm, GetIntrinsicISize(), GetIntrinsicBSize());
  return result.ConvertTo(aWM, wm);
}

/* virtual */
nsIFrame::SizeComputationResult nsSubDocumentFrame::ComputeSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorderPadding, const StyleSizeOverrides& aSizeOverrides,
    ComputeSizeFlags aFlags) {
  return {ComputeSizeWithIntrinsicDimensions(
              aRenderingContext, aWM, GetIntrinsicSize(), GetAspectRatio(),
              aCBSize, aMargin, aBorderPadding, aSizeOverrides, aFlags),
          AspectRatioUsage::None};
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
  const auto wm = aReflowInput.GetWritingMode();
  aDesiredSize.SetSize(wm, aReflowInput.ComputedSizeWithBorderPadding(wm));

  // "offset" is the offset of our content area from our frame's
  // top-left corner.
  nsPoint offset = nsPoint(aReflowInput.ComputedPhysicalBorderPadding().left,
                           aReflowInput.ComputedPhysicalBorderPadding().top);

  if (mInnerView) {
    const nsMargin& bp = aReflowInput.ComputedPhysicalBorderPadding();
    nsSize innerSize(aDesiredSize.Width() - bp.LeftRight(),
                     aDesiredSize.Height() - bp.TopBottom());

    // Size & position the view according to 'object-fit' & 'object-position'.
    nsRect destRect = nsLayoutUtils::ComputeObjectDestRect(
        nsRect(offset, innerSize), GetIntrinsicSize(), GetIntrinsicRatio(),
        StylePosition());

    nsViewManager* vm = mInnerView->GetViewManager();
    vm->MoveViewTo(mInnerView, destRect.x, destRect.y);
    vm->ResizeView(mInnerView, nsRect(nsPoint(0, 0), destRect.Size()), true);
  }

  aDesiredSize.SetOverflowAreasToDesiredBounds();

  FinishAndStoreOverflow(&aDesiredSize);

  if (!aPresContext->IsRootPaginatedDocument() && !mPostedReflowCallback) {
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
  } else if (aAttribute == nsGkAtoms::marginwidth ||
             aAttribute == nsGkAtoms::marginheight) {
    // Notify the frameloader
    if (RefPtr<nsFrameLoader> frameloader = FrameLoader()) {
      frameloader->MarginsChanged();
    }
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
  PropagateIsUnderHiddenEmbedderElementToSubView(true);
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

nsFrameLoader* nsSubDocumentFrame::FrameLoader() const {
  if (mFrameLoader) {
    return mFrameLoader;
  }

  if (RefPtr<nsFrameLoaderOwner> loaderOwner = do_QueryObject(GetContent())) {
    mFrameLoader = loaderOwner->GetFrameLoader();
  }

  return mFrameLoader;
}

auto nsSubDocumentFrame::GetRemotePaintData() const -> RemoteFramePaintData {
  if (mRetainedRemoteFrame) {
    return *mRetainedRemoteFrame;
  }

  RemoteFramePaintData data;
  nsFrameLoader* fl = FrameLoader();
  if (!fl) {
    return data;
  }

  auto* rb = fl->GetRemoteBrowser();
  if (!rb) {
    return data;
  }
  data.mLayersId = rb->GetLayersId();
  data.mTabId = rb->GetTabId();
  return data;
}

void nsSubDocumentFrame::ResetFrameLoader(RetainPaintData aRetain) {
  if (aRetain == RetainPaintData::Yes && mFrameLoader) {
    mRetainedRemoteFrame = Some(GetRemotePaintData());
  } else {
    mRetainedRemoteFrame.reset();
  }
  mFrameLoader = nullptr;
  ClearDisplayItems();
  nsContentUtils::AddScriptRunner(new AsyncFrameInit(this));
}

void nsSubDocumentFrame::ClearRetainedPaintData() {
  mRetainedRemoteFrame.reset();
  ClearDisplayItems();
  InvalidateFrameSubtree();
}

// XXX this should be called ObtainDocShell or something like that,
// to indicate that it could have side effects
nsIDocShell* nsSubDocumentFrame::GetDocShell() const {
  // How can FrameLoader() return null???
  if (NS_WARN_IF(!FrameLoader())) {
    return nullptr;
  }
  return mFrameLoader->GetDocShell(IgnoreErrors());
}

static void DestroyDisplayItemDataForFrames(nsIFrame* aFrame) {
  for (const auto& childList : aFrame->ChildLists()) {
    for (nsIFrame* child : childList.mList) {
      DestroyDisplayItemDataForFrames(child);
    }
  }
}

static CallState BeginSwapDocShellsForDocument(Document& aDocument) {
  if (PresShell* presShell = aDocument.GetPresShell()) {
    // Disable painting while the views are detached, see bug 946929.
    presShell->SetNeverPainting(true);

    if (nsIFrame* rootFrame = presShell->GetRootFrame()) {
      ::DestroyDisplayItemDataForFrames(rootFrame);
    }
  }
  aDocument.EnumerateSubDocuments(BeginSwapDocShellsForDocument);
  return CallState::Continue;
}

static nsView* BeginSwapDocShellsForViews(nsView* aSibling) {
  // Collect the removed sibling views in reverse order in 'removedViews'.
  nsView* removedViews = nullptr;
  while (aSibling) {
    if (Document* doc = ::GetDocumentFromView(aSibling)) {
      ::BeginSwapDocShellsForDocument(*doc);
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

static CallState EndSwapDocShellsForDocument(Document& aDocument) {
  // Our docshell and view trees have been updated for the new hierarchy.
  // Now also update all nsDeviceContext::mWidget to that of the
  // container view in the new hierarchy.
  if (nsCOMPtr<nsIDocShell> ds = aDocument.GetDocShell()) {
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

  aDocument.EnumerateSubDocuments(EndSwapDocShellsForDocument);
  return CallState::Continue;
}

static void EndSwapDocShellsForViews(nsView* aSibling) {
  for (; aSibling; aSibling = aSibling->GetNextSibling()) {
    if (Document* doc = ::GetDocumentFromView(aSibling)) {
      ::EndSwapDocShellsForDocument(*doc);
    }
    nsIFrame* frame = aSibling->GetFrame();
    if (frame) {
      nsIFrame* parent = nsLayoutUtils::GetCrossDocParentFrameInProcess(frame);
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
          parent = nsLayoutUtils::GetCrossDocParentFrameInProcess(parent);
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
  for (nsDisplayItem* i : DisplayItems()) {
    if (i->GetType() == DisplayItemType::TYPE_SUBDOCUMENT) {
      nsIFrame* displayRoot = nsLayoutUtils::GetDisplayRootFrame(this);
      MOZ_ASSERT(displayRoot);

      RetainedDisplayListBuilder* retainedBuilder =
          displayRoot->GetProperty(RetainedDisplayListBuilder::Cached());
      MOZ_ASSERT(retainedBuilder);

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

void nsSubDocumentFrame::SubdocumentIntrinsicSizeOrRatioChanged() {
  if (MOZ_UNLIKELY(HasAllStateBits(NS_FRAME_IS_DIRTY))) {
    // We will be reflowed soon anyway.
    return;
  }

  const nsStylePosition* pos = StylePosition();
  bool dependsOnIntrinsics =
      !pos->mWidth.ConvertsToLength() || !pos->mHeight.ConvertsToLength();

  if (dependsOnIntrinsics || pos->mObjectFit != StyleObjectFit::Fill) {
    auto dirtyHint = dependsOnIntrinsics ? IntrinsicDirty::StyleChange
                                         : IntrinsicDirty::Resize;
    PresShell()->FrameNeedsReflow(this, dirtyHint, NS_FRAME_IS_DIRTY);
  }
}

/**
 * Gets the layer-pixel offset of aContainerFrame's content rect top-left
 * from the nearest display item reference frame (which we assume will be
 * inducing a ContainerLayer).
 */
static nsPoint GetContentRectLayerOffset(nsIFrame* aContainerFrame,
                                         nsDisplayListBuilder* aBuilder) {
  // Offset to the content rect in case we have borders or padding
  // Note that aContainerFrame could be a reference frame itself, so
  // we need to be careful here to ensure that we call ToReferenceFrame
  // on aContainerFrame and not its parent.
  nsPoint frameOffset =
      aBuilder->ToReferenceFrame(aContainerFrame) +
      aContainerFrame->GetContentRectRelativeToSelf().TopLeft();

  return frameOffset;
}

// Return true iff |aManager| is a "temporary layer manager".  They're
// used for small software rendering tasks, like drawWindow.  That's
// currently implemented by a BasicLayerManager without a backing
// widget, and hence in non-retained mode.
nsDisplayRemote::nsDisplayRemote(nsDisplayListBuilder* aBuilder,
                                 nsSubDocumentFrame* aFrame)
    : nsPaintedDisplayItem(aBuilder, aFrame),
      mEventRegionsOverride(EventRegionsOverride::NoOverride) {
  bool frameIsPointerEventsNone = aFrame->StyleUI()->GetEffectivePointerEvents(
                                      aFrame) == StylePointerEvents::None;
  if (aBuilder->IsInsidePointerEventsNoneDoc() || frameIsPointerEventsNone) {
    mEventRegionsOverride |= EventRegionsOverride::ForceEmptyHitRegion;
  }
  if (nsLayoutUtils::HasDocumentLevelListenersForApzAwareEvents(
          aFrame->PresShell())) {
    mEventRegionsOverride |= EventRegionsOverride::ForceDispatchToContent;
  }

  mPaintData = aFrame->GetRemotePaintData();
}

LayerIntSize GetFrameSize(const nsIFrame* aFrame) {
  LayoutDeviceSize size = LayoutDeviceRect::FromAppUnits(
      aFrame->GetContentRectRelativeToSelf().Size(),
      aFrame->PresContext()->AppUnitsPerDevPixel());

  float cumulativeResolution = aFrame->PresShell()->GetCumulativeResolution();
  return LayerIntSize::Round(size.width * cumulativeResolution,
                             size.height * cumulativeResolution);
}

namespace mozilla {

void nsDisplayRemote::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  nsPresContext* pc = mFrame->PresContext();
  nsFrameLoader* fl = GetFrameLoader();
  if (pc->GetPrintSettings() && fl->IsRemoteFrame()) {
    // See the comment below in CreateWebRenderCommands() as for why doing this.
    fl->UpdatePositionAndSize(static_cast<nsSubDocumentFrame*>(mFrame));
  }

  DrawTarget* target = aCtx->GetDrawTarget();
  if (!target->IsRecording() || mPaintData.mTabId == 0) {
    NS_WARNING("Remote iframe not rendered");
    return;
  }

  // Rendering the inner document will apply a scale to account for its
  // app units per dev pixel ratio. We want to apply the inverse scaling
  // using our app units per dev pixel ratio, so that no actual scaling
  // will be applied if they match. For in-process rendering,
  // nsSubDocumentFrame creates an nsDisplayZoom item if the app units
  // per dev pixel ratio changes.
  int32_t appUnitsPerDevPixel = pc->AppUnitsPerDevPixel();
  gfxFloat scale = gfxFloat(AppUnitsPerCSSPixel()) / appUnitsPerDevPixel;
  gfxContextMatrixAutoSaveRestore saveMatrix(aCtx);
  aCtx->Multiply(gfxMatrix::Scaling(scale, scale));

  Rect destRect =
      NSRectToSnappedRect(GetContentRect(), AppUnitsPerCSSPixel(), *target);
  target->DrawDependentSurface(mPaintData.mTabId, destRect);
}

bool nsDisplayRemote::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (!mPaintData.mLayersId.IsValid()) {
    return true;
  }

  nsPresContext* pc = mFrame->PresContext();
  nsFrameLoader* fl = GetFrameLoader();
  if (RefPtr<RemoteBrowser> remoteBrowser = fl->GetRemoteBrowser()) {
    if (pc->GetPrintSettings()) {
      // HACK(emilio): Usually we update sizing/positioning from
      // ReflowFinished(). Print documents have no incremental reflow at all
      // though, so we can't rely on it firing after a frame becomes remote.
      // Thus, if we're painting a remote frame, update its sizing and position
      // now.
      //
      // UpdatePositionAndSize() can cause havoc for non-remote frames but
      // luckily we don't care about those, so this is fine.
      fl->UpdatePositionAndSize(static_cast<nsSubDocumentFrame*>(mFrame));
    }

    // Adjust mItemVisibleRect, which is relative to the reference frame, to be
    // relative to this frame
    nsRect visibleRect = GetBuildingRect() - ToReferenceFrame();
    nsRect contentRect = Frame()->GetContentRectRelativeToSelf();
    visibleRect.IntersectRect(visibleRect, contentRect);
    visibleRect -= contentRect.TopLeft();

    // Generate an effects update notifying the browser it is visible
    gfx::Size scale = aSc.GetInheritedScale();
    aDisplayListBuilder->AddEffectUpdate(
        remoteBrowser,
        EffectsInfo::VisibleWithinRect(visibleRect, scale.width, scale.height));

    // Create a WebRenderRemoteData to notify the RemoteBrowser when it is no
    // longer visible
    RefPtr<WebRenderRemoteData> userData =
        aManager->CommandBuilder()
            .CreateOrRecycleWebRenderUserData<WebRenderRemoteData>(this,
                                                                   nullptr);
    userData->SetRemoteBrowser(remoteBrowser);
  }

  nscoord auPerDevPixel = pc->AppUnitsPerDevPixel();
  nsPoint layerOffset = GetContentRectLayerOffset(mFrame, aDisplayListBuilder);
  mOffset = LayoutDevicePoint::FromAppUnits(layerOffset, auPerDevPixel);

  nsRect contentRect = mFrame->GetContentRectRelativeToSelf();
  contentRect.MoveTo(0, 0);
  LayoutDeviceRect rect =
      LayoutDeviceRect::FromAppUnits(contentRect, auPerDevPixel);
  rect += mOffset;

  aBuilder.PushIFrame(mozilla::wr::ToLayoutRect(rect), !BackfaceIsHidden(),
                      mozilla::wr::AsPipelineId(mPaintData.mLayersId),
                      /*ignoreMissingPipelines*/ true);

  return true;
}

bool nsDisplayRemote::UpdateScrollData(
    mozilla::layers::WebRenderScrollData* aData,
    mozilla::layers::WebRenderLayerScrollData* aLayerData) {
  if (!mPaintData.mLayersId.IsValid()) {
    return true;
  }

  if (aLayerData) {
    aLayerData->SetReferentId(mPaintData.mLayersId);

    // Apply the top level resolution if we are in the same process of the top
    // level document. We don't need to apply it in cases where we are in OOP
    // iframes since it will be applied later in
    // HitTestingTreeNode::GetTransformToGecko by walking up the tree node.
    nsPresContext* inProcessRootContext =
        mFrame->PresContext()->GetInProcessRootContentDocumentPresContext();
    if (inProcessRootContext &&
        inProcessRootContext->IsRootContentDocumentCrossProcess()) {
      float resolution = inProcessRootContext->PresShell()->GetResolution();
      aLayerData->SetResolution(resolution);
    }

    Matrix4x4 m = Matrix4x4::Translation(mOffset.x, mOffset.y, 0.0);
    aLayerData->SetTransform(m);
    aLayerData->SetEventRegionsOverride(mEventRegionsOverride);
    aLayerData->SetRemoteDocumentSize(GetFrameSize(mFrame));
  }
  return true;
}

nsFrameLoader* nsDisplayRemote::GetFrameLoader() const {
  return static_cast<nsSubDocumentFrame*>(mFrame)->FrameLoader();
}

}  // namespace mozilla
