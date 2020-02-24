/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object that goes directly inside the document's scrollbars */

#include "nsCanvasFrame.h"

#include "gfxContext.h"
#include "gfxUtils.h"
#include "nsContainerFrame.h"
#include "nsContentCreatorFunctions.h"
#include "nsCSSRendering.h"
#include "nsPresContext.h"
#include "nsPopupSetFrame.h"
#include "nsGkAtoms.h"
#include "nsIFrameInlines.h"
#include "nsDisplayList.h"
#include "nsCSSFrameConstructor.h"
#include "nsFrameManager.h"
#include "gfxPlatform.h"
#include "nsPrintfCString.h"
#include "mozilla/AccessibleCaretEventHub.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/dom/AnonymousContent.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/PresShell.h"
// for focus
#include "nsIScrollableFrame.h"
#ifdef DEBUG_CANVAS_FOCUS
#  include "nsIDocShell.h"
#endif

//#define DEBUG_CANVAS_FOCUS

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layout;
using namespace mozilla::gfx;
using namespace mozilla::layers;

nsCanvasFrame* NS_NewCanvasFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsCanvasFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsCanvasFrame)

NS_QUERYFRAME_HEAD(nsCanvasFrame)
  NS_QUERYFRAME_ENTRY(nsCanvasFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsIPopupContainer)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

void nsCanvasFrame::ShowCustomContentContainer() {
  if (mCustomContentContainer) {
    mCustomContentContainer->UnsetAttr(kNameSpaceID_None, nsGkAtoms::hidden,
                                       true);
  }
}

void nsCanvasFrame::HideCustomContentContainer() {
  if (mCustomContentContainer) {
    mCustomContentContainer->SetAttr(kNameSpaceID_None, nsGkAtoms::hidden,
                                     NS_LITERAL_STRING("true"), true);
  }
}

nsresult nsCanvasFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  MOZ_ASSERT(!mCustomContentContainer);

  if (!mContent) {
    return NS_OK;
  }

  nsCOMPtr<Document> doc = mContent->OwnerDoc();

  RefPtr<AccessibleCaretEventHub> eventHub =
      PresShell()->GetAccessibleCaretEventHub();

  // This will go through InsertAnonymousContent and such, and we don't really
  // want it to end up inserting into our content container.
  //
  // FIXME(emilio): The fact that this enters into InsertAnonymousContent is a
  // bit nasty, can we avoid it, maybe doing this off a scriptrunner?
  if (eventHub) {
    eventHub->Init();
  }

  // Create the custom content container.
  mCustomContentContainer = doc->CreateHTMLElement(nsGkAtoms::div);
#ifdef DEBUG
  // We restyle our mCustomContentContainer, even though it's root anonymous
  // content.  Normally that's not OK because the frame constructor doesn't know
  // how to order the frame tree in such cases, but we make this work for this
  // particular case, so it's OK.
  mCustomContentContainer->SetProperty(nsGkAtoms::restylableAnonymousNode,
                                       reinterpret_cast<void*>(true));
