/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SyncObject.h"

#ifdef XP_WIN
#include "mozilla/layers/TextureD3D11.h"
#endif

namespace mozilla {
namespace layers {

already_AddRefed<SyncObjectHost>
SyncObjectHost::CreateSyncObjectHost(
#ifdef XP_WIN
                                     ID3D11Device* aDevice
#endif
                                    )
{
#ifdef XP_WIN
  return MakeAndAddRef<SyncObjectD3D11Host>(aDevice);
#else
  return nullptr;
#endif
}

already_AddRefed<SyncObjectClient>
SyncObjectClient::CreateSyncObjectClient(SyncHandle aHandle
#ifdef XP_WIN
                                         , ID3D11Device* aDevice
#endif
                                        )
{
  if (!aHandle) {
    return nullptr;
  }

#ifdef XP_WIN
  return MakeAndAddRef<SyncObjectD3D11Client>(aHandle, aDevice);
#else
  MOZ_ASSERT_UNREACHABLE();
  return nullptr;
#endif
}

} // namespace layers
} // namespace mozilla
