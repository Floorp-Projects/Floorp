/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedDIB.h"

namespace mozilla {
namespace gfx {

SharedDIB::SharedDIB() : mShMem(nullptr) {}

SharedDIB::~SharedDIB() { Close(); }

nsresult SharedDIB::Create(uint32_t aSize) {
  Close();

  mShMem = new base::SharedMemory();
  if (!mShMem || !mShMem->Create(aSize)) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

bool SharedDIB::IsValid() {
  if (!mShMem) return false;

  return base::SharedMemory::IsHandleValid(mShMem->handle());
}

nsresult SharedDIB::Close() {
  delete mShMem;

  mShMem = nullptr;

  return NS_OK;
}

nsresult SharedDIB::Attach(Handle aHandle, uint32_t aSize) {
  Close();

  mShMem = new base::SharedMemory(aHandle, false);
  if (!mShMem) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

nsresult SharedDIB::ShareToProcess(base::ProcessId aTargetPid,
                                   Handle* aNewHandle) {
  if (!mShMem) return NS_ERROR_UNEXPECTED;

  if (!mShMem->ShareToProcess(aTargetPid, aNewHandle))
    return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

}  // namespace gfx
}  // namespace mozilla
