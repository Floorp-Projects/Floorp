/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GamepadTestChannelChild.h"

namespace mozilla {
namespace dom {

void
GamepadTestChannelChild::AddPromise(const uint32_t& aID, Promise* aPromise)
{
  MOZ_ASSERT(!mPromiseList.Get(aID, nullptr));
  mPromiseList.Put(aID, aPromise);
}

mozilla::ipc::IPCResult
GamepadTestChannelChild::RecvReplyGamepadIndex(const uint32_t& aID,
                                               const uint32_t& aIndex)
{
  RefPtr<Promise> p;
  if (!mPromiseList.Get(aID, getter_AddRefs(p))) {
    MOZ_CRASH("We should always have a promise.");
  }

  p->MaybeResolve(aIndex);
  mPromiseList.Remove(aID);
  return IPC_OK();
}

} // namespace dom
} // namespace mozilla
