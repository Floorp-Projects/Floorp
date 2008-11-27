/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 * The Original Code is Web Workers.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsDOMWorker.h"

#include "nsIDOMEvent.h"
#include "nsIEventTarget.h"
#include "nsIJSRuntimeService.h"
#include "nsIXPConnect.h"

#ifdef MOZ_SHARK
#include "jsdbgapi.h"
#endif
#include "nsAutoLock.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsGlobalWindow.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"

#include "nsDOMThreadService.h"
#include "nsDOMWorkerEvents.h"
#include "nsDOMWorkerPool.h"
#include "nsDOMWorkerScriptLoader.h"
#include "nsDOMWorkerTimeout.h"
#include "nsDOMWorkerXHR.h"

class nsDOMWorkerFunctions
{
public:
  // Same as window.dump().
  static JSBool Dump(JSContext* aCx, JSObject* aObj, uintN aArgc,
                     jsval* aArgv, jsval* aRval);

  // Same as window.setTimeout().
  static JSBool SetTimeout(JSContext* aCx, JSObject* aObj, uintN aArgc,
                           jsval* aArgv, jsval* aRval) {
    return MakeTimeout(aCx, aObj, aArgc, aArgv, aRval, PR_FALSE);
  }

  // Same as window.setInterval().
  static JSBool SetInterval(JSContext* aCx, JSObject* aObj, uintN aArgc,
                            jsval* aArgv, jsval* aRval) {
    return MakeTimeout(aCx, aObj, aArgc, aArgv, aRval, PR_TRUE);
  }

  // Used for both clearTimeout() and clearInterval().
  static JSBool KillTimeout(JSContext* aCx, JSObject* aObj, uintN aArgc,
                            jsval* aArgv, jsval* aRval);

  static JSBool LoadScripts(JSContext* aCx, JSObject* aObj, uintN aArgc,
                            jsval* aArgv, jsval* aRval);

  static JSBool NewXMLHttpRequest(JSContext* aCx, JSObject* aObj, uintN aArgc,
                                  jsval* aArgv, jsval* aRval);

  static JSBool NewWorker(JSContext* aCx, JSObject* aObj, uintN aArgc,
                          jsval* aArgv, jsval* aRval);

private:
  // Internal helper for SetTimeout and SetInterval.
  static JSBool MakeTimeout(JSContext* aCx, JSObject* aObj, uintN aArgc,
                            jsval* aArgv, jsval* aRval, PRBool aIsInterval);
};

JSBool
nsDOMWorkerFunctions::Dump(JSContext* aCx,
                           JSObject* /* aObj */,
                           uintN aArgc,
                           jsval* aArgv,
                           jsval* /* aRval */)
{
  if (!nsGlobalWindow::DOMWindowDumpEnabled()) {
    return JS_TRUE;
  }

  JSString* str;
  if (aArgc && (str = JS_ValueToString(aCx, aArgv[0])) && str) {
    nsDependentJSString string(str);
    fputs(NS_ConvertUTF16toUTF8(nsDependentJSString(str)).get(), stderr);
    fflush(stderr);
  }
  return JS_TRUE;
}

JSBool
nsDOMWorkerFunctions::MakeTimeout(JSContext* aCx,
                                  JSObject* /* aObj */,
                                  uintN aArgc,
                                  jsval* aArgv,
                                  jsval* aRval,
                                  PRBool aIsInterval)
{
  nsDOMWorker* worker = static_cast<nsDOMWorker*>(JS_GetContextPrivate(aCx));
  NS_ASSERTION(worker, "This should be set by the DOM thread service!");

  if (worker->IsCanceled()) {
    return JS_FALSE;
  }

  PRUint32 id = worker->NextTimeoutId();

  nsRefPtr<nsDOMWorkerTimeout> timeout = new nsDOMWorkerTimeout(worker, id);
  if (!timeout) {
    JS_ReportOutOfMemory(aCx);
    return JS_FALSE;
  }

  nsresult rv = timeout->Init(aCx, aArgc, aArgv, aIsInterval);
  if (NS_FAILED(rv)) {
    JS_ReportError(aCx, "Failed to initialize timeout!");
    return JS_FALSE;
  }

  rv = worker->AddFeature(timeout, aCx);
  if (NS_FAILED(rv)) {
    JS_ReportOutOfMemory(aCx);
    return JS_FALSE;
  }

  rv = timeout->Start();
  if (NS_FAILED(rv)) {
    JS_ReportError(aCx, "Failed to start timeout!");
    return JS_FALSE;
  }

  *aRval = INT_TO_JSVAL(id);
  return JS_TRUE;
}

JSBool
nsDOMWorkerFunctions::KillTimeout(JSContext* aCx,
                                  JSObject* /* aObj */,
                                  uintN aArgc,
                                  jsval* aArgv,
                                  jsval* /* aRval */)
{
  nsDOMWorker* worker = static_cast<nsDOMWorker*>(JS_GetContextPrivate(aCx));
  NS_ASSERTION(worker, "This should be set by the DOM thread service!");

  if (worker->IsCanceled()) {
    return JS_FALSE;
  }

  if (!aArgc) {
    JS_ReportError(aCx, "Function requires at least 1 parameter");
    return JS_FALSE;
  }

  uint32 id;
  if (!JS_ValueToECMAUint32(aCx, aArgv[0], &id)) {
    JS_ReportError(aCx, "First argument must be a timeout id");
    return JS_FALSE;
  }

  worker->CancelTimeoutWithId(PRUint32(id));
  return JS_TRUE;
}

