/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/dom/PGamepadEventChannelChild.h"
#include "nsRefPtrHashtable.h"

#ifndef mozilla_dom_GamepadEventChannelChild_h_
#  define mozilla_dom_GamepadEventChannelChild_h_

namespace mozilla::dom {

class GamepadEventChannelChild final : public PGamepadEventChannelChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GamepadEventChannelChild, override)

  static already_AddRefed<GamepadEventChannelChild> Create();

  mozilla::ipc::IPCResult RecvGamepadUpdate(
      const GamepadChangeEvent& aGamepadEvent);
  mozilla::ipc::IPCResult RecvReplyGamepadPromise(const uint32_t& aPromiseID);
  void AddPromise(const uint32_t& aID, dom::Promise* aPromise);

  GamepadEventChannelChild(const GamepadEventChannelChild&) = delete;
  GamepadEventChannelChild(GamepadEventChannelChild&&) = delete;
  GamepadEventChannelChild& operator=(const GamepadEventChannelChild&) = delete;
  GamepadEventChannelChild& operator=(GamepadEventChannelChild&&) = delete;

 private:
  GamepadEventChannelChild() = default;
  ~GamepadEventChannelChild() = default;

  nsRefPtrHashtable<nsUint32HashKey, dom::Promise> mPromiseList;
};

}  // namespace mozilla::dom

#endif
