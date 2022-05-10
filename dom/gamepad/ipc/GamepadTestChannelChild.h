/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GamepadTestChannelChild_h_
#define mozilla_dom_GamepadTestChannelChild_h_

#include "mozilla/dom/PGamepadTestChannelChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/WeakPtr.h"
#include "nsRefPtrHashtable.h"

namespace mozilla::dom {

class GamepadServiceTest;

class GamepadTestChannelChild final : public PGamepadTestChannelChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GamepadTestChannelChild)

  static already_AddRefed<GamepadTestChannelChild> Create(
      GamepadServiceTest* aGamepadServiceTest);

  GamepadTestChannelChild(const GamepadTestChannelChild&) = delete;
  GamepadTestChannelChild(GamepadTestChannelChild&&) = delete;
  GamepadTestChannelChild& operator=(const GamepadTestChannelChild&) = delete;
  GamepadTestChannelChild& operator=(GamepadTestChannelChild&&) = delete;

 private:
  explicit GamepadTestChannelChild(GamepadServiceTest* aGamepadServiceTest);
  ~GamepadTestChannelChild() = default;

  mozilla::ipc::IPCResult RecvReplyGamepadHandle(const uint32_t& aID,
                                                 const GamepadHandle& aHandle);

  WeakPtr<GamepadServiceTest> mGamepadServiceTest;

  friend class PGamepadTestChannelChild;
};

}  // namespace mozilla::dom

#endif