JSBool
nsDOMWorkerFunctions::LoadScripts(JSContext* aCx,
                                  JSObject* /* aObj */,
                                  uintN aArgc,
                                  jsval* aArgv,
                                  jsval* /* aRval */)
{
  nsDOMWorker* worker = static_cast<nsDOMWorker*>(JS_GetContextPrivate(aCx));
  NS_ASSERTION(worker, "This should be set by the DOM thread service!");

  if (worker->IsCanceled()) {
    return JS_FALSE;
  }

  if (!aArgc) {
    JS_ReportError(aCx, "Function must have at least one argument!");
    return JS_FALSE;
  }

  nsAutoTArray<nsString, 10> urls;

  if (!urls.SetCapacity((PRUint32)aArgc)) {
    JS_ReportOutOfMemory(aCx);
    return JS_FALSE;
  }

  for (uintN index = 0; index < aArgc; index++) {
    jsval val = aArgv[index];

    if (!JSVAL_IS_STRING(val)) {
      JS_ReportError(aCx, "Argument %d must be a string", index);
      return JS_FALSE;
    }

    JSString* str = JS_ValueToString(aCx, val);
    if (!str) {
      JS_ReportError(aCx, "Couldn't convert argument %d to a string", index);
      return JS_FALSE;
    }

    nsString* newURL = urls.AppendElement();
    NS_ASSERTION(newURL, "Shouldn't fail if SetCapacity succeeded above!");

    newURL->Assign(nsDependentJSString(str));
  }

  nsRefPtr<nsDOMWorkerScriptLoader> loader =
    new nsDOMWorkerScriptLoader(worker);
  if (!loader) {
    JS_ReportOutOfMemory(aCx);
    return JS_FALSE;
  }

  nsresult rv = worker->AddFeature(loader, aCx);
  if (NS_FAILED(rv)) {
    JS_ReportOutOfMemory(aCx);
    return JS_FALSE;
  }

  rv = loader->LoadScripts(aCx, urls);
  if (NS_FAILED(rv)) {
    if (!JS_IsExceptionPending(aCx)) {
      JS_ReportError(aCx, "Failed to load scripts");
    }
    return JS_FALSE;
  }

  return JS_TRUE;
}

JSBool
nsDOMWorkerFunctions::NewXMLHttpRequest(JSContext* aCx,
                                        JSObject* aObj,
                                        uintN aArgc,
                                        jsval* /* aArgv */,
                                        jsval* aRval)
{
  nsDOMWorker* worker = static_cast<nsDOMWorker*>(JS_GetContextPrivate(aCx));
  NS_ASSERTION(worker, "This should be set by the DOM thread service!");

  if (worker->IsCanceled()) {
    return JS_FALSE;
  }

  if (aArgc) {
    JS_ReportError(aCx, "XMLHttpRequest constructor takes no arguments!");
    return JS_FALSE;
  }

  nsRefPtr<nsDOMWorkerXHR> xhr = new nsDOMWorkerXHR(worker);
  if (!xhr) {
    JS_ReportOutOfMemory(aCx);
    return JS_FALSE;
  }

  nsresult rv = xhr->Init();
  if (NS_FAILED(rv)) {
    JS_ReportError(aCx, "Failed to construct XMLHttpRequest!");
    return JS_FALSE;
  }

  rv = worker->AddFeature(xhr, aCx);
  if (NS_FAILED(rv)) {
    JS_ReportOutOfMemory(aCx);
    return JS_FALSE;
  }

  nsIXPConnect* xpc = nsContentUtils::XPConnect();

  nsCOMPtr<nsIXPConnectJSObjectHolder> xhrWrapped;
  rv = xpc->WrapNative(aCx, aObj, static_cast<nsIXMLHttpRequest*>(xhr),
                       NS_GET_IID(nsISupports), getter_AddRefs(xhrWrapped));
  if (NS_FAILED(rv)) {
    JS_ReportError(aCx, "Failed to wrap XMLHttpRequest!");
    return JS_FALSE;
  }

  JSObject* xhrJSObj;
  rv = xhrWrapped->GetJSObject(&xhrJSObj);
  if (NS_FAILED(rv)) {
    JS_ReportError(aCx, "Failed to get JSObject from wrapper!");
    return JS_FALSE;
  }

  *aRval = OBJECT_TO_JSVAL(xhrJSObj);
  return JS_TRUE;
}

