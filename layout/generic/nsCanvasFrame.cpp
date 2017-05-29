/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object that goes directly inside the document's scrollbars */

#include "nsCanvasFrame.h"

#include "AccessibleCaretEventHub.h"
#include "gfxUtils.h"
#include "nsContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsRenderingContext.h"
#include "nsGkAtoms.h"
#include "nsIFrameInlines.h"
#include "nsIPresShell.h"
#include "nsDisplayList.h"
#include "nsCSSFrameConstructor.h"
#include "nsFrameManager.h"
#include "gfxPlatform.h"
#include "nsPrintfCString.h"
#include "mozilla/dom/AnonymousContent.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/PresShell.h"
// for focus
#include "nsIScrollableFrame.h"
#ifdef DEBUG_CANVAS_FOCUS
#include "nsIDocShell.h"
#endif

//#define DEBUG_CANVAS_FOCUS

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layout;
using namespace mozilla::gfx;
using namespace mozilla::layers;

nsCanvasFrame*
NS_NewCanvasFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsCanvasFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsCanvasFrame)

NS_QUERYFRAME_HEAD(nsCanvasFrame)
  NS_QUERYFRAME_ENTRY(nsCanvasFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

void
nsCanvasFrame::ShowCustomContentContainer()
{
  if (mCustomContentContainer) {
    mCustomContentContainer->UnsetAttr(kNameSpaceID_None, nsGkAtoms::hidden, true);
  }
}

void
nsCanvasFrame::HideCustomContentContainer()
{
  if (mCustomContentContainer) {
    mCustomContentContainer->SetAttr(kNameSpaceID_None, nsGkAtoms::hidden,
                                     NS_LITERAL_STRING("true"),
                                     true);
  }
}

nsresult
nsCanvasFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  if (!mContent) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc = mContent->OwnerDoc();
  nsresult rv = NS_OK;

  // Create the custom content container.
  mCustomContentContainer = doc->CreateHTMLElement(nsGkAtoms::div);
#ifdef DEBUG
  // We restyle our mCustomContentContainer, even though it's root anonymous
  // content.  Normally that's not OK because the frame constructor doesn't know
  // how to order the frame tree in such cases, but we make this work for this
  // particular case, so it's OK.
  mCustomContentContainer->SetProperty(nsGkAtoms::restylableAnonymousNode,
                                       reinterpret_cast<void*>(true));
#endif // DEBUG

  aElements.AppendElement(mCustomContentContainer);

  // XXX add :moz-native-anonymous or will that be automatically set?
  rv = mCustomContentContainer->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                                        NS_LITERAL_STRING("moz-custom-content-container"),
                                        true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Append all existing AnonymousContent nodes stored at document level if any.
  size_t len = doc->GetAnonymousContents().Length();
  for (size_t i = 0; i < len; ++i) {
    nsCOMPtr<Element> node = doc->GetAnonymousContents()[i]->GetContentNode();
    mCustomContentContainer->AppendChildTo(node->AsContent(), true);
  }

  // Only create a frame for mCustomContentContainer if it has some children.
  if (len == 0) {
    HideCustomContentContainer();
  }

  RefPtr<AccessibleCaretEventHub> eventHub =
    PresContext()->GetPresShell()->GetAccessibleCaretEventHub();
  if (eventHub) {
    // AccessibleCaret will insert anonymous caret elements.
    eventHub->Init();
  }

  return NS_OK;
}

void
nsCanvasFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements, uint32_t aFilter)
{
  aElements.AppendElement(mCustomContentContainer);
}

