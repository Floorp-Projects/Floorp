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
#include "nsAtom.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "nsIPrincipal.h"
#include "nsIXPConnect.h"
#include "nsDataHashtable.h"
#include "nsClassHashtable.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"
#include "nsIWeakReferenceUtils.h"
#include "mozilla/Attributes.h"
#include "js/RootingAPI.h"
#include "nsTObserverArray.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/dom/CallbackObject.h"
#include "mozilla/dom/SameProcessMessageQueue.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/jsipc/CpowHolder.h"

class nsFrameLoader;

namespace mozilla {

namespace ipc {
  class FileDescriptor;
}

namespace dom {

class nsIContentParent;
class nsIContentChild;
class ChildProcessMessageManager;
class ChromeMessageBroadcaster;
class ClonedMessageData;
class MessageBroadcaster;
class MessageListener;
class MessageListenerManager;
class MessageManagerReporter;
template<typename T> class Optional;
class ParentProcessMessageManager;
class ProcessMessageManager;

namespace ipc {

class WritableSharedMap;

// Note: we round the time we spend to the nearest millisecond. So a min value
// of 1 ms actually captures from 500us and above.
static const uint32_t kMinTelemetrySyncMessageManagerLatencyMs = 1;

enum class MessageManagerFlags {
  MM_NONE = 0,
  MM_CHROME = 1,
  MM_GLOBAL = 2,
  MM_PROCESSMANAGER = 4,
  MM_BROADCASTER = 8,
  MM_OWNSCALLBACK = 16
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(MessageManagerFlags);

class MessageManagerCallback
{
public:
  virtual ~MessageManagerCallback() {}

  virtual bool DoLoadMessageManagerScript(const nsAString& aURL, bool aRunInGlobalScope)
  {
    return true;
  }

  virtual bool DoSendBlockingMessage(JSContext* aCx,
                                     const nsAString& aMessage,
                                     StructuredCloneData& aData,
                                     JS::Handle<JSObject*> aCpows,
                                     nsIPrincipal* aPrincipal,
                                     nsTArray<StructuredCloneData>* aRetVal,
                                     bool aIsSync)
  {
    return true;
  }

  virtual nsresult DoSendAsyncMessage(JSContext* aCx,
                                      const nsAString& aMessage,
                                      StructuredCloneData& aData,
                                      JS::Handle<JSObject*> aCpows,
                                      nsIPrincipal* aPrincipal)
  {
    return NS_OK;
  }

  virtual mozilla::dom::ProcessMessageManager* GetProcessMessageManager() const
  {
    return nullptr;
  }

  virtual void DoGetRemoteType(nsAString& aRemoteType,
                               ErrorResult& aError) const;

protected:
  bool BuildClonedMessageDataForParent(nsIContentParent* aParent,
                                       StructuredCloneData& aData,
                                       ClonedMessageData& aClonedData);
  bool BuildClonedMessageDataForChild(nsIContentChild* aChild,
                                      StructuredCloneData& aData,
                                      ClonedMessageData& aClonedData);
};

void UnpackClonedMessageDataForParent(const ClonedMessageData& aClonedData,
                                      StructuredCloneData& aData);

void UnpackClonedMessageDataForChild(const ClonedMessageData& aClonedData,
                                     StructuredCloneData& aData);

} // namespace ipc
} // namespace dom
} // namespace mozilla

struct nsMessageListenerInfo
{
  bool operator==(const nsMessageListenerInfo& aOther) const
  {
    return &aOther == this;
  }

  // If mWeakListener is null then mStrongListener holds a MessageListener.
  // If mWeakListener is non-null then mStrongListener contains null.
  RefPtr<mozilla::dom::MessageListener> mStrongListener;
  nsWeakPtr mWeakListener;
  bool mListenWhenClosed;
};

class MOZ_STACK_CLASS SameProcessCpowHolder : public mozilla::jsipc::CpowHolder
{
public:
  SameProcessCpowHolder(JS::RootingContext* aRootingCx, JS::Handle<JSObject*> aObj)
    : mObj(aRootingCx, aObj)
  {
  }

  virtual bool ToObject(JSContext* aCx, JS::MutableHandle<JSObject*> aObjp)
    override;

private:
  JS::Rooted<JSObject*> mObj;
};

class nsFrameMessageManager : public nsIMessageSender
{
  friend class mozilla::dom::MessageManagerReporter;
  typedef mozilla::dom::ipc::StructuredCloneData StructuredCloneData;

protected:
  typedef mozilla::dom::ipc::MessageManagerFlags MessageManagerFlags;

