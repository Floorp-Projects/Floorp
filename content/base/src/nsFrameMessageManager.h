/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFrameMessageManager_h__
#define nsFrameMessageManager_h__

#include "nsIMessageManager.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsIAtom.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "nsIPrincipal.h"
#include "nsIXPConnect.h"
#include "nsDataHashtable.h"
#include "nsClassHashtable.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"
#include "nsWeakPtr.h"
#include "mozilla/Attributes.h"
#include "js/RootingAPI.h"
#include "nsTObserverArray.h"
#include "mozilla/dom/StructuredCloneUtils.h"

namespace mozilla {
namespace dom {

class nsIContentParent;
class nsIContentChild;
class ClonedMessageData;
class MessageManagerReporter;

namespace ipc {

enum MessageManagerFlags {
  MM_CHILD = 0,
  MM_CHROME = 1,
  MM_GLOBAL = 2,
  MM_PROCESSMANAGER = 4,
  MM_BROADCASTER = 8,
  MM_OWNSCALLBACK = 16
};

class MessageManagerCallback
{
public:
  virtual ~MessageManagerCallback() {}

  virtual bool DoLoadFrameScript(const nsAString& aURL, bool aRunInGlobalScope)
  {
    return true;
  }

  virtual bool DoSendBlockingMessage(JSContext* aCx,
                                     const nsAString& aMessage,
                                     const StructuredCloneData& aData,
                                     JS::Handle<JSObject*> aCpows,
                                     nsIPrincipal* aPrincipal,
                                     InfallibleTArray<nsString>* aJSONRetVal,
                                     bool aIsSync)
  {
    return true;
  }

  virtual bool DoSendAsyncMessage(JSContext* aCx,
                                  const nsAString& aMessage,
                                  const StructuredCloneData& aData,
                                  JS::Handle<JSObject*> aCpows,
                                  nsIPrincipal* aPrincipal)
  {
    return true;
  }

  virtual bool CheckPermission(const nsAString& aPermission)
  {
    return false;
  }

  virtual bool CheckManifestURL(const nsAString& aManifestURL)
  {
    return false;
  }

  virtual bool CheckAppHasPermission(const nsAString& aPermission)
  {
    return false;
  }

  virtual bool CheckAppHasStatus(unsigned short aStatus)
  {
    return false;
  }

protected:
  bool BuildClonedMessageDataForParent(nsIContentParent* aParent,
                                       const StructuredCloneData& aData,
                                       ClonedMessageData& aClonedData);
  bool BuildClonedMessageDataForChild(nsIContentChild* aChild,
                                      const StructuredCloneData& aData,
                                      ClonedMessageData& aClonedData);
};

StructuredCloneData UnpackClonedMessageDataForParent(const ClonedMessageData& aData);
StructuredCloneData UnpackClonedMessageDataForChild(const ClonedMessageData& aData);

} // namespace ipc
} // namespace dom
} // namespace mozilla

class nsAXPCNativeCallContext;

struct nsMessageListenerInfo
{
  bool operator==(const nsMessageListenerInfo& aOther) const
  {
    return &aOther == this;
  }

  // Exactly one of mStrongListener and mWeakListener must be non-null.
  nsCOMPtr<nsIMessageListener> mStrongListener;
  nsWeakPtr mWeakListener;
};

class CpowHolder
{
public:
  virtual bool ToObject(JSContext* cx, JS::MutableHandle<JSObject*> objp) = 0;
};

class MOZ_STACK_CLASS SameProcessCpowHolder : public CpowHolder
{
public:
  SameProcessCpowHolder(JSRuntime *aRuntime, JS::Handle<JSObject*> aObj)
    : mObj(aRuntime, aObj)
  {
  }

  bool ToObject(JSContext* aCx, JS::MutableHandle<JSObject*> aObjp);

private:
  JS::Rooted<JSObject*> mObj;
};

