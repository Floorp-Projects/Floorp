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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsSOAPParameter.h"
#include "nsSOAPUtils.h"
#include "nsIXPConnect.h"
#include "nsIServiceManager.h"

nsSOAPParameter::nsSOAPParameter()
{
  NS_INIT_ISUPPORTS();
  mType = PARAMETER_TYPE_NULL;
  mJSValue = nsnull;
  JSContext* cx;
  cx = nsSOAPUtils::GetSafeContext();
  if (cx) {
    JS_AddNamedRoot(cx, &mJSValue, "nsSOAPParameter");
  }
}

nsSOAPParameter::~nsSOAPParameter()
{
  JSContext* cx;
  cx = nsSOAPUtils::GetSafeContext();
  if (cx) {
    JS_RemoveRoot(cx, &mJSValue);
  }
}

NS_IMPL_ISUPPORTS4(nsSOAPParameter, 
                   nsISOAPParameter, 
                   nsISecurityCheckedComponent,
                   nsIXPCScriptable,
                   nsIJSNativeInitializer)

/* attribute string encodingStyleURI; */
NS_IMETHODIMP nsSOAPParameter::GetEncodingStyleURI(char * *aEncodingStyleURI)
{
  NS_ENSURE_ARG_POINTER(aEncodingStyleURI);
  if (mEncodingStyleURI.Length() > 0) {
    *aEncodingStyleURI = mEncodingStyleURI.ToNewCString();
  }
  else {
    *aEncodingStyleURI = nsnull;
  }

  return NS_OK;
}
NS_IMETHODIMP nsSOAPParameter::SetEncodingStyleURI(const char * aEncodingStyleURI)
{
  if (aEncodingStyleURI) {
    mEncodingStyleURI.Assign(aEncodingStyleURI);
  }
  else {
    mEncodingStyleURI.Truncate();
  }
  return NS_OK;
}

/* attribute wstring name; */
NS_IMETHODIMP nsSOAPParameter::GetName(PRUnichar * *aName)
{
  NS_ENSURE_ARG_POINTER(aName);
  if (mName.Length() > 0) {
    *aName = mName.ToNewUnicode();
  }
  else {
    *aName = nsnull;
  }

  return NS_OK;
}
NS_IMETHODIMP nsSOAPParameter::SetName(const PRUnichar * aName)
{
  if (aName) {
    mName.Assign(aName);
  }
  else {
    mName.Truncate();
  }
  return NS_OK;
}

/* readonly attribute long type; */
NS_IMETHODIMP nsSOAPParameter::GetType(PRInt32 *aType)
{
  NS_ENSURE_ARG(aType);
  *aType = mType;
  return NS_OK;
}

/* [noscript] void setValueAndType (in nsISupports value, in long type); */
NS_IMETHODIMP nsSOAPParameter::SetValueAndType(nsISupports *value, PRInt32 type)
{
  mValue = value;
  mType = type;
  mJSValue = nsnull;
  return NS_OK;
}

/* [noscript] void getValueAndType (out nsISupports value, out long type); */
NS_IMETHODIMP nsSOAPParameter::GetValueAndType(nsISupports **value, PRInt32 *type)
{
  NS_ENSURE_ARG_POINTER(value);
  NS_ENSURE_ARG_POINTER(type);

  *value = mValue;
  NS_IF_ADDREF(*value);
  *type = mType;

  return NS_OK;
}

/* attribute nsISupports value; */
// We can't use this attribute for script calls without a variant type
// in xpidl. For now, use nsIXPCScriptable to implement.
NS_IMETHODIMP nsSOAPParameter::GetValue(nsISupports * *aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);

  *aValue = mValue;
  NS_IF_ADDREF(*aValue);

  return NS_OK;
}

NS_IMETHODIMP
nsSOAPParameter::SetValue(nsISupports* value)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsSOAPParameter::GetValue(JSContext* aContext,
                                        jsval* aValue)
{
  return nsSOAPUtils::ConvertValueToJSVal(aContext,
                                          mValue,
                                          mJSValue,
                                          mType,
                                          aValue);
}

