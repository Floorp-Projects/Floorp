/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include <stdio.h>
#include <assert.h>

#ifdef XP_PC
#include <windows.h>
#endif

#include "plstr.h"
#include "plevent.h"
#include "prenv.h"

#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsITransport.h"
#include "nsIURL.h"
#include "nsINetService.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsParseMailbox.h"
#include "nsXPComCIID.h"

#include "nsNewsDatabase.h"
#include "nsMsgDBCID.h"
#include "nsCRT.h"

static NS_DEFINE_CID(kCNewsDB, NS_NEWSDB_CID);

class newsTestDriver
{
public:
	newsTestDriver();
	virtual ~newsTestDriver();
	nsresult RunDriver(nsFileSpec &folder); 

protected:
        nsIMessage      *m_newMsgHdr;		
        nsNewsDatabase  *m_newsDB;
        char            *m_newsgroupName;
};

newsTestDriver::newsTestDriver()
{
#ifdef DEBUG
    printf("in newsTestDriver::newsTestDriver()\n");
#endif
    m_newsDB = nsnull;
}

newsTestDriver::~newsTestDriver()
{
#ifdef DEBUG
    printf("in newsTestDriver::~newsTestDriver()\n");
#endif
}

nsresult newsTestDriver::RunDriver(nsFileSpec &folder)
{
	nsresult status = NS_OK;

	m_newsgroupName = nsCRT::strdup(folder);

#ifdef DEBUG
    printf("m_newsgroupName == %s\n", m_newsgroupName);
#endif
    
    nsresult rv = NS_OK;
    nsIMsgDatabase *newsDB = nsnull;
    rv = nsComponentManager::CreateInstance(kCNewsDB, nsnull, nsIMsgDatabase::GetIID(), (void **) &newsDB);
    if (NS_SUCCEEDED(rv) && newsDB)
    {
        rv = newsDB->Open(folder, PR_TRUE, (nsIMsgDatabase **) &m_newsDB, PR_FALSE);
        newsDB->Release();
    }
    else {
	printf("CreateInstance failed.  %d\n", rv);
    }
    
    return status;
}

int main()
{
    newsTestDriver * driver = new newsTestDriver();
	if (driver)
	{
        nsFileSpec foo("/tmp/mozillanews/foo");
		driver->RunDriver(foo);
		// when it kicks out...it is done....so delete it...
		delete driver;
	}
	// shut down:
    return 0;
}