class nsFrameMessageManager MOZ_FINAL : public nsIContentFrameMessageManager,
                                        public nsIMessageBroadcaster,
                                        public nsIFrameScriptLoader,
                                        public nsIProcessChecker
{
  friend class mozilla::dom::MessageManagerReporter;
  typedef mozilla::dom::StructuredCloneData StructuredCloneData;
public:
  nsFrameMessageManager(mozilla::dom::ipc::MessageManagerCallback* aCallback,
                        nsFrameMessageManager* aParentManager,
                        /* mozilla::dom::ipc::MessageManagerFlags */ uint32_t aFlags)
  : mChrome(!!(aFlags & mozilla::dom::ipc::MM_CHROME)),
    mGlobal(!!(aFlags & mozilla::dom::ipc::MM_GLOBAL)),
    mIsProcessManager(!!(aFlags & mozilla::dom::ipc::MM_PROCESSMANAGER)),
    mIsBroadcaster(!!(aFlags & mozilla::dom::ipc::MM_BROADCASTER)),
    mOwnsCallback(!!(aFlags & mozilla::dom::ipc::MM_OWNSCALLBACK)),
    mHandlingMessage(false),
    mDisconnected(false),
    mCallback(aCallback),
    mParentManager(aParentManager)
  {
    NS_ASSERTION(mChrome || !aParentManager, "Should not set parent manager!");
    NS_ASSERTION(!mIsBroadcaster || !mCallback,
                 "Broadcasters cannot have callbacks!");
    // This is a bit hackish. When parent manager is global, we want
    // to attach the message manager to it immediately.
    // Is it just the frame message manager which waits until the
    // content process is running.
    if (mParentManager && (mCallback || IsBroadcaster())) {
      mParentManager->AddChildManager(this);
    }
    if (mOwnsCallback) {
      mOwnedCallback = aCallback;
    }
  }

private:
  ~nsFrameMessageManager()
  {
    for (int32_t i = mChildManagers.Count(); i > 0; --i) {
      static_cast<nsFrameMessageManager*>(mChildManagers[i - 1])->
        Disconnect(false);
    }
    if (mIsProcessManager) {
      if (this == sParentProcessManager) {
        sParentProcessManager = nullptr;
      }
      if (this == sChildProcessManager) {
        sChildProcessManager = nullptr;
        delete sPendingSameProcessAsyncMessages;
        sPendingSameProcessAsyncMessages = nullptr;
      }
      if (this == sSameProcessParentManager) {
        sSameProcessParentManager = nullptr;
      }
    }
  }

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsFrameMessageManager,
                                           nsIContentFrameMessageManager)
  NS_DECL_NSIMESSAGELISTENERMANAGER
  NS_DECL_NSIMESSAGESENDER
  NS_DECL_NSIMESSAGEBROADCASTER
  NS_DECL_NSISYNCMESSAGESENDER
  NS_DECL_NSICONTENTFRAMEMESSAGEMANAGER
  NS_DECL_NSIFRAMESCRIPTLOADER
  NS_DECL_NSIPROCESSCHECKER

  static nsFrameMessageManager*
  NewProcessMessageManager(mozilla::dom::nsIContentParent* aProcess);

  nsresult ReceiveMessage(nsISupports* aTarget, const nsAString& aMessage,
                          bool aIsSync, const StructuredCloneData* aCloneData,
                          CpowHolder* aCpows, nsIPrincipal* aPrincipal,
                          InfallibleTArray<nsString>* aJSONRetVal);

  void AddChildManager(nsFrameMessageManager* aManager);
  void RemoveChildManager(nsFrameMessageManager* aManager)
  {
    mChildManagers.RemoveObject(aManager);
  }
  void Disconnect(bool aRemoveFromParent = true);

  void InitWithCallback(mozilla::dom::ipc::MessageManagerCallback* aCallback);
  void SetCallback(mozilla::dom::ipc::MessageManagerCallback* aCallback);
  mozilla::dom::ipc::MessageManagerCallback* GetCallback()
  {
    return mCallback;
  }

  nsresult DispatchAsyncMessage(const nsAString& aMessageName,
                                const JS::Value& aJSON,
                                const JS::Value& aObjects,
                                nsIPrincipal* aPrincipal,
                                JSContext* aCx,
                                uint8_t aArgc);
  nsresult DispatchAsyncMessageInternal(JSContext* aCx,
                                        const nsAString& aMessage,
                                        const StructuredCloneData& aData,
                                        JS::Handle<JSObject*> aCpows,
                                        nsIPrincipal* aPrincipal);
  void RemoveFromParent();
  nsFrameMessageManager* GetParentManager() { return mParentManager; }
  void SetParentManager(nsFrameMessageManager* aParent)
  {
    NS_ASSERTION(!mParentManager, "We have parent manager already!");
    NS_ASSERTION(mChrome, "Should not set parent manager!");
    mParentManager = aParent;
  }
  bool IsGlobal() { return mGlobal; }
  bool IsBroadcaster() { return mIsBroadcaster; }

  static nsFrameMessageManager* GetParentProcessManager()
  {
    return sParentProcessManager;
  }
  static nsFrameMessageManager* GetChildProcessManager()
  {
    return sChildProcessManager;
  }
