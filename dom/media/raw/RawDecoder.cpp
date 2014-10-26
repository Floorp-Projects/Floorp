/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDecoderStateMachine.h"
#include "RawReader.h"
#include "RawDecoder.h"

namespace mozilla {

MediaDecoderStateMachine* RawDecoder::CreateStateMachine()
{
  return new MediaDecoderStateMachine(this, new RawReader(this), true);
}

} // namespace mozilla