NS_IMETHODIMP nsSOAPParameter::SetValue(JSContext* aContext,
                                        jsval aValue)
{
  return nsSOAPUtils::ConvertJSValToValue(aContext,
                                          aValue,
                                          getter_AddRefs(mValue),
                                          &mJSValue,
                                          &mType);
}

/* [noscript] readonly attribute JSObjectPtr JSValue; */
NS_IMETHODIMP nsSOAPParameter::GetJSValue(JSObject * *aJSValue)
{
  NS_ENSURE_ARG_POINTER(aJSValue);
  *aJSValue = mJSValue;
  
  return NS_OK;
}

NS_IMETHODIMP 
nsSOAPParameter::Initialize(JSContext *cx, JSObject *obj, 
                            PRUint32 argc, jsval *argv)
{
  if (argc > 0) {
    JSString* namestr = JS_ValueToString(cx, argv[0]);
    if (namestr) {
      SetName(NS_REINTERPRET_CAST(PRUnichar*, JS_GetStringChars(namestr)));
      if (argc > 1) {
        SetValue(cx, argv[1]);
      }
    }
  }

  return NS_OK;
}

XPC_IMPLEMENT_IGNORE_CREATE(nsSOAPParameter)
XPC_IMPLEMENT_IGNORE_GETFLAGS(nsSOAPParameter)
XPC_IMPLEMENT_IGNORE_LOOKUPPROPERTY(nsSOAPParameter)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsSOAPParameter)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(nsSOAPParameter)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsSOAPParameter)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsSOAPParameter)
XPC_IMPLEMENT_IGNORE_DEFAULTVALUE(nsSOAPParameter)
XPC_IMPLEMENT_IGNORE_ENUMERATE(nsSOAPParameter)
XPC_IMPLEMENT_IGNORE_CHECKACCESS(nsSOAPParameter)
XPC_IMPLEMENT_IGNORE_CALL(nsSOAPParameter)
XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsSOAPParameter)
XPC_IMPLEMENT_IGNORE_HASINSTANCE(nsSOAPParameter)
XPC_IMPLEMENT_IGNORE_FINALIZE(nsSOAPParameter)

NS_IMETHODIMP 
nsSOAPParameter::GetProperty(JSContext *cx, JSObject *obj,
                             jsid id, jsval *vp,
                             nsIXPConnectWrappedNative* wrapper,
                             nsIXPCScriptable* arbitrary,
                             JSBool* retval)
{
  *retval = JS_TRUE;
  jsval val;
  if (JS_IdToValue(cx, id, &val)) {
    if (JSVAL_IS_STRING(val)) {
      JSString* str = JSVAL_TO_STRING(val);
      char* name = JS_GetStringBytes(str);
      if (nsCRT::strcmp(name, "value") == 0) {
        return GetValue(cx, vp);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsSOAPParameter::SetProperty(JSContext *cx, JSObject *obj, jsid id, 
                             jsval *vp, nsIXPConnectWrappedNative* wrapper,
                             nsIXPCScriptable* arbitrary,
                             JSBool* retval)
{
  *retval = JS_TRUE;
  jsval val;
  if (JS_IdToValue(cx, id, &val)) {
    if (JSVAL_IS_STRING(val)) {
      JSString* str = JSVAL_TO_STRING(val);
      char* name = JS_GetStringBytes(str);
      if (nsCRT::strcmp(name, "value") == 0) {
        return SetValue(cx, *vp);
      }
    }
  }
  return NS_OK;
}


static const char* kAllAccess = "AllAccess";

/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP 
nsSOAPParameter::CanCreateWrapper(const nsIID * iid, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPParameter))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP 
nsSOAPParameter::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPParameter))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsSOAPParameter::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPParameter))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsSOAPParameter::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPParameter))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}
