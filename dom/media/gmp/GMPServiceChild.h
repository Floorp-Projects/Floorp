/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPServiceChild_h_
#define GMPServiceChild_h_

#include "GMPService.h"
#include "base/process.h"
#include "mozilla/ipc/Transport.h"
#include "mozilla/gmp/PGMPServiceChild.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/dom/ContentChild.h"

namespace mozilla {
namespace gmp {

class GMPContentParent;
class GMPServiceChild;

class GetServiceChildCallback
{
public:
  GetServiceChildCallback()
  {
    MOZ_COUNT_CTOR(GetServiceChildCallback);
  }
  virtual ~GetServiceChildCallback()
  {
    MOZ_COUNT_DTOR(GetServiceChildCallback);
  }
  virtual void Done(GMPServiceChild* aGMPServiceChild) = 0;
};

class GeckoMediaPluginServiceChild : public GeckoMediaPluginService
{
  friend class GMPServiceChild;

public:
  static already_AddRefed<GeckoMediaPluginServiceChild> GetSingleton();

  NS_IMETHOD GetPluginVersionForAPI(const nsACString& aAPI,
                                    nsTArray<nsCString>* aTags,
                                    bool* aHasPlugin,
                                    nsACString& aOutVersion) override;
  NS_IMETHOD GetNodeId(const nsAString& aOrigin,
                       const nsAString& aTopLevelOrigin,
                       const nsAString& aGMPName,
                       bool aInPrivateBrowsingMode,
                       UniquePtr<GetNodeIdCallback>&& aCallback) override;

  NS_DECL_NSIOBSERVER

  void SetServiceChild(UniquePtr<GMPServiceChild>&& aServiceChild);

  void RemoveGMPContentParent(GMPContentParent* aGMPContentParent);

  static void UpdateGMPCapabilities(nsTArray<mozilla::dom::GMPCapabilityData>&& aCapabilities);

protected:
  void InitializePlugins(AbstractThread*) override
  {
    // Nothing to do here.
  }
  bool GetContentParentFrom(GMPCrashHelper* aHelper,
                            const nsACString& aNodeId,
                            const nsCString& aAPI,
                            const nsTArray<nsCString>& aTags,
                            UniquePtr<GetGMPContentParentCallback>&& aCallback)
    override;

private:
  friend class OpenPGMPServiceChild;

  void GetServiceChild(UniquePtr<GetServiceChildCallback>&& aCallback);

  UniquePtr<GMPServiceChild> mServiceChild;
  nsTArray<UniquePtr<GetServiceChildCallback>> mGetServiceChildCallbacks;
};

class GMPServiceChild : public PGMPServiceChild
{
public:
  explicit GMPServiceChild();
  virtual ~GMPServiceChild();

  PGMPContentParent* AllocPGMPContentParent(Transport* aTransport,
                                            ProcessId aOtherPid) override;

  void GetBridgedGMPContentParent(ProcessId aOtherPid,
                                  GMPContentParent** aGMPContentParent);
  void RemoveGMPContentParent(GMPContentParent* aGMPContentParent);

  void GetAlreadyBridgedTo(nsTArray<ProcessId>& aAlreadyBridgedTo);

  static PGMPServiceChild* Create(Transport* aTransport, ProcessId aOtherPid);

private:
  nsRefPtrHashtable<nsUint64HashKey, GMPContentParent> mContentParents;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPServiceChild_h_