void
nsCanvasFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsIScrollableFrame* sf =
    PresContext()->GetPresShell()->GetRootScrollFrameAsScrollable();
  if (sf) {
    sf->RemoveScrollPositionListener(this);
  }

  // Elements inserted in the custom content container have the same lifetime as
  // the document, so before destroying the container, make sure to keep a clone
  // of each of them at document level so they can be re-appended on reframe.
  if (mCustomContentContainer) {
    nsCOMPtr<nsIDocument> doc = mContent->OwnerDoc();
    ErrorResult rv;

    nsTArray<RefPtr<mozilla::dom::AnonymousContent>>& docAnonContents =
      doc->GetAnonymousContents();
    for (size_t i = 0, len = docAnonContents.Length(); i < len; ++i) {
      AnonymousContent* content = docAnonContents[i];
      nsCOMPtr<nsINode> clonedElement = content->GetContentNode()->CloneNode(true, rv);
      content->SetContentNode(clonedElement->AsElement());
    }
  }
  nsContentUtils::DestroyAnonymousContent(&mCustomContentContainer);

  nsContainerFrame::DestroyFrom(aDestructRoot);
}

void
nsCanvasFrame::ScrollPositionWillChange(nscoord aX, nscoord aY)
{
  if (mDoPaintFocus) {
    mDoPaintFocus = false;
    PresContext()->FrameManager()->GetRootFrame()->InvalidateFrameSubtree();
  }
}

NS_IMETHODIMP
nsCanvasFrame::SetHasFocus(bool aHasFocus)
{
  if (mDoPaintFocus != aHasFocus) {
    mDoPaintFocus = aHasFocus;
    PresContext()->FrameManager()->GetRootFrame()->InvalidateFrameSubtree();

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

void
nsCanvasFrame::SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList)
{
  NS_ASSERTION(aListID != kPrincipalList ||
               aChildList.IsEmpty() || aChildList.OnlyChild(),
               "Primary child list can have at most one frame in it");
  nsContainerFrame::SetInitialChildList(aListID, aChildList);
  MaybePropagateRootElementWritingMode();
}

void
nsCanvasFrame::AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList)
{
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
  MaybePropagateRootElementWritingMode();
}

void
nsCanvasFrame::InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList)
{
  // Because we only support a single child frame inserting is the same
  // as appending
  MOZ_ASSERT(!aPrevFrame, "unexpected previous sibling frame");
  AppendFrames(aListID, aFrameList);
  MaybePropagateRootElementWritingMode();
}

#ifdef DEBUG
void
nsCanvasFrame::RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame)
{
  MOZ_ASSERT(aListID == kPrincipalList, "unexpected child list");
  nsContainerFrame::RemoveFrame(aListID, aOldFrame);
}
#endif

nsRect nsCanvasFrame::CanvasArea() const
{
  // Not clear which overflow rect we want here, but it probably doesn't
  // matter.
  nsRect result(GetVisualOverflowRect());

  nsIScrollableFrame *scrollableFrame = do_QueryFrame(GetParent());
  if (scrollableFrame) {
    nsRect portRect = scrollableFrame->GetScrollPortRect();
    result.UnionRect(result, nsRect(nsPoint(0, 0), portRect.Size()));
  }
  return result;
}

void
nsDisplayCanvasBackgroundColor::Paint(nsDisplayListBuilder* aBuilder,
                                      nsRenderingContext* aCtx)
{
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

already_AddRefed<Layer>
nsDisplayCanvasBackgroundColor::BuildLayer(nsDisplayListBuilder* aBuilder,
                                           LayerManager* aManager,
                                           const ContainerLayerParameters& aContainerParameters)
{
  if (NS_GET_A(mColor) == 0) {
    return nullptr;
  }

  if (aManager->GetBackendType() == layers::LayersBackend::LAYERS_WR) {
    return BuildDisplayItemLayer(aBuilder, aManager, aContainerParameters);
  }

  RefPtr<ColorLayer> layer = static_cast<ColorLayer*>
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
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
  layer->SetBaseTransform(gfx::Matrix4x4::Translation(aContainerParameters.mOffset.x,
                                                      aContainerParameters.mOffset.y, 0));

  return layer.forget();
}

void
nsDisplayCanvasBackgroundColor::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                        const StackingContextHelper& aSc,
                                                        nsTArray<WebRenderParentCommand>& aParentCommands,
                                                        WebRenderDisplayItemLayer* aLayer)
{
  nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
  nsPoint offset = ToReferenceFrame();
  nsRect bgClipRect = frame->CanvasArea() + offset;
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

  LayoutDeviceRect rect = LayoutDeviceRect::FromAppUnits(
          bgClipRect, appUnitsPerDevPixel);

  WrRect transformedRect = aSc.ToRelativeWrRect(rect);
  aBuilder.PushRect(transformedRect,
                    aBuilder.PushClipRegion(transformedRect),
                    wr::ToWrColor(ToDeviceColor(mColor)));
}

