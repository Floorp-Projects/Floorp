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
#include "nsStringGlue.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIProgrammingLanguage.h"

class nsIScriptGlobalObject;
class nsIScriptSecurityManager;
class nsIPrincipal;
class nsIAtom;
class nsIArray;
class nsIVariant;
class nsIObjectInputStream;
class nsIObjectOutputStream;
class nsScriptObjectHolder;

typedef void (*nsScriptTerminationFunc)(nsISupports* aRef);

#define NS_ISCRIPTCONTEXT_IID \
{ /* {52B46C37-A078-4952-AED7-035D83C810C0} */ \
  0x52b46c37, 0xa078, 0x4952, \
  {0xae, 0xd7, 0x3, 0x5d, 0x83, 0xc8, 0x10, 0xc0 } }

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
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTCONTEXT_IID)

  /* Get the ID of this language. */
  virtual PRUint32 GetScriptTypeID() = 0;

  /**
   * Compile and execute a script.
   *
   * @param aScript a string representing the script to be executed
   * @param aScopeObject a script object for the scope to execute in, or
   *                     nsnull to use a default scope
   * @param aPrincipal the principal that produced the script
   * @param aURL the URL or filename for error messages
   * @param aLineNo the starting line number of the script for error messages
   * @param aVersion the script language version to use when executing
   * @param aRetValue the result of executing the script, or null for no result.
   *        If this is a JS context, it's the caller's responsibility to
   *        preserve aRetValue from GC across this call
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
                                  PRUint32 aVersion,
                                  nsAString *aRetValue,
                                  PRBool* aIsUndefined) = 0;

  // Note JS bigotry remains here - 'void *aRetValue' is assumed to be a
  // jsval.  This must move to JSObject before it can be made agnostic.
  virtual nsresult EvaluateStringWithValue(const nsAString& aScript,
                                           void *aScopeObject,
                                           nsIPrincipal *aPrincipal,
                                           const char *aURL,
                                           PRUint32 aLineNo,
                                           PRUint32 aVersion,
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
   *                      the script.
   *
   * @return NS_OK if the script source was valid and got compiled.
   *
   **/
  virtual nsresult CompileScript(const PRUnichar* aText,
                                 PRInt32 aTextLength,
                                 void* aScopeObject,
                                 nsIPrincipal* aPrincipal,
                                 const char* aURL,
                                 PRUint32 aLineNo,
                                 PRUint32 aVersion,
                                 nsScriptObjectHolder &aScriptObject) = 0;

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
   * into a function object returned if ok via aHandler.  Does NOT bind the
   * function to anything - BindCompiledEventHandler() should be used for that
   * purpose.  Note that this event handler is always considered 'shared' and
   * hence is compiled without principals.  Never call the returned object
   * directly - it must be bound (and thereby cloned, and therefore have the 
   * correct principals) before use!
   *
   * @param aName an nsIAtom pointer naming the function; it must be lowercase
   *        and ASCII, and should not be longer than 63 chars.  This bound on
   *        length is enforced only by assertions, so caveat caller!
   * @param aEventName the name that the event object should be bound to
   * @param aBody the event handler function's body
   * @param aURL the URL or filename for error messages
   * @param aLineNo the starting line number of the script for error messages
   * @param aHandler the out parameter in which a void pointer to the compiled
   *        function object is stored on success
   *
   * @return NS_OK if the function body was valid and got compiled
   */
  virtual nsresult CompileEventHandler(nsIAtom* aName,
                                       PRUint32 aArgCount,
                                       const char** aArgNames,
                                       const nsAString& aBody,
                                       const char* aURL, PRUint32 aLineNo,
                                       nsScriptObjectHolder &aHandler) = 0;

  /**
   * Call the function object with given args and return its boolean result,
   * or true if the result isn't boolean.
   *
   * @param aTarget the event target
   * @param aScript an object telling the scope in which to call the compiled
   *        event handler function.
   * @param aHandler function object (function and static scope) to invoke.
   * @param argv array of arguments.  Note each element is assumed to
   *        be an nsIVariant.
   * @param rval out parameter returning result
   **/
  virtual nsresult CallEventHandler(nsISupports* aTarget,
                                    void *aScope, void* aHandler,
                                    nsIArray *argv, nsIVariant **rval) = 0;

  /**
   * Bind an already-compiled event handler function to a name in the given
   * scope object.  The same restrictions on aName (lowercase ASCII, not too
   * long) applies here as for CompileEventHandler.  Scripting languages with
   * static scoping must re-bind the scope chain for aHandler to begin (after
   * the activation scope for aHandler itself, typically) with aTarget's scope.
   *
   * Logically, this 'bind' operation is more of a 'copy' - it simply
   * stashes/associates the event handler function with the event target, so
   * it can be fetched later with GetBoundEventHandler().
   *
   * @param aTarget an object telling the scope in which to bind the compiled
   *        event handler function.  The context will presumably associate
   *        this nsISupports with a native script object.
   * @param aName an nsIAtom pointer naming the function; it must be lowercase
   *        and ASCII, and should not be longer than 63 chars.  This bound on
   *        length is enforced only by assertions, so caveat caller!
   * @param aHandler the function object to name, created by an earlier call to
   *        CompileEventHandler
   * @return NS_OK if the function was successfully bound to the name
   *
   * XXXmarkh - fold this in with SetProperty?  Exactly the same concept!
   */
  virtual nsresult BindCompiledEventHandler(nsISupports* aTarget, void *aScope,
                                            nsIAtom* aName,
                                            void* aHandler) = 0;

  /**
   * Lookup a previously bound event handler for the specified target.  This
   * will return an object equivilent to the one passed to
   * BindCompiledEventHandler (although the pointer may not be the same).
   *
   */
  virtual nsresult GetBoundEventHandler(nsISupports* aTarget, void *aScope,
                                        nsIAtom* aName,
                                        nsScriptObjectHolder &aHandler) = 0;

  /**
   * Compile a function that isn't used as an event handler.
   *
   * NOTE: Not yet language agnostic (main problem is XBL - not yet agnostic)
   * Caller must make sure aFunctionObject is a JS GC root.
   *
   **/
  virtual nsresult CompileFunction(void* aTarget,
                                   const nsACString& aName,
                                   PRUint32 aArgCount,
                                   const char** aArgArray,
                                   const nsAString& aBody,
                                   const char* aURL,
                                   PRUint32 aLineNo,
                                   PRBool aShared,
                                   void **aFunctionObject) = 0;

  /**
   * Set the default scripting language version for this context, which must
   * be a context specific to a particular scripting language.
   *
   **/
  virtual void SetDefaultLanguageVersion(PRUint32 aVersion) = 0;

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
   * Return the native global object for this context.
   *
   **/
  virtual void *GetNativeGlobal() = 0;

  /**
   * Create a new global object that will be used for an inner window.
   * Return the native global and an nsISupports 'holder' that can be used
   * to manage the lifetime of it.
   */
  virtual nsresult CreateNativeGlobalForInner(
                                      nsIScriptGlobalObject *aNewInner,
                                      PRBool aIsChrome,
                                      void **aNativeGlobal,
                                      nsISupports **aHolder) = 0;

  /**
   * Connect this context to a new inner window, to allow "prototype"
   * chaining from the inner to the outer.
   * Called after both the the inner and outer windows are initialized
   **/
  virtual nsresult ConnectToInner(nsIScriptGlobalObject *aNewInner,
                                  void *aOuterGlobal) = 0;


  /**
   * Init this context ready for use.  If aGlobalObject is not NULL, this
   * function may initialize based on this global (for example, using the
   * global to locate a chrome window, create a new 'scope' for this
   * global, etc)
   *
   * @param aGlobalObject the gobal object, which may be nsnull.
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
   * Called as the global object discards its reference to the context.
   */
  virtual void FinalizeContext() = 0;

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

  virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                             void *aScriptObject) = 0;
  
  /* Deserialize a script from a stream.
   */
  virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                               nsScriptObjectHolder &aResult) = 0;

  /**
   * JS only - this function need not be implemented by languages other
   * than JS (ie, this should be moved to a private interface!)
   * Called to specify a function that should be called when the current
   * script (if there is one) terminates. Generally used if breakdown
   * of script state needs to happen, but should be deferred till
   * the end of script evaluation.
   *
   * @throws NS_ERROR_OUT_OF_MEMORY if that happens
   */
  virtual nsresult SetTerminationFunction(nsScriptTerminationFunc aFunc,
                                          nsISupports* aRef) = 0;

  /**
   * Called to disable/enable script execution in this context.
   */
  virtual PRBool GetScriptsEnabled() = 0;
  virtual void SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts) = 0;

  // SetProperty is suspect and jst believes should not be needed.  Currenly
  // used only for "arguments".
  virtual nsresult SetProperty(void *aTarget, const char *aPropName, nsISupports *aVal) = 0;
  /** 
   * Called to set/get information if the script context is
   * currently processing a script tag
   */
  virtual PRBool GetProcessingScriptTag() = 0;
  virtual void SetProcessingScriptTag(PRBool aResult) = 0;

  /**
   * Tell the context whether or not to GC when destroyed.  An optimization
   * used when the window is a [i]frame, so GC will happen anyway.
   */
  virtual void SetGCOnDestruction(PRBool aGCOnDestruction) = 0;

  /**
   * Initialize DOM classes on aGlobalObj, always call
   * WillInitializeContext() before calling InitContext(), and always
   * call DidInitializeContext() when a context is fully
   * (successfully) initialized.
   */
  virtual nsresult InitClasses(void *aGlobalObj) = 0;

  /**
   * Clear the scope object - may be called either as we are being torn down,
   * or before we are attached to a different document.
   *
   * aClearFromProtoChain is probably somewhat JavaScript specific.  It
   * indicates that the global scope polluter should be removed from the
   * prototype chain and that the objects in the prototype chain should
   * also have their scopes cleared.  We don't do this all the time
   * because the prototype chain is shared between inner and outer
   * windows, and needs to stay with inner windows that we're keeping
   * around.
   */
  virtual void ClearScope(void* aGlobalObj, PRBool aClearFromProtoChain) = 0;

  /**
   * Tell the context we're about to be reinitialize it.
   */
  virtual void WillInitializeContext() = 0;

  /**
   * Tell the context we're done reinitializing it.
   */
  virtual void DidInitializeContext() = 0;

  /**
   * Tell the context our global has a new document, and the scope
   * used by it.  Use nsISupports to avoid dependency issues - but expect
   * a QI for nsIDOMDocument and/or nsIDocument.
   */
  virtual void DidSetDocument(nsISupports *aDoc, void *aGlobal) = 0;

  /* Memory managment for script objects.  Used by the implementation of
   * nsScriptObjectHolder to manage the lifetimes of the held script objects.
   *
   * See also nsIScriptRuntime, which has identical methods and is useful
   * in situations when you do not have an nsIScriptContext.
   * 
   */
  virtual nsresult DropScriptObject(void *object) = 0;
  virtual nsresult HoldScriptObject(void *object) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptContext, NS_ISCRIPTCONTEXT_IID)

#endif // nsIScriptContext_h__

