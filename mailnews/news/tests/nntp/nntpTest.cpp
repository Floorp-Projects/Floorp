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
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nntpCore.h"

#include "nsINntpService.h"
#include "nsIUrlListener.h"

#include "nsIPref.h"
#include "nsIFileLocator.h"

#include "nsMsgNewsCID.h"

#include "nsString.h"

#ifdef XP_PC
#define XPCOM_DLL	"xpcom32.dll"
#define NEWS_DLL	"msgnews.dll"
#define PREF_DLL	"xppref32.dll"
#define APPSHELL_DLL	"nsappshell.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
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
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);

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
	nsresult TestConvertNewsgroupsString();
	nsresult TestPostMessage();

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

	rv = TestConvertNewsgroupsString();
	if (NS_FAILED(rv)) return rv;
	
	rv = TestPostMessage();
	if (NS_FAILED(rv)) return rv;
	
	/*
	nsIURI RunNewsUrl (in nsString urlString, in nsString newsgroupName, in nsMsgKey aKey, in nsISupports aConsumer, in nsIUrlListener aUrlListener); 

	nsIURI GetNewNews (in nsINntpIncomingServer nntpServer, in string uri, in nsIUrlListener aUrlListener);

	nsIURI CancelMessages (in string hostname, in string newsgroupname, in nsISupportsArray messages, in nsISupports aConsumer, in nsIUrlListener aUrlListener);
	*/

	return rv;
}

nsresult nsNntpTestDriver::TestPostMessage()
{
	nsresult rv;
	nsFilePath fileToPost("c:\\temp\\foo.txt");
	
	nsCAutoString newsgroups = "news://news.mozilla.org/netscape.test";

	/*
	nsIURI PostMessage (in nsFilePath pathToFile, in string newsgroupNames, in nsIUrlListener aUrlListener);
	*/
	rv=m_nntpService->PostMessage(fileToPost, newsgroups.GetBuffer(), nsnull, nsnull);
	return rv;
}

nsresult nsNntpTestDriver::TestConvertNewsgroupsString()
{
	nsresult rv;

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

    // this test should fail
    rv = ConvertStringTest("mailto://news.mozilla.org/a","a");
    if (NS_SUCCEEDED(rv)) return NS_ERROR_FAILURE;
    
    // this test should fail
    rv = ConvertStringTest("ldap://news.mozilla.org/a","a");
    if (NS_SUCCEEDED(rv)) return NS_ERROR_FAILURE;
    
    // this test should fail
    rv = ConvertStringTest("ftp://news.mozilla.org/a","a");
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

	nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kNntpServiceCID, NULL, NULL, NEWS_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kPrefCID, nsnull, nsnull, PREF_DLL, PR_TRUE, PR_TRUE);
	nsComponentManager::RegisterComponent(kFileLocatorCID,  NULL, NS_FILELOCATOR_PROGID, APPSHELL_DLL, PR_FALSE, PR_FALSE);

	// make sure prefs get initialized and loaded..
	// mscott - this is just a bad bad bad hack right now until prefs
	// has the ability to take nsnull as a parameter. Once that happens,
	// prefs will do the work of figuring out which prefs file to load...
	NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &result); 
    if (NS_FAILED(result) || (prefs == nsnull)) {
        exit(result);
    }
   
    if (NS_FAILED(prefs->ReadUserPrefs()))
    {
      printf("Failed on reading user prefs!\n");
      exit(-1);
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
