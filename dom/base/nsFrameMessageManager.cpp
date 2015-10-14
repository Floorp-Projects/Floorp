/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "nsFrameMessageManager.h"

#include "AppProcessChecker.h"
#include "ContentChild.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsJSUtils.h"
#include "nsJSPrincipals.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "nsFrameLoader.h"
#include "nsIXULRuntime.h"
#include "nsIScriptError.h"
#include "nsIConsoleService.h"
#include "nsIMemoryReporter.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptSecurityManager.h"
#include "nsIDOMClassInfo.h"
#include "xpcpublic.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/IntentionalCrash.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/nsIContentParent.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/ProcessGlobal.h"
#include "mozilla/dom/SameProcessMessageQueue.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "nsPrintfCString.h"
#include "nsXULAppAPI.h"
#include "nsQueryObject.h"
#include <algorithm>

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

#ifdef ANDROID
#include <android/log.h>
#endif
#ifdef XP_WIN
#include <windows.h>
# if defined(SendMessage)
#  undef SendMessage
# endif
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;

nsFrameMessageManager::nsFrameMessageManager(mozilla::dom::ipc::MessageManagerCallback* aCallback,
                                             nsFrameMessageManager* aParentManager,
                                             /* mozilla::dom::ipc::MessageManagerFlags */ uint32_t aFlags)
 : mChrome(!!(aFlags & mozilla::dom::ipc::MM_CHROME)),
   mGlobal(!!(aFlags & mozilla::dom::ipc::MM_GLOBAL)),
   mIsProcessManager(!!(aFlags & mozilla::dom::ipc::MM_PROCESSMANAGER)),
   mIsBroadcaster(!!(aFlags & mozilla::dom::ipc::MM_BROADCASTER)),
   mOwnsCallback(!!(aFlags & mozilla::dom::ipc::MM_OWNSCALLBACK)),
  mHandlingMessage(false),
  mClosed(false),
  mDisconnected(false),
  mCallback(aCallback),
  mParentManager(aParentManager)
{
  NS_ASSERTION(mChrome || !aParentManager, "Should not set parent manager!");
  NS_ASSERTION(!mIsBroadcaster || !mCallback,
               "Broadcasters cannot have callbacks!");
  if (mIsProcessManager && (!mChrome || IsBroadcaster())) {
    mozilla::HoldJSObjects(this);
  }
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

nsFrameMessageManager::~nsFrameMessageManager()
{
  if (mIsProcessManager && (!mChrome || IsBroadcaster())) {
    mozilla::DropJSObjects(this);
  }
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
      delete mozilla::dom::SameProcessMessageQueue::Get();
    }
    if (this == sSameProcessParentManager) {
      sSameProcessParentManager = nullptr;
    }
  }
}

