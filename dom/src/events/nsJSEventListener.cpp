/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "nsJSEventListener.h"
#include "nsJSUtils.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"
#include "nsIPrivateDOMEvent.h"
#include "nsGUIEvent.h"


/*
 * nsJSEventListener implementation
 */
nsJSEventListener::nsJSEventListener(nsIScriptContext *aContext, 
                                     nsISupports *aObject)
  : nsIJSEventListener(aContext, aObject),
    mReturnResult(nsReturnResult_eNotSet)
{
}

nsJSEventListener::~nsJSEventListener() 
{
}

NS_INTERFACE_MAP_BEGIN(nsJSEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIJSEventListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventListener)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsJSEventListener)

NS_IMPL_RELEASE(nsJSEventListener)

//static nsString onPrefix = "on";

void
nsJSEventListener::SetEventName(nsIAtom* aName)
{
  mEventName = aName;
}

nsresult
nsJSEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  jsval funval;
  jsval arg;
  jsval *argv = &arg;
  PRInt32 argc = 0;
  void *stackPtr; // For JS_[Push|Pop]Arguments()
  nsAutoString eventString;
  // XXX This doesn't seem like the correct context on which to execute
  // the event handler. Might need to get one from the JS thread context
  // stack.
  JSContext* cx = (JSContext*)mContext->GetNativeContext();

  if (!mEventName) {
    if (NS_OK != aEvent->GetType(eventString)) {
      //JS can't handle this event yet or can't handle it at all
      return NS_OK;
    }
    //if (mReturnResult == nsReturnResult_eNotSet) {
      if (eventString.EqualsLiteral("error") ||
          eventString.EqualsLiteral("mouseover")) {
        mReturnResult = nsReturnResult_eReverseReturnResult;
      }
      else {
        mReturnResult = nsReturnResult_eDoNotReverseReturnResult;
      }
    //}
    eventString.Assign(NS_LITERAL_STRING("on") + eventString);
  }
  else {
    mEventName->ToString(eventString);
  }

  nsresult rv;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID()));

  // root
  nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
  rv = xpc->WrapNative(cx, ::JS_GetGlobalObject(cx), mTarget,
                       NS_GET_IID(nsISupports), getter_AddRefs(wrapper));
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject* obj = nsnull;
  rv = wrapper->GetJSObject(&obj);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!JS_LookupUCProperty(cx, obj,
                           NS_REINTERPRET_CAST(const jschar *,
                                               eventString.get()),
                           eventString.Length(), &funval)) {
    return NS_ERROR_FAILURE;
  }

  if (JS_TypeOfValue(cx, funval) != JSTYPE_FUNCTION) {
    return NS_OK;
  }

  PRBool handledScriptError = PR_FALSE;
  if (eventString.EqualsLiteral("onerror")) {
    nsCOMPtr<nsIPrivateDOMEvent> priv(do_QueryInterface(aEvent));
    NS_ENSURE_TRUE(priv, NS_ERROR_UNEXPECTED);

    nsEvent* event;
    priv->GetInternalNSEvent(&event);
    if (event->message == NS_SCRIPT_ERROR) {
      nsScriptErrorEvent *scriptEvent =
        NS_STATIC_CAST(nsScriptErrorEvent*, event);

      argv = ::JS_PushArguments(cx, &stackPtr, "WWi", scriptEvent->errorMsg,
                                scriptEvent->fileName, scriptEvent->lineNr);
      NS_ENSURE_TRUE(argv, NS_ERROR_OUT_OF_MEMORY);
      argc = 3;
      handledScriptError = PR_TRUE;
    }
  }

  if (!handledScriptError) {
    rv = xpc->WrapNative(cx, obj, aEvent, NS_GET_IID(nsIDOMEvent),
                         getter_AddRefs(wrapper));
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject *eventObj = nsnull;
    rv = wrapper->GetJSObject(&eventObj);
    NS_ENSURE_SUCCESS(rv, rv);

    argv[0] = OBJECT_TO_JSVAL(eventObj);
    argc = 1;
  }

  jsval rval;
  rv = mContext->CallEventHandler(obj, JSVAL_TO_OBJECT(funval), argc, argv,
                                  &rval);

  if (argv != &arg) {
    ::JS_PopArguments(cx, stackPtr);
  }

  if (NS_SUCCEEDED(rv)) {
    if (eventString.EqualsLiteral("onbeforeunload")) {
      nsCOMPtr<nsIPrivateDOMEvent> priv(do_QueryInterface(aEvent));
      NS_ENSURE_TRUE(priv, NS_ERROR_UNEXPECTED);

      nsEvent* event;
      priv->GetInternalNSEvent(&event);
      NS_ENSURE_TRUE(event && event->message == NS_BEFORE_PAGE_UNLOAD,
                     NS_ERROR_UNEXPECTED);

      nsBeforePageUnloadEvent *beforeUnload =
        NS_STATIC_CAST(nsBeforePageUnloadEvent *, event);

      if (!JSVAL_IS_VOID(rval)) {
        aEvent->PreventDefault();

        if (JSVAL_IS_STRING(rval)) {
          beforeUnload->text = nsDependentJSString(JSVAL_TO_STRING(rval));
        }
      }
    } else if (JSVAL_IS_BOOLEAN(rval)) {
      // if the handler returned false and its sense is not reversed,
      // or the handler returned true and its sense is reversed from
      // the usual (false means cancel), then prevent default.

      if (JSVAL_TO_BOOLEAN(rval) ==
          (mReturnResult == nsReturnResult_eReverseReturnResult)) {
        aEvent->PreventDefault();
      }
    }
  }

  return rv;
}

/*
 * Factory functions
 */

nsresult
NS_NewJSEventListener(nsIDOMEventListener ** aInstancePtrResult,
                      nsIScriptContext *aContext, nsISupports *aObject)
{
  nsJSEventListener* it = new nsJSEventListener(aContext, aObject);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aInstancePtrResult = it;
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

