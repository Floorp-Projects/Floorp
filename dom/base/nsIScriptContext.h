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
#include "jspubtd.h"

class nsIScriptGlobalObject;
class nsIScriptSecurityManager;
class nsIPrincipal;
class nsIAtom;
class nsIArray;
class nsIVariant;
class nsIObjectInputStream;
class nsIObjectOutputStream;
class nsScriptObjectHolder;
class nsIScriptObjectPrincipal;

typedef void (*nsScriptTerminationFunc)(nsISupports* aRef);

#define NS_ISCRIPTCONTEXTPRINCIPAL_IID \
  { 0xd012cdb3, 0x8f1e, 0x4440, \
    { 0x8c, 0xbd, 0x32, 0x7f, 0x98, 0x1d, 0x37, 0xb4 } }

class nsIScriptContextPrincipal : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTCONTEXTPRINCIPAL_IID)

  virtual nsIScriptObjectPrincipal* GetObjectPrincipal() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptContextPrincipal,
                              NS_ISCRIPTCONTEXTPRINCIPAL_IID)

#define NS_ISCRIPTCONTEXT_IID \
{ 0x164ea909, 0x5cee, 0x4e20, \
  { 0x9f, 0xed, 0x43, 0x13, 0xab, 0xac, 0x1c, 0xd3 } }

/* This MUST match JSVERSION_DEFAULT.  This version stuff if we don't
   know what language we have is a little silly... */
#define SCRIPTVERSION_DEFAULT JSVERSION_DEFAULT

/**
 * It is used by the application to initialize a runtime and run scripts.
 * A script runtime would implement this interface.
 */