  nsFrameMessageManager(mozilla::dom::ipc::MessageManagerCallback* aCallback,
                        MessageManagerFlags aFlags);

  virtual ~nsFrameMessageManager();

public:
  explicit nsFrameMessageManager(mozilla::dom::ipc::MessageManagerCallback* aCallback)
    : nsFrameMessageManager(aCallback, MessageManagerFlags::MM_NONE)
  {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsFrameMessageManager)

  void MarkForCC();

  // MessageListenerManager
  void AddMessageListener(const nsAString& aMessageName,
                          mozilla::dom::MessageListener& aListener,
                          bool aListenWhenClosed,
                          mozilla::ErrorResult& aError);
  void RemoveMessageListener(const nsAString& aMessageName,
                             mozilla::dom::MessageListener& aListener,
                             mozilla::ErrorResult& aError);
  void AddWeakMessageListener(const nsAString& aMessageName,
                              mozilla::dom::MessageListener& aListener,
                              mozilla::ErrorResult& aError);
  void RemoveWeakMessageListener(const nsAString& aMessageName,
                                 mozilla::dom::MessageListener& aListener,
                                 mozilla::ErrorResult& aError);

  // MessageSender
  void SendAsyncMessage(JSContext* aCx, const nsAString& aMessageName,
                        JS::Handle<JS::Value> aObj,
                        JS::Handle<JSObject*> aObjects,
                        nsIPrincipal* aPrincipal,
                        JS::Handle<JS::Value> aTransfers,
                        mozilla::ErrorResult& aError)
  {
    DispatchAsyncMessage(aCx, aMessageName, aObj, aObjects, aPrincipal, aTransfers,
                         aError);
  }
  already_AddRefed<mozilla::dom::ProcessMessageManager>
    GetProcessMessageManager(mozilla::ErrorResult& aError);
  void GetRemoteType(nsAString& aRemoteType, mozilla::ErrorResult& aError) const;

  // SyncMessageSender
  void SendSyncMessage(JSContext* aCx, const nsAString& aMessageName,
                       JS::Handle<JS::Value> aObj,
                       JS::Handle<JSObject*> aObjects,
                       nsIPrincipal* aPrincipal,
                       nsTArray<JS::Value>& aResult,
                       mozilla::ErrorResult& aError)
  {
    SendMessage(aCx, aMessageName, aObj, aObjects, aPrincipal, true, aResult, aError);
  }
  void SendRpcMessage(JSContext* aCx, const nsAString& aMessageName,
                      JS::Handle<JS::Value> aObj,
                      JS::Handle<JSObject*> aObjects,
                      nsIPrincipal* aPrincipal,
                      nsTArray<JS::Value>& aResult,
                      mozilla::ErrorResult& aError)
  {
    SendMessage(aCx, aMessageName, aObj, aObjects, aPrincipal, false, aResult, aError);
  }

  // GlobalProcessScriptLoader
  void GetInitialProcessData(JSContext* aCx,
                             JS::MutableHandle<JS::Value> aInitialProcessData,
                             mozilla::ErrorResult& aError);

  mozilla::dom::ipc::WritableSharedMap* SharedData();

  NS_DECL_NSIMESSAGESENDER

  static mozilla::dom::ProcessMessageManager* NewProcessMessageManager(bool aIsRemote);

  void ReceiveMessage(nsISupports* aTarget, nsFrameLoader* aTargetFrameLoader,
                      const nsAString& aMessage, bool aIsSync,
                      StructuredCloneData* aCloneData, mozilla::jsipc::CpowHolder* aCpows,
                      nsIPrincipal* aPrincipal, nsTArray<StructuredCloneData>* aRetVal,
                      mozilla::ErrorResult& aError)
  {
    ReceiveMessage(aTarget, aTargetFrameLoader, mClosed, aMessage, aIsSync, aCloneData,
                   aCpows, aPrincipal, aRetVal, aError);
  }

  void Disconnect(bool aRemoveFromParent = true);
  void Close();

  void SetCallback(mozilla::dom::ipc::MessageManagerCallback* aCallback);

  mozilla::dom::ipc::MessageManagerCallback* GetCallback()
  {
    return mCallback;
  }

