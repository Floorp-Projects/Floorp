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
#include "nsCxPusher.h"

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
static nsAutoPtr<WpaSupplicant> gWpaSupplicant;

// Runnable used dispatch the WaitForEvent result on the main thread.
class WifiEventDispatcher : public nsRunnable
{
public:
  WifiEventDispatcher(const nsAString& aEvent, const nsACString& aInterface)
    : mEvent(aEvent)
    , mInterface(aInterface)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run()
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
class EventRunnable : public nsRunnable
{
public:
  EventRunnable(const nsACString& aInterface)
    : mInterface(aInterface)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    nsAutoString event;
    gWpaSupplicant->WaitForEvent(event, mInterface);
    if (!event.IsEmpty()) {
#ifdef MOZ_TASK_TRACER
      // Make wifi initialization events to be the source events of TaskTracer,
      // and originate the rest correlation tasks from here.
      AutoSourceEvent taskTracerEvent(SourceEventType::WIFI);
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
class WifiResultDispatcher : public nsRunnable
{
public:
  WifiResultDispatcher(WifiResultOptions& aResult, const nsACString& aInterface)
    : mInterface(aInterface)
  {
    MOZ_ASSERT(!NS_IsMainThread());

    // XXX: is there a better way to copy webidl dictionnaries?
    // the copy constructor is private.
#define COPY_FIELD(prop) mResult.prop = aResult.prop;

    COPY_FIELD(mId)
    COPY_FIELD(mStatus)
    COPY_FIELD(mReply)
    COPY_FIELD(mRoute)
    COPY_FIELD(mError)
    COPY_FIELD(mValue)
    COPY_FIELD(mIpaddr_str)
    COPY_FIELD(mGateway_str)
    COPY_FIELD(mDns1_str)
    COPY_FIELD(mDns2_str)
    COPY_FIELD(mMask_str)
    COPY_FIELD(mServer_str)
    COPY_FIELD(mVendor_str)
    COPY_FIELD(mLease)
    COPY_FIELD(mMask)
    COPY_FIELD(mIpaddr)
    COPY_FIELD(mGateway)
    COPY_FIELD(mDns1)
    COPY_FIELD(mDns2)
    COPY_FIELD(mServer)

#undef COPY_FIELD
  }

  NS_IMETHOD Run()
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
class ControlRunnable : public nsRunnable
{
public:
  ControlRunnable(CommandOptions aOptions, const nsACString& aInterface)
    : mOptions(aOptions)
    , mInterface(aInterface)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run()
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
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!gWifiProxyService) {
    gWifiProxyService = new WifiProxyService();
    ClearOnShutdown(&gWifiProxyService);

    gWpaSupplicant = new WpaSupplicant();
    ClearOnShutdown(&gWpaSupplicant);
  }

  nsRefPtr<WifiProxyService> service = gWifiProxyService.get();
  return service.forget();
}

NS_IMETHODIMP
WifiProxyService::Start(nsIWifiEventListener* aListener,
                        const char ** aInterfaces,
                        uint32_t aNumOfInterfaces)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);

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

  // Dispatch to the event thread which has the given interface name
  for (size_t i = 0; i < mEventThreadList.Length(); i++) {
    if (mEventThreadList[i].mInterface.Equals(aInterface)) {
      nsCOMPtr<nsIRunnable> runnable = new EventRunnable(aInterface);
      mEventThreadList[i].mThread->Dispatch(runnable, nsIEventTarget::DISPATCH_NORMAL);
      return NS_OK;
    }
  }

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

  // Call the listener with a JS value.
  mListener->OnCommand(val, aInterface);
}

void
WifiProxyService::DispatchWifiEvent(const nsAString& aEvent, const nsACString& aInterface)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsAutoString event;
  if (StringBeginsWith(aEvent, NS_LITERAL_STRING("IFNAME"))) {
    // Jump over IFNAME for gonk-kk.
    event = Substring(aEvent, aEvent.FindChar(' ') + 1);
  }
  else {
    event = aEvent;
  }
  // Call the listener.
  mListener->OnWaitEvent(event, aInterface);
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
