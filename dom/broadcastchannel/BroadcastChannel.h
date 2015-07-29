/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BroadcastChannel_h
#define mozilla_dom_BroadcastChannel_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsIIPCBackgroundChildCreateCallback.h"
#include "nsIObserver.h"
#include "nsTArray.h"
#include "mozilla/nsRefPtr.h"

class nsPIDOMWindow;

namespace mozilla {

namespace ipc {
class PrincipalInfo;
} // namespace ipc

namespace dom {

namespace workers {
class WorkerFeature;
} // namespace workers

class BroadcastChannelChild;
class BroadcastChannelMessage;

class BroadcastChannel final
  : public DOMEventTargetHelper
  , public nsIIPCBackgroundChildCreateCallback
  , public nsIObserver
{
  NS_DECL_NSIIPCBACKGROUNDCHILDCREATECALLBACK
  NS_DECL_NSIOBSERVER

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

  EventHandlerNonNull* GetOnmessage();
  void SetOnmessage(EventHandlerNonNull* aCallback);

  using nsIDOMEventTarget::AddEventListener;
  using nsIDOMEventTarget::RemoveEventListener;

  virtual void AddEventListener(const nsAString& aType,
                                EventListener* aCallback,
                                bool aCapture,
                                const Nullable<bool>& aWantsUntrusted,
                                ErrorResult& aRv) override;
  virtual void RemoveEventListener(const nsAString& aType,
                                   EventListener* aCallback,
                                   bool aCapture,
                                   ErrorResult& aRv) override;

  void Shutdown();

  bool IsClosed() const
  {
    return mState != StateActive;
  }

private:
  BroadcastChannel(nsPIDOMWindow* aWindow,
                   const PrincipalInfo& aPrincipalInfo,
                   const nsACString& aOrigin,
                   const nsAString& aChannel,
                   bool aPrivateBrowsing);

  ~BroadcastChannel();

  void PostMessageData(BroadcastChannelMessage* aData);

  void PostMessageInternal(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                           ErrorResult& aRv);

  void UpdateMustKeepAlive();

  bool IsCertainlyAliveForCC() const override
  {
    return mIsKeptAlive;
  }

  nsRefPtr<BroadcastChannelChild> mActor;
  nsTArray<nsRefPtr<BroadcastChannelMessage>> mPendingMessages;

  nsAutoPtr<workers::WorkerFeature> mWorkerFeature;

  nsAutoPtr<PrincipalInfo> mPrincipalInfo;

  nsCString mOrigin;
  nsString mChannel;
  bool mPrivateBrowsing;

  bool mIsKeptAlive;

  uint64_t mInnerID;

  enum {
    StateActive,
    StateClosing,
    StateClosed
  } mState;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BroadcastChannel_h