JSBool
nsDOMWorkerFunctions::NewWorker(JSContext* aCx,
                                JSObject* aObj,
                                uintN aArgc,
                                jsval* aArgv,
                                jsval* aRval)
{
  nsDOMWorker* worker = static_cast<nsDOMWorker*>(JS_GetContextPrivate(aCx));
  NS_ASSERTION(worker, "This should be set by the DOM thread service!");

  if (worker->IsCanceled()) {
    return JS_FALSE;
  }

  if (!aArgc) {
    JS_ReportError(aCx, "Worker constructor must have an argument!");
    return JS_FALSE;
  }

  nsRefPtr<nsDOMWorkerPool> pool = worker->Pool();
  if (!pool) {
    JS_ReportError(aCx, "Couldn't get pool from worker!");
    return JS_FALSE;
  }

  // This pointer is protected by our pool, but it is *not* threadsafe and must
  // not be used in any way other than to pass it along to the Initialize call.
  nsIScriptGlobalObject* owner = pool->ScriptGlobalObject();
  if (!owner) {
    JS_ReportError(aCx, "Couldn't get owner from pool!");
    return JS_FALSE;
  }

  nsCOMPtr<nsIXPConnectWrappedNative> wrappedWorker =
    worker->GetWrappedNative();
  if (!wrappedWorker) {
    JS_ReportError(aCx, "Couldn't get wrapped native of worker!");
    return JS_FALSE;
  }

  nsRefPtr<nsDOMWorker> newWorker = new nsDOMWorker(worker, wrappedWorker);
  if (!newWorker) {
    JS_ReportOutOfMemory(aCx);
    return JS_FALSE;
  }

  nsresult rv = newWorker->InitializeInternal(owner, aCx, aObj, aArgc, aArgv);
  if (NS_FAILED(rv)) {
    JS_ReportError(aCx, "Couldn't initialize new worker!");
    return JS_FALSE;
  }

  nsIXPConnect* xpc = nsContentUtils::XPConnect();

  nsCOMPtr<nsIXPConnectJSObjectHolder> workerWrapped;
  rv = xpc->WrapNative(aCx, aObj, static_cast<nsIWorker*>(newWorker),
                       NS_GET_IID(nsISupports), getter_AddRefs(workerWrapped));
  if (NS_FAILED(rv)) {
    JS_ReportError(aCx, "Failed to wrap new worker!");
    return JS_FALSE;
  }

  JSObject* workerJSObj;
  rv = workerWrapped->GetJSObject(&workerJSObj);
  if (NS_FAILED(rv)) {
    JS_ReportError(aCx, "Failed to get JSObject from wrapper!");
    return JS_FALSE;
  }

  *aRval = OBJECT_TO_JSVAL(workerJSObj);
  return JS_TRUE;
}

JSFunctionSpec gDOMWorkerFunctions[] = {
  { "dump",                  nsDOMWorkerFunctions::Dump,              1, 0, 0 },
  { "setTimeout",            nsDOMWorkerFunctions::SetTimeout,        1, 0, 0 },
  { "clearTimeout",          nsDOMWorkerFunctions::KillTimeout,       1, 0, 0 },
  { "setInterval",           nsDOMWorkerFunctions::SetInterval,       1, 0, 0 },
  { "clearInterval",         nsDOMWorkerFunctions::KillTimeout,       1, 0, 0 },
  { "importScripts",         nsDOMWorkerFunctions::LoadScripts,       1, 0, 0 },
  { "XMLHttpRequest",        nsDOMWorkerFunctions::NewXMLHttpRequest, 0, 0, 0 },
  { "Worker",                nsDOMWorkerFunctions::NewWorker,         1, 0, 0 },
#ifdef MOZ_SHARK
  { "startShark",            js_StartShark,                           0, 0, 0 },
  { "stopShark",             js_StopShark,                            0, 0, 0 },
  { "connectShark",          js_ConnectShark,                         0, 0, 0 },
  { "disconnectShark",       js_DisconnectShark,                      0, 0, 0 },
#endif
  { nsnull,                  nsnull,                                  0, 0, 0 }
};

class nsDOMWorkerScope : public nsIWorkerScope,
                         public nsIDOMEventTarget,
                         public nsIXPCScriptable,
                         public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWORKERGLOBALSCOPE
  NS_DECL_NSIWORKERSCOPE
  NS_DECL_NSIDOMEVENTTARGET
  NS_DECL_NSIXPCSCRIPTABLE
  NS_DECL_NSICLASSINFO

  nsDOMWorkerScope(nsDOMWorker* aWorker)
  : mWorker(aWorker) {
    NS_ASSERTION(aWorker, "Null pointer!");
  }

private:
  nsDOMWorker* mWorker;
};

NS_IMPL_THREADSAFE_ISUPPORTS5(nsDOMWorkerScope, nsIWorkerScope,
                                                nsIWorkerGlobalScope,
                                                nsIDOMEventTarget,
                                                nsIXPCScriptable,
                                                nsIClassInfo)

NS_IMPL_CI_INTERFACE_GETTER4(nsDOMWorkerScope, nsIWorkerScope,
                                               nsIWorkerGlobalScope,
                                               nsIDOMEventTarget,
                                               nsIXPCScriptable)

NS_IMPL_THREADSAFE_DOM_CI_GETINTERFACES(nsDOMWorkerScope)
NS_IMPL_THREADSAFE_DOM_CI_ALL_THE_REST(nsDOMWorkerScope)

// Need to return a scriptable helper so that XPConnect can get our
// nsIXPCScriptable flags properly (to not enumerate QI, for instance).
NS_IMETHODIMP
nsDOMWorkerScope::GetHelperForLanguage(PRUint32 aLanguage,
                                       nsISupports** _retval)
{
  if (aLanguage == nsIProgrammingLanguage::JAVASCRIPT) {
    NS_ADDREF(*_retval = NS_ISUPPORTS_CAST(nsIWorkerScope*, this));
  }
  else {
    *_retval = nsnull;
  }
  return NS_OK;
}

