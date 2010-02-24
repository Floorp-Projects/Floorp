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

#include "nsFrameMessageManager.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "jsnum.h"
#include "jsarray.h"
#include "jsinterp.h"
#include "nsJSUtils.h"

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
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIFrameMessageManager, nsIContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIContentFrameMessageManager, !mChrome)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIChromeFrameMessageManager, mChrome)
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
  if (aAllowDelayedLoad && !mCallbackData && !mChildManagers.Count()) {
    mPendingScripts.AppendElement(aURL);
    return NS_OK;
  }

  if (mCallbackData) {
#ifdef DEBUG_smaug
    printf("Will load %s \n", NS_ConvertUTF16toUTF8(aURL).get());
#endif
    NS_ENSURE_TRUE(mLoadScriptCallback(mCallbackData, aURL), NS_ERROR_FAILURE);
  }

  PRInt32 len = mChildManagers.Count();
  for (PRInt32 i = 0; i < len; ++i) {
    static_cast<nsFrameMessageManager*>(mChildManagers[i])->LoadFrameScript(aURL,
                                                                            PR_FALSE);
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
  if (mSyncCallback) {
    NS_ENSURE_TRUE(mCallbackData, NS_ERROR_NOT_INITIALIZED);
    nsString messageName;
    nsString json;
    nsresult rv = GetParamsForMessage(messageName, json);
    NS_ENSURE_SUCCESS(rv, rv);
    nsTArray<nsString> retval;
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
      jsval* dest = nsnull;
      JSObject* dataArray = js_NewArrayObjectWithCapacity(ctx, len, &dest);
      NS_ENSURE_TRUE(dataArray, NS_ERROR_OUT_OF_MEMORY);
      nsAutoGCRoot arrayGCRoot(&dataArray, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      for (PRUint32 i = 0; i < len; ++i) {
        jsval ret = JSVAL_VOID;
        nsAutoGCRoot root(&ret, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        JSONParser* parser = JS_BeginJSONParse(ctx, &ret);
        JSBool ok = JS_ConsumeJSONText(ctx, parser, (jschar*)retval[i].get(),
                                       (uint32)retval[i].Length());
        ok = JS_FinishJSONParse(ctx, parser, JSVAL_NULL) && ok;
        if (ok) {
          dest[i] = ret;
        }
      }

      jsval* retvalPtr;
      ncc->GetRetValPtr(&retvalPtr);
      *retvalPtr = OBJECT_TO_JSVAL(dataArray);
      ncc->SetReturnValueWasSet(PR_TRUE);
    }
  }
  NS_ASSERTION(!mParentManager, "Should not have parent manager in content!");
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
  SendAsyncMessageInternal(messageName, json);
  return NS_OK;
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

nsresult
nsFrameMessageManager::ReceiveMessage(nsISupports* aTarget,
                                      const nsAString& aMessage,
                                      PRBool aSync, const nsAString& aJSON,
                                      nsTArray<nsString>* aJSONRetVal)
{
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
        NS_ENSURE_STATE(pusher.Push(mContext, NS_ERROR_FAILURE));

        JSAutoRequest ar(mContext);

        // The parameter for the listener function.
        JSObject* param = JS_NewObject(mContext, NULL, NULL, NULL);
        NS_ENSURE_TRUE(param, NS_ERROR_OUT_OF_MEMORY);

        nsresult rv;
        nsAutoGCRoot resultGCRoot(&param, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        jsval json = JSVAL_NULL;
        nsAutoGCRoot root(&json, &rv);
        if (NS_SUCCEEDED(rv) && !aJSON.IsEmpty()) {
          JSONParser* parser = JS_BeginJSONParse(mContext, &json);
          if (parser) {
            JSBool ok = JS_ConsumeJSONText(mContext, parser,
                                           (jschar*)nsString(aJSON).get(),
                                           (uint32)aJSON.Length());
            ok = JS_FinishJSONParse(mContext, parser, JSVAL_NULL) && ok;
            if (!ok) {
              json = JSVAL_NULL;
            }
          }
        }
        JSString* jsMessage =
          JS_NewUCStringCopyN(mContext,
                              reinterpret_cast<const jschar *>(nsString(aMessage).get()),
                              aMessage.Length());
        NS_ENSURE_TRUE(jsMessage, NS_ERROR_OUT_OF_MEMORY);
        JS_DefineProperty(mContext, param, "name",
                          STRING_TO_JSVAL(jsMessage), NULL, NULL, JSPROP_ENUMERATE);
        JS_DefineProperty(mContext, param, "sync",
                          BOOLEAN_TO_JSVAL(aSync), NULL, NULL, JSPROP_ENUMERATE);
        JS_DefineProperty(mContext, param, "json", json, NULL, NULL, JSPROP_ENUMERATE);

        jsval funval = OBJECT_TO_JSVAL(static_cast<JSObject *>(object));
        jsval targetv;
        nsAutoGCRoot resultGCRoot2(&targetv, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        nsContentUtils::WrapNative(mContext,
                                   JS_GetGlobalObject(mContext),
                                   aTarget, &targetv);

        jsval rval = JSVAL_VOID;
        nsAutoGCRoot resultGCRoot3(&rval, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        void* mark = nsnull;
        jsval* argv = js_AllocStack(mContext, 1, &mark);
        NS_ENSURE_TRUE(argv, NS_ERROR_OUT_OF_MEMORY);

        argv[0] = OBJECT_TO_JSVAL(param);
        JSObject* target = JSVAL_TO_OBJECT(targetv);
        JS_CallFunctionValue(mContext, target,
                             funval, 1, argv, &rval);
        if (aJSONRetVal) {
          nsString json;
          if (JS_TryJSON(mContext, &rval) &&
              JS_Stringify(mContext, &rval, nsnull, JSVAL_NULL,
                           JSONCreator, &json)) {
            aJSONRetVal->AppendElement(json);
          }
        }
        js_FreeStack(mContext, mark);
      }
    }
  }
  return mParentManager ? mParentManager->ReceiveMessage(aTarget, aMessage,
                                                         aSync, aJSON,
                                                         aJSONRetVal) : NS_OK;
}

void
nsFrameMessageManager::AddChildManager(nsFrameMessageManager* aManager)
{
  mChildManagers.AppendObject(aManager);
  for (PRUint32 i = 0; i < mPendingScripts.Length(); ++i) {
    aManager->LoadFrameScript(mPendingScripts[i], PR_FALSE);
  }
}

void
nsFrameMessageManager::SetCallbackData(void* aData)
{
  if (aData && mCallbackData != aData) {
    mCallbackData = aData;
    // First load global scripts by adding this to parent manager.
    if (mParentManager) {
      mParentManager->AddChildManager(this);
    }
    for (PRUint32 i = 0; i < mPendingScripts.Length(); ++i) {
      LoadFrameScript(mPendingScripts[i], PR_FALSE);
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
}
