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
#ifndef nsJSEnvironment_h___
#define nsJSEnvironment_h___

#include "nsIScriptContext.h"
#include "jsapi.h"

class nsIScriptSecurityManager;

class nsJSContext : public nsIScriptContext {
private:
  JSContext *mContext;

public:
  nsJSContext(JSRuntime *aRuntime);
  ~nsJSContext();

  NS_DECL_ISUPPORTS

  virtual PRBool       EvaluateString(const nsString& aScript, 
                                      const char *aURL,
                                      PRUint32 aLineNo,
                                      nsString& aRetValue,
                                      PRBool* aIsUndefined);
  virtual nsIScriptGlobalObject*    GetGlobalObject();
  virtual void*                     GetNativeContext();
  virtual nsresult     InitClasses();
  virtual nsresult     InitContext(nsIScriptGlobalObject *aGlobalObject);
  virtual nsresult     AddNamedReference(void *aSlot, void *aScriptObject,
                                         const char *aName);
  virtual nsresult RemoveReference(void *aSlot, void *aScriptObject);
  virtual nsresult GC();
  NS_IMETHOD GetSecurityManager(nsIScriptSecurityManager** aInstancePtr);
};

class nsJSEnvironment {
private:
  JSRuntime *mRuntime;

public:
  static nsJSEnvironment *sTheEnvironment;

  nsJSEnvironment();
  ~nsJSEnvironment();

  nsIScriptContext* GetNewContext();

  static nsJSEnvironment *GetScriptingEnvironment();

};

#endif /* nsJSEnvironment_h___ */
