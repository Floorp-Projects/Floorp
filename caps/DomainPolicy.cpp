/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DomainPolicy.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/Unused.h"
#include "nsScriptSecurityManager.h"

namespace mozilla {

using namespace ipc;
using namespace dom;

NS_IMPL_ISUPPORTS(DomainPolicy, nsIDomainPolicy)

static nsresult BroadcastDomainSetChange(DomainSetType aSetType,
                                         DomainSetChangeType aChangeType,
                                         nsIURI* aDomain = nullptr) {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "DomainPolicy should only be exposed to the chrome process.");

  nsTArray<ContentParent*> parents;
  ContentParent::GetAll(parents);
  if (!parents.Length()) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < parents.Length(); i++) {
    Unused << parents[i]->SendDomainSetChanged(aSetType, aChangeType, aDomain);
  }
  return NS_OK;
}

DomainPolicy::DomainPolicy()
    : mBlocklist(new DomainSet(BLOCKLIST)),
      mSuperBlocklist(new DomainSet(SUPER_BLOCKLIST)),
      mAllowlist(new DomainSet(ALLOWLIST)),
      mSuperAllowlist(new DomainSet(SUPER_ALLOWLIST)) {
  if (XRE_IsParentProcess()) {
    BroadcastDomainSetChange(NO_TYPE, ACTIVATE_POLICY);
  }
}

DomainPolicy::~DomainPolicy() {
  // The SSM holds a strong ref to the DomainPolicy until Deactivate() is
  // invoked, so we should never hit the destructor until that happens.
  MOZ_ASSERT(!mBlocklist && !mSuperBlocklist && !mAllowlist &&
             !mSuperAllowlist);
}

NS_IMETHODIMP
DomainPolicy::GetBlocklist(nsIDomainSet** aSet) {
  nsCOMPtr<nsIDomainSet> set = mBlocklist.get();
  set.forget(aSet);
  return NS_OK;
}

NS_IMETHODIMP
DomainPolicy::GetSuperBlocklist(nsIDomainSet** aSet) {
  nsCOMPtr<nsIDomainSet> set = mSuperBlocklist.get();
  set.forget(aSet);
  return NS_OK;
}

NS_IMETHODIMP
DomainPolicy::GetAllowlist(nsIDomainSet** aSet) {
  nsCOMPtr<nsIDomainSet> set = mAllowlist.get();
  set.forget(aSet);
  return NS_OK;
}

NS_IMETHODIMP
DomainPolicy::GetSuperAllowlist(nsIDomainSet** aSet) {
  nsCOMPtr<nsIDomainSet> set = mSuperAllowlist.get();
  set.forget(aSet);
  return NS_OK;
}

NS_IMETHODIMP
DomainPolicy::Deactivate() {
  // Clear the hashtables first to free up memory, since script might
  // hold the doomed sets alive indefinitely.
  mBlocklist->Clear();
  mSuperBlocklist->Clear();
  mAllowlist->Clear();
  mSuperAllowlist->Clear();

  // Null them out.
  mBlocklist = nullptr;
  mSuperBlocklist = nullptr;
  mAllowlist = nullptr;
  mSuperAllowlist = nullptr;

  // Inform the SSM.
  nsScriptSecurityManager* ssm =
      nsScriptSecurityManager::GetScriptSecurityManager();
  if (ssm) {
    ssm->DeactivateDomainPolicy();
  }
  if (XRE_IsParentProcess()) {
    BroadcastDomainSetChange(NO_TYPE, DEACTIVATE_POLICY);
  }
  return NS_OK;
}

void DomainPolicy::CloneDomainPolicy(DomainPolicyClone* aClone) {
  aClone->active() = true;
  mBlocklist->CloneSet(&aClone->blocklist());
  mSuperBlocklist->CloneSet(&aClone->superBlocklist());
  mAllowlist->CloneSet(&aClone->allowlist());
  mSuperAllowlist->CloneSet(&aClone->superAllowlist());
}

