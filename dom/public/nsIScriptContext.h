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
#include "nsString.h"
#include "nsISupports.h"

class nsIScriptGlobalObject;
class nsIScriptSecurityManager;
class nsIScriptNameSpaceManager;

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
  static const nsIID& GetIID() { static nsIID iid = NS_ISCRIPTCONTEXT_IID; return iid; }

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
  NS_IMETHOD_(PRBool) EvaluateString(const nsString& aScript, 
                                     const char *aURL,
                                     PRUint32 aLineNo,
                                     nsString& aRetValue,
                                     PRBool* aIsUndefined) = 0;

  /**
   * Return the global object.
   *
   **/
  NS_IMETHOD_(nsIScriptGlobalObject*) GetGlobalObject() = 0;

  /**
   * Return the native script context
   *
   **/
  NS_IMETHOD_(void*) GetNativeContext() = 0;

  /**
   * Init all DOM classes.
   *
   **/
  NS_IMETHOD InitClasses() = 0;

  /**
   * Init this context.
   *
   * @param aGlobalObject the gobal object
   *
   * @return NS_OK if context initialization was successful
   *
   **/
  NS_IMETHOD InitContext(nsIScriptGlobalObject *aGlobalObject) = 0;

  /**
   * Check to see if context is as yet intialized. Used to prevent
   * reentrancy issues during the initialization process.
   *
   * @return NS_OK if initialized, NS_COMFALSE if not 
   *
   */
  NS_IMETHOD IsContextInitialized() = 0;

  /**
   * Add a reference to a script object. For garbage collected systems
   * the address of a slot to be used as a root is also provided. For
   * reference counted systems, the object is provided.
   * 
   * @param aSlot Slot to use as a root for garbage collected systems
   * @param aScriptObject Script object being referenced
   * @param aName Name of the reference (could be null)
   *
   * @return NS_OK if the method is successful
   */
  NS_IMETHOD AddNamedReference(void *aSlot, void *aScriptObject,
                                     const char *aName) = 0;

  /**
   * Remove a reference to a script object. For garbage collected 
   * systems, this is equivalent to removing a root.
   *
   * @param aSlot Slot corresponding to the removed root
   * @param aScriptObject script object to whom a reference is released
   *
   * @return NS_OK if the method is successful
   */
  NS_IMETHOD RemoveReference(void *aSlot, void *aScriptObject) = 0;

  /**
   * For garbage collected systems, do a synchronous collection pass.
   * May be a no-op on other systems
   *
   * @return NS_OK if the method is successful
   */
  NS_IMETHOD GC() = 0;
  
  /**
   * Get the name space manager for this context.
   * @return NS_OK if the method is successful
   */
  NS_IMETHOD GetNameSpaceManager(nsIScriptNameSpaceManager** aInstancePtr) = 0;

  /**
   * Get the security manager for this context.
   * @return NS_OK if the method is successful
   */
  NS_IMETHOD GetSecurityManager(nsIScriptSecurityManager** aInstancePtr) = 0;

  /**
   * Inform the context that a script was evaluated.
   * A GC may be done if "necessary."
   * This call is necessary if script evaluation is done
   * without using the EvaluateScript method.
   * @return NS_OK if the method is successful
   */
  NS_IMETHOD ScriptEvaluated(void) = 0;
};

/**
 * Return a new Context
 *
 */
extern "C" NS_DOM nsresult NS_CreateContext(nsIScriptGlobalObject *aGlobal, nsIScriptContext **aContext);

#endif // nsIScriptContext_h__