#endif  // DEBUG

  mCustomContentContainer->SetProperty(
      nsGkAtoms::docLevelNativeAnonymousContent, reinterpret_cast<void*>(true));

  // This will usually be done by the caller, but in this case we do it here,
  // since we reuse the document's AnoymousContent list, and those survive
  // across reframes and thus may already be flagged as being in an anonymous
  // subtree. We don't really want to have this semi-broken state where
  // anonymous nodes have a non-anonymous.
  mCustomContentContainer->SetIsNativeAnonymousRoot();

  aElements.AppendElement(mCustomContentContainer);

  // Do not create an accessible object for the container.
  mCustomContentContainer->SetAttr(kNameSpaceID_None, nsGkAtoms::role,
                                   NS_LITERAL_STRING("presentation"), false);

  mCustomContentContainer->SetAttr(
      kNameSpaceID_None, nsGkAtoms::_class,
      NS_LITERAL_STRING("moz-custom-content-container"), false);

  // Only create a frame for mCustomContentContainer if it has some children.
  if (doc->GetAnonymousContents().IsEmpty()) {
    HideCustomContentContainer();
  }

  for (RefPtr<AnonymousContent>& anonContent : doc->GetAnonymousContents()) {
    if (nsCOMPtr<nsINode> parent = anonContent->ContentNode().GetParentNode()) {
      // Parent had better be an old custom content container already removed
      // from a reframe. Forget about it since we're about to get inserted in a
      // new one.
      //
      // TODO(emilio): Maybe we should extend PostDestroyData and do this stuff
      // there instead, or something...
      MOZ_ASSERT(parent != mCustomContentContainer);
      MOZ_ASSERT(parent->IsElement());
      MOZ_ASSERT(parent->AsElement()->IsRootOfNativeAnonymousSubtree());
      MOZ_ASSERT(!parent->IsInComposedDoc());
      MOZ_ASSERT(!parent->GetParentNode());

      parent->RemoveChildNode(&anonContent->ContentNode(), false);
    }

    mCustomContentContainer->AppendChildTo(&anonContent->ContentNode(), false);
  }

  // Create a popupgroup element for chrome privileged top level non-XUL
  // documents to support context menus and tooltips.
  if (PresContext()->IsChrome() && PresContext()->IsRoot() &&
      doc->AllowXULXBL()) {
    nsNodeInfoManager* nodeInfoManager = doc->NodeInfoManager();
    RefPtr<NodeInfo> nodeInfo =
        nodeInfoManager->GetNodeInfo(nsGkAtoms::popupgroup, nullptr,
                                     kNameSpaceID_XUL, nsINode::ELEMENT_NODE);

    nsresult rv = NS_NewXULElement(getter_AddRefs(mPopupgroupContent),
                                   nodeInfo.forget(), dom::NOT_FROM_PARSER);
    NS_ENSURE_SUCCESS(rv, rv);

    mPopupgroupContent->SetProperty(nsGkAtoms::docLevelNativeAnonymousContent,
                                    reinterpret_cast<void*>(true));

    aElements.AppendElement(mPopupgroupContent);

    nodeInfo = nodeInfoManager->GetNodeInfo(
        nsGkAtoms::tooltip, nullptr, kNameSpaceID_XUL, nsINode::ELEMENT_NODE);

    rv = NS_NewXULElement(getter_AddRefs(mTooltipContent), nodeInfo.forget(),
                          dom::NOT_FROM_PARSER);
    NS_ENSURE_SUCCESS(rv, rv);

    mTooltipContent->SetAttr(kNameSpaceID_None, nsGkAtoms::_default,
                             NS_LITERAL_STRING("true"), false);
    // Set the page attribute so the XBL binding will find the text for the
    // tooltip from the currently hovered element.
    mTooltipContent->SetAttr(kNameSpaceID_None, nsGkAtoms::page,
                             NS_LITERAL_STRING("true"), false);

    mTooltipContent->SetProperty(nsGkAtoms::docLevelNativeAnonymousContent,
                                 reinterpret_cast<void*>(true));

    aElements.AppendElement(mTooltipContent);
  }

#ifdef DEBUG
  for (auto& element : aElements) {
    MOZ_ASSERT(element.mContent->GetProperty(
                   nsGkAtoms::docLevelNativeAnonymousContent),
               "NAC from the canvas frame needs to be document-level, otherwise"
               " it (1) inherits from the document which is unexpected, and (2)"
               " StyleChildrenIterator won't be able to find it properly");
  }
#endif
  return NS_OK;
}

void nsCanvasFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                             uint32_t aFilter) {
  if (mCustomContentContainer) {
    aElements.AppendElement(mCustomContentContainer);
  }
  if (mPopupgroupContent) {
    aElements.AppendElement(mPopupgroupContent);
  }
  if (mTooltipContent) {
    aElements.AppendElement(mTooltipContent);
  }
}

void nsCanvasFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                PostDestroyData& aPostDestroyData) {
  nsIScrollableFrame* sf =
      PresContext()->GetPresShell()->GetRootScrollFrameAsScrollable();
  if (sf) {
    sf->RemoveScrollPositionListener(this);
  }

  aPostDestroyData.AddAnonymousContent(mCustomContentContainer.forget());
  if (mPopupgroupContent) {
    aPostDestroyData.AddAnonymousContent(mPopupgroupContent.forget());
  }
  if (mTooltipContent) {
    aPostDestroyData.AddAnonymousContent(mTooltipContent.forget());
  }

  MOZ_ASSERT(!mPopupSetFrame ||
                 nsLayoutUtils::IsProperAncestorFrame(this, mPopupSetFrame),
             "Someone forgot to clear popup set frame");
  nsContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

void nsCanvasFrame::ScrollPositionWillChange(nscoord aX, nscoord aY) {
  if (mDoPaintFocus) {
    mDoPaintFocus = false;
    PresShell()->GetRootFrame()->InvalidateFrameSubtree();
  }
}

NS_IMETHODIMP
nsCanvasFrame::SetHasFocus(bool aHasFocus) {
  if (mDoPaintFocus != aHasFocus) {
    mDoPaintFocus = aHasFocus;
    PresShell()->GetRootFrame()->InvalidateFrameSubtree();

    if (!mAddedScrollPositionListener) {
      nsIScrollableFrame* sf =
          PresContext()->GetPresShell()->GetRootScrollFrameAsScrollable();
      if (sf) {
        sf->AddScrollPositionListener(this);
        mAddedScrollPositionListener = true;
      }
    }
  }
  return NS_OK;
}

void nsCanvasFrame::SetInitialChildList(ChildListID aListID,
                                        nsFrameList& aChildList) {
  NS_ASSERTION(aListID != kPrincipalList || aChildList.IsEmpty() ||
                   aChildList.OnlyChild(),
               "Primary child list can have at most one frame in it");
  nsContainerFrame::SetInitialChildList(aListID, aChildList);
}

void nsCanvasFrame::AppendFrames(ChildListID aListID, nsFrameList& aFrameList) {
#ifdef DEBUG
  MOZ_ASSERT(aListID == kPrincipalList, "unexpected child list");
  if (!mFrames.IsEmpty()) {
    for (nsFrameList::Enumerator e(aFrameList); !e.AtEnd(); e.Next()) {
      // We only allow native anonymous child frames to be in principal child
      // list in canvas frame.
      MOZ_ASSERT(e.get()->GetContent()->IsInNativeAnonymousSubtree(),
                 "invalid child list");
    }
  }
  nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
  nsContainerFrame::AppendFrames(aListID, aFrameList);
}

void nsCanvasFrame::InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                                 const nsLineList::iterator* aPrevFrameLine,
                                 nsFrameList& aFrameList) {
  // Because we only support a single child frame inserting is the same
  // as appending
  MOZ_ASSERT(!aPrevFrame, "unexpected previous sibling frame");
  AppendFrames(aListID, aFrameList);
}

#ifdef DEBUG
void nsCanvasFrame::RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) {
  MOZ_ASSERT(aListID == kPrincipalList, "unexpected child list");
  nsContainerFrame::RemoveFrame(aListID, aOldFrame);
}
#endif

nsRect nsCanvasFrame::CanvasArea() const {
  // Not clear which overflow rect we want here, but it probably doesn't
  // matter.
  nsRect result(GetVisualOverflowRect());

  nsIScrollableFrame* scrollableFrame = do_QueryFrame(GetParent());
  if (scrollableFrame) {
    nsRect portRect = scrollableFrame->GetScrollPortRect();
    result.UnionRect(result, nsRect(nsPoint(0, 0), portRect.Size()));
  }
  return result;
}

nsPopupSetFrame* nsCanvasFrame::GetPopupSetFrame() { return mPopupSetFrame; }

void nsCanvasFrame::SetPopupSetFrame(nsPopupSetFrame* aPopupSet) {
  MOZ_ASSERT(!aPopupSet || !mPopupSetFrame,
             "Popup set is already defined! Only 1 allowed.");
  mPopupSetFrame = aPopupSet;
}

Element* nsCanvasFrame::GetDefaultTooltip() { return mTooltipContent; }

void nsCanvasFrame::SetDefaultTooltip(Element* aTooltip) {
  MOZ_ASSERT(!aTooltip || aTooltip == mTooltipContent,
             "Default tooltip should be anonymous content tooltip.");
  mTooltipContent = aTooltip;
}

