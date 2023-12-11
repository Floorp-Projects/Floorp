/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ThreadLocal.h"
#include "mozilla/dom/quota/StringifyUtils.h"
#include "nsTHashSet.h"

namespace mozilla {

MOZ_THREAD_LOCAL(nsTHashSet<Stringifyable*>*)
Stringifyable::sActiveStringifyableInstances;

#ifdef DEBUG
Atomic<bool> sStringifyableTLSInitialized(false);
#endif

void Stringifyable::Stringify(nsACString& aData) {
  if (IsActive()) {
    aData.Append("(...)"_ns);
    return;
  }

  SetActive(true);
  aData.Append(kStringifyStartInstance);
  DoStringify(aData);
  aData.Append(kStringifyEndInstance);
  SetActive(false);
}

/* static */ void Stringifyable::InitTLS() {
  if (sActiveStringifyableInstances.init()) {
#ifdef DEBUG
    sStringifyableTLSInitialized = true;
#endif
  }
}

bool Stringifyable::IsActive() {
  MOZ_ASSERT(sStringifyableTLSInitialized);
  auto* set = sActiveStringifyableInstances.get();
  return set && set->Contains(this);
}

void Stringifyable::SetActive(bool aIsActive) {
  MOZ_ASSERT(sStringifyableTLSInitialized);
  auto* myset = sActiveStringifyableInstances.get();
  if (!myset && aIsActive) {
    myset = new nsTHashSet<Stringifyable*>();
    sActiveStringifyableInstances.set(myset);
  }
  MOZ_ASSERT(myset);
  if (aIsActive) {
    myset->Insert(this);
  } else {
    myset->Remove(this);
    if (myset->IsEmpty()) {
      sActiveStringifyableInstances.set(nullptr);
      delete myset;
    }
  }
}

}  // namespace mozilla
