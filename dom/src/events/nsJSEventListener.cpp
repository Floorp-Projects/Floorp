/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsJSEventListener.h"
#include "nsString.h"
#include "nsIScriptEventListener.h"
#include "nsIServiceManager.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"


/*
 * nsJSEventListener implementation
 */
nsJSEventListener::nsJSEventListener(nsIScriptContext *aContext, 
                                     nsISupports *aObject) 
{
  NS_INIT_REFCNT();

  // mObject is a weak reference. We are guaranteed
  // because of the ownership model that this object will be
  // freed (and the references dropped) before either the context
  // or the owner goes away.
  mContext = aContext;
  mObject = aObject;
  mReturnResult = nsReturnResult_eNotSet;
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

nsresult nsJSEventListener::SetEventName(nsIAtom* aName)
{
  mEventName = aName;
  return NS_OK;
}

nsresult nsJSEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  jsval funval;
  jsval argv[1];
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
      if (eventString.EqualsWithConversion("error") || eventString.EqualsWithConversion("mouseover")) {
        mReturnResult = nsReturnResult_eReverseReturnResult;
      }
      else {
        mReturnResult = nsReturnResult_eDoNotReverseReturnResult;
      }
    //}
    eventString.InsertWithConversion("on", 0, 2);
  }
  else {
    mEventName->ToString(eventString);
  }

  nsresult rv;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));

  // root
  nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;

  rv = xpc->WrapNative(cx, ::JS_GetGlobalObject(cx),
                       mObject, NS_GET_IID(nsISupports),
                       getter_AddRefs(wrapper));
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

  rv = xpc->WrapNative(cx, obj, aEvent, NS_GET_IID(nsIDOMEvent),
                       getter_AddRefs(wrapper));
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject *eventObj = nsnull;
  rv = wrapper->GetJSObject(&eventObj);
  NS_ENSURE_SUCCESS(rv, rv);

  argv[0] = OBJECT_TO_JSVAL(eventObj);
  PRBool jsBoolResult;
  PRBool returnResult = (mReturnResult == nsReturnResult_eReverseReturnResult);

  rv = mContext->CallEventHandler(obj, JSVAL_TO_OBJECT(funval), 1, argv,
                                  &jsBoolResult, returnResult);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!jsBoolResult) 
    aEvent->PreventDefault();

  return rv;
}

NS_IMETHODIMP 
nsJSEventListener::GetEventTarget(nsIScriptContext**aContext, 
                                  nsISupports** aTarget)
{
  NS_ENSURE_ARG_POINTER(aContext);
  NS_ENSURE_ARG_POINTER(aTarget);

  *aContext = mContext;
  NS_ADDREF(*aContext);

  *aTarget = mObject;
  NS_ADDREF(*aTarget);

  return NS_OK;
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

