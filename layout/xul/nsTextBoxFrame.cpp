/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTextBoxFrame.h"

#include "gfx2DGlue.h"
#include "gfxUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/gfx/2D.h"
#include "nsFontMetrics.h"
#include "nsReadableUtils.h"
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "gfxContext.h"
#include "nsIContent.h"
#include "nsNameSpaceManager.h"
#include "nsBoxLayoutState.h"
#include "nsMenuBarListener.h"
#include "nsString.h"
#include "nsITheme.h"
#include "nsUnicharUtils.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsCSSRendering.h"
#include "nsIReflowCallback.h"
#include "nsBoxFrame.h"
#include "nsLayoutUtils.h"
#include "nsUnicodeProperties.h"
#include "TextDrawTarget.h"

#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
#endif

#include "nsBidiUtils.h"
#include "nsBidiPresUtils.h"

using namespace mozilla;
using namespace mozilla::gfx;

class nsAccessKeyInfo {
 public:
  int32_t mAccesskeyIndex;
  nscoord mBeforeWidth, mAccessWidth, mAccessUnderlineSize, mAccessOffset;
};

bool nsTextBoxFrame::gAlwaysAppendAccessKey = false;
bool nsTextBoxFrame::gAccessKeyPrefInitialized = false;
bool nsTextBoxFrame::gInsertSeparatorBeforeAccessKey = false;
bool nsTextBoxFrame::gInsertSeparatorPrefInitialized = false;

nsIFrame* NS_NewTextBoxFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsTextBoxFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsTextBoxFrame)

NS_QUERYFRAME_HEAD(nsTextBoxFrame)
  NS_QUERYFRAME_ENTRY(nsTextBoxFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsLeafBoxFrame)

nsresult nsTextBoxFrame::AttributeChanged(int32_t aNameSpaceID,
                                          nsAtom* aAttribute,
                                          int32_t aModType) {
  bool aResize;
  bool aRedraw;

  UpdateAttributes(aAttribute, aResize, aRedraw);

  if (aResize) {
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::StyleChange,
                                  NS_FRAME_IS_DIRTY);
  } else if (aRedraw) {
    nsBoxLayoutState state(PresContext());
    XULRedraw(state);
  }

  // If the accesskey changed, register for the new value
  // The old value has been unregistered in nsXULElement::SetAttr
  if (aAttribute == nsGkAtoms::accesskey || aAttribute == nsGkAtoms::control)
    RegUnregAccessKey(true);

  return NS_OK;
}

nsTextBoxFrame::nsTextBoxFrame(ComputedStyle* aStyle,
                               nsPresContext* aPresContext)
    : nsLeafBoxFrame(aStyle, aPresContext, kClassID),
      mAccessKeyInfo(nullptr),
      mCropType(CropRight),
      mAscent(0),
      mNeedsReflowCallback(false) {
  MarkIntrinsicISizesDirty();
}

nsTextBoxFrame::~nsTextBoxFrame() { delete mAccessKeyInfo; }

void nsTextBoxFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                          nsIFrame* aPrevInFlow) {
  nsLeafBoxFrame::Init(aContent, aParent, aPrevInFlow);

  bool aResize;
  bool aRedraw;
  UpdateAttributes(nullptr, aResize, aRedraw); /* update all */

  // register access key
  RegUnregAccessKey(true);
}

void nsTextBoxFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                 PostDestroyData& aPostDestroyData) {
  // unregister access key
  RegUnregAccessKey(false);
  nsLeafBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

bool nsTextBoxFrame::AlwaysAppendAccessKey() {
  if (!gAccessKeyPrefInitialized) {
    gAccessKeyPrefInitialized = true;

    const char* prefName = "intl.menuitems.alwaysappendaccesskeys";
    nsAutoString val;
    Preferences::GetLocalizedString(prefName, val);
    gAlwaysAppendAccessKey = val.EqualsLiteral("true");
  }
  return gAlwaysAppendAccessKey;
}

bool nsTextBoxFrame::InsertSeparatorBeforeAccessKey() {
  if (!gInsertSeparatorPrefInitialized) {
    gInsertSeparatorPrefInitialized = true;

    const char* prefName = "intl.menuitems.insertseparatorbeforeaccesskeys";
    nsAutoString val;
    Preferences::GetLocalizedString(prefName, val);
    gInsertSeparatorBeforeAccessKey = val.EqualsLiteral("true");
  }
  return gInsertSeparatorBeforeAccessKey;
}

class nsAsyncAccesskeyUpdate final : public nsIReflowCallback {
 public:
  explicit nsAsyncAccesskeyUpdate(nsIFrame* aFrame) : mWeakFrame(aFrame) {}

  virtual bool ReflowFinished() override {
    bool shouldFlush = false;
    nsTextBoxFrame* frame = static_cast<nsTextBoxFrame*>(mWeakFrame.GetFrame());
    if (frame) {
      shouldFlush = frame->UpdateAccesskey(mWeakFrame);
    }
    delete this;
    return shouldFlush;
  }

  virtual void ReflowCallbackCanceled() override { delete this; }

  WeakFrame mWeakFrame;
};

bool nsTextBoxFrame::UpdateAccesskey(WeakFrame& aWeakThis) {
  nsAutoString accesskey;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey,
                                 accesskey);

  if (!accesskey.Equals(mAccessKey)) {
    // Need to get clean mTitle.
    RecomputeTitle();
    mAccessKey = accesskey;
    UpdateAccessTitle();
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::StyleChange,
                                  NS_FRAME_IS_DIRTY);
    return true;
  }
  return false;
}

