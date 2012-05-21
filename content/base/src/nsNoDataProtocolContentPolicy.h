/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Content policy implementation that prevents all loads of images,
 * subframes, etc from documents loaded as data (eg documents loaded
 * via XMLHttpRequest).
 */

#ifndef nsNoDataProtocolContentPolicy_h__
#define nsNoDataProtocolContentPolicy_h__

/* ac9e3e82-bfbd-4f26-941e-f58c8ee178c1 */
#define NS_NODATAPROTOCOLCONTENTPOLICY_CID \
 {0xac9e3e82, 0xbfbd, 0x4f26, {0x94, 0x1e, 0xf5, 0x8c, 0x8e, 0xe1, 0x78, 0xc1}}
#define NS_NODATAPROTOCOLCONTENTPOLICY_CONTRACTID \
 "@mozilla.org/no-data-protocol-content-policy;1"


#include "nsIContentPolicy.h"

class nsNoDataProtocolContentPolicy : public nsIContentPolicy
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPOLICY

  nsNoDataProtocolContentPolicy()
  {}
  ~nsNoDataProtocolContentPolicy()
  {}
};


#endif /* nsNoDataProtocolContentPolicy_h__ */
