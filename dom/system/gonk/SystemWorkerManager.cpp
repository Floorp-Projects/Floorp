/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SystemWorkerManager.h"

#include "nsINetworkService.h"
#include "nsIWifi.h"
#include "nsIWorkerHolder.h"
#include "nsIXPConnect.h"

#include "jsfriendapi.h"
#include "mozilla/dom/workers/Workers.h"
#include "mozilla/ipc/Netd.h"
#include "AutoMounter.h"
#include "TimeZoneSettingObserver.h"
#include "AudioManager.h"
#ifdef MOZ_B2G_RIL
#include "mozilla/ipc/Ril.h"
#endif
#ifdef MOZ_NFC
#include "mozilla/ipc/Nfc.h"
#endif
#include "mozilla/ipc/KeyStore.h"
#include "nsIObserverService.h"
#include "nsCxPusher.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsRadioInterfaceLayer.h"
#include "WifiWorker.h"
#include "mozilla/Services.h"

USING_WORKERS_NAMESPACE

using namespace mozilla::dom::gonk;
using namespace mozilla::ipc;
using namespace mozilla::system;

namespace {

NS_DEFINE_CID(kWifiWorkerCID, NS_WIFIWORKER_CID);
NS_DEFINE_CID(kNetworkServiceCID, NS_INETWORKSERVICE_IID);

// Doesn't carry a reference, we're owned by services.
SystemWorkerManager *gInstance = nullptr;

bool
DoNetdCommand(JSContext *cx, unsigned argc, JS::Value *vp)
{
  NS_ASSERTION(!NS_IsMainThread(), "Expecting to be on the worker thread");

  if (argc != 1) {
    JS_ReportError(cx, "Expecting a single argument with the Netd message");
    return false;
  }

  JS::Value v = JS_ARGV(cx, vp)[0];

  JSAutoByteString abs;
  void *data;
  size_t size;
  if (JSVAL_IS_STRING(v)) {
    JSString *str = JSVAL_TO_STRING(v);
    if (!abs.encodeUtf8(cx, str)) {
      return false;
    }

    size = abs.length();
    if (!size) {
      JS_ReportError(cx, "Command length is zero");
      return false;
    }

    data = abs.ptr();
    if (!data) {
      JS_ReportError(cx, "Command string is empty");
      return false;
    }
  } else if (!JSVAL_IS_PRIMITIVE(v)) {
    JSObject *obj = JSVAL_TO_OBJECT(v);
    if (!JS_IsTypedArrayObject(obj)) {
      JS_ReportError(cx, "Object passed in wasn't a typed array");
      return false;
    }

    uint32_t type = JS_GetArrayBufferViewType(obj);
    if (type != js::ArrayBufferView::TYPE_INT8 &&
        type != js::ArrayBufferView::TYPE_UINT8 &&
        type != js::ArrayBufferView::TYPE_UINT8_CLAMPED) {
      JS_ReportError(cx, "Typed array data is not octets");
      return false;
    }

    size = JS_GetTypedArrayByteLength(obj);
    if (!size) {
      JS_ReportError(cx, "Typed array byte length is zero");
      return false;
    }

    data = JS_GetArrayBufferViewData(obj);
    if (!data) {
      JS_ReportError(cx, "Array buffer view data is NULL");
      return false;
    }
  } else {
    JS_ReportError(cx,
                   "Incorrect argument. Expecting a string or a typed array");
    return false;
  }

  // Reserve one space for '\0'.
  if (size > MAX_COMMAND_SIZE - 1 || size <= 0) {
    JS_ReportError(cx, "Passed-in data size is invalid");
    return false;
  }

  NetdCommand* command = new NetdCommand();

  memcpy(command->mData, data, size);
  // Include the null terminate to the command to make netd happy.
  command->mData[size] = 0;
  command->mSize = size + 1;
  SendNetdCommand(command);
  return true;
}

class ConnectWorkerToNetd : public WorkerTask
{
public:
  virtual bool RunTask(JSContext *aCx);
};

bool
ConnectWorkerToNetd::RunTask(JSContext *aCx)
{
  // Set up the DoNetdCommand on the function for worker <--> Netd process
  // communication.
  NS_ASSERTION(!NS_IsMainThread(), "Expecting to be on the worker thread");
  NS_ASSERTION(!JS_IsRunning(aCx), "Are we being called somehow?");
  JSObject *workerGlobal = JS::CurrentGlobalOrNull(aCx);
  return !!JS_DefineFunction(aCx, workerGlobal, "postNetdCommand",
                             DoNetdCommand, 1, 0);
}

class NetdReceiver : public NetdConsumer
{
  class DispatchNetdEvent : public WorkerTask
  {
  public:
    DispatchNetdEvent(NetdCommand *aMessage)
      : mMessage(aMessage)
    { }

    virtual bool RunTask(JSContext *aCx);

  private:
    nsAutoPtr<NetdCommand> mMessage;
  };

public:
  NetdReceiver(WorkerCrossThreadDispatcher *aDispatcher)
    : mDispatcher(aDispatcher)
  { }

