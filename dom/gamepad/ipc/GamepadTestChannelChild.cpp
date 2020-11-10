/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GamepadTestChannelChild.h"

namespace mozilla::dom {

already_AddRefed<GamepadTestChannelChild> GamepadTestChannelChild::Create(
    GamepadServiceTest* aGamepadServiceTest) {
  return RefPtr<GamepadTestChannelChild>(
             new GamepadTestChannelChild(aGamepadServiceTest))
      .forget();
}

GamepadTestChannelChild::GamepadTestChannelChild(
    GamepadServiceTest* aGamepadServiceTest)
    : mGamepadServiceTest(aGamepadServiceTest) {}

mozilla::ipc::IPCResult GamepadTestChannelChild::RecvReplyGamepadIndex(
    const uint32_t& aID, const uint32_t& aIndex) {
  MOZ_RELEASE_ASSERT(
      mGamepadServiceTest,
      "Test channel should never outlive the owning GamepadServiceTest");

  mGamepadServiceTest->ReplyGamepadIndex(aID, aIndex);
  return IPC_OK();
}

}  // namespace mozilla::dom