void nsTextBoxFrame::UpdateAttributes(nsAtom* aAttribute, bool& aResize,
                                      bool& aRedraw) {
  bool doUpdateTitle = false;
  aResize = false;
  aRedraw = false;

  if (aAttribute == nullptr || aAttribute == nsGkAtoms::crop) {
    static dom::Element::AttrValuesArray strings[] = {
        nsGkAtoms::left,  nsGkAtoms::start, nsGkAtoms::center,
        nsGkAtoms::right, nsGkAtoms::end,   nsGkAtoms::none,
        nullptr};
    CroppingStyle cropType;
    switch (mContent->AsElement()->FindAttrValueIn(
        kNameSpaceID_None, nsGkAtoms::crop, strings, eCaseMatters)) {
      case 0:
      case 1:
        cropType = CropLeft;
        break;
      case 2:
        cropType = CropCenter;
        break;
      case 3:
      case 4:
        cropType = CropRight;
        break;
      case 5:
        cropType = CropNone;
        break;
      default:
        cropType = CropAuto;
        break;
    }

    if (cropType != mCropType) {
      aResize = true;
      mCropType = cropType;
    }
  }

  if (aAttribute == nullptr || aAttribute == nsGkAtoms::value) {
    RecomputeTitle();
    doUpdateTitle = true;
  }

  if (aAttribute == nullptr || aAttribute == nsGkAtoms::accesskey) {
    mNeedsReflowCallback = true;
    // Ensure that layout is refreshed and reflow callback called.
    aResize = true;
  }

  if (doUpdateTitle) {
    UpdateAccessTitle();
    aResize = true;
  }
}

class nsDisplayXULTextBox final : public nsPaintedDisplayItem {
 public:
  nsDisplayXULTextBox(nsDisplayListBuilder* aBuilder, nsTextBoxFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayXULTextBox);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayXULTextBox)
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  NS_DISPLAY_DECL_NAME("XULTextBox", TYPE_XUL_TEXT_BOX)

  virtual nsRect GetComponentAlphaBounds(
      nsDisplayListBuilder* aBuilder) const override;

  void PaintTextToContext(gfxContext* aCtx, nsPoint aOffset,
                          const nscolor* aColor);

  virtual bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
};

static void PaintTextShadowCallback(gfxContext* aCtx, nsPoint aShadowOffset,
                                    const nscolor& aShadowColor, void* aData) {
  reinterpret_cast<nsDisplayXULTextBox*>(aData)->PaintTextToContext(
      aCtx, aShadowOffset, &aShadowColor);
}

void nsDisplayXULTextBox::Paint(nsDisplayListBuilder* aBuilder,
                                gfxContext* aCtx) {
  DrawTargetAutoDisableSubpixelAntialiasing disable(aCtx->GetDrawTarget(),
                                                    IsSubpixelAADisabled());

  // Paint the text shadow before doing any foreground stuff
  nsRect drawRect =
      static_cast<nsTextBoxFrame*>(mFrame)->mTextDrawRect + ToReferenceFrame();
  nsLayoutUtils::PaintTextShadow(mFrame, aCtx, drawRect, GetPaintRect(),
                                 mFrame->StyleText()->mColor.ToColor(),
                                 PaintTextShadowCallback, (void*)this);

  PaintTextToContext(aCtx, nsPoint(0, 0), nullptr);
}

void nsDisplayXULTextBox::PaintTextToContext(gfxContext* aCtx, nsPoint aOffset,
                                             const nscolor* aColor) {
  static_cast<nsTextBoxFrame*>(mFrame)->PaintTitle(
      *aCtx, GetPaintRect(), ToReferenceFrame() + aOffset, aColor);
}

bool nsDisplayXULTextBox::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  bool snap = false;
  auto bounds = GetBounds(aDisplayListBuilder, &snap);

  if (bounds.IsEmpty()) {
    return true;
  }

  auto appUnitsPerDevPixel = Frame()->PresContext()->AppUnitsPerDevPixel();
  gfx::Point deviceOffset =
      LayoutDevicePoint::FromAppUnits(bounds.TopLeft(), appUnitsPerDevPixel)
          .ToUnknownPoint();

  RefPtr<mozilla::layout::TextDrawTarget> textDrawer =
      new mozilla::layout::TextDrawTarget(aBuilder, aResources, aSc, aManager,
                                          this, bounds);
  RefPtr<gfxContext> captureCtx =
      gfxContext::CreateOrNull(textDrawer, deviceOffset);

  Paint(aDisplayListBuilder, captureCtx);
  textDrawer->TerminateShadows();

  return textDrawer->Finish();
}

nsRect nsDisplayXULTextBox::GetBounds(nsDisplayListBuilder* aBuilder,
                                      bool* aSnap) const {
  *aSnap = false;
  return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
}

