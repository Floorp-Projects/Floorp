/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsINNTPProtocol_h___
#define nsINNTPProtocol_h___

#include "nsISupports.h"
#include "nscore.h"

#include "nsIStreamListener.h"
#include "nsIURL.h"

#define NS_INNTPPROTOCOL_IID                   \
{/* {1940d682-1865-11d3-9deb-004005263078} */  \
 0x1940d682, 0x1865, 0x11d3, \
 {0x9d, 0xeb, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} \
}

class nsINNTPProtocol : public nsIStreamListener
{
public:
    // initialization function given a new url and transport layer
    NS_IMETHOD Initialize(nsIURL * aURL, nsITransport * transportLayer) = 0;
    
    // aConsumer is typically a display stream you may want the results to be displayed into...
    NS_IMETHOD LoadURL(nsIURL * aURL, nsISupports * aConsumer, PRInt32 * _retval) = 0;
    
    // returns true if we are currently running a url and false otherwise...
    NS_IMETHOD IsRunningUrl(PRBool * _retval) = 0;
};

#endif /* nsINNTPProtocol_h___ */
