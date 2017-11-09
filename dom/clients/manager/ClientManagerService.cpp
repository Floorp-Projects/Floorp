/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientManagerService.h"

#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::AssertIsOnBackgroundThread;

namespace {

ClientManagerService* sClientManagerServiceInstance = nullptr;

} // anonymous namespace

ClientManagerService::ClientManagerService()
{
  AssertIsOnBackgroundThread();
}

ClientManagerService::~ClientManagerService()
{
  AssertIsOnBackgroundThread();

  MOZ_DIAGNOSTIC_ASSERT(sClientManagerServiceInstance == this);
  sClientManagerServiceInstance = nullptr;
}

// static
already_AddRefed<ClientManagerService>
ClientManagerService::GetOrCreateInstance()
{
  AssertIsOnBackgroundThread();

  if (!sClientManagerServiceInstance) {
    sClientManagerServiceInstance = new ClientManagerService();
  }

  RefPtr<ClientManagerService> ref(sClientManagerServiceInstance);
  return ref.forget();
}

} // namespace dom
} // namespace mozilla
