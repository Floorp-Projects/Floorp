/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __AppleDecoder_h__
#define __AppleDecoder_h__

#include "MediaDecoder.h"

namespace mozilla {

class AppleDecoder : public MediaDecoder
{
public:
  AppleDecoder();

  virtual MediaDecoder* Clone() MOZ_OVERRIDE;
  virtual MediaDecoderStateMachine* CreateStateMachine() MOZ_OVERRIDE;

};

}

#endif // __AppleDecoder_h__
