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

#include "nsCOMPtr.h"

#include "plstr.h"
#include "plevent.h"

#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsITransport.h"
#include "nsIURL.h"
#include "nsINetService.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nntpCore.h"

#include "nsINntpService.h"

#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "nsIEventQueue.h" 
#include "nsIEventQueueService.h"
#include "nsIUrlListener.h"

#include "nsIPref.h"
#include "nsIFileLocator.h"

#include "nsMsgNewsCID.h"

#ifdef XP_PC
#define NETLIB_DLL	"netlib.dll"
#define XPCOM_DLL	"xpcom32.dll"
#define NEWS_DLL	"msgnews.dll"
#define PREF_DLL	"xppref32.dll"
#define APPSHELL_DLL	"nsappshell.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
#define NEWS_DLL   "libmsgnews"MOZ_DLL_SUFFIX
#define PREF_DLL   "libpref"MOZ_DLL_SUFFIX
#define APPSHELL_DLL "libnsappshell"MOZ_DLL_SUFFIX
#endif
#endif

/////////////////////////////////////////////////////////////////////////////////
// Define keys for all of the interfaces we are going to require for this test
/////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kNntpServiceCID, NS_NNTPSERVICE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kFileLocatorCID, NS_FILELOCATOR_CID);

#ifdef XP_UNIX
extern "C" char *fe_GetConfigDir(void) {
  printf("XXX: return /tmp for fe_GetConfigDir\n");
  return PL_strdup("/tmp");
}
#endif /* XP_UNIX */


class nsNntpTestDriver
{
public:
	nsNntpTestDriver();
    ~nsNntpTestDriver();

    nsresult ConvertStringTest(const char *str, const char *expectedStr);
	nsresult RunDriver();

protected:
    nsCOMPtr <nsINntpService> m_nntpService;
};

nsNntpTestDriver::nsNntpTestDriver()
{    
}

nsNntpTestDriver::~nsNntpTestDriver()
{
}

nsresult nsNntpTestDriver::RunDriver()
{
    nsresult rv;
    
    rv = nsComponentManager::CreateInstance(kNntpServiceCID, nsnull, nsINntpService::GetIID(), getter_AddRefs(m_nntpService));
    
    if (NS_FAILED(rv)) return rv;

    rv = ConvertStringTest("news://news.mozilla.org/a","a");
    if (NS_FAILED(rv)) return rv;

    rv = ConvertStringTest("news://news.mozilla.org/a,news://news.mozilla.org/b","a,b");
    if (NS_FAILED(rv)) return rv;

    rv = ConvertStringTest("news://news.mozilla.org/a,news://news.mozilla.org/b,news://news.mozilla.org/c","a,b,c");
    if (NS_FAILED(rv)) return rv;

    rv = ConvertStringTest("news://news.mozilla.org/a,news.mozilla.org/b,c","a,b,c");
    if (NS_FAILED(rv)) return rv;

    // this test should fail
    rv = ConvertStringTest("news://news.mozilla.org/a,news://news.mcom.com/b","a,b");
    if (NS_SUCCEEDED(rv)) return NS_ERROR_FAILURE;

    // this test should fail
    rv = ConvertStringTest("http://news.mozilla.org/a","a");
    if (NS_SUCCEEDED(rv)) return NS_ERROR_FAILURE;
    
    return NS_OK;
}

nsresult nsNntpTestDriver::ConvertStringTest(const char *str, const char *expectedStr)
{
    char *resultStr = nsnull;
    nsresult rv;
    rv = m_nntpService->ConvertNewsgroupsString(str, &resultStr);
    if (NS_FAILED(rv)) return rv;

    printf("%s -> %s\n",str,resultStr);
    printf("%s vs. %s\n",resultStr, expectedStr);
    if (!strcmp(resultStr, expectedStr)) {
        rv = NS_OK;
    }
    else {
        rv = NS_ERROR_FAILURE;
    }
    
    delete [] resultStr;
    resultStr = nsnull;
    return rv;
}

///////////////////////////////////////////////////////////////////////////////////
// End on command handlers for news
////////////////////////////////////////////////////////////////////////////////

int main()
{
    nsresult result;

    nsComponentManager::RegisterComponent(kNntpServiceCID, NULL, NULL, NEWS_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kPrefCID, nsnull, nsnull, PREF_DLL, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kFileLocatorCID,  NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
    
	// make sure prefs get initialized and loaded..
	// mscott - this is just a bad bad bad hack right now until prefs
	// has the ability to take nsnull as a parameter. Once that happens,
	// prefs will do the work of figuring out which prefs file to load...
	NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &result); 
    if (NS_FAILED(result) || !prefs) {
        exit(result);
    }

	nsNntpTestDriver * driver = new nsNntpTestDriver;
	if (driver)
	{
		result = driver->RunDriver();

        if (NS_FAILED(result)) {
            return result;
        }
	}
    else {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    printf("\"All's Well That Ends Well.\"\n\tA play by William Shakespeare. 1564-1616\n");
    return 0;
}
