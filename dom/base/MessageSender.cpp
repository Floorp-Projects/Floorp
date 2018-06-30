/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MessageSender.h"
#include "mozilla/dom/MessageBroadcaster.h"

namespace mozilla {
namespace dom {

void
MessageSender::InitWithCallback(ipc::MessageManagerCallback* aCallback)
{
  if (mCallback) {
    // Initialization should only happen once.
    return;
  }

  SetCallback(aCallback);

  // First load parent scripts by adding this to parent manager.
  if (mParentManager) {
    mParentManager->AddChildManager(this);
  }

  for (uint32_t i = 0; i < mPendingScripts.Length(); ++i) {
    LoadScript(mPendingScripts[i], false, mPendingScriptsGlobalStates[i], IgnoreErrors());
  }
}

} // namespace dom
} // namespace mozilla
