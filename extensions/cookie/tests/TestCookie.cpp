/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#include "nsIServiceManager.h"
#include "nsICookieService.h"
#include "stdio.h"
#include "nsNetUtil.h"
#include "nsXPIDLString.h"
#include "nsIEventQueueService.h"
#include "nsIStringBundle.h"


static nsIEventQueue* gEventQ = nsnull;


static NS_DEFINE_CID(kCookieServiceCID, NS_COOKIESERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);

void SetACookie(nsICookieService *cookieService, const char* aSpec, const char* aCookieString) {
    nsCOMPtr<nsIURI> uri;
    (void)NS_NewURI(getter_AddRefs(uri), aSpec);
    NS_ASSERTION(uri, "malformed uri");   
    
    printf("setting cookie for \"%s\" : ", aSpec);
    nsresult rv = cookieService->SetCookieString(uri, nsnull, (char *)aCookieString);
    if (NS_FAILED(rv)) {
        printf("NOT-SET\n");
    } else {
        printf("\"%s\" was set.\n", aCookieString);
    }
    return;
}

void GetACookie(nsICookieService *cookieService, const char* aSpec, char* *aCookie) {
    nsCOMPtr<nsIURI> uri;
    (void)NS_NewURI(getter_AddRefs(uri), aSpec);
    NS_ASSERTION(uri, "malformed uri");   

    char * cookieString;
    printf("retrieving cookie(s) for \"%s\" : ", aSpec);
    nsresult rv = cookieService->GetCookieString(uri, &cookieString);
    if (NS_FAILED(rv)) printf("XXX GetCookieString() failed!\n");

    if (!cookieString) {
        printf("NOT-FOUND\n");
    } else {
        printf("FOUND: ");
        printf("%s\n", cookieString);
        nsCRT::free(cookieString);
    }
    return;
}


int main(PRInt32 argc, char *argv[])
{
    nsresult rv = NS_InitXPCOM(nsnull, nsnull);
    if (NS_FAILED(rv)) return -1;

    // Create the Event Queue for this thread...
    nsCOMPtr<nsIEventQueueService> eventQService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return -1;

    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) return -1;

    eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);

    nsCOMPtr<nsIStringBundleService> bundleService = 
             do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIStringBundle> stringBundle;
        char*  propertyURL = "chrome://necko/locale/necko.properties";
        rv = bundleService->CreateBundle(propertyURL, getter_AddRefs(stringBundle));
    }

    nsCOMPtr<nsICookieService> cookieService = 
             do_GetService(kCookieServiceCID, &rv);
	if (NS_FAILED(rv)) return -1;
 
    SetACookie(cookieService, "http://www.blah.com", "myCookie=yup; path=/");
    nsXPIDLCString cookie;
    GetACookie(cookieService, "http://www.blah.com", getter_Copies(cookie));
    GetACookie(cookieService, "http://www.blah.com/testPath/testfile.txt", getter_Copies(cookie));

    SetACookie(cookieService, "http://blah/path/file", "aaa=bbb; path=/path");
    GetACookie(cookieService, "http://blah/path", getter_Copies(cookie));
    GetACookie(cookieService, "http://blah/2path2", getter_Copies(cookie));

    SetACookie(cookieService, "http://www.netscape.com/", "cookie=true; domain=bull.com;");
    GetACookie(cookieService, "http://www.bull.com/", getter_Copies(cookie));

    SetACookie(cookieService, "http://www.netscape.com/", "cookie=true; domain=netscape.com;");
    GetACookie(cookieService, "http://www.netscape.com/", getter_Copies(cookie));

    SetACookie(cookieService, "http://www.netscape.com/", "cookie=true; domain=.netscape.com;");
    GetACookie(cookieService, "http://bla.netscape.com/", getter_Copies(cookie));


    NS_ShutdownXPCOM(nsnull);
    
    return 0;
}

