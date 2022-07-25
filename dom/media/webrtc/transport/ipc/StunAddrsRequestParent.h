/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_StunAddrsRequestParent_h
#define mozilla_net_StunAddrsRequestParent_h

#include "mozilla/net/PStunAddrsRequestParent.h"

struct MDNSService;

namespace mozilla::net {

class StunAddrsRequestParent : public PStunAddrsRequestParent {
  friend class PStunAddrsRequestParent;

 public:
  StunAddrsRequestParent();

  NS_IMETHOD_(MozExternalRefCountType) AddRef();
  NS_IMETHOD_(MozExternalRefCountType) Release();

  mozilla::ipc::IPCResult Recv__delete__() override;

  void OnQueryComplete(const nsACString& hostname,
                       const Maybe<nsCString>& address);

 protected:
  virtual ~StunAddrsRequestParent();

  virtual mozilla::ipc::IPCResult RecvGetStunAddrs() override;
  virtual mozilla::ipc::IPCResult RecvRegisterMDNSHostname(
      const nsACString& hostname, const nsACString& address) override;
  virtual mozilla::ipc::IPCResult RecvQueryMDNSHostname(
      const nsACString& hostname) override;
  virtual mozilla::ipc::IPCResult RecvUnregisterMDNSHostname(
      const nsACString& hostname) override;
  virtual void ActorDestroy(ActorDestroyReason why) override;

  nsCOMPtr<nsIThread> mMainThread;
  nsCOMPtr<nsISerialEventTarget> mSTSThread;

  void GetStunAddrs_s();
  void SendStunAddrs_m(const NrIceStunAddrArray& addrs);

  void OnQueryComplete_m(const nsACString& hostname,
                         const Maybe<nsCString>& address);

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

 private:
  bool mIPCClosed;  // true if IPDL channel has been closed (child crash)

  class MDNSServiceWrapper {
   public:
    explicit MDNSServiceWrapper(const std::string& ifaddr);
    void RegisterHostname(const char* hostname, const char* address);
    void QueryHostname(void* data, const char* hostname);
    void UnregisterHostname(const char* hostname);

    NS_IMETHOD_(MozExternalRefCountType) AddRef();
    NS_IMETHOD_(MozExternalRefCountType) Release();

   protected:
    ThreadSafeAutoRefCnt mRefCnt;
    NS_DECL_OWNINGTHREAD

   private:
    virtual ~MDNSServiceWrapper();
    void StartIfRequired();

    std::string ifaddr;
    MDNSService* mMDNSService = nullptr;
  };

  static StaticRefPtr<MDNSServiceWrapper> mSharedMDNSService;
};

}  // namespace mozilla::net

#endif  // mozilla_net_StunAddrsRequestParent_h