void nsDisplayCanvasBackgroundColor::Paint(nsDisplayListBuilder* aBuilder,
                                           gfxContext* aCtx) {
  nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
  nsPoint offset = ToReferenceFrame();
  nsRect bgClipRect = frame->CanvasArea() + offset;
  if (NS_GET_A(mColor) > 0) {
    DrawTarget* drawTarget = aCtx->GetDrawTarget();
    int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
    Rect devPxRect =
        NSRectToSnappedRect(bgClipRect, appUnitsPerDevPixel, *drawTarget);
    drawTarget->FillRect(devPxRect, ColorPattern(ToDeviceColor(mColor)));
  }
}

already_AddRefed<Layer> nsDisplayCanvasBackgroundColor::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  if (NS_GET_A(mColor) == 0) {
    return nullptr;
  }

  RefPtr<ColorLayer> layer = static_cast<ColorLayer*>(
      aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateColorLayer();
    if (!layer) {
      return nullptr;
    }
  }
  layer->SetColor(ToDeviceColor(mColor));

  nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
  nsPoint offset = ToReferenceFrame();
  nsRect bgClipRect = frame->CanvasArea() + offset;

  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

  layer->SetBounds(bgClipRect.ToNearestPixels(appUnitsPerDevPixel));
  layer->SetBaseTransform(gfx::Matrix4x4::Translation(
      aContainerParameters.mOffset.x, aContainerParameters.mOffset.y, 0));

  return layer.forget();
}

bool nsDisplayCanvasBackgroundColor::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  ContainerLayerParameters parameter;

  nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
  nsPoint offset = ToReferenceFrame();
  nsRect bgClipRect = frame->CanvasArea() + offset;
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

  LayoutDeviceRect rect =
      LayoutDeviceRect::FromAppUnits(bgClipRect, appUnitsPerDevPixel);

  wr::LayoutRect r = wr::ToLayoutRect(rect);
  aBuilder.PushRect(r, r, !BackfaceIsHidden(),
                    wr::ToColorF(ToDeviceColor(mColor)));
  return true;
}

void nsDisplayCanvasBackgroundColor::WriteDebugInfo(
    std::stringstream& aStream) {
  aStream << " (rgba " << (int)NS_GET_R(mColor) << "," << (int)NS_GET_G(mColor)
          << "," << (int)NS_GET_B(mColor) << "," << (int)NS_GET_A(mColor)
          << ")";
}

void nsDisplayCanvasBackgroundImage::Paint(nsDisplayListBuilder* aBuilder,
                                           gfxContext* aCtx) {
  nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
  nsPoint offset = ToReferenceFrame();
  nsRect bgClipRect = frame->CanvasArea() + offset;

  PaintInternal(aBuilder, aCtx, GetPaintRect(), &bgClipRect);
}

bool nsDisplayCanvasBackgroundImage::IsSingleFixedPositionImage(
    nsDisplayListBuilder* aBuilder, const nsRect& aClipRect,
    gfxRect* aDestRect) {
  if (!mBackgroundStyle) return false;

  if (mBackgroundStyle->StyleBackground()->mImage.mLayers.Length() != 1)
    return false;

  nsPresContext* presContext = mFrame->PresContext();
  uint32_t flags = aBuilder->GetBackgroundPaintFlags();
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  const nsStyleImageLayers::Layer& layer =
      mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer];

  if (layer.mAttachment != StyleImageLayerAttachment::Fixed) return false;

  nsBackgroundLayerState state = nsCSSRendering::PrepareImageLayer(
      presContext, mFrame, flags, borderArea, aClipRect, layer);

  // We only care about images here, not gradients.
  if (!mIsRasterImage) return false;

  int32_t appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  *aDestRect =
      nsLayoutUtils::RectToGfxRect(state.mFillArea, appUnitsPerDevPixel);

  return true;
}

void nsDisplayCanvasThemedBackground::Paint(nsDisplayListBuilder* aBuilder,
                                            gfxContext* aCtx) {
  nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
  nsPoint offset = ToReferenceFrame();
  nsRect bgClipRect = frame->CanvasArea() + offset;

  PaintInternal(aBuilder, aCtx, GetPaintRect(), &bgClipRect);
}

/**
 * A display item to paint the focus ring for the document.
 *
 * The only reason this can't use nsDisplayGeneric is overriding GetBounds.
 */
