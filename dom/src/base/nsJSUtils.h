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
  static JSBool GetCallingLocation(JSContext* aContext, const char* *aFilename,
                                   PRUint32 *aLineno);

  static JSBool ReportError(JSContext* aContext, JSObject* aObj,
                            nsresult aResult, const char* aMessage=nsnull);

  static void ConvertStringToJSVal(const nsString& aProp, JSContext* aContext,
                                   jsval* aReturn);

  static PRBool ConvertJSValToXPCObject(nsISupports** aSupports, REFNSIID aIID,
                                        JSContext* aContext, jsval aValue);

  static void ConvertJSValToString(nsAWritableString& aString,
                                   JSContext* aContext, jsval aValue);

  static PRBool ConvertJSValToUint32(PRUint32* aProp, JSContext* aContext,
                                     jsval aValue);

  static nsresult GetStaticScriptGlobal(JSContext* aContext, JSObject* aObj,
                                        nsIScriptGlobalObject** aGlobal);

  static nsresult GetStaticScriptContext(JSContext* aContext, JSObject* aObj,
                                         nsIScriptContext** aScriptContext);

  static nsresult GetDynamicScriptGlobal(JSContext *aContext,
                                         nsIScriptGlobalObject** aGlobal);

  static nsresult GetDynamicScriptContext(JSContext *aContext,
                                          nsIScriptContext** aScriptContext);

  static JSBool PR_CALLBACK CheckAccess(JSContext *cx, JSObject *obj,
                                        jsid id, JSAccessMode mode, jsval *vp);

  static nsIScriptSecurityManager *GetSecurityManager(JSContext *cx,
                                                      JSObject *obj);

  static void ClearCachedSecurityManager();

protected:
  static PRBool NameAndFormatForNSResult(nsresult rv, const char** name,
                                         const char** format);

  static nsIScriptSecurityManager *mCachedSecurityManager;
};

#endif /* nsJSUtils_h__ */