nsRect nsDisplayXULTextBox::GetComponentAlphaBounds(
    nsDisplayListBuilder* aBuilder) const {
  return static_cast<nsTextBoxFrame*>(mFrame)->GetComponentAlphaBounds() +
         ToReferenceFrame();
}

void nsTextBoxFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                      const nsDisplayListSet& aLists) {
  if (!IsVisibleForPainting()) return;

  nsLeafBoxFrame::BuildDisplayList(aBuilder, aLists);

  aLists.Content()->AppendNewToTop<nsDisplayXULTextBox>(aBuilder, this);
}

void nsTextBoxFrame::PaintTitle(gfxContext& aRenderingContext,
                                const nsRect& aDirtyRect, nsPoint aPt,
                                const nscolor* aOverrideColor) {
  if (mTitle.IsEmpty()) return;

  DrawText(aRenderingContext, aDirtyRect, mTextDrawRect + aPt, aOverrideColor);
}

void nsTextBoxFrame::DrawText(gfxContext& aRenderingContext,
                              const nsRect& aDirtyRect, const nsRect& aTextRect,
                              const nscolor* aOverrideColor) {
  nsPresContext* presContext = PresContext();
  int32_t appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();

  // paint the title
  nscolor overColor = 0;
  nscolor underColor = 0;
  nscolor strikeColor = 0;
  uint8_t overStyle = 0;
  uint8_t underStyle = 0;
  uint8_t strikeStyle = 0;

  // Begin with no decorations
  auto decorations = StyleTextDecorationLine::NONE;
  // A mask of all possible line decorations.
  auto decorMask = StyleTextDecorationLine::UNDERLINE |
                   StyleTextDecorationLine::OVERLINE |
                   StyleTextDecorationLine::LINE_THROUGH;

  WritingMode wm = GetWritingMode();
  bool vertical = wm.IsVertical();

  nsIFrame* f = this;
  do {  // find decoration colors
    ComputedStyle* context = f->Style();
    if (!context->HasTextDecorationLines()) {
      break;
    }
    const nsStyleTextReset* styleText = context->StyleTextReset();

    // a decoration defined here
    if (decorMask & styleText->mTextDecorationLine) {
      nscolor color;
      if (aOverrideColor) {
        color = *aOverrideColor;
      } else {
        color = styleText->mTextDecorationColor.CalcColor(*context);
      }
      uint8_t style = styleText->mTextDecorationStyle;

      if (StyleTextDecorationLine::UNDERLINE & decorMask &
          styleText->mTextDecorationLine) {
        underColor = color;
        underStyle = style;
        decorMask &= ~StyleTextDecorationLine::UNDERLINE;
        decorations |= StyleTextDecorationLine::UNDERLINE;
      }
      if (StyleTextDecorationLine::OVERLINE & decorMask &
          styleText->mTextDecorationLine) {
        overColor = color;
        overStyle = style;
        decorMask &= ~StyleTextDecorationLine::OVERLINE;
        decorations |= StyleTextDecorationLine::OVERLINE;
      }
      if (StyleTextDecorationLine::LINE_THROUGH & decorMask &
          styleText->mTextDecorationLine) {
        strikeColor = color;
        strikeStyle = style;
        decorMask &= ~StyleTextDecorationLine::LINE_THROUGH;
        decorations |= StyleTextDecorationLine::LINE_THROUGH;
      }
    }
  } while (decorMask && (f = nsLayoutUtils::GetParentOrPlaceholderFor(f)));

  RefPtr<nsFontMetrics> fontMet =
      nsLayoutUtils::GetFontMetricsForFrame(this, 1.0f);
  fontMet->SetVertical(wm.IsVertical());
  fontMet->SetTextOrientation(StyleVisibility()->mTextOrientation);

  nscoord offset;
  nscoord size;
  nscoord ascent = fontMet->MaxAscent();

  nsPoint baselinePt;
  if (wm.IsVertical()) {
    baselinePt.x = presContext->RoundAppUnitsToNearestDevPixels(
        aTextRect.x + (wm.IsVerticalRL() ? aTextRect.width - ascent : ascent));
    baselinePt.y = aTextRect.y;
  } else {
    baselinePt.x = aTextRect.x;
    baselinePt.y =
        presContext->RoundAppUnitsToNearestDevPixels(aTextRect.y + ascent);
  }

  nsCSSRendering::PaintDecorationLineParams params;
  params.dirtyRect = ToRect(presContext->AppUnitsToGfxUnits(aDirtyRect));
  params.pt = Point(presContext->AppUnitsToGfxUnits(aTextRect.x),
                    presContext->AppUnitsToGfxUnits(aTextRect.y));
  params.icoordInFrame =
      Float(PresContext()->AppUnitsToGfxUnits(mTextDrawRect.x));
  params.lineSize = Size(presContext->AppUnitsToGfxUnits(aTextRect.width), 0);
  params.ascent = presContext->AppUnitsToGfxUnits(ascent);
  params.vertical = vertical;

  // XXX todo: vertical-mode support for decorations not tested yet,
  // probably won't be positioned correctly

  // Underlines are drawn before overlines, and both before the text
  // itself, per http://www.w3.org/TR/CSS21/zindex.html point 7.2.1.4.1.1.
  // (We don't apply this rule to the access-key underline because we only
  // find out where that is as a side effect of drawing the text, in the
  // general case -- see below.)
  if (decorations & (StyleTextDecorationLine::OVERLINE |
                     StyleTextDecorationLine::UNDERLINE)) {
    fontMet->GetUnderline(offset, size);
    params.lineSize.height = presContext->AppUnitsToGfxUnits(size);
    if ((decorations & StyleTextDecorationLine::UNDERLINE) &&
        underStyle != NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
      params.color = underColor;
      params.offset = presContext->AppUnitsToGfxUnits(offset);
      params.decoration = StyleTextDecorationLine::UNDERLINE;
      params.style = underStyle;
      nsCSSRendering::PaintDecorationLine(this, *drawTarget, params);
    }
    if ((decorations & StyleTextDecorationLine::OVERLINE) &&
        overStyle != NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
      params.color = overColor;
      params.offset = params.ascent;
      params.decoration = StyleTextDecorationLine::OVERLINE;
      params.style = overStyle;
      nsCSSRendering::PaintDecorationLine(this, *drawTarget, params);
    }
  }

  RefPtr<gfxContext> refContext =
      PresShell()->CreateReferenceRenderingContext();
  DrawTarget* refDrawTarget = refContext->GetDrawTarget();

  CalculateUnderline(refDrawTarget, *fontMet);

  DeviceColor color = ToDeviceColor(
      aOverrideColor ? *aOverrideColor : StyleText()->mColor.ToColor());
  ColorPattern colorPattern(color);
  aRenderingContext.SetDeviceColor(color);

  nsresult rv = NS_ERROR_FAILURE;

  if (mState & NS_FRAME_IS_BIDI) {
    presContext->SetBidiEnabled();
    nsBidiLevel level = nsBidiPresUtils::BidiLevelFromStyle(Style());
    if (mAccessKeyInfo && mAccessKeyInfo->mAccesskeyIndex != kNotFound) {
      // We let the RenderText function calculate the mnemonic's
      // underline position for us.
      nsBidiPositionResolve posResolve;
      posResolve.logicalIndex = mAccessKeyInfo->mAccesskeyIndex;
      rv = nsBidiPresUtils::RenderText(
          mCroppedTitle.get(), mCroppedTitle.Length(), level, presContext,
          aRenderingContext, refDrawTarget, *fontMet, baselinePt.x,
          baselinePt.y, &posResolve, 1);
      mAccessKeyInfo->mBeforeWidth = posResolve.visualLeftTwips;
      mAccessKeyInfo->mAccessWidth = posResolve.visualWidth;
    } else {
      rv = nsBidiPresUtils::RenderText(
          mCroppedTitle.get(), mCroppedTitle.Length(), level, presContext,
          aRenderingContext, refDrawTarget, *fontMet, baselinePt.x,
          baselinePt.y);
    }
  }
  if (NS_FAILED(rv)) {
    fontMet->SetTextRunRTL(false);

    if (mAccessKeyInfo && mAccessKeyInfo->mAccesskeyIndex != kNotFound) {
      // In the simple (non-BiDi) case, we calculate the mnemonic's
      // underline position by getting the text metric.
      // XXX are attribute values always two byte?
      if (mAccessKeyInfo->mAccesskeyIndex > 0)
        mAccessKeyInfo->mBeforeWidth = nsLayoutUtils::AppUnitWidthOfString(
            mCroppedTitle.get(), mAccessKeyInfo->mAccesskeyIndex, *fontMet,
            refDrawTarget);
      else
        mAccessKeyInfo->mBeforeWidth = 0;
    }

    fontMet->DrawString(mCroppedTitle.get(), mCroppedTitle.Length(),
                        baselinePt.x, baselinePt.y, &aRenderingContext,
                        refDrawTarget);
  }

  if (mAccessKeyInfo && mAccessKeyInfo->mAccesskeyIndex != kNotFound) {
    nsRect r(aTextRect.x + mAccessKeyInfo->mBeforeWidth,
             aTextRect.y + mAccessKeyInfo->mAccessOffset,
             mAccessKeyInfo->mAccessWidth,
             mAccessKeyInfo->mAccessUnderlineSize);
    Rect devPxRect = NSRectToSnappedRect(r, appUnitsPerDevPixel, *drawTarget);
    drawTarget->FillRect(devPxRect, colorPattern);
  }

  // Strikeout is drawn on top of the text, per
  // http://www.w3.org/TR/CSS21/zindex.html point 7.2.1.4.1.1.
  if ((decorations & StyleTextDecorationLine::LINE_THROUGH) &&
      strikeStyle != NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
    fontMet->GetStrikeout(offset, size);
    params.color = strikeColor;
    params.lineSize.height = presContext->AppUnitsToGfxUnits(size);
    params.offset = presContext->AppUnitsToGfxUnits(offset);
    params.decoration = StyleTextDecorationLine::LINE_THROUGH;
    params.style = strikeStyle;
    nsCSSRendering::PaintDecorationLine(this, *drawTarget, params);
  }
}

