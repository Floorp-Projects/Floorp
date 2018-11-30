/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalStorageCommon.h"

namespace mozilla {
namespace dom {

namespace {

Atomic<int32_t> gNextGenLocalStorageEnabled(-1);

}  // namespace

const char16_t* kLocalStorageType = u"localStorage";

bool NextGenLocalStorageEnabled() {
  MOZ_ASSERT(NS_IsMainThread());

  if (gNextGenLocalStorageEnabled == -1) {
    bool enabled = Preferences::GetBool("dom.storage.next_gen", false);
    gNextGenLocalStorageEnabled = enabled ? 1 : 0;
  }

  return !!gNextGenLocalStorageEnabled;
}

bool CachedNextGenLocalStorageEnabled() {
  MOZ_ASSERT(gNextGenLocalStorageEnabled != -1);

  return !!gNextGenLocalStorageEnabled;
}

}  // namespace dom
}  // namespace mozilla
