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
	nsresult RunDriver();
    nsresult GetDatabase();
    nsresult GetPath(nsFileSpec& aPathName);

protected:
    nsIMsgDatabase   *m_newsDB;
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

nsresult newsTestDriver::RunDriver()
{
    nsresult rv = NS_OK;

    rv = GetDatabase();

    if (NS_SUCCEEDED(rv) && m_newsDB) {
        nsIMessage		*newMsgHdr = nsnull;

        m_newsDB->CreateNewHdr(1, &newMsgHdr);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("m_newsDB->CreateNewHdr() failed\n");
#endif
            return rv;
        }

        newMsgHdr->SetSubject("hello, world");
        newMsgHdr->SetFlags(MSG_FLAG_READ);
        newMsgHdr->SetAuthor("zelig@allen.com");

        rv = m_newsDB->AddNewHdrToDB(newMsgHdr, PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("m_newsDB->AddNewHdrToDB() failed\n");
#endif
            return rv;           
        }
        
        newMsgHdr->Release();
        newMsgHdr = nsnull;

        m_newsDB->Close(PR_TRUE);
    }

    NS_RELEASE(m_newsDB);
    m_newsDB = nsnull;
    
    return rv;
}

nsresult newsTestDriver::GetPath(nsFileSpec& aPathName)
{
    /* turn news://news.mozilla.org/netscape.public.mozilla.unix 
       into /tmp/mozillanews/news.mozilla.org/netscape.public.mozilla.unix 
    */
  aPathName = "/tmp/mozillanews/nnn";
  return NS_OK;
}

nsresult newsTestDriver::GetDatabase()
{
    if (m_newsDB == nsnull) {
        nsNativeFileSpec path;
		nsresult rv = GetPath(path);
		if (NS_FAILED(rv)) return rv;

        nsresult newsDBOpen = NS_OK;
        nsIMsgDatabase *newsDBFactory = nsnull;

        rv = nsComponentManager::CreateInstance(kCNewsDB, nsnull, nsIMsgDatabase::GetIID(), (void **) &newsDBFactory);
        if (NS_SUCCEEDED(rv) && newsDBFactory) {
                newsDBOpen = newsDBFactory->Open(path, PR_TRUE, (nsIMsgDatabase **) &m_newsDB, PR_FALSE);
#ifdef DEBUG
                if (NS_SUCCEEDED(newsDBOpen)) {
                    printf ("newsDBFactory->Open() succeeded\n");
                }
                else {
                    printf ("newsDBFactory->Open() failed\n");
                }
#endif
                NS_RELEASE(newsDBFactory);
                newsDBFactory = nsnull;
                return rv;
        }
#ifdef DEBUG
        else {
            printf("nsComponentManager::CreateInstance(kCNewsDB,...) failed\n");
        }
#endif
    }
    
    return NS_OK;
}

int main()
{
    newsTestDriver * driver = new newsTestDriver();
	if (driver)
	{
		driver->RunDriver();
		// when it kicks out...it is done....so delete it...
		delete driver;
        driver = nsnull;
	}
	// shut down:
    return 0;
}
