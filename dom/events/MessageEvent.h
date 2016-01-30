/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessageEvent_h_
#define mozilla_dom_MessageEvent_h_

#include "mozilla/dom/Event.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/MessagePortList.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

struct MessageEventInit;
class MessagePort;
class MessagePortList;
class OwningWindowProxyOrMessagePort;
class WindowProxyOrMessagePort;

/**
 * Implements the MessageEvent event, used for cross-document messaging and
 * server-sent events.
 *
 * See http://www.whatwg.org/specs/web-apps/current-work/#messageevent for
 * further details.
 */
class MessageEvent final : public Event
{
public:
  MessageEvent(EventTarget* aOwner,
               nsPresContext* aPresContext,
               WidgetEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(MessageEvent, Event)

  // Forward to base class
  NS_FORWARD_TO_EVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetData(JSContext* aCx, JS::MutableHandle<JS::Value> aData,
               ErrorResult& aRv);
  void GetOrigin(nsAString&) const;
  void GetLastEventId(nsAString&) const;
  void GetSource(Nullable<OwningWindowProxyOrMessagePort>& aValue) const;

  MessagePortList* GetPorts()
  {
    return mPorts;
  }

  void SetPorts(MessagePortList* aPorts);

  // Non WebIDL methods
  void SetSource(mozilla::dom::MessagePort* aPort);

  void SetSource(nsPIDOMWindowInner* aWindow)
  {
    mWindowSource = aWindow;
  }

  static already_AddRefed<MessageEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const MessageEventInit& aEventInit,
              ErrorResult& aRv);

  static already_AddRefed<MessageEvent>
  Constructor(EventTarget* aEventTarget,
              const nsAString& aType,
              const MessageEventInit& aEventInit,
              ErrorResult& aRv);

  void InitMessageEvent(JSContext* aCx, const nsAString& aType, bool aCanBubble,
                        bool aCancelable, JS::Handle<JS::Value> aData,
                        const nsAString& aOrigin, const nsAString& aLastEventId,
                        const Nullable<WindowProxyOrMessagePort>& aSource,
                        const Nullable<Sequence<OwningNonNull<MessagePort>>>& aPorts);

protected:
  ~MessageEvent();

private:
  JS::Heap<JS::Value> mData;
  nsString mOrigin;
  nsString mLastEventId;
  RefPtr<nsPIDOMWindowInner> mWindowSource;
  RefPtr<MessagePort> mPortSource;
  RefPtr<MessagePortList> mPorts;
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::MessageEvent>
NS_NewDOMMessageEvent(mozilla::dom::EventTarget* aOwner,
                      nsPresContext* aPresContext,
                      mozilla::WidgetEvent* aEvent);

#endif // mozilla_dom_MessageEvent_h_
