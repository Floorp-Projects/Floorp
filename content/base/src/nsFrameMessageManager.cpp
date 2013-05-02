/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "nsFrameMessageManager.h"

#include "AppProcessChecker.h"
#include "ContentChild.h"
#include "ContentParent.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "nsJSUtils.h"
#include "nsJSPrincipals.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "nsFrameLoader.h"
#include "nsIXULRuntime.h"
#include "nsIScriptError.h"
#include "nsIConsoleService.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSRuntimeService.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMFile.h"
#include "xpcpublic.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/StructuredCloneUtils.h"
#include <algorithm>

#ifdef ANDROID
#include <android/log.h>
#endif
#ifdef XP_WIN
#include <windows.h>
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;


static bool
IsChromeProcess()
{
  nsCOMPtr<nsIXULRuntime> rt = do_GetService("@mozilla.org/xre/runtime;1");
  if (!rt)
    return true;

  uint32_t type;
  rt->GetProcessType(&type);
  return type == nsIXULRuntime::PROCESS_TYPE_DEFAULT;
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsFrameMessageManager)
  uint32_t count = tmp->mListeners.Length();
  for (uint32_t i = 0; i < count; i++) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mListeners[i] mListener");
    cb.NoteXPCOMChild(tmp->mListeners[i].mListener.get());
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChildManagers)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsFrameMessageManager)
  tmp->mListeners.Clear();
  for (int32_t i = tmp->mChildManagers.Count(); i > 0; --i) {
    static_cast<nsFrameMessageManager*>(tmp->mChildManagers[i - 1])->
      Disconnect(false);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChildManagers)
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
                       const StructuredCloneData& aData,
                       ClonedMessageData& aClonedData)
{
  SerializedStructuredCloneBuffer& buffer = aClonedData.data();
  buffer.data = aData.mData;
  buffer.dataLength = aData.mDataLength;
  const nsTArray<nsCOMPtr<nsIDOMBlob> >& blobs = aData.mClosure.mBlobs;
  if (!blobs.IsEmpty()) {
    typedef typename BlobTraits<Flavor>::ProtocolType ProtocolType;
    InfallibleTArray<ProtocolType*>& blobList = DataBlobs<Flavor>::Blobs(aClonedData);
    uint32_t length = blobs.Length();
    blobList.SetCapacity(length);
    for (uint32_t i = 0; i < length; ++i) {
      Blob<Flavor>* protocolActor = aManager->GetOrCreateActorForBlob(blobs[i]);
      if (!protocolActor) {
        return false;
      }
      blobList.AppendElement(protocolActor);
    }
  }
  return true;
}

bool
MessageManagerCallback::BuildClonedMessageDataForParent(ContentParent* aParent,
                                                        const StructuredCloneData& aData,
                                                        ClonedMessageData& aClonedData)
{
  return BuildClonedMessageData<Parent>(aParent, aData, aClonedData);
}

bool
MessageManagerCallback::BuildClonedMessageDataForChild(ContentChild* aChild,
                                                       const StructuredCloneData& aData,
                                                       ClonedMessageData& aClonedData)
{
  return BuildClonedMessageData<Child>(aChild, aData, aClonedData);
}

template<ActorFlavorEnum Flavor>
static StructuredCloneData
UnpackClonedMessageData(const ClonedMessageData& aData)
{
  const SerializedStructuredCloneBuffer& buffer = aData.data();
  typedef typename BlobTraits<Flavor>::ProtocolType ProtocolType;
  const InfallibleTArray<ProtocolType*>& blobs = DataBlobs<Flavor>::Blobs(aData);
  StructuredCloneData cloneData;
  cloneData.mData = buffer.data;
  cloneData.mDataLength = buffer.dataLength;
  if (!blobs.IsEmpty()) {
    uint32_t length = blobs.Length();
    cloneData.mClosure.mBlobs.SetCapacity(length);
    for (uint32_t i = 0; i < length; ++i) {
      Blob<Flavor>* blob = static_cast<Blob<Flavor>*>(blobs[i]);
      MOZ_ASSERT(blob);
      nsCOMPtr<nsIDOMBlob> domBlob = blob->GetBlob();
      MOZ_ASSERT(domBlob);
      cloneData.mClosure.mBlobs.AppendElement(domBlob);
    }
  }
  return cloneData;
}

StructuredCloneData
mozilla::dom::ipc::UnpackClonedMessageDataForParent(const ClonedMessageData& aData)
{
  return UnpackClonedMessageData<Parent>(aData);
}

StructuredCloneData
mozilla::dom::ipc::UnpackClonedMessageDataForChild(const ClonedMessageData& aData)
{
  return UnpackClonedMessageData<Child>(aData);
}

// nsIMessageListenerManager

