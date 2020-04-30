/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLCrossProcessCommandQueue.h"
#include "WebGLMethodDispatcher.h"

namespace mozilla {

#if 0
bool HostWebGLCommandSink<Consumer, Queue>::DispatchCommand(size_t command) {
  if (!mHostContext) {
    return false;
  }

  return WebGLMethodDispatcher::DispatchCommand(command, *this, *mHostContext);
}
#endif

}  // namespace mozilla
