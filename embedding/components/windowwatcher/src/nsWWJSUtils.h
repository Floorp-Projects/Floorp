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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsWWJSUtils_h__
#define nsWWJSUtils_h__

/**
 * Utility functions copied from nsJSUtils in dom/src/base.
 */

#include "nsISupports.h"
#include "jsapi.h"

class nsIScriptContext;
class nsIScriptGlobalObject;

class nsWWJSUtils {
public:
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
};

#endif /* nsWWJSUtils_h__ */
