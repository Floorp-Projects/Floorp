/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WifiProxyService.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsXULAppAPI.h"
#include "WifiUtils.h"

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
using namespace mozilla::tasktracer;
#endif

#define NS_WIFIPROXYSERVICE_CID \
  { 0xc6c9be7e, 0x744f, 0x4222, {0xb2, 0x03, 0xcd, 0x55, 0xdf, 0xc8, 0xbc, 0x12} }

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {

// The singleton Wifi service, to be used on the main thread.
static StaticRefPtr<WifiProxyService> gWifiProxyService;

// The singleton supplicant class, that can be used on any thread.
static UniquePtr<WpaSupplicant> gWpaSupplicant;

// Runnable used dispatch the WaitForEvent result on the main thread.
class WifiEventDispatcher : public Runnable
{
public:
  WifiEventDispatcher(const nsAString& aEvent, const nsACString& aInterface)
    : mEvent(aEvent)
    , mInterface(aInterface)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    gWifiProxyService->DispatchWifiEvent(mEvent, mInterface);
    return NS_OK;
  }

private:
  nsString mEvent;
  nsCString mInterface;
};

// Runnable used to call WaitForEvent on the event thread.
class EventRunnable : public Runnable
{
public:
  EventRunnable(const nsACString& aInterface)
    : mInterface(aInterface)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread());
    nsAutoString event;
    gWpaSupplicant->WaitForEvent(event, mInterface);
    if (!event.IsEmpty()) {
#ifdef MOZ_TASK_TRACER
      // Make wifi initialization events to be the source events of TaskTracer,
      // and originate the rest correlation tasks from here.
      AutoSourceEvent taskTracerEvent(SourceEventType::Wifi);
      AddLabel("%s %s", mInterface.get(), NS_ConvertUTF16toUTF8(event).get());
#endif
      nsCOMPtr<nsIRunnable> runnable = new WifiEventDispatcher(event, mInterface);
      NS_DispatchToMainThread(runnable);
    }
    return NS_OK;
  }

private:
  nsCString mInterface;
};

// Runnable used dispatch the Command result on the main thread.
class WifiResultDispatcher : public Runnable
{
public:
  WifiResultDispatcher(WifiResultOptions& aResult, const nsACString& aInterface)
    : mResult(aResult)
    , mInterface(aInterface)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    gWifiProxyService->DispatchWifiResult(mResult, mInterface);
    return NS_OK;
  }

private:
  WifiResultOptions mResult;
  nsCString mInterface;
};

// Runnable used to call SendCommand on the control thread.
class ControlRunnable : public Runnable
{
public:
  ControlRunnable(CommandOptions aOptions, const nsACString& aInterface)
    : mOptions(aOptions)
    , mInterface(aInterface)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run() override
  {
    WifiResultOptions result;
    if (gWpaSupplicant->ExecuteCommand(mOptions, result, mInterface)) {
      nsCOMPtr<nsIRunnable> runnable = new WifiResultDispatcher(result, mInterface);
      NS_DispatchToMainThread(runnable);
    }
    return NS_OK;
  }
private:
   CommandOptions mOptions;
   nsCString mInterface;
};

NS_IMPL_ISUPPORTS(WifiProxyService, nsIWifiProxyService)

WifiProxyService::WifiProxyService()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gWifiProxyService);
}

WifiProxyService::~WifiProxyService()
{
  MOZ_ASSERT(!gWifiProxyService);
}

already_AddRefed<WifiProxyService>
WifiProxyService::FactoryCreate()
{
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!gWifiProxyService) {
    gWifiProxyService = new WifiProxyService();
    ClearOnShutdown(&gWifiProxyService);

    gWpaSupplicant = MakeUnique<WpaSupplicant>();
    ClearOnShutdown(&gWpaSupplicant);
  }

  RefPtr<WifiProxyService> service = gWifiProxyService.get();
  return service.forget();
}

NS_IMETHODIMP
WifiProxyService::Start(nsIWifiEventListener* aListener,
                        const char ** aInterfaces,
                        uint32_t aNumOfInterfaces)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);

#if ANDROID_VERSION >= 19
  // KK changes the way mux'ing/demux'ing different supplicant interfaces
  // (e.g. wlan0/p2p0) from multi-sockets to single socket embedded with
  // prefixed interface name (e.g. IFNAME=wlan0 xxxxxx). Therefore, we use
  // the first given interface as the global interface for KK.
  aNumOfInterfaces = 1;
#endif

  nsresult rv;

  // Since EventRunnable runs in the manner of blocking, we have to
  // spin a thread for each interface.
  // (See the WpaSupplicant::WaitForEvent)
  mEventThreadList.SetLength(aNumOfInterfaces);
  for (uint32_t i = 0; i < aNumOfInterfaces; i++) {
    mEventThreadList[i].mInterface = aInterfaces[i];
    rv = NS_NewThread(getter_AddRefs(mEventThreadList[i].mThread));
    if (NS_FAILED(rv)) {
      NS_WARNING("Can't create wifi event thread");
      Shutdown();
      return NS_ERROR_FAILURE;
    }
  }

  rv = NS_NewThread(getter_AddRefs(mControlThread));
  if (NS_FAILED(rv)) {
    NS_WARNING("Can't create wifi control thread");
    Shutdown();
    return NS_ERROR_FAILURE;
  }

  mListener = aListener;

  return NS_OK;
}

