/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPC_GonkNativeHandle_h
#define IPC_GonkNativeHandle_h

#include "mozilla/RefPtr.h"             // for RefPtr
#include "nsISupportsImpl.h"

namespace mozilla {
namespace layers {

struct GonkNativeHandle {
  bool operator==(const GonkNativeHandle&) const { return false; }
};

} // namespace layers
} // namespace mozilla

#endif // IPC_GonkNativeHandle_h