#ifdef MOZ_DUMP_PAINTING
void
nsDisplayCanvasBackgroundColor::WriteDebugInfo(std::stringstream& aStream)
{
  aStream << " (rgba "
          << (int)NS_GET_R(mColor) << ","
          << (int)NS_GET_G(mColor) << ","
          << (int)NS_GET_B(mColor) << ","
          << (int)NS_GET_A(mColor) << ")";
}
#endif

#ifndef MOZ_GFX_OPTIMIZE_MOBILE
static void BlitSurface(DrawTarget* aDest, const gfxRect& aRect, DrawTarget* aSource)
{
  RefPtr<SourceSurface> source = aSource->Snapshot();
  aDest->DrawSurface(source,
                     Rect(aRect.x, aRect.y, aRect.width, aRect.height),
                     Rect(0, 0, aRect.width, aRect.height));
}
#endif

void
nsDisplayCanvasBackgroundImage::Paint(nsDisplayListBuilder* aBuilder,
                                      nsRenderingContext* aCtx)
{
  nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
  nsPoint offset = ToReferenceFrame();
  nsRect bgClipRect = frame->CanvasArea() + offset;

#ifndef MOZ_GFX_OPTIMIZE_MOBILE
  RefPtr<gfxContext> dest = aCtx->ThebesContext();
  gfxRect destRect;
  if (IsSingleFixedPositionImage(aBuilder, bgClipRect, &destRect) &&
      aBuilder->IsPaintingToWindow() && !aBuilder->IsCompositingCheap() &&
      !dest->CurrentMatrix().HasNonIntegerTranslation()) {
    // Snap image rectangle to nearest pixel boundaries. This is the right way
    // to snap for this context, because we checked HasNonIntegerTranslation
    // above.
    destRect.Round();
    RefPtr<DrawTarget> dt = 
      Frame()->GetProperty(nsIFrame::CachedBackgroundImageDT());
    DrawTarget* destDT = dest->GetDrawTarget();
    if (dt) {
      BlitSurface(destDT, destRect, dt);
      return;
    }

    dt = destDT->CreateSimilarDrawTarget(IntSize::Ceil(destRect.width,
                                                       destRect.height),
                                         SurfaceFormat::B8G8R8A8);
    if (dt && dt->IsValid()) {
      RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(dt);
      MOZ_ASSERT(ctx); // already checked draw target above
      ctx->SetMatrix(ctx->CurrentMatrix().Translate(-destRect.x, -destRect.y));
      nsRenderingContext context(ctx);
      PaintInternal(aBuilder, &context, bgClipRect, &bgClipRect);
      BlitSurface(dest->GetDrawTarget(), destRect, dt);
      frame->SetProperty(nsIFrame::CachedBackgroundImageDT(),
                              dt.forget().take());
      return;
    }
  }
#endif
  PaintInternal(aBuilder, aCtx, mVisibleRect, &bgClipRect);
}

bool
nsDisplayCanvasBackgroundImage::IsSingleFixedPositionImage(nsDisplayListBuilder* aBuilder,
                                                           const nsRect& aClipRect,
                                                           gfxRect* aDestRect)
{
  if (!mBackgroundStyle)
    return false;

  if (mBackgroundStyle->mImage.mLayers.Length() != 1)
    return false;


  nsPresContext* presContext = mFrame->PresContext();
  uint32_t flags = aBuilder->GetBackgroundPaintFlags();
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  const nsStyleImageLayers::Layer &layer = mBackgroundStyle->mImage.mLayers[mLayer];

  if (layer.mAttachment != NS_STYLE_IMAGELAYER_ATTACHMENT_FIXED)
    return false;

   nsBackgroundLayerState state =
     nsCSSRendering::PrepareImageLayer(presContext, mFrame, flags,
                                       borderArea, aClipRect, layer);


  // We only care about images here, not gradients.
  if (!mIsRasterImage)
    return false;

  int32_t appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  *aDestRect = nsLayoutUtils::RectToGfxRect(state.mFillArea, appUnitsPerDevPixel);

  return true;
}


