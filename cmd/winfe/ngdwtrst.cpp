/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "stdafx.h"

#include "ngdwtrst.h"
#include "helper.h"
#include "viewerse.h"

CSpawnList::CSpawnList()    {
//	Purpose:    Create our spawn list, by reading in entries out of the INI file.
//	Arguments:  void
//	Returns:    none
//	Comments:   Must be called after correct resolution of the INI file is completed.
//	Revision History:
//      04-10-95    created GAB
//
    CString csProfile = AfxGetApp()->m_pszProfileName;  //  Profile file.
    CString csEntry, csRelationship;    //  Entry, actual value.

    DWORD dwRead;
    DWORD dwBuffSize = 0;   //  Size of read buffer.
    char *pBuffer = NULL;
    do  {
        dwBuffSize += 16;  //  Increment the buffer, if we already had one, get rid of it.
        if(pBuffer != NULL) {
            delete[] pBuffer;
        }

        pBuffer = new char[dwBuffSize];    //  keep off stack.
        if(pBuffer == NULL) {
            //  Well, there's not enough memory to do anything.
            //  We're just skipping out with no registrations whatsoever.
            pBuffer = new char[2];
            *pBuffer = '\0';
            break;
        }

        //  Read in an entire section, to get all the possible app names.
        dwRead = theApp.GetPrivateProfileString(
        	SZ_IN_GOD_WE_TRUST, NULL, "", pBuffer, dwBuffSize, csProfile);

        //  Continue the loop if the buffer was too small.
    }   while(dwRead == (dwBuffSize - 2));

    //  Loop through the entries.
    //  End of list is double NULL.
    char *pTraverse = pBuffer;
    CTrust *pTrust;
    while(*pTraverse != '\0') {
        csEntry = pTraverse;

        //  Look up to see if this entry is friend or foe.
        csRelationship = theApp.GetProfileString(SZ_IN_GOD_WE_TRUST, csEntry, SZ_NO);

        //  Add it to the list (Default is no).
        if(csRelationship.CompareNoCase(SZ_YES) == 0) {
            pTrust = new CTrust(csEntry, CTrust::m_Friend);
        }
        else    {
            pTrust = new CTrust(csEntry, CTrust::m_Stranger);
        }
        m_cplOpinions.AddTail(pTrust);

        //  Increment beyond the length of the entry, +1 for the NULL.
        pTraverse += csEntry.GetLength() + 1;
    }

    //  Get rid of our buffer.
    delete[] pBuffer;
}

CSpawnList::~CSpawnList()    {
//	Purpose:    Write our spawn list to the INI file.
//	Arguments:  void
//	Returns:    none
//	Comments:   Must be called before the INI file name is lost.
//	Revision History:
//      04-10-95    created GAB
//

    //  Go through our list, and write each value out to the INI file.
    CTrust *pTrust;
    while(m_cplOpinions.IsEmpty() == FALSE) {
        pTrust = (CTrust *)m_cplOpinions.RemoveHead();

        if(pTrust->GetRelationship() == CTrust::m_Stranger)   {
            theApp.WriteProfileString(SZ_IN_GOD_WE_TRUST, pTrust->GetExeName(), SZ_NO);
        }
        else    {
            theApp.WriteProfileString(SZ_IN_GOD_WE_TRUST, pTrust->GetExeName(), SZ_YES);
        }

        delete pTrust;
    }
}

