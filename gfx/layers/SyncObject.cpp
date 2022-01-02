/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SyncObject.h"

#ifdef XP_WIN
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "mozilla/layers/TextureD3D11.h"
#endif
#include "nsIXULRuntime.h"  // for BrowserTabsRemoteAutostart

namespace mozilla {
namespace layers {

#ifdef XP_WIN
/* static */
already_AddRefed<SyncObjectHost> SyncObjectHost::CreateSyncObjectHost(
    ID3D11Device* aDevice) {
  return MakeAndAddRef<SyncObjectD3D11Host>(aDevice);
}

/* static */
already_AddRefed<SyncObjectClient> SyncObjectClient::CreateSyncObjectClient(
    SyncHandle aHandle, ID3D11Device* aDevice) {
  if (!aHandle) {
    return nullptr;
  }

  return MakeAndAddRef<SyncObjectD3D11Client>(aHandle, aDevice);
}
#endif

already_AddRefed<SyncObjectClient>
SyncObjectClient::CreateSyncObjectClientForContentDevice(SyncHandle aHandle) {
#ifndef XP_WIN
  return nullptr;
#else
  if (!aHandle) {
    return nullptr;
  }

  return MakeAndAddRef<SyncObjectD3D11ClientContentDevice>(aHandle);
#endif
}

}  // namespace layers
}  // namespace mozilla