void
nsDisplayCanvasThemedBackground::Paint(nsDisplayListBuilder* aBuilder,
                                       nsRenderingContext* aCtx)
{
  nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
  nsPoint offset = ToReferenceFrame();
  nsRect bgClipRect = frame->CanvasArea() + offset;

  PaintInternal(aBuilder, aCtx, mVisibleRect, &bgClipRect);
}

/**
 * A display item to paint the focus ring for the document.
 *
 * The only reason this can't use nsDisplayGeneric is overriding GetBounds.
 */
class nsDisplayCanvasFocus : public nsDisplayItem {
public:
  nsDisplayCanvasFocus(nsDisplayListBuilder* aBuilder, nsCanvasFrame *aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  {
    MOZ_COUNT_CTOR(nsDisplayCanvasFocus);
  }
  virtual ~nsDisplayCanvasFocus() {
    MOZ_COUNT_DTOR(nsDisplayCanvasFocus);
  }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) override
  {
    *aSnap = false;
    // This is an overestimate, but that's not a problem.
    nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
    return frame->CanvasArea() + ToReferenceFrame();
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) override
  {
    nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
    frame->PaintFocus(aCtx->GetDrawTarget(), ToReferenceFrame());
  }

  NS_DISPLAY_DECL_NAME("CanvasFocus", TYPE_CANVAS_FOCUS)
};

void
nsCanvasFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists)
{
  if (GetPrevInFlow()) {
    DisplayOverflowContainers(aBuilder, aDirtyRect, aLists);
  }

  // Force a background to be shown. We may have a background propagated to us,
  // in which case StyleBackground wouldn't have the right background
  // and the code in nsFrame::DisplayBorderBackgroundOutline might not give us
  // a background.
  // We don't have any border or outline, and our background draws over
  // the overflow area, so just add nsDisplayCanvasBackground instead of
  // calling DisplayBorderBackgroundOutline.
  if (IsVisibleForPainting(aBuilder)) {
    nsStyleContext* bgSC;
    const nsStyleBackground* bg = nullptr;
    bool isThemed = IsThemed();
    if (!isThemed && nsCSSRendering::FindBackground(this, &bgSC)) {
      bg = bgSC->StyleBackground();
    }
    aLists.BorderBackground()->AppendNewToTop(
        new (aBuilder) nsDisplayCanvasBackgroundColor(aBuilder, this));
  
    if (isThemed) {
      aLists.BorderBackground()->AppendNewToTop(
        new (aBuilder) nsDisplayCanvasThemedBackground(aBuilder, this));
      return;
    }

    if (!bg) {
      return;
    }

    const ActiveScrolledRoot* asr =
      aBuilder->CurrentActiveScrolledRoot();

    bool needBlendContainer = false;
    nsDisplayListBuilder::AutoContainerASRTracker contASRTracker(aBuilder);

    // Create separate items for each background layer.
    const nsStyleImageLayers& layers = bg->mImage;
    NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, layers) {
      if (layers.mLayers[i].mImage.IsEmpty()) {
        continue;
      }
      if (layers.mLayers[i].mBlendMode != NS_STYLE_BLEND_NORMAL) {
        needBlendContainer = true;
      }

      nsRect bgRect = GetRectRelativeToSelf() + aBuilder->ToReferenceFrame(this);

      const ActiveScrolledRoot* thisItemASR = asr;
      nsDisplayList thisItemList;
      nsDisplayBackgroundImage::InitData bgData =
        nsDisplayBackgroundImage::GetInitData(aBuilder, this, i, bgRect, bg,
                                              nsDisplayBackgroundImage::LayerizeFixed::ALWAYS_LAYERIZE_FIXED_BACKGROUND);

      if (bgData.shouldFixToViewport) {

        auto* displayData = aBuilder->GetCurrentFixedBackgroundDisplayData();
        nsDisplayListBuilder::AutoBuildingDisplayList
          buildingDisplayList(aBuilder, this, aBuilder->GetDirtyRect(), false);

        DisplayListClipState::AutoSaveRestore clipState(aBuilder);
        nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter asrSetter(aBuilder);
        if (displayData) {
          nsRect dirtyRect = displayData->mDirtyRect + GetOffsetTo(PresContext()->GetPresShell()->GetRootFrame());
          buildingDisplayList.SetDirtyRect(dirtyRect);
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
          bgItem = new (aBuilder) nsDisplayCanvasBackgroundImage(bgData);
        }
        thisItemList.AppendNewToTop(
          nsDisplayFixedPosition::CreateForFixedBackground(aBuilder, this, bgItem, i));

      } else {
        thisItemList.AppendNewToTop(new (aBuilder) nsDisplayCanvasBackgroundImage(bgData));
      }

      if (layers.mLayers[i].mBlendMode != NS_STYLE_BLEND_NORMAL) {
        DisplayListClipState::AutoSaveRestore blendClip(aBuilder);
        blendClip.ClearUpToASR(thisItemASR);
        thisItemList.AppendNewToTop(
          new (aBuilder) nsDisplayBlendMode(aBuilder, this, &thisItemList,
                                            layers.mLayers[i].mBlendMode,
                                            thisItemASR, i + 1));
      }
      aLists.BorderBackground()->AppendToTop(&thisItemList);
    }

    if (needBlendContainer) {
      const ActiveScrolledRoot* containerASR = contASRTracker.GetContainerASR();
      DisplayListClipState::AutoSaveRestore blendContainerClip(aBuilder);
      blendContainerClip.ClearUpToASR(containerASR);
      aLists.BorderBackground()->AppendNewToTop(
        nsDisplayBlendContainer::CreateForBackgroundBlendMode(aBuilder, this,
                                                              aLists.BorderBackground(),
                                                              containerASR));
    }
  }

  for (nsIFrame* kid : PrincipalChildList()) {
    // Put our child into its own pseudo-stack.
    BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
  }

