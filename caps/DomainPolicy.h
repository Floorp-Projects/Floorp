/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DomainPolicy_h__
#define DomainPolicy_h__

#include "nsIDomainPolicy.h"
#include "nsTHashtable.h"
#include "nsURIHashKey.h"

namespace mozilla {

enum DomainSetChangeType {
  ACTIVATE_POLICY,
  DEACTIVATE_POLICY,
  ADD_DOMAIN,
  REMOVE_DOMAIN,
  CLEAR_DOMAINS
};

enum DomainSetType {
  NO_TYPE,
  BLOCKLIST,
  SUPER_BLOCKLIST,
  ALLOWLIST,
  SUPER_ALLOWLIST
};

class DomainSet final : public nsIDomainSet {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMAINSET

  explicit DomainSet(DomainSetType aType) : mType(aType) {}

  void CloneSet(nsTArray<RefPtr<nsIURI>>* aDomains);

 protected:
  virtual ~DomainSet() {}
  nsTHashtable<nsURIHashKey> mHashTable;
  DomainSetType mType;
};

class DomainPolicy final : public nsIDomainPolicy {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMAINPOLICY
  DomainPolicy();

 private:
  virtual ~DomainPolicy();

  RefPtr<DomainSet> mBlocklist;
  RefPtr<DomainSet> mSuperBlocklist;
  RefPtr<DomainSet> mAllowlist;
  RefPtr<DomainSet> mSuperAllowlist;
};

} /* namespace mozilla */

#endif /* DomainPolicy_h__ */
