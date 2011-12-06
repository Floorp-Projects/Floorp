/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is Telephony.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
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

#include "RadioManager.h"
#include "nsITelephonyWorker.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "nsIJSContextStack.h"
#include "nsIObserverService.h"
#include "mozilla/dom/workers/Workers.h"
#include "jstypedarray.h"

#include "nsThreadUtils.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#else
#define LOG(args...)  printf(args);
#endif

USING_WORKERS_NAMESPACE
using namespace mozilla::ipc;

static NS_DEFINE_CID(kTelephonyWorkerCID, NS_TELEPHONYWORKER_CID);

// Topic we listen to for shutdown.
#define PROFILE_BEFORE_CHANGE_TOPIC "profile-before-change"

USING_TELEPHONY_NAMESPACE

namespace {

// Doesn't carry a reference, we're owned by services.
RadioManager* gInstance = nsnull;

class ConnectWorkerToRIL : public WorkerTask {
public:
  virtual bool RunTask(JSContext *aCx);
};

JSBool
PostToRIL(JSContext *cx, uintN argc, jsval *vp)
{
  NS_ASSERTION(!NS_IsMainThread(), "Expecting to be on the worker thread");

  if (argc != 1) {
    JS_ReportError(cx, "Expecting a single argument with the RIL message");
    return false;
  }

  jsval v = JS_ARGV(cx, vp)[0];

  nsAutoPtr<RilMessage> rm(new RilMessage());
  JSAutoByteString abs;
  void *data;
  size_t size;
  if (JSVAL_IS_STRING(v)) {
    JSString *str = JSVAL_TO_STRING(v);
    if (!abs.encode(cx, str)) {
      return false;
    }

    size = JS_GetStringLength(str);
    data = abs.ptr();
  } else if (!JSVAL_IS_PRIMITIVE(v)) {
    JSObject *obj = JSVAL_TO_OBJECT(v);
    if (!js_IsTypedArray(obj)) {
      JS_ReportError(cx, "Object passed in wasn't a typed array");
      return false;
    }

    JSUint32 type = JS_GetTypedArrayType(obj);
    if (type != js::TypedArray::TYPE_INT8 &&
        type != js::TypedArray::TYPE_UINT8 &&
        type != js::TypedArray::TYPE_UINT8_CLAMPED) {
      JS_ReportError(cx, "Typed array data is not octets");
      return false;
    }

    size = JS_GetTypedArrayByteLength(obj);
    data = JS_GetTypedArrayData(obj);
  } else {
    JS_ReportError(cx,
                   "Incorrect argument. Expecting a string or a typed array");
    return false;
  }

  if (size > RilMessage::DATA_SIZE) {
    JS_ReportError(cx, "Passed-in data is too large");
    return false;
  }

  rm->mSize = size;
  memcpy(rm->mData, data, size);

  RilMessage *tosend = rm.forget();
  JS_ALWAYS_TRUE(SendRilMessage(&tosend));
  return true;
}

bool
ConnectWorkerToRIL::RunTask(JSContext *aCx)
{
  // Set up the postRILMessage on the function for worker -> RIL thread
  // communication.
  NS_ASSERTION(!NS_IsMainThread(), "Expecting to be on the worker thread");
  NS_ASSERTION(!JS_IsRunning(aCx), "Are we being called somehow?");
  JSObject *workerGlobal = JS_GetGlobalObject(aCx);

  return JS_DefineFunction(aCx, workerGlobal, "postRILMessage", PostToRIL, 1, 0);
}

class RILReceiver : public RilConsumer
{
  class DispatchRILEvent : public WorkerTask {
  public:
    DispatchRILEvent(RilMessage *aMessage)
      : mMessage(aMessage)
    { }

    virtual bool RunTask(JSContext *aCx);

  private:
    nsAutoPtr<RilMessage> mMessage;
  };

public:
  RILReceiver(WorkerCrossThreadDispatcher *aDispatcher)
    : mDispatcher(aDispatcher)
  { }

