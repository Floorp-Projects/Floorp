/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIScriptContext_h__
#define nsIScriptContext_h__

#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "jsapi.h"

class nsIScriptGlobalObject;
class nsIScriptSecurityManager;
class nsIScriptContextOwner;
class nsIPrincipal;
class nsIAtom;

typedef void (*nsScriptTerminationFunc)(nsISupports* aRef);

#define NS_ISCRIPTCONTEXT_IID \
{ /* b3fd8821-b46d-4160-913f-cc8fe8176f5f */ \
  0xb3fd8821, 0xb46d, 0x4160, \
  {0x91, 0x3f, 0xcc, 0x8f, 0xe8, 0x17, 0x6f, 0x5f} }

/**
 * It is used by the application to initialize a runtime and run scripts.
 * A script runtime would implement this interface.
 * <P><I>It does have a bit too much java script information now, that
 * should be removed in a short time. Ideally this interface will be
 * language neutral</I>
 */
class nsIScriptContext : public nsISupports
{
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
   * @param aLineNo the starting line number of the script for error messages
   * @param aVersion the script language version to use when executing
   * @param aRetValue the result of executing the script, or null for no result
   * @param aIsUndefined true if the result of executing the script is the
   *                     undefined value
   *
   * @return NS_OK if the script was valid and got executed
   *
   **/
  virtual nsresult EvaluateString(const nsAString& aScript,
                                  void *aScopeObject,
                                  nsIPrincipal *aPrincipal,
                                  const char *aURL,
                                  PRUint32 aLineNo,
                                  const char* aVersion,
                                  nsAString *aRetValue,
                                  PRBool* aIsUndefined) = 0;

  virtual nsresult EvaluateStringWithValue(const nsAString& aScript,
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
   * @param aLineNo the starting line number of the script for error messages
   * @param aVersion the script language version to use when executing
   * @param aScriptObject an executable object that's the result of compiling
   *                      the script.  The caller is responsible for GC rooting
   *                      this object.
   *
   * @return NS_OK if the script source was valid and got compiled
   *
   **/
  virtual nsresult CompileScript(const PRUnichar* aText,
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
  virtual nsresult ExecuteScript(void* aScriptObject,
                                 void* aScopeObject,
                                 nsAString* aRetValue,
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
   * @param aEventName the name that the event object should be bound to
   * @param aBody the event handler function's body
   * @param aURL the URL or filename for error messages
   * @param aLineNo the starting line number of the script for error messages
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
  virtual nsresult CompileEventHandler(void* aTarget,
                                       nsIAtom* aName,
                                       const char* aEventName,
                                       const nsAString& aBody,
                                       const char* aURL,
                                       PRUint32 aLineNo,
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
  virtual nsresult CallEventHandler(JSObject* aTarget, JSObject* aHandler,
                                    uintN argc, jsval* argv,
                                    jsval* rval) = 0;

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
  virtual nsresult BindCompiledEventHandler(void* aTarget,
                                            nsIAtom* aName,
                                            void* aHandler) = 0;

  virtual nsresult CompileFunction(void* aTarget,
                                   const nsACString& aName,
                                   PRUint32 aArgCount,
                                   const char** aArgArray,
                                   const nsAString& aBody,
                                   const char* aURL,
                                   PRUint32 aLineNo,
                                   PRBool aShared,
                                   void** aFunctionObject) = 0;


  /**
   * Set the default scripting language version for this context, which must
   * be a context specific to a particular scripting language.
   *
   **/
  virtual void SetDefaultLanguageVersion(const char* aVersion) = 0;

  /**
   * Return the global object.
   *
   **/
  virtual nsIScriptGlobalObject *GetGlobalObject() = 0;

  /**
   * Return the native script context
   *
   **/
  virtual void *GetNativeContext() = 0;

  /**
   * Init this context.
   *
   * @param aGlobalObject the gobal object
   *
   * @return NS_OK if context initialization was successful
   *
   **/
  virtual nsresult InitContext(nsIScriptGlobalObject *aGlobalObject) = 0;

  /**
   * Check to see if context is as yet intialized. Used to prevent
   * reentrancy issues during the initialization process.
   *
   * @return PR_TRUE if initialized, PR_FALSE if not
   *
   */
  virtual PRBool IsContextInitialized() = 0;

  /**
   * For garbage collected systems, do a synchronous collection pass.
   * May be a no-op on other systems
   *
   * @return NS_OK if the method is successful
   */
  virtual void GC() = 0;

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
  virtual void ScriptEvaluated(PRBool aTerminated) = 0;

  /**
   * Let the script context know who its owner is.
   * The script context should not addref the owner. It
   * will be told when the owner goes away.
   * @return NS_OK if the method is successful
   */
  virtual void SetOwner(nsIScriptContextOwner* owner) = 0;

  /**
   * Get the script context of the owner. The method
   * addrefs the returned reference according to regular
   * XPCOM rules, even though the internal reference itself
   * is a "weak" reference.
   */
  virtual nsIScriptContextOwner *GetOwner() = 0;

  /**
   * Called to specify a function that should be called when the current
   * script (if there is one) terminates. Generally used if breakdown
   * of script state needs to be happen, but should be deferred till
   * the end of script evaluation.
   */
  virtual void SetTerminationFunction(nsScriptTerminationFunc aFunc,
                                      nsISupports* aRef) = 0;

  /**
   * Called to disable/enable script execution in this context.
   */
  virtual PRBool GetScriptsEnabled() = 0;
  virtual void SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts) = 0;

  /** 
   * Called to set/get information if the script context is
   * currently processing a script tag
   */
  virtual PRBool GetProcessingScriptTag() = 0;
  virtual void SetProcessingScriptTag(PRBool aResult) = 0;

  /**
   * Tell the context whether or not to GC when destroyed.
   */
  virtual void SetGCOnDestruction(PRBool aGCOnDestruction) = 0;
};

inline nsIScriptContext *
GetScriptContextFromJSContext(JSContext *cx)
{
  if (!(::JS_GetOptions(cx) & JSOPTION_PRIVATE_IS_NSISUPPORTS)) {
    return nsnull;
  }

  nsCOMPtr<nsIScriptContext> scx =
    do_QueryInterface(NS_STATIC_CAST(nsISupports *,
                                     ::JS_GetContextPrivate(cx)));

  // This will return a pointer to something that's about to be
  // released, but that's ok here.
  return scx;
}

#endif // nsIScriptContext_h__

