/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "nsFrameMessageManager.h"

#include "ContentChild.h"
#include "GeckoProfiler.h"
#include "nsASCIIMask.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsJSUtils.h"
#include "nsJSPrincipals.h"
#include "nsNetUtil.h"
#include "mozilla/dom/ScriptLoader.h"
#include "nsFrameLoader.h"
#include "nsIInputStream.h"
#include "nsIXULRuntime.h"
#include "nsIScriptError.h"
#include "nsIConsoleService.h"
#include "nsIMemoryReporter.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptSecurityManager.h"
#include "xpcpublic.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/Preferences.h"
#include "mozilla/ScriptPreloader.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/ChildProcessMessageManager.h"
#include "mozilla/dom/ChromeMessageBroadcaster.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MessageManagerBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ParentProcessMessageManager.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/ProcessGlobal.h"
#include "mozilla/dom/ProcessMessageManager.h"
#include "mozilla/dom/ResolveSystemBinding.h"
#include "mozilla/dom/SameProcessMessageQueue.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "nsPrintfCString.h"
#include "nsXULAppAPI.h"
#include "nsQueryObject.h"
#include "xpcprivate.h"
#include <algorithm>
#include "chrome/common/ipc_channel.h" // for IPC::Channel::kMaximumMessageSize

#ifdef XP_WIN
# if defined(SendMessage)
#  undef SendMessage
# endif
#endif

#ifdef FUZZING
#include "MessageManagerFuzzer.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;

nsFrameMessageManager::nsFrameMessageManager(MessageManagerCallback* aCallback,
                                             MessageManagerFlags aFlags)
 : mChrome(aFlags & MessageManagerFlags::MM_CHROME),
   mGlobal(aFlags & MessageManagerFlags::MM_GLOBAL),
   mIsProcessManager(aFlags & MessageManagerFlags::MM_PROCESSMANAGER),
   mIsBroadcaster(aFlags & MessageManagerFlags::MM_BROADCASTER),
   mOwnsCallback(aFlags & MessageManagerFlags::MM_OWNSCALLBACK),
   mHandlingMessage(false),
   mClosed(false),
   mDisconnected(false),
   mCallback(aCallback)
{
  NS_ASSERTION(!mIsBroadcaster || !mCallback,
               "Broadcasters cannot have callbacks!");
  if (mOwnsCallback) {
    mOwnedCallback = aCallback;
  }
}