void nsTextBoxFrame::CalculateUnderline(DrawTarget* aDrawTarget,
                                        nsFontMetrics& aFontMetrics) {
  if (mAccessKeyInfo && mAccessKeyInfo->mAccesskeyIndex != kNotFound) {
    // Calculate all fields of mAccessKeyInfo which
    // are the same for both BiDi and non-BiDi frames.
    const char16_t* titleString = mCroppedTitle.get();
    aFontMetrics.SetTextRunRTL(false);
    mAccessKeyInfo->mAccessWidth = nsLayoutUtils::AppUnitWidthOfString(
        titleString[mAccessKeyInfo->mAccesskeyIndex], aFontMetrics,
        aDrawTarget);

    nscoord offset, baseline;
    aFontMetrics.GetUnderline(offset, mAccessKeyInfo->mAccessUnderlineSize);
    baseline = aFontMetrics.MaxAscent();
    mAccessKeyInfo->mAccessOffset = baseline - offset;
  }
}

nscoord nsTextBoxFrame::CalculateTitleForWidth(gfxContext& aRenderingContext,
                                               nscoord aWidth) {
  DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();

  if (mTitle.IsEmpty()) {
    mCroppedTitle.Truncate();
    return 0;
  }

  RefPtr<nsFontMetrics> fm = nsLayoutUtils::GetFontMetricsForFrame(this, 1.0f);

  // see if the text will completely fit in the width given
  nscoord titleWidth = nsLayoutUtils::AppUnitWidthOfStringBidi(
      mTitle, this, *fm, aRenderingContext);
  if (titleWidth <= aWidth) {
    mCroppedTitle = mTitle;
    if (HasRTLChars(mTitle) ||
        StyleVisibility()->mDirection == StyleDirection::Rtl) {
      AddStateBits(NS_FRAME_IS_BIDI);
    }
    return titleWidth;  // fits, done.
  }

  const nsDependentString& kEllipsis = nsContentUtils::GetLocalizedEllipsis();
  if (mCropType != CropNone) {
    // start with an ellipsis
    mCroppedTitle.Assign(kEllipsis);

    // see if the width is even smaller than the ellipsis
    // if so, clear the text (XXX set as many '.' as we can?).
    fm->SetTextRunRTL(false);
    titleWidth =
        nsLayoutUtils::AppUnitWidthOfString(kEllipsis, *fm, drawTarget);

    if (titleWidth > aWidth) {
      mCroppedTitle.SetLength(0);
      return 0;
    }

    // if the ellipsis fits perfectly, no use in trying to insert
    if (titleWidth == aWidth) return titleWidth;

    aWidth -= titleWidth;
  } else {
    mCroppedTitle.Truncate(0);
    titleWidth = 0;
  }

  using mozilla::unicode::ClusterIterator;
  using mozilla::unicode::ClusterReverseIterator;

  // ok crop things
  switch (mCropType) {
    case CropAuto:
    case CropNone:
    case CropRight: {
      ClusterIterator iter(mTitle.Data(), mTitle.Length());
      const char16_t* dataBegin = iter;
      const char16_t* pos = dataBegin;
      nscoord charWidth;
      nscoord totalWidth = 0;

      while (!iter.AtEnd()) {
        iter.Next();
        const char16_t* nextPos = iter;
        ptrdiff_t length = nextPos - pos;
        charWidth =
            nsLayoutUtils::AppUnitWidthOfString(pos, length, *fm, drawTarget);
        if (totalWidth + charWidth > aWidth) {
          break;
        }

        if (UTF16_CODE_UNIT_IS_BIDI(*pos)) {
          AddStateBits(NS_FRAME_IS_BIDI);
        }
        pos = nextPos;
        totalWidth += charWidth;
      }

      if (pos == dataBegin) {
        return titleWidth;
      }

      // insert what character we can in.
      nsAutoString title(mTitle);
      title.Truncate(pos - dataBegin);
      mCroppedTitle.Insert(title, 0);
    } break;

    case CropLeft: {
      ClusterReverseIterator iter(mTitle.Data(), mTitle.Length());
      const char16_t* dataEnd = iter;
      const char16_t* prevPos = dataEnd;
      nscoord charWidth;
      nscoord totalWidth = 0;

      while (!iter.AtEnd()) {
        iter.Next();
        const char16_t* pos = iter;
        ptrdiff_t length = prevPos - pos;
        charWidth =
            nsLayoutUtils::AppUnitWidthOfString(pos, length, *fm, drawTarget);
        if (totalWidth + charWidth > aWidth) {
          break;
        }

        if (UTF16_CODE_UNIT_IS_BIDI(*pos)) {
          AddStateBits(NS_FRAME_IS_BIDI);
        }
        prevPos = pos;
        totalWidth += charWidth;
      }

      if (prevPos == dataEnd) {
        return titleWidth;
      }

      nsAutoString copy;
      mTitle.Right(copy, dataEnd - prevPos);
      mCroppedTitle += copy;
    } break;

    case CropCenter: {
      nscoord stringWidth = nsLayoutUtils::AppUnitWidthOfStringBidi(
          mTitle, this, *fm, aRenderingContext);
      if (stringWidth <= aWidth) {
        // the entire string will fit in the maximum width
        mCroppedTitle.Insert(mTitle, 0);
        break;
      }

      // determine how much of the string will fit in the max width
      nscoord charWidth = 0;
      nscoord totalWidth = 0;
      ClusterIterator leftIter(mTitle.Data(), mTitle.Length());
      ClusterReverseIterator rightIter(mTitle.Data(), mTitle.Length());
      const char16_t* dataBegin = leftIter;
      const char16_t* dataEnd = rightIter;
      const char16_t* leftPos = dataBegin;
      const char16_t* rightPos = dataEnd;
      const char16_t* pos;
      ptrdiff_t length;
      nsAutoString leftString, rightString;

      while (leftPos < rightPos) {
        leftIter.Next();
        pos = leftIter;
        length = pos - leftPos;
        charWidth = nsLayoutUtils::AppUnitWidthOfString(leftPos, length, *fm,
                                                        drawTarget);
        if (totalWidth + charWidth > aWidth) {
          break;
        }

        if (UTF16_CODE_UNIT_IS_BIDI(*leftPos)) {
          AddStateBits(NS_FRAME_IS_BIDI);
        }

        leftString.Append(leftPos, length);
        leftPos = pos;
        totalWidth += charWidth;

        if (leftPos >= rightPos) {
          break;
        }

        rightIter.Next();
        pos = rightIter;
        length = rightPos - pos;
        charWidth =
            nsLayoutUtils::AppUnitWidthOfString(pos, length, *fm, drawTarget);
        if (totalWidth + charWidth > aWidth) {
          break;
        }

        if (UTF16_CODE_UNIT_IS_BIDI(*pos)) {
          AddStateBits(NS_FRAME_IS_BIDI);
        }

        rightString.Insert(pos, 0, length);
        rightPos = pos;
        totalWidth += charWidth;
      }

      mCroppedTitle = leftString + kEllipsis + rightString;
    } break;
  }

  return nsLayoutUtils::AppUnitWidthOfStringBidi(mCroppedTitle, this, *fm,
                                                 aRenderingContext);
}

