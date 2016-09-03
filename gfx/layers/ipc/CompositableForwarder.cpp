/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositableForwarder.h"
#include "mozilla/layers/CompositableChild.h"

namespace mozilla {
namespace layers {

void
CompositableForwarder::Destroy(CompositableChild* aCompositable)
{
  AssertInForwarderThread();

  if (!aCompositable->CanSend()) {
    return;
  }

  if (!DestroyInTransaction(aCompositable, false)) {
    aCompositable->SendDestroy();
  }
}

} // namespace layers
} // namespace mozilla