nsFrameMessageManager::~nsFrameMessageManager()
{
  for (int32_t i = mChildManagers.Length(); i > 0; --i) {
    mChildManagers[i - 1]->Disconnect(false);
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

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsMessageListenerInfo& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  ImplCycleCollectionTraverse(aCallback, aField.mStrongListener, aName, aFlags);
  ImplCycleCollectionTraverse(aCallback, aField.mWeakListener, aName, aFlags);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsFrameMessageManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsFrameMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListeners)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChildManagers)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsFrameMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mInitialProcessData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsFrameMessageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mListeners)
  for (int32_t i = tmp->mChildManagers.Length(); i > 0; --i) {
    tmp->mChildManagers[i - 1]->Disconnect(false);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChildManagers)
  tmp->mInitialProcessData.setNull();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentFrameMessageManager)

  /* Message managers in child process implement nsIMessageSender.
     Message managers in the chrome process are
     either broadcasters (if they have subordinate/child message
     managers) or they're simple message senders. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIMessageSender, !mChrome || !mIsBroadcaster)

  /* nsIContentFrameMessageManager is accessible only in TabChildGlobal. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIContentFrameMessageManager,
                                     !mChrome && !mIsProcessManager)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFrameMessageManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFrameMessageManager)

void
MessageManagerCallback::DoGetRemoteType(nsAString& aRemoteType,
                                        ErrorResult& aError) const
{
  aRemoteType.Truncate();
  mozilla::dom::ProcessMessageManager* parent = GetProcessMessageManager();
  if (!parent) {
    return;
  }

  parent->GetRemoteType(aRemoteType, aError);
}

bool
MessageManagerCallback::BuildClonedMessageDataForParent(nsIContentParent* aParent,
                                                        StructuredCloneData& aData,
                                                        ClonedMessageData& aClonedData)
{
  return aData.BuildClonedMessageDataForParent(aParent, aClonedData);
}

bool
MessageManagerCallback::BuildClonedMessageDataForChild(nsIContentChild* aChild,
                                                       StructuredCloneData& aData,
                                                       ClonedMessageData& aClonedData)
{
  return aData.BuildClonedMessageDataForChild(aChild, aClonedData);
}

void
mozilla::dom::ipc::UnpackClonedMessageDataForParent(const ClonedMessageData& aClonedData,
                                                    StructuredCloneData& aData)
{
  aData.BorrowFromClonedMessageDataForParent(aClonedData);
}

void
mozilla::dom::ipc::UnpackClonedMessageDataForChild(const ClonedMessageData& aClonedData,
                                                   StructuredCloneData& aData)
{
  aData.BorrowFromClonedMessageDataForChild(aClonedData);
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

void
nsFrameMessageManager::AddMessageListener(const nsAString& aMessageName,
                                          MessageListener& aListener,
                                          bool aListenWhenClosed,
                                          ErrorResult& aError)
{
  auto listeners = mListeners.LookupForAdd(aMessageName).OrInsert([]() {
      return new nsAutoTObserverArray<nsMessageListenerInfo, 1>();
    });
  uint32_t len = listeners->Length();
  for (uint32_t i = 0; i < len; ++i) {
    MessageListener* strongListener = listeners->ElementAt(i).mStrongListener;
    if (strongListener && *strongListener == aListener) {
      return;
    }
  }

  nsMessageListenerInfo* entry = listeners->AppendElement();
  entry->mStrongListener = &aListener;
  entry->mListenWhenClosed = aListenWhenClosed;
}

void
nsFrameMessageManager::RemoveMessageListener(const nsAString& aMessageName,
                                             MessageListener& aListener,
                                             ErrorResult& aError)
{
  nsAutoTObserverArray<nsMessageListenerInfo, 1>* listeners =
    mListeners.Get(aMessageName);
  if (listeners) {
    uint32_t len = listeners->Length();
    for (uint32_t i = 0; i < len; ++i) {
      MessageListener* strongListener = listeners->ElementAt(i).mStrongListener;
      if (strongListener && *strongListener == aListener) {
        listeners->RemoveElementAt(i);
        return;
      }
    }
  }
}

static already_AddRefed<nsISupports>
ToXPCOMMessageListener(MessageListener& aListener)
{
  return CallbackObjectHolder<mozilla::dom::MessageListener,
                              nsISupports>(&aListener).ToXPCOMCallback();
}

void
nsFrameMessageManager::AddWeakMessageListener(const nsAString& aMessageName,
                                              MessageListener& aListener,
                                              ErrorResult& aError)
{
  nsCOMPtr<nsISupports> listener(ToXPCOMMessageListener(aListener));
  nsWeakPtr weak = do_GetWeakReference(listener);
  if (!weak) {
    aError.Throw(NS_ERROR_NO_INTERFACE);
    return;
  }

#ifdef DEBUG
  // It's technically possible that one object X could give two different
  // nsIWeakReference*'s when you do_GetWeakReference(X).  We really don't want
  // this to happen; it will break e.g. RemoveWeakMessageListener.  So let's
  // check that we're not getting ourselves into that situation.
  nsCOMPtr<nsISupports> canonical = do_QueryInterface(listener);
  for (auto iter = mListeners.Iter(); !iter.Done(); iter.Next()) {
    nsAutoTObserverArray<nsMessageListenerInfo, 1>* listeners = iter.UserData();
    uint32_t count = listeners->Length();
    for (uint32_t i = 0; i < count; i++) {
      nsWeakPtr weakListener = listeners->ElementAt(i).mWeakListener;
      if (weakListener) {
        nsCOMPtr<nsISupports> otherCanonical = do_QueryReferent(weakListener);
        MOZ_ASSERT((canonical == otherCanonical) == (weak == weakListener));
      }
    }
  }
#endif

  auto listeners = mListeners.LookupForAdd(aMessageName).OrInsert([]() {
      return new nsAutoTObserverArray<nsMessageListenerInfo, 1>();
    });
  uint32_t len = listeners->Length();
  for (uint32_t i = 0; i < len; ++i) {
    if (listeners->ElementAt(i).mWeakListener == weak) {
      return;
    }
  }

  nsMessageListenerInfo* entry = listeners->AppendElement();
  entry->mWeakListener = weak;
  entry->mListenWhenClosed = false;
}

void
nsFrameMessageManager::RemoveWeakMessageListener(const nsAString& aMessageName,
                                                 MessageListener& aListener,
                                                 ErrorResult& aError)
{
  nsCOMPtr<nsISupports> listener(ToXPCOMMessageListener(aListener));
  nsWeakPtr weak = do_GetWeakReference(listener);
  if (!weak) {
    aError.Throw(NS_ERROR_NO_INTERFACE);
    return;
  }

  nsAutoTObserverArray<nsMessageListenerInfo, 1>* listeners =
    mListeners.Get(aMessageName);
  if (!listeners) {
    return;
  }

  uint32_t len = listeners->Length();
  for (uint32_t i = 0; i < len; ++i) {
    if (listeners->ElementAt(i).mWeakListener == weak) {
      listeners->RemoveElementAt(i);
      return;
    }
  }
}

void
nsFrameMessageManager::LoadScript(const nsAString& aURL,
                                  bool aAllowDelayedLoad,
                                  bool aRunInGlobalScope,
                                  ErrorResult& aError)
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
    if (!mCallback->DoLoadMessageManagerScript(aURL, aRunInGlobalScope)) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }
  }

  for (uint32_t i = 0; i < mChildManagers.Length(); ++i) {
    RefPtr<nsFrameMessageManager> mm = mChildManagers[i];
    if (mm) {
      // Use false here, so that child managers don't cache the script, which
      // is already cached in the parent.
      mm->LoadScript(aURL, false, aRunInGlobalScope, IgnoreErrors());
    }
  }
}

void
nsFrameMessageManager::RemoveDelayedScript(const nsAString& aURL)
{
  for (uint32_t i = 0; i < mPendingScripts.Length(); ++i) {
    if (mPendingScripts[i] == aURL) {
      mPendingScripts.RemoveElementAt(i);
      mPendingScriptsGlobalStates.RemoveElementAt(i);
      break;
    }
  }
}

void
nsFrameMessageManager::GetDelayedScripts(JSContext* aCx,
                                         nsTArray<nsTArray<JS::Value>>& aList,
                                         ErrorResult& aError)
{
  // Frame message managers may return an incomplete list because scripts
  // that were loaded after it was connected are not added to the list.
  if (!IsGlobal() && !IsBroadcaster()) {
    NS_WARNING("Cannot retrieve list of pending frame scripts for frame"
               "message managers as it may be incomplete");
    aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return;
  }

  aError.MightThrowJSException();

  aList.SetCapacity(mPendingScripts.Length());
  for (uint32_t i = 0; i < mPendingScripts.Length(); ++i) {
    JS::Rooted<JS::Value> url(aCx);
    if (!ToJSValue(aCx, mPendingScripts[i], &url)) {
      aError.NoteJSContextException(aCx);
      return;
    }

    nsTArray<JS::Value>* array = aList.AppendElement(2);
    array->AppendElement(url);
    array->AppendElement(JS::BooleanValue(mPendingScriptsGlobalStates[i]));
  }
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
                    const JS::Value& aTransfer,
                    StructuredCloneData& aData)
{
  // First try to use structured clone on the whole thing.
  JS::RootedValue v(aCx, aValue);
  JS::RootedValue t(aCx, aTransfer);
  ErrorResult rv;
  aData.Write(aCx, v, t, rv);
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
                nsIScriptError::warningFlag, "chrome javascript",
                false /* from private window */);
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


