/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScriptContext_h__
#define nsIScriptContext_h__

#include "nscore.h"
#include "nsStringGlue.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIProgrammingLanguage.h"
#include "jsfriendapi.h"
#include "jspubtd.h"
#include "js/GCAPI.h"

class nsIScriptGlobalObject;
class nsIScriptSecurityManager;
class nsIPrincipal;
class nsIAtom;
class nsIArray;
class nsIVariant;
class nsIObjectInputStream;
class nsIObjectOutputStream;
class nsIScriptObjectPrincipal;
class nsIDOMWindow;
class nsIURI;

typedef void (*nsScriptTerminationFunc)(nsISupports* aRef);

#define NS_ISCRIPTCONTEXT_IID \
{ 0x821c5be9, 0xbf9e, 0x4041, \
  { 0x9f, 0xf2, 0x1f, 0xca, 0x39, 0xf7, 0x89, 0xf3 } }

/* This MUST match JSVERSION_DEFAULT.  This version stuff if we don't
   know what language we have is a little silly... */
#define SCRIPTVERSION_DEFAULT JSVERSION_DEFAULT

/**
 * It is used by the application to initialize a runtime and run scripts.
 * A script runtime would implement this interface.
 */
class nsIScriptContext : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTCONTEXT_IID)

  /**
   * Compile and execute a script.
   *
   * @param aScript a string representing the script to be executed
   * @param aScopeObject a script object for the scope to execute in.
   * @param aOptions an options object. You probably want to at least set
   *                 filename and line number. The principal is computed
   *                 internally, though 'originPrincipals' may be passed.
   * @param aCoerceToString if the return value is not JSVAL_VOID, convert it
   *                        to a string before returning.
   * @param aRetValue the result of executing the script.  Pass null if you
   *                  don't care about the result.  Note that asking for a
   *                  result will deoptimize your script somewhat in many cases.
   */
  virtual nsresult EvaluateString(const nsAString& aScript,
                                  JSObject& aScopeObject,
                                  JS::CompileOptions& aOptions,
                                  bool aCoerceToString,
                                  JS::Value* aRetValue) = 0;

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
   * @param aSaveSource force the source code to be saved by the JS engine in memory
   *
   * @return NS_OK if the script source was valid and got compiled.
   *
   **/
  virtual nsresult CompileScript(const PRUnichar* aText,
                                 int32_t aTextLength,
                                 nsIPrincipal* aPrincipal,
                                 const char* aURL,
                                 uint32_t aLineNo,
                                 uint32_t aVersion,
                                 JS::MutableHandle<JSScript*> aScriptObject,
                                 bool aSaveSource = false) = 0;

  /**
   * Execute a precompiled script object.
   *
   * @param aScriptObject an object representing the script to be executed
   * @param aScopeObject an object telling the scope in which to execute,
   *                     or nullptr to use a default scope
   * @param aRetValue the result of executing the script, may be null in
   *                  which case no result string is computed
   * @param aIsUndefined true if the result of executing the script is the
   *                     undefined value, may be null for "don't care"
   *
   * @return NS_OK if the script was valid and got executed
   *
   */
  virtual nsresult ExecuteScript(JSScript* aScriptObject,
                                 JSObject* aScopeObject) = 0;

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
                                            JSObject* aHandler,
                                            JS::MutableHandle<JSObject*> aBoundHandler) = 0;

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
   * Initialize the context generally. Does not create a global object.
   **/
  virtual nsresult InitContext() = 0;

  /**
   * Check to see if context is as yet intialized. Used to prevent
   * reentrancy issues during the initialization process.
   *
   * @return true if initialized, false if not
   *
   */
  virtual bool IsContextInitialized() = 0;

  /**
   * For garbage collected systems, do a synchronous collection pass.
   * May be a no-op on other systems
   *
   * @return NS_OK if the method is successful
   */
  virtual void GC(JS::gcreason::Reason aReason) = 0;

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
                               JS::MutableHandle<JSScript*> aResult) = 0;

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
  virtual void SetTerminationFunction(nsScriptTerminationFunc aFunc,
                                      nsIDOMWindow* aRef) = 0;

  /**
   * Called to disable/enable script execution in this context.
   */
  virtual bool GetScriptsEnabled() = 0;
  virtual void SetScriptsEnabled(bool aEnabled, bool aFireTimeouts) = 0;

  // SetProperty is suspect and jst believes should not be needed.  Currenly
  // used only for "arguments".
  virtual nsresult SetProperty(JSObject* aTarget, const char* aPropName, nsISupports* aVal) = 0;
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
   * Initialize DOM classes on aGlobalObj, always call
   * WillInitializeContext() before calling InitContext(), and always
   * call DidInitializeContext() when a context is fully
   * (successfully) initialized.
   */
  virtual nsresult InitClasses(JSObject* aGlobalObj) = 0;

  /**
   * Tell the context we're about to be reinitialize it.
   */
  virtual void WillInitializeContext() = 0;

  /**
   * Tell the context we're done reinitializing it.
   */
  virtual void DidInitializeContext() = 0;

  virtual void EnterModalState() = 0;
  virtual void LeaveModalState() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptContext, NS_ISCRIPTCONTEXT_IID)

#endif // nsIScriptContext_h__

