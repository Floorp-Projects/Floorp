/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleCaret.h"

#include "AccessibleCaretLogger.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/ToString.h"
#include "nsCanvasFrame.h"
#include "nsCaret.h"
#include "nsCSSFrameConstructor.h"
#include "nsDOMTokenList.h"
#include "nsGenericHTMLElement.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
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

static constexpr auto kTextOverlayElementId = u"text-overlay"_ns;
static constexpr auto kCaretImageElementId = u"image"_ns;

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
    AC_PROCESS_ENUM_TO_STREAM(PositionChangedResult::Position);
    AC_PROCESS_ENUM_TO_STREAM(PositionChangedResult::Zoom);
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
    MOZ_ASSERT(mPresShell->GetDocument());
    InjectCaretElement(mPresShell->GetDocument());
  }
}

AccessibleCaret::~AccessibleCaret() {
  if (mPresShell) {
    RemoveCaretElement(mPresShell->GetDocument());
  }
}

dom::Element* AccessibleCaret::TextOverlayElement() const {
  return mCaretElementHolder->Root()->GetElementById(kTextOverlayElementId);
}

dom::Element* AccessibleCaret::CaretImageElement() const {
  return mCaretElementHolder->Root()->GetElementById(kCaretImageElementId);
}

void AccessibleCaret::SetAppearance(Appearance aAppearance) {
  if (mAppearance == aAppearance) {
    return;
  }

  IgnoredErrorResult rv;
  CaretElement().ClassList()->Remove(AppearanceString(mAppearance), rv);
  MOZ_ASSERT(!rv.Failed(), "Remove old appearance failed!");

  CaretElement().ClassList()->Add(AppearanceString(aAppearance), rv);
  MOZ_ASSERT(!rv.Failed(), "Add new appearance failed!");

  AC_LOG("%s: %s -> %s", __FUNCTION__, ToString(mAppearance).c_str(),
         ToString(aAppearance).c_str());

  mAppearance = aAppearance;

  // Need to reset rect since the cached rect will be compared in SetPosition.
  if (mAppearance == Appearance::None) {
    ClearCachedData();
  }
}

/* static */
nsAutoString AccessibleCaret::AppearanceString(Appearance aAppearance) {
  nsAutoString string;
  switch (aAppearance) {
    case Appearance::None:
      string = u"none"_ns;
      break;
    case Appearance::NormalNotShown:
      string = u"hidden"_ns;
      break;
    case Appearance::Normal:
      string = u"normal"_ns;
      break;
    case Appearance::Right:
      string = u"right"_ns;
      break;
    case Appearance::Left:
      string = u"left"_ns;
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
    CaretElement().AddEventListener(u"touchstart"_ns, mDummyTouchListener,
                                    false);
  }
}

bool AccessibleCaret::IsInPositionFixedSubtree() const {
  return nsLayoutUtils::IsInPositionFixedSubtree(
      mImaginaryCaretReferenceFrame.GetFrame());
}

void AccessibleCaret::InjectCaretElement(Document* aDocument) {
  mCaretElementHolder =
      aDocument->InsertAnonymousContent(/* aForce = */ false, IgnoreErrors());
  MOZ_RELEASE_ASSERT(mCaretElementHolder, "We must have anonymous content!");

  CreateCaretElement();
  EnsureApzAware();
}

void AccessibleCaret::CreateCaretElement() const {
  // Content structure of AccessibleCaret
  // <div class="moz-accessiblecaret">  <- CaretElement()
  //   <#shadow-root>
  //     <link rel="stylesheet" href="accessiblecaret.css">
  //     <div id="text-overlay">          <- TextOverlayElement()
  //     <div id="image">                 <- CaretImageElement()

  constexpr bool kNotify = false;

  Element& host = CaretElement();
  host.SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
               u"moz-accessiblecaret none"_ns, kNotify);

  ShadowRoot* root = mCaretElementHolder->Root();
  Document* doc = host.OwnerDoc();
  {
    RefPtr<NodeInfo> linkNodeInfo = doc->NodeInfoManager()->GetNodeInfo(
        nsGkAtoms::link, nullptr, kNameSpaceID_XHTML, nsINode::ELEMENT_NODE);
    RefPtr<nsGenericHTMLElement> link =
        NS_NewHTMLLinkElement(linkNodeInfo.forget());
    if (NS_WARN_IF(!link)) {
      return;
    }
    link->SetAttr(nsGkAtoms::rel, u"stylesheet"_ns, IgnoreErrors());
    link->SetAttr(nsGkAtoms::href,
                  u"resource://content-accessible/accessiblecaret.css"_ns,
                  IgnoreErrors());
    root->AppendChildTo(link, kNotify, IgnoreErrors());
  }

  auto CreateAndAppendChildElement = [&](const nsLiteralString& aElementId) {
    RefPtr<Element> child = doc->CreateHTMLElement(nsGkAtoms::div);
    child->SetAttr(kNameSpaceID_None, nsGkAtoms::id, aElementId, kNotify);
    mCaretElementHolder->Root()->AppendChildTo(child, kNotify, IgnoreErrors());
  };

  CreateAndAppendChildElement(kTextOverlayElementId);
  CreateAndAppendChildElement(kCaretImageElementId);
}