// Use the xpc_map_end.h macros to generate the nsIXPCScriptable methods we want
// for the scope.

#define XPC_MAP_CLASSNAME nsDOMWorkerScope
#define XPC_MAP_QUOTED_CLASSNAME "DedicatedWorkerGlobalScope"

#define XPC_MAP_FLAGS                                      \
  nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY           | \
  nsIXPCScriptable::USE_JSSTUB_FOR_DELPROPERTY           | \
  nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY           | \
  nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE            | \
  nsIXPCScriptable::CLASSINFO_INTERFACES_ONLY            | \
  nsIXPCScriptable::DONT_REFLECT_INTERFACE_NAMES

#include "xpc_map_end.h"

NS_IMETHODIMP
nsDOMWorkerScope::GetSelf(nsIWorkerGlobalScope** aSelf)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_ARG_POINTER(aSelf);

  if (mWorker->IsCanceled()) {
    return NS_ERROR_ABORT;
  }

  NS_ADDREF(*aSelf = this);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerScope::PostMessage(const nsAString& aMessage,
                              nsIWorkerMessagePort* aMessagePort)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mWorker->IsCanceled()) {
    return NS_ERROR_ABORT;
  }

  if (aMessagePort) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return mWorker->PostMessageInternal(aMessage, PR_FALSE);
}

NS_IMETHODIMP
nsDOMWorkerScope::GetOnmessage(nsIDOMEventListener** aOnmessage)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_ARG_POINTER(aOnmessage);

  if (mWorker->IsCanceled()) {
    return NS_ERROR_ABORT;
  }

  nsCOMPtr<nsIDOMEventListener> listener =
    mWorker->mInnerHandler->GetOnXListener(NS_LITERAL_STRING("message"));
  listener.forget(aOnmessage);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerScope::SetOnmessage(nsIDOMEventListener* aOnmessage)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mWorker->IsCanceled()) {
    return NS_ERROR_ABORT;
  }

  return mWorker->mInnerHandler->SetOnXListener(NS_LITERAL_STRING("message"),
                                                aOnmessage);
}

NS_IMETHODIMP
nsDOMWorkerScope::AddEventListener(const nsAString& aType,
                                   nsIDOMEventListener* aListener,
                                   PRBool aUseCapture)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mWorker->IsCanceled()) {
    return NS_ERROR_ABORT;
  }

  return mWorker->mInnerHandler->AddEventListener(aType, aListener,
                                                  aUseCapture);
}

NS_IMETHODIMP
nsDOMWorkerScope::RemoveEventListener(const nsAString& aType,
                                      nsIDOMEventListener* aListener,
                                      PRBool aUseCapture)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mWorker->IsCanceled()) {
    return NS_ERROR_ABORT;
  }

  return mWorker->mInnerHandler->RemoveEventListener(aType, aListener,
                                                     aUseCapture);
}

NS_IMETHODIMP
nsDOMWorkerScope::DispatchEvent(nsIDOMEvent* aEvent,
                                PRBool* _retval)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mWorker->IsCanceled()) {
    return NS_ERROR_ABORT;
  }

  return mWorker->mInnerHandler->DispatchEvent(aEvent, _retval);
}

class nsWorkerHoldingRunnable : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  nsWorkerHoldingRunnable(nsDOMWorker* aWorker)
  : mWorker(aWorker), mWorkerWN(aWorker->GetWrappedNative()) { }

  NS_IMETHOD Run() {
    return NS_OK;
  }

protected:
  virtual ~nsWorkerHoldingRunnable() { }

  nsRefPtr<nsDOMWorker> mWorker;

private:
  nsCOMPtr<nsIXPConnectWrappedNative> mWorkerWN;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsWorkerHoldingRunnable, nsIRunnable)

class nsDOMFireEventRunnable : public nsWorkerHoldingRunnable
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsDOMFireEventRunnable(nsDOMWorker* aWorker,
                         nsDOMWorkerEvent* aEvent,
                         PRBool aToInner)
  : nsWorkerHoldingRunnable(aWorker), mEvent(aEvent), mToInner(aToInner)
  {
    NS_ASSERTION(aWorker && aEvent, "Null pointer!");
  }

  NS_IMETHOD Run() {
#ifdef DEBUG
    if (NS_IsMainThread()) {
      NS_ASSERTION(!mToInner, "Should only run outer events on main thread!");
      NS_ASSERTION(!mWorker->mParent, "Worker shouldn't have a parent!");
    }
    else {
      JSContext* cx = nsDOMThreadService::GetCurrentContext();
      nsDOMWorker* currentWorker = (nsDOMWorker*)JS_GetContextPrivate(cx);
      NS_ASSERTION(currentWorker, "Must have a worker here!");

      nsDOMWorker* targetWorker = mToInner ? mWorker.get() : mWorker->mParent;
      NS_ASSERTION(currentWorker == targetWorker, "Wrong worker!");
    }
#endif
    if (mWorker->IsCanceled()) {
      return NS_ERROR_ABORT;
    }

    nsCOMPtr<nsIDOMEventTarget> target = mToInner ?
      static_cast<nsIDOMEventTarget*>(mWorker->GetInnerScope()) :
      static_cast<nsIDOMEventTarget*>(mWorker);

    NS_ASSERTION(target, "Null target!");
    NS_ENSURE_TRUE(target, NS_ERROR_FAILURE);

    mEvent->SetTarget(target);
    return target->DispatchEvent(mEvent, nsnull);
  }