class nsDisplayCanvasFocus : public nsPaintedDisplayItem {
 public:
  nsDisplayCanvasFocus(nsDisplayListBuilder* aBuilder, nsCanvasFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayCanvasFocus);
  }
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayCanvasFocus)

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override {
    *aSnap = false;
    // This is an overestimate, but that's not a problem.
    nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
    return frame->CanvasArea() + ToReferenceFrame();
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     gfxContext* aCtx) override {
    nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
    frame->PaintFocus(aCtx->GetDrawTarget(), ToReferenceFrame());
  }

  NS_DISPLAY_DECL_NAME("CanvasFocus", TYPE_CANVAS_FOCUS)
};

void nsCanvasFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                     const nsDisplayListSet& aLists) {
  if (GetPrevInFlow()) {
    DisplayOverflowContainers(aBuilder, aLists);
  }

  // Force a background to be shown. We may have a background propagated to us,
  // in which case StyleBackground wouldn't have the right background
  // and the code in nsFrame::DisplayBorderBackgroundOutline might not give us
  // a background.
  // We don't have any border or outline, and our background draws over
  // the overflow area, so just add nsDisplayCanvasBackground instead of
  // calling DisplayBorderBackgroundOutline.
  if (IsVisibleForPainting()) {
    ComputedStyle* bg = nullptr;
    nsIFrame* dependentFrame = nullptr;
    bool isThemed = IsThemed();
    if (!isThemed &&
        nsCSSRendering::FindBackgroundFrame(this, &dependentFrame)) {
      bg = dependentFrame->Style();
      if (dependentFrame == this) {
        dependentFrame = nullptr;
      }
    }
    aLists.BorderBackground()->AppendNewToTop<nsDisplayCanvasBackgroundColor>(
        aBuilder, this);

    if (isThemed) {
      aLists.BorderBackground()
          ->AppendNewToTop<nsDisplayCanvasThemedBackground>(aBuilder, this);
      return;
    }

    if (!bg) {
      return;
    }

    const ActiveScrolledRoot* asr = aBuilder->CurrentActiveScrolledRoot();

    bool needBlendContainer = false;
    nsDisplayListBuilder::AutoContainerASRTracker contASRTracker(aBuilder);

    // In high-contrast-mode, we suppress background-image on the canvas frame
    // (even when backplating), because users expect site backgrounds to conform
    // to their HCM background color when a solid color is rendered, and some
    // websites use solid-color images instead of an overwritable background
    // color.
    const bool suppressBackgroundImage =
        !PresContext()->PrefSheetPrefs().mUseDocumentColors &&
        StaticPrefs::
            browser_display_suppress_canvas_background_image_on_forced_colors();

    // Create separate items for each background layer.
    const nsStyleImageLayers& layers = bg->StyleBackground()->mImage;
    NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, layers) {
      if (layers.mLayers[i].mImage.IsNone() || suppressBackgroundImage) {
        continue;
      }
      if (layers.mLayers[i].mBlendMode != NS_STYLE_BLEND_NORMAL) {
        needBlendContainer = true;
      }

      nsRect bgRect =
          GetRectRelativeToSelf() + aBuilder->ToReferenceFrame(this);

      const ActiveScrolledRoot* thisItemASR = asr;
      nsDisplayList thisItemList;
      nsDisplayBackgroundImage::InitData bgData =
          nsDisplayBackgroundImage::GetInitData(aBuilder, this, i, bgRect, bg);

      if (bgData.shouldFixToViewport) {
        auto* displayData = aBuilder->GetCurrentFixedBackgroundDisplayData();
        nsDisplayListBuilder::AutoBuildingDisplayList buildingDisplayList(
            aBuilder, this, aBuilder->GetVisibleRect(),
            aBuilder->GetDirtyRect());

        DisplayListClipState::AutoSaveRestore clipState(aBuilder);
        nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter asrSetter(
            aBuilder);
        if (displayData) {
          nsPoint offset =
              GetOffsetTo(PresContext()->GetPresShell()->GetRootFrame());
          aBuilder->SetVisibleRect(displayData->mVisibleRect + offset);
          aBuilder->SetDirtyRect(displayData->mDirtyRect + offset);

          clipState.SetClipChainForContainingBlockDescendants(
              displayData->mContainingBlockClipChain);
          asrSetter.SetCurrentActiveScrolledRoot(
              displayData->mContainingBlockActiveScrolledRoot);
          thisItemASR = displayData->mContainingBlockActiveScrolledRoot;
        }
        nsDisplayCanvasBackgroundImage* bgItem = nullptr;
        {
          DisplayListClipState::AutoSaveRestore bgImageClip(aBuilder);
          bgImageClip.Clear();
          bgItem = MakeDisplayItem<nsDisplayCanvasBackgroundImage>(
              aBuilder, this, bgData);
          if (bgItem) {
            bgItem->SetDependentFrame(aBuilder, dependentFrame);
          }
        }
        if (bgItem) {
          thisItemList.AppendToTop(
              nsDisplayFixedPosition::CreateForFixedBackground(aBuilder, this,
                                                               bgItem, i));
        }

      } else {
        nsDisplayCanvasBackgroundImage* bgItem =
            MakeDisplayItem<nsDisplayCanvasBackgroundImage>(aBuilder, this,
                                                            bgData);
        if (bgItem) {
          bgItem->SetDependentFrame(aBuilder, dependentFrame);
          thisItemList.AppendToTop(bgItem);
        }
      }

      if (layers.mLayers[i].mBlendMode != NS_STYLE_BLEND_NORMAL) {
        DisplayListClipState::AutoSaveRestore blendClip(aBuilder);
        thisItemList.AppendNewToTop<nsDisplayBlendMode>(
            aBuilder, this, &thisItemList, layers.mLayers[i].mBlendMode,
            thisItemASR, i + 1);
      }
      aLists.BorderBackground()->AppendToTop(&thisItemList);
    }

    if (needBlendContainer) {
      const ActiveScrolledRoot* containerASR = contASRTracker.GetContainerASR();
      DisplayListClipState::AutoSaveRestore blendContainerClip(aBuilder);
      aLists.BorderBackground()->AppendToTop(
          nsDisplayBlendContainer::CreateForBackgroundBlendMode(
              aBuilder, this, aLists.BorderBackground(), containerASR));
    }
  }

  for (nsIFrame* kid : PrincipalChildList()) {
    // Put our child into its own pseudo-stack.
    BuildDisplayListForChild(aBuilder, kid, aLists);
  }

