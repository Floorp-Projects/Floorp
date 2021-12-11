/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CompositionEvent_h_
#define mozilla_dom_CompositionEvent_h_

#include "mozilla/dom/CompositionEventBinding.h"
#include "mozilla/dom/TextClause.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/UIEvent.h"
#include "mozilla/EventForwards.h"

namespace mozilla {
namespace dom {

using TextClauseArray = nsTArray<RefPtr<TextClause>>;

class CompositionEvent : public UIEvent {
 public:
  CompositionEvent(EventTarget* aOwner, nsPresContext* aPresContext,
                   WidgetCompositionEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CompositionEvent, UIEvent)

  static already_AddRefed<CompositionEvent> Constructor(
      const GlobalObject& aGlobal, const nsAString& aType,
      const CompositionEventInit& aParam);

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override {
    return CompositionEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  void InitCompositionEvent(const nsAString& aType, bool aCanBubble,
                            bool aCancelable, nsGlobalWindowInner* aView,
                            const nsAString& aData, const nsAString& aLocale);
  void GetData(nsAString&) const;
  void GetLocale(nsAString&) const;
  void GetRanges(TextClauseArray& aRanges);

 protected:
  ~CompositionEvent() = default;

  nsString mData;
  nsString mLocale;
  TextClauseArray mRanges;
};

}  // namespace dom
}  // namespace mozilla

already_AddRefed<mozilla::dom::CompositionEvent> NS_NewDOMCompositionEvent(
    mozilla::dom::EventTarget* aOwner, nsPresContext* aPresContext,
    mozilla::WidgetCompositionEvent* aEvent);

#endif  // mozilla_dom_CompositionEvent_h_