  virtual void MessageReceived(NetdCommand *aMessage) {
    nsRefPtr<DispatchNetdEvent> dre(new DispatchNetdEvent(aMessage));
    if (!mDispatcher->PostTask(dre)) {
      NS_WARNING("Failed to PostTask to net worker");
    }
  }

private:
  nsRefPtr<WorkerCrossThreadDispatcher> mDispatcher;
};

bool
NetdReceiver::DispatchNetdEvent::RunTask(JSContext *aCx)
{
  JSObject *obj = JS::CurrentGlobalOrNull(aCx);

  JSObject *array = JS_NewUint8Array(aCx, mMessage->mSize);
  if (!array) {
    return false;
  }

  memcpy(JS_GetUint8ArrayData(array), mMessage->mData, mMessage->mSize);
  JS::Value argv[] = { OBJECT_TO_JSVAL(array) };
  return JS_CallFunctionName(aCx, obj, "onNetdMessage", NS_ARRAY_LENGTH(argv),
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
  gInstance = nullptr;
}

nsresult
SystemWorkerManager::Init()
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ASSERTION(NS_IsMainThread(), "We can only initialize on the main thread");
  NS_ASSERTION(!mShutdown, "Already shutdown!");

  mozilla::AutoSafeJSContext cx;

  nsresult rv = InitWifi(cx);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to initialize WiFi Networking!");
    return rv;
  }

  InitKeyStore(cx);

  InitAutoMounter();
  InitializeTimeZoneSettingObserver();
  rv = InitNetd(cx);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIAudioManager> audioManager =
    do_GetService(NS_AUDIOMANAGER_CONTRACTID);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    NS_WARNING("Failed to get observer service!");
    return NS_ERROR_FAILURE;
  }

  rv = obs->AddObserver(this, WORKERS_SHUTDOWN_TOPIC, false);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to initialize worker shutdown event!");
    return rv;
  }

  return NS_OK;
}

void
SystemWorkerManager::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mShutdown = true;

  ShutdownAutoMounter();

#ifdef MOZ_B2G_RIL
  RilConsumer::Shutdown();
#endif

#ifdef MOZ_NFC
  NfcConsumer::Shutdown();
#endif

  StopNetd();
  mNetdWorker = nullptr;

  nsCOMPtr<nsIWifi> wifi(do_QueryInterface(mWifiWorker));
  if (wifi) {
    wifi->Shutdown();
    wifi = nullptr;
  }
  mWifiWorker = nullptr;

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
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
      return nullptr;
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

  if (aIID.Equals(NS_GET_IID(nsIWifi))) {
    return CallQueryInterface(mWifiWorker,
                              reinterpret_cast<nsIWifi**>(aResult));
  }

  if (aIID.Equals(NS_GET_IID(nsINetworkService))) {
    return CallQueryInterface(mNetdWorker,
                              reinterpret_cast<nsINetworkService**>(aResult));
  }

  NS_WARNING("Got nothing for the requested IID!");
  return NS_ERROR_NO_INTERFACE;
}

nsresult
SystemWorkerManager::RegisterRilWorker(unsigned int aClientId,
                                       JS::Handle<JS::Value> aWorker,
                                       JSContext *aCx)
{
#ifndef MOZ_B2G_RIL
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  NS_ENSURE_TRUE(aWorker.isObject(), NS_ERROR_UNEXPECTED);

  JSAutoCompartment ac(aCx, &aWorker.toObject());

  WorkerCrossThreadDispatcher *wctd =
    GetWorkerCrossThreadDispatcher(aCx, aWorker);
  if (!wctd) {
    NS_WARNING("Failed to GetWorkerCrossThreadDispatcher for ril");
    return NS_ERROR_FAILURE;
  }

  return RilConsumer::Register(aClientId, wctd);
#endif // MOZ_B2G_RIL
}

nsresult
SystemWorkerManager::RegisterNfcWorker(JS::Handle<JS::Value> aWorker,
                                       JSContext* aCx)
{
#ifndef MOZ_NFC
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  NS_ENSURE_TRUE(aWorker.isObject(), NS_ERROR_UNEXPECTED);

  JSAutoCompartment ac(aCx, &aWorker.toObject());

  WorkerCrossThreadDispatcher* wctd =
    GetWorkerCrossThreadDispatcher(aCx, aWorker);
  if (!wctd) {
    NS_WARNING("Failed to GetWorkerCrossThreadDispatcher for nfc");
    return NS_ERROR_FAILURE;
  }

  return NfcConsumer::Register(wctd);
#endif // MOZ_NFC
}

nsresult
SystemWorkerManager::InitNetd(JSContext *cx)
{
  nsCOMPtr<nsIWorkerHolder> worker = do_GetService(kNetworkServiceCID);
  NS_ENSURE_TRUE(worker, NS_ERROR_FAILURE);

  JS::Rooted<JS::Value> workerval(cx);
  nsresult rv = worker->GetWorker(&workerval);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(workerval.isObject(), NS_ERROR_UNEXPECTED);

  JSAutoCompartment ac(cx, &workerval.toObject());

  WorkerCrossThreadDispatcher *wctd =
    GetWorkerCrossThreadDispatcher(cx, workerval);
  if (!wctd) {
    NS_WARNING("Failed to GetWorkerCrossThreadDispatcher for netd");
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<ConnectWorkerToNetd> connection = new ConnectWorkerToNetd();
  if (!wctd->PostTask(connection)) {
    NS_WARNING("Failed to connect worker to netd");
    return NS_ERROR_UNEXPECTED;
  }

  // Now that we're set up, connect ourselves to the Netd process.
  mozilla::RefPtr<NetdReceiver> receiver = new NetdReceiver(wctd);
  StartNetd(receiver);
  mNetdWorker = worker;
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

nsresult
SystemWorkerManager::InitKeyStore(JSContext *cx)
{
  mKeyStore = new KeyStore();
  return NS_OK;
}

NS_IMPL_ISUPPORTS3(SystemWorkerManager,
                   nsIObserver,
                   nsIInterfaceRequestor,
                   nsISystemWorkerManager)

NS_IMETHODIMP
SystemWorkerManager::Observe(nsISupports *aSubject, const char *aTopic,
                             const char16_t *aData)
{
  if (!strcmp(aTopic, WORKERS_SHUTDOWN_TOPIC)) {
    Shutdown();
  }

  return NS_OK;
}
