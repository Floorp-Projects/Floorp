/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AnimationEffectReadOnly.h"
#include "mozilla/dom/AnimationEffectReadOnlyBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AnimationEffectReadOnly, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(AnimationEffectReadOnly)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AnimationEffectReadOnly)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AnimationEffectReadOnly)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

} // namespace dom
} // namespace mozilla
