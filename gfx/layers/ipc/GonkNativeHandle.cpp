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
  RefPtr<NhObj> nhObj;
  if (IsValid()) {
    native_handle* nativeHandle =
      native_handle_create(mNhObj->mHandle->numFds, mNhObj->mHandle->numInts);

    for (int i = 0; i < mNhObj->mHandle->numFds; ++i) {
        nativeHandle->data[i] = dup(mNhObj->mHandle->data[i]);
    }

    memcpy(nativeHandle->data + nativeHandle->numFds,
           mNhObj->mHandle->data + mNhObj->mHandle->numFds,
           sizeof(int) * mNhObj->mHandle->numInts);

    nhObj = new GonkNativeHandle::NhObj(nativeHandle);
  } else {
    nhObj = new GonkNativeHandle::NhObj();
  }
  return nhObj.forget();
}

} // namespace layers
} // namespace mozilla