static PLDHashOperator
CycleCollectorTraverseListeners(const nsAString& aKey,
                                nsAutoTObserverArray<nsMessageListenerInfo, 1>* aListeners,
                                void* aCb)
{
  nsCycleCollectionTraversalCallback* cb =
    static_cast<nsCycleCollectionTraversalCallback*> (aCb);
  uint32_t count = aListeners->Length();
  for (uint32_t i = 0; i < count; ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "listeners[i] mStrongListener");
    cb->NoteXPCOMChild(aListeners->ElementAt(i).mStrongListener.get());
  }
  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsFrameMessageManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsFrameMessageManager)
  tmp->mListeners.EnumerateRead(CycleCollectorTraverseListeners,
                                static_cast<void*>(&cb));
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChildManagers)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParentManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsFrameMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mInitialProcessData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsFrameMessageManager)
  tmp->mListeners.Clear();
  for (int32_t i = tmp->mChildManagers.Count(); i > 0; --i) {
    static_cast<nsFrameMessageManager*>(tmp->mChildManagers[i - 1])->
      Disconnect(false);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChildManagers)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParentManager)
  tmp->mInitialProcessData.setNull();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentFrameMessageManager)

  /* nsFrameMessageManager implements nsIMessageSender and nsIMessageBroadcaster,
   * both of which descend from nsIMessageListenerManager. QI'ing to
   * nsIMessageListenerManager is therefore ambiguous and needs explicit casts
   * depending on which child interface applies. */
  NS_INTERFACE_MAP_ENTRY_AGGREGATED(nsIMessageListenerManager,
                                    (mIsBroadcaster ?
                                       static_cast<nsIMessageListenerManager*>(
                                         static_cast<nsIMessageBroadcaster*>(this)) :
                                       static_cast<nsIMessageListenerManager*>(
                                         static_cast<nsIMessageSender*>(this))))

  /* Message managers in child process implement nsIMessageSender and
     nsISyncMessageSender.  Message managers in the chrome process are
     either broadcasters (if they have subordinate/child message
     managers) or they're simple message senders. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISyncMessageSender, !mChrome)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIMessageSender, !mChrome || !mIsBroadcaster)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIMessageBroadcaster, mChrome && mIsBroadcaster)

  /* nsIContentFrameMessageManager is accessible only in TabChildGlobal. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIContentFrameMessageManager,
                                     !mChrome && !mIsProcessManager)

  /* Frame message managers (non-process message managers) support nsIFrameScriptLoader. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIFrameScriptLoader,
                                     mChrome && !mIsProcessManager)

  /* Process message managers (process message managers) support nsIProcessScriptLoader. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIProcessScriptLoader,
                                     mChrome && mIsProcessManager)

  /* Global process message managers (process message managers) support nsIGlobalProcessScriptLoader. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIGlobalProcessScriptLoader,
                                     mChrome && mIsProcessManager && mIsBroadcaster)

  /* Message senders in the chrome process support nsIProcessChecker. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIProcessChecker,
                                     mChrome && !mIsBroadcaster)

  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(ChromeMessageBroadcaster,
                                                   mChrome && mIsBroadcaster)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(ChromeMessageSender,
                                                   mChrome && !mIsBroadcaster)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFrameMessageManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFrameMessageManager)

enum ActorFlavorEnum {
  Parent = 0,
  Child
};

template <ActorFlavorEnum>
struct BlobTraits
{ };

template <>
struct BlobTraits<Parent>
{
  typedef mozilla::dom::BlobParent BlobType;
  typedef mozilla::dom::PBlobParent ProtocolType;
  typedef mozilla::dom::nsIContentParent ConcreteContentManagerType;
};

template <>
struct BlobTraits<Child>
{
  typedef mozilla::dom::BlobChild BlobType;
  typedef mozilla::dom::PBlobChild ProtocolType;
  typedef mozilla::dom::nsIContentChild ConcreteContentManagerType;
};

template<ActorFlavorEnum>
struct DataBlobs
{ };

template<>
struct DataBlobs<Parent>
{
  typedef BlobTraits<Parent>::ProtocolType ProtocolType;

  static InfallibleTArray<ProtocolType*>& Blobs(ClonedMessageData& aData)
  {
    return aData.blobsParent();
  }

  static const InfallibleTArray<ProtocolType*>& Blobs(const ClonedMessageData& aData)
  {
    return aData.blobsParent();
  }
};

template<>
struct DataBlobs<Child>
{
  typedef BlobTraits<Child>::ProtocolType ProtocolType;

  static InfallibleTArray<ProtocolType*>& Blobs(ClonedMessageData& aData)
  {
    return aData.blobsChild();
  }

  static const InfallibleTArray<ProtocolType*>& Blobs(const ClonedMessageData& aData)
  {
    return aData.blobsChild();
  }
};

template<ActorFlavorEnum Flavor>
static bool
BuildClonedMessageData(typename BlobTraits<Flavor>::ConcreteContentManagerType* aManager,
                       StructuredCloneData& aData,
                       ClonedMessageData& aClonedData)
{
  SerializedStructuredCloneBuffer& buffer = aClonedData.data();
  buffer.data = aData.Data();
  buffer.dataLength = aData.DataLength();
  const nsTArray<nsRefPtr<BlobImpl>>& blobImpls = aData.BlobImpls();

  if (!blobImpls.IsEmpty()) {
    typedef typename BlobTraits<Flavor>::ProtocolType ProtocolType;
    InfallibleTArray<ProtocolType*>& blobList = DataBlobs<Flavor>::Blobs(aClonedData);
    uint32_t length = blobImpls.Length();
    blobList.SetCapacity(length);
    for (uint32_t i = 0; i < length; ++i) {
      typename BlobTraits<Flavor>::BlobType* protocolActor =
        aManager->GetOrCreateActorForBlobImpl(blobImpls[i]);
      if (!protocolActor) {
        return false;
      }
      blobList.AppendElement(protocolActor);
    }
  }
  return true;
}

bool
MessageManagerCallback::BuildClonedMessageDataForParent(nsIContentParent* aParent,
                                                        StructuredCloneData& aData,
                                                        ClonedMessageData& aClonedData)
{
  return BuildClonedMessageData<Parent>(aParent, aData, aClonedData);
}

bool
MessageManagerCallback::BuildClonedMessageDataForChild(nsIContentChild* aChild,
                                                       StructuredCloneData& aData,
                                                       ClonedMessageData& aClonedData)
{
  return BuildClonedMessageData<Child>(aChild, aData, aClonedData);
}

template<ActorFlavorEnum Flavor>
static void
UnpackClonedMessageData(const ClonedMessageData& aClonedData,
                        StructuredCloneData& aData)
{
  const SerializedStructuredCloneBuffer& buffer = aClonedData.data();
  typedef typename BlobTraits<Flavor>::ProtocolType ProtocolType;
  const InfallibleTArray<ProtocolType*>& blobs = DataBlobs<Flavor>::Blobs(aClonedData);

  aData.UseExternalData(buffer.data, buffer.dataLength);

  if (!blobs.IsEmpty()) {
    uint32_t length = blobs.Length();
    aData.BlobImpls().SetCapacity(length);
    for (uint32_t i = 0; i < length; ++i) {
      auto* blob =
        static_cast<typename BlobTraits<Flavor>::BlobType*>(blobs[i]);
      MOZ_ASSERT(blob);

      nsRefPtr<BlobImpl> blobImpl = blob->GetBlobImpl();
      MOZ_ASSERT(blobImpl);

      aData.BlobImpls().AppendElement(blobImpl);
    }
  }
}

void
mozilla::dom::ipc::UnpackClonedMessageDataForParent(const ClonedMessageData& aClonedData,
                                                    StructuredCloneData& aData)
{
  UnpackClonedMessageData<Parent>(aClonedData, aData);
}

void
mozilla::dom::ipc::UnpackClonedMessageDataForChild(const ClonedMessageData& aClonedData,
                                                   StructuredCloneData& aData)
{
  UnpackClonedMessageData<Child>(aClonedData, aData);
}

bool
SameProcessCpowHolder::ToObject(JSContext* aCx,
                                JS::MutableHandle<JSObject*> aObjp)
{
  if (!mObj) {
    return true;
  }

  aObjp.set(mObj);
  return JS_WrapObject(aCx, aObjp);
}

// nsIMessageListenerManager

NS_IMETHODIMP
nsFrameMessageManager::AddMessageListener(const nsAString& aMessage,
                                          nsIMessageListener* aListener,
                                          bool aListenWhenClosed)
{
  nsAutoTObserverArray<nsMessageListenerInfo, 1>* listeners =
    mListeners.Get(aMessage);
  if (!listeners) {
    listeners = new nsAutoTObserverArray<nsMessageListenerInfo, 1>();
    mListeners.Put(aMessage, listeners);
  } else {
    uint32_t len = listeners->Length();
    for (uint32_t i = 0; i < len; ++i) {
      if (listeners->ElementAt(i).mStrongListener == aListener) {
        return NS_OK;
      }
    }
  }

  nsMessageListenerInfo* entry = listeners->AppendElement();
  NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);
  entry->mStrongListener = aListener;
  entry->mListenWhenClosed = aListenWhenClosed;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::RemoveMessageListener(const nsAString& aMessage,
                                             nsIMessageListener* aListener)
{
  nsAutoTObserverArray<nsMessageListenerInfo, 1>* listeners =
    mListeners.Get(aMessage);
  if (!listeners) {
    return NS_OK;
  }

  uint32_t len = listeners->Length();
  for (uint32_t i = 0; i < len; ++i) {
    if (listeners->ElementAt(i).mStrongListener == aListener) {
      listeners->RemoveElementAt(i);
      return NS_OK;
    }
  }
  return NS_OK;
}

#ifdef DEBUG
typedef struct
{
  nsCOMPtr<nsISupports> mCanonical;
  nsWeakPtr mWeak;
} CanonicalCheckerParams;

static PLDHashOperator
CanonicalChecker(const nsAString& aKey,
                 nsAutoTObserverArray<nsMessageListenerInfo, 1>* aListeners,
                 void* aParams)
{
  CanonicalCheckerParams* params =
    static_cast<CanonicalCheckerParams*> (aParams);

  uint32_t count = aListeners->Length();
  for (uint32_t i = 0; i < count; i++) {
    if (!aListeners->ElementAt(i).mWeakListener) {
      continue;
    }
    nsCOMPtr<nsISupports> otherCanonical =
      do_QueryReferent(aListeners->ElementAt(i).mWeakListener);
    MOZ_ASSERT((params->mCanonical == otherCanonical) ==
               (params->mWeak == aListeners->ElementAt(i).mWeakListener));
  }
  return PL_DHASH_NEXT;
}
#endif

NS_IMETHODIMP
nsFrameMessageManager::AddWeakMessageListener(const nsAString& aMessage,
                                              nsIMessageListener* aListener)
{
  nsWeakPtr weak = do_GetWeakReference(aListener);
  NS_ENSURE_TRUE(weak, NS_ERROR_NO_INTERFACE);

#ifdef DEBUG
  // It's technically possible that one object X could give two different
  // nsIWeakReference*'s when you do_GetWeakReference(X).  We really don't want
  // this to happen; it will break e.g. RemoveWeakMessageListener.  So let's
  // check that we're not getting ourselves into that situation.
  nsCOMPtr<nsISupports> canonical = do_QueryInterface(aListener);
  CanonicalCheckerParams params;
  params.mCanonical = canonical;
  params.mWeak = weak;
  mListeners.EnumerateRead(CanonicalChecker, (void*)&params);
#endif

  nsAutoTObserverArray<nsMessageListenerInfo, 1>* listeners =
    mListeners.Get(aMessage);
  if (!listeners) {
    listeners = new nsAutoTObserverArray<nsMessageListenerInfo, 1>();
    mListeners.Put(aMessage, listeners);
  } else {
    uint32_t len = listeners->Length();
    for (uint32_t i = 0; i < len; ++i) {
      if (listeners->ElementAt(i).mWeakListener == weak) {
        return NS_OK;
      }
    }
  }

  nsMessageListenerInfo* entry = listeners->AppendElement();
  entry->mWeakListener = weak;
  entry->mListenWhenClosed = false;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::RemoveWeakMessageListener(const nsAString& aMessage,
                                                 nsIMessageListener* aListener)
{
  nsWeakPtr weak = do_GetWeakReference(aListener);
  NS_ENSURE_TRUE(weak, NS_OK);

  nsAutoTObserverArray<nsMessageListenerInfo, 1>* listeners =
    mListeners.Get(aMessage);
  if (!listeners) {
    return NS_OK;
  }

  uint32_t len = listeners->Length();
  for (uint32_t i = 0; i < len; ++i) {
    if (listeners->ElementAt(i).mWeakListener == weak) {
      listeners->RemoveElementAt(i);
      return NS_OK;
    }
  }

  return NS_OK;
}

// nsIFrameScriptLoader

NS_IMETHODIMP
nsFrameMessageManager::LoadScript(const nsAString& aURL,
                                  bool aAllowDelayedLoad,
                                  bool aRunInGlobalScope)
{
  if (aAllowDelayedLoad) {
    // Cache for future windows or frames
    mPendingScripts.AppendElement(aURL);
    mPendingScriptsGlobalStates.AppendElement(aRunInGlobalScope);
  }

  if (mCallback) {
#ifdef DEBUG_smaug
    printf("Will load %s \n", NS_ConvertUTF16toUTF8(aURL).get());
#endif
    NS_ENSURE_TRUE(mCallback->DoLoadMessageManagerScript(aURL, aRunInGlobalScope),
                   NS_ERROR_FAILURE);
  }

  for (int32_t i = 0; i < mChildManagers.Count(); ++i) {
    nsRefPtr<nsFrameMessageManager> mm =
      static_cast<nsFrameMessageManager*>(mChildManagers[i]);
    if (mm) {
      // Use false here, so that child managers don't cache the script, which
      // is already cached in the parent.
      mm->LoadScript(aURL, false, aRunInGlobalScope);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::RemoveDelayedScript(const nsAString& aURL)
{
  for (uint32_t i = 0; i < mPendingScripts.Length(); ++i) {
    if (mPendingScripts[i] == aURL) {
      mPendingScripts.RemoveElementAt(i);
      mPendingScriptsGlobalStates.RemoveElementAt(i);
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::GetDelayedScripts(JSContext* aCx, JS::MutableHandle<JS::Value> aList)
{
  // Frame message managers may return an incomplete list because scripts
  // that were loaded after it was connected are not added to the list.
  if (!IsGlobal() && !IsBroadcaster()) {
    NS_WARNING("Cannot retrieve list of pending frame scripts for frame"
               "message managers as it may be incomplete");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  JS::Rooted<JSObject*> array(aCx, JS_NewArrayObject(aCx, mPendingScripts.Length()));
  NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);

  JS::Rooted<JSString*> url(aCx);
  JS::Rooted<JSObject*> pair(aCx);
  for (uint32_t i = 0; i < mPendingScripts.Length(); ++i) {
    url = JS_NewUCStringCopyN(aCx, mPendingScripts[i].get(), mPendingScripts[i].Length());
    NS_ENSURE_TRUE(url, NS_ERROR_OUT_OF_MEMORY);

    JS::AutoValueArray<2> pairElts(aCx);
    pairElts[0].setString(url);
    pairElts[1].setBoolean(mPendingScriptsGlobalStates[i]);

    pair = JS_NewArrayObject(aCx, pairElts);
    NS_ENSURE_TRUE(pair, NS_ERROR_OUT_OF_MEMORY);

    NS_ENSURE_TRUE(JS_DefineElement(aCx, array, i, pair, JSPROP_ENUMERATE),
                   NS_ERROR_OUT_OF_MEMORY);
  }

  aList.setObject(*array);
  return NS_OK;
}

// nsIFrameScriptLoader

NS_IMETHODIMP
nsFrameMessageManager::LoadFrameScript(const nsAString& aURL,
                                       bool aAllowDelayedLoad,
                                       bool aRunInGlobalScope)
{
  return LoadScript(aURL, aAllowDelayedLoad, aRunInGlobalScope);
}

NS_IMETHODIMP
nsFrameMessageManager::RemoveDelayedFrameScript(const nsAString& aURL)
{
  return RemoveDelayedScript(aURL);
}

NS_IMETHODIMP
nsFrameMessageManager::GetDelayedFrameScripts(JSContext* aCx, JS::MutableHandle<JS::Value> aList)
{
  return GetDelayedScripts(aCx, aList);
}

// nsIProcessScriptLoader

NS_IMETHODIMP
nsFrameMessageManager::LoadProcessScript(const nsAString& aURL,
                                         bool aAllowDelayedLoad)
{
  return LoadScript(aURL, aAllowDelayedLoad, false);
}

NS_IMETHODIMP
nsFrameMessageManager::RemoveDelayedProcessScript(const nsAString& aURL)
{
  return RemoveDelayedScript(aURL);
}

NS_IMETHODIMP
nsFrameMessageManager::GetDelayedProcessScripts(JSContext* aCx, JS::MutableHandle<JS::Value> aList)
{
  return GetDelayedScripts(aCx, aList);
}

static bool
JSONCreator(const char16_t* aBuf, uint32_t aLen, void* aData)
{
  nsAString* result = static_cast<nsAString*>(aData);
  result->Append(static_cast<const char16_t*>(aBuf),
                 static_cast<uint32_t>(aLen));
  return true;
}

static bool
GetParamsForMessage(JSContext* aCx,
                    const JS::Value& aValue,
                    StructuredCloneData& aData)
{
  // First try to use structured clone on the whole thing.
  JS::RootedValue v(aCx, aValue);
  ErrorResult rv;
  aData.Write(aCx, v, rv);
  if (!rv.Failed()) {
    return true;
  }

  rv.SuppressException();
  JS_ClearPendingException(aCx);

  nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (console) {
    nsAutoString filename;
    uint32_t lineno = 0, column = 0;
    nsJSUtils::GetCallingLocation(aCx, filename, &lineno, &column);
    nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
    error->Init(NS_LITERAL_STRING("Sending message that cannot be cloned. Are you trying to send an XPCOM object?"),
                filename, EmptyString(), lineno, column,
                nsIScriptError::warningFlag, "chrome javascript");
    console->LogMessage(error);
  }

  // Not clonable, try JSON
  //XXX This is ugly but currently structured cloning doesn't handle
  //    properly cases when interface is implemented in JS and used
  //    as a dictionary.
  nsAutoString json;
  NS_ENSURE_TRUE(JS_Stringify(aCx, &v, nullptr, JS::NullHandleValue,
                              JSONCreator, &json), false);
  NS_ENSURE_TRUE(!json.IsEmpty(), false);

  JS::Rooted<JS::Value> val(aCx, JS::NullValue());
  NS_ENSURE_TRUE(JS_ParseJSON(aCx, static_cast<const char16_t*>(json.get()),
                              json.Length(), &val), false);

  aData.Write(aCx, val, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    return false;
  }

  return true;
}

// nsISyncMessageSender

static bool sSendingSyncMessage = false;

NS_IMETHODIMP
nsFrameMessageManager::SendSyncMessage(const nsAString& aMessageName,
                                       JS::Handle<JS::Value> aJSON,
                                       JS::Handle<JS::Value> aObjects,
                                       nsIPrincipal* aPrincipal,
                                       JSContext* aCx,
                                       uint8_t aArgc,
                                       JS::MutableHandle<JS::Value> aRetval)
{
  return SendMessage(aMessageName, aJSON, aObjects, aPrincipal, aCx, aArgc,
                     aRetval, true);
}

NS_IMETHODIMP
nsFrameMessageManager::SendRpcMessage(const nsAString& aMessageName,
                                      JS::Handle<JS::Value> aJSON,
                                      JS::Handle<JS::Value> aObjects,
                                      nsIPrincipal* aPrincipal,
                                      JSContext* aCx,
                                      uint8_t aArgc,
                                      JS::MutableHandle<JS::Value> aRetval)
{
  return SendMessage(aMessageName, aJSON, aObjects, aPrincipal, aCx, aArgc,
                     aRetval, false);
}

nsresult
nsFrameMessageManager::SendMessage(const nsAString& aMessageName,
                                   JS::Handle<JS::Value> aJSON,
                                   JS::Handle<JS::Value> aObjects,
                                   nsIPrincipal* aPrincipal,
                                   JSContext* aCx,
                                   uint8_t aArgc,
                                   JS::MutableHandle<JS::Value> aRetval,
                                   bool aIsSync)
{
  NS_ASSERTION(!IsGlobal(), "Should not call SendSyncMessage in chrome");
  NS_ASSERTION(!IsBroadcaster(), "Should not call SendSyncMessage in chrome");
  NS_ASSERTION(!mParentManager, "Should not have parent manager in content!");

  aRetval.setUndefined();
  NS_ENSURE_TRUE(mCallback, NS_ERROR_NOT_INITIALIZED);

  if (sSendingSyncMessage && aIsSync) {
    // No kind of blocking send should be issued on top of a sync message.
    return NS_ERROR_UNEXPECTED;
  }

  StructuredCloneData data;
  if (aArgc >= 2 && !GetParamsForMessage(aCx, aJSON, data)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  JS::Rooted<JSObject*> objects(aCx);
  if (aArgc >= 3 && aObjects.isObject()) {
    objects = &aObjects.toObject();
  }

  nsTArray<StructuredCloneData> retval;

  sSendingSyncMessage |= aIsSync;
  bool ok = mCallback->DoSendBlockingMessage(aCx, aMessageName, data, objects,
                                             aPrincipal, &retval, aIsSync);
  if (aIsSync) {
    sSendingSyncMessage = false;
  }

  if (!ok) {
    return NS_OK;
  }

  uint32_t len = retval.Length();
  JS::Rooted<JSObject*> dataArray(aCx, JS_NewArrayObject(aCx, len));
  NS_ENSURE_TRUE(dataArray, NS_ERROR_OUT_OF_MEMORY);

  for (uint32_t i = 0; i < len; ++i) {
    JS::Rooted<JS::Value> ret(aCx);
    ErrorResult rv;
    retval[i].Read(aCx, &ret, rv);
    if (rv.Failed()) {
      MOZ_ASSERT(false, "Unable to read structured clone in SendMessage");
      return NS_ERROR_UNEXPECTED;
    }

    NS_ENSURE_TRUE(JS_DefineElement(aCx, dataArray, i, ret, JSPROP_ENUMERATE),
                   NS_ERROR_OUT_OF_MEMORY);
  }

  aRetval.setObject(*dataArray);
  return NS_OK;
}

nsresult
nsFrameMessageManager::DispatchAsyncMessageInternal(JSContext* aCx,
                                                    const nsAString& aMessage,
                                                    StructuredCloneData& aData,
                                                    JS::Handle<JSObject *> aCpows,
                                                    nsIPrincipal* aPrincipal)
{
  if (mIsBroadcaster) {
    int32_t len = mChildManagers.Count();
    for (int32_t i = 0; i < len; ++i) {
      static_cast<nsFrameMessageManager*>(mChildManagers[i])->
         DispatchAsyncMessageInternal(aCx, aMessage, aData, aCpows, aPrincipal);
    }
    return NS_OK;
  }

  if (!mCallback) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = mCallback->DoSendAsyncMessage(aCx, aMessage, aData, aCpows, aPrincipal);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

nsresult
nsFrameMessageManager::DispatchAsyncMessage(const nsAString& aMessageName,
                                            const JS::Value& aJSON,
                                            const JS::Value& aObjects,
                                            nsIPrincipal* aPrincipal,
                                            JSContext* aCx,
                                            uint8_t aArgc)
{
  StructuredCloneData data;
  if (aArgc >= 2 && !GetParamsForMessage(aCx, aJSON, data)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  JS::Rooted<JSObject*> objects(aCx);
  if (aArgc >= 3 && aObjects.isObject()) {
    objects = &aObjects.toObject();
  }

  return DispatchAsyncMessageInternal(aCx, aMessageName, data, objects,
                                      aPrincipal);
}


// nsIMessageSender

NS_IMETHODIMP
nsFrameMessageManager::SendAsyncMessage(const nsAString& aMessageName,
                                        JS::Handle<JS::Value> aJSON,
                                        JS::Handle<JS::Value> aObjects,
                                        nsIPrincipal* aPrincipal,
                                        JSContext* aCx,
                                        uint8_t aArgc)
{
  return DispatchAsyncMessage(aMessageName, aJSON, aObjects, aPrincipal, aCx,
                              aArgc);
}


// nsIMessageBroadcaster

NS_IMETHODIMP
nsFrameMessageManager::BroadcastAsyncMessage(const nsAString& aMessageName,
                                             JS::Handle<JS::Value> aJSON,
                                             JS::Handle<JS::Value> aObjects,
                                             JSContext* aCx,
                                             uint8_t aArgc)
{
  return DispatchAsyncMessage(aMessageName, aJSON, aObjects, nullptr, aCx,
                              aArgc);
}

NS_IMETHODIMP
nsFrameMessageManager::GetChildCount(uint32_t* aChildCount)
{
  *aChildCount = static_cast<uint32_t>(mChildManagers.Count());
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::GetChildAt(uint32_t aIndex,
                                  nsIMessageListenerManager** aMM)
{
  *aMM = nullptr;
  nsCOMPtr<nsIMessageListenerManager> mm =
    do_QueryInterface(mChildManagers.SafeObjectAt(static_cast<uint32_t>(aIndex)));
  mm.swap(*aMM);
  return NS_OK;
}


// nsIContentFrameMessageManager

NS_IMETHODIMP
nsFrameMessageManager::Dump(const nsAString& aStr)
{
#ifdef ANDROID
  __android_log_print(ANDROID_LOG_INFO, "Gecko", "%s", NS_ConvertUTF16toUTF8(aStr).get());
#endif
#ifdef XP_WIN
  if (IsDebuggerPresent()) {
    OutputDebugStringW(PromiseFlatString(aStr).get());
  }
#endif
  fputs(NS_ConvertUTF16toUTF8(aStr).get(), stdout);
  fflush(stdout);
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::PrivateNoteIntentionalCrash()
{
  if (XRE_IsContentProcess()) {
    mozilla::NoteIntentionalCrash("tab");
    return NS_OK;
  } else {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
}

NS_IMETHODIMP
nsFrameMessageManager::GetContent(nsIDOMWindow** aContent)
{
  *aContent = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::GetDocShell(nsIDocShell** aDocShell)
{
  *aDocShell = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::Btoa(const nsAString& aBinaryData,
                            nsAString& aAsciiBase64String)
{
  return nsContentUtils::Btoa(aBinaryData, aAsciiBase64String);
}

NS_IMETHODIMP
nsFrameMessageManager::Atob(const nsAString& aAsciiString,
                            nsAString& aBinaryData)
{
  return nsContentUtils::Atob(aAsciiString, aBinaryData);
}

// nsIProcessChecker

NS_IMETHODIMP
nsFrameMessageManager::KillChild(bool *aValid)
{
  if (!mCallback) {
    *aValid = false;
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aValid = mCallback->KillChild();
  return NS_OK;
}

nsresult
nsFrameMessageManager::AssertProcessInternal(ProcessCheckerType aType,
                                             const nsAString& aCapability,
                                             bool* aValid)
{
  *aValid = false;

  // This API is only supported for message senders in the chrome process.
  if (!mChrome || mIsBroadcaster) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  if (!mCallback) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  switch (aType) {
    case PROCESS_CHECKER_PERMISSION:
      *aValid = mCallback->CheckPermission(aCapability);
      break;
    case PROCESS_CHECKER_MANIFEST_URL:
      *aValid = mCallback->CheckManifestURL(aCapability);
      break;
    case ASSERT_APP_HAS_PERMISSION:
      *aValid = mCallback->CheckAppHasPermission(aCapability);
      break;
    default:
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::AssertPermission(const nsAString& aPermission,
                                        bool* aHasPermission)
{
  return AssertProcessInternal(PROCESS_CHECKER_PERMISSION,
                               aPermission,
                               aHasPermission);
}

NS_IMETHODIMP
nsFrameMessageManager::AssertContainApp(const nsAString& aManifestURL,
                                        bool* aHasManifestURL)
{
  return AssertProcessInternal(PROCESS_CHECKER_MANIFEST_URL,
                               aManifestURL,
                               aHasManifestURL);
}

NS_IMETHODIMP
nsFrameMessageManager::AssertAppHasPermission(const nsAString& aPermission,
                                              bool* aHasPermission)
{
  return AssertProcessInternal(ASSERT_APP_HAS_PERMISSION,
                               aPermission,
                               aHasPermission);
}

NS_IMETHODIMP
nsFrameMessageManager::AssertAppHasStatus(unsigned short aStatus,
                                          bool* aHasStatus)
{
  *aHasStatus = false;

  // This API is only supported for message senders in the chrome process.
  if (!mChrome || mIsBroadcaster) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  if (!mCallback) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aHasStatus = mCallback->CheckAppHasStatus(aStatus);

  return NS_OK;
}

class MMListenerRemover
{
public:
  explicit MMListenerRemover(nsFrameMessageManager* aMM)
    : mWasHandlingMessage(aMM->mHandlingMessage)
    , mMM(aMM)
  {
    mMM->mHandlingMessage = true;
  }
  ~MMListenerRemover()
  {
    if (!mWasHandlingMessage) {
      mMM->mHandlingMessage = false;
      if (mMM->mDisconnected) {
        mMM->mListeners.Clear();
      }
    }
  }

  bool mWasHandlingMessage;
  nsRefPtr<nsFrameMessageManager> mMM;
};


// nsIMessageListener

nsresult
nsFrameMessageManager::ReceiveMessage(nsISupports* aTarget,
                                      nsIFrameLoader* aTargetFrameLoader,
                                      const nsAString& aMessage,
                                      bool aIsSync,
                                      StructuredCloneData* aCloneData,
                                      mozilla::jsipc::CpowHolder* aCpows,
                                      nsIPrincipal* aPrincipal,
                                      nsTArray<StructuredCloneData>* aRetVal)
{
  return ReceiveMessage(aTarget, aTargetFrameLoader, mClosed, aMessage, aIsSync,
                        aCloneData, aCpows, aPrincipal, aRetVal);
}

nsresult
nsFrameMessageManager::ReceiveMessage(nsISupports* aTarget,
                                      nsIFrameLoader* aTargetFrameLoader,
                                      bool aTargetClosed,
                                      const nsAString& aMessage,
                                      bool aIsSync,
                                      StructuredCloneData* aCloneData,
                                      mozilla::jsipc::CpowHolder* aCpows,
                                      nsIPrincipal* aPrincipal,
                                      nsTArray<StructuredCloneData>* aRetVal)
{
  nsAutoTObserverArray<nsMessageListenerInfo, 1>* listeners =
    mListeners.Get(aMessage);
  if (listeners) {

    MMListenerRemover lr(this);

    nsAutoTObserverArray<nsMessageListenerInfo, 1>::EndLimitedIterator
      iter(*listeners);
    while(iter.HasMore()) {
      nsMessageListenerInfo& listener = iter.GetNext();
      // Remove mListeners[i] if it's an expired weak listener.
      nsCOMPtr<nsISupports> weakListener;
      if (listener.mWeakListener) {
        weakListener = do_QueryReferent(listener.mWeakListener);
        if (!weakListener) {
          listeners->RemoveElement(listener);
          continue;
        }
      }

      if (!listener.mListenWhenClosed && aTargetClosed) {
        continue;
      }

      nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS;
      if (weakListener) {
        wrappedJS = do_QueryInterface(weakListener);
      } else {
        wrappedJS = do_QueryInterface(listener.mStrongListener);
      }

      if (!wrappedJS) {
        continue;
      }

      if (!wrappedJS->GetJSObject()) {
        continue;
      }

      // Note - The ergonomics here will get a lot better with bug 971673:
      //
      // AutoEntryScript aes;
      // if (!aes.Init(wrappedJS->GetJSObject())) {
      //   continue;
      // }
      // JSContext* cx = aes.cx();
      nsIGlobalObject* nativeGlobal =
        xpc::NativeGlobal(js::GetGlobalForObjectCrossCompartment(wrappedJS->GetJSObject()));
      AutoEntryScript aes(nativeGlobal, "message manager handler");
      aes.TakeOwnershipOfErrorReporting();
      JSContext* cx = aes.cx();
      JS::Rooted<JSObject*> object(cx, wrappedJS->GetJSObject());

      // The parameter for the listener function.
      JS::Rooted<JSObject*> param(cx, JS_NewPlainObject(cx));
      NS_ENSURE_TRUE(param, NS_ERROR_OUT_OF_MEMORY);

      JS::Rooted<JS::Value> targetv(cx);
      js::AssertSameCompartment(cx, object);
      nsresult rv = nsContentUtils::WrapNative(cx, aTarget, &targetv);
      NS_ENSURE_SUCCESS(rv, rv);

      JS::Rooted<JSObject*> cpows(cx);
      if (aCpows) {
        if (!aCpows->ToObject(cx, &cpows)) {
          return NS_ERROR_UNEXPECTED;
        }
      }

      if (!cpows) {
        cpows = JS_NewPlainObject(cx);
        if (!cpows) {
          return NS_ERROR_UNEXPECTED;
        }
      }

      JS::Rooted<JS::Value> cpowsv(cx, JS::ObjectValue(*cpows));

      JS::Rooted<JS::Value> json(cx, JS::NullValue());
      if (aCloneData && aCloneData->DataLength()) {
        ErrorResult rv;
        aCloneData->Read(cx, &json, rv);
        if (NS_WARN_IF(rv.Failed())) {
          rv.SuppressException();
          JS_ClearPendingException(cx);
          return NS_OK;
        }
      }
      JS::Rooted<JSString*> jsMessage(cx,
        JS_NewUCStringCopyN(cx,
                            static_cast<const char16_t*>(aMessage.BeginReading()),
                            aMessage.Length()));
      NS_ENSURE_TRUE(jsMessage, NS_ERROR_OUT_OF_MEMORY);
      JS::Rooted<JS::Value> syncv(cx, JS::BooleanValue(aIsSync));
      bool ok = JS_DefineProperty(cx, param, "target", targetv, JSPROP_ENUMERATE) &&
                JS_DefineProperty(cx, param, "name", jsMessage, JSPROP_ENUMERATE) &&
                JS_DefineProperty(cx, param, "sync", syncv, JSPROP_ENUMERATE) &&
                JS_DefineProperty(cx, param, "json", json, JSPROP_ENUMERATE) && // deprecated
                JS_DefineProperty(cx, param, "data", json, JSPROP_ENUMERATE) &&
                JS_DefineProperty(cx, param, "objects", cpowsv, JSPROP_ENUMERATE);
      NS_ENSURE_TRUE(ok, NS_ERROR_UNEXPECTED);

      if (aTargetFrameLoader) {
        JS::Rooted<JS::Value> targetFrameLoaderv(cx);
        nsresult rv = nsContentUtils::WrapNative(cx, aTargetFrameLoader, &targetFrameLoaderv);
        NS_ENSURE_SUCCESS(rv, rv);

        ok = JS_DefineProperty(cx, param, "targetFrameLoader", targetFrameLoaderv,
                               JSPROP_ENUMERATE);
        NS_ENSURE_TRUE(ok, NS_ERROR_UNEXPECTED);
      }

      // message.principal == null
      if (!aPrincipal) {
        bool ok = JS_DefineProperty(cx, param, "principal",
                                    JS::UndefinedHandleValue, JSPROP_ENUMERATE);
        NS_ENSURE_TRUE(ok, NS_ERROR_UNEXPECTED);
      }

      // message.principal = the principal
      else {
        JS::Rooted<JS::Value> principalValue(cx);
        nsresult rv = nsContentUtils::WrapNative(cx, aPrincipal,
                                                 &NS_GET_IID(nsIPrincipal),
                                                 &principalValue);
        NS_ENSURE_SUCCESS(rv, rv);
        bool ok = JS_DefineProperty(cx, param, "principal", principalValue,
                                    JSPROP_ENUMERATE);
        NS_ENSURE_TRUE(ok, NS_ERROR_UNEXPECTED);
      }

      JS::Rooted<JS::Value> thisValue(cx, JS::UndefinedValue());

      JS::Rooted<JS::Value> funval(cx);
      if (JS::IsCallable(object)) {
        // If the listener is a JS function:
        funval.setObject(*object);

        // A small hack to get 'this' value right on content side where
        // messageManager is wrapped in TabChildGlobal.
        nsCOMPtr<nsISupports> defaultThisValue;
        if (mChrome) {
          defaultThisValue = do_QueryObject(this);
        } else {
          defaultThisValue = aTarget;
        }
        js::AssertSameCompartment(cx, object);
        nsresult rv = nsContentUtils::WrapNative(cx, defaultThisValue, &thisValue);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        // If the listener is a JS object which has receiveMessage function:
        if (!JS_GetProperty(cx, object, "receiveMessage", &funval) ||
            !funval.isObject()) {
          return NS_ERROR_UNEXPECTED;
        }

        // Check if the object is even callable.
        NS_ENSURE_STATE(JS::IsCallable(&funval.toObject()));
        thisValue.setObject(*object);
      }

      JS::Rooted<JS::Value> rval(cx, JS::UndefinedValue());
      JS::Rooted<JS::Value> argv(cx, JS::ObjectValue(*param));

      {
        JS::Rooted<JSObject*> thisObject(cx, thisValue.toObjectOrNull());

        JSAutoCompartment tac(cx, thisObject);
        if (!JS_WrapValue(cx, &argv)) {
          return NS_ERROR_UNEXPECTED;
        }

        if (!JS_CallFunctionValue(cx, thisObject, funval,
                                  JS::HandleValueArray(argv), &rval)) {
          continue;
        }
        if (aRetVal) {
          ErrorResult rv;
          StructuredCloneData* data = aRetVal->AppendElement();
          data->Write(cx, rval, rv);
          if (NS_WARN_IF(rv.Failed())) {
            aRetVal->RemoveElementAt(aRetVal->Length() - 1);
            nsString msg = aMessage + NS_LITERAL_STRING(": message reply cannot be cloned. Are you trying to send an XPCOM object?");

            nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
            if (console) {
              nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
              error->Init(msg, EmptyString(), EmptyString(),
                          0, 0, nsIScriptError::warningFlag, "chrome javascript");
              console->LogMessage(error);
            }

            JS_ClearPendingException(cx);
            continue;
          }
        }
      }
    }
  }
  nsRefPtr<nsFrameMessageManager> kungfuDeathGrip = mParentManager;
  return mParentManager ? mParentManager->ReceiveMessage(aTarget, aTargetFrameLoader,
                                                         aTargetClosed, aMessage,
                                                         aIsSync, aCloneData,
                                                         aCpows, aPrincipal,
                                                         aRetVal) : NS_OK;
}

void
nsFrameMessageManager::AddChildManager(nsFrameMessageManager* aManager)
{
  mChildManagers.AppendObject(aManager);

  nsRefPtr<nsFrameMessageManager> kungfuDeathGrip = this;
  nsRefPtr<nsFrameMessageManager> kungfuDeathGrip2 = aManager;

  LoadPendingScripts(this, aManager);
}

void
nsFrameMessageManager::LoadPendingScripts(nsFrameMessageManager* aManager,
                                          nsFrameMessageManager* aChildMM)
{
  // We have parent manager if we're a message broadcaster.
  // In that case we want to load the pending scripts from all parent
  // message managers in the hierarchy. Process the parent first so
  // that pending scripts higher up in the hierarchy are loaded before others.
  if (aManager->mParentManager) {
    LoadPendingScripts(aManager->mParentManager, aChildMM);
  }

  for (uint32_t i = 0; i < aManager->mPendingScripts.Length(); ++i) {
    aChildMM->LoadFrameScript(aManager->mPendingScripts[i],
                              false,
                              aManager->mPendingScriptsGlobalStates[i]);
  }
}

void
nsFrameMessageManager::LoadPendingScripts()
{
  nsRefPtr<nsFrameMessageManager> kungfuDeathGrip = this;
  LoadPendingScripts(this, this);
}

void
nsFrameMessageManager::SetCallback(MessageManagerCallback* aCallback)
{
  MOZ_ASSERT(!mIsBroadcaster || !mCallback,
             "Broadcasters cannot have callbacks!");
  if (aCallback && mCallback != aCallback) {
    mCallback = aCallback;
    if (mOwnsCallback) {
      mOwnedCallback = aCallback;
    }
  }
}

void
nsFrameMessageManager::InitWithCallback(MessageManagerCallback* aCallback)
{
  if (mCallback) {
    // Initialization should only happen once.
    return;
  }

  SetCallback(aCallback);

  // First load parent scripts by adding this to parent manager.
  if (mParentManager) {
    mParentManager->AddChildManager(this);
  }

  for (uint32_t i = 0; i < mPendingScripts.Length(); ++i) {
    LoadFrameScript(mPendingScripts[i], false, mPendingScriptsGlobalStates[i]);
  }
}

void
nsFrameMessageManager::RemoveFromParent()
{
  if (mParentManager) {
    mParentManager->RemoveChildManager(this);
  }
  mParentManager = nullptr;
  mCallback = nullptr;
  mOwnedCallback = nullptr;
}

void
nsFrameMessageManager::Close()
{
  if (!mClosed) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->NotifyObservers(NS_ISUPPORTS_CAST(nsIContentFrameMessageManager*, this),
                            "message-manager-close", nullptr);
    }
  }
  mClosed = true;
  mCallback = nullptr;
  mOwnedCallback = nullptr;
}

void
nsFrameMessageManager::Disconnect(bool aRemoveFromParent)
{
  // Notify message-manager-close if we haven't already.
  Close();

  if (!mDisconnected) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
       obs->NotifyObservers(NS_ISUPPORTS_CAST(nsIContentFrameMessageManager*, this),
                            "message-manager-disconnect", nullptr);
    }
  }
  if (mParentManager && aRemoveFromParent) {
    mParentManager->RemoveChildManager(this);
  }
  mDisconnected = true;
  mParentManager = nullptr;
  if (!mHandlingMessage) {
    mListeners.Clear();
  }
}

void
nsFrameMessageManager::SetInitialProcessData(JS::HandleValue aInitialData)
{
  MOZ_ASSERT(!mChrome);
  MOZ_ASSERT(mIsProcessManager);
  mInitialProcessData = aInitialData;
}

NS_IMETHODIMP
nsFrameMessageManager::GetInitialProcessData(JSContext* aCx, JS::MutableHandleValue aResult)
{
  MOZ_ASSERT(mIsProcessManager);
  MOZ_ASSERT_IF(mChrome, IsBroadcaster());

  JS::RootedValue init(aCx, mInitialProcessData);
  if (mChrome && init.isUndefined()) {
    // We create the initial object in the junk scope. If we created it in a
    // normal compartment, that compartment would leak until shutdown.
    JS::RootedObject global(aCx, xpc::PrivilegedJunkScope());
    JSAutoCompartment ac(aCx, global);

    JS::RootedObject obj(aCx, JS_NewPlainObject(aCx));
    if (!obj) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    mInitialProcessData.setObject(*obj);
    init.setObject(*obj);
  }

  if (!mChrome && XRE_IsParentProcess()) {
    // This is the cpmm in the parent process. We should use the same object as the ppmm.
    nsCOMPtr<nsIGlobalProcessScriptLoader> ppmm =
      do_GetService("@mozilla.org/parentprocessmessagemanager;1");
    ppmm->GetInitialProcessData(aCx, &init);
    mInitialProcessData = init;
  }

  if (!JS_WrapValue(aCx, &init)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult.set(init);
  return NS_OK;
}

namespace {

struct MessageManagerReferentCount
{
  MessageManagerReferentCount() : mStrong(0), mWeakAlive(0), mWeakDead(0) {}
  size_t mStrong;
  size_t mWeakAlive;
  size_t mWeakDead;
  nsTArray<nsString> mSuspectMessages;
  nsDataHashtable<nsStringHashKey, uint32_t> mMessageCounter;
};

} // namespace

namespace mozilla {
namespace dom {

class MessageManagerReporter final : public nsIMemoryReporter
{
  ~MessageManagerReporter() {}

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  static const size_t kSuspectReferentCount = 300;
protected:
  void CountReferents(nsFrameMessageManager* aMessageManager,
                      MessageManagerReferentCount* aReferentCount);
};

NS_IMPL_ISUPPORTS(MessageManagerReporter, nsIMemoryReporter)

static PLDHashOperator
CollectMessageListenerData(const nsAString& aKey,
                           nsAutoTObserverArray<nsMessageListenerInfo, 1>* aListeners,
                           void* aData)
{
  MessageManagerReferentCount* referentCount =
    static_cast<MessageManagerReferentCount*>(aData);

  uint32_t listenerCount = aListeners->Length();
  if (!listenerCount) {
    return PL_DHASH_NEXT;
  }

  nsString key(aKey);
  uint32_t oldCount = 0;
  referentCount->mMessageCounter.Get(key, &oldCount);
  uint32_t currentCount = oldCount + listenerCount;
  referentCount->mMessageCounter.Put(key, currentCount);

  // Keep track of messages that have a suspiciously large
  // number of referents (symptom of leak).
  if (currentCount == MessageManagerReporter::kSuspectReferentCount) {
    referentCount->mSuspectMessages.AppendElement(key);
  }

  for (uint32_t i = 0; i < listenerCount; ++i) {
    const nsMessageListenerInfo& listenerInfo =
      aListeners->ElementAt(i);
    if (listenerInfo.mWeakListener) {
      nsCOMPtr<nsISupports> referent =
        do_QueryReferent(listenerInfo.mWeakListener);
      if (referent) {
        referentCount->mWeakAlive++;
      } else {
        referentCount->mWeakDead++;
      }
    } else {
      referentCount->mStrong++;
    }
  }
  return PL_DHASH_NEXT;
}

void
MessageManagerReporter::CountReferents(nsFrameMessageManager* aMessageManager,
                                       MessageManagerReferentCount* aReferentCount)
{
  aMessageManager->mListeners.EnumerateRead(CollectMessageListenerData,
                                            aReferentCount);

  // Add referent count in child managers because the listeners
  // participate in messages dispatched from parent message manager.
  for (uint32_t i = 0; i < aMessageManager->mChildManagers.Length(); ++i) {
    nsRefPtr<nsFrameMessageManager> mm =
      static_cast<nsFrameMessageManager*>(aMessageManager->mChildManagers[i]);
    CountReferents(mm, aReferentCount);
  }
}

static nsresult
ReportReferentCount(const char* aManagerType,
                    const MessageManagerReferentCount& aReferentCount,
                    nsIMemoryReporterCallback* aCb,
                    nsISupports* aClosure)
{
#define REPORT(_path, _amount, _desc)                                         \
    do {                                                                      \
      nsresult rv;                                                            \
      rv = aCb->Callback(EmptyCString(), _path,                               \
                         nsIMemoryReporter::KIND_OTHER,                       \
                         nsIMemoryReporter::UNITS_COUNT, _amount,             \
                         _desc, aClosure);                                    \
      NS_ENSURE_SUCCESS(rv, rv);                                              \
    } while (0)

  REPORT(nsPrintfCString("message-manager/referent/%s/strong", aManagerType),
         aReferentCount.mStrong,
         nsPrintfCString("The number of strong referents held by the message "
                         "manager in the %s manager.", aManagerType));
  REPORT(nsPrintfCString("message-manager/referent/%s/weak/alive", aManagerType),
         aReferentCount.mWeakAlive,
         nsPrintfCString("The number of weak referents that are still alive "
                         "held by the message manager in the %s manager.",
                         aManagerType));
  REPORT(nsPrintfCString("message-manager/referent/%s/weak/dead", aManagerType),
         aReferentCount.mWeakDead,
         nsPrintfCString("The number of weak referents that are dead "
                         "held by the message manager in the %s manager.",
                         aManagerType));

  for (uint32_t i = 0; i < aReferentCount.mSuspectMessages.Length(); i++) {
    uint32_t totalReferentCount = 0;
    aReferentCount.mMessageCounter.Get(aReferentCount.mSuspectMessages[i],
                                       &totalReferentCount);
    NS_ConvertUTF16toUTF8 suspect(aReferentCount.mSuspectMessages[i]);
    REPORT(nsPrintfCString("message-manager-suspect/%s/referent(message=%s)",
                           aManagerType, suspect.get()), totalReferentCount,
           nsPrintfCString("A message in the %s message manager with a "
                           "suspiciously large number of referents (symptom "
                           "of a leak).", aManagerType));
  }

#undef REPORT

  return NS_OK;
}

NS_IMETHODIMP
MessageManagerReporter::CollectReports(nsIMemoryReporterCallback* aCb,
                                       nsISupports* aClosure, bool aAnonymize)
{
  nsresult rv;

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIMessageBroadcaster> globalmm =
      do_GetService("@mozilla.org/globalmessagemanager;1");
    if (globalmm) {
      nsRefPtr<nsFrameMessageManager> mm =
        static_cast<nsFrameMessageManager*>(globalmm.get());
      MessageManagerReferentCount count;
      CountReferents(mm, &count);
      rv = ReportReferentCount("global-manager", count, aCb, aClosure);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (nsFrameMessageManager::sParentProcessManager) {
    MessageManagerReferentCount count;
    CountReferents(nsFrameMessageManager::sParentProcessManager, &count);
    rv = ReportReferentCount("parent-process-manager", count, aCb, aClosure);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (nsFrameMessageManager::sChildProcessManager) {
    MessageManagerReferentCount count;
    CountReferents(nsFrameMessageManager::sChildProcessManager, &count);
    rv = ReportReferentCount("child-process-manager", count, aCb, aClosure);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla

nsresult
NS_NewGlobalMessageManager(nsIMessageBroadcaster** aResult)
{
  NS_ENSURE_TRUE(XRE_IsParentProcess(),
                 NS_ERROR_NOT_AVAILABLE);
  nsRefPtr<nsFrameMessageManager> mm = new nsFrameMessageManager(nullptr,
                                                                 nullptr,
                                                                 MM_CHROME | MM_GLOBAL | MM_BROADCASTER);
  RegisterStrongMemoryReporter(new MessageManagerReporter());
  mm.forget(aResult);
  return NS_OK;
}

nsDataHashtable<nsStringHashKey, nsMessageManagerScriptHolder*>*
  nsMessageManagerScriptExecutor::sCachedScripts = nullptr;
nsScriptCacheCleaner* nsMessageManagerScriptExecutor::sScriptCacheCleaner = nullptr;

void
nsMessageManagerScriptExecutor::DidCreateGlobal()
{
  NS_ASSERTION(mGlobal, "Should have mGlobal!");
  if (!sCachedScripts) {
    sCachedScripts =
      new nsDataHashtable<nsStringHashKey, nsMessageManagerScriptHolder*>;

    nsRefPtr<nsScriptCacheCleaner> scriptCacheCleaner =
      new nsScriptCacheCleaner();
    scriptCacheCleaner.forget(&sScriptCacheCleaner);
  }
}

static PLDHashOperator
RemoveCachedScriptEntry(const nsAString& aKey,
                        nsMessageManagerScriptHolder*& aData,
                        void* aUserArg)
{
  delete aData;
  return PL_DHASH_REMOVE;
}

// static
void
nsMessageManagerScriptExecutor::Shutdown()
{
  if (sCachedScripts) {
    AutoSafeJSContext cx;
    NS_ASSERTION(sCachedScripts != nullptr, "Need cached scripts");
    sCachedScripts->Enumerate(RemoveCachedScriptEntry, nullptr);

    delete sCachedScripts;
    sCachedScripts = nullptr;

    nsRefPtr<nsScriptCacheCleaner> scriptCacheCleaner;
    scriptCacheCleaner.swap(sScriptCacheCleaner);
  }
}

void
nsMessageManagerScriptExecutor::LoadScriptInternal(const nsAString& aURL,
                                                   bool aRunInGlobalScope)
{
  if (!mGlobal || !sCachedScripts) {
    return;
  }

  JSRuntime* rt = CycleCollectedJSRuntime::Get()->Runtime();
  JS::Rooted<JSScript*> script(rt);

  nsMessageManagerScriptHolder* holder = sCachedScripts->Get(aURL);
  if (holder && holder->WillRunInGlobalScope() == aRunInGlobalScope) {
    script = holder->mScript;
  } else {
    // Don't put anything in the cache if we already have an entry
    // with a different WillRunInGlobalScope() value.
    bool shouldCache = !holder;
    TryCacheLoadAndCompileScript(aURL, aRunInGlobalScope,
                                 shouldCache, &script);
  }

  JS::Rooted<JSObject*> global(rt, mGlobal->GetJSObject());
  if (global) {
    AutoEntryScript aes(xpc::NativeGlobal(global),
                        "message manager script load");
    aes.TakeOwnershipOfErrorReporting();
    JSContext* cx = aes.cx();
    if (script) {
      if (aRunInGlobalScope) {
        JS::CloneAndExecuteScript(cx, script);
      } else {
        JS::Rooted<JSObject*> scope(cx);
        bool ok = js::ExecuteInGlobalAndReturnScope(cx, global, script, &scope);
        if (ok) {
          // Force the scope to stay alive.
          mAnonymousGlobalScopes.AppendElement(scope);
        }
      }
    }
  }
}

void
nsMessageManagerScriptExecutor::TryCacheLoadAndCompileScript(
  const nsAString& aURL,
  bool aRunInGlobalScope,
  bool aShouldCache,
  JS::MutableHandle<JSScript*> aScriptp)
{
  nsCString url = NS_ConvertUTF16toUTF8(aURL);
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), url);
  if (NS_FAILED(rv)) {
    return;
  }

  bool hasFlags;
  rv = NS_URIChainHasFlags(uri,
                           nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                           &hasFlags);
  if (NS_FAILED(rv) || !hasFlags) {
    NS_WARNING("Will not load a frame script!");
    return;
  }

  nsCOMPtr<nsIChannel> channel;
  NS_NewChannel(getter_AddRefs(channel),
                uri,
                nsContentUtils::GetSystemPrincipal(),
                nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                nsIContentPolicy::TYPE_OTHER);

  if (!channel) {
    return;
  }

  nsCOMPtr<nsIInputStream> input;
  rv = channel->Open2(getter_AddRefs(input));
  NS_ENSURE_SUCCESS_VOID(rv);
  nsString dataString;
  char16_t* dataStringBuf = nullptr;
  size_t dataStringLength = 0;
  uint64_t avail64 = 0;
  if (input && NS_SUCCEEDED(input->Available(&avail64)) && avail64) {
    if (avail64 > UINT32_MAX) {
      return;
    }
    nsCString buffer;
    uint32_t avail = (uint32_t)std::min(avail64, (uint64_t)UINT32_MAX);
    if (NS_FAILED(NS_ReadInputStreamToString(input, buffer, avail))) {
      return;
    }
    nsScriptLoader::ConvertToUTF16(channel, (uint8_t*)buffer.get(), avail,
                                   EmptyString(), nullptr,
                                   dataStringBuf, dataStringLength);
  }

  JS::SourceBufferHolder srcBuf(dataStringBuf, dataStringLength,
                                JS::SourceBufferHolder::GiveOwnership);

  if (dataStringBuf && dataStringLength > 0) {
    AutoSafeJSContext cx;
    // Compile the script in the compilation scope instead of the current global
    // to avoid keeping the current compartment alive.
    JS::Rooted<JSObject*> global(cx, xpc::CompilationScope());

    JSAutoCompartment ac(cx, global);
    JS::CompileOptions options(cx, JSVERSION_LATEST);
    options.setFileAndLine(url.get(), 1);
    options.setNoScriptRval(true);
    JS::Rooted<JSScript*> script(cx);

    if (aRunInGlobalScope) {
      if (!JS::Compile(cx, options, srcBuf, &script)) {
        return;
      }
    // We're going to run these against some non-global scope.
    } else if (!JS::CompileForNonSyntacticScope(cx, options, srcBuf, &script)) {
      return;
    }

    MOZ_ASSERT(script);
    aScriptp.set(script);

    nsAutoCString scheme;
    uri->GetScheme(scheme);
    // We don't cache data: scripts!
    if (aShouldCache && !scheme.EqualsLiteral("data")) {
      // Root the object also for caching.
      nsMessageManagerScriptHolder* holder =
        new nsMessageManagerScriptHolder(cx, script, aRunInGlobalScope);
      sCachedScripts->Put(aURL, holder);
    }
  }
}

void
nsMessageManagerScriptExecutor::TryCacheLoadAndCompileScript(
  const nsAString& aURL,
  bool aRunInGlobalScope)
{
  AutoSafeJSContext cx;
  JS::Rooted<JSScript*> script(cx);
  TryCacheLoadAndCompileScript(aURL, aRunInGlobalScope, true, &script);
}

bool
nsMessageManagerScriptExecutor::InitChildGlobalInternal(
  nsISupports* aScope,
  const nsACString& aID)
{
  AutoSafeJSContext cx;
  nsContentUtils::GetSecurityManager()->GetSystemPrincipal(getter_AddRefs(mPrincipal));

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  const uint32_t flags = nsIXPConnect::INIT_JS_STANDARD_CLASSES;

  JS::CompartmentOptions options;
  options.setZone(JS::SystemZone)
         .setVersion(JSVERSION_LATEST);

  nsresult rv =
    xpc->InitClassesWithNewWrappedGlobal(cx, aScope, mPrincipal,
                                         flags, options, getter_AddRefs(mGlobal));
  NS_ENSURE_SUCCESS(rv, false);


  JS::Rooted<JSObject*> global(cx, mGlobal->GetJSObject());
  NS_ENSURE_TRUE(global, false);

  // Set the location information for the new global, so that tools like
  // about:memory may use that information.
  xpc::SetLocationForGlobal(global, aID);

  DidCreateGlobal();
  return true;
}

void
nsMessageManagerScriptExecutor::MarkScopesForCC()
{
  for (uint32_t i = 0; i < mAnonymousGlobalScopes.Length(); ++i) {
    JSObject* obj = mAnonymousGlobalScopes[i];
    if (obj) {
      JS::ExposeObjectToActiveJS(obj);
    }
  }
}

NS_IMPL_ISUPPORTS(nsScriptCacheCleaner, nsIObserver)

nsFrameMessageManager* nsFrameMessageManager::sChildProcessManager = nullptr;
nsFrameMessageManager* nsFrameMessageManager::sParentProcessManager = nullptr;
nsFrameMessageManager* nsFrameMessageManager::sSameProcessParentManager = nullptr;

class nsAsyncMessageToSameProcessChild : public nsSameProcessAsyncMessageBase,
                                         public nsRunnable
{
public:
  nsAsyncMessageToSameProcessChild(JSContext* aCx, JS::Handle<JSObject*> aCpows)
    : nsSameProcessAsyncMessageBase(aCx, aCpows)
  { }
  NS_IMETHOD Run()
  {
    nsFrameMessageManager* ppm = nsFrameMessageManager::GetChildProcessManager();
    ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm), nullptr, ppm);
    return NS_OK;
  }
};


/**
 * Send messages to an imaginary child process in a single-process scenario.
 */
