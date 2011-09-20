/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "ContentChild.h"
#include "ContentParent.h"
#include "jscntxt.h"
#include "nsFrameMessageManager.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "jsinterp.h"
#include "nsJSUtils.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "nsIJSContextStack.h"
#include "nsIXULRuntime.h"
#include "nsIScriptError.h"
#include "nsIConsoleService.h"
#include "nsIProtocolHandler.h"

static PRBool
IsChromeProcess()
{
  nsCOMPtr<nsIXULRuntime> rt = do_GetService("@mozilla.org/xre/runtime;1");
  if (!rt)
    return PR_TRUE;

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
      Disconnect(PR_FALSE);
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
                                       PRBool aAllowDelayedLoad)
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
      // Use PR_FALSE here, so that child managers don't cache the script, which
      // is already cached in the parent.
      mm->LoadFrameScript(aURL, PR_FALSE);
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
JSONCreator(const jschar* aBuf, uint32 aLen, void* aData)
{
  nsAString* result = static_cast<nsAString*>(aData);
  result->Append((PRUnichar*)aBuf, (PRUint32)aLen);
  return JS_TRUE;
}

nsresult
nsFrameMessageManager::GetParamsForMessage(nsAString& aMessageName,
                                           nsAString& aJSON)
{
  aMessageName.Truncate();
  aJSON.Truncate();
  nsAXPCNativeCallContext* ncc = nsnull;
  nsresult rv = nsContentUtils::XPConnect()->GetCurrentNativeCallContext(&ncc);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_STATE(ncc);

  JSContext* ctx = nsnull;
  rv = ncc->GetJSContext(&ctx);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 argc;
  jsval* argv = nsnull;
  ncc->GetArgc(&argc);
  ncc->GetArgvPtr(&argv);

  JSAutoRequest ar(ctx);
  JSString* str;
  if (argc && (str = JS_ValueToString(ctx, argv[0])) && str) {
    nsDependentJSString depStr;
    if (!depStr.init(ctx, str)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aMessageName.Assign(depStr);
  }

  if (argc >= 2) {
    jsval v = argv[1];
    JS_Stringify(ctx, &v, nsnull, JSVAL_NULL, JSONCreator, &aJSON);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::SendSyncMessage()
{
  NS_ASSERTION(!IsGlobal(), "Should not call SendSyncMessage in chrome");
  NS_ASSERTION(!IsWindowLevel(), "Should not call SendSyncMessage in chrome");
  NS_ASSERTION(!mParentManager, "Should not have parent manager in content!");
  if (mSyncCallback) {
    NS_ENSURE_TRUE(mCallbackData, NS_ERROR_NOT_INITIALIZED);
    nsString messageName;
    nsString json;
    nsresult rv = GetParamsForMessage(messageName, json);
    NS_ENSURE_SUCCESS(rv, rv);
    InfallibleTArray<nsString> retval;
    if (mSyncCallback(mCallbackData, messageName, json, &retval)) {
      nsAXPCNativeCallContext* ncc = nsnull;
      rv = nsContentUtils::XPConnect()->GetCurrentNativeCallContext(&ncc);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_STATE(ncc);

      JSContext* ctx = nsnull;
      rv = ncc->GetJSContext(&ctx);
      NS_ENSURE_SUCCESS(rv, rv);
      JSAutoRequest ar(ctx);

      PRUint32 len = retval.Length();
      JSObject* dataArray = JS_NewArrayObject(ctx, len, NULL);
      NS_ENSURE_TRUE(dataArray, NS_ERROR_OUT_OF_MEMORY);

      for (PRUint32 i = 0; i < len; ++i) {
        if (retval[i].IsEmpty())
          continue;

        jsval ret = JSVAL_VOID;
        if (!JS_ParseJSON(ctx, (jschar*)retval[i].get(),
                          (uint32)retval[i].Length(), &ret)) {
          return NS_ERROR_UNEXPECTED;
        }
        NS_ENSURE_TRUE(JS_SetElement(ctx, dataArray, i, &ret), NS_ERROR_OUT_OF_MEMORY);
      }

      jsval* retvalPtr;
      ncc->GetRetValPtr(&retvalPtr);
      *retvalPtr = OBJECT_TO_JSVAL(dataArray);
      ncc->SetReturnValueWasSet(PR_TRUE);
    }
  }
  return NS_OK;
}

nsresult
nsFrameMessageManager::SendAsyncMessageInternal(const nsAString& aMessage,
                                                const nsAString& aJSON)
{
  if (mAsyncCallback) {
    NS_ENSURE_TRUE(mCallbackData, NS_ERROR_NOT_INITIALIZED);
    mAsyncCallback(mCallbackData, aMessage, aJSON);
  }
  PRInt32 len = mChildManagers.Count();
  for (PRInt32 i = 0; i < len; ++i) {
    static_cast<nsFrameMessageManager*>(mChildManagers[i])->
      SendAsyncMessageInternal(aMessage, aJSON);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::SendAsyncMessage()
{
  nsString messageName;
  nsString json;
  nsresult rv = GetParamsForMessage(messageName, json);
  NS_ENSURE_SUCCESS(rv, rv);
  return SendAsyncMessageInternal(messageName, json);
}

NS_IMETHODIMP
nsFrameMessageManager::Dump(const nsAString& aStr)
{
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
  *aContent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameMessageManager::GetDocShell(nsIDocShell** aDocShell)
{
  *aDocShell = nsnull;
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
  *aMM = nsnull;
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

nsresult
nsFrameMessageManager::ReceiveMessage(nsISupports* aTarget,
                                      const nsAString& aMessage,
                                      PRBool aSync, const nsAString& aJSON,
                                      JSObject* aObjectsArray,
                                      InfallibleTArray<nsString>* aJSONRetVal,
                                      JSContext* aContext)
{
  JSContext* ctx = mContext ? mContext : aContext;
  if (!ctx) {
    nsContentUtils::ThreadJSContextStack()->GetSafeJSContext(&ctx);
  }
  if (mListeners.Length()) {
    nsCOMPtr<nsIAtom> name = do_GetAtom(aMessage);
    nsRefPtr<nsFrameMessageManager> kungfuDeathGrip(this);

    for (PRUint32 i = 0; i < mListeners.Length(); ++i) {
      if (mListeners[i].mMessage == name) {
        nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS =
          do_QueryInterface(mListeners[i].mListener);
        if (!wrappedJS) {
          continue;
        }
        JSObject* object = nsnull;
        wrappedJS->GetJSObject(&object);
        if (!object) {
          continue;
        }
        nsCxPusher pusher;
        NS_ENSURE_STATE(pusher.Push(ctx, PR_FALSE));

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
                                   aTarget, &targetv, nsnull, PR_TRUE);

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

        js::AutoValueRooter objectsv(ctx);
        objectsv.set(OBJECT_TO_JSVAL(aObjectsArray));
        if (!JS_WrapValue(ctx, objectsv.jsval_addr()))
            return NS_ERROR_UNEXPECTED;

        jsval json = JSVAL_NULL;
        if (!aJSON.IsEmpty()) {
          if (!JS_ParseJSON(ctx, (jschar*)nsString(aJSON).get(),
                            (uint32)aJSON.Length(), &json)) {
            json = JSVAL_NULL;
          }
        }
        JSString* jsMessage =
          JS_NewUCStringCopyN(ctx,
                              reinterpret_cast<const jschar *>(nsString(aMessage).get()),
                              aMessage.Length());
        NS_ENSURE_TRUE(jsMessage, NS_ERROR_OUT_OF_MEMORY);
        JS_DefineProperty(ctx, param, "target", targetv, NULL, NULL, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "name",
                          STRING_TO_JSVAL(jsMessage), NULL, NULL, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "sync",
                          BOOLEAN_TO_JSVAL(aSync), NULL, NULL, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "json", json, NULL, NULL, JSPROP_ENUMERATE);
        JS_DefineProperty(ctx, param, "objects", objectsv.jsval_value(), NULL, NULL, JSPROP_ENUMERATE);

        jsval thisValue = JSVAL_VOID;

        jsval funval = JSVAL_VOID;
        if (JS_ObjectIsCallable(ctx, object)) {
          // If the listener is a JS function:
          funval = OBJECT_TO_JSVAL(object);

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
                                     defaultThisValue, &thisValue, nsnull, PR_TRUE);
        } else {
          // If the listener is a JS object which has receiveMessage function:
          NS_ENSURE_STATE(JS_GetProperty(ctx, object, "receiveMessage",
                                         &funval) &&
                          JSVAL_IS_OBJECT(funval) &&
                          !JSVAL_IS_NULL(funval));
          JSObject* funobject = JSVAL_TO_OBJECT(funval);
          NS_ENSURE_STATE(JS_ObjectIsCallable(ctx, funobject));
          thisValue = OBJECT_TO_JSVAL(object);
        }

        jsval rval = JSVAL_VOID;

        js::AutoValueRooter argv(ctx);
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
            if (JS_Stringify(ctx, &rval, nsnull, JSVAL_NULL,
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
                                                         aSync, aJSON, aObjectsArray,
                                                         aJSONRetVal, mContext) : NS_OK;
}

void
nsFrameMessageManager::AddChildManager(nsFrameMessageManager* aManager,
                                       PRBool aLoadScripts)
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
        aManager->LoadFrameScript(globalMM->mPendingScripts[i], PR_FALSE);
      }
    }
    for (PRUint32 i = 0; i < mPendingScripts.Length(); ++i) {
      aManager->LoadFrameScript(mPendingScripts[i], PR_FALSE);
    }
  }
}

void
nsFrameMessageManager::SetCallbackData(void* aData, PRBool aLoadScripts)
{
  if (aData && mCallbackData != aData) {
    mCallbackData = aData;
    // First load global scripts by adding this to parent manager.
    if (mParentManager) {
      mParentManager->AddChildManager(this, aLoadScripts);
    }
    if (aLoadScripts) {
      for (PRUint32 i = 0; i < mPendingScripts.Length(); ++i) {
        LoadFrameScript(mPendingScripts[i], PR_FALSE);
      }
    }
  }
}

void
nsFrameMessageManager::Disconnect(PRBool aRemoveFromParent)
{
  if (mParentManager && aRemoveFromParent) {
    mParentManager->RemoveChildManager(this);
  }
  mParentManager = nsnull;
  mCallbackData = nsnull;
  mContext = nsnull;
}

nsresult
NS_NewGlobalMessageManager(nsIChromeFrameMessageManager** aResult)
{
  NS_ENSURE_TRUE(IsChromeProcess(), NS_ERROR_NOT_AVAILABLE);
  nsFrameMessageManager* mm = new nsFrameMessageManager(PR_TRUE,
                                                        nsnull,
                                                        nsnull,
                                                        nsnull,
                                                        nsnull,
                                                        nsnull,
                                                        nsnull,
                                                        PR_TRUE);
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
  nsFrameScriptExecutor::sCachedScripts = nsnull;
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
    mDelayedCxDestroy = PR_TRUE;
    return;
  }
  mDelayedCxDestroy = PR_FALSE;
  if (mCx) {
    nsIXPConnect* xpc = nsContentUtils::XPConnect();
    if (xpc) {
      xpc->ReleaseJSContext(mCx, PR_TRUE);
    } else {
      JS_DestroyContext(mCx);
    }
  }
  mCx = nsnull;
  mGlobal = nsnull;
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
    JSContext* cx = nsnull;
    nsContentUtils::ThreadJSContextStack()->GetSafeJSContext(&cx);
    if (cx) {
#ifdef DEBUG_smaug
      printf("Will clear cached frame manager scripts!\n");
#endif
      JSAutoRequest ar(cx);
      NS_ASSERTION(sCachedScripts != nsnull, "Need cached scripts");
      sCachedScripts->Enumerate(CachedScriptUnrooter, cx);
    } else {
      NS_WARNING("No context available. Leaking cached scripts!\n");
    }

    delete sCachedScripts;
    sCachedScripts = nsnull;

    sScriptCacheCleaner = nsnull;
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
      JSObject* global = nsnull;
      mGlobal->GetJSObject(&global);
      if (global) {
        (void) JS_ExecuteScript(mCx, global, holder->mScript, nsnull);
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
  
  PRBool hasFlags;
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
                                   EmptyString(), nsnull, dataString);
  }

  if (!dataString.IsEmpty()) {
    nsContentUtils::ThreadJSContextStack()->Push(mCx);
    {
      // Need to scope JSAutoRequest to happen after Push but before Pop,
      // at least for now. See bug 584673.
      JSAutoRequest ar(mCx);
      JSObject* global = nsnull;
      mGlobal->GetJSObject(&global);
      if (global) {
        JSPrincipals* jsprin = nsnull;
        mPrincipal->GetJSPrincipals(mCx, &jsprin);

        uint32 oldopts = JS_GetOptions(mCx);
        JS_SetOptions(mCx, oldopts | JSOPTION_NO_SCRIPT_RVAL);

        JSScript* script =
          JS_CompileUCScriptForPrincipals(mCx, nsnull, jsprin,
                                         (jschar*)dataString.get(),
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
          (void) JS_ExecuteScript(mCx, global, script, nsnull);
        }
        //XXX Argh, JSPrincipals are manually refcounted!
        JSPRINCIPALS_DROP(mCx, jsprin);
      }
    } 
    JSContext* unused;
    nsContentUtils::ThreadJSContextStack()->Pop(&unused);
  }
}

// static
void
nsFrameScriptExecutor::Traverse(nsFrameScriptExecutor *tmp,
                                nsCycleCollectionTraversalCallback &cb)
{
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mGlobal)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mCx");
  nsContentUtils::XPConnect()->NoteJSContext(tmp->mCx, cb);
}

NS_IMPL_ISUPPORTS1(nsScriptCacheCleaner, nsIObserver)

nsFrameMessageManager* nsFrameMessageManager::sChildProcessManager = nsnull;
nsFrameMessageManager* nsFrameMessageManager::sParentProcessManager = nsnull;

bool SendAsyncMessageToChildProcess(void* aCallbackData,
                                    const nsAString& aMessage,
                                    const nsAString& aJSON)
{
  mozilla::dom::ContentParent* cp =
    static_cast<mozilla::dom::ContentParent*>(aCallbackData);
  NS_WARN_IF_FALSE(cp, "No child process!");
  if (cp) {
    return cp->SendAsyncMessage(nsString(aMessage), nsString(aJSON));
  }
  return true;
}

bool SendSyncMessageToParentProcess(void* aCallbackData,
                                    const nsAString& aMessage,
                                    const nsAString& aJSON,
                                    InfallibleTArray<nsString>* aJSONRetVal)
{
  mozilla::dom::ContentChild* cc =
    mozilla::dom::ContentChild::GetSingleton();
  if (cc) {
    return
      cc->SendSyncMessage(nsString(aMessage), nsString(aJSON), aJSONRetVal);
  }
  return true;
}

bool SendAsyncMessageToParentProcess(void* aCallbackData,
                                     const nsAString& aMessage,
                                     const nsAString& aJSON)
{
  mozilla::dom::ContentChild* cc =
    mozilla::dom::ContentChild::GetSingleton();
  if (cc) {
    return cc->SendAsyncMessage(nsString(aMessage), nsString(aJSON));
  }
  return true;
}

// This creates the global parent process message manager.
nsresult
NS_NewParentProcessMessageManager(nsIFrameMessageManager** aResult)
{
  NS_ASSERTION(!nsFrameMessageManager::sParentProcessManager,
               "Re-creating sParentProcessManager");
  NS_ENSURE_TRUE(IsChromeProcess(), NS_ERROR_NOT_AVAILABLE);
  nsFrameMessageManager* mm = new nsFrameMessageManager(PR_TRUE,
                                                        nsnull,
                                                        nsnull,
                                                        nsnull,
                                                        nsnull,
                                                        nsnull,
                                                        nsnull,
                                                        PR_FALSE,
                                                        PR_TRUE);
  NS_ENSURE_TRUE(mm, NS_ERROR_OUT_OF_MEMORY);
  nsFrameMessageManager::sParentProcessManager = mm;
  return CallQueryInterface(mm, aResult);
}

nsFrameMessageManager*
nsFrameMessageManager::NewProcessMessageManager(mozilla::dom::ContentParent* aProcess)
{
  if (!nsFrameMessageManager::sParentProcessManager) {
     nsCOMPtr<nsIFrameMessageManager> dummy;
     NS_NewParentProcessMessageManager(getter_AddRefs(dummy));
  }

  nsFrameMessageManager* mm = new nsFrameMessageManager(PR_TRUE,
                                                        nsnull,
                                                        SendAsyncMessageToChildProcess,
                                                        nsnull,
                                                        aProcess,
                                                        nsFrameMessageManager::sParentProcessManager,
                                                        nsnull,
                                                        PR_FALSE,
                                                        PR_TRUE);
  return mm;
}

nsresult
NS_NewChildProcessMessageManager(nsISyncMessageSender** aResult)
{
  NS_ASSERTION(!nsFrameMessageManager::sChildProcessManager,
               "Re-creating sChildProcessManager");
  NS_ENSURE_TRUE(!IsChromeProcess(), NS_ERROR_NOT_AVAILABLE);
  nsFrameMessageManager* mm = new nsFrameMessageManager(PR_FALSE,
                                                        SendSyncMessageToParentProcess,
                                                        SendAsyncMessageToParentProcess,
                                                        nsnull,
                                                        &nsFrameMessageManager::sChildProcessManager,
                                                        nsnull,
                                                        nsnull,
                                                        PR_FALSE,
                                                        PR_TRUE);
  NS_ENSURE_TRUE(mm, NS_ERROR_OUT_OF_MEMORY);
  nsFrameMessageManager::sChildProcessManager = mm;
  return CallQueryInterface(mm, aResult);
}
