/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VisualViewport_h
#define mozilla_dom_VisualViewport_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/VisualViewportBinding.h"
#include "Units.h"

namespace mozilla {

class PresShell;

namespace dom {

/* Visual Viewport API spec:
 * https://wicg.github.io/visual-viewport/#the-visualviewport-interface */
class VisualViewport final : public mozilla::DOMEventTargetHelper {
 public:
  explicit VisualViewport(nsPIDOMWindowInner* aWindow);

  double OffsetLeft() const;
  double OffsetTop() const;
  double PageLeft() const;
  double PageTop() const;
  MOZ_CAN_RUN_SCRIPT double Width() const;
  MOZ_CAN_RUN_SCRIPT double Height() const;
  double Scale() const;
  IMPL_EVENT_HANDLER(resize)
  IMPL_EVENT_HANDLER(scroll)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

  void PostResizeEvent();
  void PostScrollEvent(const nsPoint& aPrevVisualOffset,
                       const nsPoint& aPrevLayoutOffset);

  // These two events are modelled after the ScrollEvent class in
  // nsGfxScrollFrame.h.
  class VisualViewportResizeEvent : public Runnable {
   public:
    NS_DECL_NSIRUNNABLE
    VisualViewportResizeEvent(VisualViewport* aViewport,
                              nsPresContext* aPresContext);
    bool HasPresContext(nsPresContext* aContext) const;
    void Revoke();

   private:
    VisualViewport* mViewport;
    WeakPtr<nsPresContext> mPresContext;
  };

  class VisualViewportScrollEvent : public Runnable {
   public:
    NS_DECL_NSIRUNNABLE
    VisualViewportScrollEvent(VisualViewport* aViewport,
                              nsPresContext* aPresContext,
                              const nsPoint& aPrevVisualOffset,
                              const nsPoint& aPrevLayoutOffset);
    bool HasPresContext(nsPresContext* aContext) const;
    void Revoke();
    nsPoint PrevVisualOffset() const { return mPrevVisualOffset; }
    nsPoint PrevLayoutOffset() const { return mPrevLayoutOffset; }

   private:
    VisualViewport* mViewport;
    WeakPtr<nsPresContext> mPresContext;
    // The VisualViewport "scroll" event is supposed to be fired only when the
    // *relative* offset between visual and layout viewport changes. The two
    // viewports are updated independently from each other, though, so the only
    // thing we can do is note the fact that one of the inputs into the relative
    // visual viewport offset changed and then check the offset again at the
    // next refresh driver tick, just before the event is going to fire.
    // Hopefully, at this point both visual and layout viewport positions have
    // been updated, so that we're able to tell whether the relative offset did
    // in fact change or not.
    const nsPoint mPrevVisualOffset;
    const nsPoint mPrevLayoutOffset;
  };

 private:
  virtual ~VisualViewport();

  MOZ_CAN_RUN_SCRIPT CSSSize VisualViewportSize() const;
  CSSPoint VisualViewportOffset() const;
  CSSPoint LayoutViewportOffset() const;
  Document* GetDocument() const;
  PresShell* GetPresShell() const;
  nsPresContext* GetPresContext() const;

  void FireResizeEvent();
  void FireScrollEvent();

  RefPtr<VisualViewportResizeEvent> mResizeEvent;
  RefPtr<VisualViewportScrollEvent> mScrollEvent;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_VisualViewport_h
