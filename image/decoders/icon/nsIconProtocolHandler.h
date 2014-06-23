/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIconProtocolHandler_h___
#define nsIconProtocolHandler_h___

#include "nsWeakReference.h"
#include "nsIProtocolHandler.h"

class nsIconProtocolHandler : public nsIProtocolHandler, public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER

    // nsIconProtocolHandler methods:
    nsIconProtocolHandler();

protected:
    virtual ~nsIconProtocolHandler();
};

#endif /* nsIconProtocolHandler_h___ */
