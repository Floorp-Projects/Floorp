/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessageEvent_h_
#define mozilla_dom_MessageEvent_h_

#include "js/RootingAPI.h"
#include "js/Value.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/Event.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsStringFwd.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class BrowsingContext;
struct MessageEventInit;
class MessagePort;
class OwningWindowProxyOrMessagePortOrServiceWorker;
class ServiceWorker;
class WindowProxyOrMessagePortOrServiceWorker;

/**
 * Implements the MessageEvent event, used for cross-document messaging and
 * server-sent events.
 *
 * See http://www.whatwg.org/specs/web-apps/current-work/#messageevent for
 * further details.
 */
class MessageEvent final : public Event {
 public:
  MessageEvent(EventTarget* aOwner, nsPresContext* aPresContext,
               WidgetEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(MessageEvent, Event)

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetData(JSContext* aCx, JS::MutableHandle<JS::Value> aData,
               ErrorResult& aRv);
  void GetOrigin(nsAString&) const;
  void GetLastEventId(nsAString&) const;
  void GetSource(
      Nullable<OwningWindowProxyOrMessagePortOrServiceWorker>& aValue) const;

  void GetPorts(nsTArray<RefPtr<MessagePort>>& aPorts);

  static already_AddRefed<MessageEvent> Constructor(
      const GlobalObject& aGlobal, const nsAString& aType,
      const MessageEventInit& aEventInit);

  static already_AddRefed<MessageEvent> Constructor(
      EventTarget* aEventTarget, const nsAString& aType,
      const MessageEventInit& aEventInit);

  void InitMessageEvent(
      JSContext* aCx, const nsAString& aType, bool aCanBubble, bool aCancelable,
      JS::Handle<JS::Value> aData, const nsAString& aOrigin,
      const nsAString& aLastEventId,
      const Nullable<WindowProxyOrMessagePortOrServiceWorker>& aSource,
      const Sequence<OwningNonNull<MessagePort>>& aPorts) {
    InitMessageEvent(aCx, aType, aCanBubble ? CanBubble::eYes : CanBubble::eNo,
                     aCancelable ? Cancelable::eYes : Cancelable::eNo, aData,
                     aOrigin, aLastEventId, aSource, aPorts);
  }

  void InitMessageEvent(
      JSContext* aCx, const nsAString& aType, mozilla::CanBubble,
      mozilla::Cancelable, JS::Handle<JS::Value> aData,
      const nsAString& aOrigin, const nsAString& aLastEventId,
      const Nullable<WindowProxyOrMessagePortOrServiceWorker>& aSource,
      const Sequence<OwningNonNull<MessagePort>>& aPorts);

 protected:
  ~MessageEvent();

 private:
  JS::Heap<JS::Value> mData;
  nsString mOrigin;
  nsString mLastEventId;
  RefPtr<BrowsingContext> mWindowSource;
  RefPtr<MessagePort> mPortSource;
  RefPtr<ServiceWorker> mServiceWorkerSource;

  nsTArray<RefPtr<MessagePort>> mPorts;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MessageEvent_h_
