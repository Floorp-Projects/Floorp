/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChannelPolicy_h___
#define nsChannelPolicy_h___

#include "nsCOMPtr.h"
#include "nsIChannelPolicy.h"

#define NSCHANNELPOLICY_CONTRACTID "@mozilla.org/nschannelpolicy;1"
#define NSCHANNELPOLICY_CID \
{ 0xd396b3cd, 0xf164, 0x4ce8, \
  { 0x93, 0xa7, 0xe3, 0x85, 0xe1, 0x46, 0x56, 0x3c } }

class nsChannelPolicy : public nsIChannelPolicy
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICHANNELPOLICY

    nsChannelPolicy();
    virtual ~nsChannelPolicy();

protected:
    /* Represents the type of content being loaded in the channel per
     * nsIContentPolicy, e.g. TYPE_IMAGE, TYPE_SCRIPT
     */
    unsigned long mLoadType;

    /* pointer to a Content Security Policy object if available */
    nsCOMPtr<nsISupports> mCSP;
};

#endif /* nsChannelPolicy_h___ */
