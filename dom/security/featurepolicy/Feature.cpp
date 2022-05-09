/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Feature.h"
#include "mozilla/BasePrincipal.h"

namespace mozilla::dom {

void Feature::GetAllowList(nsTArray<nsCOMPtr<nsIPrincipal>>& aList) const {
  MOZ_ASSERT(mPolicy == eAllowList);
  aList.AppendElements(mAllowList);
}

bool Feature::Allows(nsIPrincipal* aPrincipal) const {
  if (mPolicy == eNone) {
    return false;
  }

  if (mPolicy == eAll) {
    return true;
  }

  return AllowListContains(aPrincipal);
}

Feature::Feature(const nsAString& aFeatureName)
    : mFeatureName(aFeatureName), mPolicy(eAllowList) {}

Feature::~Feature() = default;

const nsAString& Feature::Name() const { return mFeatureName; }

void Feature::SetAllowsNone() {
  mPolicy = eNone;
  mAllowList.Clear();
}

bool Feature::AllowsNone() const { return mPolicy == eNone; }

void Feature::SetAllowsAll() {
  mPolicy = eAll;
  mAllowList.Clear();
}

bool Feature::AllowsAll() const { return mPolicy == eAll; }

void Feature::AppendToAllowList(nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aPrincipal);

  mPolicy = eAllowList;
  mAllowList.AppendElement(aPrincipal);
}

bool Feature::AllowListContains(nsIPrincipal* aPrincipal) const {
  MOZ_ASSERT(aPrincipal);

  if (!HasAllowList()) {
    return false;
  }

  for (nsIPrincipal* principal : mAllowList) {
    if (BasePrincipal::Cast(principal)->Subsumes(
            aPrincipal, BasePrincipal::ConsiderDocumentDomain)) {
      return true;
    }
  }

  return false;
}

bool Feature::HasAllowList() const { return mPolicy == eAllowList; }

}  // namespace mozilla::dom
