/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StunAddrsRequestParent.h"

#include "../runnable_utils.h"
#include "mozilla/StaticPtr.h"
#include "nsIThread.h"
#include "nsNetUtil.h"

#include "transport/nricectx.h"
#include "transport/nricemediastream.h"  // needed only for including nricectx.h
#include "transport/nricestunaddr.h"

#include "../mdns_service/mdns_service.h"

extern "C" {
#include "local_addr.h"
}

using namespace mozilla::ipc;

namespace mozilla::net {

static void mdns_service_resolved(void* cb, const char* hostname,
                                  const char* addr) {
  StunAddrsRequestParent* self = static_cast<StunAddrsRequestParent*>(cb);
  self->OnQueryComplete(nsCString(hostname), Some(nsCString(addr)));
}

void mdns_service_timedout(void* cb, const char* hostname) {
  StunAddrsRequestParent* self = static_cast<StunAddrsRequestParent*>(cb);
  self->OnQueryComplete(nsCString(hostname), Nothing());
}

StunAddrsRequestParent::StunAddrsRequestParent() : mIPCClosed(false) {
  NS_GetMainThread(getter_AddRefs(mMainThread));

  nsresult res;
  mSTSThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &res);
  MOZ_ASSERT(mSTSThread);
}

StunAddrsRequestParent::~StunAddrsRequestParent() {
  ASSERT_ON_THREAD(mMainThread);
}

mozilla::ipc::IPCResult StunAddrsRequestParent::RecvGetStunAddrs() {
  ASSERT_ON_THREAD(mMainThread);

  if (mIPCClosed) {
    return IPC_OK();
  }

  RUN_ON_THREAD(mSTSThread,
                WrapRunnable(RefPtr<StunAddrsRequestParent>(this),
                             &StunAddrsRequestParent::GetStunAddrs_s),
                NS_DISPATCH_NORMAL);

  return IPC_OK();
}