class SameParentProcessMessageManagerCallback : public MessageManagerCallback
{
public:
  SameParentProcessMessageManagerCallback()
  {
    MOZ_COUNT_CTOR(SameParentProcessMessageManagerCallback);
  }
  virtual ~SameParentProcessMessageManagerCallback()
  {
    MOZ_COUNT_DTOR(SameParentProcessMessageManagerCallback);
  }

  virtual bool DoLoadMessageManagerScript(const nsAString& aURL,
                                          bool aRunInGlobalScope) override
  {
    ProcessGlobal* global = ProcessGlobal::Get();
    MOZ_ASSERT(!aRunInGlobalScope);
    global->LoadScript(aURL);
    return true;
  }

  virtual nsresult DoSendAsyncMessage(JSContext* aCx,
                                      const nsAString& aMessage,
                                      StructuredCloneData& aData,
                                      JS::Handle<JSObject *> aCpows,
                                      nsIPrincipal* aPrincipal) override
  {
    nsRefPtr<nsAsyncMessageToSameProcessChild> ev =
      new nsAsyncMessageToSameProcessChild(aCx, aCpows);

    nsresult rv = ev->Init(aCx, aMessage, aData, aPrincipal);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = NS_DispatchToCurrentThread(ev);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  bool CheckPermission(const nsAString& aPermission) override
  {
    // In a single-process scenario, the child always has all capabilities.
    return true;
  }

  bool CheckManifestURL(const nsAString& aManifestURL) override
  {
    // In a single-process scenario, the child always has all capabilities.
    return true;
  }

  bool CheckAppHasPermission(const nsAString& aPermission) override
  {
    // In a single-process scenario, the child always has all capabilities.
    return true;
  }

  virtual bool CheckAppHasStatus(unsigned short aStatus) override
  {
    // In a single-process scenario, the child always has all capabilities.
    return true;
  }
};


/**
 * Send messages to the parent process.
 */
class ChildProcessMessageManagerCallback : public MessageManagerCallback
{
public:
  ChildProcessMessageManagerCallback()
  {
    MOZ_COUNT_CTOR(ChildProcessMessageManagerCallback);
  }
  virtual ~ChildProcessMessageManagerCallback()
  {
    MOZ_COUNT_DTOR(ChildProcessMessageManagerCallback);
  }

