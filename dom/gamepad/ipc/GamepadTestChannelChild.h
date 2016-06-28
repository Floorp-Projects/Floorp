/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PGamepadTestChannelChild.h"
#include "mozilla/dom/Promise.h"

#ifndef mozilla_dom_GamepadTestChannelChild_h_
#define mozilla_dom_GamepadTestChannelChild_h_

namespace mozilla {
namespace dom {

class GamepadTestChannelChild final : public PGamepadTestChannelChild
{
 public:
  GamepadTestChannelChild() {}
  ~GamepadTestChannelChild() {}
  void AddPromise(const uint32_t& aID, Promise* aPromise);
 private:
  virtual bool RecvReplyGamepadIndex(const uint32_t& aID,
                                     const uint32_t& aIndex) override;
  nsRefPtrHashtable<nsUint32HashKey, Promise> mPromiseList;
};

}// namespace dom
}// namespace mozilla

#endif
