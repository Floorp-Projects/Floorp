/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=4:expandtab:shiftwidth=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLayoutDebugCLH_h_
#define nsLayoutDebugCLH_h_

#include "nsICommandLineHandler.h"
#define ICOMMANDLINEHANDLER nsICommandLineHandler

class nsLayoutDebugCLH : public ICOMMANDLINEHANDLER {
 public:
  nsLayoutDebugCLH();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICOMMANDLINEHANDLER

 protected:
  virtual ~nsLayoutDebugCLH();
};

#endif /* !defined(nsLayoutDebugCLH_h_) */