  virtual bool DoSendBlockingMessage(JSContext* aCx,
                                     const nsAString& aMessage,
                                     StructuredCloneData& aData,
                                     JS::Handle<JSObject *> aCpows,
                                     nsIPrincipal* aPrincipal,
                                     nsTArray<StructuredCloneData>* aRetVal,
                                     bool aIsSync) override
  {
    mozilla::dom::ContentChild* cc =
      mozilla::dom::ContentChild::GetSingleton();
    if (!cc) {
      return true;
    }
    ClonedMessageData data;
    if (!BuildClonedMessageDataForChild(cc, aData, data)) {
      return false;
    }
    InfallibleTArray<mozilla::jsipc::CpowEntry> cpows;
    if (aCpows && !cc->GetCPOWManager()->Wrap(aCx, aCpows, &cpows)) {
      return false;
    }
    if (aIsSync) {
      return cc->SendSyncMessage(PromiseFlatString(aMessage), data, cpows,
                                 IPC::Principal(aPrincipal), aRetVal);
    }
    return cc->SendRpcMessage(PromiseFlatString(aMessage), data, cpows,
                              IPC::Principal(aPrincipal), aRetVal);
  }

  virtual nsresult DoSendAsyncMessage(JSContext* aCx,
                                      const nsAString& aMessage,
                                      StructuredCloneData& aData,
                                      JS::Handle<JSObject *> aCpows,
                                      nsIPrincipal* aPrincipal) override
  {
    mozilla::dom::ContentChild* cc =
      mozilla::dom::ContentChild::GetSingleton();
    if (!cc) {
      return NS_OK;
    }
    ClonedMessageData data;
    if (!BuildClonedMessageDataForChild(cc, aData, data)) {
      return NS_ERROR_DOM_DATA_CLONE_ERR;
    }
    InfallibleTArray<mozilla::jsipc::CpowEntry> cpows;
    if (aCpows && !cc->GetCPOWManager()->Wrap(aCx, aCpows, &cpows)) {
      return NS_ERROR_UNEXPECTED;
    }
    if (!cc->SendAsyncMessage(PromiseFlatString(aMessage), data, cpows,
                              IPC::Principal(aPrincipal))) {
      return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
  }
};


class nsAsyncMessageToSameProcessParent : public nsSameProcessAsyncMessageBase,
                                          public SameProcessMessageQueue::Runnable
{
public:
  nsAsyncMessageToSameProcessParent(JSContext* aCx, JS::Handle<JSObject*> aCpows)
    : nsSameProcessAsyncMessageBase(aCx, aCpows)
  { }
  virtual nsresult HandleMessage() override
  {
    nsFrameMessageManager* ppm = nsFrameMessageManager::sSameProcessParentManager;
    ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm), nullptr, ppm);
    return NS_OK;
  }
};

