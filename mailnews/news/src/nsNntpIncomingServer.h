/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 */

#ifndef __nsNntpIncomingServer_h
#define __nsNntpIncomingServer_h

#include "nsINntpIncomingServer.h"
#include "nscore.h"

#include "nsMsgIncomingServer.h"

#include "nsIPref.h"

#include "prmem.h"
#include "plstr.h"
#include "prprf.h"

#include "nsEnumeratorUtils.h" 

/* get some implementation from nsMsgIncomingServer */
class nsNntpIncomingServer : public nsMsgIncomingServer,
                             public nsINntpIncomingServer
							 
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSINNTPINCOMINGSERVER

    nsNntpIncomingServer();
    virtual ~nsNntpIncomingServer();
    
    NS_IMETHOD GetLocalStoreType(char * *type);
    NS_IMETHOD CloseCachedConnections();
    NS_IMETHOD PerformExpand();
	NS_IMETHOD PerformBiff();
	NS_IMETHOD GetServerRequiresPasswordForBiff(PRBool *_retval);

private:
    PRBool mNewsrcHasChanged;
	nsAdapterEnumerator *mGroupsEnumerator;
};

#endif