#ifdef DEBUG_CANVAS_FOCUS
  nsCOMPtr<nsIContent> focusContent;
  aPresContext->EventStateManager()->GetFocusedContent(
      getter_AddRefs(focusContent));

  bool hasFocus = false;
  nsCOMPtr<nsISupports> container;
  aPresContext->GetContainer(getter_AddRefs(container));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
  if (docShell) {
    docShell->GetHasFocus(&hasFocus);
    nsRect dirty = aBuilder->GetDirtyRect();
    printf("%p - nsCanvasFrame::Paint R:%d,%d,%d,%d  DR: %d,%d,%d,%d\n", this,
           mRect.x, mRect.y, mRect.width, mRect.height, dirty.x, dirty.y,
           dirty.width, dirty.height);
  }
  printf("%p - Focus: %s   c: %p  DoPaint:%s\n", docShell.get(),
         hasFocus ? "Y" : "N", focusContent.get(), mDoPaintFocus ? "Y" : "N");
#endif

  if (!mDoPaintFocus) return;
  // Only paint the focus if we're visible
  if (!StyleVisibility()->IsVisible()) return;

  aLists.Outlines()->AppendNewToTop<nsDisplayCanvasFocus>(aBuilder, this);
}

void nsCanvasFrame::PaintFocus(DrawTarget* aDrawTarget, nsPoint aPt) {
  nsRect focusRect(aPt, GetSize());

  nsIScrollableFrame* scrollableFrame = do_QueryFrame(GetParent());
  if (scrollableFrame) {
    nsRect portRect = scrollableFrame->GetScrollPortRect();
    focusRect.width = portRect.width;
    focusRect.height = portRect.height;
    focusRect.MoveBy(scrollableFrame->GetScrollPosition());
  }

  // XXX use the root frame foreground color, but should we find BODY frame
  // for HTML documents?
  nsIFrame* root = mFrames.FirstChild();
  const auto* text = root ? root->StyleText() : StyleText();
  nsCSSRendering::PaintFocus(PresContext(), aDrawTarget, focusRect,
                             text->mColor.ToColor());
}