/**
 * Send messages to the imaginary parent process in a single-process scenario.
 */
class SameChildProcessMessageManagerCallback : public MessageManagerCallback
{
public:
  SameChildProcessMessageManagerCallback()
  {
    MOZ_COUNT_CTOR(SameChildProcessMessageManagerCallback);
  }
  virtual ~SameChildProcessMessageManagerCallback()
  {
    MOZ_COUNT_DTOR(SameChildProcessMessageManagerCallback);
  }

  virtual bool DoSendBlockingMessage(JSContext* aCx,
                                     const nsAString& aMessage,
                                     StructuredCloneData& aData,
                                     JS::Handle<JSObject *> aCpows,
                                     nsIPrincipal* aPrincipal,
                                     nsTArray<StructuredCloneData>* aRetVal,
                                     bool aIsSync) override
  {
    SameProcessMessageQueue* queue = SameProcessMessageQueue::Get();
    queue->Flush();

    if (nsFrameMessageManager::sSameProcessParentManager) {
      SameProcessCpowHolder cpows(js::GetRuntime(aCx), aCpows);
      nsRefPtr<nsFrameMessageManager> ppm = nsFrameMessageManager::sSameProcessParentManager;
      ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()), nullptr, aMessage,
                          true, &aData, &cpows, aPrincipal, aRetVal);
    }
    return true;
  }

  virtual nsresult DoSendAsyncMessage(JSContext* aCx,
                                  const nsAString& aMessage,
                                  StructuredCloneData& aData,
                                  JS::Handle<JSObject *> aCpows,
                                  nsIPrincipal* aPrincipal) override
  {
    SameProcessMessageQueue* queue = SameProcessMessageQueue::Get();
    nsRefPtr<nsAsyncMessageToSameProcessParent> ev =
      new nsAsyncMessageToSameProcessParent(aCx, aCpows);
    nsresult rv = ev->Init(aCx, aMessage, aData, aPrincipal);

    if (NS_FAILED(rv)) {
      return rv;
    }
    queue->Push(ev);
    return NS_OK;
  }

};


