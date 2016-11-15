/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/dom/PGamepadEventChannelChild.h"

#ifndef mozilla_dom_GamepadEventChannelChild_h_
#define mozilla_dom_GamepadEventChannelChild_h_

namespace mozilla{
namespace dom{

class GamepadEventChannelChild final : public PGamepadEventChannelChild
{
 public:
  GamepadEventChannelChild() {}
  ~GamepadEventChannelChild() {}
  virtual mozilla::ipc::IPCResult
  RecvGamepadUpdate(const GamepadChangeEvent& aGamepadEvent) override;
};

}// namespace dom
}// namespace mozilla

#endif
