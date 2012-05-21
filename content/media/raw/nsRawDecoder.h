/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(nsRawDecoder_h_)
#define nsRawDecoder_h_

#include "nsBuiltinDecoder.h"

class nsRawDecoder : public nsBuiltinDecoder
{
public:
  virtual nsMediaDecoder* Clone() { 
    if (!nsHTMLMediaElement::IsRawEnabled()) {
      return nsnull;
    }    
    return new nsRawDecoder();
  }
  virtual nsDecoderStateMachine* CreateStateMachine();
};

#endif
