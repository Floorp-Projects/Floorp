/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemQuotaClientFactory.h"

#include "FileSystemQuotaClient.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla::dom::fs {

namespace {

StaticRefPtr<FileSystemQuotaClientFactory> gCustomFactory;

}  // namespace

// static
void FileSystemQuotaClientFactory::SetCustomFactory(
    RefPtr<FileSystemQuotaClientFactory> aCustomFactory) {
  gCustomFactory = std::move(aCustomFactory);
}

// static
already_AddRefed<quota::Client>
FileSystemQuotaClientFactory::CreateQuotaClient() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  if (gCustomFactory) {
    return gCustomFactory->AllocQuotaClient();
  }

  auto factory = MakeRefPtr<FileSystemQuotaClientFactory>();

  return factory->AllocQuotaClient();
}

already_AddRefed<quota::Client>
FileSystemQuotaClientFactory::AllocQuotaClient() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  RefPtr<FileSystemQuotaClient> result = new FileSystemQuotaClient();
  return result.forget();
}

}  // namespace mozilla::dom::fs