class nsIScriptContext : public nsIScriptContextPrincipal
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
                                  JSObject* aScopeObject,
                                  nsIPrincipal *aPrincipal,
                                  const char *aURL,
                                  PRUint32 aLineNo,
                                  PRUint32 aVersion,
                                  nsAString *aRetValue,
                                  bool* aIsUndefined) = 0;

  virtual nsresult EvaluateStringWithValue(const nsAString& aScript,
                                           JSObject* aScopeObject,
                                           nsIPrincipal *aPrincipal,
                                           const char *aURL,
                                           PRUint32 aLineNo,
                                           PRUint32 aVersion,
                                           JS::Value* aRetValue,
                                           bool* aIsUndefined) = 0;

  /**
   * Compile a script.
   *
   * @param aText a PRUnichar buffer containing script source
   * @param aTextLength number of characters in aText
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
  virtual nsresult ExecuteScript(JSScript* aScriptObject,
                                 JSObject* aScopeObject,
                                 nsAString* aRetValue,
                                 bool* aIsUndefined) = 0;

  /**
   * Compile the event handler named by atom aName, with function body aBody
   * into a function object returned if ok via aHandler.  Does NOT bind the
   * function to anything - BindCompiledEventHandler() should be used for that
   * purpose.  Note that this event handler is always considered 'shared' and
   * hence is compiled without principals.  Never call the returned object
   * directly - it must be bound (and thereby cloned, and therefore have the 
   * correct principals) before use!
   *
   * If the compilation sets a pending exception on the native context, it is
   * this method's responsibility to report said exception immediately, without
   * relying on callers to do so.
   *
   *
   * @param aName an nsIAtom pointer naming the function; it must be lowercase
   *        and ASCII, and should not be longer than 63 chars.  This bound on
   *        length is enforced only by assertions, so caveat caller!
   * @param aEventName the name that the event object should be bound to
   * @param aBody the event handler function's body
   * @param aURL the URL or filename for error messages
   * @param aLineNo the starting line number of the script for error messages
   * @param aVersion the script language version to use when executing
   * @param aHandler the out parameter in which a void pointer to the compiled
   *        function object is stored on success
   *
   * @return NS_OK if the function body was valid and got compiled
   */
  virtual nsresult CompileEventHandler(nsIAtom* aName,
                                       PRUint32 aArgCount,
                                       const char** aArgNames,
                                       const nsAString& aBody,
                                       const char* aURL,
                                       PRUint32 aLineNo,
                                       PRUint32 aVersion,
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
                                    JSObject* aScope, void* aHandler,
                                    nsIArray *argv, nsIVariant **rval) = 0;

  /**
   * Bind an already-compiled event handler function to the given
   * target.  Scripting languages with static scoping must re-bind the
   * scope chain for aHandler to begin (after the activation scope for
   * aHandler itself, typically) with aTarget's scope.
   *
   * The result of the bind operation is a new handler object, with
   * principals now set and scope set as above.  This is returned in
   * aBoundHandler.  When this function is called, aBoundHandler is
   * expected to not be holding an object.
   *
   * @param aTarget an object telling the scope in which to bind the compiled
   *        event handler function.  The context will presumably associate
   *        this nsISupports with a native script object.
   * @param aScope the scope in which the script object for aTarget should be
   *        looked for.
   * @param aHandler the function object to bind, created by an earlier call to
   *        CompileEventHandler
   * @param aBoundHandler [out] the result of the bind operation.
   * @return NS_OK if the function was successfully bound
   */
  virtual nsresult BindCompiledEventHandler(nsISupports* aTarget,
                                            JSObject* aScope,
                                            void* aHandler,
                                            nsScriptObjectHolder& aBoundHandler) = 0;

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
                                   PRUint32 aVersion,
                                   bool aShared,
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
  virtual JSContext* GetNativeContext() = 0;

  /**
   * Return the native global object for this context.
   *
   **/
  virtual JSObject* GetNativeGlobal() = 0;

  /**
   * Create a new global object that will be used for an inner window.
   * Return the native global and an nsISupports 'holder' that can be used
   * to manage the lifetime of it.
   */
  virtual nsresult CreateNativeGlobalForInner(
                                      nsIScriptGlobalObject *aNewInner,
                                      bool aIsChrome,
                                      nsIPrincipal *aPrincipal,
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
   * Initialize the context generally. Does not create a global object.
   **/
  virtual nsresult InitContext() = 0;

  /**
   * Creates the outer window for this context.
   *
   * @param aGlobalObject The script global object to use as our global.
   */
  virtual nsresult CreateOuterObject(nsIScriptGlobalObject *aGlobalObject,
                                     nsIScriptGlobalObject *aCurrentInner) = 0;

  /**
   * Given an outer object, updates this context with that outer object.
   */
  virtual nsresult SetOuterObject(void *aOuterObject) = 0;

  /**
   * Prepares this context for use with the current inner window for the
   * context's global object. This must be called after CreateOuterObject.
   */
  virtual nsresult InitOuterWindow() = 0;

  /**
   * Check to see if context is as yet intialized. Used to prevent
   * reentrancy issues during the initialization process.
   *
   * @return true if initialized, false if not
   *
   */
  virtual bool IsContextInitialized() = 0;

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
  virtual void ScriptEvaluated(bool aTerminated) = 0;

  virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                             JSScript* aScriptObject) = 0;
  
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
  virtual bool GetScriptsEnabled() = 0;
  virtual void SetScriptsEnabled(bool aEnabled, bool aFireTimeouts) = 0;

  // SetProperty is suspect and jst believes should not be needed.  Currenly
  // used only for "arguments".
  virtual nsresult SetProperty(void *aTarget, const char *aPropName, nsISupports *aVal) = 0;
  /** 
   * Called to set/get information if the script context is
   * currently processing a script tag
   */
  virtual bool GetProcessingScriptTag() = 0;
  virtual void SetProcessingScriptTag(bool aResult) = 0;

  /**
   * Called to find out if this script context might be executing script.
   */
  virtual bool GetExecutingScript() = 0;

  /**
   * Tell the context whether or not to GC when destroyed.  An optimization
   * used when the window is a [i]frame, so GC will happen anyway.
   */
  virtual void SetGCOnDestruction(bool aGCOnDestruction) = 0;

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
  virtual void ClearScope(void* aGlobalObj, bool aClearFromProtoChain) = 0;

  /**
   * Tell the context we're about to be reinitialize it.
   */
  virtual void WillInitializeContext() = 0;

  /**
   * Tell the context we're done reinitializing it.
   */
  virtual void DidInitializeContext() = 0;

  /* Memory managment for script objects.  Used by the implementation of
   * nsScriptObjectHolder to manage the lifetimes of the held script objects.
   *
   * See also nsIScriptRuntime, which has identical methods and is useful
   * in situations when you do not have an nsIScriptContext.
   * 
   */
  virtual nsresult DropScriptObject(void *object) = 0;
  virtual nsresult HoldScriptObject(void *object) = 0;

  virtual void EnterModalState() = 0;
  virtual void LeaveModalState() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptContext, NS_ISCRIPTCONTEXT_IID)

#endif // nsIScriptContext_h__

