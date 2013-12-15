/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DomainPolicy_h__
#define DomainPolicy_h__

#include "nsIDomainPolicy.h"
#include "nsTHashtable.h"
#include "nsURIHashKey.h"

namespace mozilla {

class DomainPolicy : public nsIDomainPolicy
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMAINPOLICY
    DomainPolicy();
    virtual ~DomainPolicy();

private:
    nsCOMPtr<nsIDomainSet> mBlacklist;
    nsCOMPtr<nsIDomainSet> mSuperBlacklist;
    nsCOMPtr<nsIDomainSet> mWhitelist;
    nsCOMPtr<nsIDomainSet> mSuperWhitelist;
};

class DomainSet : public nsIDomainSet
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMAINSET

    DomainSet() {}
    virtual ~DomainSet() {}

protected:
    nsTHashtable<nsURIHashKey> mHashTable;
};

} /* namespace mozilla */

#endif /* DomainPolicy_h__ */
