/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
*/

#ifndef _NSPSMUICALLBACKS_H
#define _NSPSMUICALLBACKS_H

#include "prtypes.h"
#include "cmtcmn.h"

#include "nsIPSMUIHandler.h"
#include "nsIDOMWindow.h"

PRStatus InitPSMUICallbacks(PCMT_CONTROL gControl);
PRStatus InitPSMEventLoop(PCMT_CONTROL gControl);
PRStatus DisplayPSMUIDialog(PCMT_CONTROL control, const char* pickledStatus, const char *hostName, nsIDOMWindow * window);


#define NS_PSMUIHANDLER_CID {0x15944e30, 0x601e, 0x11d3, {0x8c, 0x4a, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74}}

class nsPSMUIHandlerImpl : public nsIPSMUIHandler 
{
    public:

        NS_DEFINE_STATIC_CID_ACCESSOR( NS_PSMUIHANDLER_CID );
    
        /* ctor/dtor */
        nsPSMUIHandlerImpl() { NS_INIT_REFCNT(); }
        virtual ~nsPSMUIHandlerImpl() { }

        NS_DECL_ISUPPORTS
        NS_DECL_NSIPSMUIHANDLER

        static NS_METHOD CreatePSMUIHandler(nsISupports* aOuter, REFNSIID aIID, void **aResult);
};

#endif
