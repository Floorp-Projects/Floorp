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
#ifndef nsJSEnvironment_h
#define nsJSEnvironment_h

#include "nsIScriptContext.h"
#include "nsIScriptRuntime.h"
#include "nsCOMPtr.h"
#include "jsapi.h"
#include "nsIObserver.h"
#include "nsIXPCScriptNotify.h"
#include "prtime.h"
#include "nsCycleCollectionParticipant.h"

class nsIXPConnectJSObjectHolder;
class nsRootedJSValueArray;
class nsScriptNameSpaceManager;
namespace js {
class AutoArrayRooter;
}
namespace mozilla {
template <class> class Maybe;
}

class nsJSContext : public nsIScriptContext,
                    public nsIXPCScriptNotify
{
public:
  nsJSContext(JSRuntime *aRuntime);
  virtual ~nsJSContext();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsJSContext,
                                                         nsIScriptContext)

  virtual nsIScriptObjectPrincipal* GetObjectPrincipal();

  virtual PRUint32 GetScriptTypeID()
    { return nsIProgrammingLanguage::JAVASCRIPT; }

  virtual nsresult EvaluateString(const nsAString& aScript,
                                  JSObject* aScopeObject,
                                  nsIPrincipal *principal,
                                  const char *aURL,
                                  PRUint32 aLineNo,
                                  PRUint32 aVersion,
                                  nsAString *aRetValue,
                                  bool* aIsUndefined);
  virtual nsresult EvaluateStringWithValue(const nsAString& aScript,
                                           JSObject* aScopeObject,
                                           nsIPrincipal* aPrincipal,
                                           const char* aURL,
                                           PRUint32 aLineNo,
                                           PRUint32 aVersion,
                                           JS::Value* aRetValue,
                                           bool* aIsUndefined);

  virtual nsresult CompileScript(const PRUnichar* aText,
                                 PRInt32 aTextLength,
                                 nsIPrincipal *principal,
                                 const char *aURL,
                                 PRUint32 aLineNo,
                                 PRUint32 aVersion,
                                 nsScriptObjectHolder &aScriptObject);
  virtual nsresult ExecuteScript(JSScript* aScriptObject,
                                 JSObject* aScopeObject,
                                 nsAString* aRetValue,
                                 bool* aIsUndefined);

  virtual nsresult CompileEventHandler(nsIAtom *aName,
                                       PRUint32 aArgCount,
                                       const char** aArgNames,
                                       const nsAString& aBody,
                                       const char *aURL, PRUint32 aLineNo,
                                       PRUint32 aVersion,
                                       nsScriptObjectHolder &aHandler);
  virtual nsresult CallEventHandler(nsISupports* aTarget, JSObject* aScope,
                                    JSObject* aHandler,
                                    nsIArray *argv, nsIVariant **rv);
  virtual nsresult BindCompiledEventHandler(nsISupports *aTarget,
                                            JSObject *aScope,
                                            JSObject* aHandler,
                                            nsScriptObjectHolder& aBoundHandler);
  virtual nsresult CompileFunction(JSObject* aTarget,
                                   const nsACString& aName,
                                   PRUint32 aArgCount,
                                   const char** aArgArray,
                                   const nsAString& aBody,
                                   const char* aURL,
                                   PRUint32 aLineNo,
                                   PRUint32 aVersion,
                                   bool aShared,
                                   JSObject** aFunctionObject);

  virtual void SetDefaultLanguageVersion(PRUint32 aVersion);
  virtual nsIScriptGlobalObject *GetGlobalObject();
  virtual JSContext* GetNativeContext();
  virtual JSObject* GetNativeGlobal();
  virtual nsresult CreateNativeGlobalForInner(
                                      nsIScriptGlobalObject *aGlobal,
                                      bool aIsChrome,
                                      nsIPrincipal *aPrincipal,
                                      JSObject** aNativeGlobal,
                                      nsISupports **aHolder);
  virtual nsresult ConnectToInner(nsIScriptGlobalObject *aNewInner,
                                  JSObject *aOuterGlobal);
  virtual nsresult InitContext();
  virtual nsresult CreateOuterObject(nsIScriptGlobalObject *aGlobalObject,
                                     nsIScriptGlobalObject *aCurrentInner);
  virtual nsresult SetOuterObject(JSObject* aOuterObject);
  virtual nsresult InitOuterWindow();
  virtual bool IsContextInitialized();
  virtual void FinalizeContext();

  virtual void ScriptEvaluated(bool aTerminated);
  virtual nsresult SetTerminationFunction(nsScriptTerminationFunc aFunc,
                                          nsISupports* aRef);
  virtual bool GetScriptsEnabled();
  virtual void SetScriptsEnabled(bool aEnabled, bool aFireTimeouts);

  virtual nsresult SetProperty(void *aTarget, const char *aPropName, nsISupports *aVal);

  virtual bool GetProcessingScriptTag();
  virtual void SetProcessingScriptTag(bool aResult);

  virtual bool GetExecutingScript();

  virtual void SetGCOnDestruction(bool aGCOnDestruction);

  virtual nsresult InitClasses(JSObject* aGlobalObj);
  virtual void ClearScope(void* aGlobalObj, bool bClearPolluters);

  virtual void WillInitializeContext();
  virtual void DidInitializeContext();

  virtual nsresult Serialize(nsIObjectOutputStream* aStream, JSScript* aScriptObject);
  virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                               nsScriptObjectHolder &aResult);

  virtual nsresult DropScriptObject(void *object);
  virtual nsresult HoldScriptObject(void *object);

  virtual void EnterModalState();
  virtual void LeaveModalState();

  NS_DECL_NSIXPCSCRIPTNOTIFY

  static void LoadStart();
  static void LoadEnd();

  static void GarbageCollectNow(bool shrinkingGC = false);
  static void CycleCollectNow(nsICycleCollectorListener *aListener = nsnull);

  static void PokeGC();
  static void KillGCTimer();

  static void PokeCC();
  static void MaybePokeCC();
  static void KillCCTimer();

  virtual void GC();

