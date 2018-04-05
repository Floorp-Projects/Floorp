/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BroadcastChannel_h
#define mozilla_dom_BroadcastChannel_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "mozilla/RefPtr.h"

class nsPIDOMWindowInner;

namespace mozilla {

namespace ipc {
class PrincipalInfo;
} // namespace ipc

namespace dom {

class BroadcastChannelChild;
class BroadcastChannelMessage;
class WorkerRef;

class BroadcastChannel final
  : public DOMEventTargetHelper
{
  friend class BroadcastChannelChild;

  typedef mozilla::ipc::PrincipalInfo PrincipalInfo;

public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BroadcastChannel,
                                           DOMEventTargetHelper)

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<BroadcastChannel>
  Constructor(const GlobalObject& aGlobal, const nsAString& aChannel,
              ErrorResult& aRv);

  void GetName(nsAString& aName) const
  {
    aName = mChannel;
  }

  void PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                   ErrorResult& aRv);

  void Close();

  IMPL_EVENT_HANDLER(message)
  IMPL_EVENT_HANDLER(messageerror)

  void Shutdown();

private:
  BroadcastChannel(nsPIDOMWindowInner* aWindow,
                   const nsAString& aChannel);

  ~BroadcastChannel();

  void PostMessageData(BroadcastChannelMessage* aData);

  void PostMessageInternal(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                           ErrorResult& aRv);

  void RemoveDocFromBFCache();

  void DisconnectFromOwner() override;

  RefPtr<BroadcastChannelChild> mActor;

  RefPtr<WorkerRef> mWorkerRef;

  nsString mChannel;

  enum {
    StateActive,
    StateClosed
  } mState;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BroadcastChannel_h
