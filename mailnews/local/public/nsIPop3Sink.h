/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIPop3Sink_h__
#define nsIPop3Sink_h__

#include "nsISupports.h"
#include "nsIPop3IncomingServer.h"

/* starting interface nsIPop3Sink */

/* {EC54D490-BB00-11D2-AB5E-00805F8AC968} */
#define NS_IPOP3SINK_IID_STR "EC54D490-BB00-11D2-AB5E-00805F8AC968"
#define NS_IPOP3SINK_IID \
  {0xEC54D490, 0xBB00, 0x11D2, \
			{0xAB, 0x5E, 0x00, 0x80, 0x5F, 0x8A, 0xC9, 0x68}}

class nsIPop3Sink : public nsISupports 
{
public:
		static const nsIID& GetIID() {
				static nsIID iid = NS_IPOP3SINK_IID;
				return iid;
		}

    NS_IMETHOD SetUserAuthenticated(PRBool authed) = 0;
    NS_IMETHOD SetMailAccountURL(const char* urlString) = 0;
    NS_IMETHOD BeginMailDelivery(PRBool *aBool) = 0;
    NS_IMETHOD EndMailDelivery() = 0;
    NS_IMETHOD AbortMailDelivery() = 0;
    NS_IMETHOD IncorporateBegin(const char* uidlString, nsIURL* aURL, 
                                PRUint32 flags, void** closure) = 0;
    NS_IMETHOD IncorporateWrite(void* closure, const char* block, 
                                PRInt32 length) = 0;
    NS_IMETHOD IncorporateComplete(void* closure) = 0;
    NS_IMETHOD IncorporateAbort(void* closure, PRInt32 status) = 0;
    NS_IMETHOD BiffGetNewMail() = 0;
    NS_IMETHOD SetBiffStateAndUpdateFE(PRUint32 aBiffState) = 0;
    NS_IMETHOD SetSenderAuthedFlag(void* closure, PRBool authed) = 0;
    NS_IMETHOD SetPopServer(nsIPop3IncomingServer *server) = 0;
    NS_IMETHOD GetPopServer(nsIPop3IncomingServer* *server) = 0;
};

#endif /*  nsIPop3Sink_h__ */
