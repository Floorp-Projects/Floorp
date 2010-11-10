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

#ifdef MOZ_IPC
#include "ContentChild.h"
#include "ContentParent.h"
#endif
#include "jscntxt.h"
#include "nsFrameMessageManager.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsinterp.h"
#include "nsJSUtils.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "nsIJSContextStack.h"
#include "nsIXULRuntime.h"

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
  /* Process message manager doesn't support nsIChromeFrameMessageManager. */
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIChromeFrameMessageManager,
                                     mChrome && !mIsProcessManager)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsFrameMessageManager,
                                          nsIContentFrameMessageManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsFrameMessageManager,
                                           nsIContentFrameMessageManager)

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
    aMessageName.Assign(nsDependentJSString(str));
  }

  if (argc >= 2) {
    jsval v = argv[1];
    nsAutoGCRoot root(&v, &rv);
    NS_ENSURE_SUCCESS(rv, JS_FALSE);
    if (JS_TryJSON(ctx, &v)) {
      JS_Stringify(ctx, &v, nsnull, JSVAL_NULL, JSONCreator, &aJSON);
    }
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
        if (!retval[i].Length())
          continue;

        jsval ret = JSVAL_VOID;
        JSONParser* parser = JS_BeginJSONParse(ctx, &ret);
        JSBool ok = JS_ConsumeJSONText(ctx, parser, (jschar*)retval[i].get(),
                                       (uint32)retval[i].Length());
        ok = JS_FinishJSONParse(ctx, parser, JSVAL_NULL) && ok;
        if (ok) {
          NS_ENSURE_TRUE(JS_SetElement(ctx, dataArray, i, &ret), NS_ERROR_OUT_OF_MEMORY);
        }
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

        // The parameter for the listener function.
        JSObject* param = JS_NewObject(ctx, NULL, NULL, NULL);
        NS_ENSURE_TRUE(param, NS_ERROR_OUT_OF_MEMORY);

        nsresult rv;
        nsAutoGCRoot resultGCRoot(&param, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        jsval targetv;
        nsAutoGCRoot resultGCRoot2(&targetv, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        nsContentUtils::WrapNative(ctx,
                                   JS_GetGlobalObject(ctx),
                                   aTarget, &targetv);

        // To keep compatibility with e10s message manager,
        // define empty objects array.
        if (!aObjectsArray) {
          // Because we want JS messages to have always the same properties,
          // create array even if len == 0.
          aObjectsArray = JS_NewArrayObject(ctx, 0, NULL);
          if (!aObjectsArray) {
            return false;
          }
        }
        nsAutoGCRoot arrayGCRoot(&aObjectsArray, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        jsval json = JSVAL_NULL;
        nsAutoGCRoot root(&json, &rv);
        if (NS_SUCCEEDED(rv) && !aJSON.IsEmpty()) {
          JSONParser* parser = JS_BeginJSONParse(ctx, &json);
          if (parser) {
            JSBool ok = JS_ConsumeJSONText(ctx, parser,
                                           (jschar*)nsString(aJSON).get(),
                                           (uint32)aJSON.Length());
            ok = JS_FinishJSONParse(ctx, parser, JSVAL_NULL) && ok;
            if (!ok) {
              json = JSVAL_NULL;
            }
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
        JS_DefineProperty(ctx, param, "objects", OBJECT_TO_JSVAL(aObjectsArray),
                          NULL, NULL, JSPROP_ENUMERATE);

        jsval thisValue = JSVAL_VOID;
        nsAutoGCRoot resultGCRoot3(&thisValue, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        JSAutoEnterCompartment ac;

        if (!ac.enter(ctx, object))
          return PR_FALSE;

        jsval funval = JSVAL_VOID;
        if (JS_ObjectIsFunction(ctx, object)) {
          // If the listener is a JS function:
          funval = OBJECT_TO_JSVAL(object);

          // A small hack to get 'this' value right on content side where
          // messageManager is wrapped in TabChildGlobal.
          nsCOMPtr<nsISupports> defaultThisValue;
          if (mChrome) {
            defaultThisValue =
              do_QueryInterface(static_cast<nsIContentFrameMessageManager*>(this));
          } else {
            defaultThisValue = aTarget;
          }
          nsContentUtils::WrapNative(ctx,
                                     JS_GetGlobalObject(ctx),
                                     defaultThisValue, &thisValue);
        } else {
          // If the listener is a JS object which has receiveMessage function:
          NS_ENSURE_STATE(JS_GetProperty(ctx, object, "receiveMessage",
                                         &funval) &&
                          JSVAL_IS_OBJECT(funval) &&
                          !JSVAL_IS_NULL(funval));
          JSObject* funobject = JSVAL_TO_OBJECT(funval);
          NS_ENSURE_STATE(JS_ObjectIsFunction(ctx, funobject));
          thisValue = OBJECT_TO_JSVAL(object);
        }

        jsval rval = JSVAL_VOID;
        nsAutoGCRoot resultGCRoot4(&rval, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

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
            if (JS_TryJSON(ctx, &rval) &&
                JS_Stringify(ctx, &rval, nsnull, JSVAL_NULL,
                             JSONCreator, &json)) {
              aJSONRetVal->AppendElement(json);
            }
          }
        }
      }
    }
  }
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

nsDataHashtable<nsStringHashKey, nsFrameScriptExecutorJSObjectHolder*>*
  nsFrameScriptExecutor::sCachedScripts = nsnull;
nsRefPtr<nsScriptCacheCleaner> nsFrameScriptExecutor::sScriptCacheCleaner;

void
nsFrameScriptExecutor::DidCreateCx()
{
  NS_ASSERTION(mCx, "Should have mCx!");
  if (!sCachedScripts) {
    sCachedScripts =
      new nsDataHashtable<nsStringHashKey, nsFrameScriptExecutorJSObjectHolder*>;
    sCachedScripts->Init();

    sScriptCacheCleaner = new nsScriptCacheCleaner();
  }
}

void
nsFrameScriptExecutor::DestroyCx()
{
  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  if (xpc) {
    xpc->ReleaseJSContext(mCx, PR_TRUE);
  } else {
    JS_DestroyContext(mCx);
  }
  mCx = nsnull;
  mGlobal = nsnull;
}

static PLDHashOperator
CachedScriptUnrooter(const nsAString& aKey,
                       nsFrameScriptExecutorJSObjectHolder*& aData,
                       void* aUserArg)
{
  JSContext* cx = static_cast<JSContext*>(aUserArg);
  JS_RemoveObjectRoot(cx, &(aData->mObject));
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
  if (!mGlobal || !mCx) {
    return;
  }

  nsFrameScriptExecutorJSObjectHolder* holder = sCachedScripts->Get(aURL);
  if (holder) {
    nsContentUtils::ThreadJSContextStack()->Push(mCx);
    {
      // Need to scope JSAutoRequest to happen after Push but before Pop,
      // at least for now. See bug 584673.
      JSAutoRequest ar(mCx);
      JSObject* global = nsnull;
      mGlobal->GetJSObject(&global);
      if (global) {
        jsval val;
        JS_ExecuteScript(mCx, global,
                         (JSScript*)JS_GetPrivate(mCx, holder->mObject),
                         &val);
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
  nsCOMPtr<nsIChannel> channel;
  NS_NewChannel(getter_AddRefs(channel), uri);
  if (!channel) {
    return;
  }

  nsCOMPtr<nsIInputStream> input;
  channel->Open(getter_AddRefs(input));
  nsString dataString;
  if (input) {
    const PRUint32 bufferSize = 8192;
    char buffer[bufferSize];
    nsCString data;
    PRUint32 avail = 0;
    input->Available(&avail);
    PRUint32 read = 0;
    if (avail) {
      while (NS_SUCCEEDED(input->Read(buffer, bufferSize, &read)) && read) {
        data.Append(buffer, read);
        read = 0;
      }
    }
    nsScriptLoader::ConvertToUTF16(channel, (PRUint8*)data.get(), data.Length(),
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
        nsContentUtils::XPConnect()->FlagSystemFilenamePrefix(url.get(), PR_TRUE);
        JSScript* script =
          JS_CompileUCScriptForPrincipals(mCx, nsnull, jsprin,
                                         (jschar*)dataString.get(),
                                          dataString.Length(),
                                          url.get(), 1);

        if (script) {
          JSObject* scriptObj = JS_NewScriptObject(mCx, script);
          JS_AddObjectRoot(mCx, &scriptObj);
          nsCAutoString scheme;
          uri->GetScheme(scheme);
          // We don't cache data: scripts!
          if (!scheme.EqualsLiteral("data")) {
            nsFrameScriptExecutorJSObjectHolder* holder =
              new nsFrameScriptExecutorJSObjectHolder(scriptObj);
            // Root the object also for caching.
            JS_AddNamedObjectRoot(mCx, &(holder->mObject),
                                  "Cached message manager script");
            sCachedScripts->Put(aURL, holder);
          }
          jsval val;
          JS_ExecuteScript(mCx, global,
                           (JSScript*)JS_GetPrivate(mCx, scriptObj), &val);
          JS_RemoveObjectRoot(mCx, &scriptObj);
        }
        //XXX Argh, JSPrincipals are manually refcounted!
        JSPRINCIPALS_DROP(mCx, jsprin);
      }
    } 
    JSContext* unused;
    nsContentUtils::ThreadJSContextStack()->Pop(&unused);
  }
}

NS_IMPL_ISUPPORTS1(nsScriptCacheCleaner, nsIObserver)

nsFrameMessageManager* nsFrameMessageManager::sChildProcessManager = nsnull;
nsFrameMessageManager* nsFrameMessageManager::sParentProcessManager = nsnull;

#ifdef MOZ_IPC
bool SendAsyncMessageToChildProcess(void* aCallbackData,
                                    const nsAString& aMessage,
                                    const nsAString& aJSON)
{
  mozilla::dom::ContentParent* cp =
    mozilla::dom::ContentParent::GetSingleton(PR_FALSE);
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

#endif

nsresult
NS_NewParentProcessMessageManager(nsIFrameMessageManager** aResult)
{
  NS_ASSERTION(!nsFrameMessageManager::sParentProcessManager,
               "Re-creating sParentProcessManager");
#ifdef MOZ_IPC
  NS_ENSURE_TRUE(IsChromeProcess(), NS_ERROR_NOT_AVAILABLE);
  nsFrameMessageManager* mm = new nsFrameMessageManager(PR_TRUE,
                                                        nsnull,
                                                        SendAsyncMessageToChildProcess,
                                                        nsnull,
                                                        &nsFrameMessageManager::sParentProcessManager,
                                                        nsnull,
                                                        nsnull,
                                                        PR_FALSE,
                                                        PR_TRUE);
  NS_ENSURE_TRUE(mm, NS_ERROR_OUT_OF_MEMORY);
  nsFrameMessageManager::sParentProcessManager = mm;
  return CallQueryInterface(mm, aResult);
#else
  return NS_ERROR_NOT_AVAILABLE;
#endif
}


nsresult
NS_NewChildProcessMessageManager(nsISyncMessageSender** aResult)
{
  NS_ASSERTION(!nsFrameMessageManager::sChildProcessManager,
               "Re-creating sChildProcessManager");
#ifdef MOZ_IPC
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
#else
  return NS_ERROR_NOT_AVAILABLE;
#endif
}
