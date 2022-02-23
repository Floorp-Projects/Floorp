/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFrameMessageManager.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>
#include "ContentChild.h"
#include "ErrorList.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/Unused.h"
#include "base/process_util.h"
#include "chrome/common/ipc_channel.h"
#include "js/CallAndConstruct.h"  // JS::IsCallable, JS_CallFunctionValue
#include "js/CompilationAndEvaluation.h"
#include "js/CompileOptions.h"
#include "js/experimental/JSStencil.h"
#include "js/GCVector.h"
#include "js/JSON.h"
#include "js/PropertyAndElement.h"  // JS_GetProperty
#include "js/RootingAPI.h"
#include "js/SourceText.h"
#include "js/StructuredClone.h"
#include "js/TypeDecls.h"
#include "js/Wrapper.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/NotNull.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ScriptPreloader.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryHistogramEnums.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/CallbackObject.h"
#include "mozilla/dom/ChildProcessMessageManager.h"
#include "mozilla/dom/ChromeMessageBroadcaster.h"
#include "mozilla/dom/ContentProcessMessageManager.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/MessageBroadcaster.h"
#include "mozilla/dom/MessageListenerManager.h"
#include "mozilla/dom/MessageManagerBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/ParentProcessMessageManager.h"
#include "mozilla/dom/ProcessMessageManager.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/SameProcessMessageQueue.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/MessageManagerCallback.h"
#include "mozilla/dom/ipc/SharedMap.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "nsASCIIMask.h"
#include "nsBaseHashtable.h"
#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionNoteChild.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTHashMap.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsFrameMessageManager.h"
#include "nsHashKeys.h"
#include "nsIChannel.h"
#include "nsIConsoleService.h"
#include "nsIContentPolicy.h"
#include "nsIInputStream.h"
#include "nsILoadInfo.h"
#include "nsIMemoryReporter.h"
#include "nsIMessageManager.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptError.h"
#include "nsISupports.h"
#include "nsISupportsUtils.h"
#include "nsIURI.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIXPConnect.h"
#include "nsJSUtils.h"
#include "nsLiteralString.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsQueryObject.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsStringFlags.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTLiteralString.h"
#include "nsTObserverArray.h"
#include "nsTPromiseFlatString.h"
#include "nsTStringRepr.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "nscore.h"
#include "xpcpublic.h"

#ifdef XP_WIN
#  if defined(SendMessage)
#    undef SendMessage
#  endif
#endif

#ifdef FUZZING
#  include "MessageManagerFuzzer.h"
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
      mCallback(aCallback) {
  NS_ASSERTION(!mIsBroadcaster || !mCallback,
               "Broadcasters cannot have callbacks!");
  if (mOwnsCallback) {
    mOwnedCallback = WrapUnique(aCallback);
  }
}

