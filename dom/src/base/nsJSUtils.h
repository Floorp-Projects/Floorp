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

#ifndef nsJSUtils_h__
#define nsJSUtils_h__

/**
 * This is not a generated file. It contains common utility functions 
 * invoked from the JavaScript code generated from IDL interfaces.
 * The goal of the utility functions is to cut down on the size of
 * the generated code itself.
 */

#include "nsISupports.h"
#include "jsapi.h"
#include "nsString.h"

class nsIDOMEventListener;
class nsIScriptContext;
class nsIScriptGlobalObject;
class nsIScriptSecurityManager;

class nsJSUtils {
public:
  static NS_EXPORT JSBool nsGetCallingLocation(JSContext* aContext,
                                               const char* *aFilename,
                                               PRUint32 *aLineno);

  static NS_EXPORT JSBool nsReportError(JSContext* aContext, 
                                        JSObject* aObj,
                                        nsresult aResult,
                                        const char* aMessage=nsnull);

  static NS_EXPORT PRBool nsCallJSScriptObjectGetProperty(nsISupports* aSupports,
                                                JSContext* aContext,
                                                JSObject* aObj,
                                                jsval aId,
                                                jsval* aReturn);

  static NS_EXPORT PRBool nsCallJSScriptObjectSetProperty(nsISupports* aSupports,
                                                JSContext* aContext,
                                                JSObject* aObj,
                                                jsval aId,
                                                jsval* aReturn);

  static NS_EXPORT void nsConvertObjectToJSVal(nsISupports* aSupports,
                                     JSContext* aContext,
                                     JSObject* aObj,
                                     jsval* aReturn);

  static NS_EXPORT void nsConvertXPCObjectToJSVal(nsISupports* aSupports,
                                                  const nsIID& aIID,
                                                  JSContext* aContext,
                                                  JSObject* aScope,
                                                  jsval* aReturn);

  static NS_EXPORT void nsConvertStringToJSVal(const nsString& aProp,
                                               JSContext* aContext,
                                               jsval* aReturn);

  static NS_EXPORT PRBool nsConvertJSValToObject(nsISupports** aSupports,
                                       REFNSIID aIID,
                                       const nsString& aTypeName,
                                       JSContext* aContext,
                                       jsval aValue);

  static NS_EXPORT PRBool nsConvertJSValToXPCObject(nsISupports** aSupports,
                                                    REFNSIID aIID,
                                                    JSContext* aContext,
                                                    jsval aValue);

  static NS_EXPORT void nsConvertJSValToString(nsAWritableString& aString,
                                               JSContext* aContext,
                                               jsval aValue);

  static NS_EXPORT PRBool nsConvertJSValToBool(PRBool* aProp,
                                               JSContext* aContext,
                                               jsval aValue);

  static NS_EXPORT PRBool nsConvertJSValToUint32(PRUint32* aProp,
                                                 JSContext* aContext,
                                                 jsval aValue);

  static NS_EXPORT PRBool nsConvertJSValToFunc(nsIDOMEventListener** aListener,
                                               JSContext* aContext,
                                               JSObject* aObj,
                                               jsval aValue);

  static NS_EXPORT void PR_CALLBACK nsGenericFinalize(JSContext* aContext,
                                                      JSObject* aObj);

  static NS_EXPORT JSBool nsGenericEnumerate(JSContext* aContext,
                                             JSObject* aObj,
                                             JSPropertySpec* aLazyPropSpec);

  static NS_EXPORT JSBool nsGlobalResolve(JSContext* aContext,
                                          JSObject* aObj, 
                                          jsval aId,
                                          JSPropertySpec* aLazyPropSpec);

  static NS_EXPORT JSBool nsGenericResolve(JSContext* aContext,
                                           JSObject* aObj, 
                                           jsval aId,
                                           JSPropertySpec* aLazyPropSpec);

  static NS_EXPORT nsISupports* nsGetNativeThis(JSContext* aContext,
                                                JSObject* aObj);
  
  static NS_EXPORT nsresult nsGetStaticScriptGlobal(JSContext* aContext,
                                    JSObject* aObj,
                                    nsIScriptGlobalObject** aNativeGlobal);

  static NS_EXPORT nsresult nsGetStaticScriptContext(JSContext* aContext,
                                    JSObject* aObj,
                                    nsIScriptContext** aScriptContext);

  static NS_EXPORT nsresult nsGetDynamicScriptGlobal(JSContext *aContext,
                                    nsIScriptGlobalObject** aNativeGlobal);

  static NS_EXPORT nsresult nsGetDynamicScriptContext(JSContext *aContext,
                                    nsIScriptContext** aScriptContext);

  static NS_EXPORT JSBool PR_CALLBACK nsCheckAccess(JSContext *cx,
                                    JSObject *obj, jsid id, JSAccessMode mode,
	                            jsval *vp);

  static NS_EXPORT nsIScriptSecurityManager *
                    nsGetSecurityManager(JSContext *cx, JSObject *obj);

  static NS_EXPORT void nsClearCachedSecurityManager();

protected:
  static PRBool NameAndFormatForNSResult(nsresult rv,
                                         const char** name,
                                         const char** format);

  static nsIScriptSecurityManager *mCachedSecurityManager;
};

#endif /* nsJSUtils_h__ */