// This creates the global parent process message manager.
nsresult
NS_NewParentProcessMessageManager(nsIMessageBroadcaster** aResult)
{
  NS_ASSERTION(!nsFrameMessageManager::sParentProcessManager,
               "Re-creating sParentProcessManager");
  nsRefPtr<nsFrameMessageManager> mm = new nsFrameMessageManager(nullptr,
                                                                 nullptr,
                                                                 MM_CHROME | MM_PROCESSMANAGER | MM_BROADCASTER);
  nsFrameMessageManager::sParentProcessManager = mm;
  nsFrameMessageManager::NewProcessMessageManager(false); // Create same process message manager.
  mm.forget(aResult);
  return NS_OK;
}


nsFrameMessageManager*
nsFrameMessageManager::NewProcessMessageManager(bool aIsRemote)
{
  if (!nsFrameMessageManager::sParentProcessManager) {
     nsCOMPtr<nsIMessageBroadcaster> dummy =
       do_GetService("@mozilla.org/parentprocessmessagemanager;1");
  }

  MOZ_ASSERT(nsFrameMessageManager::sParentProcessManager,
             "parent process manager not created");
  nsFrameMessageManager* mm;
  if (aIsRemote) {
    // Callback is set in ContentParent::InitInternal so that the process has
    // already started when we send pending scripts.
    mm = new nsFrameMessageManager(nullptr,
                                   nsFrameMessageManager::sParentProcessManager,
                                   MM_CHROME | MM_PROCESSMANAGER);
  } else {
    mm = new nsFrameMessageManager(new SameParentProcessMessageManagerCallback(),
                                   nsFrameMessageManager::sParentProcessManager,
                                   MM_CHROME | MM_PROCESSMANAGER | MM_OWNSCALLBACK);
    sSameProcessParentManager = mm;
  }
  return mm;
}