static bool sSendingSyncMessage = false;

static bool
AllowMessage(size_t aDataLength, const nsAString& aMessageName)
{
  // A message includes more than structured clone data, so subtract
  // 20KB to make it more likely that a message within this bound won't
  // result in an overly large IPC message.
  static const size_t kMaxMessageSize = IPC::Channel::kMaximumMessageSize - 20 * 1024;
  if (aDataLength < kMaxMessageSize) {
    return true;
  }

  NS_ConvertUTF16toUTF8 messageName(aMessageName);
  messageName.StripTaggedASCII(ASCIIMask::Mask0to9());

  Telemetry::Accumulate(Telemetry::REJECTED_MESSAGE_MANAGER_MESSAGE,
                        messageName);

  return false;
}

void
nsFrameMessageManager::SendMessage(JSContext* aCx,
                                   const nsAString& aMessageName,
                                   JS::Handle<JS::Value> aObj,
                                   JS::Handle<JSObject*> aObjects,
                                   nsIPrincipal* aPrincipal,
                                   bool aIsSync,
                                   nsTArray<JS::Value>& aResult,
                                   ErrorResult& aError)
{
  NS_ASSERTION(!IsGlobal(), "Should not call SendSyncMessage in chrome");
  NS_ASSERTION(!IsBroadcaster(), "Should not call SendSyncMessage in chrome");
  NS_ASSERTION(!GetParentManager(),
               "Should not have parent manager in content!");

  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
    "nsFrameMessageManager::SendMessage", EVENTS, aMessageName);

  if (sSendingSyncMessage && aIsSync) {
    // No kind of blocking send should be issued on top of a sync message.
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  StructuredCloneData data;
  if (!aObj.isUndefined() &&
      !GetParamsForMessage(aCx, aObj, JS::UndefinedHandleValue, data)) {
    aError.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

#ifdef FUZZING
  if (data.DataLength() > 0) {
    MessageManagerFuzzer::TryMutate(
      aCx,
      aMessageName,
      &data,
      JS::UndefinedHandleValue);
  }
#endif

  if (!AllowMessage(data.DataLength(), aMessageName)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (!mCallback) {
    aError.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  nsTArray<StructuredCloneData> retval;

  TimeStamp start = TimeStamp::Now();
  sSendingSyncMessage |= aIsSync;
  bool ok = mCallback->DoSendBlockingMessage(aCx, aMessageName, data, aObjects,
                                             aPrincipal, &retval, aIsSync);
  if (aIsSync) {
    sSendingSyncMessage = false;
  }

  uint32_t latencyMs = round((TimeStamp::Now() - start).ToMilliseconds());
  if (latencyMs >= kMinTelemetrySyncMessageManagerLatencyMs) {
    NS_ConvertUTF16toUTF8 messageName(aMessageName);
    // NOTE: We need to strip digit characters from the message name in order to
    // avoid a large number of buckets due to generated names from addons (such
    // as "ublock:sb:{N}"). See bug 1348113 comment 10.
    messageName.StripTaggedASCII(ASCIIMask::Mask0to9());
    Telemetry::Accumulate(Telemetry::IPC_SYNC_MESSAGE_MANAGER_LATENCY_MS,
                          messageName, latencyMs);
  }

  if (!ok) {
    return;
  }

  uint32_t len = retval.Length();
  aResult.SetCapacity(len);
  for (uint32_t i = 0; i < len; ++i) {
    JS::Rooted<JS::Value> ret(aCx);
    retval[i].Read(aCx, &ret, aError);
    if (aError.Failed()) {
      MOZ_ASSERT(false, "Unable to read structured clone in SendMessage");
      return;
    }
    aResult.AppendElement(ret);
  }
}

nsresult
nsFrameMessageManager::DispatchAsyncMessageInternal(JSContext* aCx,
                                                    const nsAString& aMessage,
                                                    StructuredCloneData& aData,
                                                    JS::Handle<JSObject *> aCpows,
                                                    nsIPrincipal* aPrincipal)
{
  if (mIsBroadcaster) {
    uint32_t len = mChildManagers.Length();
    for (uint32_t i = 0; i < len; ++i) {
      mChildManagers[i]->
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

void
nsFrameMessageManager::DispatchAsyncMessage(JSContext* aCx,
                                            const nsAString& aMessageName,
                                            JS::Handle<JS::Value> aObj,
                                            JS::Handle<JSObject*> aObjects,
                                            nsIPrincipal* aPrincipal,
                                            JS::Handle<JS::Value> aTransfers,
                                            ErrorResult& aError)
{
  StructuredCloneData data;
  if (!aObj.isUndefined() && !GetParamsForMessage(aCx, aObj, aTransfers, data)) {
    aError.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

#ifdef FUZZING
  if (data.DataLength()) {
    MessageManagerFuzzer::TryMutate(aCx, aMessageName, &data, aTransfers);
  }
#endif

  if (!AllowMessage(data.DataLength(), aMessageName)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  aError = DispatchAsyncMessageInternal(aCx, aMessageName, data, aObjects,
                                        aPrincipal);
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
  RefPtr<nsFrameMessageManager> mMM;
};


void
nsFrameMessageManager::ReceiveMessage(nsISupports* aTarget,
                                      nsFrameLoader* aTargetFrameLoader,
                                      bool aTargetClosed,
                                      const nsAString& aMessage,
                                      bool aIsSync,
                                      StructuredCloneData* aCloneData,
                                      mozilla::jsipc::CpowHolder* aCpows,
                                      nsIPrincipal* aPrincipal,
                                      nsTArray<StructuredCloneData>* aRetVal,
                                      ErrorResult& aError)
{
  MOZ_ASSERT(aTarget);

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

      JS::RootingContext* rcx = RootingCx();
      JS::Rooted<JSObject*> object(rcx);

      RefPtr<MessageListener> webIDLListener;
      if (!weakListener) {
        webIDLListener = listener.mStrongListener;
        object = webIDLListener->CallbackOrNull();
      } else {
        nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS = do_QueryInterface(weakListener);
        if (!wrappedJS) {
          continue;
        }

        object = wrappedJS->GetJSObject();
      }

      if (!object) {
        continue;
      }

      AutoEntryScript aes(object, "message manager handler");
      JSContext* cx = aes.cx();

      RootedDictionary<ReceiveMessageArgument> argument(cx);

      JS::Rooted<JSObject*> cpows(cx);
      if (aCpows && !aCpows->ToObject(cx, &cpows)) {
        aError.Throw(NS_ERROR_UNEXPECTED);
        return;
      }

      if (!cpows) {
        cpows = JS_NewPlainObject(cx);
        if (!cpows) {
          aError.Throw(NS_ERROR_UNEXPECTED);
          return;
        }
      }
      argument.mObjects = cpows;

      JS::Rooted<JS::Value> json(cx, JS::NullValue());
      if (aCloneData && aCloneData->DataLength()) {
        aCloneData->Read(cx, &json, aError);
        if (NS_WARN_IF(aError.Failed())) {
          aError.SuppressException();
          JS_ClearPendingException(cx);
          return;
        }
      }
      argument.mData = json;
      argument.mJson = json;

      // Get cloned MessagePort from StructuredCloneData.
      if (aCloneData) {
        Sequence<OwningNonNull<MessagePort>> ports;
        if (!aCloneData->TakeTransferredPortsAsSequence(ports)) {
          aError.Throw(NS_ERROR_FAILURE);
          return;
        }
        argument.mPorts.Construct(std::move(ports));
      }

      argument.mName = aMessage;
      argument.mPrincipal = aPrincipal;
      argument.mSync = aIsSync;
      argument.mTarget = aTarget;
      if (aTargetFrameLoader) {
        argument.mTargetFrameLoader.Construct(*aTargetFrameLoader);
      }

      JS::Rooted<JS::Value> thisValue(cx, JS::UndefinedValue());

      if (JS::IsCallable(object)) {
        // A small hack to get 'this' value right on content side where
        // messageManager is wrapped in TabChildGlobal.
        nsCOMPtr<nsISupports> defaultThisValue;
        if (mChrome) {
          defaultThisValue = do_QueryObject(this);
        } else {
          defaultThisValue = aTarget;
        }
        js::AssertSameCompartment(cx, object);
        aError = nsContentUtils::WrapNative(cx, defaultThisValue, &thisValue);
        if (aError.Failed()) {
          return;
        }
      }

      JS::Rooted<JS::Value> rval(cx, JS::UndefinedValue());
      if (webIDLListener) {
        webIDLListener->ReceiveMessage(thisValue, argument, &rval, aError);
        if (aError.Failed()) {
          // At this point the call to ReceiveMessage will have reported any exceptions
          // (we kept the default of eReportExceptions). We suppress the failure in the
          // ErrorResult and continue.
          aError.SuppressException();
          continue;
        }
      } else {
        JS::Rooted<JS::Value> funval(cx);
        if (JS::IsCallable(object)) {
          // If the listener is a JS function:
          funval.setObject(*object);
        } else {
          // If the listener is a JS object which has receiveMessage function:
          if (!JS_GetProperty(cx, object, "receiveMessage", &funval) ||
              !funval.isObject()) {
            aError.Throw(NS_ERROR_UNEXPECTED);
            return;
          }

          // Check if the object is even callable.
          if (!JS::IsCallable(&funval.toObject())) {
            aError.Throw(NS_ERROR_UNEXPECTED);
            return;
          }
          thisValue.setObject(*object);
        }

        JS::Rooted<JS::Value> argv(cx);
        if (!ToJSValue(cx, argument, &argv)) {
          aError.Throw(NS_ERROR_UNEXPECTED);
          return;
        }

        {
          JS::Rooted<JSObject*> thisObject(cx, thisValue.toObjectOrNull());
          js::AssertSameCompartment(cx, thisObject);
          if (!JS_CallFunctionValue(cx, thisObject, funval,
                                    JS::HandleValueArray(argv), &rval)) {
            // Because the AutoEntryScript is inside the loop this continue will make us
            // report any exceptions (after which we'll move on to the next listener).
            continue;
          }
        }
      }

      if (aRetVal) {
        StructuredCloneData* data = aRetVal->AppendElement();
        data->InitScope(JS::StructuredCloneScope::DifferentProcess);
        data->Write(cx, rval, aError);
        if (NS_WARN_IF(aError.Failed())) {
          aRetVal->RemoveLastElement();
          nsString msg = aMessage + NS_LITERAL_STRING(": message reply cannot be cloned. Are you trying to send an XPCOM object?");

          nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
          if (console) {
            nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
            error->Init(msg, EmptyString(), EmptyString(),
                        0, 0, nsIScriptError::warningFlag, "chrome javascript",
                        false /* from private window */);
            console->LogMessage(error);
          }

          JS_ClearPendingException(cx);
          continue;
        }
      }
    }
  }

  RefPtr<nsFrameMessageManager> kungFuDeathGrip = GetParentManager();
  if (kungFuDeathGrip) {
    kungFuDeathGrip->ReceiveMessage(aTarget, aTargetFrameLoader, aTargetClosed, aMessage,
                                    aIsSync, aCloneData, aCpows, aPrincipal, aRetVal,
                                    aError);
  }
}

void
nsFrameMessageManager::LoadPendingScripts(nsFrameMessageManager* aManager,
                                          nsFrameMessageManager* aChildMM)
{
  // We have parent manager if we're a message broadcaster.
  // In that case we want to load the pending scripts from all parent
  // message managers in the hierarchy. Process the parent first so
  // that pending scripts higher up in the hierarchy are loaded before others.
  nsFrameMessageManager* parentManager = aManager->GetParentManager();
  if (parentManager) {
    LoadPendingScripts(parentManager, aChildMM);
  }

  for (uint32_t i = 0; i < aManager->mPendingScripts.Length(); ++i) {
    aChildMM->LoadScript(aManager->mPendingScripts[i],
                         false,
                         aManager->mPendingScriptsGlobalStates[i],
                         IgnoreErrors());
  }
}

void
nsFrameMessageManager::LoadPendingScripts()
{
  RefPtr<nsFrameMessageManager> kungfuDeathGrip = this;
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
nsFrameMessageManager::Close()
{
  if (!mClosed) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->NotifyObservers(this, "message-manager-close", nullptr);
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
       obs->NotifyObservers(this, "message-manager-disconnect", nullptr);
    }
  }

  ClearParentManager(aRemoveFromParent);

  mDisconnected = true;
  if (!mHandlingMessage) {
    mListeners.Clear();
  }
}

void
nsFrameMessageManager::SetInitialProcessData(JS::HandleValue aInitialData)
{
  MOZ_ASSERT(!mChrome);
  MOZ_ASSERT(mIsProcessManager);
  MOZ_ASSERT(aInitialData.isObject());
  mInitialProcessData = aInitialData;
}

void
nsFrameMessageManager::GetInitialProcessData(JSContext* aCx,
                                             JS::MutableHandle<JS::Value> aInitialProcessData,
                                             ErrorResult& aError)
{
  MOZ_ASSERT(mIsProcessManager);
  MOZ_ASSERT_IF(mChrome, IsBroadcaster());

  JS::RootedValue init(aCx, mInitialProcessData);
  if (mChrome && init.isUndefined()) {
    // We create the initial object in the junk scope. If we created it in a
    // normal realm, that realm would leak until shutdown.
    JS::RootedObject global(aCx, xpc::PrivilegedJunkScope());
    JSAutoRealm ar(aCx, global);

    JS::RootedObject obj(aCx, JS_NewPlainObject(aCx));
    if (!obj) {
      aError.NoteJSContextException(aCx);
      return;
    }

    mInitialProcessData.setObject(*obj);
    init.setObject(*obj);
  }

  if (!mChrome && XRE_IsParentProcess()) {
    // This is the cpmm in the parent process. We should use the same object as the ppmm.
    // Create it first through do_GetService and use the cached pointer in
    // sParentProcessManager.
    nsCOMPtr<nsISupports> ppmm = do_GetService("@mozilla.org/parentprocessmessagemanager;1");
    sParentProcessManager->GetInitialProcessData(aCx, &init, aError);
    if (aError.Failed()) {
      return;
    }
    mInitialProcessData = init;
  }

  if (!JS_WrapValue(aCx, &init)) {
    aError.NoteJSContextException(aCx);
    return;
  }
  aInitialProcessData.set(init);
}

already_AddRefed<ProcessMessageManager>
nsFrameMessageManager::GetProcessMessageManager(ErrorResult& aError)
{
  RefPtr<ProcessMessageManager> pmm;
  if (mCallback) {
    pmm = mCallback->GetProcessMessageManager();
  }
  return pmm.forget();
}

void
nsFrameMessageManager::GetRemoteType(nsAString& aRemoteType, ErrorResult& aError) const
{
  aRemoteType.Truncate();
  if (mCallback) {
    mCallback->DoGetRemoteType(aRemoteType, aError);
  }
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
  ~MessageManagerReporter() = default;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  static const size_t kSuspectReferentCount = 300;
protected:
  void CountReferents(nsFrameMessageManager* aMessageManager,
                      MessageManagerReferentCount* aReferentCount);
};

NS_IMPL_ISUPPORTS(MessageManagerReporter, nsIMemoryReporter)

void
MessageManagerReporter::CountReferents(nsFrameMessageManager* aMessageManager,
                                       MessageManagerReferentCount* aReferentCount)
{
  for (auto it = aMessageManager->mListeners.Iter(); !it.Done(); it.Next()) {
    nsAutoTObserverArray<nsMessageListenerInfo, 1>* listeners =
      it.UserData();
    uint32_t listenerCount = listeners->Length();
    if (listenerCount == 0) {
      continue;
    }

    nsString key(it.Key());
    uint32_t oldCount = 0;
    aReferentCount->mMessageCounter.Get(key, &oldCount);
    uint32_t currentCount = oldCount + listenerCount;
    aReferentCount->mMessageCounter.Put(key, currentCount);

    // Keep track of messages that have a suspiciously large
    // number of referents (symptom of leak).
    if (currentCount == MessageManagerReporter::kSuspectReferentCount) {
      aReferentCount->mSuspectMessages.AppendElement(key);
    }

    for (uint32_t i = 0; i < listenerCount; ++i) {
      const nsMessageListenerInfo& listenerInfo = listeners->ElementAt(i);
      if (listenerInfo.mWeakListener) {
        nsCOMPtr<nsISupports> referent =
          do_QueryReferent(listenerInfo.mWeakListener);
        if (referent) {
          aReferentCount->mWeakAlive++;
        } else {
          aReferentCount->mWeakDead++;
        }
      } else {
        aReferentCount->mStrong++;
      }
    }
  }

  // Add referent count in child managers because the listeners
  // participate in messages dispatched from parent message manager.
  for (uint32_t i = 0; i < aMessageManager->mChildManagers.Length(); ++i) {
    RefPtr<nsFrameMessageManager> mm = aMessageManager->mChildManagers[i];
    CountReferents(mm, aReferentCount);
  }
}

static void
ReportReferentCount(const char* aManagerType,
                    const MessageManagerReferentCount& aReferentCount,
                    nsIHandleReportCallback* aHandleReport,
                    nsISupports* aData)
{
#define REPORT(_path, _amount, _desc) \
    do { \
      aHandleReport->Callback(EmptyCString(), _path, \
                              nsIMemoryReporter::KIND_OTHER, \
                              nsIMemoryReporter::UNITS_COUNT, _amount, \
                              _desc, aData); \
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
}

static StaticRefPtr<ChromeMessageBroadcaster> sGlobalMessageManager;

NS_IMETHODIMP
MessageManagerReporter::CollectReports(nsIHandleReportCallback* aHandleReport,
                                       nsISupports* aData, bool aAnonymize)
{
  if (XRE_IsParentProcess() && sGlobalMessageManager) {
    MessageManagerReferentCount count;
    CountReferents(sGlobalMessageManager, &count);
    ReportReferentCount("global-manager", count, aHandleReport, aData);
  }

  if (nsFrameMessageManager::sParentProcessManager) {
    MessageManagerReferentCount count;
    CountReferents(nsFrameMessageManager::sParentProcessManager, &count);
    ReportReferentCount("parent-process-manager", count, aHandleReport, aData);
  }

  if (nsFrameMessageManager::sChildProcessManager) {
    MessageManagerReferentCount count;
    CountReferents(nsFrameMessageManager::sChildProcessManager, &count);
    ReportReferentCount("child-process-manager", count, aHandleReport, aData);
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla

already_AddRefed<ChromeMessageBroadcaster>
nsFrameMessageManager::GetGlobalMessageManager()
{
  RefPtr<ChromeMessageBroadcaster> mm;
  if (sGlobalMessageManager) {
    mm = sGlobalMessageManager;
  } else {
    sGlobalMessageManager = mm =
      new ChromeMessageBroadcaster(MessageManagerFlags::MM_GLOBAL);
    ClearOnShutdown(&sGlobalMessageManager);
    RegisterStrongMemoryReporter(new MessageManagerReporter());
  }
  return mm.forget();
}

nsresult
NS_NewGlobalMessageManager(nsISupports** aResult)
{
  *aResult = nsFrameMessageManager::GetGlobalMessageManager().take();
  return NS_OK;
}

nsDataHashtable<nsStringHashKey, nsMessageManagerScriptHolder*>*
  nsMessageManagerScriptExecutor::sCachedScripts = nullptr;
StaticRefPtr<nsScriptCacheCleaner> nsMessageManagerScriptExecutor::sScriptCacheCleaner;

void
nsMessageManagerScriptExecutor::DidCreateGlobal()
{
  if (!sCachedScripts) {
    sCachedScripts =
      new nsDataHashtable<nsStringHashKey, nsMessageManagerScriptHolder*>;
    sScriptCacheCleaner = new nsScriptCacheCleaner();
  }
}

// static
void
nsMessageManagerScriptExecutor::PurgeCache()
{
  if (sCachedScripts) {
    NS_ASSERTION(sCachedScripts != nullptr, "Need cached scripts");
    for (auto iter = sCachedScripts->Iter(); !iter.Done(); iter.Next()) {
      delete iter.Data();
      iter.Remove();
    }
  }
}

// static
void
nsMessageManagerScriptExecutor::Shutdown()
{
  if (sCachedScripts) {
    PurgeCache();

    delete sCachedScripts;
    sCachedScripts = nullptr;
    sScriptCacheCleaner = nullptr;
  }
}

void
nsMessageManagerScriptExecutor::LoadScriptInternal(JS::Handle<JSObject*> aGlobal,
                                                   const nsAString& aURL,
                                                   bool aRunInGlobalScope)
{
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
    "nsMessageManagerScriptExecutor::LoadScriptInternal", OTHER, aURL);

  if (!sCachedScripts) {
    return;
  }

  JS::RootingContext* rcx = RootingCx();
  JS::Rooted<JSScript*> script(rcx);

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

  AutoEntryScript aes(aGlobal, "message manager script load");
  JSContext* cx = aes.cx();
  if (script) {
    if (aRunInGlobalScope) {
      JS::RootedValue rval(cx);
      JS::CloneAndExecuteScript(cx, script, &rval);
    } else {
      JS::Rooted<JSObject*> scope(cx);
      bool ok = js::ExecuteInGlobalAndReturnScope(cx, aGlobal, script, &scope);
      if (ok) {
        // Force the scope to stay alive.
        mAnonymousGlobalScopes.AppendElement(scope);
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

  // Compile the script in the compilation scope instead of the current global
  // to avoid keeping the current compartment alive.
  AutoJSAPI jsapi;
  if (!jsapi.Init(xpc::CompilationScope())) {
    return;
  }
  JSContext* cx = jsapi.cx();
  JS::Rooted<JSScript*> script(cx);

  script = ScriptPreloader::GetChildSingleton().GetCachedScript(cx, url);

  if (!script) {
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
    if (input) {
      nsCString buffer;
      uint64_t written;
      if (NS_FAILED(NS_ReadInputStreamToString(input, buffer, -1, &written))) {
        return;
      }

      uint32_t size = (uint32_t)std::min(written, (uint64_t)UINT32_MAX);
      ScriptLoader::ConvertToUTF16(channel, (uint8_t*)buffer.get(), size,
                                   EmptyString(), nullptr,
                                   dataStringBuf, dataStringLength);
    }

    JS::SourceBufferHolder srcBuf(dataStringBuf, dataStringLength,
                                  JS::SourceBufferHolder::GiveOwnership);

    if (!dataStringBuf || dataStringLength == 0) {
      return;
    }

    JS::CompileOptions options(cx);
    options.setFileAndLine(url.get(), 1);
    options.setNoScriptRval(true);

    if (aRunInGlobalScope) {
      if (!JS::Compile(cx, options, srcBuf, &script)) {
        return;
      }
    // We're going to run these against some non-global scope.
    } else if (!JS::CompileForNonSyntacticScope(cx, options, srcBuf, &script)) {
      return;
    }
  }

  MOZ_ASSERT(script);
  aScriptp.set(script);

  nsAutoCString scheme;
  uri->GetScheme(scheme);
  // We don't cache data: scripts!
  if (aShouldCache && !scheme.EqualsLiteral("data")) {
    ScriptPreloader::GetChildSingleton().NoteScript(url, url, script);
    // Root the object also for caching.
    auto* holder = new nsMessageManagerScriptHolder(cx, script, aRunInGlobalScope);
    sCachedScripts->Put(aURL, holder);
  }
}

void
nsMessageManagerScriptExecutor::TryCacheLoadAndCompileScript(
  const nsAString& aURL,
  bool aRunInGlobalScope)
{
  JS::Rooted<JSScript*> script(RootingCx());
  TryCacheLoadAndCompileScript(aURL, aRunInGlobalScope, true, &script);
}

void
nsMessageManagerScriptExecutor::Trace(const TraceCallbacks& aCallbacks, void* aClosure)
{
  for (size_t i = 0, length = mAnonymousGlobalScopes.Length(); i < length; ++i) {
    aCallbacks.Trace(&mAnonymousGlobalScopes[i], "mAnonymousGlobalScopes[i]", aClosure);
  }
}

void
nsMessageManagerScriptExecutor::Unlink()
{
  ImplCycleCollectionUnlink(mAnonymousGlobalScopes);
}

bool
nsMessageManagerScriptExecutor::InitChildGlobalInternal(const nsACString& aID)
{
  AutoSafeJSContext cx;
  if (!SystemBindingInitIds(cx)) {
    return false;
  }

  nsContentUtils::GetSecurityManager()->GetSystemPrincipal(getter_AddRefs(mPrincipal));

  JS::RealmOptions options;
  options.creationOptions().setSystemZone();

  xpc::InitGlobalObjectOptions(options, mPrincipal);
  JS::Rooted<JSObject*> global(cx);
  if (!WrapGlobalObject(cx, options, &global)) {
    return false;
  }

  xpc::InitGlobalObject(cx, global, 0);

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
    mAnonymousGlobalScopes[i].exposeToActiveJS();
  }
}

NS_IMPL_ISUPPORTS(nsScriptCacheCleaner, nsIObserver)

ChildProcessMessageManager* nsFrameMessageManager::sChildProcessManager = nullptr;
ParentProcessMessageManager* nsFrameMessageManager::sParentProcessManager = nullptr;
nsFrameMessageManager* nsFrameMessageManager::sSameProcessParentManager = nullptr;

class nsAsyncMessageToSameProcessChild : public nsSameProcessAsyncMessageBase,
                                         public Runnable
{
public:
  nsAsyncMessageToSameProcessChild(JS::RootingContext* aRootingCx,
                                   JS::Handle<JSObject*> aCpows)
    : nsSameProcessAsyncMessageBase(aRootingCx, aCpows)
    , mozilla::Runnable("nsAsyncMessageToSameProcessChild")
  { }
  NS_IMETHOD Run() override
  {
    nsFrameMessageManager* ppm = nsFrameMessageManager::GetChildProcessManager();
    ReceiveMessage(ppm, nullptr, ppm);
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
  ~SameParentProcessMessageManagerCallback() override
  {
    MOZ_COUNT_DTOR(SameParentProcessMessageManagerCallback);
  }

  bool DoLoadMessageManagerScript(const nsAString& aURL,
                                  bool aRunInGlobalScope) override
  {
    ProcessGlobal* global = ProcessGlobal::Get();
    MOZ_ASSERT(!aRunInGlobalScope);
    global->LoadScript(aURL);
    return true;
  }

  nsresult DoSendAsyncMessage(JSContext* aCx,
                              const nsAString& aMessage,
                              StructuredCloneData& aData,
                              JS::Handle<JSObject *> aCpows,
                              nsIPrincipal* aPrincipal) override
  {
    JS::RootingContext* rcx = JS::RootingContext::get(aCx);
    RefPtr<nsAsyncMessageToSameProcessChild> ev =
      new nsAsyncMessageToSameProcessChild(rcx, aCpows);

    nsresult rv = ev->Init(aMessage, aData, aPrincipal);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = NS_DispatchToCurrentThread(ev);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
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
  ~ChildProcessMessageManagerCallback() override
  {
    MOZ_COUNT_DTOR(ChildProcessMessageManagerCallback);
  }

  bool DoSendBlockingMessage(JSContext* aCx,
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

  nsresult DoSendAsyncMessage(JSContext* aCx,
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
    if (!cc->SendAsyncMessage(PromiseFlatString(aMessage), cpows,
                              IPC::Principal(aPrincipal), data)) {
      return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
  }
};


class nsAsyncMessageToSameProcessParent : public nsSameProcessAsyncMessageBase,
                                          public SameProcessMessageQueue::Runnable
{
public:
  nsAsyncMessageToSameProcessParent(JS::RootingContext* aRootingCx,
                                    JS::Handle<JSObject*> aCpows)
    : nsSameProcessAsyncMessageBase(aRootingCx, aCpows)
  { }
  nsresult HandleMessage() override
  {
    nsFrameMessageManager* ppm = nsFrameMessageManager::sSameProcessParentManager;
    ReceiveMessage(ppm, nullptr, ppm);
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
  ~SameChildProcessMessageManagerCallback() override
  {
    MOZ_COUNT_DTOR(SameChildProcessMessageManagerCallback);
  }

  bool DoSendBlockingMessage(JSContext* aCx,
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
      SameProcessCpowHolder cpows(JS::RootingContext::get(aCx), aCpows);
      RefPtr<nsFrameMessageManager> ppm = nsFrameMessageManager::sSameProcessParentManager;
      ppm->ReceiveMessage(ppm, nullptr, aMessage, true, &aData, &cpows, aPrincipal,
                          aRetVal, IgnoreErrors());
    }
    return true;
  }

  nsresult DoSendAsyncMessage(JSContext* aCx,
                              const nsAString& aMessage,
                              StructuredCloneData& aData,
                              JS::Handle<JSObject *> aCpows,
                              nsIPrincipal* aPrincipal) override
  {
    SameProcessMessageQueue* queue = SameProcessMessageQueue::Get();
    JS::RootingContext* rcx = JS::RootingContext::get(aCx);
    RefPtr<nsAsyncMessageToSameProcessParent> ev =
      new nsAsyncMessageToSameProcessParent(rcx, aCpows);
    nsresult rv = ev->Init(aMessage, aData, aPrincipal);

    if (NS_FAILED(rv)) {
      return rv;
    }
    queue->Push(ev);
    return NS_OK;
  }

};


// This creates the global parent process message manager.
nsresult
NS_NewParentProcessMessageManager(nsISupports** aResult)
{
  NS_ASSERTION(!nsFrameMessageManager::sParentProcessManager,
               "Re-creating sParentProcessManager");
  RefPtr<ParentProcessMessageManager> mm = new ParentProcessMessageManager();
  nsFrameMessageManager::sParentProcessManager = mm;
  nsFrameMessageManager::NewProcessMessageManager(false); // Create same process message manager.
  mm.forget(aResult);
  return NS_OK;
}


ProcessMessageManager*
nsFrameMessageManager::NewProcessMessageManager(bool aIsRemote)
{
  if (!nsFrameMessageManager::sParentProcessManager) {
     nsCOMPtr<nsISupports> dummy =
       do_GetService("@mozilla.org/parentprocessmessagemanager;1");
  }

  MOZ_ASSERT(nsFrameMessageManager::sParentProcessManager,
             "parent process manager not created");
  ProcessMessageManager* mm;
  if (aIsRemote) {
    // Callback is set in ContentParent::InitInternal so that the process has
    // already started when we send pending scripts.
    mm = new ProcessMessageManager(nullptr,
                                   nsFrameMessageManager::sParentProcessManager);
  } else {
    mm = new ProcessMessageManager(new SameParentProcessMessageManagerCallback(),
                                   nsFrameMessageManager::sParentProcessManager,
                                   MessageManagerFlags::MM_OWNSCALLBACK);
    sSameProcessParentManager = mm;
  }
  return mm;
}

nsresult
NS_NewChildProcessMessageManager(nsISupports** aResult)
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
  auto* mm = new ChildProcessMessageManager(cb);
  nsFrameMessageManager::SetChildProcessManager(mm);
  RefPtr<ProcessGlobal> global = new ProcessGlobal(mm);
  NS_ENSURE_TRUE(global->Init(), NS_ERROR_UNEXPECTED);
  return CallQueryInterface(global, aResult);
}

void
nsFrameMessageManager::MarkForCC()
{
  for (auto iter = mListeners.Iter(); !iter.Done(); iter.Next()) {
    nsAutoTObserverArray<nsMessageListenerInfo, 1>* listeners = iter.UserData();
    uint32_t count = listeners->Length();
    for (uint32_t i = 0; i < count; i++) {
      MessageListener* strongListener = listeners->ElementAt(i).mStrongListener;
      if (strongListener) {
        strongListener->MarkForCC();
      }
    }
  }

  if (mRefCnt.IsPurple()) {
    mRefCnt.RemovePurple();
  }
}

nsSameProcessAsyncMessageBase::nsSameProcessAsyncMessageBase(JS::RootingContext* aRootingCx,
                                                             JS::Handle<JSObject*> aCpows)
  : mCpows(aRootingCx, aCpows)
#ifdef DEBUG
  , mCalledInit(false)
#endif
{ }


nsresult
nsSameProcessAsyncMessageBase::Init(const nsAString& aMessage,
                                    StructuredCloneData& aData,
                                    nsIPrincipal* aPrincipal)
{
  if (!mData.Copy(aData)) {
    Telemetry::Accumulate(Telemetry::IPC_SAME_PROCESS_MESSAGE_COPY_OOM_KB, aData.DataLength());
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mMessage = aMessage;
  mPrincipal = aPrincipal;
#ifdef DEBUG
  mCalledInit = true;
#endif

  return NS_OK;
}

void
nsSameProcessAsyncMessageBase::ReceiveMessage(nsISupports* aTarget,
                                              nsFrameLoader* aTargetFrameLoader,
                                              nsFrameMessageManager* aManager)
{
  // Make sure that we have called Init() and it has succeeded.
  MOZ_ASSERT(mCalledInit);
  if (aManager) {
    SameProcessCpowHolder cpows(RootingCx(), mCpows);

    RefPtr<nsFrameMessageManager> mm = aManager;
    mm->ReceiveMessage(aTarget, aTargetFrameLoader, mMessage, false, &mData,
                       &cpows, mPrincipal, nullptr, IgnoreErrors());
  }
}
