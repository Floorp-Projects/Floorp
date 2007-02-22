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
 * Portions created by the Initial Developer are Copyright (C) 1998
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
#ifndef nsJSEnvironment_h___
#define nsJSEnvironment_h___

#include "nsIScriptContext.h"
#include "nsIScriptRuntime.h"
#include "nsCOMPtr.h"
#include "jsapi.h"
#include "nsIObserver.h"
#include "nsIXPCScriptNotify.h"
#include "nsITimer.h"
#include "prtime.h"

class nsIXPConnectJSObjectHolder;

class nsJSContext : public nsIScriptContext,
                    public nsIXPCScriptNotify,
                    public nsITimerCallback
{
public:
  nsJSContext(JSRuntime *aRuntime);
  virtual ~nsJSContext();

  NS_DECL_ISUPPORTS

  virtual PRUint32 GetScriptTypeID()
    { return nsIProgrammingLanguage::JAVASCRIPT; }

  virtual nsresult EvaluateString(const nsAString& aScript,
                                  void *aScopeObject,
                                  nsIPrincipal *principal,
                                  const char *aURL,
                                  PRUint32 aLineNo,
                                  PRUint32 aVersion,
                                  nsAString *aRetValue,
                                  PRBool* aIsUndefined);
  virtual nsresult EvaluateStringWithValue(const nsAString& aScript,
                                     void *aScopeObject,
                                     nsIPrincipal *aPrincipal,
                                     const char *aURL,
                                     PRUint32 aLineNo,
                                     PRUint32 aVersion,
                                     void* aRetValue,
                                     PRBool* aIsUndefined);

  virtual nsresult CompileScript(const PRUnichar* aText,
                                 PRInt32 aTextLength,
                                 void *aScopeObject,
                                 nsIPrincipal *principal,
                                 const char *aURL,
                                 PRUint32 aLineNo,
                                 PRUint32 aVersion,
                                 nsScriptObjectHolder &aScriptObject);
  virtual nsresult ExecuteScript(void* aScriptObject,
                                 void *aScopeObject,
                                 nsAString* aRetValue,
                                 PRBool* aIsUndefined);

  virtual nsresult CompileEventHandler(nsIAtom *aName,
                                       PRUint32 aArgCount,
                                       const char** aArgNames,
                                       const nsAString& aBody,
                                       const char *aURL, PRUint32 aLineNo,
                                       nsScriptObjectHolder &aHandler);
  virtual nsresult CallEventHandler(nsISupports* aTarget, void *aScope,
                                    void* aHandler,
                                    nsIArray *argv, nsIVariant **rv);
  virtual nsresult BindCompiledEventHandler(nsISupports *aTarget,
                                            void *aScope,
                                            nsIAtom *aName,
                                            void *aHandler);
  virtual nsresult GetBoundEventHandler(nsISupports* aTarget, void *aScope,
                                        nsIAtom* aName,
                                        nsScriptObjectHolder &aHandler);
  virtual nsresult CompileFunction(void* aTarget,
                                   const nsACString& aName,
                                   PRUint32 aArgCount,
                                   const char** aArgArray,
                                   const nsAString& aBody,
                                   const char* aURL,
                                   PRUint32 aLineNo,
                                   PRBool aShared,
                                   void** aFunctionObject);

  virtual void SetDefaultLanguageVersion(PRUint32 aVersion);
  virtual nsIScriptGlobalObject *GetGlobalObject();
  virtual void *GetNativeContext();
  virtual void *GetNativeGlobal();
  virtual nsresult CreateNativeGlobalForInner(
                                      nsIScriptGlobalObject *aGlobal,
                                      PRBool aIsChrome,
                                      void **aNativeGlobal,
                                      nsISupports **aHolder);
  virtual nsresult ConnectToInner(nsIScriptGlobalObject *aNewInner,
                                  void *aOuterGlobal);
  virtual nsresult InitContext(nsIScriptGlobalObject *aGlobalObject);
  virtual PRBool IsContextInitialized();
  virtual void FinalizeContext();

  virtual void GC();

  virtual void ScriptEvaluated(PRBool aTerminated);
  virtual nsresult SetTerminationFunction(nsScriptTerminationFunc aFunc,
                                          nsISupports* aRef);
  virtual PRBool GetScriptsEnabled();
  virtual void SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts);

  virtual nsresult SetProperty(void *aTarget, const char *aPropName, nsISupports *aVal);

  virtual PRBool GetProcessingScriptTag();
  virtual void SetProcessingScriptTag(PRBool aResult);

  virtual void SetGCOnDestruction(PRBool aGCOnDestruction);

  virtual nsresult InitClasses(void *aGlobalObj);
  virtual void ClearScope(void* aGlobalObj, PRBool bClearPolluters);

  virtual void WillInitializeContext();
  virtual void DidInitializeContext();
  virtual void DidSetDocument(nsISupports *aDocdoc, void *aGlobal) {;}

  virtual nsresult Serialize(nsIObjectOutputStream* aStream, void *aScriptObject);
  virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                               nsScriptObjectHolder &aResult);

  virtual nsresult DropScriptObject(void *object);
  virtual nsresult HoldScriptObject(void *object);

  NS_DECL_NSIXPCSCRIPTNOTIFY

  NS_DECL_NSITIMERCALLBACK

  static void LoadStart();
  static void LoadEnd();

