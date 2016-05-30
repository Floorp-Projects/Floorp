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

  size_t nbytes = nativeHandle->numInts * sizeof(int);
  aMsg->WriteSize(nbytes);
  aMsg->WriteBytes((nativeHandle->data + nativeHandle->numFds), nbytes);

  for (size_t i = 0; i < static_cast<size_t>(nativeHandle->numFds); ++i) {
    aMsg->WriteFileDescriptor(base::FileDescriptor(nativeHandle->data[i], true));
  }
}

bool
ParamTraits<GonkNativeHandle>::Read(const Message* aMsg,
                               PickleIterator* aIter, paramType* aResult)
{
  size_t nbytes;
  const char* data;
  if (!aMsg->ReadSize(aIter, &nbytes) ||
      !aMsg->ReadBytes(aIter, &data, nbytes)) {
    return false;
  }

  if (nbytes % sizeof(int) != 0) {
    return false;
  }

  size_t numInts = nbytes / sizeof(int);
  size_t numFds = aMsg->num_fds();
  native_handle* nativeHandle = native_handle_create(numFds, numInts);
  if (!nativeHandle) {
    return false;
  }

  memcpy(nativeHandle->data + nativeHandle->numFds, data, nbytes);

  for (size_t i = 0; i < static_cast<size_t>(nativeHandle->numFds); ++i) {
    base::FileDescriptor fd;
    if (!aMsg->ReadFileDescriptor(aIter, &fd)) {
      return false;
    }
    nativeHandle->data[i] = fd.fd;
  }

  GonkNativeHandle handle(new GonkNativeHandle::NhObj(nativeHandle));
  handle.TransferToAnother(*aResult);

  return true;
}

} // namespace IPC
