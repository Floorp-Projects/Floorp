/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScrollBoxObject_h
#define mozilla_dom_ScrollBoxObject_h

#include "mozilla/dom/BoxObject.h"
#include "Units.h"

class nsIScrollableFrame;
struct nsRect;

namespace mozilla {
namespace dom {

class ScrollBoxObject final : public BoxObject
{
public:
  ScrollBoxObject();

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual nsIScrollableFrame* GetScrollFrame();

  void ScrollTo(int32_t x, int32_t y, ErrorResult& aRv);
  void ScrollBy(int32_t dx, int32_t dy, ErrorResult& aRv);
  void ScrollByLine(int32_t dlines, ErrorResult& aRv);
  void ScrollByIndex(int32_t dindexes, ErrorResult& aRv);
  void ScrollToLine(int32_t line, ErrorResult& aRv);
  void ScrollToElement(Element& child, ErrorResult& aRv);
  void ScrollToIndex(int32_t index, ErrorResult& aRv);
  int32_t GetPositionX(ErrorResult& aRv);
  int32_t GetPositionY(ErrorResult& aRv);
  int32_t GetScrolledWidth(ErrorResult& aRv);
  int32_t GetScrolledHeight(ErrorResult& aRv);
  void EnsureElementIsVisible(Element& child, ErrorResult& aRv);
  void EnsureIndexIsVisible(int32_t index, ErrorResult& aRv);
  void EnsureLineIsVisible(int32_t line, ErrorResult& aRv);

  // Deprecated APIs from old IDL
  void GetPosition(JSContext* cx,
                   JS::Handle<JSObject*> x,
                   JS::Handle<JSObject*> y,
                   ErrorResult& aRv);

  void GetScrolledSize(JSContext* cx,
                       JS::Handle<JSObject*> width,
                       JS::Handle<JSObject*> height,
                       ErrorResult& aRv);

protected:
  void GetScrolledSize(nsRect& aRect, ErrorResult& aRv);
  void GetPosition(CSSIntPoint& aPos, ErrorResult& aRv);

private:
  ~ScrollBoxObject();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ScrollBoxObject_h
