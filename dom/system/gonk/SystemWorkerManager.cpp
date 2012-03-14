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

#include "SystemWorkerManager.h"

#include "nsIObserverService.h"
#include "nsIJSContextStack.h"
#include "nsIRadioInterfaceLayer.h"
#include "nsIWifi.h"
#include "nsIWorkerHolder.h"
#include "nsIXPConnect.h"

#include "jstypedarray.h"
#include "mozilla/dom/workers/Workers.h"
#include "mozilla/ipc/Ril.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsRadioInterfaceLayer.h"
#include "nsWifiWorker.h"

USING_TELEPHONY_NAMESPACE
USING_WORKERS_NAMESPACE
using namespace mozilla::ipc;

namespace {

NS_DEFINE_CID(kRadioInterfaceLayerCID, NS_RADIOINTERFACELAYER_CID);
NS_DEFINE_CID(kWifiWorkerCID, NS_WIFIWORKER_CID);

// Doesn't carry a reference, we're owned by services.
SystemWorkerManager *gInstance = nsnull;

class ConnectWorkerToRIL : public WorkerTask
{
public:
  virtual bool RunTask(JSContext *aCx);
};

JSBool
PostToRIL(JSContext *cx, unsigned argc, jsval *vp)
{
  NS_ASSERTION(!NS_IsMainThread(), "Expecting to be on the worker thread");

  if (argc != 1) {
    JS_ReportError(cx, "Expecting a single argument with the RIL message");
    return false;
  }

  jsval v = JS_ARGV(cx, vp)[0];

  nsAutoPtr<RilRawData> rm(new RilRawData());
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

    uint32_t type = JS_GetTypedArrayType(obj);
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

  if (size > RilRawData::MAX_DATA_SIZE) {
    JS_ReportError(cx, "Passed-in data is too large");
    return false;
  }

  rm->mSize = size;
  memcpy(rm->mData, data, size);

  RilRawData *tosend = rm.forget();
  JS_ALWAYS_TRUE(SendRilRawData(&tosend));
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

  return !!JS_DefineFunction(aCx, workerGlobal, "postRILMessage", PostToRIL, 1,
                             0);
}

class RILReceiver : public RilConsumer
{
  class DispatchRILEvent : public WorkerTask
  {
  public:
    DispatchRILEvent(RilRawData *aMessage)
      : mMessage(aMessage)
    { }

    virtual bool RunTask(JSContext *aCx);

  private:
    nsAutoPtr<RilRawData> mMessage;
  };

public:
  RILReceiver(WorkerCrossThreadDispatcher *aDispatcher)
    : mDispatcher(aDispatcher)
  { }

  virtual void MessageReceived(RilRawData *aMessage) {
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

SystemWorkerManager::SystemWorkerManager()
  : mShutdown(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance, "There should only be one instance!");
}

SystemWorkerManager::~SystemWorkerManager()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance || gInstance == this,
               "There should only be one instance!");
  gInstance = nsnull;
}

nsresult
SystemWorkerManager::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "We can only initialize on the main thread");
  NS_ASSERTION(!mShutdown, "Already shutdown!");

  JSContext *cx;
  nsresult rv = nsContentUtils::ThreadJSContextStack()->GetSafeJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCxPusher pusher;
  if (!cx || !pusher.Push(cx, false)) {
    return NS_ERROR_FAILURE;
  }

  rv = InitRIL(cx);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = InitWifi(cx);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObserverService> obs =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (!obs) {
    NS_WARNING("Failed to get observer service!");
    return NS_ERROR_FAILURE;
  }

  rv = obs->AddObserver(this, WORKERS_SHUTDOWN_TOPIC, false);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
SystemWorkerManager::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mShutdown = true;

  StopRil();

  mRILWorker = nsnull;
  mWifiWorker = nsnull;

  nsCOMPtr<nsIObserverService> obs =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (obs) {
    obs->RemoveObserver(this, WORKERS_SHUTDOWN_TOPIC);
  }
}

// static
already_AddRefed<SystemWorkerManager>
SystemWorkerManager::FactoryCreate()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<SystemWorkerManager> instance(gInstance);

  if (!instance) {
    instance = new SystemWorkerManager();
    if (NS_FAILED(instance->Init())) {
      instance->Shutdown();
      return nsnull;
    }

    gInstance = instance;
  }

  return instance.forget();
}

// static
nsIInterfaceRequestor*
SystemWorkerManager::GetInterfaceRequestor()
{
  return gInstance;
}

NS_IMETHODIMP
SystemWorkerManager::GetInterface(const nsIID &aIID, void **aResult)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (aIID.Equals(NS_GET_IID(nsIRadioInterfaceLayer))) {
    return CallQueryInterface(mRILWorker,
                              reinterpret_cast<nsIRadioInterfaceLayer**>(aResult));
  }

  if (aIID.Equals(NS_GET_IID(nsIWifi))) {
    return CallQueryInterface(mWifiWorker,
                              reinterpret_cast<nsIWifi**>(aResult));
  }

  NS_WARNING("Got nothing for the requested IID!");
  return NS_ERROR_NO_INTERFACE;
}

nsresult
SystemWorkerManager::InitRIL(JSContext *cx)
{
  // We're keeping as much of this implementation as possible in JS, so the real
  // worker lives in RadioInterfaceLayer.js. All we do here is hold it alive and
  // hook it up to the RIL thread.
  nsCOMPtr<nsIWorkerHolder> worker = do_CreateInstance(kRadioInterfaceLayerCID);
  NS_ENSURE_TRUE(worker, NS_ERROR_FAILURE);

  jsval workerval;
  nsresult rv = worker->GetWorker(&workerval);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(!JSVAL_IS_PRIMITIVE(workerval), NS_ERROR_UNEXPECTED);

  JSAutoRequest ar(cx);
  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, JSVAL_TO_OBJECT(workerval))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  WorkerCrossThreadDispatcher *wctd =
    GetWorkerCrossThreadDispatcher(cx, workerval);
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

  mRILWorker = worker;
  return NS_OK;
}

nsresult
SystemWorkerManager::InitWifi(JSContext *cx)
{
  nsCOMPtr<nsIWorkerHolder> worker = do_CreateInstance(kWifiWorkerCID);
  NS_ENSURE_TRUE(worker, NS_ERROR_FAILURE);

  mWifiWorker = worker;
  return NS_OK;
}

NS_IMPL_ISUPPORTS2(SystemWorkerManager, nsIObserver, nsIInterfaceRequestor)

NS_IMETHODIMP
SystemWorkerManager::Observe(nsISupports *aSubject, const char *aTopic,
                             const PRUnichar *aData)
{
  if (!strcmp(aTopic, WORKERS_SHUTDOWN_TOPIC)) {
    Shutdown();
  }

  return NS_OK;
}