#define OLD_ELLIPSIS NS_LITERAL_STRING("...")

// the following block is to append the accesskey to mTitle if there is an
// accesskey but the mTitle doesn't have the character
void nsTextBoxFrame::UpdateAccessTitle() {
  /*
   * Note that if you change appending access key label spec,
   * you need to maintain same logic in following methods. See bug 324159.
   * toolkit/components/prompts/src/CommonDialog.jsm (setLabelForNode)
   * toolkit/content/widgets/text.js (formatAccessKey)
   */
  int32_t menuAccessKey;
  nsMenuBarListener::GetMenuAccessKey(&menuAccessKey);
  if (!menuAccessKey || mAccessKey.IsEmpty()) return;

  if (!AlwaysAppendAccessKey() &&
      FindInReadable(mAccessKey, mTitle, nsCaseInsensitiveStringComparator()))
    return;

  nsAutoString accessKeyLabel;
  accessKeyLabel += '(';
  accessKeyLabel += mAccessKey;
  ToUpperCase(accessKeyLabel);
  accessKeyLabel += ')';

  if (mTitle.IsEmpty()) {
    mTitle = accessKeyLabel;
    return;
  }

  if (StringEndsWith(mTitle, accessKeyLabel)) {
    // Never append another "(X)" if the title already ends with "(X)".
    return;
  }

  const nsDependentString& kEllipsis = nsContentUtils::GetLocalizedEllipsis();
  uint32_t offset = mTitle.Length();
  if (StringEndsWith(mTitle, kEllipsis)) {
    offset -= kEllipsis.Length();
  } else if (StringEndsWith(mTitle, OLD_ELLIPSIS)) {
    // Try to check with our old ellipsis (for old addons)
    offset -= OLD_ELLIPSIS.Length();
  } else {
    // Try to check with
    // our default ellipsis (for non-localized addons) or ':'
    const char16_t kLastChar = mTitle.Last();
    if (kLastChar == char16_t(0x2026) || kLastChar == char16_t(':')) offset--;
  }

  if (InsertSeparatorBeforeAccessKey() && offset > 0 &&
      !NS_IS_SPACE(mTitle[offset - 1])) {
    mTitle.Insert(' ', offset);
    offset++;
  }

  mTitle.Insert(accessKeyLabel, offset);
}

