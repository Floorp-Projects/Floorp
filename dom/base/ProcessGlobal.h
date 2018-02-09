/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ProcessGlobal_h
#define mozilla_dom_ProcessGlobal_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/MessageManagerGlobal.h"
#include "nsCOMPtr.h"
#include "nsFrameMessageManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIClassInfo.h"
#include "nsIRunnable.h"
#include "nsIGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsServiceManagerUtils.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class ProcessGlobal :
  public nsIMessageManagerGlobal,
  public nsMessageManagerScriptExecutor,
  public nsIGlobalObject,
  public nsIScriptObjectPrincipal,
  public nsSupportsWeakReference,
  public ipc::MessageManagerCallback,
  public MessageManagerGlobal,
  public nsWrapperCache
{
public:
  explicit ProcessGlobal(nsFrameMessageManager* aMessageManager);

  bool DoResolve(JSContext* aCx, JS::Handle<JSObject*> aObj,
                 JS::Handle<jsid> aId,
                 JS::MutableHandle<JS::PropertyDescriptor> aDesc);
  static bool MayResolve(jsid aId);
  void GetOwnPropertyNames(JSContext* aCx, JS::AutoIdVector& aNames,
                           bool aEnumerableOnly, ErrorResult& aRv);

  using ipc::MessageManagerCallback::GetProcessMessageManager;

  bool Init();

  static ProcessGlobal* Get();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(ProcessGlobal, nsIMessageManagerGlobal)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override
  {
    MOZ_CRASH("We should never get here!");
  }
  virtual bool WrapGlobalObject(JSContext* aCx,
                                JS::CompartmentOptions& aOptions,
                                JS::MutableHandle<JSObject*> aReflector) override;

  using MessageManagerGlobal::AddMessageListener;
  using MessageManagerGlobal::RemoveMessageListener;
  using MessageManagerGlobal::AddWeakMessageListener;
  using MessageManagerGlobal::RemoveWeakMessageListener;
  using MessageManagerGlobal::SendAsyncMessage;
  using MessageManagerGlobal::GetProcessMessageManager;
  using MessageManagerGlobal::GetRemoteType;
  using MessageManagerGlobal::SendSyncMessage;
  using MessageManagerGlobal::SendRpcMessage;
  using MessageManagerGlobal::Dump;
  using MessageManagerGlobal::PrivateNoteIntentionalCrash;
  using MessageManagerGlobal::Atob;
  using MessageManagerGlobal::Btoa;

  // ContentProcessMessageManager
  void GetInitialProcessData(JSContext* aCx,
                             JS::MutableHandle<JS::Value> aInitialProcessData,
                             ErrorResult& aError)
  {
    if (!mMessageManager) {
      aError.Throw(NS_ERROR_NULL_POINTER);
      return;
    }
    mMessageManager->GetInitialProcessData(aCx, aInitialProcessData, aError);
  }

  NS_FORWARD_SAFE_NSIMESSAGELISTENERMANAGER(mMessageManager)
  NS_FORWARD_SAFE_NSIMESSAGESENDER(mMessageManager)
  NS_FORWARD_SAFE_NSISYNCMESSAGESENDER(mMessageManager)
  NS_FORWARD_SAFE_NSIMESSAGEMANAGERGLOBAL(mMessageManager)

  virtual void LoadScript(const nsAString& aURL);

  virtual JSObject* GetGlobalJSObject() override
  {
    return GetWrapper();
  }
  virtual nsIPrincipal* GetPrincipal() override { return mPrincipal; }

  void SetInitialProcessData(JS::HandleValue aInitialData);

protected:
  virtual ~ProcessGlobal();

private:
  bool mInitialized;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ProcessGlobal_h
