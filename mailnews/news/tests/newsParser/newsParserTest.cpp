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
#endif /* XP_PC */

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

#include "nsNewsDatabase.h"
#include "nsMsgDBCID.h"
#include "nsCRT.h"

#include "nsIPref.h"
#include "nsIFileLocator.h"

#include "nsNewsUtils.h"
#include "nsCOMPtr.h"

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL  "xpcom32.dll"
#define PREF_DLL   "xppref32.dll"
#define APPSHELL_DLL "nsappshell.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
#define PREF_DLL   "libpref"MOZ_DLL_SUFFIX
#define APPSHELL_DLL "libnsappshell"MOZ_DLL_SUFFIX
#endif /* XP_MAC */
#endif /* XP_PC */

static NS_DEFINE_CID(kCNewsDB, NS_NEWSDB_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kFileLocatorCID, NS_FILELOCATOR_CID);

class newsTestDriver
{
public:
	newsTestDriver();
	virtual ~newsTestDriver();
	nsresult RunDriver();
    nsresult GetDatabase(const char *uri);
    nsresult GetPath(const char *uri, nsFileSpec& aPathName);
    nsresult AddNewHeader(const char *subject, const char *author, const char *id, PRUint32 key);

protected:
    nsCOMPtr <nsIMsgDatabase> m_newsDB;
};

newsTestDriver::newsTestDriver()
{
#ifdef DEBUG_sspitzer
    printf("in newsTestDriver::newsTestDriver()\n");
#endif
    m_newsDB = nsnull;
}

newsTestDriver::~newsTestDriver()
{
#ifdef DEBUG_sspitzer
    printf("in newsTestDriver::~newsTestDriver()\n");
#endif
}

nsresult newsTestDriver::RunDriver()
{
    nsresult rv = NS_OK;

    rv = GetDatabase("news://news.mozilla.org/netscape.public.mozilla.mail-news");

    if (NS_SUCCEEDED(rv) && m_newsDB) {
        rv = AddNewHeader("praising with faint damnation","David McCusker <davidmc@netscape.com>","372A0615.F03C44C2@netscape.com", 0);
        if (NS_FAILED(rv)) {
            return rv;
        }

        rv = AddNewHeader("help running M5", "Steve Clark <buster@netscape.com>", "372A8676.59838C76@netscape.com", 1);

        if (NS_FAILED(rv)) {
            return rv;
        }
        
        /* closing the newsDB isn't enough.  something still has
         * reference to it.  (need to look into this, bienvenu 
         * suggests nsMsgFolderInfo?)
         * for now, we need to Commit() to force the changes
         * to the disk
         */
        m_newsDB->Commit(kSessionCommit);
        m_newsDB->Close(PR_TRUE);
    }

    return rv;
}

nsresult newsTestDriver::AddNewHeader(const char *subject, const char *author, const char *id, PRUint32 key)
{
    nsresult rv = NS_OK;

    nsCOMPtr <nsIMsgDBHdr> newMsgHdr;
    
    m_newsDB->CreateNewHdr(key, getter_AddRefs(newMsgHdr));
    if (NS_FAILED(rv)) {
#ifdef DEBUG_sspitzer
        printf("m_newsDB->CreateNewHdr() failed\n");
#endif
        return rv;
    }

    newMsgHdr->SetSubject(subject);
    newMsgHdr->SetFlags(MSG_FLAG_READ);
    newMsgHdr->SetAuthor(author);
    newMsgHdr->SetMessageId(id);
    
    rv = m_newsDB->AddNewHdrToDB(newMsgHdr, PR_TRUE);
    if (NS_FAILED(rv)) {
#ifdef DEBUG_sspitzer
        printf("m_newsDB->AddNewHdrToDB() failed\n");
#endif
        return rv;           
    }
    
    return NS_OK;
}

nsresult newsTestDriver::GetPath(const char *uri, nsFileSpec& aPathName)
{
  nsresult rv = NS_OK;

  rv = nsNewsURI2Path(kNewsRootURI, uri, aPathName);

#ifdef DEBUG_sspitzer
  printf("newsTestDriver::GetPath(%s,??) -> %s\n", uri, (const char *)aPathName);
#endif

  return rv;
}

nsresult newsTestDriver::GetDatabase(const char *uri)
{
    if (m_newsDB == nsnull) {
        nsNativeFileSpec path;
		nsresult rv = GetPath(uri, path);
		if (NS_FAILED(rv)) return rv;

        nsresult newsDBOpen = NS_OK;
        nsCOMPtr<nsIMsgDatabase> newsDBFactory;
        
        rv = nsComponentManager::CreateInstance(kCNewsDB, nsnull, nsIMsgDatabase::GetIID(), getter_AddRefs(newsDBFactory));
        if (NS_SUCCEEDED(rv) && newsDBFactory) {
                newsDBOpen = newsDBFactory->Open(path, PR_TRUE, PR_FALSE, getter_AddRefs(m_newsDB));
#ifdef DEBUG_sspitzer
                if (NS_SUCCEEDED(newsDBOpen)) {
                    printf ("newsDBFactory->Open() succeeded\n");
                }
                else {
                    printf ("newsDBFactory->Open() failed\n");
                }
#endif
                return rv;
        }
#ifdef DEBUG_sspitzer
        else {
            printf("nsComponentManager::CreateInstance(kCNewsDB,...) failed\n");
        }
#endif
    }
    
    return NS_OK;
}

int main()
{
    nsresult result;

    nsComponentManager::RegisterComponent(kPrefCID, nsnull, nsnull, PREF_DLL, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kFileLocatorCID,  NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
	// make sure prefs get initialized and loaded..
	// mscott - this is just a bad bad bad hack right now until prefs
	// has the ability to take nsnull as a parameter. Once that happens,
	// prefs will do the work of figuring out which prefs file to load...
	NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &result); 
    if (NS_FAILED(result) || prefs == nsnull) {
        exit(result);
    }

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