NS_IMETHODIMP
nsFrameMessageManager::AddMessageListener(const nsAString& aMessage,
                                          nsIMessageListener* aListener)
{
  nsCOMPtr<nsIAtom> message = do_GetAtom(aMessage);
  uint32_t len = mListeners.Length();
  for (uint32_t i = 0; i < len; ++i) {
    if (mListeners[i].mMessage == message &&
      mListeners[i].mListener == aListener) {
      return NS_OK;
    }
  }
  nsMessageListenerInfo* entry = mListeners.AppendElement();
  NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);
  entry->mMessage = message;
  entry->mListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::RemoveMessageListener(const nsAString& aMessage,
                                             nsIMessageListener* aListener)
{
  nsCOMPtr<nsIAtom> message = do_GetAtom(aMessage);
  uint32_t len = mListeners.Length();
  for (uint32_t i = 0; i < len; ++i) {
    if (mListeners[i].mMessage == message &&
      mListeners[i].mListener == aListener) {
      mListeners.RemoveElementAt(i);
      return NS_OK;
    }
  }
  return NS_OK;
}

// nsIFrameScriptLoader

NS_IMETHODIMP
nsFrameMessageManager::LoadFrameScript(const nsAString& aURL,
                                       bool aAllowDelayedLoad)
{
  if (aAllowDelayedLoad) {
    if (IsGlobal() || IsWindowLevel()) {
      // Cache for future windows or frames
      mPendingScripts.AppendElement(aURL);
    } else if (!mCallback) {
      // We're frame message manager, which isn't connected yet.
      mPendingScripts.AppendElement(aURL);
      return NS_OK;
    }
  }

  if (mCallback) {
#ifdef DEBUG_smaug
    printf("Will load %s \n", NS_ConvertUTF16toUTF8(aURL).get());
#endif
    NS_ENSURE_TRUE(mCallback->DoLoadFrameScript(aURL), NS_ERROR_FAILURE);
  }

  for (int32_t i = 0; i < mChildManagers.Count(); ++i) {
    nsRefPtr<nsFrameMessageManager> mm =
      static_cast<nsFrameMessageManager*>(mChildManagers[i]);
    if (mm) {
      // Use false here, so that child managers don't cache the script, which
      // is already cached in the parent.
      mm->LoadFrameScript(aURL, false);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::RemoveDelayedFrameScript(const nsAString& aURL)
{
  mPendingScripts.RemoveElement(aURL);
  return NS_OK;
}

static JSBool
JSONCreator(const jschar* aBuf, uint32_t aLen, void* aData)
{
  nsAString* result = static_cast<nsAString*>(aData);
  result->Append(static_cast<const PRUnichar*>(aBuf),
                 static_cast<uint32_t>(aLen));
  return true;
}

static bool
GetParamsForMessage(JSContext* aCx,
                    const JS::Value& aObject,
                    JSAutoStructuredCloneBuffer& aBuffer,
                    StructuredCloneClosure& aClosure)
{
  if (WriteStructuredClone(aCx, aObject, aBuffer, aClosure)) {
    return true;
  }
  JS_ClearPendingException(aCx);

  // Not clonable, try JSON
  //XXX This is ugly but currently structured cloning doesn't handle
  //    properly cases when interface is implemented in JS and used
  //    as a dictionary.
  nsAutoString json;
  JSAutoRequest ar(aCx);
  JS::Value v = aObject;
  NS_ENSURE_TRUE(JS_Stringify(aCx, &v, nullptr, JSVAL_NULL, JSONCreator, &json), false);
  NS_ENSURE_TRUE(!json.IsEmpty(), false);

  JS::Value val = JSVAL_NULL;
  NS_ENSURE_TRUE(JS_ParseJSON(aCx, static_cast<const jschar*>(json.get()),
                              json.Length(), &val), false);

  return WriteStructuredClone(aCx, val, aBuffer, aClosure);
}


// nsISyncMessageSender

NS_IMETHODIMP
nsFrameMessageManager::SendSyncMessage(const nsAString& aMessageName,
                                       const JS::Value& aObject,
                                       JSContext* aCx,
                                       uint8_t aArgc,
                                       JS::Value* aRetval)
{
  NS_ASSERTION(!IsGlobal(), "Should not call SendSyncMessage in chrome");
  NS_ASSERTION(!IsWindowLevel(), "Should not call SendSyncMessage in chrome");
  NS_ASSERTION(!mParentManager, "Should not have parent manager in content!");

  *aRetval = JSVAL_VOID;
  NS_ENSURE_TRUE(mCallback, NS_ERROR_NOT_INITIALIZED);

  StructuredCloneData data;
  JSAutoStructuredCloneBuffer buffer;
  if (aArgc >= 2 &&
      !GetParamsForMessage(aCx, aObject, buffer, data.mClosure)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }
  data.mData = buffer.data();
  data.mDataLength = buffer.nbytes();

  InfallibleTArray<nsString> retval;
  if (mCallback->DoSendSyncMessage(aMessageName, data, &retval)) {
    JSAutoRequest ar(aCx);
    uint32_t len = retval.Length();
    JSObject* dataArray = JS_NewArrayObject(aCx, len, nullptr);
    NS_ENSURE_TRUE(dataArray, NS_ERROR_OUT_OF_MEMORY);

    for (uint32_t i = 0; i < len; ++i) {
      if (retval[i].IsEmpty()) {
        continue;
      }

      JS::Value ret = JSVAL_VOID;
      if (!JS_ParseJSON(aCx, static_cast<const jschar*>(retval[i].get()),
                        retval[i].Length(), &ret)) {
        return NS_ERROR_UNEXPECTED;
      }
      NS_ENSURE_TRUE(JS_SetElement(aCx, dataArray, i, &ret), NS_ERROR_OUT_OF_MEMORY);
    }

    *aRetval = OBJECT_TO_JSVAL(dataArray);
  }
  return NS_OK;
}

nsresult
nsFrameMessageManager::DispatchAsyncMessageInternal(const nsAString& aMessage,
                                                    const StructuredCloneData& aData)
{
  if (mIsBroadcaster) {
    int32_t len = mChildManagers.Count();
    for (int32_t i = 0; i < len; ++i) {
      static_cast<nsFrameMessageManager*>(mChildManagers[i])->
         DispatchAsyncMessageInternal(aMessage, aData);
    }
    return NS_OK;
  }

  NS_ENSURE_TRUE(mCallback, NS_ERROR_NOT_INITIALIZED);
  if (!mCallback->DoSendAsyncMessage(aMessage, aData)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
nsFrameMessageManager::DispatchAsyncMessage(const nsAString& aMessageName,
                                            const JS::Value& aObject,
                                            JSContext* aCx,
                                            uint8_t aArgc)
{
  StructuredCloneData data;
  JSAutoStructuredCloneBuffer buffer;

  if (aArgc >= 2 &&
      !GetParamsForMessage(aCx, aObject, buffer, data.mClosure)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  data.mData = buffer.data();
  data.mDataLength = buffer.nbytes();

  return DispatchAsyncMessageInternal(aMessageName, data);
}


// nsIMessageSender

NS_IMETHODIMP
nsFrameMessageManager::SendAsyncMessage(const nsAString& aMessageName,
                                        const JS::Value& aObject,
                                        JSContext* aCx,
                                        uint8_t aArgc)
{
  return DispatchAsyncMessage(aMessageName, aObject, aCx, aArgc);
}


// nsIMessageBroadcaster

NS_IMETHODIMP
nsFrameMessageManager::BroadcastAsyncMessage(const nsAString& aMessageName,
                                             const JS::Value& aObject,
                                             JSContext* aCx,
                                             uint8_t aArgc)
{
  return DispatchAsyncMessage(aMessageName, aObject, aCx, aArgc);
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
  return NS_ERROR_NOT_IMPLEMENTED;
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
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::Atob(const nsAString& aAsciiString,
                            nsAString& aBinaryData)
{
  return NS_OK;
}

// nsIProcessChecker

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

class MMListenerRemover
{
public:
  MMListenerRemover(nsFrameMessageManager* aMM)
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
                                      const nsAString& aMessage,
                                      bool aSync,
                                      const StructuredCloneData* aCloneData,
                                      JSObject* aObjectsArray,
                                      InfallibleTArray<nsString>* aJSONRetVal,
                                      JSContext* aContext)
{
  JSContext *cxToUse = mContext ? mContext
                                : (aContext ? aContext
                                            : nsContentUtils::GetSafeJSContext());
  AutoPushJSContext ctx(cxToUse);
  if (mListeners.Length()) {
    nsCOMPtr<nsIAtom> name = do_GetAtom(aMessage);
    MMListenerRemover lr(this);

    for (uint32_t i = 0; i < mListeners.Length(); ++i) {
      if (mListeners[i].mMessage == name) {
        nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS =
          do_QueryInterface(mListeners[i].mListener);
        if (!wrappedJS) {
          continue;
        }
        JSObject* object = nullptr;
        wrappedJS->GetJSObject(&object);
        if (!object) {
          continue;
        }
        nsCxPusher pusher;
        pusher.Push(ctx);

        JSAutoRequest ar(ctx);
        JSAutoCompartment ac(ctx, object);

        // The parameter for the listener function.
        JS::Rooted<JSObject*> param(ctx,
          JS_NewObject(ctx, nullptr, nullptr, nullptr));
        NS_ENSURE_TRUE(param, NS_ERROR_OUT_OF_MEMORY);

        JS::Value targetv;
        nsContentUtils::WrapNative(ctx,
                                   JS_GetGlobalForObject(ctx, object),
                                   aTarget, &targetv, nullptr, true);

        // To keep compatibility with e10s message manager,
        // define empty objects array.
        if (!aObjectsArray) {
          // Because we want JS messages to have always the same properties,
          // create array even if len == 0.
          aObjectsArray = JS_NewArrayObject(ctx, 0, nullptr);
          if (!aObjectsArray) {
            return NS_ERROR_OUT_OF_MEMORY;
          }
        }

        JS::Rooted<JS::Value> objectsv(ctx, JS::ObjectValue(*aObjectsArray));
        if (!JS_WrapValue(ctx, objectsv.address()))
            return NS_ERROR_UNEXPECTED;

        JS::Value json = JSVAL_NULL;
        if (aCloneData && aCloneData->mDataLength &&
            !ReadStructuredClone(ctx, *aCloneData, &json)) {
          JS_ClearPendingException(ctx);
          return NS_OK;
        }
        JSString* jsMessage =
          JS_NewUCStringCopyN(ctx,
                              static_cast<const jschar*>(aMessage.BeginReading()),
                              aMessage.Length());
        NS_ENSURE_TRUE(jsMessage, NS_ERROR_OUT_OF_MEMORY);
        JS_DefineProperty(ctx, param, "target", targetv, nullptr, nullptr, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "name",
                          STRING_TO_JSVAL(jsMessage), nullptr, nullptr, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "sync",
                          BOOLEAN_TO_JSVAL(aSync), nullptr, nullptr, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "json", json, nullptr, nullptr, JSPROP_ENUMERATE); // deprecated
        JS_DefineProperty(ctx, param, "data", json, nullptr, nullptr, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "objects", objectsv, nullptr, nullptr, JSPROP_ENUMERATE);

        JS::Rooted<JS::Value> thisValue(ctx, JSVAL_VOID);

        JS::Value funval;
        if (JS_ObjectIsCallable(ctx, object)) {
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
          nsContentUtils::WrapNative(ctx, JS_GetGlobalForObject(ctx, object),
                                     defaultThisValue, thisValue.address(),
                                     nullptr, true);
        } else {
          // If the listener is a JS object which has receiveMessage function:
          if (!JS_GetProperty(ctx, object, "receiveMessage", &funval) ||
              !funval.isObject())
            return NS_ERROR_UNEXPECTED;

          // Check if the object is even callable.
          NS_ENSURE_STATE(JS_ObjectIsCallable(ctx, &funval.toObject()));
          thisValue.setObject(*object);
        }

        JS::Rooted<JS::Value> rval(ctx, JSVAL_VOID);
        JS::Rooted<JS::Value> argv(ctx, JS::ObjectValue(*param));

        {
          JS::Rooted<JSObject*> thisObject(ctx, thisValue.toObjectOrNull());

          JSAutoCompartment tac(ctx, thisObject);
          if (!JS_WrapValue(ctx, argv.address()))
            return NS_ERROR_UNEXPECTED;

          JS_CallFunctionValue(ctx, thisObject,
                               funval, 1, argv.address(), rval.address());
          if (aJSONRetVal) {
            nsString json;
            if (JS_Stringify(ctx, rval.address(), nullptr, JSVAL_NULL,
                             JSONCreator, &json)) {
              aJSONRetVal->AppendElement(json);
            }
          }
        }
      }
    }
  }
  nsRefPtr<nsFrameMessageManager> kungfuDeathGrip = mParentManager;
  return mParentManager ? mParentManager->ReceiveMessage(aTarget, aMessage,
                                                         aSync, aCloneData,
                                                         aObjectsArray,
                                                         aJSONRetVal, mContext) : NS_OK;
}

void
nsFrameMessageManager::AddChildManager(nsFrameMessageManager* aManager,
                                       bool aLoadScripts)
{
  mChildManagers.AppendObject(aManager);
  if (aLoadScripts) {
    nsRefPtr<nsFrameMessageManager> kungfuDeathGrip = this;
    nsRefPtr<nsFrameMessageManager> kungfuDeathGrip2 = aManager;
    // We have parent manager if we're a window message manager.
    // In that case we want to load the pending scripts from global
    // message manager.
    if (mParentManager) {
      nsRefPtr<nsFrameMessageManager> globalMM = mParentManager;
      for (uint32_t i = 0; i < globalMM->mPendingScripts.Length(); ++i) {
        aManager->LoadFrameScript(globalMM->mPendingScripts[i], false);
      }
    }
    for (uint32_t i = 0; i < mPendingScripts.Length(); ++i) {
      aManager->LoadFrameScript(mPendingScripts[i], false);
    }
  }
}

void
nsFrameMessageManager::SetCallback(MessageManagerCallback* aCallback, bool aLoadScripts)
{
  NS_ASSERTION(!mIsBroadcaster || !mCallback,
               "Broadcasters cannot have callbacks!");
  if (aCallback && mCallback != aCallback) {
    mCallback = aCallback;
    if (mOwnsCallback) {
      mOwnedCallback = aCallback;
    }
    // First load global scripts by adding this to parent manager.
    if (mParentManager) {
      mParentManager->AddChildManager(this, aLoadScripts);
    }
    if (aLoadScripts) {
      for (uint32_t i = 0; i < mPendingScripts.Length(); ++i) {
        LoadFrameScript(mPendingScripts[i], false);
      }
    }
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
  mContext = nullptr;
}

void
nsFrameMessageManager::Disconnect(bool aRemoveFromParent)
{
  if (mParentManager && aRemoveFromParent) {
    mParentManager->RemoveChildManager(this);
  }
  mDisconnected = true;
  mParentManager = nullptr;
  mCallback = nullptr;
  mOwnedCallback = nullptr;
  mContext = nullptr;
  if (!mHandlingMessage) {
    mListeners.Clear();
  }
}

nsresult
NS_NewGlobalMessageManager(nsIMessageBroadcaster** aResult)
{
  NS_ENSURE_TRUE(IsChromeProcess(), NS_ERROR_NOT_AVAILABLE);
  nsFrameMessageManager* mm = new nsFrameMessageManager(nullptr,
                                                        nullptr,
                                                        nullptr,
                                                        MM_CHROME | MM_GLOBAL | MM_BROADCASTER);
  return CallQueryInterface(mm, aResult);
}

void
ContentScriptErrorReporter(JSContext* aCx,
                           const char* aMessage,
                           JSErrorReport* aReport)
{
  nsresult rv;
  nsCOMPtr<nsIScriptError> scriptError =
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return;
  }
  nsAutoString message, filename, line;
  uint32_t lineNumber, columnNumber, flags, errorNumber;

  if (aReport) {
    if (aReport->ucmessage) {
      message.Assign(static_cast<const PRUnichar*>(aReport->ucmessage));
    }
    filename.AssignWithConversion(aReport->filename);
    line.Assign(static_cast<const PRUnichar*>(aReport->uclinebuf));
    lineNumber = aReport->lineno;
    columnNumber = aReport->uctokenptr - aReport->uclinebuf;
    flags = aReport->flags;
    errorNumber = aReport->errorNumber;
  } else {
    lineNumber = columnNumber = errorNumber = 0;
    flags = nsIScriptError::errorFlag | nsIScriptError::exceptionFlag;
  }

  if (message.IsEmpty()) {
    message.AssignWithConversion(aMessage);
  }

  rv = scriptError->Init(message, filename, line,
                         lineNumber, columnNumber, flags,
                         "Message manager content script");
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIConsoleService> consoleService =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (consoleService) {
    (void) consoleService->LogMessage(scriptError);
  }

#ifdef DEBUG
  // Print it to stderr as well, for the benefit of those invoking
  // mozilla with -console.
  nsAutoCString error;
  error.Assign("JavaScript ");
  if (JSREPORT_IS_STRICT(flags)) {
    error.Append("strict ");
  }
  if (JSREPORT_IS_WARNING(flags)) {
    error.Append("warning: ");
  } else {
    error.Append("error: ");
  }
  error.Append(aReport->filename);
  error.Append(", line ");
  error.AppendInt(lineNumber, 10);
  error.Append(": ");
  if (aReport->ucmessage) {
    AppendUTF16toUTF8(static_cast<const PRUnichar*>(aReport->ucmessage),
                      error);
  } else {
    error.Append(aMessage);
  }

  fprintf(stderr, "%s\n", error.get());
  fflush(stderr);
#endif
}

nsDataHashtable<nsStringHashKey, nsFrameJSScriptExecutorHolder*>*
  nsFrameScriptExecutor::sCachedScripts = nullptr;
nsScriptCacheCleaner* nsFrameScriptExecutor::sScriptCacheCleaner = nullptr;

void
nsFrameScriptExecutor::DidCreateCx()
{
  NS_ASSERTION(mCx, "Should have mCx!");
  if (!sCachedScripts) {
    sCachedScripts =
      new nsDataHashtable<nsStringHashKey, nsFrameJSScriptExecutorHolder*>;
    sCachedScripts->Init();

    nsRefPtr<nsScriptCacheCleaner> scriptCacheCleaner =
      new nsScriptCacheCleaner();
    scriptCacheCleaner.forget(&sScriptCacheCleaner);
  }
}

void
nsFrameScriptExecutor::DestroyCx()
{
  if (mCxStackRefCnt) {
    mDelayedCxDestroy = true;
    return;
  }
  mDelayedCxDestroy = false;
  if (mCx) {
    nsIXPConnect* xpc = nsContentUtils::XPConnect();
    if (xpc) {
      xpc->ReleaseJSContext(mCx, true);
    } else {
      JS_DestroyContext(mCx);
    }
  }
  mCx = nullptr;
  mGlobal = nullptr;
}

static PLDHashOperator
CachedScriptUnrooter(const nsAString& aKey,
                       nsFrameJSScriptExecutorHolder*& aData,
                       void* aUserArg)
{
  JSContext* cx = static_cast<JSContext*>(aUserArg);
  JS_RemoveScriptRoot(cx, &(aData->mScript));
  delete aData;
  return PL_DHASH_REMOVE;
}

// static
void
nsFrameScriptExecutor::Shutdown()
{
  if (sCachedScripts) {
    SafeAutoJSContext cx;
    JSAutoRequest ar(cx);
    NS_ASSERTION(sCachedScripts != nullptr, "Need cached scripts");
    sCachedScripts->Enumerate(CachedScriptUnrooter, cx);

    delete sCachedScripts;
    sCachedScripts = nullptr;

    nsRefPtr<nsScriptCacheCleaner> scriptCacheCleaner;
    scriptCacheCleaner.swap(sScriptCacheCleaner);
  }
}

void
nsFrameScriptExecutor::LoadFrameScriptInternal(const nsAString& aURL)
{
  if (!mGlobal || !mCx || !sCachedScripts) {
    return;
  }

  nsFrameJSScriptExecutorHolder* holder = sCachedScripts->Get(aURL);
  if (!holder) {
    TryCacheLoadAndCompileScript(aURL, EXECUTE_IF_CANT_CACHE);
    holder = sCachedScripts->Get(aURL);
  }

  if (holder) {
    nsCxPusher pusher;
    pusher.Push(mCx);
    {
      // Need to scope JSAutoRequest to happen after Push but before Pop,
      // at least for now. See bug 584673.
      JSAutoRequest ar(mCx);
      JSObject* global = nullptr;
      mGlobal->GetJSObject(&global);
      if (global) {
        (void) JS_ExecuteScript(mCx, global, holder->mScript, nullptr);
      }
    }
  }
}

void
nsFrameScriptExecutor::TryCacheLoadAndCompileScript(const nsAString& aURL,
                                                    CacheFailedBehavior aBehavior)
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
  NS_NewChannel(getter_AddRefs(channel), uri);
  if (!channel) {
    return;
  }

  nsCOMPtr<nsIInputStream> input;
  channel->Open(getter_AddRefs(input));
  nsString dataString;
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
                                   EmptyString(), nullptr, dataString);
  }

  if (!dataString.IsEmpty()) {
    nsCxPusher pusher;
    pusher.Push(mCx);
    {
      // Need to scope JSAutoRequest to happen after Push but before Pop,
      // at least for now. See bug 584673.
      JSAutoRequest ar(mCx);
      JSObject* global = nullptr;
      mGlobal->GetJSObject(&global);
      if (global) {
        JSAutoCompartment ac(mCx, global);
        JS::CompileOptions options(mCx);
        options.setNoScriptRval(true)
               .setFileAndLine(url.get(), 1)
               .setPrincipals(nsJSPrincipals::get(mPrincipal));
        JS::RootedObject empty(mCx, nullptr);
        JS::Rooted<JSScript*> script(mCx,
          JS::Compile(mCx, empty, options, dataString.get(),
                      dataString.Length()));

        if (script) {
          nsAutoCString scheme;
          uri->GetScheme(scheme);
          // We don't cache data: scripts!
          if (!scheme.EqualsLiteral("data")) {
            nsFrameJSScriptExecutorHolder* holder =
              new nsFrameJSScriptExecutorHolder(script);
            // Root the object also for caching.
            JS_AddNamedScriptRoot(mCx, &(holder->mScript),
                                  "Cached message manager script");
            sCachedScripts->Put(aURL, holder);
          } else if (aBehavior == EXECUTE_IF_CANT_CACHE) {
            (void) JS_ExecuteScript(mCx, global, script, nullptr);
          }
        }
      }
    }
  }
}

bool
nsFrameScriptExecutor::InitTabChildGlobalInternal(nsISupports* aScope,
                                                  const nsACString& aID)
{

  nsCOMPtr<nsIJSRuntimeService> runtimeSvc =
    do_GetService("@mozilla.org/js/xpc/RuntimeService;1");
  NS_ENSURE_TRUE(runtimeSvc, false);

  JSRuntime* rt = nullptr;
  runtimeSvc->GetRuntime(&rt);
  NS_ENSURE_TRUE(rt, false);

  JSContext* cx = JS_NewContext(rt, 8192);
  NS_ENSURE_TRUE(cx, false);

  mCx = cx;

  nsContentUtils::GetSecurityManager()->GetSystemPrincipal(getter_AddRefs(mPrincipal));

  JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_PRIVATE_IS_NSISUPPORTS);
  JS_SetVersion(cx, JSVERSION_LATEST);
  JS_SetErrorReporter(cx, ContentScriptErrorReporter);

  JSAutoRequest ar(cx);
  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  const uint32_t flags = nsIXPConnect::INIT_JS_STANDARD_CLASSES;

  
  JS_SetContextPrivate(cx, aScope);

  nsresult rv =
    xpc->InitClassesWithNewWrappedGlobal(cx, aScope, mPrincipal,
                                         flags, JS::SystemZone, getter_AddRefs(mGlobal));
  NS_ENSURE_SUCCESS(rv, false);

    
  JSObject* global = nullptr;
  rv = mGlobal->GetJSObject(&global);
  NS_ENSURE_SUCCESS(rv, false);

  JS_SetGlobalObject(cx, global);

  // Set the location information for the new global, so that tools like
  // about:memory may use that information.
  xpc::SetLocationForGlobal(global, aID);

  DidCreateCx();
  return true;
}

// static
void
nsFrameScriptExecutor::Traverse(nsFrameScriptExecutor *tmp,
                                nsCycleCollectionTraversalCallback &cb)
{
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  if (xpc) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mCx");
    xpc->NoteJSContext(tmp->mCx, cb);
  }
}

// static
void
nsFrameScriptExecutor::Unlink(nsFrameScriptExecutor* aTmp)
{
  if (aTmp->mCx) {
    JSAutoRequest ar(aTmp->mCx);
    JS_SetGlobalObject(aTmp->mCx, nullptr);
  }
}

NS_IMPL_ISUPPORTS1(nsScriptCacheCleaner, nsIObserver)

nsFrameMessageManager* nsFrameMessageManager::sChildProcessManager = nullptr;
nsFrameMessageManager* nsFrameMessageManager::sParentProcessManager = nullptr;
nsFrameMessageManager* nsFrameMessageManager::sSameProcessParentManager = nullptr;
nsTArray<nsCOMPtr<nsIRunnable> >* nsFrameMessageManager::sPendingSameProcessAsyncMessages = nullptr;


class nsAsyncMessageToSameProcessChild : public nsRunnable
{
public:
  nsAsyncMessageToSameProcessChild(const nsAString& aMessage,
                                   const StructuredCloneData& aData)
    : mMessage(aMessage)
  {
    if (aData.mDataLength && !mData.copy(aData.mData, aData.mDataLength)) {
      NS_RUNTIMEABORT("OOM");
    }
    mClosure = aData.mClosure;
  }

  NS_IMETHOD Run()
  {
    if (nsFrameMessageManager::sChildProcessManager) {
      StructuredCloneData data;
      data.mData = mData.data();
      data.mDataLength = mData.nbytes();
      data.mClosure = mClosure;

      nsRefPtr<nsFrameMessageManager> ppm = nsFrameMessageManager::sChildProcessManager;
      ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()), mMessage,
                          false, &data, nullptr, nullptr, nullptr);
    }
    return NS_OK;
  }
  nsString mMessage;
  JSAutoStructuredCloneBuffer mData;
  StructuredCloneClosure mClosure;
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

  virtual bool DoSendAsyncMessage(const nsAString& aMessage,
                                  const StructuredCloneData& aData)
  {
    nsRefPtr<nsIRunnable> ev =
      new nsAsyncMessageToSameProcessChild(aMessage, aData);
    NS_DispatchToCurrentThread(ev);
    return true;
  }

  bool CheckPermission(const nsAString& aPermission)
  {
    // In a single-process scenario, the child always has all capabilities.
    return true;
  }

  bool CheckManifestURL(const nsAString& aManifestURL)
  {
    // In a single-process scenario, the child always has all capabilities.
    return true;
  }

  bool CheckAppHasPermission(const nsAString& aPermission)
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

  virtual bool DoSendSyncMessage(const nsAString& aMessage,
                                 const mozilla::dom::StructuredCloneData& aData,
                                 InfallibleTArray<nsString>* aJSONRetVal)
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
    return cc->SendSyncMessage(nsString(aMessage), data, aJSONRetVal);
  }

  virtual bool DoSendAsyncMessage(const nsAString& aMessage,
                                  const mozilla::dom::StructuredCloneData& aData)
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
    return cc->SendAsyncMessage(nsString(aMessage), data);
  }

};


