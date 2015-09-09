/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleCaret.h"

#include "AccessibleCaretLogger.h"
#include "mozilla/Preferences.h"
#include "mozilla/ToString.h"
#include "nsCanvasFrame.h"
#include "nsCaret.h"
#include "nsDOMTokenList.h"
#include "nsIFrame.h"

namespace mozilla {
using namespace dom;

#undef AC_LOG
#define AC_LOG(message, ...)                                                   \
  AC_LOG_BASE("AccessibleCaret (%p): " message, this, ##__VA_ARGS__);

#undef AC_LOGV
#define AC_LOGV(message, ...)                                                  \
  AC_LOGV_BASE("AccessibleCaret (%p): " message, this, ##__VA_ARGS__);

NS_IMPL_ISUPPORTS(AccessibleCaret::DummyTouchListener, nsIDOMEventListener)

float AccessibleCaret::sWidth = 0.0f;
float AccessibleCaret::sHeight = 0.0f;
float AccessibleCaret::sMarginLeft = 0.0f;
float AccessibleCaret::sBarWidth = 0.0f;

std::ostream&
operator<<(std::ostream& aStream, const AccessibleCaret::Appearance& aAppearance)
{
  using Appearance = AccessibleCaret::Appearance;

#define AC_PROCESS_APPEARANCE_TO_STREAM(e) case(e): aStream << #e; break;
  switch (aAppearance) {
    AC_PROCESS_APPEARANCE_TO_STREAM(Appearance::None);
    AC_PROCESS_APPEARANCE_TO_STREAM(Appearance::Normal);
    AC_PROCESS_APPEARANCE_TO_STREAM(Appearance::NormalNotShown);
    AC_PROCESS_APPEARANCE_TO_STREAM(Appearance::Left);
    AC_PROCESS_APPEARANCE_TO_STREAM(Appearance::Right);
  }
#undef AC_PROCESS_APPEARANCE_TO_STREAM

  return aStream;
}

// -----------------------------------------------------------------------------
// Implementation of AccessibleCaret methods

AccessibleCaret::AccessibleCaret(nsIPresShell* aPresShell)
  : mPresShell(aPresShell)
{
  // Check all resources required.
  MOZ_ASSERT(mPresShell);
  MOZ_ASSERT(RootFrame());
  MOZ_ASSERT(mPresShell->GetDocument());
  MOZ_ASSERT(mPresShell->GetCanvasFrame());
  MOZ_ASSERT(mPresShell->GetCanvasFrame()->GetCustomContentContainer());

  InjectCaretElement(mPresShell->GetDocument());

  static bool prefsAdded = false;
  if (!prefsAdded) {
    Preferences::AddFloatVarCache(&sWidth, "layout.accessiblecaret.width");
    Preferences::AddFloatVarCache(&sHeight, "layout.accessiblecaret.height");
    Preferences::AddFloatVarCache(&sMarginLeft, "layout.accessiblecaret.margin-left");
    Preferences::AddFloatVarCache(&sBarWidth, "layout.accessiblecaret.bar.width");
    prefsAdded = true;
  }
}

AccessibleCaret::~AccessibleCaret()
{
  RemoveCaretElement(mPresShell->GetDocument());
}

void
AccessibleCaret::SetAppearance(Appearance aAppearance)
{
  if (mAppearance == aAppearance) {
    return;
  }

  ErrorResult rv;
  CaretElement()->ClassList()->Remove(AppearanceString(mAppearance), rv);
  MOZ_ASSERT(!rv.Failed(), "Remove old appearance failed!");

  CaretElement()->ClassList()->Add(AppearanceString(aAppearance), rv);
  MOZ_ASSERT(!rv.Failed(), "Add new appearance failed!");

  AC_LOG("%s: %s -> %s", __FUNCTION__, ToString(mAppearance).c_str(),
         ToString(aAppearance).c_str());

  mAppearance = aAppearance;

  // Need to reset rect since the cached rect will be compared in SetPosition.
  if (mAppearance == Appearance::None) {
    mImaginaryCaretRect = nsRect();
  }
}

void
AccessibleCaret::SetSelectionBarEnabled(bool aEnabled)
{
  if (mSelectionBarEnabled == aEnabled) {
    return;
  }

  AC_LOG("Set selection bar %s", __FUNCTION__, aEnabled ? "Enabled" : "Disabled");

  ErrorResult rv;
  CaretElement()->ClassList()->Toggle(NS_LITERAL_STRING("no-bar"),
                                      Optional<bool>(!aEnabled), rv);
  MOZ_ASSERT(!rv.Failed());

  mSelectionBarEnabled = aEnabled;
}

/* static */ nsString
AccessibleCaret::AppearanceString(Appearance aAppearance)
{
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

bool
AccessibleCaret::Intersects(const AccessibleCaret& aCaret) const
{
  MOZ_ASSERT(mPresShell == aCaret.mPresShell);

  if (!IsVisuallyVisible() || !aCaret.IsVisuallyVisible()) {
    return false;
  }

  nsRect rect = nsLayoutUtils::GetRectRelativeToFrame(CaretElement(), RootFrame());
  nsRect rhsRect = nsLayoutUtils::GetRectRelativeToFrame(aCaret.CaretElement(), RootFrame());
  return rect.Intersects(rhsRect);
}

bool
AccessibleCaret::Contains(const nsPoint& aPoint) const
{
  if (!IsVisuallyVisible()) {
    return false;
  }

  nsRect rect =
    nsLayoutUtils::GetRectRelativeToFrame(CaretImageElement(), RootFrame());

  return rect.Contains(aPoint);
}

void
AccessibleCaret::InjectCaretElement(nsIDocument* aDocument)
{
  ErrorResult rv;
  nsCOMPtr<Element> element = CreateCaretElement(aDocument);
  mCaretElementHolder = aDocument->InsertAnonymousContent(*element, rv);

  MOZ_ASSERT(!rv.Failed(), "Insert anonymous content should not fail!");
  MOZ_ASSERT(mCaretElementHolder.get(), "We must have anonymous content!");

  // InsertAnonymousContent will clone the element to make an AnonymousContent.
  // Since event listeners are not being cloned when cloning a node, we need to
  // add the listener here.
  CaretElement()->AddEventListener(NS_LITERAL_STRING("touchstart"),
                                   mDummyTouchListener, false);
}

already_AddRefed<Element>
AccessibleCaret::CreateCaretElement(nsIDocument* aDocument) const
{
  // Content structure of AccessibleCaret
  // <div class="moz-accessiblecaret">  <- CaretElement()
  //   <div class="image">              <- CaretImageElement()
  //   <div class="bar">                <- SelectionBarElement()

  ErrorResult rv;
  nsCOMPtr<Element> parent = aDocument->CreateHTMLElement(nsGkAtoms::div);
  parent->ClassList()->Add(NS_LITERAL_STRING("moz-accessiblecaret"), rv);
  parent->ClassList()->Add(NS_LITERAL_STRING("none"), rv);
  parent->ClassList()->Add(NS_LITERAL_STRING("no-bar"), rv);

  nsCOMPtr<Element> image = aDocument->CreateHTMLElement(nsGkAtoms::div);
  image->ClassList()->Add(NS_LITERAL_STRING("image"), rv);
  parent->AppendChildTo(image, false);

  nsCOMPtr<Element> bar = aDocument->CreateHTMLElement(nsGkAtoms::div);
  bar->ClassList()->Add(NS_LITERAL_STRING("bar"), rv);
  parent->AppendChildTo(bar, false);

  return parent.forget();
}

void
AccessibleCaret::RemoveCaretElement(nsIDocument* aDocument)
{
  CaretElement()->RemoveEventListener(NS_LITERAL_STRING("touchstart"),
                                      mDummyTouchListener, false);

  ErrorResult rv;
  aDocument->RemoveAnonymousContent(*mCaretElementHolder, rv);
  // It's OK rv is failed since nsCanvasFrame might not exists now.
}

AccessibleCaret::PositionChangedResult
AccessibleCaret::SetPosition(nsIFrame* aFrame, int32_t aOffset)
{
  if (!CustomContentContainerFrame()) {
    return PositionChangedResult::NotChanged;
  }

  nsRect imaginaryCaretRectInFrame =
    nsCaret::GetGeometryForFrame(aFrame, aOffset, nullptr);

  imaginaryCaretRectInFrame =
    nsLayoutUtils::ClampRectToScrollFrames(aFrame, imaginaryCaretRectInFrame);

  if (imaginaryCaretRectInFrame.IsEmpty()) {
    // Don't bother to set the caret position since it's invisible.
    return PositionChangedResult::Invisible;
  }

  nsRect imaginaryCaretRect = imaginaryCaretRectInFrame;
  nsLayoutUtils::TransformRect(aFrame, RootFrame(), imaginaryCaretRect);

  if (imaginaryCaretRect.IsEqualEdges(mImaginaryCaretRect)) {
    return PositionChangedResult::NotChanged;
  }

  mImaginaryCaretRect = imaginaryCaretRect;

  // SetCaretElementStyle() and SetSelectionBarElementStyle() require the
  // input rect relative to container frame.
  nsRect imaginaryCaretRectInContainerFrame = imaginaryCaretRectInFrame;
  nsLayoutUtils::TransformRect(aFrame, CustomContentContainerFrame(),
                               imaginaryCaretRectInContainerFrame);
  SetCaretElementStyle(imaginaryCaretRectInContainerFrame);
  SetSelectionBarElementStyle(imaginaryCaretRectInContainerFrame);

  return PositionChangedResult::Changed;
}

nsIFrame*
AccessibleCaret::CustomContentContainerFrame() const
{
  nsCanvasFrame* canvasFrame = mPresShell->GetCanvasFrame();
  Element* container = canvasFrame->GetCustomContentContainer();
  nsIFrame* containerFrame = container->GetPrimaryFrame();
  return containerFrame;
}

void
AccessibleCaret::SetCaretElementStyle(const nsRect& aRect)
{
  nsPoint position = CaretElementPosition(aRect);
  nsAutoString styleStr;
  styleStr.AppendPrintf("left: %dpx; top: %dpx; padding-top: %dpx;",
                        nsPresContext::AppUnitsToIntCSSPixels(position.x),
                        nsPresContext::AppUnitsToIntCSSPixels(position.y),
                        nsPresContext::AppUnitsToIntCSSPixels(aRect.height));

  float zoomLevel = GetZoomLevel();
  styleStr.AppendPrintf(" width: %.2fpx; height: %.2fpx; margin-left: %.2fpx",
                        sWidth / zoomLevel,
                        sHeight / zoomLevel,
                        sMarginLeft / zoomLevel);

  ErrorResult rv;
  CaretElement()->SetAttribute(NS_LITERAL_STRING("style"), styleStr, rv);
  MOZ_ASSERT(!rv.Failed());

  AC_LOG("Set caret style: %s", NS_ConvertUTF16toUTF8(styleStr).get());
}

void
AccessibleCaret::SetSelectionBarElementStyle(const nsRect& aRect)
{
  int32_t height = nsPresContext::AppUnitsToIntCSSPixels(aRect.height);
  nsAutoString barStyleStr;
  barStyleStr.AppendPrintf("margin-top: -%dpx; height: %dpx;",
                           height, height);

  float zoomLevel = GetZoomLevel();
  barStyleStr.AppendPrintf(" width: %.2fpx;", sBarWidth / zoomLevel);

  ErrorResult rv;
  SelectionBarElement()->SetAttribute(NS_LITERAL_STRING("style"), barStyleStr, rv);
  MOZ_ASSERT(!rv.Failed());

  AC_LOG("Set bar style: %s", NS_ConvertUTF16toUTF8(barStyleStr).get());
}

float
AccessibleCaret::GetZoomLevel()
{
  // Full zoom on desktop.
  float fullZoom = mPresShell->GetPresContext()->GetFullZoom();

  // Pinch-zoom on B2G.
  float resolution = mPresShell->GetCumulativeResolution();

  return fullZoom * resolution;
}

} // namespace mozilla
