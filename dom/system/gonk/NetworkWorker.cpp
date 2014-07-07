/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NetworkWorker.h"
#include "NetworkUtils.h"
#include <nsThreadUtils.h>
#include "mozilla/ModuleUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsXULAppAPI.h"
#include "nsCxPusher.h"

#define NS_NETWORKWORKER_CID \
  { 0x6df093e1, 0x8127, 0x4fa7, {0x90, 0x13, 0xa3, 0xaa, 0xa7, 0x79, 0xbb, 0xdd} }

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

#define PROPERTY_VALUE_MAX 80

namespace mozilla {

nsCOMPtr<nsIThread> gWorkerThread;

// The singleton network worker, to be used on the main thread.
StaticRefPtr<NetworkWorker> gNetworkWorker;

// The singleton networkutils class, that can be used on any thread.
static nsAutoPtr<NetworkUtils> gNetworkUtils;

// Runnable used dispatch command result on the main thread.
class NetworkResultDispatcher : public nsRunnable
{
public:
  NetworkResultDispatcher(const NetworkResultOptions& aResult)
  {
    MOZ_ASSERT(!NS_IsMainThread());

#define COPY_FIELD(prop) mResult.prop = aResult.prop;
#define COPY_SEQUENCE_FIELD(prop, type)                                                   \
    if (aResult.prop.WasPassed()) {                                                       \
      mozilla::dom::Sequence<type > const & currentValue = aResult.prop.InternalValue();  \
      uint32_t length = currentValue.Length();                                            \
      mResult.prop.Construct();                                                           \
      for (uint32_t idx = 0; idx < length; idx++) {                                       \
        mResult.prop.Value().AppendElement(currentValue[idx]);                            \
      }                                                                                   \
    }
    COPY_FIELD(mId)
    COPY_FIELD(mRet)
    COPY_FIELD(mBroadcast)
    COPY_FIELD(mTopic)
    COPY_FIELD(mReason)
    COPY_FIELD(mResultCode)
    COPY_FIELD(mResultReason)
    COPY_FIELD(mError)
    COPY_FIELD(mRxBytes)
    COPY_FIELD(mTxBytes)
    COPY_FIELD(mDate)
    COPY_FIELD(mEnable)
    COPY_FIELD(mResult)
    COPY_FIELD(mSuccess)
    COPY_FIELD(mCurExternalIfname)
    COPY_FIELD(mCurInternalIfname)
    COPY_FIELD(mIpAddr)
    COPY_FIELD(mGateway)
    COPY_FIELD(mDns1)
    COPY_FIELD(mDns2)
    COPY_FIELD(mServer)
    COPY_FIELD(mLease)
    COPY_FIELD(mVendorInfo)
    COPY_FIELD(mMaskLength)
    COPY_FIELD(mFlag)
    COPY_SEQUENCE_FIELD(mInterfaceList, nsString)
#undef COPY_FIELD
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (gNetworkWorker) {
      gNetworkWorker->DispatchNetworkResult(mResult);
    }
    return NS_OK;
  }
private:
  NetworkResultOptions mResult;
};

// Runnable used dispatch netd command on the worker thread.
class NetworkCommandDispatcher : public nsRunnable
{
public:
  NetworkCommandDispatcher(const NetworkParams& aParams)
    : mParams(aParams)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    if (gNetworkUtils) {
      gNetworkUtils->ExecuteCommand(mParams);
    }
    return NS_OK;
  }
private:
  NetworkParams mParams;
};

// Runnable used for blocking command.
class RunDhcpEvent : public nsRunnable
{
public:
  RunDhcpEvent(const NetworkParams& aParams)
  : mParams(aParams)
  {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    nsAutoPtr<NetUtils> netUtils;
    netUtils = new NetUtils();

    NetworkResultOptions result;
    result.mId = mParams.mId;

    int32_t status;
    char ipaddr[PROPERTY_VALUE_MAX];
    char gateway[PROPERTY_VALUE_MAX];
    uint32_t prefixLength;
    char dns1[PROPERTY_VALUE_MAX];
    char dns2[PROPERTY_VALUE_MAX];
    char server[PROPERTY_VALUE_MAX];
    uint32_t lease;
    char vendorinfo[PROPERTY_VALUE_MAX];
    status = netUtils->do_dhcp_do_request(NS_ConvertUTF16toUTF8(mParams.mIfname).get(),
                                          ipaddr,
                                          gateway,
                                          &prefixLength,
                                          dns1,
                                          dns2,
                                          server,
                                          &lease,
                                          vendorinfo);


    if (status == 0) {
      // run dhcp success.
      result.mSuccess = true;
      result.mIpAddr = NS_ConvertUTF8toUTF16(ipaddr);
      result.mGateway = NS_ConvertUTF8toUTF16(gateway);
      result.mDns1 = NS_ConvertUTF8toUTF16(dns1);
      result.mDns2 = NS_ConvertUTF8toUTF16(dns2);
      result.mServer = NS_ConvertUTF8toUTF16(server);
      result.mLease = lease;
      result.mVendorInfo = NS_ConvertUTF8toUTF16(vendorinfo);
      result.mMaskLength = prefixLength;
    }

    nsCOMPtr<nsIRunnable> runnable = new NetworkResultDispatcher(result);
    NS_DispatchToMainThread(runnable);

    return NS_OK;
  }
private:
  NetworkParams mParams;
};

