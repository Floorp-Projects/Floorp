/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "nsFrameMessageManager.h"

#include "ContentChild.h"
#include "ContentParent.h"
#include "nsContentUtils.h"
#include "nsDOMError.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "nsJSUtils.h"
#include "nsJSPrincipals.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "nsIJSContextStack.h"
#include "nsIXULRuntime.h"
#include "nsIScriptError.h"
#include "nsIConsoleService.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSRuntimeService.h"
#include "nsIDOMFile.h"
#include "xpcpublic.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/StructuredCloneUtils.h"

#ifdef ANDROID
#include <android/log.h>
#endif
#ifdef XP_WIN
#include <windows.h>
#endif

using namespace mozilla;
using namespace mozilla::dom;

static bool
IsChromeProcess()
{
  nsCOMPtr<nsIXULRuntime> rt = do_GetService("@mozilla.org/xre/runtime;1");
  if (!rt)
    return true;

  PRUint32 type;
  rt->GetProcessType(&type);
  return type == nsIXULRuntime::PROCESS_TYPE_DEFAULT;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsFrameMessageManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsFrameMessageManager)
  PRUint32 count = tmp->mListeners.Length();
  for (PRUint32 i = 0; i < count; i++) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mListeners[i] mListener");
    cb.NoteXPCOMChild(tmp->mListeners[i].mListener.get());
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mChildManagers)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsFrameMessageManager)
  tmp->mListeners.Clear();
  for (PRInt32 i = tmp->mChildManagers.Count(); i > 0; --i) {
    static_cast<nsFrameMessageManager*>(tmp->mChildManagers[i - 1])->
      Disconnect(false);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mChildManagers)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY_AGGREGATED(nsIFrameMessageManager,
                                    (mChrome ?
                                       static_cast<nsIFrameMessageManager*>(
                                         static_cast<nsIChromeFrameMessageManager*>(this)) :
                                       static_cast<nsIFrameMessageManager*>(
                                         static_cast<nsIContentFrameMessageManager*>(this))))
  /* nsIContentFrameMessageManager is accessible only in TabChildGlobal. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIContentFrameMessageManager,
                                     !mChrome && !mIsProcessManager)
  /* Message managers in child process support nsISyncMessageSender. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISyncMessageSender, !mChrome)
  /* Message managers in chrome process support nsITreeItemFrameMessageManager. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsITreeItemFrameMessageManager, mChrome)
  /* Process message manager doesn't support nsIChromeFrameMessageManager. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIChromeFrameMessageManager,
                                     mChrome && !mIsProcessManager)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFrameMessageManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFrameMessageManager)

NS_IMETHODIMP
nsFrameMessageManager::AddMessageListener(const nsAString& aMessage,
                                          nsIFrameMessageListener* aListener)
{
  nsCOMPtr<nsIAtom> message = do_GetAtom(aMessage);
  PRUint32 len = mListeners.Length();
  for (PRUint32 i = 0; i < len; ++i) {
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
                                             nsIFrameMessageListener* aListener)
{
  nsCOMPtr<nsIAtom> message = do_GetAtom(aMessage);
  PRUint32 len = mListeners.Length();
  for (PRUint32 i = 0; i < len; ++i) {
    if (mListeners[i].mMessage == message &&
      mListeners[i].mListener == aListener) {
      mListeners.RemoveElementAt(i);
      return NS_OK;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::LoadFrameScript(const nsAString& aURL,
                                       bool aAllowDelayedLoad)
{
  if (aAllowDelayedLoad) {
    if (IsGlobal() || IsWindowLevel()) {
      // Cache for future windows or frames
      mPendingScripts.AppendElement(aURL);
    } else if (!mCallbackData) {
      // We're frame message manager, which isn't connected yet.
      mPendingScripts.AppendElement(aURL);
      return NS_OK;
    }
  }

  if (mCallbackData) {
#ifdef DEBUG_smaug
    printf("Will load %s \n", NS_ConvertUTF16toUTF8(aURL).get());
#endif
    NS_ENSURE_TRUE(mLoadScriptCallback(mCallbackData, aURL), NS_ERROR_FAILURE);
  }

  for (PRInt32 i = 0; i < mChildManagers.Count(); ++i) {
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
                 static_cast<PRUint32>(aLen));
  return true;
}

static bool
GetParamsForMessage(JSContext* aCx,
                    const jsval& aObject,
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
  jsval v = aObject;
  NS_ENSURE_TRUE(JS_Stringify(aCx, &v, nullptr, JSVAL_NULL, JSONCreator, &json), false);
  NS_ENSURE_TRUE(!json.IsEmpty(), false);

  jsval val = JSVAL_NULL;
  NS_ENSURE_TRUE(JS_ParseJSON(aCx, static_cast<const jschar*>(PromiseFlatString(json).get()),
                              json.Length(), &val), false);

  return WriteStructuredClone(aCx, val, aBuffer, aClosure);
}

NS_IMETHODIMP
nsFrameMessageManager::SendSyncMessage(const nsAString& aMessageName,
                                       const jsval& aObject,
                                       JSContext* aCx,
                                       PRUint8 aArgc,
                                       jsval* aRetval)
{
  NS_ASSERTION(!IsGlobal(), "Should not call SendSyncMessage in chrome");
  NS_ASSERTION(!IsWindowLevel(), "Should not call SendSyncMessage in chrome");
  NS_ASSERTION(!mParentManager, "Should not have parent manager in content!");
  *aRetval = JSVAL_VOID;
  if (mSyncCallback) {
    NS_ENSURE_TRUE(mCallbackData, NS_ERROR_NOT_INITIALIZED);
    StructuredCloneData data;
    JSAutoStructuredCloneBuffer buffer;
    if (aArgc >= 2 &&
        !GetParamsForMessage(aCx, aObject, buffer, data.mClosure)) {
      return NS_ERROR_DOM_DATA_CLONE_ERR;
    }
    data.mData = buffer.data();
    data.mDataLength = buffer.nbytes();

    InfallibleTArray<nsString> retval;
    if (mSyncCallback(mCallbackData, aMessageName, data, &retval)) {
      JSAutoRequest ar(aCx);
      PRUint32 len = retval.Length();
      JSObject* dataArray = JS_NewArrayObject(aCx, len, NULL);
      NS_ENSURE_TRUE(dataArray, NS_ERROR_OUT_OF_MEMORY);

      for (PRUint32 i = 0; i < len; ++i) {
        if (retval[i].IsEmpty()) {
          continue;
        }

        jsval ret = JSVAL_VOID;
        if (!JS_ParseJSON(aCx, static_cast<const jschar*>(retval[i].get()),
                          retval[i].Length(), &ret)) {
          return NS_ERROR_UNEXPECTED;
        }
        NS_ENSURE_TRUE(JS_SetElement(aCx, dataArray, i, &ret), NS_ERROR_OUT_OF_MEMORY);
      }

      *aRetval = OBJECT_TO_JSVAL(dataArray);
    }
  }
  return NS_OK;
}

nsresult
nsFrameMessageManager::SendAsyncMessageInternal(const nsAString& aMessage,
                                                const StructuredCloneData& aData)
{
  if (mAsyncCallback) {
    NS_ENSURE_TRUE(mCallbackData, NS_ERROR_NOT_INITIALIZED);
    mAsyncCallback(mCallbackData, aMessage, aData);
  }
  PRInt32 len = mChildManagers.Count();
  for (PRInt32 i = 0; i < len; ++i) {
    static_cast<nsFrameMessageManager*>(mChildManagers[i])->
      SendAsyncMessageInternal(aMessage, aData);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::SendAsyncMessage(const nsAString& aMessageName,
                                        const jsval& aObject,
                                        JSContext* aCx,
                                        PRUint8 aArgc)
{
  StructuredCloneData data;
  JSAutoStructuredCloneBuffer buffer;

  if (aArgc >= 2 &&
      !GetParamsForMessage(aCx, aObject, buffer, data.mClosure)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  data.mData = buffer.data();
  data.mDataLength = buffer.nbytes();

  return SendAsyncMessageInternal(aMessageName, data);
}

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
nsFrameMessageManager::GetChildCount(PRUint32* aChildCount)
{
  *aChildCount = static_cast<PRUint32>(mChildManagers.Count()); 
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::GetChildAt(PRUint32 aIndex, 
                                  nsITreeItemFrameMessageManager** aMM)
{
  *aMM = nullptr;
  nsCOMPtr<nsITreeItemFrameMessageManager> mm =
    do_QueryInterface(mChildManagers.SafeObjectAt(static_cast<PRUint32>(aIndex)));
  mm.swap(*aMM);
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

nsresult
nsFrameMessageManager::ReceiveMessage(nsISupports* aTarget,
                                      const nsAString& aMessage,
                                      bool aSync,
                                      const StructuredCloneData* aCloneData,
                                      JSObject* aObjectsArray,
                                      InfallibleTArray<nsString>* aJSONRetVal,
                                      JSContext* aContext)
{
  JSContext* ctx = mContext ? mContext : aContext;
  if (!ctx) {
    ctx = nsContentUtils::ThreadJSContextStack()->GetSafeJSContext();
  }
  if (mListeners.Length()) {
    nsCOMPtr<nsIAtom> name = do_GetAtom(aMessage);
    MMListenerRemover lr(this);

    for (PRUint32 i = 0; i < mListeners.Length(); ++i) {
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
        NS_ENSURE_STATE(pusher.Push(ctx, false));

        JSAutoRequest ar(ctx);

        JSAutoEnterCompartment ac;
        if (!ac.enter(ctx, object))
          return NS_ERROR_FAILURE;

        // The parameter for the listener function.
        JSObject* param = JS_NewObject(ctx, NULL, NULL, NULL);
        NS_ENSURE_TRUE(param, NS_ERROR_OUT_OF_MEMORY);

        jsval targetv;
        nsContentUtils::WrapNative(ctx,
                                   JS_GetGlobalForObject(ctx, object),
                                   aTarget, &targetv, nullptr, true);

        // To keep compatibility with e10s message manager,
        // define empty objects array.
        if (!aObjectsArray) {
          // Because we want JS messages to have always the same properties,
          // create array even if len == 0.
          aObjectsArray = JS_NewArrayObject(ctx, 0, NULL);
          if (!aObjectsArray) {
            return NS_ERROR_OUT_OF_MEMORY;
          }
        }

        JS::AutoValueRooter objectsv(ctx);
        objectsv.set(OBJECT_TO_JSVAL(aObjectsArray));
        if (!JS_WrapValue(ctx, objectsv.jsval_addr()))
            return NS_ERROR_UNEXPECTED;

        jsval json = JSVAL_NULL;
        if (aCloneData && aCloneData->mDataLength &&
            !ReadStructuredClone(ctx, *aCloneData, &json)) {
          JS_ClearPendingException(ctx);
          return NS_OK;
        }
        JSString* jsMessage =
          JS_NewUCStringCopyN(ctx,
                              static_cast<const jschar*>(PromiseFlatString(aMessage).get()),
                              aMessage.Length());
        NS_ENSURE_TRUE(jsMessage, NS_ERROR_OUT_OF_MEMORY);
        JS_DefineProperty(ctx, param, "target", targetv, NULL, NULL, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "name",
                          STRING_TO_JSVAL(jsMessage), NULL, NULL, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "sync",
                          BOOLEAN_TO_JSVAL(aSync), NULL, NULL, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "json", json, NULL, NULL, JSPROP_ENUMERATE); // deprecated
        JS_DefineProperty(ctx, param, "data", json, NULL, NULL, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "objects", objectsv.jsval_value(), NULL, NULL, JSPROP_ENUMERATE);

        jsval thisValue = JSVAL_VOID;

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
          nsContentUtils::WrapNative(ctx,
                                     JS_GetGlobalForObject(ctx, object),
                                     defaultThisValue, &thisValue, nullptr, true);
        } else {
          // If the listener is a JS object which has receiveMessage function:
          if (!JS_GetProperty(ctx, object, "receiveMessage", &funval) ||
              !funval.isObject())
            return NS_ERROR_UNEXPECTED;

          // Check if the object is even callable.
          NS_ENSURE_STATE(JS_ObjectIsCallable(ctx, &funval.toObject()));
          thisValue.setObject(*object);
        }

        jsval rval = JSVAL_VOID;

        JS::AutoValueRooter argv(ctx);
        argv.set(OBJECT_TO_JSVAL(param));

        {
          JSAutoEnterCompartment tac;

          JSObject* thisObject = JSVAL_TO_OBJECT(thisValue);

          if (!tac.enter(ctx, thisObject) ||
              !JS_WrapValue(ctx, argv.jsval_addr()))
            return NS_ERROR_UNEXPECTED;

          JS_CallFunctionValue(ctx, thisObject,
                               funval, 1, argv.jsval_addr(), &rval);
          if (aJSONRetVal) {
            nsString json;
            if (JS_Stringify(ctx, &rval, nullptr, JSVAL_NULL,
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
      for (PRUint32 i = 0; i < globalMM->mPendingScripts.Length(); ++i) {
        aManager->LoadFrameScript(globalMM->mPendingScripts[i], false);
      }
    }
    for (PRUint32 i = 0; i < mPendingScripts.Length(); ++i) {
      aManager->LoadFrameScript(mPendingScripts[i], false);
    }
  }
}

void
nsFrameMessageManager::SetCallbackData(void* aData, bool aLoadScripts)
{
  if (aData && mCallbackData != aData) {
    mCallbackData = aData;
    // First load global scripts by adding this to parent manager.
    if (mParentManager) {
      mParentManager->AddChildManager(this, aLoadScripts);
    }
    if (aLoadScripts) {
      for (PRUint32 i = 0; i < mPendingScripts.Length(); ++i) {
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
  mCallbackData = nullptr;
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
  mCallbackData = nullptr;
  mContext = nullptr;
  if (!mHandlingMessage) {
    mListeners.Clear();
  }
}

nsresult
NS_NewGlobalMessageManager(nsIChromeFrameMessageManager** aResult)
{
  NS_ENSURE_TRUE(IsChromeProcess(), NS_ERROR_NOT_AVAILABLE);
  nsFrameMessageManager* mm = new nsFrameMessageManager(true,
                                                        nullptr,
                                                        nullptr,
                                                        nullptr,
                                                        nullptr,
                                                        nullptr,
                                                        nullptr,
                                                        true);
  NS_ENSURE_TRUE(mm, NS_ERROR_OUT_OF_MEMORY);
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
  PRUint32 lineNumber, columnNumber, flags, errorNumber;

  if (aReport) {
    if (aReport->ucmessage) {
      message.Assign(reinterpret_cast<const PRUnichar*>(aReport->ucmessage));
    }
    filename.AssignWithConversion(aReport->filename);
    line.Assign(reinterpret_cast<const PRUnichar*>(aReport->uclinebuf));
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

  rv = scriptError->Init(message.get(), filename.get(), line.get(),
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
  nsCAutoString error;
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
    AppendUTF16toUTF8(reinterpret_cast<const PRUnichar*>(aReport->ucmessage),
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
nsRefPtr<nsScriptCacheCleaner> nsFrameScriptExecutor::sScriptCacheCleaner;

void
nsFrameScriptExecutor::DidCreateCx()
{
  NS_ASSERTION(mCx, "Should have mCx!");
  if (!sCachedScripts) {
    sCachedScripts =
      new nsDataHashtable<nsStringHashKey, nsFrameJSScriptExecutorHolder*>;
    sCachedScripts->Init();

    sScriptCacheCleaner = new nsScriptCacheCleaner();
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
    JSContext* cx = nsContentUtils::ThreadJSContextStack()->GetSafeJSContext();
    if (cx) {
#ifdef DEBUG_smaug
      printf("Will clear cached frame manager scripts!\n");
#endif
      JSAutoRequest ar(cx);
      NS_ASSERTION(sCachedScripts != nullptr, "Need cached scripts");
      sCachedScripts->Enumerate(CachedScriptUnrooter, cx);
    } else {
      NS_WARNING("No context available. Leaking cached scripts!\n");
    }

    delete sCachedScripts;
    sCachedScripts = nullptr;

    sScriptCacheCleaner = nullptr;
  }
}

void
nsFrameScriptExecutor::LoadFrameScriptInternal(const nsAString& aURL)
{
  if (!mGlobal || !mCx || !sCachedScripts) {
    return;
  }

  nsFrameJSScriptExecutorHolder* holder = sCachedScripts->Get(aURL);
  if (holder) {
    nsContentUtils::ThreadJSContextStack()->Push(mCx);
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
    JSContext* unused;
    nsContentUtils::ThreadJSContextStack()->Pop(&unused);
    return;
  }

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
  PRUint32 avail = 0;
  if (input && NS_SUCCEEDED(input->Available(&avail)) && avail) {
    nsCString buffer;
    if (NS_FAILED(NS_ReadInputStreamToString(input, buffer, avail))) {
      return;
    }
    nsScriptLoader::ConvertToUTF16(channel, (PRUint8*)buffer.get(), avail,
                                   EmptyString(), nullptr, dataString);
  }

  if (!dataString.IsEmpty()) {
    nsContentUtils::ThreadJSContextStack()->Push(mCx);
    {
      // Need to scope JSAutoRequest to happen after Push but before Pop,
      // at least for now. See bug 584673.
      JSAutoRequest ar(mCx);
      JSObject* global = nullptr;
      mGlobal->GetJSObject(&global);
      JSAutoEnterCompartment ac;
      if (global && ac.enter(mCx, global)) {
        uint32_t oldopts = JS_GetOptions(mCx);
        JS_SetOptions(mCx, oldopts | JSOPTION_NO_SCRIPT_RVAL);

        JSScript* script =
          JS_CompileUCScriptForPrincipals(mCx, nullptr,
                                          nsJSPrincipals::get(mPrincipal),
                                          static_cast<const jschar*>(dataString.get()),
                                          dataString.Length(),
                                          url.get(), 1);

        JS_SetOptions(mCx, oldopts);

        if (script) {
          nsCAutoString scheme;
          uri->GetScheme(scheme);
          // We don't cache data: scripts!
          if (!scheme.EqualsLiteral("data")) {
            nsFrameJSScriptExecutorHolder* holder =
              new nsFrameJSScriptExecutorHolder(script);
            // Root the object also for caching.
            JS_AddNamedScriptRoot(mCx, &(holder->mScript),
                                  "Cached message manager script");
            sCachedScripts->Put(aURL, holder);
          }
          (void) JS_ExecuteScript(mCx, global, script, nullptr);
        }
      }
    } 
    JSContext* unused;
    nsContentUtils::ThreadJSContextStack()->Pop(&unused);
  }
}

bool
nsFrameScriptExecutor::InitTabChildGlobalInternal(nsISupports* aScope)
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

  bool allowXML = Preferences::GetBool("javascript.options.xml.chrome");
  JS_SetOptions(cx, JS_GetOptions(cx) |
                    JSOPTION_PRIVATE_IS_NSISUPPORTS |
                    (allowXML ? JSOPTION_ALLOW_XML : 0));
  JS_SetVersion(cx, JSVERSION_LATEST);
  JS_SetErrorReporter(cx, ContentScriptErrorReporter);

  xpc_LocalizeContext(cx);

  JSAutoRequest ar(cx);
  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  const PRUint32 flags = nsIXPConnect::INIT_JS_STANDARD_CLASSES |
                         nsIXPConnect::FLAG_SYSTEM_GLOBAL_OBJECT;

  
  JS_SetContextPrivate(cx, aScope);

  nsresult rv =
    xpc->InitClassesWithNewWrappedGlobal(cx, aScope, mPrincipal,
                                         flags, getter_AddRefs(mGlobal));
  NS_ENSURE_SUCCESS(rv, false);

    
  JSObject* global = nullptr;
  rv = mGlobal->GetJSObject(&global);
  NS_ENSURE_SUCCESS(rv, false);

  JS_SetGlobalObject(cx, global);
  DidCreateCx();
  return true;
}

// static
void
nsFrameScriptExecutor::Traverse(nsFrameScriptExecutor *tmp,
                                nsCycleCollectionTraversalCallback &cb)
{
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mGlobal)
  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  if (xpc) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mCx");
    xpc->NoteJSContext(tmp->mCx, cb);
  }
}

NS_IMPL_ISUPPORTS1(nsScriptCacheCleaner, nsIObserver)

nsFrameMessageManager* nsFrameMessageManager::sChildProcessManager = nullptr;
nsFrameMessageManager* nsFrameMessageManager::sParentProcessManager = nullptr;
nsFrameMessageManager* nsFrameMessageManager::sSameProcessParentManager = nullptr;
nsTArray<nsCOMPtr<nsIRunnable> >* nsFrameMessageManager::sPendingSameProcessAsyncMessages = nullptr;

bool SendAsyncMessageToChildProcess(void* aCallbackData,
                                    const nsAString& aMessage,
                                    const StructuredCloneData& aData)
{
  mozilla::dom::ContentParent* cp =
    static_cast<mozilla::dom::ContentParent*>(aCallbackData);
  NS_WARN_IF_FALSE(cp, "No child process!");
  if (cp) {
    ClonedMessageData data;
    SerializedStructuredCloneBuffer& buffer = data.data();
    buffer.data = aData.mData;
    buffer.dataLength = aData.mDataLength;
    const nsTArray<nsCOMPtr<nsIDOMBlob> >& blobs = aData.mClosure.mBlobs;
    if (!blobs.IsEmpty()) {
      InfallibleTArray<PBlobParent*>& blobParents = data.blobsParent();
      PRUint32 length = blobs.Length();
      blobParents.SetCapacity(length);
      for (PRUint32 i = 0; i < length; ++i) {
        BlobParent* blobParent = cp->GetOrCreateActorForBlob(blobs[i]);
        if (!blobParent) {
          return false;
  }
        blobParents.AppendElement(blobParent);
      }
    }

    return cp->SendAsyncMessage(nsString(aMessage), data);
  }
  return true;
}

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

bool SendAsyncMessageToSameProcessChild(void* aCallbackData,
                                        const nsAString& aMessage,
                                        const StructuredCloneData& aData)
{
  nsRefPtr<nsIRunnable> ev =
    new nsAsyncMessageToSameProcessChild(aMessage, aData);
  NS_DispatchToCurrentThread(ev);
  return true;
}

bool SendSyncMessageToParentProcess(void* aCallbackData,
                                    const nsAString& aMessage,
                                    const StructuredCloneData& aData,
                                    InfallibleTArray<nsString>* aJSONRetVal)
{
  mozilla::dom::ContentChild* cc =
    mozilla::dom::ContentChild::GetSingleton();
  if (cc) {
    ClonedMessageData data;
    SerializedStructuredCloneBuffer& buffer = data.data();
    buffer.data = aData.mData;
    buffer.dataLength = aData.mDataLength;
    const nsTArray<nsCOMPtr<nsIDOMBlob> >& blobs = aData.mClosure.mBlobs;
    if (!blobs.IsEmpty()) {
      InfallibleTArray<PBlobChild*>& blobChildList = data.blobsChild();
      PRUint32 length = blobs.Length();
      blobChildList.SetCapacity(length);
      for (PRUint32 i = 0; i < length; ++i) {
        BlobChild* blobChild = cc->GetOrCreateActorForBlob(blobs[i]);
        if (!blobChild) {
          return false;
        }
        blobChildList.AppendElement(blobChild);
      }
    }
    return
      cc->SendSyncMessage(nsString(aMessage), data, aJSONRetVal);
  }
  return true;
}

bool SendSyncMessageToSameProcessParent(void* aCallbackData,
                                        const nsAString& aMessage,
                                        const StructuredCloneData& aData,
                                        InfallibleTArray<nsString>* aJSONRetVal)
{
  nsTArray<nsCOMPtr<nsIRunnable> > asyncMessages;
  if (nsFrameMessageManager::sPendingSameProcessAsyncMessages) {
    asyncMessages.SwapElements(*nsFrameMessageManager::sPendingSameProcessAsyncMessages);
    PRUint32 len = asyncMessages.Length();
    for (PRUint32 i = 0; i < len; ++i) {
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

bool SendAsyncMessageToParentProcess(void* aCallbackData,
                                     const nsAString& aMessage,
                                          const StructuredCloneData& aData)
{
  mozilla::dom::ContentChild* cc =
    mozilla::dom::ContentChild::GetSingleton();
  if (cc) {
    ClonedMessageData data;
    SerializedStructuredCloneBuffer& buffer = data.data();
    buffer.data = aData.mData;
    buffer.dataLength = aData.mDataLength;
    const nsTArray<nsCOMPtr<nsIDOMBlob> >& blobs = aData.mClosure.mBlobs;
    if (!blobs.IsEmpty()) {
      InfallibleTArray<PBlobChild*>& blobChildList = data.blobsChild();
      PRUint32 length = blobs.Length();
      blobChildList.SetCapacity(length);
      for (PRUint32 i = 0; i < length; ++i) {
        BlobChild* blobChild = cc->GetOrCreateActorForBlob(blobs[i]);
        if (!blobChild) {
          return false;
        }
        blobChildList.AppendElement(blobChild);
      }
    }
    return cc->SendAsyncMessage(nsString(aMessage), data);
  }
  return true;
}

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

bool SendAsyncMessageToSameProcessParent(void* aCallbackData,
                                              const nsAString& aMessage,
                                              const StructuredCloneData& aData)
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

// This creates the global parent process message manager.
nsresult
NS_NewParentProcessMessageManager(nsIFrameMessageManager** aResult)
{
  NS_ASSERTION(!nsFrameMessageManager::sParentProcessManager,
               "Re-creating sParentProcessManager");
  NS_ENSURE_TRUE(IsChromeProcess(), NS_ERROR_NOT_AVAILABLE);
  nsRefPtr<nsFrameMessageManager> mm = new nsFrameMessageManager(true,
                                                                 nullptr,
                                                                 nullptr,
                                                                 nullptr,
                                                                 nullptr,
                                                                 nullptr,
                                                                 nullptr,
                                                                 false,
                                                                 true);
  NS_ENSURE_TRUE(mm, NS_ERROR_OUT_OF_MEMORY);
  nsFrameMessageManager::sParentProcessManager = mm;
  nsFrameMessageManager::NewProcessMessageManager(nullptr); // Create same process message manager.
  return CallQueryInterface(mm, aResult);
}

nsFrameMessageManager*
nsFrameMessageManager::NewProcessMessageManager(mozilla::dom::ContentParent* aProcess)
{
  if (!nsFrameMessageManager::sParentProcessManager) {
     nsCOMPtr<nsIFrameMessageManager> dummy;
     NS_NewParentProcessMessageManager(getter_AddRefs(dummy));
  }

  nsFrameMessageManager* mm = new nsFrameMessageManager(true,
                                                        nullptr,
                                                        aProcess ? SendAsyncMessageToChildProcess
                                                                 : SendAsyncMessageToSameProcessChild,
                                                        nullptr,
                                                        aProcess ? static_cast<void*>(aProcess)
                                                                 : static_cast<void*>(&nsFrameMessageManager::sChildProcessManager),
                                                        nsFrameMessageManager::sParentProcessManager,
                                                        nullptr,
                                                        false,
                                                        true);
  if (!aProcess) {
    sSameProcessParentManager = mm;
  }
  return mm;
}

nsresult
NS_NewChildProcessMessageManager(nsISyncMessageSender** aResult)
{
  NS_ASSERTION(!nsFrameMessageManager::sChildProcessManager,
               "Re-creating sChildProcessManager");
  bool isChrome = IsChromeProcess();
  nsFrameMessageManager* mm = new nsFrameMessageManager(false,
                                                        isChrome ? SendSyncMessageToSameProcessParent
                                                                 : SendSyncMessageToParentProcess,
                                                        isChrome ? SendAsyncMessageToSameProcessParent
                                                                 : SendAsyncMessageToParentProcess,
                                                        nullptr,
                                                        &nsFrameMessageManager::sChildProcessManager,
                                                        nullptr,
                                                        nullptr,
                                                        false,
                                                        true);
  NS_ENSURE_TRUE(mm, NS_ERROR_OUT_OF_MEMORY);
  nsFrameMessageManager::sChildProcessManager = mm;
  return CallQueryInterface(mm, aResult);
}

bool
nsFrameMessageManager::MarkForCC()
{
  PRUint32 len = mListeners.Length();
  for (PRUint32 i = 0; i < len; ++i) {
    xpc_TryUnmarkWrappedGrayObject(mListeners[i].mListener);
  }
  return true;
}