  nsresult DispatchAsyncMessageInternal(JSContext* aCx,
                                        const nsAString& aMessage,
                                        StructuredCloneData& aData,
                                        JS::Handle<JSObject*> aCpows,
                                        nsIPrincipal* aPrincipal);
  bool IsGlobal() { return mGlobal; }
  bool IsBroadcaster() { return mIsBroadcaster; }
  bool IsChrome() { return mChrome; }

  // GetGlobalMessageManager creates the global message manager if it hasn't been yet.
  static already_AddRefed<mozilla::dom::ChromeMessageBroadcaster>
    GetGlobalMessageManager();
  static mozilla::dom::ParentProcessMessageManager* GetParentProcessManager()
  {
    return sParentProcessManager;
  }
  static mozilla::dom::ChildProcessMessageManager* GetChildProcessManager()
  {
    return sChildProcessManager;
  }
  static void SetChildProcessManager(mozilla::dom::ChildProcessMessageManager* aManager)
  {
    sChildProcessManager = aManager;
  }

  void SetInitialProcessData(JS::HandleValue aInitialData);

  void LoadPendingScripts();

protected:
  friend class MMListenerRemover;

  virtual mozilla::dom::MessageBroadcaster* GetParentManager()
  {
    return nullptr;
  }
  virtual void ClearParentManager(bool aRemove)
  {
  }

  void DispatchAsyncMessage(JSContext* aCx, const nsAString& aMessageName,
                            JS::Handle<JS::Value> aObj,
                            JS::Handle<JSObject*> aObjects,
                            nsIPrincipal* aPrincipal,
                            JS::Handle<JS::Value> aTransfers,
                            mozilla::ErrorResult& aError);

  void SendMessage(JSContext* aCx, const nsAString& aMessageName,
                   JS::Handle<JS::Value> aObj, JS::Handle<JSObject*> aObjects,
                   nsIPrincipal* aPrincipal, bool aIsSync, nsTArray<JS::Value>& aResult,
                   mozilla::ErrorResult& aError);

  void ReceiveMessage(nsISupports* aTarget, nsFrameLoader* aTargetFrameLoader,
                      bool aTargetClosed, const nsAString& aMessage, bool aIsSync,
                      StructuredCloneData* aCloneData, mozilla::jsipc::CpowHolder* aCpows,
                      nsIPrincipal* aPrincipal, nsTArray<StructuredCloneData>* aRetVal,
                      mozilla::ErrorResult& aError);

  void LoadScript(const nsAString& aURL, bool aAllowDelayedLoad,
                  bool aRunInGlobalScope, mozilla::ErrorResult& aError);
  void RemoveDelayedScript(const nsAString& aURL);
  void GetDelayedScripts(JSContext* aCx, nsTArray<nsTArray<JS::Value>>& aList,
                         mozilla::ErrorResult& aError);

  // We keep the message listeners as arrays in a hastable indexed by the
  // message name. That gives us fast lookups in ReceiveMessage().
  nsClassHashtable<nsStringHashKey,
                   nsAutoTObserverArray<nsMessageListenerInfo, 1>> mListeners;
  nsTArray<RefPtr<mozilla::dom::MessageListenerManager>> mChildManagers;
  bool mChrome;     // true if we're in the chrome process
  bool mGlobal;     // true if we're the global frame message manager
  bool mIsProcessManager; // true if the message manager belongs to the process realm
  bool mIsBroadcaster; // true if the message manager is a broadcaster
  bool mOwnsCallback;
  bool mHandlingMessage;
  bool mClosed;    // true if we can no longer send messages
  bool mDisconnected;
  mozilla::dom::ipc::MessageManagerCallback* mCallback;
  nsAutoPtr<mozilla::dom::ipc::MessageManagerCallback> mOwnedCallback;
  nsTArray<nsString> mPendingScripts;
  nsTArray<bool> mPendingScriptsGlobalStates;
  JS::Heap<JS::Value> mInitialProcessData;
  RefPtr<mozilla::dom::ipc::WritableSharedMap> mSharedData;

