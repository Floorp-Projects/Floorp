/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ISurfaceAllocator.h"

namespace mozilla {
namespace layers {

NS_IMPL_ISUPPORTS(GfxMemoryImageReporter, nsIMemoryReporter)

mozilla::Atomic<ptrdiff_t> GfxMemoryImageReporter::sAmount(0);

mozilla::ipc::SharedMemory::SharedMemoryType OptimalShmemType()
{
  return ipc::SharedMemory::SharedMemoryType::TYPE_BASIC;
}

} // namespace layers
} // namespace mozilla
