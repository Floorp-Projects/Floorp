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

#ifndef nsIScriptContext_h__
#define nsIScriptContext_h__

#include "nscore.h"
#include "nsISupports.h"
#include "jsapi.h"

class nsIWebWidget;

#define NS_ISCRIPTCONTEXT_IID \
{ /* 8f6bca7d-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca7d, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

/**
 * It is used by the application to initialize a runtime and run scripts.
 * A script runtime would implement this interface.
 * <P><I>It does have a bit too much java script information now, that
 * should be removed in a short time. Ideally this interface will be
 * language neutral</I>
 */
class nsIScriptContext : public nsISupports {
public:
  /**
   * Execute a script.
   *
   * @param aScript a string representing the script to be executed
   * @param aScriptSize the length of aScript
   * @param aRetValue return value 
   *
   * @return true if the script was valid and got executed
   *
   **/
  virtual PRBool              EvaluateString(const char *aScript, 
                                              PRUint32 aScriptSize, 
                                              jsval *aRetValue) = 0;

  /**
   * Return the global object.
   *
   **/
  virtual JSObject*           GetGlobalObject() = 0;

  /**
   * Return the JSContext 
   *
   **/
  virtual JSContext*          GetContext() = 0;

  /**
   * Init all DOM classes.
   *
   **/
  virtual nsresult            InitAllClasses() = 0;

  /**
   * Init this context.
   * <P>
   * <I> The dependency with nsIWebWidget will disappear soon. A more general "global object"
   * interface should be defined. nsIWebWidget is too specific.</I>
   *
   * @param aGlobalObject the gobal object
   *
   * @return NS_OK if context initialization was successful
   *
   **/
  virtual nsresult            InitContext(nsIWebWidget *aGlobalObject) = 0;
};

/**
 * Return a new Context
 *
 */
extern "C" NS_DOM NS_CreateContext(nsIWebWidget *aGlobal, nsIScriptContext **aContext);

#endif // nsIScriptContext_h__

