/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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

class nsJSUtils {
public:
  static PRBool nsCallJSScriptObjectGetProperty(nsISupports* aSupports,
                                                JSContext* aContext,
                                                jsval aId,
                                                jsval* aReturn);

  static PRBool nsLookupGlobalName(nsISupports* aSupports,
                                   JSContext* aContext,
                                   jsval aId,
                                   jsval* aReturn);

  static PRBool nsCallJSScriptObjectSetProperty(nsISupports* aSupports,
                                                JSContext* aContext,
                                                jsval aId,
                                                jsval* aReturn);

  static void nsConvertObjectToJSVal(nsISupports* aSupports,
                                     JSContext* aContext,
                                     jsval* aReturn);

  static void nsConvertStringToJSVal(const nsString& aProp,
                                     JSContext* aContext,
                                     jsval* aReturn);

  static PRBool nsConvertJSValToObject(nsISupports** aSupports,
                                       REFNSIID aIID,
                                       const nsString& aTypeName,
                                       JSContext* aContext,
                                       jsval aValue);

  static void nsConvertJSValToString(nsString& aString,
                                     JSContext* aContext,
                                     jsval aValue);

  static PRBool nsConvertJSValToBool(PRBool* aProp,
                                     JSContext* aContext,
                                     jsval aValue);

  static void nsGenericFinalize(JSContext* aContext,
                                JSObject* aObj);
		     
  static JSBool nsGenericEnumerate(JSContext* aContext,
                                   JSObject* aObj);

  static JSBool nsGlobalResolve(JSContext* aContext,
                                JSObject* aObj, 
                                jsval aId);

  static JSBool nsGenericResolve(JSContext* aContext,
                                 JSObject* aObj, 
                                 jsval aId);
};

#endif /* nsJSUtils_h__ */
