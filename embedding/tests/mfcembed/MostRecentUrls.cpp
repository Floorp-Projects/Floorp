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
 *   Rod Spears <rods@netscape.com>   
 */

//
// CMostRecentUrls object is responsible for keeping track of the
// 16 most recently used URLs. It stores this list in a file named
// "urls.txt" on a per profile basis in the user's profile directory
//
// The constructor loads the URL list
// The destructor saves the URL list
//

#include "StdAfx.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsAppDirectoryServiceDefs.h"
#include "MostRecentUrls.h"

//--------------------------------------------------------
//-- CMostRecentUrls
//--------------------------------------------------------
CMostRecentUrls::CMostRecentUrls() :
        mNumURLs(0)
{
	for (int i=0;i<MAX_URLS;i++) {
		mURLs[i] = NULL;
	}

	FILE * fd = GetFD("r");
	if (fd) {
		char line[512];
		while (fgets(line, 512, fd)) {
			if (strlen(line) > 1) {
				line[strlen(line)-1] = 0;
				mURLs[mNumURLs++] = _strdup(line);
			}
		}
		fclose(fd);
	}

}

FILE * CMostRecentUrls::GetFD(const char * aMode) 
{
    FILE * fd = nsnull;
    nsCOMPtr<nsIFile> file;
    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsILocalFile> local_file(do_QueryInterface(file));
        local_file->Append("urls.txt");
        local_file->OpenANSIFileDesc(aMode, &fd);
    }

    return fd;
}

CMostRecentUrls::~CMostRecentUrls() 
{
    FILE * fd = GetFD("w");
    if (fd) {
        for (int i=0;i<MAX_URLS;i++) {
            if(mURLs[i])
                fprintf(fd, "%s\n", mURLs[i]);
        }
    fclose(fd);
    }

    for (int i=0;i<MAX_URLS;i++) {
        if(mURLs[i])
          free(mURLs[i]);
    }
}

char * CMostRecentUrls::GetURL(int aInx)
{
    if (aInx < mNumURLs) {
        return mURLs[aInx];
    }

    return NULL;
}

void CMostRecentUrls::AddURL(const char * aURL)
{
	TCHAR szTemp[512];
	strcpy(szTemp, aURL);

	// check to see if an existing url matches the one passed in
	for (int i=0; i<MAX_URLS-1; i++)
	{
        if(mURLs[i])
        {
            if(strcmpi(mURLs[i], szTemp) == 0)
                break; 
        }
	}

    // if there was a match "i" will point to matching url entry
    // if not i will be MAX_URLS-1

    // move all url entries before this one down
	for (; i>0; i--)
	{
        if(mURLs[i])
          free(mURLs[i]);

        if(mURLs[i-1])  
            mURLs[i] = _strdup(mURLs[i-1]);
	}

	// place this url at the top
    if(mURLs[0])
        free(mURLs[0]);
    mURLs[0] = _strdup(szTemp);
}
