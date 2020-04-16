/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessageManagerGlobal_h
#define mozilla_dom_MessageManagerGlobal_h

#include "nsFrameMessageManager.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {

/**
 * Base class for implementing the WebIDL MessageManagerGlobal class.
 */
class MessageManagerGlobal {
 public:
  // MessageListenerManager
  void AddMessageListener(const nsAString& aMessageName,
                          MessageListener& aListener, bool aListenWhenClosed,
                          ErrorResult& aError) {
    if (!mMessageManager) {
      aError.Throw(NS_ERROR_NOT_INITIALIZED);
      return;
    }
    mMessageManager->AddMessageListener(aMessageName, aListener,
                                        aListenWhenClosed, aError);
  }
  void RemoveMessageListener(const nsAString& aMessageName,
                             MessageListener& aListener, ErrorResult& aError) {
    if (!mMessageManager) {
      aError.Throw(NS_ERROR_NOT_INITIALIZED);
      return;
    }
    mMessageManager->RemoveMessageListener(aMessageName, aListener, aError);
  }
  void AddWeakMessageListener(const nsAString& aMessageName,
                              MessageListener& aListener, ErrorResult& aError) {
    if (!mMessageManager) {
      aError.Throw(NS_ERROR_NOT_INITIALIZED);
      return;
    }
    mMessageManager->AddWeakMessageListener(aMessageName, aListener, aError);
  }
  void RemoveWeakMessageListener(const nsAString& aMessageName,
                                 MessageListener& aListener,
                                 ErrorResult& aError) {
    if (!mMessageManager) {
      aError.Throw(NS_ERROR_NOT_INITIALIZED);
      return;
    }
    mMessageManager->RemoveWeakMessageListener(aMessageName, aListener, aError);
  }

  // MessageSender
  void SendAsyncMessage(JSContext* aCx, const nsAString& aMessageName,
                        JS::Handle<JS::Value> aObj,
                        JS::Handle<JSObject*> aObjects,
                        JS::Handle<JS::Value> aTransfers, ErrorResult& aError) {
    if (!mMessageManager) {
      aError.Throw(NS_ERROR_NOT_INITIALIZED);
      return;
    }
    mMessageManager->SendAsyncMessage(aCx, aMessageName, aObj, aObjects,
                                      aTransfers, aError);
  }
  already_AddRefed<ProcessMessageManager> GetProcessMessageManager(
      mozilla::ErrorResult& aError) {
    if (!mMessageManager) {
      aError.Throw(NS_ERROR_NOT_INITIALIZED);
      return nullptr;
    }
    return mMessageManager->GetProcessMessageManager(aError);
  }

  void GetRemoteType(nsAString& aRemoteType, mozilla::ErrorResult& aError) {
    if (!mMessageManager) {
      aError.Throw(NS_ERROR_NOT_INITIALIZED);
      return;
    }
    mMessageManager->GetRemoteType(aRemoteType, aError);
  }

  // SyncMessageSender
  void SendSyncMessage(JSContext* aCx, const nsAString& aMessageName,
                       JS::Handle<JS::Value> aObj,
                       JS::Handle<JSObject*> aObjects,
                       nsTArray<JS::Value>& aResult, ErrorResult& aError) {
    if (!mMessageManager) {
      aError.Throw(NS_ERROR_NOT_INITIALIZED);
      return;
    }
    mMessageManager->SendSyncMessage(aCx, aMessageName, aObj, aObjects, aResult,
                                     aError);
  }
  void SendRpcMessage(JSContext* aCx, const nsAString& aMessageName,
                      JS::Handle<JS::Value> aObj,
                      JS::Handle<JSObject*> aObjects,
                      nsTArray<JS::Value>& aResult, ErrorResult& aError) {
    if (!mMessageManager) {
      aError.Throw(NS_ERROR_NOT_INITIALIZED);
      return;
    }
    mMessageManager->SendRpcMessage(aCx, aMessageName, aObj, aObjects, aResult,
                                    aError);
  }

  // MessageManagerGlobal
  void Dump(const nsAString& aStr);
  void PrivateNoteIntentionalCrash(ErrorResult& aError);
  void Atob(const nsAString& aAsciiString, nsAString& aBase64Data,
            ErrorResult& aError);
  void Btoa(const nsAString& aBase64Data, nsAString& aAsciiString,
            ErrorResult& aError);

  void MarkForCC() {
    if (mMessageManager) {
      mMessageManager->MarkForCC();
    }
  }

 protected:
  explicit MessageManagerGlobal(nsFrameMessageManager* aMessageManager)
      : mMessageManager(aMessageManager) {}

  RefPtr<nsFrameMessageManager> mMessageManager;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MessageManagerGlobal_h