#ifdef DEBUG_CANVAS_FOCUS
  nsCOMPtr<nsIContent> focusContent;
  aPresContext->EventStateManager()->
    GetFocusedContent(getter_AddRefs(focusContent));

  bool hasFocus = false;
  nsCOMPtr<nsISupports> container;
  aPresContext->GetContainer(getter_AddRefs(container));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
  if (docShell) {
    docShell->GetHasFocus(&hasFocus);
    printf("%p - nsCanvasFrame::Paint R:%d,%d,%d,%d  DR: %d,%d,%d,%d\n", this, 
            mRect.x, mRect.y, mRect.width, mRect.height,
            aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  }
  printf("%p - Focus: %s   c: %p  DoPaint:%s\n", docShell.get(), hasFocus?"Y":"N", 
         focusContent.get(), mDoPaintFocus?"Y":"N");
#endif

  if (!mDoPaintFocus)
    return;
  // Only paint the focus if we're visible
  if (!StyleVisibility()->IsVisible())
    return;
  
  aLists.Outlines()->AppendNewToTop(new (aBuilder)
    nsDisplayCanvasFocus(aBuilder, this));
}

void
nsCanvasFrame::PaintFocus(DrawTarget* aDrawTarget, nsPoint aPt)
{
  nsRect focusRect(aPt, GetSize());

  nsIScrollableFrame *scrollableFrame = do_QueryFrame(GetParent());
  if (scrollableFrame) {
    nsRect portRect = scrollableFrame->GetScrollPortRect();
    focusRect.width = portRect.width;
    focusRect.height = portRect.height;
    focusRect.MoveBy(scrollableFrame->GetScrollPosition());
  }

 // XXX use the root frame foreground color, but should we find BODY frame
 // for HTML documents?
  nsIFrame* root = mFrames.FirstChild();
  const nsStyleColor* color = root ? root->StyleColor() : StyleColor();
  if (!color) {
    NS_ERROR("current color cannot be found");
    return;
  }

  nsCSSRendering::PaintFocus(PresContext(), aDrawTarget,
                             focusRect, color->mColor);
}

