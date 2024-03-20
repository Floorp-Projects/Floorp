/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentProcessMessageManager_h
#define mozilla_dom_ContentProcessMessageManager_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/MessageManagerGlobal.h"
#include "mozilla/dom/MessageManagerCallback.h"
#include "nsCOMPtr.h"
#include "nsIScriptContext.h"
#include "nsServiceManagerUtils.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"
#include "xpcpublic.h"

class nsFrameMessageManager;

namespace mozilla::dom {

namespace ipc {
class SharedMap;
}

/**
 * This class implements a singleton process message manager for content
 * processes. Each child process has exactly one instance of this class, which
 * hosts the process's process scripts, and may exchange messages with its
 * corresponding ParentProcessMessageManager on the parent side.
 */

class ContentProcessMessageManager : public nsIMessageSender,
                                     public nsMessageManagerScriptExecutor,
                                     public nsSupportsWeakReference,
                                     public ipc::MessageManagerCallback,
                                     public MessageManagerGlobal,
                                     public nsWrapperCache {
 public:
  explicit ContentProcessMessageManager(nsFrameMessageManager* aMessageManager);

  using ipc::MessageManagerCallback::GetProcessMessageManager;
  using MessageManagerGlobal::GetProcessMessageManager;

  bool Init();

  static ContentProcessMessageManager* Get();
  static bool WasCreated();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(
      ContentProcessMessageManager, nsIMessageSender)

  void MarkForCC();

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  [[nodiscard]] JSObject* GetOrCreateWrapper();

  using MessageManagerGlobal::AddMessageListener;
  using MessageManagerGlobal::AddWeakMessageListener;
  using MessageManagerGlobal::RemoveMessageListener;
  using MessageManagerGlobal::RemoveWeakMessageListener;

  // ContentProcessMessageManager
  void GetInitialProcessData(JSContext* aCx,
                             JS::MutableHandle<JS::Value> aInitialProcessData,
                             ErrorResult& aError) {
    if (!mMessageManager) {
      aError.Throw(NS_ERROR_NOT_INITIALIZED);
      return;
    }
    mMessageManager->GetInitialProcessData(aCx, aInitialProcessData, aError);
  }

  already_AddRefed<ipc::SharedMap> GetSharedData();

  NS_FORWARD_SAFE_NSIMESSAGESENDER(mMessageManager)

  nsIGlobalObject* GetParentObject() const {
    return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
  }

  [[nodiscard]] virtual bool LoadScript(const nsAString& aURL);

  bool IsProcessScoped() const override { return true; }

  void SetInitialProcessData(JS::Handle<JS::Value> aInitialData);

 protected:
  virtual ~ContentProcessMessageManager();

 private:
  bool mInitialized;

  static bool sWasCreated;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_ContentProcessMessageManager_h
