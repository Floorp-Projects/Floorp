/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsJSEnvironment_h
#define nsJSEnvironment_h

#include "nsIScriptContext.h"
#include "nsIScriptRuntime.h"
#include "nsIScriptGlobalObject.h"
#include "nsCOMPtr.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsIObserver.h"
#include "nsIXPCScriptNotify.h"
#include "prtime.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIXPConnect.h"
#include "nsIArray.h"

class nsIXPConnectJSObjectHolder;
class nsRootedJSValueArray;
class nsScriptNameSpaceManager;
namespace mozilla {
template <class> class Maybe;
}

// The amount of time we wait between a request to GC (due to leaving
// a page) and doing the actual GC.
#define NS_GC_DELAY                 4000 // ms

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

  virtual void SetGlobalObject(nsIScriptGlobalObject* aGlobalObject)
  {
    mGlobalObjectRef = aGlobalObject;
  }

  virtual nsresult EvaluateString(const nsAString& aScript,
                                  JSObject* aScopeObject,
                                  nsIPrincipal *principal,
                                  nsIPrincipal *originPrincipal,
                                  const char *aURL,
                                  PRUint32 aLineNo,
                                  JSVersion aVersion,
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
                                 nsScriptObjectHolder<JSScript>& aScriptObject);
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
                                       nsScriptObjectHolder<JSObject>& aHandler);
  virtual nsresult CallEventHandler(nsISupports* aTarget, JSObject* aScope,
                                    JSObject* aHandler,
                                    nsIArray *argv, nsIVariant **rv);
  virtual nsresult BindCompiledEventHandler(nsISupports *aTarget,
                                            JSObject *aScope,
                                            JSObject* aHandler,
                                            nsScriptObjectHolder<JSObject>& aBoundHandler);
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

  virtual nsIScriptGlobalObject *GetGlobalObject();
  inline nsIScriptGlobalObject *GetGlobalObjectRef() { return mGlobalObjectRef; };

  virtual JSContext* GetNativeContext();
  virtual JSObject* GetNativeGlobal();
  virtual nsresult CreateNativeGlobalForInner(
                                      nsIScriptGlobalObject *aGlobal,
                                      bool aIsChrome,
                                      nsIPrincipal *aPrincipal,
                                      JSObject** aNativeGlobal,
                                      nsISupports **aHolder);
  virtual nsresult InitContext();
  virtual nsresult InitOuterWindow();
  virtual bool IsContextInitialized();

  virtual void ScriptEvaluated(bool aTerminated);
  virtual void SetTerminationFunction(nsScriptTerminationFunc aFunc,
                                      nsIDOMWindow* aRef);
  virtual bool GetScriptsEnabled();
  virtual void SetScriptsEnabled(bool aEnabled, bool aFireTimeouts);

  virtual nsresult SetProperty(JSObject* aTarget, const char* aPropName, nsISupports* aVal);

  virtual bool GetProcessingScriptTag();
  virtual void SetProcessingScriptTag(bool aResult);

  virtual bool GetExecutingScript();

  virtual void SetGCOnDestruction(bool aGCOnDestruction);

  virtual nsresult InitClasses(JSObject* aGlobalObj);

  virtual void WillInitializeContext();
  virtual void DidInitializeContext();

  virtual nsresult Serialize(nsIObjectOutputStream* aStream, JSScript* aScriptObject);
  virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                               nsScriptObjectHolder<JSScript>& aResult);

  virtual nsresult DropScriptObject(void *object);
  virtual nsresult HoldScriptObject(void *object);

  virtual void EnterModalState();
  virtual void LeaveModalState();

  NS_DECL_NSIXPCSCRIPTNOTIFY

  static void LoadStart();
  static void LoadEnd();

  static void GarbageCollectNow(js::gcreason::Reason reason,
                                PRUint32 aGckind,
                                bool aGlobal);
  static void ShrinkGCBuffersNow();
  // If aExtraForgetSkippableCalls is -1, forgetSkippable won't be
  // called even if the previous collection was GC.
  static void CycleCollectNow(nsICycleCollectorListener *aListener = nsnull,
                              PRInt32 aExtraForgetSkippableCalls = 0);

  static void PokeGC(js::gcreason::Reason aReason, int aDelay = 0);
  static void KillGCTimer();

  static void PokeShrinkGCBuffers();
  static void KillShrinkGCBuffersTimer();

  static void MaybePokeCC();
  static void KillCCTimer();
  static void KillFullGCTimer();

  virtual void GC(js::gcreason::Reason aReason);

  static PRUint32 CleanupsSinceLastGC();

  nsIScriptGlobalObject* GetCachedGlobalObject()
  {
    // Verify that we have a global so that this
    // does always return a null when GetGlobalObject() is null.
    JSObject* global = JS_GetGlobalObject(mContext);
    return global ? mGlobalObjectRef.get() : nsnull;
  }
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
  bool mActive;

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

  nsJSContext *mNext;
  nsJSContext **mPrev;

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
{ 0xb6acdac8, 0xf5c6, 0x432c, \
  { 0xa8, 0x6e, 0x33, 0xee, 0xb1, 0xb0, 0xcd, 0xdc } }

class nsIJSArgArray : public nsIArray
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
                                    JSStructuredCloneReader* reader, uint32_t tag,
                                    uint32_t data, void* closure);

JSBool NS_DOMWriteStructuredClone(JSContext* cx,
                                  JSStructuredCloneWriter* writer,
                                  JSObject* obj, void *closure);

void NS_DOMStructuredCloneError(JSContext* cx, uint32_t errorid);

#endif /* nsJSEnvironment_h */