private:
  nsresult SendMessage(const nsAString& aMessageName,
                       JS::Handle<JS::Value> aJSON,
                       JS::Handle<JS::Value> aObjects,
                       nsIPrincipal* aPrincipal,
                       JSContext* aCx,
                       uint8_t aArgc,
                       JS::MutableHandle<JS::Value> aRetval,
                       bool aIsSync);
protected:
  friend class MMListenerRemover;
  // We keep the message listeners as arrays in a hastable indexed by the
  // message name. That gives us fast lookups in ReceiveMessage().
  nsClassHashtable<nsStringHashKey,
                   nsAutoTObserverArray<nsMessageListenerInfo, 1>> mListeners;
  nsCOMArray<nsIContentFrameMessageManager> mChildManagers;
  bool mChrome;     // true if we're in the chrome process
  bool mGlobal;     // true if we're the global frame message manager
  bool mIsProcessManager; // true if the message manager belongs to the process realm
  bool mIsBroadcaster; // true if the message manager is a broadcaster
  bool mOwnsCallback;
  bool mHandlingMessage;
  bool mDisconnected;
  mozilla::dom::ipc::MessageManagerCallback* mCallback;
  nsAutoPtr<mozilla::dom::ipc::MessageManagerCallback> mOwnedCallback;
  nsFrameMessageManager* mParentManager;
  nsTArray<nsString> mPendingScripts;
  nsTArray<bool> mPendingScriptsGlobalStates;

  void LoadPendingScripts(nsFrameMessageManager* aManager,
                          nsFrameMessageManager* aChildMM);
public:
  static nsFrameMessageManager* sParentProcessManager;
  static nsFrameMessageManager* sChildProcessManager;
  static nsFrameMessageManager* sSameProcessParentManager;
  static nsTArray<nsCOMPtr<nsIRunnable> >* sPendingSameProcessAsyncMessages;
private:
  enum ProcessCheckerType {
    PROCESS_CHECKER_PERMISSION,
    PROCESS_CHECKER_MANIFEST_URL,
    ASSERT_APP_HAS_PERMISSION
  };
  nsresult AssertProcessInternal(ProcessCheckerType aType,
                                 const nsAString& aCapability,
                                 bool* aValid);
};

/* A helper class for taking care of many details for async message sending
   within a single process.  Intended to be used like so:

   class MyAsyncMessage : public nsSameProcessAsyncMessageBase, public nsRunnable
   {
     // Initialize nsSameProcessAsyncMessageBase...

     NS_IMETHOD Run() {
       ReceiveMessage(..., ...);
       return NS_OK;
     }
   };
 */