BOOL CSpawnList::CanSpawn(CString& csExeName, CWnd *pWnd)   {
//	Purpose:    Determines if an EXE can be spawned off or not.
//	Arguments:  csExeName   The application in question.
//              pWnd        The owner of the dialog box.
//	Returns:    BOOL    TRUE    Can spawn.
//                      FALSE   Can't spawn.
//	Comments:   If the EXE isn't in the list, then we'll popup a generic dialog box asking
//                  the user what to do.  If they ask for the entry to be persistant, then
//                  we'll add it to the list.
//              All spawned executables, wether external viewers, or implicit embeds, must be
//                  validated by the user.
//	Revision History:
//      04-10-95    created GAB
//

    //  Can't spawn anything without a name!
    if(csExeName.IsEmpty()) {
        return(FALSE);
    }

    //  We immediately have to trust anything that is marked as good in the
    //      registry.

    //  First, check to see if the executable is by default trusted by the user (set up
    //      as an external viewer through preferences).
    POSITION rIndex = CHelperApp::m_cplHelpers.GetHeadPosition();
    CHelperApp *pApp;
    while(rIndex != NULL)   {
        pApp = (CHelperApp *)CHelperApp::m_cplHelpers.GetNext(rIndex);

        if(pApp->csCmd.CompareNoCase(csExeName) == 0)   {
            //  We're going to be comparing a mime type below, make sure the app has one assigned.
            if(pApp->cd_item != NULL && pApp->cd_item->ci.type != NULL) {
                //  Before we go off validating this one, make sure it's not a fake association from
                //      the registry or win.ini file.
                if(strncmp(pApp->cd_item->ci.type, SZ_WINASSOC, strlen(SZ_WINASSOC)) == 0)    {
                    //  It's fake, we won't let this through.
                    //  We continue, because there very well could be duplicate apps, some which
                    //      may be valid.
                    continue;
                }

                //  Make sure that the app is set up to handle as an external viewer.
                //  If not, then we'll continue along our merry way, there may be a duplicate
                //      setting allowing the app to function as an external viewer.
                if(pApp->how_handle == HANDLE_EXTERNAL) {
                    //  Trusted since the user has set this app up to be an external viewer.
                    return(TRUE);
                }
            }
        }
    }

    //  Loop through the entries that we have.
    //  Do a comparison without case.
    //  If it exists, then we check wether or not we will trust it.
    rIndex = m_cplOpinions.GetHeadPosition();
    CTrust *pTrust;
    while(rIndex != NULL)   {
        pTrust = (CTrust *)m_cplOpinions.GetNext(rIndex);

        if(pTrust->GetExeName().CompareNoCase(csExeName) == 0)    {
            if(pTrust->GetRelationship() == CTrust::m_Stranger) {
                return(FALSE);
            }
            else    {
                return(TRUE);
            }
        }
    }

    //  Finally, it doesn't exist in our list.  We need to prompt the user, and possibly
    //      add the entry to our list if they want the setting to be persistant.
    CViewerSecurity dlgSecurity(pWnd);

    //  Initialize the dialog to some defaults.
    char *pBuf = new char[512]; //  Keep it off the stack.
    CString csBuf;

    csBuf.LoadString(IDS_VIEWER_SEC_MESSAGE);
    sprintf(pBuf, csBuf, (const char *)csExeName);
    dlgSecurity.m_csMessage = pBuf;

    csBuf.LoadString(IDS_VIEWER_SEC_DONTASK);
    sprintf(pBuf, csBuf, (const char *)csExeName);
    dlgSecurity.m_csDontAskText = pBuf;
    delete[] pBuf;

    //  Have the user decide.
    dlgSecurity.DoModal();
    if(dlgSecurity.m_bCanceled == FALSE)   {
        //  Check for persistance.
        if(dlgSecurity.m_bAskNoMore == TRUE)    {
            m_cplOpinions.AddTail(new CTrust(csExeName, CTrust::m_Friend));
        }

        //  They want to go ahead and become the recepticle of a virus.
        return(TRUE);
    }
    else {
        //  Check for persistance.
        if(dlgSecurity.m_bAskNoMore == TRUE)    {
            m_cplOpinions.AddTail(new CTrust(csExeName, CTrust::m_Stranger));
        }

        //  They want no part of corrupting their machine, and are good little children of the
        //      internet.
        return(FALSE);
    }
}

// Returns TRUE if we should ask the user before downloading a file of this type or
// FALSE if the user has indicated we trust it
BOOL CSpawnList::PromptBeforeOpening(LPCSTR lpszApp)
{
	BOOL	    bPrompt = TRUE;
	POSITION    rIndex;

    // Loop through the entries that we have. Do a comparison without case.
    // If it exists, then we check wether or not we will trust it.
    rIndex = m_cplOpinions.GetHeadPosition();

    while (rIndex != NULL)   {
        CTrust *pTrust = (CTrust *)m_cplOpinions.GetNext(rIndex);

        if (pTrust->GetExeName().CompareNoCase(lpszApp) == 0) {
            bPrompt = pTrust->GetRelationship() == CTrust::m_Stranger;
            break;
        }
    }

	return bPrompt;
}

void
CSpawnList::SetPromptBeforeOpening(LPCSTR lpszApp, BOOL bPrompt)
{
    POSITION rIndex, rCurrent;

    // Loop through the entries that we have. Do a comparison without case.
    rIndex = m_cplOpinions.GetHeadPosition();

    while (rIndex != NULL)   {
        rCurrent = rIndex;

        CTrust *pTrust = (CTrust *)m_cplOpinions.GetNext(rIndex);

        if (pTrust->GetExeName().CompareNoCase(lpszApp) == 0) {
            if (bPrompt) {
                // Remove this entry from the list
                theApp.WriteProfileString(SZ_IN_GOD_WE_TRUST, pTrust->GetExeName(), NULL);
                m_cplOpinions.RemoveAt(rCurrent);
                delete pTrust;

            } else {
                // User no longer wants to be prompted before opening downloaded
                // files of this type. Mark it as being trusted
                pTrust->SetRelationship(CTrust::m_Friend);
            }

            return;
        }
    }

    // We didn't find an existing entry in the list
    if (!bPrompt) {
        CString	strApp(lpszApp);

        // User no longer wants to be prompted before opening downloaded files
        // of this type
        m_cplOpinions.AddTail(new CTrust(strApp, CTrust::m_Friend));
    }
}