nsresult
NS_NewChildProcessMessageManager(nsISyncMessageSender** aResult)
{
  NS_ASSERTION(!nsFrameMessageManager::GetChildProcessManager(),
               "Re-creating sChildProcessManager");

  MessageManagerCallback* cb;
  if (XRE_IsParentProcess()) {
    cb = new SameChildProcessMessageManagerCallback();
  } else {
    cb = new ChildProcessMessageManagerCallback();
    RegisterStrongMemoryReporter(new MessageManagerReporter());
  }
  nsFrameMessageManager* mm = new nsFrameMessageManager(cb,
                                                        nullptr,
                                                        MM_PROCESSMANAGER | MM_OWNSCALLBACK);
  nsFrameMessageManager::SetChildProcessManager(mm);
  nsRefPtr<ProcessGlobal> global = new ProcessGlobal(mm);
  NS_ENSURE_TRUE(global->Init(), NS_ERROR_UNEXPECTED);
  global.forget(aResult);
  return NS_OK;
}

static PLDHashOperator
CycleCollectorMarkListeners(const nsAString& aKey,
                            nsAutoTObserverArray<nsMessageListenerInfo, 1>* aListeners,
                            void* aData)
{
  uint32_t count = aListeners->Length();
  for (uint32_t i = 0; i < count; i++) {
    if (aListeners->ElementAt(i).mStrongListener) {
      xpc_TryUnmarkWrappedGrayObject(aListeners->ElementAt(i).mStrongListener);
    }
  }
  return PL_DHASH_NEXT;
}