protected:
  nsresult InitializeExternalClasses();

  // Helper to convert xpcom datatypes to jsvals.
  nsresult ConvertSupportsTojsvals(nsISupports *aArgs,
                                   JSObject *aScope,
                                   PRUint32 *aArgc,
                                   jsval **aArgv,
                                   mozilla::Maybe<nsRootedJSValueArray> &aPoolRelease);

  nsresult AddSupportsPrimitiveTojsvals(nsISupports *aArg, jsval *aArgv);

  // given an nsISupports object (presumably an event target or some other
  // DOM object), get (or create) the JSObject wrapping it.
  nsresult JSObjectFromInterface(nsISupports *aSup, JSObject *aScript,
                                 JSObject **aRet);

  // Report the pending exception on our mContext, if any.  This
  // function will set aside the frame chain on mContext before
  // reporting.
  void ReportPendingException();
private:
  void DestroyJSContext();

  nsrefcnt GetCCRefcnt();

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
  bool mIsInitialized;
  bool mScriptsEnabled;
  bool mGCOnDestruction;
  bool mProcessingScriptTag;

  PRUint32 mExecuteDepth;
  PRUint32 mDefaultJSOptions;
  PRTime mOperationCallbackTime;

  PRTime mModalStateTime;
  PRUint32 mModalStateDepth;

  // mGlobalObjectRef ensures that the outer window stays alive as long as the
  // context does. It is eventually collected by the cycle collector.
  nsCOMPtr<nsIScriptGlobalObject> mGlobalObjectRef;

  static int JSOptionChangedCallback(const char *pref, void *data);

  static JSBool DOMOperationCallback(JSContext *cx);
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

  virtual PRUint32 GetScriptTypeID() {
    return nsIProgrammingLanguage::JAVASCRIPT;
  }

  virtual already_AddRefed<nsIScriptContext> CreateContext();

  virtual nsresult ParseVersion(const nsString &aVersionStr, PRUint32 *flags);

  virtual nsresult DropScriptObject(void *object);
  virtual nsresult HoldScriptObject(void *object);
  
  static void Startup();
  static void Shutdown();
  // Setup all the statics etc - safe to call multiple times after Startup()
  static nsresult Init();
  // Get the NameSpaceManager, creating if necessary
  static nsScriptNameSpaceManager* GetNameSpaceManager();
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
void NS_ScriptErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);

JSObject* NS_DOMReadStructuredClone(JSContext* cx,
                                    JSStructuredCloneReader* reader, uint32 tag,
                                    uint32 data, void* closure);

JSBool NS_DOMWriteStructuredClone(JSContext* cx,
                                  JSStructuredCloneWriter* writer,
                                  JSObject* obj, void *closure);

void NS_DOMStructuredCloneError(JSContext* cx, uint32 errorid);

#endif /* nsJSEnvironment_h */
