/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleCaret.h"

#include "AccessibleCaretLogger.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/ToString.h"
#include "nsCanvasFrame.h"
#include "nsCaret.h"
#include "nsCSSFrameConstructor.h"
#include "nsDOMTokenList.h"
#include "nsIFrame.h"
#include "nsPlaceholderFrame.h"

namespace mozilla {
using namespace dom;

#undef AC_LOG
#define AC_LOG(message, ...) \
  AC_LOG_BASE("AccessibleCaret (%p): " message, this, ##__VA_ARGS__);

#undef AC_LOGV
#define AC_LOGV(message, ...) \
  AC_LOGV_BASE("AccessibleCaret (%p): " message, this, ##__VA_ARGS__);

NS_IMPL_ISUPPORTS(AccessibleCaret::DummyTouchListener, nsIDOMEventListener)

NS_NAMED_LITERAL_STRING(AccessibleCaret::sTextOverlayElementId, "text-overlay");
NS_NAMED_LITERAL_STRING(AccessibleCaret::sCaretImageElementId, "image");

#define AC_PROCESS_ENUM_TO_STREAM(e) \
  case (e):                          \
    aStream << #e;                   \
    break;
std::ostream& operator<<(std::ostream& aStream,
                         const AccessibleCaret::Appearance& aAppearance) {
  using Appearance = AccessibleCaret::Appearance;
  switch (aAppearance) {
    AC_PROCESS_ENUM_TO_STREAM(Appearance::None);
    AC_PROCESS_ENUM_TO_STREAM(Appearance::Normal);
    AC_PROCESS_ENUM_TO_STREAM(Appearance::NormalNotShown);
    AC_PROCESS_ENUM_TO_STREAM(Appearance::Left);
    AC_PROCESS_ENUM_TO_STREAM(Appearance::Right);
  }
  return aStream;
}

std::ostream& operator<<(
    std::ostream& aStream,
    const AccessibleCaret::PositionChangedResult& aResult) {
  using PositionChangedResult = AccessibleCaret::PositionChangedResult;
  switch (aResult) {
    AC_PROCESS_ENUM_TO_STREAM(PositionChangedResult::NotChanged);
    AC_PROCESS_ENUM_TO_STREAM(PositionChangedResult::Changed);
    AC_PROCESS_ENUM_TO_STREAM(PositionChangedResult::Invisible);
  }
  return aStream;
}
#undef AC_PROCESS_ENUM_TO_STREAM

// -----------------------------------------------------------------------------
// Implementation of AccessibleCaret methods

AccessibleCaret::AccessibleCaret(PresShell* aPresShell)
    : mPresShell(aPresShell) {
  // Check all resources required.
  if (mPresShell) {
    MOZ_ASSERT(RootFrame());
    MOZ_ASSERT(mPresShell->GetDocument());
    InjectCaretElement(mPresShell->GetDocument());
  }
}

AccessibleCaret::~AccessibleCaret() {
  if (mPresShell) {
    RemoveCaretElement(mPresShell->GetDocument());
  }
}

void AccessibleCaret::SetAppearance(Appearance aAppearance) {
  if (mAppearance == aAppearance) {
    return;
  }

  ErrorResult rv;
  CaretElement().ClassList()->Remove(AppearanceString(mAppearance), rv);
  MOZ_ASSERT(!rv.Failed(), "Remove old appearance failed!");

  CaretElement().ClassList()->Add(AppearanceString(aAppearance), rv);
  MOZ_ASSERT(!rv.Failed(), "Add new appearance failed!");

  AC_LOG("%s: %s -> %s", __FUNCTION__, ToString(mAppearance).c_str(),
         ToString(aAppearance).c_str());

  mAppearance = aAppearance;

  // Need to reset rect since the cached rect will be compared in SetPosition.
  if (mAppearance == Appearance::None) {
    mImaginaryCaretRect = nsRect();
    mZoomLevel = 0.0f;
  }
}

/* static */
nsAutoString AccessibleCaret::AppearanceString(Appearance aAppearance) {
  nsAutoString string;
  switch (aAppearance) {
    case Appearance::None:
    case Appearance::NormalNotShown:
      string = NS_LITERAL_STRING("none");
      break;
    case Appearance::Normal:
      string = NS_LITERAL_STRING("normal");
      break;
    case Appearance::Right:
      string = NS_LITERAL_STRING("right");
      break;
    case Appearance::Left:
      string = NS_LITERAL_STRING("left");
      break;
  }
  return string;
}

bool AccessibleCaret::Intersects(const AccessibleCaret& aCaret) const {
  MOZ_ASSERT(mPresShell == aCaret.mPresShell);

  if (!IsVisuallyVisible() || !aCaret.IsVisuallyVisible()) {
    return false;
  }

  nsRect rect =
      nsLayoutUtils::GetRectRelativeToFrame(&CaretElement(), RootFrame());
  nsRect rhsRect = nsLayoutUtils::GetRectRelativeToFrame(&aCaret.CaretElement(),
                                                         RootFrame());
  return rect.Intersects(rhsRect);
}

bool AccessibleCaret::Contains(const nsPoint& aPoint,
                               TouchArea aTouchArea) const {
  if (!IsVisuallyVisible()) {
    return false;
  }

  nsRect textOverlayRect =
      nsLayoutUtils::GetRectRelativeToFrame(TextOverlayElement(), RootFrame());
  nsRect caretImageRect =
      nsLayoutUtils::GetRectRelativeToFrame(CaretImageElement(), RootFrame());

  if (aTouchArea == TouchArea::CaretImage) {
    return caretImageRect.Contains(aPoint);
  }

  MOZ_ASSERT(aTouchArea == TouchArea::Full, "Unexpected TouchArea type!");
  return textOverlayRect.Contains(aPoint) || caretImageRect.Contains(aPoint);
}

void AccessibleCaret::EnsureApzAware() {
  // If the caret element was cloned, the listener might have been lost. So
  // if that's the case we register a dummy listener if there isn't one on
  // the element already.
  if (!CaretElement().IsApzAware()) {
    // FIXME(emilio): Is this needed anymore?
    CaretElement().AddEventListener(NS_LITERAL_STRING("touchstart"),
                                    mDummyTouchListener, false);
  }
}

void AccessibleCaret::InjectCaretElement(Document* aDocument) {
  ErrorResult rv;
  RefPtr<Element> element = CreateCaretElement(aDocument);
  mCaretElementHolder = aDocument->InsertAnonymousContent(*element, rv);

  MOZ_ASSERT(!rv.Failed(), "Insert anonymous content should not fail!");
  MOZ_ASSERT(mCaretElementHolder, "We must have anonymous content!");

  // InsertAnonymousContent will clone the element to make an AnonymousContent.
  // Since event listeners are not being cloned when cloning a node, we need to
  // add the listener here.
  EnsureApzAware();
}

already_AddRefed<Element> AccessibleCaret::CreateCaretElement(
    Document* aDocument) const {
  // Content structure of AccessibleCaret
  // <div class="moz-accessiblecaret">  <- CaretElement()
  //   <div id="text-overlay"           <- TextOverlayElement()
  //   <div id="image">                 <- CaretImageElement()

  ErrorResult rv;
  RefPtr<Element> parent = aDocument->CreateHTMLElement(nsGkAtoms::div);
  parent->ClassList()->Add(NS_LITERAL_STRING("moz-accessiblecaret"), rv);
  parent->ClassList()->Add(NS_LITERAL_STRING("none"), rv);

  auto CreateAndAppendChildElement =
      [aDocument, &parent](const nsLiteralString& aElementId) {
        RefPtr<Element> child = aDocument->CreateHTMLElement(nsGkAtoms::div);
        child->SetAttr(kNameSpaceID_None, nsGkAtoms::id, aElementId, true);
        parent->AppendChildTo(child, false);
      };

  CreateAndAppendChildElement(sTextOverlayElementId);
  CreateAndAppendChildElement(sCaretImageElementId);

  return parent.forget();
}

void AccessibleCaret::RemoveCaretElement(Document* aDocument) {
  CaretElement().RemoveEventListener(NS_LITERAL_STRING("touchstart"),
                                     mDummyTouchListener, false);

  // FIXME(emilio): This shouldn't be needed and should be done by
  // ContentRemoved via RemoveAnonymousContent, but the current setup tears down
  // the accessible caret manager after the shell has stopped observing the
  // document, but before the frame tree has gone away. This could clearly be
  // better...
  if (nsIFrame* frame = CaretElement().GetPrimaryFrame()) {
    if (frame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW)) {
      frame = frame->GetPlaceholderFrame();
    }
    nsAutoScriptBlocker scriptBlocker;
    frame->GetParent()->RemoveFrame(nsIFrame::kPrincipalList, frame);
  }

  aDocument->RemoveAnonymousContent(*mCaretElementHolder, IgnoreErrors());
}

AccessibleCaret::PositionChangedResult AccessibleCaret::SetPosition(
    nsIFrame* aFrame, int32_t aOffset) {
  if (!CustomContentContainerFrame()) {
    return PositionChangedResult::NotChanged;
  }

  nsRect imaginaryCaretRectInFrame =
      nsCaret::GetGeometryForFrame(aFrame, aOffset, nullptr);

  imaginaryCaretRectInFrame =
      nsLayoutUtils::ClampRectToScrollFrames(aFrame, imaginaryCaretRectInFrame);

  if (imaginaryCaretRectInFrame.IsEmpty()) {
    // Don't bother to set the caret position since it's invisible.
    mImaginaryCaretRect = nsRect();
    mZoomLevel = 0.0f;
    return PositionChangedResult::Invisible;
  }

  nsRect imaginaryCaretRect = imaginaryCaretRectInFrame;
  nsLayoutUtils::TransformRect(aFrame, RootFrame(), imaginaryCaretRect);
  float zoomLevel = GetZoomLevel();

  if (imaginaryCaretRect.IsEqualEdges(mImaginaryCaretRect) &&
      FuzzyEqualsMultiplicative(zoomLevel, mZoomLevel)) {
    return PositionChangedResult::NotChanged;
  }

  mImaginaryCaretRect = imaginaryCaretRect;
  mZoomLevel = zoomLevel;

  // SetCaretElementStyle() requires the input rect relative to container frame.
  nsRect imaginaryCaretRectInContainerFrame = imaginaryCaretRectInFrame;
  nsLayoutUtils::TransformRect(aFrame, CustomContentContainerFrame(),
                               imaginaryCaretRectInContainerFrame);
  SetCaretElementStyle(imaginaryCaretRectInContainerFrame, mZoomLevel);

  return PositionChangedResult::Changed;
}

nsIFrame* AccessibleCaret::CustomContentContainerFrame() const {
  nsCanvasFrame* canvasFrame = mPresShell->GetCanvasFrame();
  Element* container = canvasFrame->GetCustomContentContainer();
  nsIFrame* containerFrame = container->GetPrimaryFrame();
  return containerFrame;
}

void AccessibleCaret::SetCaretElementStyle(const nsRect& aRect,
                                           float aZoomLevel) {
  nsPoint position = CaretElementPosition(aRect);
  nsAutoString styleStr;
  styleStr.AppendPrintf(
      "left: %dpx; top: %dpx; "
      "width: ",
      nsPresContext::AppUnitsToIntCSSPixels(position.x),
      nsPresContext::AppUnitsToIntCSSPixels(position.y));
  // We can't use AppendPrintf here, because it does locale-specific
  // formatting of floating-point values.
  styleStr.AppendFloat(StaticPrefs::layout_accessiblecaret_width() /
                       aZoomLevel);
  styleStr.AppendLiteral("px; height: ");
  styleStr.AppendFloat(StaticPrefs::layout_accessiblecaret_height() /
                       aZoomLevel);
  styleStr.AppendLiteral("px; margin-left: ");
  styleStr.AppendFloat(StaticPrefs::layout_accessiblecaret_margin_left() /
                       aZoomLevel);
  styleStr.AppendLiteral("px");

  CaretElement().SetAttr(kNameSpaceID_None, nsGkAtoms::style, styleStr, true);
  AC_LOG("%s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(styleStr).get());

  // Set style string for children.
  SetTextOverlayElementStyle(aRect, aZoomLevel);
  SetCaretImageElementStyle(aRect, aZoomLevel);
}

void AccessibleCaret::SetTextOverlayElementStyle(const nsRect& aRect,
                                                 float aZoomLevel) {
  nsAutoString styleStr;
  styleStr.AppendPrintf("height: %dpx;",
                        nsPresContext::AppUnitsToIntCSSPixels(aRect.height));
  TextOverlayElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::style, styleStr,
                                true);
  AC_LOG("%s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(styleStr).get());
}

void AccessibleCaret::SetCaretImageElementStyle(const nsRect& aRect,
                                                float aZoomLevel) {
  nsAutoString styleStr;
  styleStr.AppendPrintf("margin-top: %dpx;",
                        nsPresContext::AppUnitsToIntCSSPixels(aRect.height));
  CaretImageElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::style, styleStr,
                               true);
  AC_LOG("%s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(styleStr).get());
}

float AccessibleCaret::GetZoomLevel() {
  // Full zoom on desktop.
  float fullZoom = mPresShell->GetPresContext()->GetFullZoom();

  // Pinch-zoom on fennec.
  float resolution = mPresShell->GetCumulativeResolution();

  return fullZoom * resolution;
}

}  // namespace mozilla
