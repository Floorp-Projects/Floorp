/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
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

#ifndef nsIScriptContext_h__
#define nsIScriptContext_h__

#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"

class nsIScriptGlobalObject;
class nsIScriptSecurityManager;
class nsIScriptContextOwner;
class nsIPrincipal;
class nsIAtom;

typedef void (*nsScriptTerminationFunc)(nsISupports* aRef);

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
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISCRIPTCONTEXT_IID)

  /**
   * Compile and execute a script.
   *
   * @param aScript a string representing the script to be executed
   * @param aScopeObject a JavaScript JSObject for the scope to execute in, or
   *                     nsnull to use a default scope
   * @param aPrincipal the principal that produced the script
   * @param aURL the URL or filename for error messages
   * @param aLineNo the starting line number for the script for error messages
   * @param aVersion the script language version to use when executing
   * @param aRetValue the result of executing the script
   * @param aIsUndefined true if the result of executing the script is the
   *                     undefined value
   *
   * @return NS_OK if the script was valid and got executed
   *
   **/
  NS_IMETHOD EvaluateString(const nsAReadableString& aScript,
                            void *aScopeObject,
                            nsIPrincipal *aPrincipal,
                            const char *aURL,
                            PRUint32 aLineNo,
                            const char* aVersion,
                            nsAWritableString& aRetValue,
                            PRBool* aIsUndefined) = 0;

  NS_IMETHOD EvaluateStringWithValue(const nsAReadableString& aScript,
                                     void *aScopeObject,
                                     nsIPrincipal *aPrincipal,
                                     const char *aURL,
                                     PRUint32 aLineNo,
                                     const char* aVersion,
                                     void* aRetValue,
                                     PRBool* aIsUndefined) = 0;

  /**
   * Compile a script.
   *
   * @param aText a PRUnichar buffer containing script source
   * @param aTextLength number of characters in aText
   * @param aScopeObject an object telling the scope in which to execute,
   *                     or nsnull to use a default scope
   * @param aPrincipal the principal that produced the script
   * @param aURL the URL or filename for error messages
   * @param aLineNo the starting line number for the script for error messages
   * @param aVersion the script language version to use when executing
   * @param aScriptObject an executable object that's the result of compiling
   *                      the script.  The caller is responsible for GC rooting
   *                      this object.
   *
   * @return NS_OK if the script source was valid and got compiled
   *
   **/
  NS_IMETHOD CompileScript(const PRUnichar* aText,
                           PRInt32 aTextLength,
                           void* aScopeObject,
                           nsIPrincipal* aPrincipal,
                           const char* aURL,
                           PRUint32 aLineNo,
                           const char* aVersion,
                           void** aScriptObject) = 0;

  /**
   * Execute a precompiled script object.
   *
   * @param aScriptObject an object representing the script to be executed
   * @param aScopeObject an object telling the scope in which to execute,
   *                     or nsnull to use a default scope
   * @param aRetValue the result of executing the script, may be null in
   *                  which case no result string is computed
   * @param aIsUndefined true if the result of executing the script is the
   *                     undefined value, may be null for "don't care"
   *
   * @return NS_OK if the script was valid and got executed
   *
   */
  NS_IMETHOD ExecuteScript(void* aScriptObject,
                           void* aScopeObject,
                           nsAWritableString* aRetValue,
                           PRBool* aIsUndefined) = 0;

  /**
   * Compile the event handler named by atom aName, with function body aBody
   * into a function object returned if ok via *aHandler.  Bind the lowercase
   * ASCII name to the function in its parent object aTarget.
   *
   * @param aTarget an object telling the scope in which to bind the compiled
   *        event handler function to aName.
   * @param aName an nsIAtom pointer naming the function; it must be lowercase
   *        and ASCII, and should not be longer than 63 chars.  This bound on
   *        length is enforced only by assertions, so caveat caller!
   * @param aBody the event handler function's body
   * @param aShared flag telling whether the compiled event handler will be
   *        shared via nsIScriptEventHandlerOwner, in which case any static
   *        link compiled into it based on aTarget should be cleared, both
   *        to avoid entraining garbage to be collected, and to trigger static
   *        link re-binding in BindCompiledEventHandler (see below).
   * @param aHandler the out parameter in which a void pointer to the compiled
   *        function object is returned on success; may be null, meaning the
   *        caller doesn't need to store the handler for later use.
   *
   * @return NS_OK if the function body was valid and got compiled
   */
  NS_IMETHOD CompileEventHandler(void* aTarget,
                                 nsIAtom* aName,
                                 const nsAReadableString& aBody,
                                 PRBool aShared,
                                 void** aHandler) = 0;

  /**
   * Call the function object with given args and return its boolean result,
   * or true if the result isn't boolean.
   *
   * @param aTarget an object telling the scope in which to bind the compiled
   *        event handler function.
   * @param aHandler function object (function and static scope) to invoke.
   * @param argc actual argument count; length of argv
   * @param argv vector of arguments; length is argc
   * @param aBoolResult out parameter returning boolean function result, or
   *        true if the result was not boolean.
   **/
  NS_IMETHOD CallEventHandler(void* aTarget, void* aHandler,
                              PRUint32 argc, void* argv,
                              PRBool* aBoolResult, PRBool aReverseReturnResult) = 0;

  /**
   * Bind an already-compiled event handler function to a name in the given
   * scope object.  The same restrictions on aName (lowercase ASCII, not too
   * long) applies here as for CompileEventHandler.  Scripting languages with
   * static scoping must re-bind the scope chain for aHandler to begin (after
   * the activation scope for aHandler itself, typically) with aTarget's scope.
   *
   * @param aTarget an object telling the scope in which to bind the compiled
   *        event handler function.
   * @param aName an nsIAtom pointer naming the function; it must be lowercase
   *        and ASCII, and should not be longer than 63 chars.  This bound on
   *        length is enforced only by assertions, so caveat caller!
   * @param aHandler the function object to name, created by an earlier call to
   *        CompileEventHandler
   * @return NS_OK if the function was successfully bound to the name
   */
  NS_IMETHOD BindCompiledEventHandler(void* aTarget,
                                      nsIAtom* aName,
                                      void* aHandler) = 0;

  NS_IMETHOD CompileFunction(void* aTarget,
                             const nsCString& aName,
                             PRUint32 aArgCount,
                             const char** aArgArray,
                             const nsAReadableString& aBody,
                             const char* aURL,
                             PRUint32 aLineNo,
                             PRBool aShared,
                             void** aFunctionObject) = 0;


  /**
   * Set the default scripting language version for this context, which must
   * be a context specific to a particular scripting language.
   *
   **/
  NS_IMETHOD SetDefaultLanguageVersion(const char* aVersion) = 0;

  /**
   * Return the global object.
   *
   **/
  NS_IMETHOD GetGlobalObject(nsIScriptGlobalObject** aGlobalObject) = 0;

  /**
   * Return the native script context
   *
   **/
  NS_IMETHOD_(void*) GetNativeContext() = 0;

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
   * @return NS_OK if initialized, NS_ERROR_NOT_INITIALIZED if not
   *
   */
  NS_IMETHOD IsContextInitialized() = 0;

  /**
   * For garbage collected systems, do a synchronous collection pass.
   * May be a no-op on other systems
   *
   * @return NS_OK if the method is successful
   */
  NS_IMETHOD GC() = 0;

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
   * @param aTerminated If true then call termination function if it was 
   *    previously set. Within DOM this will always be true, but outside 
   *    callers (such as xpconnect) who may do script evaluations nested
   *    inside DOM script evaluations can pass false to avoid premature
   *    calls to the termination function.
   * @return NS_OK if the method is successful
   */
  NS_IMETHOD ScriptEvaluated(PRBool aTerminated) = 0;

  /**
   * Let the script context know who its owner is.
   * The script context should not addref the owner. It
   * will be told when the owner goes away.
   * @return NS_OK if the method is successful
   */
  NS_IMETHOD SetOwner(nsIScriptContextOwner* owner) = 0;

  /**
   * Get the script context of the owner. The method
   * addrefs the returned reference according to regular
   * XPCOM rules, even though the internal reference itself
   * is a "weak" reference.
   */
  NS_IMETHOD GetOwner(nsIScriptContextOwner** owner) = 0;

  /**
   * Called to specify a function that should be called when the current
   * script (if there is one) terminates. Generally used if breakdown
   * of script state needs to be happen, but should be deferred till
   * the end of script evaluation.
   */
  NS_IMETHOD SetTerminationFunction(nsScriptTerminationFunc aFunc,
                                    nsISupports* aRef) = 0;

  /**
   * Called to disable/enable script execution in this context.
   */
  NS_IMETHOD GetScriptsEnabled(PRBool *aEnabled) = 0;
  NS_IMETHOD SetScriptsEnabled(PRBool aEnabled) = 0;

  /** 
   * Called to set/get information if the script context is
   * currently processing a script tag
   */
  NS_IMETHOD GetProcessingScriptTag(PRBool * aResult) =0;
  NS_IMETHOD SetProcessingScriptTag(PRBool  aResult) =0;
};

#endif // nsIScriptContext_h__