nsFrameMessageManager::~nsFrameMessageManager() {
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

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    nsMessageListenerInfo& aField, const char* aName, uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(aCallback, aField.mStrongListener, aName, aFlags);
  ImplCycleCollectionTraverse(aCallback, aField.mWeakListener, aName, aFlags);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsFrameMessageManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsFrameMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListeners)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChildManagers)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSharedData)
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSharedData)
  tmp->mInitialProcessData.setNull();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsISupports)

  /* Message managers in child process implement nsIMessageSender.
     Message managers in the chrome process are
     either broadcasters (if they have subordinate/child message
     managers) or they're simple message senders. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIMessageSender,
                                     !mChrome || !mIsBroadcaster)

NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFrameMessageManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFrameMessageManager)

void MessageManagerCallback::DoGetRemoteType(nsACString& aRemoteType,
                                             ErrorResult& aError) const {
  aRemoteType.Truncate();
  mozilla::dom::ProcessMessageManager* parent = GetProcessMessageManager();
  if (!parent) {
    return;
  }

  parent->GetRemoteType(aRemoteType, aError);
}

bool MessageManagerCallback::BuildClonedMessageDataForParent(
    ContentParent* aParent, StructuredCloneData& aData,
    ClonedMessageData& aClonedData) {
  return aData.BuildClonedMessageDataForParent(aParent, aClonedData);
}

bool MessageManagerCallback::BuildClonedMessageDataForChild(
    ContentChild* aChild, StructuredCloneData& aData,
    ClonedMessageData& aClonedData) {
  return aData.BuildClonedMessageDataForChild(aChild, aClonedData);
}

void mozilla::dom::ipc::UnpackClonedMessageDataForParent(
    const ClonedMessageData& aClonedData, StructuredCloneData& aData) {
  aData.BorrowFromClonedMessageDataForParent(aClonedData);
}

void mozilla::dom::ipc::UnpackClonedMessageDataForChild(
    const ClonedMessageData& aClonedData, StructuredCloneData& aData) {
  aData.BorrowFromClonedMessageDataForChild(aClonedData);
}

void nsFrameMessageManager::AddMessageListener(const nsAString& aMessageName,
                                               MessageListener& aListener,
                                               bool aListenWhenClosed,
                                               ErrorResult& aError) {
  auto* const listeners = mListeners.GetOrInsertNew(aMessageName);
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

void nsFrameMessageManager::RemoveMessageListener(const nsAString& aMessageName,
                                                  MessageListener& aListener,
                                                  ErrorResult& aError) {
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

static already_AddRefed<nsISupports> ToXPCOMMessageListener(
    MessageListener& aListener) {
  return CallbackObjectHolder<mozilla::dom::MessageListener, nsISupports>(
             &aListener)
      .ToXPCOMCallback();
}

void nsFrameMessageManager::AddWeakMessageListener(
    const nsAString& aMessageName, MessageListener& aListener,
    ErrorResult& aError) {
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
  for (const auto& entry : mListeners) {
    nsAutoTObserverArray<nsMessageListenerInfo, 1>* listeners = entry.GetWeak();
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

  auto* const listeners = mListeners.GetOrInsertNew(aMessageName);
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

void nsFrameMessageManager::RemoveWeakMessageListener(
    const nsAString& aMessageName, MessageListener& aListener,
    ErrorResult& aError) {
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

void nsFrameMessageManager::LoadScript(const nsAString& aURL,
                                       bool aAllowDelayedLoad,
                                       bool aRunInGlobalScope,
                                       ErrorResult& aError) {
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

void nsFrameMessageManager::RemoveDelayedScript(const nsAString& aURL) {
  for (uint32_t i = 0; i < mPendingScripts.Length(); ++i) {
    if (mPendingScripts[i] == aURL) {
      mPendingScripts.RemoveElementAt(i);
      mPendingScriptsGlobalStates.RemoveElementAt(i);
      break;
    }
  }
}

void nsFrameMessageManager::GetDelayedScripts(
    JSContext* aCx, nsTArray<nsTArray<JS::Value>>& aList, ErrorResult& aError) {
  // Frame message managers may return an incomplete list because scripts
  // that were loaded after it was connected are not added to the list.
  if (!IsGlobal() && !IsBroadcaster()) {
    NS_WARNING(
        "Cannot retrieve list of pending frame scripts for frame"
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

/* static */
bool nsFrameMessageManager::GetParamsForMessage(JSContext* aCx,
                                                const JS::Value& aValue,
                                                const JS::Value& aTransfer,
                                                StructuredCloneData& aData) {
  // First try to use structured clone on the whole thing.
  JS::RootedValue v(aCx, aValue);
  JS::RootedValue t(aCx, aTransfer);
  ErrorResult rv;
  aData.Write(aCx, v, t, JS::CloneDataPolicy(), rv);
  if (!rv.Failed()) {
    return true;
  }

  rv.SuppressException();
  JS_ClearPendingException(aCx);

  nsCOMPtr<nsIConsoleService> console(
      do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (console) {
    nsAutoString filename;
    uint32_t lineno = 0, column = 0;
    nsJSUtils::GetCallingLocation(aCx, filename, &lineno, &column);
    nsCOMPtr<nsIScriptError> error(
        do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
    error->Init(
        u"Sending message that cannot be cloned. Are "
        "you trying to send an XPCOM object?"_ns,
        filename, u""_ns, lineno, column, nsIScriptError::warningFlag,
        "chrome javascript", false /* from private window */,
        true /* from chrome context */);
    console->LogMessage(error);
  }

  // Not clonable, try JSON
  // XXX This is ugly but currently structured cloning doesn't handle
  //    properly cases when interface is implemented in JS and used
  //    as a dictionary.
  nsAutoString json;
  NS_ENSURE_TRUE(nsContentUtils::StringifyJSON(aCx, &v, json), false);
  NS_ENSURE_TRUE(!json.IsEmpty(), false);

  JS::Rooted<JS::Value> val(aCx, JS::NullValue());
  NS_ENSURE_TRUE(JS_ParseJSON(aCx, static_cast<const char16_t*>(json.get()),
                              json.Length(), &val),
                 false);

  aData.Write(aCx, val, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    return false;
  }

  return true;
}

static bool sSendingSyncMessage = false;

static bool AllowMessage(size_t aDataLength, const nsAString& aMessageName) {
  // A message includes more than structured clone data, so subtract
  // 20KB to make it more likely that a message within this bound won't
  // result in an overly large IPC message.
  static const size_t kMaxMessageSize =
      IPC::Channel::kMaximumMessageSize - 20 * 1024;
  return aDataLength < kMaxMessageSize;
}

void nsFrameMessageManager::SendSyncMessage(JSContext* aCx,
                                            const nsAString& aMessageName,
                                            JS::Handle<JS::Value> aObj,
                                            nsTArray<JS::Value>& aResult,
                                            ErrorResult& aError) {
  NS_ASSERTION(!IsGlobal(), "Should not call SendSyncMessage in chrome");
  NS_ASSERTION(!IsBroadcaster(), "Should not call SendSyncMessage in chrome");
  NS_ASSERTION(!GetParentManager(),
               "Should not have parent manager in content!");

  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
      "nsFrameMessageManager::SendMessage", OTHER, aMessageName);

  if (sSendingSyncMessage) {
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
    MessageManagerFuzzer::TryMutate(aCx, aMessageName, &data,
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
  sSendingSyncMessage = true;
  bool ok = mCallback->DoSendBlockingMessage(aMessageName, data, &retval);
  sSendingSyncMessage = false;

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

nsresult nsFrameMessageManager::DispatchAsyncMessageInternal(
    JSContext* aCx, const nsAString& aMessage, StructuredCloneData& aData) {
  if (mIsBroadcaster) {
    uint32_t len = mChildManagers.Length();
    for (uint32_t i = 0; i < len; ++i) {
      mChildManagers[i]->DispatchAsyncMessageInternal(aCx, aMessage, aData);
    }
    return NS_OK;
  }

  if (!mCallback) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = mCallback->DoSendAsyncMessage(aMessage, aData);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

void nsFrameMessageManager::DispatchAsyncMessage(
    JSContext* aCx, const nsAString& aMessageName, JS::Handle<JS::Value> aObj,
    JS::Handle<JS::Value> aTransfers, ErrorResult& aError) {
  StructuredCloneData data;
  if (!aObj.isUndefined() &&
      !GetParamsForMessage(aCx, aObj, aTransfers, data)) {
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

  aError = DispatchAsyncMessageInternal(aCx, aMessageName, data);
}

class MMListenerRemover {
 public:
  explicit MMListenerRemover(nsFrameMessageManager* aMM)
      : mWasHandlingMessage(aMM->mHandlingMessage), mMM(aMM) {
    mMM->mHandlingMessage = true;
  }
  ~MMListenerRemover() {
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

void nsFrameMessageManager::ReceiveMessage(
    nsISupports* aTarget, nsFrameLoader* aTargetFrameLoader, bool aTargetClosed,
    const nsAString& aMessage, bool aIsSync, StructuredCloneData* aCloneData,
    nsTArray<StructuredCloneData>* aRetVal, ErrorResult& aError) {
  MOZ_ASSERT(aTarget);

  nsAutoTObserverArray<nsMessageListenerInfo, 1>* listeners =
      mListeners.Get(aMessage);
  if (listeners) {
    MMListenerRemover lr(this);

    nsAutoTObserverArray<nsMessageListenerInfo, 1>::EndLimitedIterator iter(
        *listeners);
    while (iter.HasMore()) {
      nsMessageListenerInfo& listener = iter.GetNext();
      // Remove mListeners[i] if it's an expired weak listener.
      nsCOMPtr<nsISupports> weakListener;
      if (listener.mWeakListener) {
        weakListener = do_QueryReferent(listener.mWeakListener);
        if (!weakListener) {
          iter.Remove();
          continue;
        }
      }

      if (!listener.mListenWhenClosed && aTargetClosed) {
        continue;
      }

      JS::RootingContext* rcx = RootingCx();
      JS::Rooted<JSObject*> object(rcx);
      JS::Rooted<JSObject*> objectGlobal(rcx);

      RefPtr<MessageListener> webIDLListener;
      if (!weakListener) {
        webIDLListener = listener.mStrongListener;
        object = webIDLListener->CallbackOrNull();
        objectGlobal = webIDLListener->CallbackGlobalOrNull();
      } else {
        nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS =
            do_QueryInterface(weakListener);
        if (!wrappedJS) {
          continue;
        }

        object = wrappedJS->GetJSObject();
        objectGlobal = wrappedJS->GetJSObjectGlobal();
      }

      if (!object) {
        continue;
      }

      AutoEntryScript aes(js::UncheckedUnwrap(object),
                          "message manager handler");
      JSContext* cx = aes.cx();

      // We passed the unwrapped object to AutoEntryScript so we now need to
      // enter the realm of the global object that represents the realm of our
      // callback.
      JSAutoRealm ar(cx, objectGlobal);

      RootedDictionary<ReceiveMessageArgument> argument(cx);

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
      argument.mSync = aIsSync;
      argument.mTarget = aTarget;
      if (aTargetFrameLoader) {
        argument.mTargetFrameLoader.Construct(*aTargetFrameLoader);
      }

      JS::Rooted<JS::Value> thisValue(cx, JS::UndefinedValue());

      if (JS::IsCallable(object)) {
        // A small hack to get 'this' value right on content side where
        // messageManager is wrapped in BrowserChildMessageManager's global.
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
          // At this point the call to ReceiveMessage will have reported any
          // exceptions (we kept the default of eReportExceptions). We suppress
          // the failure in the ErrorResult and continue.
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
            // Because the AutoEntryScript is inside the loop this continue will
            // make us report any exceptions (after which we'll move on to the
            // next listener).
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
          nsString msg =
              aMessage + nsLiteralString(
                             u": message reply cannot be cloned. Are "
                             "you trying to send an XPCOM object?");

          nsCOMPtr<nsIConsoleService> console(
              do_GetService(NS_CONSOLESERVICE_CONTRACTID));
          if (console) {
            nsCOMPtr<nsIScriptError> error(
                do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
            error->Init(msg, u""_ns, u""_ns, 0, 0, nsIScriptError::warningFlag,
                        "chrome javascript", false /* from private window */,
                        true /* from chrome context */);
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
    kungFuDeathGrip->ReceiveMessage(aTarget, aTargetFrameLoader, aTargetClosed,
                                    aMessage, aIsSync, aCloneData, aRetVal,
                                    aError);
  }
}

void nsFrameMessageManager::LoadPendingScripts(
    nsFrameMessageManager* aManager, nsFrameMessageManager* aChildMM) {
  // We have parent manager if we're a message broadcaster.
  // In that case we want to load the pending scripts from all parent
  // message managers in the hierarchy. Process the parent first so
  // that pending scripts higher up in the hierarchy are loaded before others.
  nsFrameMessageManager* parentManager = aManager->GetParentManager();
  if (parentManager) {
    LoadPendingScripts(parentManager, aChildMM);
  }

  for (uint32_t i = 0; i < aManager->mPendingScripts.Length(); ++i) {
    aChildMM->LoadScript(aManager->mPendingScripts[i], false,
                         aManager->mPendingScriptsGlobalStates[i],
                         IgnoreErrors());
  }
}

void nsFrameMessageManager::LoadPendingScripts() {
  RefPtr<nsFrameMessageManager> kungfuDeathGrip = this;
  LoadPendingScripts(this, this);
}

void nsFrameMessageManager::SetCallback(MessageManagerCallback* aCallback) {
  MOZ_ASSERT(!mIsBroadcaster || !mCallback,
             "Broadcasters cannot have callbacks!");
  if (aCallback && mCallback != aCallback) {
    mCallback = aCallback;
    if (mOwnsCallback) {
      mOwnedCallback = WrapUnique(aCallback);
    }
  }
}

void nsFrameMessageManager::Close() {
  if (!mClosed) {
    if (nsCOMPtr<nsIObserverService> obs =
            mozilla::services::GetObserverService()) {
      obs->NotifyWhenScriptSafe(this, "message-manager-close", nullptr);
    }
  }
  mClosed = true;
  mCallback = nullptr;
  mOwnedCallback = nullptr;
}

void nsFrameMessageManager::Disconnect(bool aRemoveFromParent) {
  // Notify message-manager-close if we haven't already.
  Close();

  if (!mDisconnected) {
    if (nsCOMPtr<nsIObserverService> obs =
            mozilla::services::GetObserverService()) {
      obs->NotifyWhenScriptSafe(this, "message-manager-disconnect", nullptr);
    }
  }

  ClearParentManager(aRemoveFromParent);

  mDisconnected = true;
  if (!mHandlingMessage) {
    mListeners.Clear();
  }
}

void nsFrameMessageManager::SetInitialProcessData(
    JS::HandleValue aInitialData) {
  MOZ_ASSERT(!mChrome);
  MOZ_ASSERT(mIsProcessManager);
  MOZ_ASSERT(aInitialData.isObject());
  mInitialProcessData = aInitialData;
}

void nsFrameMessageManager::GetInitialProcessData(
    JSContext* aCx, JS::MutableHandle<JS::Value> aInitialProcessData,
    ErrorResult& aError) {
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
    // This is the cpmm in the parent process. We should use the same object as
    // the ppmm. Create it first through do_GetService and use the cached
    // pointer in sParentProcessManager.
    nsCOMPtr<nsISupports> ppmm =
        do_GetService("@mozilla.org/parentprocessmessagemanager;1");
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

WritableSharedMap* nsFrameMessageManager::SharedData() {
  if (!mChrome || !mIsProcessManager) {
    MOZ_ASSERT(false, "Should only call this binding method on ppmm");
    return nullptr;
  }
  if (!mSharedData) {
    mSharedData = new WritableSharedMap();
  }
  return mSharedData;
}

already_AddRefed<ProcessMessageManager>
nsFrameMessageManager::GetProcessMessageManager(ErrorResult& aError) {
  RefPtr<ProcessMessageManager> pmm;
  if (mCallback) {
    pmm = mCallback->GetProcessMessageManager();
  }
  return pmm.forget();
}

void nsFrameMessageManager::GetRemoteType(nsACString& aRemoteType,
                                          ErrorResult& aError) const {
  aRemoteType.Truncate();
  if (mCallback) {
    mCallback->DoGetRemoteType(aRemoteType, aError);
  }
}

namespace {

struct MessageManagerReferentCount {
  MessageManagerReferentCount() : mStrong(0), mWeakAlive(0), mWeakDead(0) {}
  size_t mStrong;
  size_t mWeakAlive;
  size_t mWeakDead;
  nsTArray<nsString> mSuspectMessages;
  nsTHashMap<nsStringHashKey, uint32_t> mMessageCounter;
};

}  // namespace

namespace mozilla::dom {

class MessageManagerReporter final : public nsIMemoryReporter {
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

void MessageManagerReporter::CountReferents(
    nsFrameMessageManager* aMessageManager,
    MessageManagerReferentCount* aReferentCount) {
  for (const auto& entry : aMessageManager->mListeners) {
    nsAutoTObserverArray<nsMessageListenerInfo, 1>* listeners = entry.GetWeak();
    uint32_t listenerCount = listeners->Length();
    if (listenerCount == 0) {
      continue;
    }

    nsString key(entry.GetKey());
    const uint32_t currentCount =
        (aReferentCount->mMessageCounter.LookupOrInsert(key, 0) +=
         listenerCount);

    // Keep track of messages that have a suspiciously large
    // number of referents (symptom of leak).
    if (currentCount >= MessageManagerReporter::kSuspectReferentCount) {
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

static void ReportReferentCount(
    const char* aManagerType, const MessageManagerReferentCount& aReferentCount,
    nsIHandleReportCallback* aHandleReport, nsISupports* aData) {
#define REPORT(_path, _amount, _desc)                                       \
  do {                                                                      \
    aHandleReport->Callback(""_ns, _path, nsIMemoryReporter::KIND_OTHER,    \
                            nsIMemoryReporter::UNITS_COUNT, _amount, _desc, \
                            aData);                                         \
  } while (0)

  REPORT(nsPrintfCString("message-manager/referent/%s/strong", aManagerType),
         aReferentCount.mStrong,
         nsPrintfCString("The number of strong referents held by the message "
                         "manager in the %s manager.",
                         aManagerType));
  REPORT(
      nsPrintfCString("message-manager/referent/%s/weak/alive", aManagerType),
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
    const uint32_t totalReferentCount =
        aReferentCount.mMessageCounter.Get(aReferentCount.mSuspectMessages[i]);
    NS_ConvertUTF16toUTF8 suspect(aReferentCount.mSuspectMessages[i]);
    REPORT(nsPrintfCString("message-manager-suspect/%s/referent(message=%s)",
                           aManagerType, suspect.get()),
           totalReferentCount,
           nsPrintfCString("A message in the %s message manager with a "
                           "suspiciously large number of referents (symptom "
                           "of a leak).",
                           aManagerType));
  }

#undef REPORT
}

static StaticRefPtr<ChromeMessageBroadcaster> sGlobalMessageManager;

NS_IMETHODIMP
MessageManagerReporter::CollectReports(nsIHandleReportCallback* aHandleReport,
                                       nsISupports* aData, bool aAnonymize) {
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

}  // namespace mozilla::dom

already_AddRefed<ChromeMessageBroadcaster>
nsFrameMessageManager::GetGlobalMessageManager() {
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

nsresult NS_NewGlobalMessageManager(nsISupports** aResult) {
  *aResult = nsFrameMessageManager::GetGlobalMessageManager().take();
  return NS_OK;
}

nsTHashMap<nsStringHashKey, nsMessageManagerScriptHolder*>*
    nsMessageManagerScriptExecutor::sCachedScripts = nullptr;
StaticRefPtr<nsScriptCacheCleaner>
    nsMessageManagerScriptExecutor::sScriptCacheCleaner;

void nsMessageManagerScriptExecutor::DidCreateScriptLoader() {
  if (!sCachedScripts) {
    sCachedScripts =
        new nsTHashMap<nsStringHashKey, nsMessageManagerScriptHolder*>;
    sScriptCacheCleaner = new nsScriptCacheCleaner();
  }
}

// static
void nsMessageManagerScriptExecutor::PurgeCache() {
  if (sCachedScripts) {
    NS_ASSERTION(sCachedScripts != nullptr, "Need cached scripts");
    for (auto iter = sCachedScripts->Iter(); !iter.Done(); iter.Next()) {
      delete iter.Data();
      iter.Remove();
    }
  }
}

// static
void nsMessageManagerScriptExecutor::Shutdown() {
  if (sCachedScripts) {
    PurgeCache();

    delete sCachedScripts;
    sCachedScripts = nullptr;
    sScriptCacheCleaner = nullptr;
  }
}

static void FillCompileOptionsForCachedStencil(JS::CompileOptions& aOptions) {
  ScriptPreloader::FillCompileOptionsForCachedStencil(aOptions);
  aOptions.setNonSyntacticScope(true);
}

void nsMessageManagerScriptExecutor::LoadScriptInternal(
    JS::Handle<JSObject*> aMessageManager, const nsAString& aURL,
    bool aRunInUniqueScope) {
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
      "nsMessageManagerScriptExecutor::LoadScriptInternal", OTHER, aURL);

  if (!sCachedScripts) {
    return;
  }

  RefPtr<JS::Stencil> stencil;
  nsMessageManagerScriptHolder* holder = sCachedScripts->Get(aURL);
  if (holder) {
    stencil = holder->mStencil;
  } else {
    stencil =
        TryCacheLoadAndCompileScript(aURL, aRunInUniqueScope, aMessageManager);
  }

  AutoEntryScript aes(aMessageManager, "message manager script load");
  JSContext* cx = aes.cx();
  if (stencil) {
    JS::CompileOptions options(cx);
    FillCompileOptionsForCachedStencil(options);
    JS::InstantiateOptions instantiateOptions(options);
    JS::Rooted<JSScript*> script(
        cx, JS::InstantiateGlobalStencil(cx, instantiateOptions, stencil));

    if (script) {
      if (aRunInUniqueScope) {
        JS::Rooted<JSObject*> scope(cx);
        bool ok = js::ExecuteInFrameScriptEnvironment(cx, aMessageManager,
                                                      script, &scope);
        if (ok) {
          // Force the scope to stay alive.
          mAnonymousGlobalScopes.AppendElement(scope);
        }
      } else {
        JS::RootedValue rval(cx);
        JS::RootedVector<JSObject*> envChain(cx);
        if (!envChain.append(aMessageManager)) {
          return;
        }
        Unused << JS_ExecuteScript(cx, envChain, script, &rval);
      }
    }
  }
}

already_AddRefed<JS::Stencil>
nsMessageManagerScriptExecutor::TryCacheLoadAndCompileScript(
    const nsAString& aURL, bool aRunInUniqueScope,
    JS::Handle<JSObject*> aMessageManager) {
  nsCString url = NS_ConvertUTF16toUTF8(aURL);
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), url);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  bool hasFlags;
  rv = NS_URIChainHasFlags(uri, nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                           &hasFlags);
  if (NS_FAILED(rv) || !hasFlags) {
    NS_WARNING("Will not load a frame script!");
    return nullptr;
  }

  // If this script won't be cached, or there is only one of this type of
  // message manager per process, treat this script as run-once. Run-once
  // scripts can be compiled directly for the target global, and will be dropped
  // from the preloader cache after they're executed and serialized.
  //
  // NOTE: This does not affect the JS::CompileOptions. We generate the same
  // bytecode as though it were run multiple times. This is required for the
  // batch decoding from ScriptPreloader to work.
  bool isRunOnce = IsProcessScoped();

  // We don't cache data: scripts!
  nsAutoCString scheme;
  uri->GetScheme(scheme);
  bool isCacheable = !scheme.EqualsLiteral("data");
  bool useScriptPreloader = isCacheable;

  // If the script will be reused in this session, compile it in the compilation
  // scope instead of the current global to avoid keeping the current
  // compartment alive.
  AutoJSAPI jsapi;
  if (!jsapi.Init(isRunOnce ? aMessageManager : xpc::CompilationScope())) {
    return nullptr;
  }
  JSContext* cx = jsapi.cx();

  RefPtr<JS::Stencil> stencil;
  if (useScriptPreloader) {
    JS::DecodeOptions decodeOptions;
    ScriptPreloader::FillDecodeOptionsForCachedStencil(decodeOptions);
    stencil = ScriptPreloader::GetChildSingleton().GetCachedStencil(
        cx, decodeOptions, url);
  }

  if (!stencil) {
    nsCOMPtr<nsIChannel> channel;
    NS_NewChannel(getter_AddRefs(channel), uri,
                  nsContentUtils::GetSystemPrincipal(),
                  nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
                  nsIContentPolicy::TYPE_INTERNAL_FRAME_MESSAGEMANAGER_SCRIPT);

    if (!channel) {
      return nullptr;
    }

    nsCOMPtr<nsIInputStream> input;
    rv = channel->Open(getter_AddRefs(input));
    NS_ENSURE_SUCCESS(rv, nullptr);
    nsString dataString;
    char16_t* dataStringBuf = nullptr;
    size_t dataStringLength = 0;
    if (input) {
      nsCString buffer;
      uint64_t written;
      if (NS_FAILED(NS_ReadInputStreamToString(input, buffer, -1, &written))) {
        return nullptr;
      }

      uint32_t size = (uint32_t)std::min(written, (uint64_t)UINT32_MAX);
      ScriptLoader::ConvertToUTF16(channel, (uint8_t*)buffer.get(), size,
                                   u""_ns, nullptr, dataStringBuf,
                                   dataStringLength);
    }

    if (!dataStringBuf || dataStringLength == 0) {
      return nullptr;
    }

    JS::CompileOptions options(cx);
    FillCompileOptionsForCachedStencil(options);
    options.setFileAndLine(url.get(), 1);

    // If we are not encoding to the ScriptPreloader cache, we can now relax the
    // compile options and use the JS syntax-parser for lower latency.
    if (!useScriptPreloader || !ScriptPreloader::GetChildSingleton().Active()) {
      options.setSourceIsLazy(false);
    }

    JS::UniqueTwoByteChars srcChars(dataStringBuf);

    JS::SourceText<char16_t> srcBuf;
    if (!srcBuf.init(cx, std::move(srcChars), dataStringLength)) {
      return nullptr;
    }

    stencil = JS::CompileGlobalScriptToStencil(cx, options, srcBuf);
    if (!stencil) {
      return nullptr;
    }

    if (isCacheable && !isRunOnce) {
      // Store into our cache only when we compile it here.
      auto* holder = new nsMessageManagerScriptHolder(stencil);
      sCachedScripts->InsertOrUpdate(aURL, holder);
    }

#ifdef DEBUG
    // The above shouldn't touch any options for instantiation.
    JS::InstantiateOptions instantiateOptions(options);
    instantiateOptions.assertDefault();
#endif
  }

  MOZ_ASSERT(stencil);

  if (useScriptPreloader) {
    ScriptPreloader::GetChildSingleton().NoteStencil(url, url, stencil,
                                                     isRunOnce);
  }

  return stencil.forget();
}

void nsMessageManagerScriptExecutor::Trace(const TraceCallbacks& aCallbacks,
                                           void* aClosure) {
  for (size_t i = 0, length = mAnonymousGlobalScopes.Length(); i < length;
       ++i) {
    aCallbacks.Trace(&mAnonymousGlobalScopes[i], "mAnonymousGlobalScopes[i]",
                     aClosure);
  }
}

void nsMessageManagerScriptExecutor::Unlink() {
  ImplCycleCollectionUnlink(mAnonymousGlobalScopes);
}

bool nsMessageManagerScriptExecutor::Init() {
  DidCreateScriptLoader();
  return true;
}

void nsMessageManagerScriptExecutor::MarkScopesForCC() {
  for (uint32_t i = 0; i < mAnonymousGlobalScopes.Length(); ++i) {
    mAnonymousGlobalScopes[i].exposeToActiveJS();
  }
}

NS_IMPL_ISUPPORTS(nsScriptCacheCleaner, nsIObserver)

ChildProcessMessageManager* nsFrameMessageManager::sChildProcessManager =
    nullptr;
ParentProcessMessageManager* nsFrameMessageManager::sParentProcessManager =
    nullptr;
nsFrameMessageManager* nsFrameMessageManager::sSameProcessParentManager =
    nullptr;

class nsAsyncMessageToSameProcessChild : public nsSameProcessAsyncMessageBase,
                                         public Runnable {
 public:
  nsAsyncMessageToSameProcessChild()
      : nsSameProcessAsyncMessageBase(),
        mozilla::Runnable("nsAsyncMessageToSameProcessChild") {}
  NS_IMETHOD Run() override {
    nsFrameMessageManager* ppm =
        nsFrameMessageManager::GetChildProcessManager();
    ReceiveMessage(ppm, nullptr, ppm);
    return NS_OK;
  }
};

/**
 * Send messages to an imaginary child process in a single-process scenario.
 */
class SameParentProcessMessageManagerCallback : public MessageManagerCallback {
 public:
  SameParentProcessMessageManagerCallback() {
    MOZ_COUNT_CTOR(SameParentProcessMessageManagerCallback);
  }
  ~SameParentProcessMessageManagerCallback() override {
    MOZ_COUNT_DTOR(SameParentProcessMessageManagerCallback);
  }

  bool DoLoadMessageManagerScript(const nsAString& aURL,
                                  bool aRunInGlobalScope) override {
    auto* global = ContentProcessMessageManager::Get();
    MOZ_ASSERT(!aRunInGlobalScope);
    global->LoadScript(aURL);
    return true;
  }

  nsresult DoSendAsyncMessage(const nsAString& aMessage,
                              StructuredCloneData& aData) override {
    RefPtr<nsAsyncMessageToSameProcessChild> ev =
        new nsAsyncMessageToSameProcessChild();

    nsresult rv = ev->Init(aMessage, aData);
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
class ChildProcessMessageManagerCallback : public MessageManagerCallback {
 public:
  ChildProcessMessageManagerCallback() {
    MOZ_COUNT_CTOR(ChildProcessMessageManagerCallback);
  }
  ~ChildProcessMessageManagerCallback() override {
    MOZ_COUNT_DTOR(ChildProcessMessageManagerCallback);
  }

  bool DoSendBlockingMessage(const nsAString& aMessage,
                             StructuredCloneData& aData,
                             nsTArray<StructuredCloneData>* aRetVal) override {
    mozilla::dom::ContentChild* cc = mozilla::dom::ContentChild::GetSingleton();
    if (!cc) {
      return true;
    }
    ClonedMessageData data;
    if (!BuildClonedMessageDataForChild(cc, aData, data)) {
      return false;
    }
    return cc->SendSyncMessage(PromiseFlatString(aMessage), data, aRetVal);
  }

  nsresult DoSendAsyncMessage(const nsAString& aMessage,
                              StructuredCloneData& aData) override {
    mozilla::dom::ContentChild* cc = mozilla::dom::ContentChild::GetSingleton();
    if (!cc) {
      return NS_OK;
    }
    ClonedMessageData data;
    if (!BuildClonedMessageDataForChild(cc, aData, data)) {
      return NS_ERROR_DOM_DATA_CLONE_ERR;
    }
    if (!cc->SendAsyncMessage(PromiseFlatString(aMessage), data)) {
      return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
  }
};

class nsAsyncMessageToSameProcessParent
    : public nsSameProcessAsyncMessageBase,
      public SameProcessMessageQueue::Runnable {
 public:
  nsAsyncMessageToSameProcessParent() : nsSameProcessAsyncMessageBase() {}
  nsresult HandleMessage() override {
    nsFrameMessageManager* ppm =
        nsFrameMessageManager::sSameProcessParentManager;
    ReceiveMessage(ppm, nullptr, ppm);
    return NS_OK;
  }
};

/**
 * Send messages to the imaginary parent process in a single-process scenario.
 */
class SameChildProcessMessageManagerCallback : public MessageManagerCallback {
 public:
  SameChildProcessMessageManagerCallback() {
    MOZ_COUNT_CTOR(SameChildProcessMessageManagerCallback);
  }
  ~SameChildProcessMessageManagerCallback() override {
    MOZ_COUNT_DTOR(SameChildProcessMessageManagerCallback);
  }

  bool DoSendBlockingMessage(const nsAString& aMessage,
                             StructuredCloneData& aData,
                             nsTArray<StructuredCloneData>* aRetVal) override {
    SameProcessMessageQueue* queue = SameProcessMessageQueue::Get();
    queue->Flush();

    if (nsFrameMessageManager::sSameProcessParentManager) {
      RefPtr<nsFrameMessageManager> ppm =
          nsFrameMessageManager::sSameProcessParentManager;
      ppm->ReceiveMessage(ppm, nullptr, aMessage, true, &aData, aRetVal,
                          IgnoreErrors());
    }
    return true;
  }

  nsresult DoSendAsyncMessage(const nsAString& aMessage,
                              StructuredCloneData& aData) override {
    SameProcessMessageQueue* queue = SameProcessMessageQueue::Get();
    RefPtr<nsAsyncMessageToSameProcessParent> ev =
        new nsAsyncMessageToSameProcessParent();
    nsresult rv = ev->Init(aMessage, aData);

    if (NS_FAILED(rv)) {
      return rv;
    }
    queue->Push(ev);
    return NS_OK;
  }
};

// This creates the global parent process message manager.
nsresult NS_NewParentProcessMessageManager(nsISupports** aResult) {
  NS_ASSERTION(!nsFrameMessageManager::sParentProcessManager,
               "Re-creating sParentProcessManager");
  RefPtr<ParentProcessMessageManager> mm = new ParentProcessMessageManager();
  nsFrameMessageManager::sParentProcessManager = mm;
  nsFrameMessageManager::NewProcessMessageManager(
      false);  // Create same process message manager.
  mm.forget(aResult);
  return NS_OK;
}

ProcessMessageManager* nsFrameMessageManager::NewProcessMessageManager(
    bool aIsRemote) {
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
    mm = new ProcessMessageManager(
        nullptr, nsFrameMessageManager::sParentProcessManager);
  } else {
    mm =
        new ProcessMessageManager(new SameParentProcessMessageManagerCallback(),
                                  nsFrameMessageManager::sParentProcessManager,
                                  MessageManagerFlags::MM_OWNSCALLBACK);
    mm->SetOsPid(base::GetCurrentProcId());
    sSameProcessParentManager = mm;
  }
  return mm;
}

nsresult NS_NewChildProcessMessageManager(nsISupports** aResult) {
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
  auto global = MakeRefPtr<ContentProcessMessageManager>(mm);
  NS_ENSURE_TRUE(global->Init(), NS_ERROR_UNEXPECTED);
  return CallQueryInterface(global, aResult);
}

void nsFrameMessageManager::MarkForCC() {
  for (const auto& entry : mListeners) {
    nsAutoTObserverArray<nsMessageListenerInfo, 1>* listeners = entry.GetWeak();
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

nsSameProcessAsyncMessageBase::nsSameProcessAsyncMessageBase()
#ifdef DEBUG
    : mCalledInit(false)
#endif
{
}

nsresult nsSameProcessAsyncMessageBase::Init(const nsAString& aMessage,
                                             StructuredCloneData& aData) {
  if (!mData.Copy(aData)) {
    Telemetry::Accumulate(Telemetry::IPC_SAME_PROCESS_MESSAGE_COPY_OOM_KB,
                          aData.DataLength());
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mMessage = aMessage;
#ifdef DEBUG
  mCalledInit = true;
#endif

  return NS_OK;
}

void nsSameProcessAsyncMessageBase::ReceiveMessage(
    nsISupports* aTarget, nsFrameLoader* aTargetFrameLoader,
    nsFrameMessageManager* aManager) {
  // Make sure that we have called Init() and it has succeeded.
  MOZ_ASSERT(mCalledInit);
  if (aManager) {
    RefPtr<nsFrameMessageManager> mm = aManager;
    mm->ReceiveMessage(aTarget, aTargetFrameLoader, mMessage, false, &mData,
                       nullptr, IgnoreErrors());
  }
}
