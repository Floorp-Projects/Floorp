/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NetworkWorker.h"
#include "NetworkUtils.h"
#include <nsThreadUtils.h>
#include "mozilla/ModuleUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsXULAppAPI.h"
#include "nsCxPusher.h"

#define NS_NETWORKWORKER_CID \
  { 0x6df093e1, 0x8127, 0x4fa7, {0x90, 0x13, 0xa3, 0xaa, 0xa7, 0x79, 0xbb, 0xdd} }

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla {

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
    if (gNetworkUtils) {
      gNetworkUtils->ExecuteCommand(mParams);
    }
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
  { }

  NS_IMETHOD Run()
  {
    if (gNetworkUtils) {
      gNetworkUtils->onNetdMessage(mCommand);
    }
    return NS_OK;
  }

private:
  nsAutoPtr<NetdCommand> mCommand;
};

NS_IMPL_ISUPPORTS1(NetworkWorker, nsINetworkWorker)

NetworkWorker::NetworkWorker()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gNetworkWorker);
}

NetworkWorker::~NetworkWorker()
{
  MOZ_ASSERT(!gNetworkWorker);
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

  nsresult rv;

  if (gNetworkWorker) {
    StartNetd(gNetworkWorker);
  }

  rv = NS_NewThread(getter_AddRefs(mWorkerThread));
  if (NS_FAILED(rv)) {
    NS_WARNING("Can't create network control thread");
    Shutdown();
    return NS_ERROR_FAILURE;
  }

  mListener = aListener;

  return NS_OK;
}

NS_IMETHODIMP
NetworkWorker::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  StopNetd();

  if (mWorkerThread) {
    mWorkerThread->Shutdown();
    mWorkerThread = nullptr;
  }
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

  // Dispatch the command to the control thread.
  NetworkParams NetworkParams(options);
  nsCOMPtr<nsIRunnable> runnable = new NetworkCommandDispatcher(NetworkParams);
  if (mWorkerThread) {
    mWorkerThread->Dispatch(runnable, nsIEventTarget::DISPATCH_NORMAL);
  }
  return NS_OK;
}

void
NetworkWorker::DispatchNetworkResult(const NetworkResultOptions& aOptions)
{
  MOZ_ASSERT(NS_IsMainThread());

  mozilla::AutoSafeJSContext cx;
  JS::RootedValue val(cx);

  if (!aOptions.ToObject(cx, JS::NullPtr(), &val)) {
    return;
  }

  // Call the listener with a JS value.
  if (mListener) {
    mListener->OnEvent(val);
  }
}

// Callback function from Netd, dispatch result to network worker thread.
void
NetworkWorker::MessageReceived(NetdCommand* aCommand)
{
  nsCOMPtr<nsIRunnable> runnable = new NetdEventRunnable(aCommand);
  if (mWorkerThread) {
    mWorkerThread->Dispatch(runnable, nsIEventTarget::DISPATCH_NORMAL);
  }
}

// Callback function from network worker thread to update result on main thread.
void
NetworkWorker::NotifyResult(NetworkResultOptions& aResult)
{
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
