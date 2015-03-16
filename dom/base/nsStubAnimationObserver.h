/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStubAnimationObserver_h_
#define nsStubAnimationObserver_h_

#include "nsIAnimationObserver.h"

class nsStubAnimationObserver : public nsIAnimationObserver {
public:
  NS_DECL_NSIANIMATIONOBSERVER
};

#endif // !defined(nsStubAnimationObserver_h_)