static void CopyURIs(const nsTArray<RefPtr<nsIURI>>& aDomains,
                     nsIDomainSet* aSet) {
  for (uint32_t i = 0; i < aDomains.Length(); i++) {
    if (NS_WARN_IF(!aDomains[i])) {
      continue;
    }
    aSet->Add(aDomains[i]);
  }
}

void DomainPolicy::ApplyClone(const DomainPolicyClone* aClone) {
  CopyURIs(aClone->blocklist(), mBlocklist);
  CopyURIs(aClone->allowlist(), mAllowlist);
  CopyURIs(aClone->superBlocklist(), mSuperBlocklist);
  CopyURIs(aClone->superAllowlist(), mSuperAllowlist);
}

static already_AddRefed<nsIURI> GetCanonicalClone(nsIURI* aURI) {
  nsCOMPtr<nsIURI> clone;
  nsresult rv = NS_MutateURI(aURI)
                    .SetUserPass(EmptyCString())
                    .SetPathQueryRef(EmptyCString())
                    .Finalize(clone);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return clone.forget();
}

NS_IMPL_ISUPPORTS(DomainSet, nsIDomainSet)

NS_IMETHODIMP
DomainSet::Add(nsIURI* aDomain) {
  nsCOMPtr<nsIURI> clone = GetCanonicalClone(aDomain);
  NS_ENSURE_TRUE(clone, NS_ERROR_FAILURE);
  mHashTable.PutEntry(clone);
  if (XRE_IsParentProcess()) {
    return BroadcastDomainSetChange(mType, ADD_DOMAIN, aDomain);
  }

  return NS_OK;
}

NS_IMETHODIMP
DomainSet::Remove(nsIURI* aDomain) {
  nsCOMPtr<nsIURI> clone = GetCanonicalClone(aDomain);
  NS_ENSURE_TRUE(clone, NS_ERROR_FAILURE);
  mHashTable.RemoveEntry(clone);
  if (XRE_IsParentProcess()) {
    return BroadcastDomainSetChange(mType, REMOVE_DOMAIN, aDomain);
  }

  return NS_OK;
}

NS_IMETHODIMP
DomainSet::Clear() {
  mHashTable.Clear();
  if (XRE_IsParentProcess()) {
    return BroadcastDomainSetChange(mType, CLEAR_DOMAINS);
  }

  return NS_OK;
}

NS_IMETHODIMP
DomainSet::Contains(nsIURI* aDomain, bool* aContains) {
  *aContains = false;
  nsCOMPtr<nsIURI> clone = GetCanonicalClone(aDomain);
  NS_ENSURE_TRUE(clone, NS_ERROR_FAILURE);
  *aContains = mHashTable.Contains(clone);
  return NS_OK;
}

NS_IMETHODIMP
DomainSet::ContainsSuperDomain(nsIURI* aDomain, bool* aContains) {
  *aContains = false;
  nsCOMPtr<nsIURI> clone = GetCanonicalClone(aDomain);
  NS_ENSURE_TRUE(clone, NS_ERROR_FAILURE);
  nsAutoCString domain;
  nsresult rv = clone->GetHost(domain);
  NS_ENSURE_SUCCESS(rv, rv);
  while (true) {
    // Check the current domain.
    if (mHashTable.Contains(clone)) {
      *aContains = true;
      return NS_OK;
    }

    // Chop off everything before the first dot, or break if there are no
    // dots left.
    int32_t index = domain.Find(".");
    if (index == kNotFound) break;
    domain.Assign(Substring(domain, index + 1));
    rv = NS_MutateURI(clone).SetHost(domain).Finalize(clone);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // No match.
  return NS_OK;
}

void DomainSet::CloneSet(nsTArray<RefPtr<nsIURI>>* aDomains) {
  for (auto iter = mHashTable.Iter(); !iter.Done(); iter.Next()) {
    nsIURI* key = iter.Get()->GetKey();
    aDomains->AppendElement(key);
  }
}

} /* namespace mozilla */