protected:
  nsresult InitializeExternalClasses();
  nsresult InitializeLiveConnectClasses(JSObject *aGlobalObj);
  // aHolder should be holding our global object
  nsresult FindXPCNativeWrapperClass(nsIXPConnectJSObjectHolder *aHolder);

  // Helper to convert xpcom datatypes to jsvals.
  nsresult ConvertSupportsTojsvals(nsISupports *aArgs,
                                   void *aScope,
                                   PRUint32 *aArgc, void **aArgv,
                                   void **aMarkp);

  nsresult AddSupportsPrimitiveTojsvals(nsISupports *aArg, jsval *aArgv);

  void FireGCTimer(PRBool aLoadInProgress);

  // given an nsISupports object (presumably an event target or some other
  // DOM object), get (or create) the JSObject wrapping it.
  nsresult JSObjectFromInterface(nsISupports *aSup, void *aScript, 
                                 JSObject **aRet);

private:
  JSContext *mContext;
  PRUint32 mNumEvaluations;

protected:
  struct TerminationFuncHolder;
  friend struct TerminationFuncHolder;
  
  struct TerminationFuncClosure
  {
    TerminationFuncClosure(nsScriptTerminationFunc aFunc,
                           nsISupports* aArg,
                           TerminationFuncClosure* aNext) :
      mTerminationFunc(aFunc),
      mTerminationFuncArg(aArg),
      mNext(aNext)
    {
    }
    ~TerminationFuncClosure()
    {
      delete mNext;
    }
    
    nsScriptTerminationFunc mTerminationFunc;
    nsCOMPtr<nsISupports> mTerminationFuncArg;
    TerminationFuncClosure* mNext;
  };

  struct TerminationFuncHolder
  {
    TerminationFuncHolder(nsJSContext* aContext)
      : mContext(aContext),
        mTerminations(aContext->mTerminations)
    {
      aContext->mTerminations = nsnull;
    }
    ~TerminationFuncHolder()
    {
      // Have to be careful here.  mContext might have picked up new
      // termination funcs while the script was evaluating.  Prepend whatever
      // we have to the current termination funcs on the context (since our
      // termination funcs were posted first).
      if (mTerminations) {
        TerminationFuncClosure* cur = mTerminations;
        while (cur->mNext) {
          cur = cur->mNext;
        }
        cur->mNext = mContext->mTerminations;
        mContext->mTerminations = mTerminations;
      }
    }

    nsJSContext* mContext;
    TerminationFuncClosure* mTerminations;
  };
  
  TerminationFuncClosure* mTerminations;

private:
  PRPackedBool mIsInitialized;
  PRPackedBool mScriptsEnabled;
  PRPackedBool mGCOnDestruction;
  PRPackedBool mProcessingScriptTag;
  PRPackedBool mIsTrackingChromeCodeTime;

  PRUint32 mBranchCallbackCount;
  PRTime mBranchCallbackTime;
  PRUint32 mDefaultJSOptions;

  // mGlobalWrapperRef is used only to hold a strong reference to the
  // global object wrapper while the nsJSContext is alive. This cuts
  // down on the number of rooting and unrooting calls XPConnect has
  // to make when the global object is touched in JS.

  nsCOMPtr<nsISupports> mGlobalWrapperRef;

  static int PR_CALLBACK JSOptionChangedCallback(const char *pref, void *data);

  static JSBool JS_DLL_CALLBACK DOMBranchCallback(JSContext *cx,
                                                  JSScript *script);
};

class nsIJSRuntimeService;

class nsJSRuntime : public nsIScriptRuntime
{
public:
  // let people who can see us use our runtime for convenience.
  static JSRuntime *sRuntime;

public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIScriptRuntime
  virtual void ShutDown();

  virtual PRUint32 GetScriptTypeID() {
            return nsIProgrammingLanguage::JAVASCRIPT;
  }

  virtual nsresult CreateContext(nsIScriptContext **ret);

  virtual nsresult ParseVersion(const nsString &aVersionStr, PRUint32 *flags);

  virtual nsresult DropScriptObject(void *object);
  virtual nsresult HoldScriptObject(void *object);
  
  // Private stuff.
  // called by the nsDOMScriptObjectFactory to initialize statics
  static void Startup();
  // Setup all the statics etc - safe to call multiple times after Startup()
  static nsresult Init();
};

// An interface for fast and native conversion to/from nsIArray. If an object
// supports this interface, JS can reach directly in for the argv, and avoid
// nsISupports conversion. If this interface is not supported, the object will
// be queried for nsIArray, and everything converted via xpcom objects.
#define NS_IJSARGARRAY_IID \
 { /*{E96FB2AE-CB4F-44a0-81F8-D91C80AFE9A3} */ \
 0xe96fb2ae, 0xcb4f, 0x44a0, \
 { 0x81, 0xf8, 0xd9, 0x1c, 0x80, 0xaf, 0xe9, 0xa3 } }

class nsIJSArgArray: public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IJSARGARRAY_IID)
  // Bug 312003 describes why this must be "void **", but after calling argv
  // may be cast to jsval* and the args found at:
  //    ((jsval*)argv)[0], ..., ((jsval*)argv)[argc - 1]
  virtual nsresult GetArgs(PRUint32 *argc, void **argv) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIJSArgArray, NS_IJSARGARRAY_IID)

/* factory functions */
nsresult NS_CreateJSRuntime(nsIScriptRuntime **aRuntime);

/* prototypes */
void JS_DLL_CALLBACK NS_ScriptErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);

#endif /* nsJSEnvironment_h___ */
