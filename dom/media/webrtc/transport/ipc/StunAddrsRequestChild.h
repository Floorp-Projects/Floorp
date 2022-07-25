/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_StunAddrsRequestChild_h
#define mozilla_net_StunAddrsRequestChild_h

#include "mozilla/net/PStunAddrsRequestChild.h"

class nsISerialEventTarget;

namespace mozilla::net {

class StunAddrsListener {
 public:
  virtual void OnMDNSQueryComplete(const nsCString& hostname,
                                   const Maybe<nsCString>& address) = 0;
  virtual void OnStunAddrsAvailable(const NrIceStunAddrArray& addrs) = 0;

  NS_IMETHOD_(MozExternalRefCountType) AddRef();
  NS_IMETHOD_(MozExternalRefCountType) Release();

 protected:
  virtual ~StunAddrsListener() = default;

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

class StunAddrsRequestChild final : public PStunAddrsRequestChild {
  friend class PStunAddrsRequestChild;

 public:
  explicit StunAddrsRequestChild(StunAddrsListener* listener);

  NS_IMETHOD_(MozExternalRefCountType) AddRef();
  NS_IMETHOD_(MozExternalRefCountType) Release();

  // Not sure why AddIPDLReference & ReleaseIPDLReference don't come
  // from PStunAddrsRequestChild since the IPC plumbing seem to
  // expect this.
  void AddIPDLReference() { AddRef(); }
  void ReleaseIPDLReference() { Release(); }

  void Cancel();

 protected:
  virtual ~StunAddrsRequestChild() = default;

  virtual mozilla::ipc::IPCResult RecvOnMDNSQueryComplete(
      const nsACString& aHostname, const Maybe<nsCString>& aAddress) override;

  virtual mozilla::ipc::IPCResult RecvOnStunAddrsAvailable(
      const NrIceStunAddrArray& addrs) override;

  RefPtr<StunAddrsListener> mListener;

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

}  // namespace mozilla::net

#endif  // mozilla_net_StunAddrsRequestChild_h
