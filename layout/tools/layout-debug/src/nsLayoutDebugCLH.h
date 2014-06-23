/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:tabstop=4:expandtab:shiftwidth=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLayoutDebugCLH_h_
#define nsLayoutDebugCLH_h_

#include "nsICommandLineHandler.h"
#define ICOMMANDLINEHANDLER nsICommandLineHandler

#define NS_LAYOUTDEBUGCLH_CID \
 { 0xa8f52633, 0x5ecf, 0x424a, \
   { 0xa1, 0x47, 0x47, 0xc3, 0x22, 0xf7, 0xbc, 0xe2 }}

class nsLayoutDebugCLH : public ICOMMANDLINEHANDLER
{
public:
    nsLayoutDebugCLH();

    NS_DECL_ISUPPORTS
    NS_DECL_NSICOMMANDLINEHANDLER

protected:
    virtual ~nsLayoutDebugCLH();
};

#endif /* !defined(nsLayoutDebugCLH_h_) */