class nsAsyncMessageToSameProcessParent : public nsRunnable
{
public:
  nsAsyncMessageToSameProcessParent(const nsAString& aMessage,
                                    const StructuredCloneData& aData)
    : mMessage(aMessage)
  {
    if (aData.mDataLength && !mData.copy(aData.mData, aData.mDataLength)) {
      NS_RUNTIMEABORT("OOM");
    }
    mClosure = aData.mClosure;
  }

  NS_IMETHOD Run()
  {
    if (nsFrameMessageManager::sPendingSameProcessAsyncMessages) {
      nsFrameMessageManager::sPendingSameProcessAsyncMessages->RemoveElement(this);
    }
    if (nsFrameMessageManager::sSameProcessParentManager) {
      StructuredCloneData data;
      data.mData = mData.data();
      data.mDataLength = mData.nbytes();
      data.mClosure = mClosure;

      nsRefPtr<nsFrameMessageManager> ppm =
        nsFrameMessageManager::sSameProcessParentManager;
      ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()),
                          mMessage, false, &data, nullptr, nullptr, nullptr);
     }
     return NS_OK;
  }
  nsString mMessage;
  JSAutoStructuredCloneBuffer mData;
  StructuredCloneClosure mClosure;
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

  virtual bool DoSendSyncMessage(const nsAString& aMessage,
                                 const mozilla::dom::StructuredCloneData& aData,
                                 InfallibleTArray<nsString>* aJSONRetVal)
  {
    nsTArray<nsCOMPtr<nsIRunnable> > asyncMessages;
    if (nsFrameMessageManager::sPendingSameProcessAsyncMessages) {
      asyncMessages.SwapElements(*nsFrameMessageManager::sPendingSameProcessAsyncMessages);
      uint32_t len = asyncMessages.Length();
      for (uint32_t i = 0; i < len; ++i) {
        nsCOMPtr<nsIRunnable> async = asyncMessages[i];
        async->Run();
      }
    }
    if (nsFrameMessageManager::sSameProcessParentManager) {
      nsRefPtr<nsFrameMessageManager> ppm = nsFrameMessageManager::sSameProcessParentManager;
      ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()), aMessage,
                          true, &aData, nullptr, aJSONRetVal);
    }
    return true;
  }

  virtual bool DoSendAsyncMessage(const nsAString& aMessage,
                                  const mozilla::dom::StructuredCloneData& aData)
  {
    if (!nsFrameMessageManager::sPendingSameProcessAsyncMessages) {
      nsFrameMessageManager::sPendingSameProcessAsyncMessages = new nsTArray<nsCOMPtr<nsIRunnable> >;
    }
    nsCOMPtr<nsIRunnable> ev =
      new nsAsyncMessageToSameProcessParent(aMessage, aData);
    nsFrameMessageManager::sPendingSameProcessAsyncMessages->AppendElement(ev);
    NS_DispatchToCurrentThread(ev);
    return true;
  }

};


