/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/LoaderAPIInterfaces.h"

#include "freestanding/CheckForCaller.h"
#include "freestanding/LoaderPrivateAPI.h"

namespace mozilla {

extern "C" MOZ_EXPORT nt::LoaderAPI* GetNtLoaderAPI(
    nt::LoaderObserver* aNewObserver) {
  const bool isCallerMozglue =
      CheckForAddress(RETURN_ADDRESS(), L"mozglue.dll");
  MOZ_ASSERT(isCallerMozglue);
  if (!isCallerMozglue) {
    return nullptr;
  }

  freestanding::EnsureInitialized();
  freestanding::LoaderPrivateAPI& api = freestanding::gLoaderPrivateAPI;
  api.SetObserver(aNewObserver);

  return &api;
}

}  // namespace mozilla
