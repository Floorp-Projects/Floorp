/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <unistd.h>

#include "GonkNativeHandle.h"

using namespace mozilla::layers;

namespace mozilla {
namespace layers {

GonkNativeHandle::GonkNativeHandle()
  : mNhObj(new NhObj())
{
}

GonkNativeHandle::GonkNativeHandle(NhObj* aNhObj)
  : mNhObj(aNhObj)
{
  MOZ_ASSERT(aNhObj);
}


void
GonkNativeHandle::TransferToAnother(GonkNativeHandle& aHandle)
{
  aHandle.mNhObj = this->GetAndResetNhObj();
}

already_AddRefed<GonkNativeHandle::NhObj>
GonkNativeHandle::GetAndResetNhObj()
{
  RefPtr<NhObj> nhObj = mNhObj;
  mNhObj = new NhObj();
  return nhObj.forget();
}

already_AddRefed<GonkNativeHandle::NhObj>
GonkNativeHandle::GetDupNhObj()
{
  if (!IsValid()) {
    return GonkNativeHandle::CreateDupNhObj(nullptr);
  }
  return GonkNativeHandle::CreateDupNhObj(mNhObj->mHandle);
}

/* static */ already_AddRefed<GonkNativeHandle::NhObj>
GonkNativeHandle::CreateDupNhObj(native_handle_t* aHandle)
{
  RefPtr<NhObj> nhObj;
  if (aHandle) {
    native_handle* nativeHandle =
      native_handle_create(aHandle->numFds, aHandle->numInts);
    if (!nativeHandle) {
      nhObj = new GonkNativeHandle::NhObj();
      return nhObj.forget();
    }

    for (int i = 0; i < aHandle->numFds; ++i) {
      nativeHandle->data[i] = dup(aHandle->data[i]);
    }

    memcpy(nativeHandle->data + nativeHandle->numFds,
           aHandle->data + aHandle->numFds,
           sizeof(int) * aHandle->numInts);

    nhObj = new GonkNativeHandle::NhObj(nativeHandle);
  } else {
    nhObj = new GonkNativeHandle::NhObj();
  }
  return nhObj.forget();
}

} // namespace layers
} // namespace mozilla