protected:
  nsRefPtr<nsDOMWorkerEvent> mEvent;
  PRBool mToInner;
};

NS_IMPL_ISUPPORTS_INHERITED0(nsDOMFireEventRunnable, nsWorkerHoldingRunnable)

// Standard NS_IMPL_THREADSAFE_ADDREF without the logging stuff (since this
// class is made to be inherited anyway).
NS_IMETHODIMP_(nsrefcnt)
nsDOMWorkerFeature::AddRef()
{
  NS_ASSERTION(mRefCnt >= 0, "Illegal refcnt!");
  return PR_AtomicIncrement((PRInt32*)&mRefCnt);
}

// Custom NS_IMPL_THREADSAFE_RELEASE. Checks the mFreeToDie flag before calling
// delete. If the flag is false then the feature still lives in the worker's
// list and must be removed. We rely on the fact that the RemoveFeature method
// calls AddRef and Release after setting the mFreeToDie flag so we won't leak.
NS_IMETHODIMP_(nsrefcnt)
nsDOMWorkerFeature::Release()
{
  NS_ASSERTION(mRefCnt, "Double release!");
  nsrefcnt count = PR_AtomicDecrement((PRInt32*)&mRefCnt);
  if (count == 0) {
    if (mFreeToDie) {
      mRefCnt = 1;
      delete this;
    }
    else {
      mWorker->RemoveFeature(this, nsnull);
    }
  }
  return count;
}

NS_IMPL_QUERY_INTERFACE0(nsDOMWorkerFeature)

class nsDOMWorkerClassInfo : public nsIClassInfo
{
public:
  NS_DECL_NSICLASSINFO

  NS_IMETHOD_(nsrefcnt) AddRef() {
    return 2;
  }

  NS_IMETHOD_(nsrefcnt) Release() {
    return 1;
  }

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
};

NS_IMPL_QUERY_INTERFACE1(nsDOMWorkerClassInfo, nsIClassInfo)

// Keep this list in sync with the list in nsDOMClassInfo.cpp!
NS_IMPL_CI_INTERFACE_GETTER3(nsDOMWorkerClassInfo, nsIWorker,
                                                   nsIAbstractWorker,
                                                   nsIDOMEventTarget)

NS_IMPL_THREADSAFE_DOM_CI(nsDOMWorkerClassInfo)

static nsDOMWorkerClassInfo sDOMWorkerClassInfo;

nsDOMWorker::nsDOMWorker(nsDOMWorker* aParent,
                         nsIXPConnectWrappedNative* aParentWN)
: mParent(aParent),
  mParentWN(aParentWN),
  mCallbackCount(0),
  mLock(nsnull),
  mInnerScope(nsnull),
  mGlobal(NULL),
  mNextTimeoutId(0),
  mFeatureSuspendDepth(0),
  mWrappedNative(nsnull),
  mCanceled(PR_FALSE),
  mSuspended(PR_FALSE),
  mCompileAttempted(PR_FALSE)
{
#ifdef DEBUG
  PRBool mainThread = NS_IsMainThread();
  NS_ASSERTION(aParent ? !mainThread : mainThread, "Wrong thread!");
#endif
}

nsDOMWorker::~nsDOMWorker()
{
  if (mPool) {
    mPool->NoteDyingWorker(this);
  }

  if (mLock) {
    nsAutoLock::DestroyLock(mLock);
  }

  NS_ASSERTION(!mFeatures.Length(), "Live features!");
}

/* static */ nsresult
nsDOMWorker::NewWorker(nsISupports** aNewObject)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsISupports> newWorker =
    NS_ISUPPORTS_CAST(nsIWorker*, new nsDOMWorker(nsnull, nsnull));
  NS_ENSURE_TRUE(newWorker, NS_ERROR_OUT_OF_MEMORY);

  newWorker.forget(aNewObject);
  return NS_OK;
}

NS_IMPL_THREADSAFE_ADDREF(nsDOMWorker)
NS_IMPL_THREADSAFE_RELEASE(nsDOMWorker)

NS_INTERFACE_MAP_BEGIN(nsDOMWorker)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWorker)
  NS_INTERFACE_MAP_ENTRY(nsIWorker)
  NS_INTERFACE_MAP_ENTRY(nsIAbstractWorker)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
    foundInterface = static_cast<nsIClassInfo*>(&sDOMWorkerClassInfo);
  } else
NS_INTERFACE_MAP_END

// Use the xpc_map_end.h macros to generate the nsIXPCScriptable methods we want
// for the worker.

#define XPC_MAP_CLASSNAME nsDOMWorker
#define XPC_MAP_QUOTED_CLASSNAME "Worker"
#define XPC_MAP_WANT_POSTCREATE
#define XPC_MAP_WANT_TRACE
#define XPC_MAP_WANT_FINALIZE

#define XPC_MAP_FLAGS                                      \
  nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE            | \
  nsIXPCScriptable::CLASSINFO_INTERFACES_ONLY            | \
  nsIXPCScriptable::DONT_REFLECT_INTERFACE_NAMES

#include "xpc_map_end.h"

