/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Johnny Stenback <jst@netscape.com> (original author)
 */

#include "nsDOMScriptableHelper.h"
#include "jsapi.h"
#include "nsContentUtils.h"
#include "nsString.h"
#include "nsIServiceManager.h"

// nsHTMLDocument helper includes
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMLocation.h"
#include "nsIDOMNodeList.h"

// nsHTMLFormElement helper includes
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMNSHTMLFormControlList.h"


jsid nsDOMScriptableHelper::sLocation_id = nsnull;
PRUint32 nsDOMScriptableHelper::sInstanceCount = 0;
nsIXPConnect *nsDOMScriptableHelper::sXPConnect = nsnull;

nsDOMScriptableHelper::nsDOMScriptableHelper()
{
  NS_INIT_REFCNT();

  if (!sXPConnect) {
    nsServiceManager::GetService(nsIXPConnect::GetCID(),
                                 nsIXPConnect::GetIID(),
                                 (nsISupports **)&sXPConnect);
  }

  sInstanceCount++;
}

nsDOMScriptableHelper::~nsDOMScriptableHelper()
{
  sInstanceCount--;

  if (!sInstanceCount) {
    NS_IF_RELEASE(sXPConnect);
  }
}

NS_IMPL_ADDREF(nsDOMScriptableHelper);
NS_IMPL_RELEASE(nsDOMScriptableHelper);

NS_INTERFACE_MAP_BEGIN(nsDOMScriptableHelper)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

XPC_IMPLEMENT_FORWARD_CREATE(nsDOMScriptableHelper)
XPC_IMPLEMENT_FORWARD_GETFLAGS(nsDOMScriptableHelper)
XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(nsDOMScriptableHelper)
XPC_IMPLEMENT_FORWARD_DEFINEPROPERTY(nsDOMScriptableHelper)
XPC_IMPLEMENT_FORWARD_GETPROPERTY(nsDOMScriptableHelper)
XPC_IMPLEMENT_FORWARD_SETPROPERTY(nsDOMScriptableHelper)
XPC_IMPLEMENT_FORWARD_GETATTRIBUTES(nsDOMScriptableHelper)
XPC_IMPLEMENT_FORWARD_SETATTRIBUTES(nsDOMScriptableHelper)
XPC_IMPLEMENT_FORWARD_DELETEPROPERTY(nsDOMScriptableHelper)
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(nsDOMScriptableHelper)
XPC_IMPLEMENT_FORWARD_ENUMERATE(nsDOMScriptableHelper)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(nsDOMScriptableHelper)
XPC_IMPLEMENT_FORWARD_CALL(nsDOMScriptableHelper)
XPC_IMPLEMENT_FORWARD_CONSTRUCT(nsDOMScriptableHelper)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsDOMScriptableHelper)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsDOMScriptableHelper)

// static
nsresult
nsDOMScriptableHelper::GetDefaultHelper(void **aHelper)
{
  static nsIXPCScriptable *sHelper = nsnull;

  if (!sHelper) {
    nsresult rv = GetHelperBody(new nsDOMScriptableHelper, &sHelper);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aHelper = sHelper;

  NS_ADDREF(sHelper);

  return NS_OK;
}

nsresult
nsDOMScriptableHelper::GetHelperBody(nsDOMScriptableHelper *aHelper,
                                     nsIXPCScriptable **aStaticHolder)
{
  if (!aHelper) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aStaticHolder = aHelper;

  return nsContentUtils::ReleaseOnShutdown(NS_REINTERPRET_CAST(nsISupports **,
                                                               aStaticHolder));
}


static PRBool
DefineJSId(JSContext *cx, const char *str, jsval *val)
{
  JSString *jsstr = ::JS_InternString(cx, str);

  if (!jsstr)
    return PR_FALSE;

  return ::JS_ValueToId(cx, STRING_TO_JSVAL(jsstr), val);
}

// static
nsresult
nsDOMScriptableHelper::DefineStaticJSIds(JSContext *cx)
{
  if (!DefineJSId(cx, "location", &sLocation_id))
    return NS_ERROR_FAILURE;

  return NS_OK;
}


// nsHTMLDocument helper

class nsHTMLDocHelper : nsDOMScriptableHelper
{
public:
  virtual ~nsHTMLDocHelper() {};

  NS_IMETHOD GetProperty(JSContext *cx, JSObject *obj,
                         jsid id, jsval *vp,
                         nsIXPConnectWrappedNative* wrapper,
                         nsIXPCScriptable* arbitrary,
                         JSBool* retval);

  NS_IMETHOD SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp,
                         nsIXPConnectWrappedNative* wrapper,
                         nsIXPCScriptable* arbitrary,
                         JSBool* retval);
};

