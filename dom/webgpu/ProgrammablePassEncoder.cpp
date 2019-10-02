/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProgrammablePassEncoder.h"

namespace mozilla {
namespace webgpu {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ProgrammablePassEncoder)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ProgrammablePassEncoder)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(ProgrammablePassEncoder)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ProgrammablePassEncoder)

}  // namespace webgpu
}  // namespace mozilla
