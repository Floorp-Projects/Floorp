/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BenchmarkStorageChild.h"
#include "mozilla/dom/ContentChild.h"

namespace mozilla {

static PBenchmarkStorageChild* sChild = nullptr;

/* static */
PBenchmarkStorageChild* BenchmarkStorageChild::Instance() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!sChild) {
    sChild = new BenchmarkStorageChild();
    PContentChild* contentChild = dom::ContentChild::GetSingleton();
    MOZ_ASSERT(contentChild);
    if (!contentChild->SendPBenchmarkStorageConstructor()) {
      MOZ_CRASH("SendPBenchmarkStorageConstructor failed");
    }
  }
  MOZ_ASSERT(sChild);
  return sChild;
}

BenchmarkStorageChild::~BenchmarkStorageChild() {
  MOZ_ASSERT(NS_IsMainThread());
  if (sChild == this) {
    sChild = nullptr;
  }
}

}  // namespace mozilla