// Runnable used dispatch netd result on the worker thread.
class NetdEventRunnable : public nsRunnable
{
public:
  NetdEventRunnable(NetdCommand* aCommand)
    : mCommand(aCommand)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    if (gNetworkUtils) {
      gNetworkUtils->onNetdMessage(mCommand);
    }
    return NS_OK;
  }

private:
  nsAutoPtr<NetdCommand> mCommand;
};

class NetdMessageConsumer : public NetdConsumer
{
public:
  NetdMessageConsumer()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void MessageReceived(NetdCommand* aCommand)
  {
    MOZ_ASSERT(!NS_IsMainThread());

    nsCOMPtr<nsIRunnable> runnable = new NetdEventRunnable(aCommand);
    if (gWorkerThread) {
      gWorkerThread->Dispatch(runnable, nsIEventTarget::DISPATCH_NORMAL);
    }
  }
};

NS_IMPL_ISUPPORTS(NetworkWorker, nsINetworkWorker)

NetworkWorker::NetworkWorker()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gNetworkWorker);
}

NetworkWorker::~NetworkWorker()
{
  MOZ_ASSERT(!gNetworkWorker);
  MOZ_ASSERT(!mListener);
}

already_AddRefed<NetworkWorker>
NetworkWorker::FactoryCreate()
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!gNetworkWorker) {
    gNetworkWorker = new NetworkWorker();
    ClearOnShutdown(&gNetworkWorker);

    gNetworkUtils = new NetworkUtils(NetworkWorker::NotifyResult);
    ClearOnShutdown(&gNetworkUtils);
  }

  nsRefPtr<NetworkWorker> worker = gNetworkWorker.get();
  return worker.forget();
}

NS_IMETHODIMP
NetworkWorker::Start(nsINetworkEventListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);

  if (mListener) {
    return NS_OK;
  }

  nsresult rv;

  rv = NS_NewNamedThread("NetworkWorker", getter_AddRefs(gWorkerThread));
  if (NS_FAILED(rv)) {
    NS_WARNING("Can't create network control thread");
    return NS_ERROR_FAILURE;
  }

  StartNetd(new NetdMessageConsumer());
  mListener = aListener;

  return NS_OK;
}

NS_IMETHODIMP
NetworkWorker::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mListener) {
    return NS_OK;
  }

  StopNetd();

  gWorkerThread->Shutdown();
  gWorkerThread = nullptr;

  mListener = nullptr;
  return NS_OK;
}

// Receive command from main thread (NetworkService.js).
NS_IMETHODIMP
NetworkWorker::PostMessage(JS::Handle<JS::Value> aOptions, JSContext* aCx)
{
  MOZ_ASSERT(NS_IsMainThread());

  NetworkCommandOptions options;
  if (!options.Init(aCx, aOptions)) {
    NS_WARNING("Bad dictionary passed to NetworkWorker::SendCommand");
    return NS_ERROR_FAILURE;
  }

  NetworkParams NetworkParams(options);

  if (NetworkParams.mIsBlocking) {
    NetworkWorker::HandleBlockingCommand(NetworkParams);
    return NS_OK;
  }

  // Dispatch the command to the control thread.
  nsCOMPtr<nsIRunnable> runnable = new NetworkCommandDispatcher(NetworkParams);
  if (gWorkerThread) {
    gWorkerThread->Dispatch(runnable, nsIEventTarget::DISPATCH_NORMAL);
  }
  return NS_OK;
}

void
NetworkWorker::HandleBlockingCommand(NetworkParams& aOptions)
{
  if (aOptions.mCmd.EqualsLiteral("runDhcp")) {
    NetworkWorker::RunDhcp(aOptions);
  }
}

void
NetworkWorker::RunDhcp(NetworkParams& aOptions)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIRunnable> runnable = new RunDhcpEvent(aOptions);
  nsCOMPtr<nsIThread> thread;
  NS_NewThread(getter_AddRefs(thread), runnable);
}

void
NetworkWorker::DispatchNetworkResult(const NetworkResultOptions& aOptions)
{
  MOZ_ASSERT(NS_IsMainThread());

  mozilla::AutoSafeJSContext cx;
  JS::RootedValue val(cx);

  if (!ToJSValue(cx, aOptions, &val)) {
    return;
  }

  // Call the listener with a JS value.
  if (mListener) {
    mListener->OnEvent(val);
  }
}

// Callback function from network worker thread to update result on main thread.
void
NetworkWorker::NotifyResult(NetworkResultOptions& aResult)
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIRunnable> runnable = new NetworkResultDispatcher(aResult);
  NS_DispatchToMainThread(runnable);
}

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(NetworkWorker,
                                         NetworkWorker::FactoryCreate)

NS_DEFINE_NAMED_CID(NS_NETWORKWORKER_CID);

static const mozilla::Module::CIDEntry kNetworkWorkerCIDs[] = {
  { &kNS_NETWORKWORKER_CID, false, nullptr, NetworkWorkerConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kNetworkWorkerContracts[] = {
  { "@mozilla.org/network/worker;1", &kNS_NETWORKWORKER_CID },
  { nullptr }
};

static const mozilla::Module kNetworkWorkerModule = {
  mozilla::Module::kVersion,
  kNetworkWorkerCIDs,
  kNetworkWorkerContracts,
  nullptr
};

} // namespace mozilla

NSMODULE_DEFN(NetworkWorkerModule) = &kNetworkWorkerModule;
