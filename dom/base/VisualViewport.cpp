/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VisualViewport.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/PresShell.h"
#include "mozilla/ToString.h"
#include "nsIScrollableFrame.h"
#include "nsIDocShell.h"
#include "nsPresContext.h"
#include "nsRefreshDriver.h"
#include "DocumentInlines.h"

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

/* virtual */
void VisualViewport::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  EventMessage msg = aVisitor.mEvent->mMessage;

  aVisitor.mCanHandle = true;
  EventTarget* parentTarget = nullptr;
  // Only our special internal events are allowed to escape the
  // Visual Viewport and be dispatched further up the DOM tree.
  if (msg == eMozVisualScroll || msg == eMozVisualResize) {
    if (nsPIDOMWindowInner* win = GetOwner()) {
      if (Document* doc = win->GetExtantDoc()) {
        parentTarget = doc;
      }
    }
  }
  aVisitor.SetParentTarget(parentTarget, false);
}

CSSSize VisualViewport::VisualViewportSize() const {
  CSSSize size = CSSSize(0, 0);

  // Flush layout, as that may affect the answer below (e.g. scrollbars
  // may have appeared, decreasing the available viewport size).
  RefPtr<const VisualViewport> kungFuDeathGrip(this);
  if (Document* doc = GetDocument()) {
    doc->FlushPendingNotifications(FlushType::Layout);
  }

  // Fetch the pres shell after the layout flush, as it might have destroyed it.
  if (PresShell* presShell = GetPresShell()) {
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
  if (PresShell* presShell = GetPresShell()) {
    scale = presShell->GetResolution();
  }
  return scale;
}

CSSPoint VisualViewport::VisualViewportOffset() const {
  CSSPoint offset = CSSPoint(0, 0);

  if (PresShell* presShell = GetPresShell()) {
    offset = CSSPoint::FromAppUnits(presShell->GetVisualViewportOffset());
  }
  return offset;
}

CSSPoint VisualViewport::LayoutViewportOffset() const {
  CSSPoint offset = CSSPoint(0, 0);

  if (PresShell* presShell = GetPresShell()) {
    offset = CSSPoint::FromAppUnits(presShell->GetLayoutViewportOffset());
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

Document* VisualViewport::GetDocument() const {
  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  if (!window) {
    return nullptr;
  }

  nsIDocShell* docShell = window->GetDocShell();
  if (!docShell) {
    return nullptr;
  }

  return docShell->GetDocument();
}

PresShell* VisualViewport::GetPresShell() const {
  RefPtr<Document> document = GetDocument();
  return document ? document->GetPresShell() : nullptr;
}

nsPresContext* VisualViewport::GetPresContext() const {
  RefPtr<Document> document = GetDocument();
  return document ? document->GetPresContext() : nullptr;
}

/* ================= Resize event handling ================= */

void VisualViewport::PostResizeEvent() {
  VVP_LOG("%p: PostResizeEvent (pre-existing: %d)\n", this, !!mResizeEvent);
  nsPresContext* presContext = GetPresContext();
  if (mResizeEvent && mResizeEvent->HasPresContext(presContext)) {
    return;
  }
  if (mResizeEvent) {
    // prescontext changed, so discard the old resize event and queue a new one
    mResizeEvent->Revoke();
    mResizeEvent = nullptr;
  }

  // The event constructor will register itself with the refresh driver.
  if (presContext) {
    mResizeEvent = new VisualViewportResizeEvent(this, presContext);
    VVP_LOG("%p: PostResizeEvent, created new event\n", this);
  }
}

VisualViewport::VisualViewportResizeEvent::VisualViewportResizeEvent(
    VisualViewport* aViewport, nsPresContext* aPresContext)
    : Runnable("VisualViewport::VisualViewportResizeEvent"),
      mViewport(aViewport),
      mPresContext(aPresContext) {
  VVP_LOG("%p: Registering PostResize on %p %p\n", aViewport, aPresContext,
          aPresContext->RefreshDriver());
  aPresContext->RefreshDriver()->PostVisualViewportResizeEvent(this);
}

bool VisualViewport::VisualViewportResizeEvent::HasPresContext(
    nsPresContext* aContext) const {
  return mPresContext.get() == aContext;
}

void VisualViewport::VisualViewportResizeEvent::Revoke() {
  mViewport = nullptr;
  mPresContext = nullptr;
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

  VVP_LOG("%p, FireResizeEvent, fire mozvisualresize\n", this);
  WidgetEvent mozEvent(true, eMozVisualResize);
  mozEvent.mFlags.mOnlySystemGroupDispatch = true;
  EventDispatcher::Dispatch(this, GetPresContext(), &mozEvent);

  VVP_LOG("%p, FireResizeEvent, fire VisualViewport resize\n", this);
  WidgetEvent event(true, eResize);
  event.mFlags.mBubbles = false;
  event.mFlags.mCancelable = false;
  EventDispatcher::Dispatch(this, GetPresContext(), &event);
}

/* ================= Scroll event handling ================= */

void VisualViewport::PostScrollEvent(const nsPoint& aPrevVisualOffset,
                                     const nsPoint& aPrevLayoutOffset) {
  VVP_LOG("%p: PostScrollEvent, prevRelativeOffset=%s (pre-existing: %d)\n",
          this, ToString(aPrevVisualOffset - aPrevLayoutOffset).c_str(),
          !!mScrollEvent);
  nsPresContext* presContext = GetPresContext();
  if (mScrollEvent && mScrollEvent->HasPresContext(presContext)) {
    return;
  }

  if (mScrollEvent) {
    // prescontext changed, so discard the old scroll event and queue a new one
    mScrollEvent->Revoke();
    mScrollEvent = nullptr;
  }

  // The event constructor will register itself with the refresh driver.
  if (presContext) {
    mScrollEvent = new VisualViewportScrollEvent(
        this, presContext, aPrevVisualOffset, aPrevLayoutOffset);
    VVP_LOG("%p: PostScrollEvent, created new event\n", this);
  }
}

VisualViewport::VisualViewportScrollEvent::VisualViewportScrollEvent(
    VisualViewport* aViewport, nsPresContext* aPresContext,
    const nsPoint& aPrevVisualOffset, const nsPoint& aPrevLayoutOffset)
    : Runnable("VisualViewport::VisualViewportScrollEvent"),
      mViewport(aViewport),
      mPresContext(aPresContext),
      mPrevVisualOffset(aPrevVisualOffset),
      mPrevLayoutOffset(aPrevLayoutOffset) {
  VVP_LOG("%p: Registering PostScroll on %p %p\n", aViewport, aPresContext,
          aPresContext->RefreshDriver());
  aPresContext->RefreshDriver()->PostVisualViewportScrollEvent(this);
}

bool VisualViewport::VisualViewportScrollEvent::HasPresContext(
    nsPresContext* aContext) const {
  return mPresContext.get() == aContext;
}

void VisualViewport::VisualViewportScrollEvent::Revoke() {
  mViewport = nullptr;
  mPresContext = nullptr;
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
  nsPoint prevVisualOffset = mScrollEvent->PrevVisualOffset();
  nsPoint prevLayoutOffset = mScrollEvent->PrevLayoutOffset();
  mScrollEvent->Revoke();
  mScrollEvent = nullptr;

  if (PresShell* presShell = GetPresShell()) {
    if (presShell->GetVisualViewportOffset() != prevVisualOffset) {
      // The internal event will be fired whenever the visual viewport's
      // *absolute* offset changed, i.e. relative to the page.
      VVP_LOG("%p: FireScrollEvent, fire mozvisualscroll\n", this);
      WidgetEvent mozEvent(true, eMozVisualScroll);
      mozEvent.mFlags.mOnlySystemGroupDispatch = true;
      EventDispatcher::Dispatch(this, GetPresContext(), &mozEvent);
    }

    // Check whether the relative visual viewport offset actually changed -
    // maybe both visual and layout viewport scrolled together and there was no
    // change after all.
    nsPoint curRelativeOffset =
        presShell->GetVisualViewportOffsetRelativeToLayoutViewport();
    nsPoint prevRelativeOffset = prevVisualOffset - prevLayoutOffset;
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