NS_IMETHODIMP
nsDOMWorker::PostCreate(nsIXPConnectWrappedNative* aWrapper,
                        JSContext* /* aCx */,
                        JSObject* /* aObj */)
{
  mWrappedNative = aWrapper;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorker::Trace(nsIXPConnectWrappedNative* /* aWrapper */,
                   JSTracer* aTracer,
                   JSObject* /*aObj */)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!IsCanceled()) {
    if (mGlobal) {
      JS_SET_TRACING_DETAILS(aTracer, nsnull, this, 0);
      JS_CallTracer(aTracer, mGlobal, JSTRACE_OBJECT);
    }
    // We should never get null handlers here if our call to Initialize succeeded.
    NS_ASSERTION(mInnerHandler && mOuterHandler, "Shouldn't be possible!");

    mInnerHandler->Trace(aTracer);
    mOuterHandler->Trace(aTracer);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorker::Finalize(nsIXPConnectWrappedNative* /* aWrapper */,
                      JSContext* aCx,
                      JSObject* /* aObj */)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Don't leave dangling JSObject pointers in our handlers!
  mInnerHandler->ClearAllListeners();
  mOuterHandler->ClearAllListeners();

  // Clear our wrapped native now that it has died.
  mWrappedNative = nsnull;

  // We no longer need to keep our inner scope.
  mGlobal = NULL;
  mInnerScope = nsnull;

  // And we can let our parent die now too.
  mParent = nsnull;
  mParentWN = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorker::Initialize(nsISupports* aOwner,
                        JSContext* aCx,
                        JSObject* aObj,
                        PRUint32 aArgc,
                        jsval* aArgv)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_ARG_POINTER(aOwner);

  nsCOMPtr<nsIScriptGlobalObject> globalObj(do_QueryInterface(aOwner));
  NS_ENSURE_TRUE(globalObj, NS_NOINTERFACE);

  return InitializeInternal(globalObj, aCx, aObj, aArgc, aArgv);
}

nsresult
nsDOMWorker::InitializeInternal(nsIScriptGlobalObject* aOwner,
                                JSContext* aCx,
                                JSObject* aObj,
                                PRUint32 aArgc,
                                jsval* aArgv)
{
  NS_ENSURE_TRUE(aArgc, NS_ERROR_INVALID_ARG);
  NS_ENSURE_ARG_POINTER(aArgv);
  NS_ENSURE_TRUE(JSVAL_IS_STRING(aArgv[0]), NS_ERROR_INVALID_ARG);

  JSString* str = JS_ValueToString(aCx, aArgv[0]);
  NS_ENSURE_STATE(str);

  mScriptURL.Assign(nsDependentJSString(str));
  NS_ENSURE_FALSE(mScriptURL.IsEmpty(), NS_ERROR_INVALID_ARG);

  mLock = nsAutoLock::NewLock("nsDOMWorker::mLock");
  NS_ENSURE_TRUE(mLock, NS_ERROR_OUT_OF_MEMORY);

  mInnerHandler = new nsDOMWorkerMessageHandler();
  NS_ENSURE_TRUE(mInnerHandler, NS_ERROR_OUT_OF_MEMORY);

  mOuterHandler = new nsDOMWorkerMessageHandler();
  NS_ENSURE_TRUE(mOuterHandler, NS_ERROR_OUT_OF_MEMORY);

  NS_ASSERTION(!mGlobal, "Already got a global?!");

  nsIXPConnect* xpc = nsContentUtils::XPConnect();

  nsCOMPtr<nsIXPConnectJSObjectHolder> thisWrapped;
  nsresult rv = xpc->WrapNative(aCx, aObj, static_cast<nsIWorker*>(this),
                                NS_GET_IID(nsISupports),
                                getter_AddRefs(thisWrapped));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(mWrappedNative, "Post-create hook should have set this!");

  // This is pretty cool - all we have to do to get our script executed is to
  // pass a no-op runnable to the thread service and it will make sure we have
  // a context and global object.
  nsCOMPtr<nsIRunnable> runnable(new nsWorkerHoldingRunnable(this));
  NS_ENSURE_TRUE(runnable, NS_ERROR_OUT_OF_MEMORY);

  nsRefPtr<nsDOMThreadService> threadService =
    nsDOMThreadService::GetOrInitService();
  NS_ENSURE_STATE(threadService);

  rv = threadService->RegisterWorker(this, aOwner);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(mPool, "RegisterWorker should have set our pool!");

  rv = threadService->Dispatch(this, runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
nsDOMWorker::Cancel()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  mCanceled = PR_TRUE;

  CancelFeatures();
}

void
nsDOMWorker::Suspend()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mSuspended, "Suspended more than once!");
  mSuspended = PR_TRUE;

  SuspendFeatures();
}

void
nsDOMWorker::Resume()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mSuspended, "Not suspended!");
  mSuspended = PR_FALSE;

  ResumeFeatures();
}