mozilla::ipc::IPCResult StunAddrsRequestParent::RecvRegisterMDNSHostname(
    const nsACString& aHostname, const nsACString& aAddress) {
  ASSERT_ON_THREAD(mMainThread);

  if (mIPCClosed) {
    return IPC_OK();
  }

  if (mSharedMDNSService) {
    mSharedMDNSService->RegisterHostname(aHostname.BeginReading(),
                                         aAddress.BeginReading());
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult StunAddrsRequestParent::RecvQueryMDNSHostname(
    const nsACString& aHostname) {
  ASSERT_ON_THREAD(mMainThread);

  if (mIPCClosed) {
    return IPC_OK();
  }

  if (mSharedMDNSService) {
    mSharedMDNSService->QueryHostname(this, aHostname.BeginReading());
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult StunAddrsRequestParent::RecvUnregisterMDNSHostname(
    const nsACString& aHostname) {
  ASSERT_ON_THREAD(mMainThread);

  if (mIPCClosed) {
    return IPC_OK();
  }

  if (mSharedMDNSService) {
    mSharedMDNSService->UnregisterHostname(aHostname.BeginReading());
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult StunAddrsRequestParent::Recv__delete__() {
  // see note below in ActorDestroy
  mIPCClosed = true;
  return IPC_OK();
}

void StunAddrsRequestParent::OnQueryComplete(const nsACString& hostname,
                                             const Maybe<nsCString>& address) {
  RUN_ON_THREAD(mMainThread,
                WrapRunnable(RefPtr<StunAddrsRequestParent>(this),
                             &StunAddrsRequestParent::OnQueryComplete_m,
                             nsCString(hostname), address),
                NS_DISPATCH_NORMAL);
}

void StunAddrsRequestParent::ActorDestroy(ActorDestroyReason why) {
  // We may still have refcount>0 if we haven't made it through
  // GetStunAddrs_s and SendStunAddrs_m yet, but child process
  // has crashed.  We must not send any more msgs to child, or
  // IPDL will kill chrome process, too.
  mIPCClosed = true;

  // We need to stop the mDNS service here to ensure that we don't
  // end up with any messages queued for the main thread after the
  // destructors run. Because of Bug 1569311, all of the
  // StunAddrsRequestParent instances end up being destroyed one
  // after the other, so it is ok to free the shared service when
  // the first one is destroyed rather than waiting for the last one.
  // If this behaviour changes, we would potentially end up starting
  // and stopping instances repeatedly and should add a refcount and
  // a way of cancelling pending queries to avoid churn in that case.
  if (mSharedMDNSService) {
    mSharedMDNSService = nullptr;
  }
}

void StunAddrsRequestParent::GetStunAddrs_s() {
  ASSERT_ON_THREAD(mSTSThread);

  // get the stun addresses while on STS thread
  NrIceStunAddrArray addrs = NrIceCtx::GetStunAddrs();

  if (mIPCClosed) {
    return;
  }

  // in order to return the result over IPC, we need to be on main thread
  RUN_ON_THREAD(
      mMainThread,
      WrapRunnable(RefPtr<StunAddrsRequestParent>(this),
                   &StunAddrsRequestParent::SendStunAddrs_m, std::move(addrs)),
      NS_DISPATCH_NORMAL);
}

void StunAddrsRequestParent::SendStunAddrs_m(const NrIceStunAddrArray& addrs) {
  ASSERT_ON_THREAD(mMainThread);

  if (mIPCClosed) {
    // nothing to do: child probably crashed
    return;
  }

  // This means that the mDNS service will continue running until shutdown
  // once started. The StunAddrsRequestParent destructor does not run until
  // shutdown anyway (see Bug 1569311), so there is not much we can do about
  // this here. One option would be to add a check if there are no hostnames
  // registered after UnregisterHostname is called, and if so, stop the mDNS
  // service at that time (see Bug 1569955.)
  if (!mSharedMDNSService) {
    std::ostringstream o;
    char buffer[16];
    for (auto& addr : addrs) {
      if (addr.localAddr().addr.ip_version == NR_IPV4 &&
          !nr_transport_addr_is_loopback(&addr.localAddr().addr)) {
        nr_transport_addr_get_addrstring(&addr.localAddr().addr, buffer, 16);
        o << buffer << ";";
      }
    }
    std::string addrstring = o.str();
    if (!addrstring.empty()) {
      mSharedMDNSService = new MDNSServiceWrapper(addrstring);
    }
  }

  // send the new addresses back to the child
  Unused << SendOnStunAddrsAvailable(addrs);
}

void StunAddrsRequestParent::OnQueryComplete_m(
    const nsACString& hostname, const Maybe<nsCString>& address) {
  ASSERT_ON_THREAD(mMainThread);

  if (mIPCClosed) {
    // nothing to do: child probably crashed
    return;
  }

  // send the hostname and address back to the child
  Unused << SendOnMDNSQueryComplete(hostname, address);
}

StaticRefPtr<StunAddrsRequestParent::MDNSServiceWrapper>
    StunAddrsRequestParent::mSharedMDNSService;

NS_IMPL_ADDREF(StunAddrsRequestParent)
NS_IMPL_RELEASE(StunAddrsRequestParent)

StunAddrsRequestParent::MDNSServiceWrapper::MDNSServiceWrapper(
    const std::string& ifaddr)
    : ifaddr(ifaddr) {}

void StunAddrsRequestParent::MDNSServiceWrapper::RegisterHostname(
    const char* hostname, const char* address) {
  StartIfRequired();
  if (mMDNSService) {
    mdns_service_register_hostname(mMDNSService, hostname, address);
  }
}

void StunAddrsRequestParent::MDNSServiceWrapper::QueryHostname(
    void* data, const char* hostname) {
  StartIfRequired();
  if (mMDNSService) {
    mdns_service_query_hostname(mMDNSService, data, mdns_service_resolved,
                                mdns_service_timedout, hostname);
  }
}

void StunAddrsRequestParent::MDNSServiceWrapper::UnregisterHostname(
    const char* hostname) {
  StartIfRequired();
  if (mMDNSService) {
    mdns_service_unregister_hostname(mMDNSService, hostname);
  }
}

StunAddrsRequestParent::MDNSServiceWrapper::~MDNSServiceWrapper() {
  if (mMDNSService) {
    mdns_service_stop(mMDNSService);
    mMDNSService = nullptr;
  }
}

void StunAddrsRequestParent::MDNSServiceWrapper::StartIfRequired() {
  if (!mMDNSService) {
    mMDNSService = mdns_service_start(ifaddr.c_str());
  }
}

NS_IMPL_ADDREF(StunAddrsRequestParent::MDNSServiceWrapper)
NS_IMPL_RELEASE(StunAddrsRequestParent::MDNSServiceWrapper)

}  // namespace mozilla::net