  virtual void MessageReceived(RilMessage *aMessage) {
    nsRefPtr<DispatchRILEvent> dre(new DispatchRILEvent(aMessage));
    mDispatcher->PostTask(dre);
  }

private:
  nsRefPtr<WorkerCrossThreadDispatcher> mDispatcher;
};

bool
RILReceiver::DispatchRILEvent::RunTask(JSContext *aCx)
{
  JSObject *obj = JS_GetGlobalObject(aCx);

  JSObject *array =
    js_CreateTypedArray(aCx, js::TypedArray::TYPE_UINT8, mMessage->mSize);
  if (!array) {
    return false;
  }

  memcpy(JS_GetTypedArrayData(array), mMessage->mData, mMessage->mSize);
  jsval argv[] = { OBJECT_TO_JSVAL(array) };
  return JS_CallFunctionName(aCx, obj, "onRILMessage", NS_ARRAY_LENGTH(argv),
                             argv, argv);
}

} // anonymous namespace

RadioManager::RadioManager()
  : mShutdown(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance, "There should only be one instance!");
}

RadioManager::~RadioManager()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance || gInstance == this,
               "There should only be one instance!");
  gInstance = nsnull;
}

nsresult
RadioManager::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "We can only initialize on the main thread");

  nsCOMPtr<nsIObserverService> obs =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (!obs) {
    NS_WARNING("Failed to get observer service!");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = obs->AddObserver(this, PROFILE_BEFORE_CHANGE_TOPIC, false);
  NS_ENSURE_SUCCESS(rv, rv);

  // The telephony worker component is a hack that gives us a global object for
  // our own functions and makes creating the worker possible.
  nsCOMPtr<nsITelephonyWorker> worker(do_CreateInstance(kTelephonyWorkerCID));
  NS_ENSURE_TRUE(worker, NS_ERROR_FAILURE);

  jsval workerval;
  rv = worker->GetWorker(&workerval);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(!JSVAL_IS_PRIMITIVE(workerval), "bad worker value");

  JSContext *cx;
  rv = nsContentUtils::ThreadJSContextStack()->GetSafeJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCxPusher pusher;
  if (!cx || !pusher.Push(cx, false)) {
    return NS_ERROR_FAILURE;
  }

  JSObject *workerobj = JSVAL_TO_OBJECT(workerval);

  JSAutoRequest ar(cx);
  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, workerobj)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  WorkerCrossThreadDispatcher *wctd = GetWorkerCrossThreadDispatcher(cx, workerval);
  if (!wctd) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<ConnectWorkerToRIL> connection = new ConnectWorkerToRIL();
  if (!wctd->PostTask(connection)) {
    return NS_ERROR_UNEXPECTED;
  }

  // Now that we're set up, connect ourselves to the RIL thread.
  mozilla::RefPtr<RILReceiver> receiver = new RILReceiver(wctd);
  StartRil(receiver);

  mRadioInterface = do_QueryInterface(worker);
  NS_ENSURE_TRUE(mRadioInterface, NS_ERROR_FAILURE);

  return NS_OK;
}

void
RadioManager::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  StopRil();
  mRadioInterface = nsnull;

  mShutdown = true;
}

// static
already_AddRefed<RadioManager>
RadioManager::FactoryCreate()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<RadioManager> instance(gInstance);

  if (!instance) {
    instance = new RadioManager();
    if (NS_FAILED(instance->Init())) {
      return nsnull;
    }

    gInstance = instance;
  }

  return instance.forget();
}

// static
already_AddRefed<nsIRadioInterface>
RadioManager::GetRadioInterface()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (gInstance) {
    nsCOMPtr<nsIRadioInterface> retval = gInstance->mRadioInterface;
    return retval.forget();
  }

  return nsnull;
}


NS_IMPL_ISUPPORTS1(RadioManager, nsIObserver)

NS_IMETHODIMP
RadioManager::Observe(nsISupports* aSubject, const char* aTopic,
                      const PRUnichar* aData)
{
  if (!strcmp(aTopic, PROFILE_BEFORE_CHANGE_TOPIC)) {
    Shutdown();

    nsCOMPtr<nsIObserverService> obs =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    if (obs) {
      if (NS_FAILED(obs->RemoveObserver(this, aTopic))) {
        NS_WARNING("Failed to remove observer!");
      }
    }
    else {
      NS_WARNING("Failed to get observer service!");
    }
  }

  return NS_OK;
}
