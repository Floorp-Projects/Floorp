/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DomainPolicy.h"
#include "nsScriptSecurityManager.h"

namespace mozilla {
namespace hotness {

NS_IMPL_ISUPPORTS1(DomainPolicy, nsIDomainPolicy)

DomainPolicy::DomainPolicy() : mBlacklist(new DomainSet())
                             , mSuperBlacklist(new DomainSet())
                             , mWhitelist(new DomainSet())
                             , mSuperWhitelist(new DomainSet())
{}

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
    nsScriptSecurityManager::GetScriptSecurityManager()->DeactivateDomainPolicy();
    return NS_OK;
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

NS_IMPL_ISUPPORTS1(DomainSet, nsIDomainSet)

NS_IMETHODIMP
DomainSet::Add(nsIURI* aDomain)
{
    nsCOMPtr<nsIURI> clone = GetCanonicalClone(aDomain);
    NS_ENSURE_TRUE(clone, NS_ERROR_FAILURE);
    mHashTable.PutEntry(clone);
    return NS_OK;
}

NS_IMETHODIMP
DomainSet::Remove(nsIURI* aDomain)
{
    nsCOMPtr<nsIURI> clone = GetCanonicalClone(aDomain);
    NS_ENSURE_TRUE(clone, NS_ERROR_FAILURE);
    mHashTable.RemoveEntry(clone);
    return NS_OK;
}

NS_IMETHODIMP
DomainSet::Clear()
{
    mHashTable.Clear();
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

} /* namespace hotness */
} /* namespace mozilla */