bool
nsFrameMessageManager::MarkForCC()
{
  mListeners.EnumerateRead(CycleCollectorMarkListeners, nullptr);

  if (mRefCnt.IsPurple()) {
    mRefCnt.RemovePurple();
  }
  return true;
}

nsSameProcessAsyncMessageBase::nsSameProcessAsyncMessageBase(JSContext* aCx, JS::Handle<JSObject*> aCpows)
  : mRuntime(nullptr)
  , mCpows(aCx, aCpows)
{ }


nsresult
nsSameProcessAsyncMessageBase::Init(JSContext* aCx,
                                    const nsAString& aMessage,
                                    StructuredCloneData& aData,
                                    nsIPrincipal* aPrincipal)
{
  if (!mData.Copy(aData)) {
    Telemetry::Accumulate(Telemetry::IPC_SAME_PROCESS_MESSAGE_COPY_OOM_KB, aData.DataLength());
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mRuntime = js::GetRuntime(aCx);
  mMessage = aMessage;
  mPrincipal = aPrincipal;

  return NS_OK;
}

void
nsSameProcessAsyncMessageBase::ReceiveMessage(nsISupports* aTarget,
                                              nsIFrameLoader* aTargetFrameLoader,
                                              nsFrameMessageManager* aManager)
{
  // Make sure that we have called Init() and it has succeeded.
  MOZ_ASSERT(mRuntime);
  if (aManager) {
    SameProcessCpowHolder cpows(mRuntime, mCpows);

    nsRefPtr<nsFrameMessageManager> mm = aManager;
    mm->ReceiveMessage(aTarget, aTargetFrameLoader, mMessage, false, &mData,
                       &cpows, mPrincipal, nullptr);
  }
}
