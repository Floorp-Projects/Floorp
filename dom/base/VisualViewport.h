/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VisualViewport_h
#define mozilla_dom_VisualViewport_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/VisualViewportBinding.h"
#include "Units.h"
#include "nsIPresShell.h"

namespace mozilla {
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
  double Width() const;
  double Height() const;
  double Scale() const;
  IMPL_EVENT_HANDLER(resize)
  IMPL_EVENT_HANDLER(scroll)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 private:
  virtual ~VisualViewport();

  CSSSize VisualViewportSize() const;
  CSSPoint VisualViewportOffset() const;
  CSSPoint LayoutViewportOffset() const;
  nsIPresShell* GetPresShell() const;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_VisualViewport_h
