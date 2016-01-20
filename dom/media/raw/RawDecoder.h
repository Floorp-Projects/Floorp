/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(RawDecoder_h_)
#define RawDecoder_h_

#include "MediaDecoder.h"

namespace mozilla {

class RawDecoder : public MediaDecoder
{
public:
  explicit RawDecoder(MediaDecoderOwner* aOwner) : MediaDecoder(aOwner) {}
  MediaDecoder* Clone(MediaDecoderOwner* aOwner) override {
    if (!IsRawEnabled()) {
      return nullptr;
    }
    return new RawDecoder(aOwner);
  }
  MediaDecoderStateMachine* CreateStateMachine() override;
};

} // namespace mozilla

#endif
