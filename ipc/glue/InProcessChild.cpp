/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/InProcessChild.h"
#include "mozilla/dom/WindowGlobalChild.h"

using namespace mozilla::dom;

namespace mozilla {
namespace ipc {

PWindowGlobalChild*
InProcessChild::AllocPWindowGlobalChild(const WindowGlobalInit& aInit)
{
  MOZ_ASSERT_UNREACHABLE("PWindowGlobalChild should not be created manually");
  return nullptr;
}

bool
InProcessChild::DeallocPWindowGlobalChild(PWindowGlobalChild* aActor)
{
  // Free IPC-held reference
  static_cast<WindowGlobalChild*>(aActor)->Release();
  return true;
}

} // namespace ipc
} // namespace mozilla