void nsTextBoxFrame::UpdateAccessIndex() {
  int32_t menuAccessKey;
  nsMenuBarListener::GetMenuAccessKey(&menuAccessKey);
  if (menuAccessKey) {
    if (mAccessKey.IsEmpty()) {
      if (mAccessKeyInfo) {
        delete mAccessKeyInfo;
        mAccessKeyInfo = nullptr;
      }
    } else {
      if (!mAccessKeyInfo) {
        mAccessKeyInfo = new nsAccessKeyInfo();
        if (!mAccessKeyInfo) return;
      }

      nsAString::const_iterator start, end;

      mCroppedTitle.BeginReading(start);
      mCroppedTitle.EndReading(end);

      // remember the beginning of the string
      nsAString::const_iterator originalStart = start;

      bool found;
      if (!AlwaysAppendAccessKey()) {
        // not appending access key - do case-sensitive search
        // first
        found = FindInReadable(mAccessKey, start, end);
        if (!found) {
          // didn't find it - perform a case-insensitive search
          start = originalStart;
          found = FindInReadable(mAccessKey, start, end,
                                 nsCaseInsensitiveStringComparator());
        }
      } else {
        found = RFindInReadable(mAccessKey, start, end,
                                nsCaseInsensitiveStringComparator());
      }

      if (found)
        mAccessKeyInfo->mAccesskeyIndex = Distance(originalStart, start);
      else
        mAccessKeyInfo->mAccesskeyIndex = kNotFound;
    }
  }
}