nsresult
nsDOMWorker::PostMessageInternal(const nsAString& aMessage,
                                 PRBool aToInner)
{
  nsRefPtr<nsDOMWorkerMessageEvent> message = new nsDOMWorkerMessageEvent();
  NS_ENSURE_TRUE(message, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = message->InitMessageEvent(NS_LITERAL_STRING("message"),
                                          PR_FALSE, PR_FALSE, aMessage,
                                          EmptyString(), nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsDOMFireEventRunnable> runnable =
    new nsDOMFireEventRunnable(this, message, aToInner);
  NS_ENSURE_TRUE(runnable, NS_ERROR_OUT_OF_MEMORY);

  // If aToInner is true then we want to target the runnable at this worker's
  // thread. Otherwise we need to target the parent's thread.
  nsDOMWorker* target = aToInner ? this : mParent;

  // If this is a top-level worker then target the main thread. Otherwise use
  // the thread service to find the target's thread.
  if (!target) {
    nsCOMPtr<nsIThread> mainThread;
    rv = NS_GetMainThread(getter_AddRefs(mainThread));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mainThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    rv = nsDOMThreadService::get()->Dispatch(target, runnable);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

PRBool
nsDOMWorker::SetGlobalForContext(JSContext* aCx)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (!CompileGlobalObject(aCx)) {
    return PR_FALSE;
  }

  JS_SetGlobalObject(aCx, mGlobal);
  return PR_TRUE;
}

PRBool
nsDOMWorker::CompileGlobalObject(JSContext* aCx)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mGlobal) {
    return PR_TRUE;
  }

  if (mCompileAttempted) {
    // Don't try to recompile a bad script.
    return PR_FALSE;
  }
  mCompileAttempted = PR_TRUE;

  NS_ASSERTION(!mScriptURL.IsEmpty(), "Must have a url here!");

  JSAutoRequest ar(aCx);

  NS_ASSERTION(!JS_GetGlobalObject(aCx), "Global object should be unset!");

  nsRefPtr<nsDOMWorkerScope> scope = new nsDOMWorkerScope(this);
  NS_ENSURE_TRUE(scope, PR_FALSE);

  nsISupports* scopeSupports = NS_ISUPPORTS_CAST(nsIWorkerScope*, scope);

  nsIXPConnect* xpc = nsContentUtils::XPConnect();

  nsCOMPtr<nsIXPConnectJSObjectHolder> globalWrapper;
  nsresult rv =
    xpc->InitClassesWithNewWrappedGlobal(aCx, scopeSupports,
                                         NS_GET_IID(nsISupports),
                                         nsIXPConnect::INIT_JS_STANDARD_CLASSES,
                                         getter_AddRefs(globalWrapper));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  JSObject* global;
  rv = globalWrapper->GetJSObject(&global);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  NS_ASSERTION(JS_GetGlobalObject(aCx) == global, "Global object mismatch!");

  // XXX Fix this!
  PRBool success = JS_DeleteProperty(aCx, global, "Components");
  NS_ENSURE_TRUE(success, PR_FALSE);

  // Set up worker thread functions
  success = JS_DefineFunctions(aCx, global, gDOMWorkerFunctions);
  NS_ENSURE_TRUE(success, PR_FALSE);

  // From here on out we have to remember to null mGlobal and mInnerScope if
  // something fails!
  mGlobal = global;
  mInnerScope = scope;

  nsRefPtr<nsDOMWorkerScriptLoader> loader =
    new nsDOMWorkerScriptLoader(this);
  NS_ASSERTION(loader, "Out of memory!");
  if (!loader) {
    mGlobal = NULL;
    mInnerScope = nsnull;
    return PR_FALSE;
  }

  rv = AddFeature(loader, aCx);
  if (NS_FAILED(rv)) {
    mGlobal = NULL;
    mInnerScope = nsnull;
    return PR_FALSE;
  }

  rv = loader->LoadScript(aCx, mScriptURL);

  JS_ReportPendingException(aCx);

  if (NS_FAILED(rv)) {
    mGlobal = NULL;
    mInnerScope = nsnull;
    return PR_FALSE;
  }

  return PR_TRUE;
}

void
nsDOMWorker::SetPool(nsDOMWorkerPool* aPool)
{
  NS_ASSERTION(!mPool, "Shouldn't ever set pool more than once!");
  mPool = aPool;
}

already_AddRefed<nsIXPConnectWrappedNative>
nsDOMWorker::GetWrappedNative()
{
  nsCOMPtr<nsIXPConnectWrappedNative> wrappedNative = mWrappedNative;
  NS_ASSERTION(wrappedNative, "Null wrapped native!");
  return wrappedNative.forget();
}

nsresult
nsDOMWorker::AddFeature(nsDOMWorkerFeature* aFeature,
                        JSContext* aCx)
{
  NS_ASSERTION(aFeature, "Null pointer!");

  PRBool shouldSuspend;
  {
    // aCx may be null.
    JSAutoSuspendRequest asr(aCx);

    nsAutoLock lock(mLock);

    nsDOMWorkerFeature** newFeature = mFeatures.AppendElement(aFeature);
    NS_ENSURE_TRUE(newFeature, NS_ERROR_OUT_OF_MEMORY);

    aFeature->FreeToDie(PR_FALSE);
    shouldSuspend = mFeatureSuspendDepth > 0;
  }

  if (shouldSuspend) {
    aFeature->Suspend();
  }

  return NS_OK;
}

void
nsDOMWorker::RemoveFeature(nsDOMWorkerFeature* aFeature,
                           JSContext* aCx)
{
  NS_ASSERTION(aFeature, "Null pointer!");

  // This *must* be a nsRefPtr so that we call Release after setting FreeToDie.
  nsRefPtr<nsDOMWorkerFeature> feature(aFeature);
  {
    // aCx may be null.
    JSAutoSuspendRequest asr(aCx);

    nsAutoLock lock(mLock);

#ifdef DEBUG
    PRBool removed =
#endif
    mFeatures.RemoveElement(aFeature);
    NS_ASSERTION(removed, "Feature not in the list!");

    aFeature->FreeToDie(PR_TRUE);
  }
}

void
nsDOMWorker::CancelTimeoutWithId(PRUint32 aId)
{
  nsRefPtr<nsDOMWorkerFeature> foundFeature;
  {
    nsAutoLock lock(mLock);
    PRUint32 count = mFeatures.Length();
    for (PRUint32 index = 0; index < count; index++) {
      nsDOMWorkerFeature*& feature = mFeatures[index];
      if (feature->HasId() && feature->GetId() == aId) {
        foundFeature = feature;
        feature->FreeToDie(PR_TRUE);
        mFeatures.RemoveElementAt(index);
        break;
      }
    }
  }

  if (foundFeature) {
    foundFeature->Cancel();
  }
}

void
nsDOMWorker::SuspendFeatures()
{
  nsAutoTArray<nsRefPtr<nsDOMWorkerFeature>, 20> features;
  {
    nsAutoLock lock(mLock);

    // We don't really have to worry about overflow here because the only way
    // to do this is through recursive script loading, which uses the stack. We
    // would exceed our stack limit long before this counter.
    NS_ASSERTION(mFeatureSuspendDepth < PR_UINT32_MAX, "Shouldn't happen!");
    if (++mFeatureSuspendDepth != 1) {
      // Allow nested suspending of timeouts.
      return;
    }

#ifdef DEBUG
    nsRefPtr<nsDOMWorkerFeature>* newFeatures =
#endif
    features.AppendElements(mFeatures);
    NS_WARN_IF_FALSE(newFeatures, "Out of memory!");
  }

  PRUint32 count = features.Length();
  for (PRUint32 i = 0; i < count; i++) {
    features[i]->Suspend();
  }
}

void
nsDOMWorker::ResumeFeatures()
{
  nsAutoTArray<nsRefPtr<nsDOMWorkerFeature>, 20> features;
  {
    nsAutoLock lock(mLock);

    NS_ASSERTION(mFeatureSuspendDepth > 0, "Shouldn't happen!");
    if (--mFeatureSuspendDepth != 0) {
      return;
    }

    features.AppendElements(mFeatures);
  }

  PRUint32 count = features.Length();
  for (PRUint32 i = 0; i < count; i++) {
    features[i]->Resume();
  }
}

void
nsDOMWorker::CancelFeatures()
{
  NS_ASSERTION(IsCanceled(), "More items can still be added!");

  PRUint32 count, index;

  nsAutoTArray<nsRefPtr<nsDOMWorkerFeature>, 20> features;
  {
    nsAutoLock lock(mLock);

    count = mFeatures.Length();
    for (index = 0; index < count; index++) {
      nsDOMWorkerFeature*& feature = mFeatures[index];

#ifdef DEBUG
      nsRefPtr<nsDOMWorkerFeature>* newFeature =
#endif
      features.AppendElement(feature);
      NS_ASSERTION(newFeature, "Out of memory!");

      feature->FreeToDie(PR_TRUE);
    }

    mFeatures.Clear();
  }

  count = features.Length();
  for (index = 0; index < count; index++) {
    features[index]->Cancel();
  }
}

already_AddRefed<nsDOMWorker>
nsDOMWorker::GetParent()
{
  nsRefPtr<nsDOMWorker> parent(mParent);
  return parent.forget();
}

/**
 * See nsIWorker
 */
NS_IMETHODIMP
nsDOMWorker::PostMessage(const nsAString& aMessage,
                         nsIWorkerMessagePort* aMessagePort)
{
  if (aMessagePort) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return PostMessageInternal(aMessage, PR_TRUE);
}

/**
 * See nsIWorker
 */
NS_IMETHODIMP
nsDOMWorker::GetOnerror(nsIDOMEventListener** aOnerror)
{
  NS_ENSURE_ARG_POINTER(aOnerror);

  nsCOMPtr<nsIDOMEventListener> listener =
    mOuterHandler->GetOnXListener(NS_LITERAL_STRING("error"));

  listener.forget(aOnerror);
  return NS_OK;
}

/**
 * See nsIWorker
 */
NS_IMETHODIMP
nsDOMWorker::SetOnerror(nsIDOMEventListener* aOnerror)
{
  return mOuterHandler->SetOnXListener(NS_LITERAL_STRING("error"), aOnerror);
}

/**
 * See nsIWorker
 */
NS_IMETHODIMP
nsDOMWorker::GetOnmessage(nsIDOMEventListener** aOnmessage)
{
  NS_ENSURE_ARG_POINTER(aOnmessage);

  nsCOMPtr<nsIDOMEventListener> listener =
    mOuterHandler->GetOnXListener(NS_LITERAL_STRING("message"));

  listener.forget(aOnmessage);
  return NS_OK;
}

/**
 * See nsIWorker
 */
NS_IMETHODIMP
nsDOMWorker::SetOnmessage(nsIDOMEventListener* aOnmessage)
{
  return mOuterHandler->SetOnXListener(NS_LITERAL_STRING("message"),
                                       aOnmessage);
}
