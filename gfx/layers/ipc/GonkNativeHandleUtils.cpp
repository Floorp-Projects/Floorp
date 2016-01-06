/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkNativeHandleUtils.h"

using namespace mozilla::layers;

namespace IPC {

void
ParamTraits<GonkNativeHandle>::Write(Message* aMsg,
                                const paramType& aParam)
{
  GonkNativeHandle handle = aParam;
  MOZ_ASSERT(handle.IsValid());

  RefPtr<GonkNativeHandle::NhObj> nhObj = handle.GetAndResetNhObj();
  native_handle_t* nativeHandle = nhObj->GetAndResetNativeHandle();

  aMsg->WriteSize(nativeHandle->numInts);
  aMsg->WriteBytes((nativeHandle->data + nativeHandle->numFds), sizeof(int) * nativeHandle->numInts);

  for (size_t i = 0; i < static_cast<size_t>(nativeHandle->numFds); ++i) {
    aMsg->WriteFileDescriptor(base::FileDescriptor(nativeHandle->data[i], true));
  }
}

bool
ParamTraits<GonkNativeHandle>::Read(const Message* aMsg,
                               void** aIter, paramType* aResult)
{
  size_t numInts;
  if (!aMsg->ReadSize(aIter, &numInts)) {
    return false;
  }
  numInts /= sizeof(int);
  size_t numFds = aMsg->num_fds();
  native_handle* nativeHandle = native_handle_create(numFds, numInts);

  const char* data = reinterpret_cast<const char*>(nativeHandle->data + nativeHandle->numFds);
  if (!aMsg->ReadBytes(aIter, &data, numInts * sizeof(int))) {
    return false;
  }

  for (size_t i = 0; i < static_cast<size_t>(nativeHandle->numFds); ++i) {
    base::FileDescriptor fd;
    if (!aMsg->ReadFileDescriptor(aIter, &fd)) {
      return false;
    }
    nativeHandle->data[i] = fd.fd;
  }

  return true;
}

} // namespace IPC