class nsSameProcessAsyncMessageBase
{
  typedef mozilla::dom::StructuredCloneClosure StructuredCloneClosure;

public:
  typedef mozilla::dom::StructuredCloneData StructuredCloneData;

  nsSameProcessAsyncMessageBase(JSContext* aCx,
                                const nsAString& aMessage,
                                const StructuredCloneData& aData,
                                JS::Handle<JSObject*> aCpows,
                                nsIPrincipal* aPrincipal);

  void ReceiveMessage(nsISupports* aTarget, nsFrameMessageManager* aManager);

private:
  nsSameProcessAsyncMessageBase(const nsSameProcessAsyncMessageBase&);

  JSRuntime* mRuntime;
  nsString mMessage;
  JSAutoStructuredCloneBuffer mData;
  StructuredCloneClosure mClosure;
  JS::PersistentRooted<JSObject*> mCpows;
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

class nsScriptCacheCleaner;

struct nsFrameScriptObjectExecutorHolder
{
  nsFrameScriptObjectExecutorHolder(JSContext* aCx, JSScript* aScript, bool aRunInGlobalScope)
   : mScript(aCx, aScript), mRunInGlobalScope(aRunInGlobalScope)
  { MOZ_COUNT_CTOR(nsFrameScriptObjectExecutorHolder); }

  ~nsFrameScriptObjectExecutorHolder()
  { MOZ_COUNT_DTOR(nsFrameScriptObjectExecutorHolder); }

  bool WillRunInGlobalScope() { return mRunInGlobalScope; }

  JS::PersistentRooted<JSScript*> mScript;
  bool mRunInGlobalScope;
};

class nsFrameScriptObjectExecutorStackHolder;

class nsFrameScriptExecutor
{
public:
  static void Shutdown();
  already_AddRefed<nsIXPConnectJSObjectHolder> GetGlobal()
  {
    nsCOMPtr<nsIXPConnectJSObjectHolder> ref = mGlobal;
    return ref.forget();
  }
protected:
  friend class nsFrameScriptCx;
  nsFrameScriptExecutor() { MOZ_COUNT_CTOR(nsFrameScriptExecutor); }
  ~nsFrameScriptExecutor() { MOZ_COUNT_DTOR(nsFrameScriptExecutor); }

  void DidCreateGlobal();
  void LoadFrameScriptInternal(const nsAString& aURL, bool aRunInGlobalScope);
  void TryCacheLoadAndCompileScript(const nsAString& aURL,
                                    bool aRunInGlobalScope,
                                    bool aShouldCache,
                                    JS::MutableHandle<JSScript*> aScriptp);
  void TryCacheLoadAndCompileScript(const nsAString& aURL,
                                    bool aRunInGlobalScope);
  bool InitTabChildGlobalInternal(nsISupports* aScope, const nsACString& aID);
  nsCOMPtr<nsIXPConnectJSObjectHolder> mGlobal;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsAutoTArray<JS::Heap<JSObject*>, 2> mAnonymousGlobalScopes;

  static nsDataHashtable<nsStringHashKey, nsFrameScriptObjectExecutorHolder*>* sCachedScripts;
  static nsScriptCacheCleaner* sScriptCacheCleaner;
};

class nsScriptCacheCleaner MOZ_FINAL : public nsIObserver
{
  ~nsScriptCacheCleaner() {}

  NS_DECL_ISUPPORTS

  nsScriptCacheCleaner()
  {
    nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
    if (obsSvc)
      obsSvc->AddObserver(this, "xpcom-shutdown", false);
  }

  NS_IMETHODIMP Observe(nsISupports *aSubject,
                        const char *aTopic,
                        const char16_t *aData)
  {
    nsFrameScriptExecutor::Shutdown();
    return NS_OK;
  }
};

#endif