NS_IMETHODIMP
nsHTMLDocHelper::GetProperty(JSContext *cx, JSObject *obj,
                             jsid id, jsval *vp,
                             nsIXPConnectWrappedNative* wrapper,
                             nsIXPCScriptable* arbitrary,
                             JSBool* retval)
{
  jsval val;

  if (!::JS_IdToValue(cx, id, &val))
    return NS_ERROR_OUT_OF_MEMORY; // Is this correct?

  if (JSVAL_IS_STRING(val)) {
    NS_ENSURE_TRUE(sXPConnect, NS_ERROR_NOT_AVAILABLE);

    nsCOMPtr<nsISupports> native;

    wrapper->GetNative(getter_AddRefs(native));
    NS_ABORT_IF_FALSE(native, "No native!");

    nsCOMPtr<nsIDOMHTMLDocument> doc(do_QueryInterface(native));

    JSString *jsstr = JSVAL_TO_STRING(val);

    nsLiteralString prop_name(NS_REINTERPRET_CAST(const PRUnichar *,
                                                  ::JS_GetStringChars(jsstr)),
                              ::JS_GetStringLength(jsstr));

    nsCOMPtr<nsIDOMNodeList> node_list;

    doc->GetElementsByName(prop_name, getter_AddRefs(node_list));

    if (node_list) {
      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

      nsresult rv = sXPConnect->WrapNative(cx, obj, node_list,
                                           NS_GET_IID(nsIDOMNodeList /* nsISupports */),
                                           getter_AddRefs(holder));
      NS_ENSURE_SUCCESS(rv, rv);

      JSObject* prop_obj = nsnull; // XPConnect-wrapped property value.

      holder->GetJSObject(&prop_obj);

      if (prop_obj) {
        *vp = OBJECT_TO_JSVAL(prop_obj);

        *retval = PR_TRUE;

        return NS_OK;
      }
    }
  }

  return arbitrary->GetProperty(cx, obj, id, vp, wrapper, NULL, retval);
}

NS_IMETHODIMP
nsHTMLDocHelper::SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp,
                             nsIXPConnectWrappedNative* wrapper,
                             nsIXPCScriptable* arbitrary,
                             JSBool* retval)
{
  if (!DidDefineStaticJSIds()) {
    DefineStaticJSIds(cx);
  }

  if (id == sLocation_id) {
    nsCOMPtr<nsISupports> native;

    wrapper->GetNative(getter_AddRefs(native));
    NS_ABORT_IF_FALSE(native, "No native!");

    nsCOMPtr<nsIDOMNSDocument> doc(do_QueryInterface(native));

    nsCOMPtr<nsIDOMLocation> location;

    doc->GetLocation(getter_AddRefs(location));

    if (!location)
      return NS_OK;

    JSString *jsstr = ::JS_ValueToString(cx, *vp);

    if (!jsstr)
      return NS_ERROR_OUT_OF_MEMORY;

    nsLiteralString str(NS_REINTERPRET_CAST(const PRUnichar *,
                                            ::JS_GetStringChars(jsstr)),
                        ::JS_GetStringLength(jsstr));

    return location->SetHref(str);
  }

  return arbitrary->SetProperty(cx, obj, id, vp, wrapper, NULL, retval);
}

// static
nsresult
nsDOMScriptableHelper::GetHTMLDocHelper(void **aHelper)
{
  static nsIXPCScriptable *sHelper = nsnull;

  if (!sHelper) {
    nsresult rv = GetHelperBody(new nsHTMLDocHelper, &sHelper);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aHelper = sHelper;

  NS_ADDREF(sHelper);

  return NS_OK;
}


// nsHTMLFormElement helper

class nsHTMLFormHelper : nsDOMScriptableHelper
{
public:
  virtual ~nsHTMLFormHelper() {};

  NS_IMETHOD GetProperty(JSContext *cx, JSObject *obj,
                         jsid id, jsval *vp,
                         nsIXPConnectWrappedNative* wrapper,
                         nsIXPCScriptable* arbitrary,
                         JSBool* retval);
};


NS_IMETHODIMP
nsHTMLFormHelper::GetProperty(JSContext *cx, JSObject *obj,
                              jsid id, jsval *vp,
                              nsIXPConnectWrappedNative* wrapper,
                              nsIXPCScriptable* arbitrary,
                              JSBool* retval)
{
  jsval val;

  if (!::JS_IdToValue(cx, id, &val))
    return NS_ERROR_OUT_OF_MEMORY; // Is this correct?

  if (JSVAL_IS_STRING(val)) {
    nsCOMPtr<nsISupports> native;

    wrapper->GetNative(getter_AddRefs(native));
    NS_ABORT_IF_FALSE(native, "No native!");

    nsCOMPtr<nsIDOMHTMLFormElement> element(do_QueryInterface(native));

    nsCOMPtr<nsIDOMHTMLCollection> html_collection;

    element->GetElements(getter_AddRefs(html_collection));

    nsCOMPtr<nsIDOMNSHTMLFormControlList>
      form_ctrl_list(do_QueryInterface(html_collection));

    JSString *jsstr = JSVAL_TO_STRING(val);

    nsLiteralString prop(NS_REINTERPRET_CAST(const PRUnichar *,
                                             ::JS_GetStringChars(jsstr)),
                         ::JS_GetStringLength(jsstr));

    nsCOMPtr<nsISupports> propValue;

    form_ctrl_list->NamedItem(prop, getter_AddRefs(propValue));

    if (propValue) {
      JSObject* interfaceObject; // XPConnect-wrapped property value.

      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

      nsresult rv = sXPConnect->WrapNative(cx, obj, propValue,
                                           NS_GET_IID(nsISupports),
                                           getter_AddRefs(holder));
      NS_ENSURE_SUCCESS(rv, rv);

      holder->GetJSObject(&interfaceObject);

      if (interfaceObject) {
        *vp = OBJECT_TO_JSVAL(interfaceObject);

        *retval = PR_TRUE;

        return NS_OK;
      }
    }
  }

  return arbitrary->GetProperty(cx, obj, id, vp, wrapper, NULL, retval);
}

// static
nsresult
nsDOMScriptableHelper::GetHTMLFormHelper(void **aHelper)
{
  static nsIXPCScriptable *sHelper = nsnull;

  if (!sHelper) {
    nsresult rv = GetHelperBody(new nsHTMLDocHelper, &sHelper);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aHelper = sHelper;

  NS_ADDREF(sHelper);

  return NS_OK;
}