void nsTextBoxFrame::RecomputeTitle() {
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::value, mTitle);

  // This doesn't handle language-specific uppercasing/lowercasing
  // rules, unlike textruns.
  StyleTextTransform textTransform = StyleText()->mTextTransform;
  if (textTransform.case_ == StyleTextTransformCase::Uppercase) {
    ToUpperCase(mTitle);
  } else if (textTransform.case_ == StyleTextTransformCase::Lowercase) {
    ToLowerCase(mTitle);
  }
  // We can't handle StyleTextTransformCase::Capitalize because we
  // have no clue about word boundaries here.  We also don't handle
  // the full-width or full-size-kana transforms.
}

void nsTextBoxFrame::DidSetComputedStyle(ComputedStyle* aOldComputedStyle) {
  nsLeafBoxFrame::DidSetComputedStyle(aOldComputedStyle);

  if (!aOldComputedStyle) {
    // We're just being initialized
    return;
  }

  const nsStyleText* oldTextStyle = aOldComputedStyle->StyleText();
  if (oldTextStyle->mTextTransform != StyleText()->mTextTransform) {
    RecomputeTitle();
    UpdateAccessTitle();
  }
}

NS_IMETHODIMP
nsTextBoxFrame::DoXULLayout(nsBoxLayoutState& aBoxLayoutState) {
  if (mNeedsReflowCallback) {
    nsIReflowCallback* cb = new nsAsyncAccesskeyUpdate(this);
    if (cb) {
      PresShell()->PostReflowCallback(cb);
    }
    mNeedsReflowCallback = false;
  }

  nsresult rv = nsLeafBoxFrame::DoXULLayout(aBoxLayoutState);

  CalcDrawRect(*aBoxLayoutState.GetRenderingContext());

  const nsStyleText* textStyle = StyleText();

  nsRect scrollBounds(nsPoint(0, 0), GetSize());
  nsRect textRect = mTextDrawRect;

  RefPtr<nsFontMetrics> fontMet =
      nsLayoutUtils::GetFontMetricsForFrame(this, 1.0f);
  nsBoundingMetrics metrics = fontMet->GetInkBoundsForVisualOverflow(
      mCroppedTitle.get(), mCroppedTitle.Length(),
      aBoxLayoutState.GetRenderingContext()->GetDrawTarget());

  WritingMode wm = GetWritingMode();
  LogicalRect tr(wm, textRect, GetSize());

  tr.IStart(wm) -= metrics.leftBearing;
  tr.ISize(wm) = metrics.width;
  // In DrawText() we always draw with the baseline at MaxAscent() (relative to
  // mTextDrawRect),
  tr.BStart(wm) += fontMet->MaxAscent() - metrics.ascent;
  tr.BSize(wm) = metrics.ascent + metrics.descent;

  textRect = tr.GetPhysicalRect(wm, GetSize());

  // Our scrollable overflow is our bounds; our visual overflow may
  // extend beyond that.
  nsRect visualBounds;
  visualBounds.UnionRect(scrollBounds, textRect);
  nsOverflowAreas overflow(visualBounds, scrollBounds);

  if (textStyle->HasTextShadow()) {
    // text-shadow extends our visual but not scrollable bounds
    nsRect& vis = overflow.VisualOverflow();
    vis.UnionRect(vis,
                  nsLayoutUtils::GetTextShadowRectsUnion(mTextDrawRect, this));
  }
  FinishAndStoreOverflow(overflow, GetSize());

  return rv;
}

nsRect nsTextBoxFrame::GetComponentAlphaBounds() const {
  if (StyleText()->HasTextShadow()) {
    return GetVisualOverflowRectRelativeToSelf();
  }
  return mTextDrawRect;
}

bool nsTextBoxFrame::XULComputesOwnOverflowArea() { return true; }

/* virtual */
void nsTextBoxFrame::MarkIntrinsicISizesDirty() {
  mNeedsRecalc = true;
  nsLeafBoxFrame::MarkIntrinsicISizesDirty();
}

void nsTextBoxFrame::GetTextSize(gfxContext& aRenderingContext,
                                 const nsString& aString, nsSize& aSize,
                                 nscoord& aAscent) {
  RefPtr<nsFontMetrics> fontMet =
      nsLayoutUtils::GetFontMetricsForFrame(this, 1.0f);
  aSize.height = fontMet->MaxHeight();
  aSize.width = nsLayoutUtils::AppUnitWidthOfStringBidi(aString, this, *fontMet,
                                                        aRenderingContext);
  aAscent = fontMet->MaxAscent();
}

void nsTextBoxFrame::CalcTextSize(nsBoxLayoutState& aBoxLayoutState) {
  if (mNeedsRecalc) {
    nsSize size;
    gfxContext* rendContext = aBoxLayoutState.GetRenderingContext();
    if (rendContext) {
      GetTextSize(*rendContext, mTitle, size, mAscent);
      if (GetWritingMode().IsVertical()) {
        std::swap(size.width, size.height);
      }
      mTextSize = size;
      mNeedsRecalc = false;
    }
  }
}