// This creates the global parent process message manager.
nsresult
NS_NewParentProcessMessageManager(nsIMessageBroadcaster** aResult)
{
  NS_ASSERTION(!nsFrameMessageManager::sParentProcessManager,
               "Re-creating sParentProcessManager");
  NS_ENSURE_TRUE(IsChromeProcess(), NS_ERROR_NOT_AVAILABLE);
  nsRefPtr<nsFrameMessageManager> mm = new nsFrameMessageManager(nullptr,
                                                                 nullptr,
                                                                 nullptr,
                                                                 MM_CHROME | MM_PROCESSMANAGER | MM_BROADCASTER);
  NS_ENSURE_TRUE(mm, NS_ERROR_OUT_OF_MEMORY);
  nsFrameMessageManager::sParentProcessManager = mm;
  nsFrameMessageManager::NewProcessMessageManager(nullptr); // Create same process message manager.
  return CallQueryInterface(mm, aResult);
}


nsFrameMessageManager*
nsFrameMessageManager::NewProcessMessageManager(mozilla::dom::ContentParent* aProcess)
{
  if (!nsFrameMessageManager::sParentProcessManager) {
     nsCOMPtr<nsIMessageBroadcaster> dummy;
     NS_NewParentProcessMessageManager(getter_AddRefs(dummy));
  }

  nsFrameMessageManager* mm;
  if (aProcess) {
    mm = new nsFrameMessageManager(aProcess,
                                   nsFrameMessageManager::sParentProcessManager,
                                   nullptr,
                                   MM_CHROME | MM_PROCESSMANAGER);
  } else {
    mm = new nsFrameMessageManager(new SameParentProcessMessageManagerCallback(),
                                   nsFrameMessageManager::sParentProcessManager,
                                   nullptr,
                                   MM_CHROME | MM_PROCESSMANAGER | MM_OWNSCALLBACK);
    sSameProcessParentManager = mm;
  }
  return mm;
}

nsresult
NS_NewChildProcessMessageManager(nsISyncMessageSender** aResult)
{
  NS_ASSERTION(!nsFrameMessageManager::sChildProcessManager,
               "Re-creating sChildProcessManager");

  MessageManagerCallback* cb;
  if (IsChromeProcess()) {
    cb = new SameChildProcessMessageManagerCallback();
  } else {
    cb = new ChildProcessMessageManagerCallback();
  }
  nsFrameMessageManager* mm = new nsFrameMessageManager(cb,
                                                        nullptr,
                                                        nullptr,
                                                        MM_PROCESSMANAGER | MM_OWNSCALLBACK);
  NS_ENSURE_TRUE(mm, NS_ERROR_OUT_OF_MEMORY);
  nsFrameMessageManager::sChildProcessManager = mm;
  return CallQueryInterface(mm, aResult);
}

bool
nsFrameMessageManager::MarkForCC()
{
  uint32_t len = mListeners.Length();
  for (uint32_t i = 0; i < len; ++i) {
    xpc_TryUnmarkWrappedGrayObject(mListeners[i].mListener);
  }
  if (mRefCnt.IsPurple()) {
    mRefCnt.RemovePurple();
  }
  return true;
}
