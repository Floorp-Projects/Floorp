/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSValidatorUtils.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/Shmem.h"
#include "nsStringFwd.h"

using namespace mozilla::ipc;

namespace mozilla::dom {
/* static */
nsresult JSValidatorUtils::CopyCStringToShmem(IProtocol* aActor,
                                              const nsCString& aCString,
                                              Shmem& aMem) {
  if (!aActor->AllocShmem(aCString.Length(), &aMem)) {
    return NS_ERROR_FAILURE;
  }
  memcpy(aMem.get<char>(), aCString.BeginReading(), aCString.Length());
  return NS_OK;
}
}  // namespace mozilla::dom