/* virtual */ nscoord
nsCanvasFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetMinISize(aRenderingContext);
  return result;
}

/* virtual */ nscoord
nsCanvasFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetPrefISize(aRenderingContext);
  return result;
}

void
nsCanvasFrame::Reflow(nsPresContext*           aPresContext,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsCanvasFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  NS_FRAME_TRACE_REFLOW_IN("nsCanvasFrame::Reflow");

  // Initialize OUT parameter
  aStatus.Reset();

  nsCanvasFrame* prevCanvasFrame = static_cast<nsCanvasFrame*>
                                               (GetPrevInFlow());
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
  } else {
    nsIFrame* kidFrame = mFrames.FirstChild();
    bool kidDirty = (kidFrame->GetStateBits() & NS_FRAME_IS_DIRTY) != 0;

    ReflowInput
      kidReflowInput(aPresContext, aReflowInput, kidFrame,
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

    kidReflowInput.ApplyRelativePositioning(&kidPt, containerSize);

    // Reflow the frame
    ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowInput,
                kidWM, kidPt, containerSize, 0, aStatus);

    // Complete the reflow and position and size the child frame
    FinishReflowChild(kidFrame, aPresContext, kidDesiredSize, &kidReflowInput,
                      kidWM, kidPt, containerSize, 0);

    if (!aStatus.IsFullyComplete()) {
      nsIFrame* nextFrame = kidFrame->GetNextInFlow();
      NS_ASSERTION(nextFrame || aStatus.NextInFlowNeedsReflow(),
        "If it's incomplete and has no nif yet, it must flag a nif reflow.");
      if (!nextFrame) {
        nextFrame = aPresContext->PresShell()->FrameConstructor()->
          CreateContinuingFrame(aPresContext, kidFrame, this);
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

    // If the child frame was just inserted, then we're responsible for making sure
    // it repaints
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
      finalSize.BSize(wm) = kidFrame->GetLogicalSize(wm).BSize(wm) +
        kidReflowInput.ComputedLogicalMargin().BStartEnd(wm);
    } else {
      finalSize.BSize(wm) = aReflowInput.ComputedBSize();
    }

    aDesiredSize.SetSize(wm, finalSize);
    aDesiredSize.SetOverflowAreasToDesiredBounds();
    aDesiredSize.mOverflowAreas.UnionWith(
      kidDesiredSize.mOverflowAreas + kidFrame->GetPosition());
  }

  if (prevCanvasFrame) {
    ReflowOverflowContainerChildren(aPresContext, aReflowInput,
                                    aDesiredSize.mOverflowAreas, 0,
                                    aStatus);
  }

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aReflowInput, aStatus);

  NS_FRAME_TRACE_REFLOW_OUT("nsCanvasFrame::Reflow", aStatus);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

nsresult
nsCanvasFrame::GetContentForEvent(WidgetEvent* aEvent, nsIContent** aContent)
{
  NS_ENSURE_ARG_POINTER(aContent);
  nsresult rv = nsFrame::GetContentForEvent(aEvent,
                                            aContent);
  if (NS_FAILED(rv) || !*aContent) {
    nsIFrame* kid = mFrames.FirstChild();
    if (kid) {
      rv = kid->GetContentForEvent(aEvent,
                                   aContent);
    }
  }

  return rv;
}

void
nsCanvasFrame::MaybePropagateRootElementWritingMode()
{
  nsIFrame* child = PrincipalChildList().FirstChild();
  if (child && child->GetContent() &&
      child->GetContent() == PresContext()->Document()->GetRootElement()) {
    nsIFrame* childPrimary = child->GetContent()->GetPrimaryFrame();
    PropagateRootElementWritingMode(childPrimary->GetWritingMode());
  }
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsCanvasFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Canvas"), aResult);
}
#endif