  void LoadPendingScripts(nsFrameMessageManager* aManager,
                          nsFrameMessageManager* aChildMM);
public:
  static mozilla::dom::ParentProcessMessageManager* sParentProcessManager;
  static nsFrameMessageManager* sSameProcessParentManager;
  static nsTArray<nsCOMPtr<nsIRunnable> >* sPendingSameProcessAsyncMessages;
private:
  static mozilla::dom::ChildProcessMessageManager* sChildProcessManager;
};

/* A helper class for taking care of many details for async message sending
   within a single process.  Intended to be used like so:

   class MyAsyncMessage : public nsSameProcessAsyncMessageBase, public Runnable
   {
     NS_IMETHOD Run() {
       ReceiveMessage(..., ...);
       return NS_OK;
     }
   };


   RefPtr<nsSameProcessAsyncMessageBase> ev = new MyAsyncMessage();
   nsresult rv = ev->Init(...);
   if (NS_SUCCEEDED(rv)) {
     NS_DispatchToMainThread(ev);
   }
*/
class nsSameProcessAsyncMessageBase
{
public:
  typedef mozilla::dom::ipc::StructuredCloneData StructuredCloneData;

  nsSameProcessAsyncMessageBase(JS::RootingContext* aRootingCx,
                                JS::Handle<JSObject*> aCpows);
  nsresult Init(const nsAString& aMessage,
                StructuredCloneData& aData,
                nsIPrincipal* aPrincipal);

  void ReceiveMessage(nsISupports* aTarget, nsFrameLoader* aTargetFrameLoader,
                      nsFrameMessageManager* aManager);
private:
  nsSameProcessAsyncMessageBase(const nsSameProcessAsyncMessageBase&);

  nsString mMessage;
  StructuredCloneData mData;
  JS::PersistentRooted<JSObject*> mCpows;
  nsCOMPtr<nsIPrincipal> mPrincipal;
#ifdef DEBUG
  bool mCalledInit;
#endif
};

class nsScriptCacheCleaner;

struct nsMessageManagerScriptHolder
{
  nsMessageManagerScriptHolder(JSContext* aCx,
                               JSScript* aScript)
   : mScript(aCx, aScript)
  { MOZ_COUNT_CTOR(nsMessageManagerScriptHolder); }

  ~nsMessageManagerScriptHolder()
  { MOZ_COUNT_DTOR(nsMessageManagerScriptHolder); }

  JS::PersistentRooted<JSScript*> mScript;
};

class nsMessageManagerScriptExecutor
{
public:
  static void PurgeCache();
  static void Shutdown();

  void MarkScopesForCC();
protected:
  friend class nsMessageManagerScriptCx;
  nsMessageManagerScriptExecutor() { MOZ_COUNT_CTOR(nsMessageManagerScriptExecutor); }
  ~nsMessageManagerScriptExecutor() { MOZ_COUNT_DTOR(nsMessageManagerScriptExecutor); }

  void DidCreateScriptLoader();
  void LoadScriptInternal(JS::Handle<JSObject*> aMessageManager, const nsAString& aURL,
                          bool aRunInUniqueScope);
  void TryCacheLoadAndCompileScript(const nsAString& aURL,
                                    bool aRunInUniqueScope,
                                    bool aShouldCache,
                                    JS::Handle<JSObject*> aMessageManager,
                                    JS::MutableHandle<JSScript*> aScriptp);
  bool Init();
  void Trace(const TraceCallbacks& aCallbacks, void* aClosure);
  void Unlink();
  nsCOMPtr<nsIPrincipal> mPrincipal;
  AutoTArray<JS::Heap<JSObject*>, 2> mAnonymousGlobalScopes;

  // Returns true if this is a process message manager. There should only be a
  // single process message manager per session, so instances of this type will
  // optimize their script loading to avoid unnecessary duplication.
  virtual bool IsProcessScoped() const
  {
    return false;
  }

  static nsDataHashtable<nsStringHashKey, nsMessageManagerScriptHolder*>* sCachedScripts;
  static mozilla::StaticRefPtr<nsScriptCacheCleaner> sScriptCacheCleaner;
};

class nsScriptCacheCleaner final : public nsIObserver
{
  ~nsScriptCacheCleaner() {}

  NS_DECL_ISUPPORTS

  nsScriptCacheCleaner()
  {
    nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
    if (obsSvc) {
      obsSvc->AddObserver(this, "message-manager-flush-caches", false);
      obsSvc->AddObserver(this, "xpcom-shutdown", false);
    }
  }

  NS_IMETHOD Observe(nsISupports *aSubject,
                     const char *aTopic,
                     const char16_t *aData) override
  {
    if (strcmp("message-manager-flush-caches", aTopic) == 0) {
      nsMessageManagerScriptExecutor::PurgeCache();
    } else if (strcmp("xpcom-shutdown", aTopic) == 0) {
      nsMessageManagerScriptExecutor::Shutdown();
    }
    return NS_OK;
  }
};

#endif
