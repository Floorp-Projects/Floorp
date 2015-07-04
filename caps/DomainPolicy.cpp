/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DomainPolicy.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/unused.h"
#include "nsIMessageManager.h"
#include "nsScriptSecurityManager.h"

namespace mozilla {

using namespace ipc;
using namespace dom;

NS_IMPL_ISUPPORTS(DomainPolicy, nsIDomainPolicy)

static nsresult
BroadcastDomainSetChange(DomainSetType aSetType, DomainSetChangeType aChangeType,
                         nsIURI* aDomain = nullptr)
{
    MOZ_ASSERT(XRE_IsParentProcess(),
               "DomainPolicy should only be exposed to the chrome process.");

    nsTArray<ContentParent*> parents;
    ContentParent::GetAll(parents);
    if (!parents.Length()) {
       return NS_OK;
    }

    OptionalURIParams uri;
    SerializeURI(aDomain, uri);

    for (uint32_t i = 0; i < parents.Length(); i++) {
        unused << parents[i]->SendDomainSetChanged(aSetType, aChangeType, uri);
    }
    return NS_OK;
}

DomainPolicy::DomainPolicy() : mBlacklist(new DomainSet(BLACKLIST))
                             , mSuperBlacklist(new DomainSet(SUPER_BLACKLIST))
                             , mWhitelist(new DomainSet(WHITELIST))
                             , mSuperWhitelist(new DomainSet(SUPER_WHITELIST))
{
    if (XRE_IsParentProcess()) {
        BroadcastDomainSetChange(NO_TYPE, ACTIVATE_POLICY);
    }
}

DomainPolicy::~DomainPolicy()
{
    // The SSM holds a strong ref to the DomainPolicy until Deactivate() is
    // invoked, so we should never hit the destructor until that happens.
    MOZ_ASSERT(!mBlacklist && !mSuperBlacklist &&
               !mWhitelist && !mSuperWhitelist);
}


NS_IMETHODIMP
DomainPolicy::GetBlacklist(nsIDomainSet** aSet)
{
    nsCOMPtr<nsIDomainSet> set = mBlacklist;
    set.forget(aSet);
    return NS_OK;
}

NS_IMETHODIMP
DomainPolicy::GetSuperBlacklist(nsIDomainSet** aSet)
{
    nsCOMPtr<nsIDomainSet> set = mSuperBlacklist;
    set.forget(aSet);
    return NS_OK;
}

NS_IMETHODIMP
DomainPolicy::GetWhitelist(nsIDomainSet** aSet)
{
    nsCOMPtr<nsIDomainSet> set = mWhitelist;
    set.forget(aSet);
    return NS_OK;
}

NS_IMETHODIMP
DomainPolicy::GetSuperWhitelist(nsIDomainSet** aSet)
{
    nsCOMPtr<nsIDomainSet> set = mSuperWhitelist;
    set.forget(aSet);
    return NS_OK;
}

NS_IMETHODIMP
DomainPolicy::Deactivate()
{
    // Clear the hashtables first to free up memory, since script might
    // hold the doomed sets alive indefinitely.
    mBlacklist->Clear();
    mSuperBlacklist->Clear();
    mWhitelist->Clear();
    mSuperWhitelist->Clear();

    // Null them out.
    mBlacklist = nullptr;
    mSuperBlacklist = nullptr;
    mWhitelist = nullptr;
    mSuperWhitelist = nullptr;

    // Inform the SSM.
    nsScriptSecurityManager* ssm = nsScriptSecurityManager::GetScriptSecurityManager();
    if (ssm) {
        ssm->DeactivateDomainPolicy();
    }
    if (XRE_IsParentProcess()) {
        BroadcastDomainSetChange(NO_TYPE, DEACTIVATE_POLICY);
    }
    return NS_OK;
}

void
DomainPolicy::CloneDomainPolicy(DomainPolicyClone* aClone)
{
    aClone->active() = true;
    static_cast<DomainSet*>(mBlacklist.get())->CloneSet(&aClone->blacklist());
    static_cast<DomainSet*>(mSuperBlacklist.get())->CloneSet(&aClone->superBlacklist());
    static_cast<DomainSet*>(mWhitelist.get())->CloneSet(&aClone->whitelist());
    static_cast<DomainSet*>(mSuperWhitelist.get())->CloneSet(&aClone->superWhitelist());
}

static
void
CopyURIs(const InfallibleTArray<URIParams>& aDomains, nsIDomainSet* aSet)
{
    for (uint32_t i = 0; i < aDomains.Length(); i++) {
        nsCOMPtr<nsIURI> uri = DeserializeURI(aDomains[i]);
        aSet->Add(uri);
    }
}

void
DomainPolicy::ApplyClone(DomainPolicyClone* aClone)
{
    nsCOMPtr<nsIDomainSet> list;

    CopyURIs(aClone->blacklist(), mBlacklist);
    CopyURIs(aClone->whitelist(), mWhitelist);
    CopyURIs(aClone->superBlacklist(), mSuperBlacklist);
    CopyURIs(aClone->superWhitelist(), mSuperWhitelist);
}

static already_AddRefed<nsIURI>
GetCanonicalClone(nsIURI* aURI)
{
    nsCOMPtr<nsIURI> clone;
    nsresult rv = aURI->Clone(getter_AddRefs(clone));
    NS_ENSURE_SUCCESS(rv, nullptr);
    rv = clone->SetUserPass(EmptyCString());
    NS_ENSURE_SUCCESS(rv, nullptr);
    rv = clone->SetPath(EmptyCString());
    NS_ENSURE_SUCCESS(rv, nullptr);
    return clone.forget();
}

NS_IMPL_ISUPPORTS(DomainSet, nsIDomainSet)

NS_IMETHODIMP
DomainSet::Add(nsIURI* aDomain)
{
    nsCOMPtr<nsIURI> clone = GetCanonicalClone(aDomain);
    NS_ENSURE_TRUE(clone, NS_ERROR_FAILURE);
    mHashTable.PutEntry(clone);
    if (XRE_IsParentProcess())
        return BroadcastDomainSetChange(mType, ADD_DOMAIN, aDomain);

    return NS_OK;
}

NS_IMETHODIMP
DomainSet::Remove(nsIURI* aDomain)
{
    nsCOMPtr<nsIURI> clone = GetCanonicalClone(aDomain);
    NS_ENSURE_TRUE(clone, NS_ERROR_FAILURE);
    mHashTable.RemoveEntry(clone);
    if (XRE_IsParentProcess())
        return BroadcastDomainSetChange(mType, REMOVE_DOMAIN, aDomain);

    return NS_OK;
}

NS_IMETHODIMP
DomainSet::Clear()
{
    mHashTable.Clear();
    if (XRE_IsParentProcess())
        return BroadcastDomainSetChange(mType, CLEAR_DOMAINS);

    return NS_OK;
}

NS_IMETHODIMP
DomainSet::Contains(nsIURI* aDomain, bool* aContains)
{
    *aContains = false;
    nsCOMPtr<nsIURI> clone = GetCanonicalClone(aDomain);
    NS_ENSURE_TRUE(clone, NS_ERROR_FAILURE);
    *aContains = mHashTable.Contains(clone);
    return NS_OK;
}

NS_IMETHODIMP
DomainSet::ContainsSuperDomain(nsIURI* aDomain, bool* aContains)
{
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
        if (index == kNotFound)
            break;
        domain.Assign(Substring(domain, index + 1));
        rv = clone->SetHost(domain);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // No match.
    return NS_OK;

}

NS_IMETHODIMP
DomainSet::GetType(uint32_t* aType)
{
    *aType = mType;
    return NS_OK;
}

static
PLDHashOperator
DomainEnumerator(nsURIHashKey* aEntry, void* aUserArg)
{
    InfallibleTArray<URIParams>* uris = static_cast<InfallibleTArray<URIParams>*>(aUserArg);
    nsIURI* key = aEntry->GetKey();

    URIParams uri;
    SerializeURI(key, uri);

    uris->AppendElement(uri);
    return PL_DHASH_NEXT;
}

void
DomainSet::CloneSet(InfallibleTArray<URIParams>* aDomains)
{
    mHashTable.EnumerateEntries(DomainEnumerator, aDomains);
}

} /* namespace mozilla */