void AccessibleCaret::RemoveCaretElement(Document* aDocument) {
  CaretElement().RemoveEventListener(u"touchstart"_ns, mDummyTouchListener,
                                     false);

  aDocument->RemoveAnonymousContent(*mCaretElementHolder);
}

void AccessibleCaret::ClearCachedData() {
  mImaginaryCaretRect = nsRect();
  mImaginaryCaretRectInContainerFrame = nsRect();
  mImaginaryCaretReferenceFrame = nullptr;
  mZoomLevel = 0.0f;
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
    ClearCachedData();
    return PositionChangedResult::Invisible;
  }

  // SetCaretElementStyle() requires the input rect relative to the custom
  // content container frame.
  nsRect imaginaryCaretRectInContainerFrame = imaginaryCaretRectInFrame;
  nsLayoutUtils::TransformRect(aFrame, CustomContentContainerFrame(),
                               imaginaryCaretRectInContainerFrame);
  const float zoomLevel = GetZoomLevel();
  const bool isSamePosition = imaginaryCaretRectInContainerFrame.IsEqualEdges(
      mImaginaryCaretRectInContainerFrame);
  const bool isSameZoomLevel = FuzzyEqualsMultiplicative(zoomLevel, mZoomLevel);

  // Always update cached mImaginaryCaretRect (relative to the root frame)
  // because it can change when the caret is scrolled.
  mImaginaryCaretRect = imaginaryCaretRectInFrame;
  nsLayoutUtils::TransformRect(aFrame, RootFrame(), mImaginaryCaretRect);

  if (isSamePosition && isSameZoomLevel) {
    return PositionChangedResult::NotChanged;
  }

  mImaginaryCaretRectInContainerFrame = imaginaryCaretRectInContainerFrame;
  mImaginaryCaretReferenceFrame = aFrame;
  mZoomLevel = zoomLevel;

  SetCaretElementStyle(imaginaryCaretRectInContainerFrame, mZoomLevel);

  return isSamePosition ? PositionChangedResult::Zoom
                        : PositionChangedResult::Position;
}

nsIFrame* AccessibleCaret::RootFrame() const {
  return mPresShell->GetRootFrame();
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
  // We can't use AppendPrintf here, because it does locale-specific
  // formatting of floating-point values.
  styleStr.AppendLiteral("left: ");
  styleStr.AppendFloat(nsPresContext::AppUnitsToFloatCSSPixels(position.x));
  styleStr.AppendLiteral("px; top: ");
  styleStr.AppendFloat(nsPresContext::AppUnitsToFloatCSSPixels(position.y));
  styleStr.AppendLiteral("px; width: ");
  styleStr.AppendFloat(StaticPrefs::layout_accessiblecaret_width() /
                       aZoomLevel);
  styleStr.AppendLiteral("px; margin-left: ");
  styleStr.AppendFloat(StaticPrefs::layout_accessiblecaret_margin_left() /
                       aZoomLevel);
  styleStr.AppendLiteral("px; transition-duration: ");
  styleStr.AppendFloat(
      StaticPrefs::layout_accessiblecaret_transition_duration());
  styleStr.AppendLiteral("ms");

  CaretElement().SetAttr(kNameSpaceID_None, nsGkAtoms::style, styleStr, true);
  AC_LOG("%s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(styleStr).get());

  // Set style string for children.
  SetTextOverlayElementStyle(aRect, aZoomLevel);
  SetCaretImageElementStyle(aRect, aZoomLevel);
}

void AccessibleCaret::SetTextOverlayElementStyle(const nsRect& aRect,
                                                 float aZoomLevel) {
  nsAutoString styleStr;
  styleStr.AppendLiteral("height: ");
  styleStr.AppendFloat(nsPresContext::AppUnitsToFloatCSSPixels(aRect.height));
  styleStr.AppendLiteral("px;");
  TextOverlayElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::style, styleStr,
                                true);
  AC_LOG("%s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(styleStr).get());
}

void AccessibleCaret::SetCaretImageElementStyle(const nsRect& aRect,
                                                float aZoomLevel) {
  nsAutoString styleStr;
  styleStr.AppendLiteral("height: ");
  styleStr.AppendFloat(StaticPrefs::layout_accessiblecaret_height() /
                       aZoomLevel);
  styleStr.AppendLiteral("px;");
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