NS_IMETHODIMP
WifiProxyService::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  for (size_t i = 0; i < mEventThreadList.Length(); i++) {
    if (mEventThreadList[i].mThread) {
      mEventThreadList[i].mThread->Shutdown();
      mEventThreadList[i].mThread = nullptr;
    }
  }

  mEventThreadList.Clear();

  if (mControlThread) {
    mControlThread->Shutdown();
    mControlThread = nullptr;
  }

  mListener = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
WifiProxyService::SendCommand(JS::Handle<JS::Value> aOptions,
                              const nsACString& aInterface,
                              JSContext* aCx)
{
  MOZ_ASSERT(NS_IsMainThread());
  WifiCommandOptions options;

  if (!options.Init(aCx, aOptions)) {
    NS_WARNING("Bad dictionary passed to WifiProxyService::SendCommand");
    return NS_ERROR_FAILURE;
  }

  if (!mControlThread) {
    return NS_ERROR_FAILURE;
  }

  // Dispatch the command to the control thread.
  CommandOptions commandOptions(options);
  nsCOMPtr<nsIRunnable> runnable = new ControlRunnable(commandOptions, aInterface);
  mControlThread->Dispatch(runnable, nsIEventTarget::DISPATCH_NORMAL);
  return NS_OK;
}

NS_IMETHODIMP
WifiProxyService::WaitForEvent(const nsACString& aInterface)
{
  MOZ_ASSERT(NS_IsMainThread());

#if ANDROID_VERSION >= 19
  // We will only have one global interface for KK.
  if (!mEventThreadList.IsEmpty()) {
    nsCOMPtr<nsIRunnable> runnable = new EventRunnable(aInterface);
    mEventThreadList[0].mThread->Dispatch(runnable, nsIEventTarget::DISPATCH_NORMAL);
    return NS_OK;
  }
#else
  // Dispatch to the event thread which has the given interface name
  for (size_t i = 0; i < mEventThreadList.Length(); i++) {
    if (mEventThreadList[i].mInterface.Equals(aInterface)) {
      nsCOMPtr<nsIRunnable> runnable = new EventRunnable(aInterface);
      mEventThreadList[i].mThread->Dispatch(runnable, nsIEventTarget::DISPATCH_NORMAL);
      return NS_OK;
    }
  }
#endif

  return NS_ERROR_FAILURE;
}

void
WifiProxyService::DispatchWifiResult(const WifiResultOptions& aOptions, const nsACString& aInterface)
{
  MOZ_ASSERT(NS_IsMainThread());

  mozilla::AutoSafeJSContext cx;
  JS::Rooted<JS::Value> val(cx);

  if (!ToJSValue(cx, aOptions, &val)) {
    return;
  }

  if (mListener) {
    // Call the listener with a JS value.
    mListener->OnCommand(val, aInterface);
  }
}

void
WifiProxyService::DispatchWifiEvent(const nsAString& aEvent, const nsACString& aInterface)
{
  MOZ_ASSERT(NS_IsMainThread());

#if ANDROID_VERSION < 19
  mListener->OnWaitEvent(aEvent, aInterface);
#else
  // The interface might be embedded in the event string such as
  // "IFNAME=wlan0 CTRL-EVENT-BSS-ADDED 65 3c:94:d5:7c:11:8b".
  // Parse the interface name from the event string and use p2p0
  // as the default interface if "IFNAME" is not found.
  nsAutoString event;
  nsAutoString embeddedInterface(NS_LITERAL_STRING("p2p0"));
  if (StringBeginsWith(aEvent, NS_LITERAL_STRING("IFNAME"))) {
    int32_t ifnameFrom = aEvent.FindChar('=') + 1;
    int32_t ifnameTo = aEvent.FindChar(' ') - 1;
    embeddedInterface = Substring(aEvent, ifnameFrom, ifnameTo - ifnameFrom + 1);
    event = Substring(aEvent, aEvent.FindChar(' ') + 1);
  }
  else {
    event = aEvent;
  }
  mListener->OnWaitEvent(event, NS_ConvertUTF16toUTF8(embeddedInterface));
#endif
}

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(WifiProxyService,
                                         WifiProxyService::FactoryCreate)

NS_DEFINE_NAMED_CID(NS_WIFIPROXYSERVICE_CID);

static const mozilla::Module::CIDEntry kWifiProxyServiceCIDs[] = {
  { &kNS_WIFIPROXYSERVICE_CID, false, nullptr, WifiProxyServiceConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kWifiProxyServiceContracts[] = {
  { "@mozilla.org/wifi/service;1", &kNS_WIFIPROXYSERVICE_CID },
  { nullptr }
};

static const mozilla::Module kWifiProxyServiceModule = {
  mozilla::Module::kVersion,
  kWifiProxyServiceCIDs,
  kWifiProxyServiceContracts,
  nullptr
};

} // namespace mozilla

NSMODULE_DEFN(WifiProxyServiceModule) = &kWifiProxyServiceModule;