/* virtual */
nscoord nsCanvasFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_MIN_INLINE_SIZE(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetMinISize(aRenderingContext);
  return result;
}

/* virtual */
nscoord nsCanvasFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_PREF_INLINE_SIZE(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetPrefISize(aRenderingContext);
  return result;
}

void nsCanvasFrame::Reflow(nsPresContext* aPresContext,
                           ReflowOutput& aDesiredSize,
                           const ReflowInput& aReflowInput,
                           nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsCanvasFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  NS_FRAME_TRACE_REFLOW_IN("nsCanvasFrame::Reflow");

  nsCanvasFrame* prevCanvasFrame = static_cast<nsCanvasFrame*>(GetPrevInFlow());
  if (prevCanvasFrame) {
    AutoFrameListPtr overflow(aPresContext,
                              prevCanvasFrame->StealOverflowFrames());
    if (overflow) {
      NS_ASSERTION(overflow->OnlyChild(),
                   "must have doc root as canvas frame's only child");
      nsContainerFrame::ReparentFrameViewList(*overflow, prevCanvasFrame, this);
      // Prepend overflow to the our child list. There may already be
      // children placeholders for fixed-pos elements, which don't get
      // reflowed but must not be lost until the canvas frame is destroyed.
      mFrames.InsertFrames(this, nullptr, *overflow);
    }
  }

  // Set our size up front, since some parts of reflow depend on it
  // being already set.  Note that the computed height may be
  // unconstrained; that's ok.  Consumers should watch out for that.
  SetSize(nsSize(aReflowInput.ComputedWidth(), aReflowInput.ComputedHeight()));

  // Reflow our one and only normal child frame. It's either the root
  // element's frame or a placeholder for that frame, if the root element
  // is abs-pos or fixed-pos. We may have additional children which
  // are placeholders for continuations of fixed-pos content, but those
  // don't need to be reflowed. The normal child is always comes before
  // the fixed-pos placeholders, because we insert it at the start
  // of the child list, above.
  ReflowOutput kidDesiredSize(aReflowInput);
  if (mFrames.IsEmpty()) {
    // We have no child frame, so return an empty size
    aDesiredSize.Width() = aDesiredSize.Height() = 0;
  } else if (mFrames.FirstChild() != mPopupSetFrame) {
    nsIFrame* kidFrame = mFrames.FirstChild();
    bool kidDirty = (kidFrame->GetStateBits() & NS_FRAME_IS_DIRTY) != 0;

    ReflowInput kidReflowInput(
        aPresContext, aReflowInput, kidFrame,
        aReflowInput.AvailableSize(kidFrame->GetWritingMode()));

    if (aReflowInput.IsBResizeForWM(kidReflowInput.GetWritingMode()) &&
        (kidFrame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_BSIZE)) {
      // Tell our kid it's being block-dir resized too.  Bit of a
      // hack for framesets.
      kidReflowInput.SetBResize(true);
    }

    WritingMode wm = aReflowInput.GetWritingMode();
    WritingMode kidWM = kidReflowInput.GetWritingMode();
    nsSize containerSize = aReflowInput.ComputedPhysicalSize();

    LogicalMargin margin = kidReflowInput.ComputedLogicalMargin();
    LogicalPoint kidPt(kidWM, margin.IStart(kidWM), margin.BStart(kidWM));

    // Reflow the frame
    ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowInput, kidWM,
                kidPt, containerSize, ReflowChildFlags::Default, aStatus);

    // Complete the reflow and position and size the child frame
    FinishReflowChild(kidFrame, aPresContext, kidDesiredSize, &kidReflowInput,
                      kidWM, kidPt, containerSize,
                      ReflowChildFlags::ApplyRelativePositioning);

    if (!aStatus.IsFullyComplete()) {
      nsIFrame* nextFrame = kidFrame->GetNextInFlow();
      NS_ASSERTION(
          nextFrame || aStatus.NextInFlowNeedsReflow(),
          "If it's incomplete and has no nif yet, it must flag a nif reflow.");
      if (!nextFrame) {
        nextFrame = aPresContext->PresShell()
                        ->FrameConstructor()
                        ->CreateContinuingFrame(aPresContext, kidFrame, this);
        SetOverflowFrames(nsFrameList(nextFrame, nextFrame));
        // Root overflow containers will be normal children of
        // the canvas frame, but that's ok because there
        // aren't any other frames we need to isolate them from
        // during reflow.
      }
      if (aStatus.IsOverflowIncomplete()) {
        nextFrame->AddStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
      }
    }

    // If the child frame was just inserted, then we're responsible for making
    // sure it repaints
    if (kidDirty) {
      // But we have a new child, which will affect our background, so
      // invalidate our whole rect.
      // Note: Even though we request to be sized to our child's size, our
      // scroll frame ensures that we are always the size of the viewport.
      // Also note: GetPosition() on a CanvasFrame is always going to return
      // (0, 0). We only want to invalidate GetRect() since Get*OverflowRect()
      // could also include overflow to our top and left (out of the viewport)
      // which doesn't need to be painted.
      nsIFrame* viewport = PresContext()->GetPresShell()->GetRootFrame();
      viewport->InvalidateFrame();
    }

    // Return our desired size. Normally it's what we're told, but
    // sometimes we can be given an unconstrained height (when a window
    // is sizing-to-content), and we should compute our desired height.
    LogicalSize finalSize(wm);
    finalSize.ISize(wm) = aReflowInput.ComputedISize();
    if (aReflowInput.ComputedBSize() == NS_UNCONSTRAINEDSIZE) {
      finalSize.BSize(wm) =
          kidFrame->GetLogicalSize(wm).BSize(wm) +
          kidReflowInput.ComputedLogicalMargin().BStartEnd(wm);
    } else {
      finalSize.BSize(wm) = aReflowInput.ComputedBSize();
    }

    aDesiredSize.SetSize(wm, finalSize);
    aDesiredSize.SetOverflowAreasToDesiredBounds();
    aDesiredSize.mOverflowAreas.UnionWith(kidDesiredSize.mOverflowAreas +
                                          kidFrame->GetPosition());
  }

  if (prevCanvasFrame) {
    ReflowOverflowContainerChildren(aPresContext, aReflowInput,
                                    aDesiredSize.mOverflowAreas,
                                    ReflowChildFlags::Default, aStatus);
  }

  if (mPopupSetFrame) {
    MOZ_ASSERT(mFrames.ContainsFrame(mPopupSetFrame),
               "Only normal flow supported.");
    nsReflowStatus popupStatus;
    ReflowOutput popupDesiredSize(aReflowInput.GetWritingMode());
    WritingMode wm = mPopupSetFrame->GetWritingMode();
    LogicalSize availSize = aReflowInput.ComputedSize(wm);
    availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
    ReflowInput popupReflowInput(aPresContext, aReflowInput, mPopupSetFrame,
                                 availSize);
    ReflowChild(mPopupSetFrame, aPresContext, popupDesiredSize,
                popupReflowInput, 0, 0, ReflowChildFlags::NoMoveFrame,
                popupStatus);
    FinishReflowChild(mPopupSetFrame, aPresContext, popupDesiredSize,
                      &popupReflowInput, 0, 0, ReflowChildFlags::NoMoveFrame);
  }

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aReflowInput,
                                 aStatus);

  NS_FRAME_TRACE_REFLOW_OUT("nsCanvasFrame::Reflow", aStatus);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

nsresult nsCanvasFrame::GetContentForEvent(WidgetEvent* aEvent,
                                           nsIContent** aContent) {
  NS_ENSURE_ARG_POINTER(aContent);
  nsresult rv = nsFrame::GetContentForEvent(aEvent, aContent);
  if (NS_FAILED(rv) || !*aContent) {
    nsIFrame* kid = mFrames.FirstChild();
    if (kid) {
      rv = kid->GetContentForEvent(aEvent, aContent);
    }
  }

  return rv;
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsCanvasFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(NS_LITERAL_STRING("Canvas"), aResult);
}
#endif
