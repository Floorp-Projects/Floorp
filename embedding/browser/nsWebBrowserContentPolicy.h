/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIContentPolicy.h"

/* f66bc334-1dd1-11b2-bab2-90e04fe15c19 */
#define NS_WEBBROWSERCONTENTPOLICY_CID \
  { 0xf66bc334, 0x1dd1, 0x11b2, { 0xba, 0xb2, 0x90, 0xe0, 0x4f, 0xe1, 0x5c, 0x19 } }

#define NS_WEBBROWSERCONTENTPOLICY_CONTRACTID                                  \
  "@mozilla.org/embedding/browser/content-policy;1"

class nsWebBrowserContentPolicy : public nsIContentPolicy
{
protected:
  virtual ~nsWebBrowserContentPolicy();

public:
  nsWebBrowserContentPolicy();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPOLICY
};