void nsTextBoxFrame::CalcDrawRect(gfxContext& aRenderingContext) {
  WritingMode wm = GetWritingMode();

  LogicalRect textRect(wm, LogicalPoint(wm, 0, 0), GetLogicalSize(wm));
  nsMargin borderPadding;
  GetXULBorderAndPadding(borderPadding);
  textRect.Deflate(wm, LogicalMargin(wm, borderPadding));

  // determine (cropped) title and underline position
  // determine (cropped) title which fits in aRect, and its width
  // (where "width" is the text measure along its baseline, i.e. actually
  // a physical height in vertical writing modes)
  nscoord titleWidth =
      CalculateTitleForWidth(aRenderingContext, textRect.ISize(wm));

#ifdef ACCESSIBILITY
  // Make sure to update the accessible tree in case when cropped title is
  // changed.
  nsAccessibilityService* accService = GetAccService();
  if (accService) {
    accService->UpdateLabelValue(PresShell(), mContent, mCroppedTitle);
  }
#endif

  // determine if and at which position to put the underline
  UpdateAccessIndex();

  // make the rect as small as our (cropped) text.
  nscoord outerISize = textRect.ISize(wm);
  textRect.ISize(wm) = titleWidth;

  // Align our text within the overall rect by checking our text-align property.
  const nsStyleText* textStyle = StyleText();
  if (textStyle->mTextAlign == StyleTextAlign::Center) {
    textRect.IStart(wm) += (outerISize - textRect.ISize(wm)) / 2;
  } else if (textStyle->mTextAlign == StyleTextAlign::End ||
             (textStyle->mTextAlign == StyleTextAlign::Left &&
              wm.IsBidiRTL()) ||
             (textStyle->mTextAlign == StyleTextAlign::Right &&
              wm.IsBidiLTR())) {
    textRect.IStart(wm) += (outerISize - textRect.ISize(wm));
  }

  mTextDrawRect = textRect.GetPhysicalRect(wm, GetSize());
}

/**
 * Ok return our dimensions
 */
nsSize nsTextBoxFrame::GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState) {
  CalcTextSize(aBoxLayoutState);

  nsSize size = mTextSize;
  DISPLAY_PREF_SIZE(this, size);

  AddXULBorderAndPadding(size);
  bool widthSet, heightSet;
  nsIFrame::AddXULPrefSize(this, size, widthSet, heightSet);

  return size;
}

/**
 * Ok return our dimensions
 */
nsSize nsTextBoxFrame::GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) {
  CalcTextSize(aBoxLayoutState);

  nsSize size = mTextSize;
  DISPLAY_MIN_SIZE(this, size);

  // if there is cropping our min width becomes our border and padding
  if (mCropType != CropNone && mCropType != CropAuto) {
    if (GetWritingMode().IsVertical()) {
      size.height = 0;
    } else {
      size.width = 0;
    }
  }

  AddXULBorderAndPadding(size);
  bool widthSet, heightSet;
  nsIFrame::AddXULMinSize(this, size, widthSet, heightSet);

  return size;
}

nscoord nsTextBoxFrame::GetXULBoxAscent(nsBoxLayoutState& aBoxLayoutState) {
  CalcTextSize(aBoxLayoutState);

  nscoord ascent = mAscent;

  nsMargin m(0, 0, 0, 0);
  GetXULBorderAndPadding(m);

  WritingMode wm = GetWritingMode();
  ascent += LogicalMargin(wm, m).BStart(wm);

  return ascent;
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsTextBoxFrame::GetFrameName(nsAString& aResult) const {
  MakeFrameName(NS_LITERAL_STRING("TextBox"), aResult);
  aResult += NS_LITERAL_STRING("[value=") + mTitle + NS_LITERAL_STRING("]");
  return NS_OK;
}
#endif

// If you make changes to this function, check its counterparts
// in nsBoxFrame and nsXULLabelFrame
nsresult nsTextBoxFrame::RegUnregAccessKey(bool aDoReg) {
  // if we have no content, we can't do anything
  if (!mContent) return NS_ERROR_FAILURE;

  // check if we have a |control| attribute
  // do this check first because few elements have control attributes, and we
  // can weed out most of the elements quickly.

  // XXXjag a side-effect is that we filter out anonymous <label>s
  // in e.g. <menu>, <menuitem>, <button>. These <label>s inherit
  // |accesskey| and would otherwise register themselves, overwriting
  // the content we really meant to be registered.
  if (!mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::control))
    return NS_OK;

  // see if we even have an access key
  nsAutoString accessKey;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey,
                                 accessKey);

  if (accessKey.IsEmpty()) return NS_OK;

  // With a valid PresContext we can get the ESM
  // and (un)register the access key
  EventStateManager* esm = PresContext()->EventStateManager();

  uint32_t key = accessKey.First();
  if (aDoReg)
    esm->RegisterAccessKey(mContent->AsElement(), key);
  else
    esm->UnregisterAccessKey(mContent->AsElement(), key);

  return NS_OK;
}
