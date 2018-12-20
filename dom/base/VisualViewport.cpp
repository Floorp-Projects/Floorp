/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VisualViewport.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/ToString.h"
#include "nsIScrollableFrame.h"
#include "nsIDocShell.h"
#include "nsPresContext.h"
#include "nsRefreshDriver.h"

#define VVP_LOG(...)
// #define VVP_LOG(...) printf_stderr("VVP: " __VA_ARGS__)

using namespace mozilla;
using namespace mozilla::dom;

VisualViewport::VisualViewport(nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow) {}

VisualViewport::~VisualViewport() {
  if (mResizeEvent) {
    mResizeEvent->Revoke();
  }

  if (mScrollEvent) {
    mScrollEvent->Revoke();
  }
}

/* virtual */
JSObject* VisualViewport::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return VisualViewport_Binding::Wrap(aCx, this, aGivenProto);
}

CSSSize VisualViewport::VisualViewportSize() const {
  CSSSize size = CSSSize(0, 0);

  nsIPresShell* presShell = GetPresShell();
  if (presShell) {
    if (presShell->IsVisualViewportSizeSet()) {
      size = CSSRect::FromAppUnits(presShell->GetVisualViewportSize());
    } else {
      nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollable();
      if (sf) {
        size = CSSRect::FromAppUnits(sf->GetScrollPortRect().Size());
      }
    }
  }
  return size;
}

double VisualViewport::Width() const {
  CSSSize size = VisualViewportSize();
  return size.width;
}

double VisualViewport::Height() const {
  CSSSize size = VisualViewportSize();
  return size.height;
}

double VisualViewport::Scale() const {
  double scale = 1;
  nsIPresShell* presShell = GetPresShell();
  if (presShell) {
    scale = presShell->GetResolution();
  }
  return scale;
}

CSSPoint VisualViewport::VisualViewportOffset() const {
  CSSPoint offset = CSSPoint(0, 0);

  nsIPresShell* presShell = GetPresShell();
  if (presShell) {
    offset = CSSPoint::FromAppUnits(presShell->GetVisualViewportOffset());
  }
  return offset;
}

CSSPoint VisualViewport::LayoutViewportOffset() const {
  CSSPoint offset = CSSPoint(0, 0);

  nsIPresShell* presShell = GetPresShell();
  if (presShell) {
    nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollable();
    if (sf) {
      offset = CSSPoint::FromAppUnits(sf->GetScrollPosition());
    }
  }
  return offset;
}

double VisualViewport::PageLeft() const { return VisualViewportOffset().X(); }

double VisualViewport::PageTop() const { return VisualViewportOffset().Y(); }

double VisualViewport::OffsetLeft() const {
  return PageLeft() - LayoutViewportOffset().X();
}

double VisualViewport::OffsetTop() const {
  return PageTop() - LayoutViewportOffset().Y();
}

nsIPresShell* VisualViewport::GetPresShell() const {
  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  if (!window) {
    return nullptr;
  }

  nsIDocShell* docShell = window->GetDocShell();
  if (!docShell) {
    return nullptr;
  }

  return docShell->GetPresShell();
}

nsPresContext* VisualViewport::GetPresContext() const {
  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return nullptr;
  }

  return presShell->GetPresContext();
}

/* ================= Resize event handling ================= */

void VisualViewport::PostResizeEvent() {
  VVP_LOG("%p: PostResizeEvent\n", this);
  if (mResizeEvent) {
    return;
  }

  // The event constructor will register itself with the refresh driver.
  if (nsPresContext* presContext = GetPresContext()) {
    mResizeEvent = new VisualViewportResizeEvent(this, presContext);
    VVP_LOG("%p: PostResizeEvent, created new event\n", this);
  }
}

VisualViewport::VisualViewportResizeEvent::VisualViewportResizeEvent(
    VisualViewport* aViewport, nsPresContext* aPresContext)
    : Runnable("VisualViewport::VisualViewportResizeEvent"),
      mViewport(aViewport) {
  aPresContext->RefreshDriver()->PostVisualViewportResizeEvent(this);
}

NS_IMETHODIMP
VisualViewport::VisualViewportResizeEvent::Run() {
  if (mViewport) {
    mViewport->FireResizeEvent();
  }
  return NS_OK;
}

void VisualViewport::FireResizeEvent() {
  MOZ_ASSERT(mResizeEvent);
  mResizeEvent->Revoke();
  mResizeEvent = nullptr;

  VVP_LOG("%p, FireResizeEvent, fire VisualViewport resize\n", this);
  WidgetEvent event(true, eResize);
  event.mFlags.mBubbles = false;
  event.mFlags.mCancelable = false;
  EventDispatcher::Dispatch(this, GetPresContext(), &event);
}

/* ================= Scroll event handling ================= */

void VisualViewport::PostScrollEvent(const nsPoint& aPrevRelativeOffset) {
  VVP_LOG("%p: PostScrollEvent, prevRelativeOffset %s\n", this,
          ToString(aPrevRelativeOffset).c_str());
  if (mScrollEvent) {
    return;
  }

  // The event constructor will register itself with the refresh driver.
  if (nsPresContext* presContext = GetPresContext()) {
    mScrollEvent =
        new VisualViewportScrollEvent(this, presContext, aPrevRelativeOffset);
    VVP_LOG("%p: PostScrollEvent, created new event\n", this);
  }
}

VisualViewport::VisualViewportScrollEvent::VisualViewportScrollEvent(
    VisualViewport* aViewport, nsPresContext* aPresContext,
    const nsPoint& aPrevRelativeOffset)
    : Runnable("VisualViewport::VisualViewportScrollEvent"),
      mViewport(aViewport),
      mPrevRelativeOffset(aPrevRelativeOffset) {
  aPresContext->RefreshDriver()->PostVisualViewportScrollEvent(this);
}

NS_IMETHODIMP
VisualViewport::VisualViewportScrollEvent::Run() {
  if (mViewport) {
    mViewport->FireScrollEvent();
  }
  return NS_OK;
}

void VisualViewport::FireScrollEvent() {
  MOZ_ASSERT(mScrollEvent);
  nsPoint prevRelativeOffset = mScrollEvent->PrevRelativeOffset();
  mScrollEvent->Revoke();
  mScrollEvent = nullptr;

  nsIPresShell* presShell = GetPresShell();
  // Check whether the relative visual viewport offset actually changed - maybe
  // both visual and layout viewport scrolled together and there was no change
  // after all.
  if (presShell) {
    nsPoint curRelativeOffset =
        presShell->GetVisualViewportOffsetRelativeToLayoutViewport();
    VVP_LOG(
        "%p: FireScrollEvent, curRelativeOffset %s, "
        "prevRelativeOffset %s\n",
        this, ToString(curRelativeOffset).c_str(),
        ToString(prevRelativeOffset).c_str());
    if (curRelativeOffset != prevRelativeOffset) {
      VVP_LOG("%p, FireScrollEvent, fire VisualViewport scroll\n", this);
      WidgetGUIEvent event(true, eScroll, nullptr);
      event.mFlags.mBubbles = false;
      event.mFlags.mCancelable = false;
      EventDispatcher::Dispatch(this, GetPresContext(), &event);
    }
  }
}
