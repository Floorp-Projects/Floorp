/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=4:expandtab:shiftwidth=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_nsFxrCommandLineHandler_h_
#define GFX_VR_nsFxrCommandLineHandler_h_

#include "nsICommandLineHandler.h"

// 5baca10a-4d53-4335-b24d-c69696640a9a
#define NS_FXRCOMMANDLINEHANDLER_CID                 \
  {                                                  \
    0x5baca10a, 0x4d53, 0x4335, {                    \
      0xb2, 0x4d, 0xc6, 0x96, 0x96, 0x64, 0x0a, 0x9a \
    }                                                \
  }

// nsFxrCommandLineHandler is responsible for handling parameters used to
// bootstrap Firefox Reality running on desktop-class machines.
class nsFxrCommandLineHandler : public nsICommandLineHandler {
 public:
  nsFxrCommandLineHandler() = default;
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOMMANDLINEHANDLER

 protected:
  virtual ~nsFxrCommandLineHandler() = default;
};

#endif /* !defined(GFX_VR_nsFxrCommandLineHandler_h_) */
