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

#include "helper.h"
#include "il_strm.h"
#include "display.h"
#include "prefapi.h" //CRN_MIME

//  List of all helpers in our helper app struct.
//  Static must be declared here.
CPtrList CHelperApp::m_cplHelpers;

//
// Parse a list of extensions for file types
//
// Extensions will come in as a list .sg,.earh,.rts .ear
//
// Parse this list, white space is used to delimit between entries.  The
//  number of entries is returned in num_exts and the individual extensions
//  are returned in list
//
// I think I've fixed it so that we are no longer dropping extensions 
//  for .mp2 files.  chouck 29-Dec-94
//


void fe_ParseExtList(const char * ext_string, int *num_exts, char **list) 
{
	char * start, *cur, *newext;
	int iLen, idx;
	BOOL bInExt = FALSE;
	char * copy = strdup(ext_string);  // make copy to muck with
	int iExtCnt = 0;
	
	start = cur = copy;
	iLen = strlen(copy);

	// traverse string	
	for (idx =0; idx < iLen; idx++) {
		if (bInExt) {
			// We are currently reading an extension
			// If we have a character add it to the current extension

			if (!isalnum(*cur))	{

				// Found a delimiter character, end the extension

				bInExt = FALSE;
				*cur = 0; // mark end
				list[iExtCnt] = strdup(newext);
				iExtCnt++;				

			}

		} else {

			// We are not currently in an extension, search for next alpha numeric character

			if (isalnum(*cur)) {
				// found one! -- mark start of new extenison -- set flag
				newext = cur;
				bInExt = TRUE;

			}

		}

		cur++;
	}
	// save last one
	if (bInExt) {
		list[iExtCnt] = strdup(newext);
		iExtCnt++;				
	}
	*num_exts = iExtCnt;
	
	//	Be sure to free your copy to muck with
	XP_FREE(copy);
}

// Returns TRUE if lpszExt is in the list of comma delimited extensions
// (this is the Netscape format used to write the suffixes associated a
// particular MIME type). lpszExt should not have a lead '.'
static BOOL
ExtensionIsInList(LPCSTR lpszExtList, LPCSTR lpszExt)
{
	LPSTR	extlist[50]; // XXX - fe_ParseExtList needs another parameter...
	int 	nExtensions = 0;
	BOOL	bResult = FALSE;
	
    ASSERT(lpszExt && (*lpszExt != '.'));
	fe_ParseExtList(lpszExtList, &nExtensions, extlist);

	// Look for our extension
	for (int i = 0; i < nExtensions; i++) {
		if (strcmpi(extlist[i], lpszExt) == 0) {
			bResult = TRUE;
			break;
		}
	}

	// Free the extensions
	for (i = 0; i < nExtensions; i++)
		XP_FREE(extlist[i]);

	return bResult;
}

// Returns TRUE if lpszExt is in the list of extensions for this
// type. lpszExt should not have a lead '.' (none of the extensions
// in pcdata have a leading '.')
static BOOL
ExtensionIsInList(NET_cdataStruct *pcdata, LPCSTR lpszExt)
{
    ASSERT(lpszExt && (*lpszExt != '.'));
	for (int i = 0; i < pcdata->num_exts; i++)	{
		if (stricmp(lpszExt, pcdata->exts[i]) == 0)	{
			return TRUE;
		}
	}

	return FALSE;
}

// Creates a front-end data structure (CHelperApp object) if there isn't already one
// Looks in the Viewers section to see how the MIME type should be configured
void fe_AddTypeToList(NET_cdataStruct *cd_item) 
{
    CHelperApp *app = NULL;

    if(cd_item->ci.fe_data) {
        app = (CHelperApp *)cd_item->ci.fe_data;
    }
    else    {
		// Create a front-end data structure
        app = new CHelperApp();
    }
	
	app->bChanged = FALSE;
	app->bNewType = FALSE;
	app->bChangedExts = FALSE;
	
	CString csCmd = theApp.GetProfileString("Viewers", cd_item->ci.type);

	// Handle internally by default
	if (csCmd.IsEmpty()) {
		if ( (!strcmp(cd_item->ci.type, TEXT_HTML))  || 
		     (!strcmp(cd_item->ci.type, TEXT_PLAIN)) ||
			 (!strcmp(cd_item->ci.type, IMAGE_GIF))  || 
			 (!strcmp(cd_item->ci.type, IMAGE_XBM))  || 
			 (!strcmp(cd_item->ci.type, IMAGE_JPG)) ||
			// (!strcmp(cd_item->ci.type, APPLICATION_BINHEX)) ||
			 (!strcmp(cd_item->ci.type, "application/x-ns-proxy-autoconfig"))) {
			app->how_handle = HANDLE_VIA_NETSCAPE;
		}

        if(!stricmp(cd_item->ci.type, APPLICATION_OCTET_STREAM)) {
                app->how_handle = HANDLE_SAVE;
        }
	}
	else {
		int iPercent = csCmd.ReverseFind('%');  // strip %ls from config
		if (iPercent > 0)
			csCmd = csCmd.Left(iPercent-1);

		if (csCmd == MIME_SAVE) 
			app->how_handle = HANDLE_SAVE;
		else if (csCmd == MIME_INTERNALLY)
			app->how_handle = HANDLE_VIA_NETSCAPE;
		else if (csCmd == MIME_PROMPT)
			app->how_handle = HANDLE_UNKNOWN;	
        else if (csCmd == MIME_OLE)	{
			app->how_handle = HANDLE_BY_OLE;
		}
        else if (csCmd == MIME_SHELLEXECUTE)	{
			app->how_handle = HANDLE_SHELLEXECUTE;
		}
		else {
			char	szFile[_MAX_FNAME];
			
			// This is the default case.
			app->csCmd = csCmd;
			app->how_handle = HANDLE_EXTERNAL;

			// If there's no description then the helper app UI won't display the item
			// Use the base filename without extension
			szFile[0] = '\0';
			_splitpath((LPCSTR)csCmd, NULL, NULL, szFile, NULL);
			if (szFile[0] == '\0') {
				// Just use the MIME type for the description
				cd_item->ci.desc = XP_STRDUP(cd_item->ci.type);

			} else {
				szFile[0] = toupper(szFile[0]);
				cd_item->ci.desc = PR_smprintf("%s %s", szFile, szLoadString(IDS_STRING_FILE));
			}
		}

		if (app->how_handle == HANDLE_SHELLEXECUTE || app->how_handle == HANDLE_BY_OLE) {
			// We need to have a description or the helper app UI won't display the
			// item. If it's set as shell execute or launch as OLE that means we're getting
            // it from the registry, and it better have a description...
            ASSERT(cd_item->ci.desc);
        }
	}

	app->cd_item = cd_item;
	cd_item->ci.fe_data = app;  // store the helper app object
	theApp.m_HelperListByType.SetAt(cd_item->ci.type,app);  // store in global list
}

void fe_SetExtensionList(NET_cdataStruct *cd_item) 
{
	if (!cd_item)
		return;

	char * extlist[50]; // BUG shouldn't limit it to 50 extensions-- big deal...blah easier to debug
	int iNumExt =0;  // number from ini file
	int idx;

	CString csExtListFromINI = theApp.GetProfileString("Suffixes",cd_item->ci.type);
	if (!csExtListFromINI.IsEmpty()) {
		// add extension out of ini file
		fe_ParseExtList((const char *) csExtListFromINI, &iNumExt, extlist);
	}

	if (iNumExt > 0) {
		// got extension from INI file...overwrite netlib stuff
		for (idx = 0; idx < cd_item->num_exts ; idx++) {
			if (cd_item->exts[idx]) {
				XP_FREE(cd_item->exts[idx]);
				cd_item->exts[idx] = NULL;
			}
		}
		// store our new stuff
		if (cd_item->exts) XP_FREE(cd_item->exts);
		cd_item->exts = (char **)XP_ALLOC(iNumExt * sizeof(char *));
		cd_item->num_exts = iNumExt;
		for (idx = 0; idx < cd_item->num_exts ; idx++)
			cd_item->exts[idx] = extlist[idx];
	}
}

// Retrieve the class name for the given extension
BOOL
GetClassName(LPCSTR lpszExtension, CString &strClass)
{
    char    szKey[_MAX_EXT + 1];  // space for '.'
    char    szClass[128];
	LONG	lResult;
	LONG	lcb;

	// Look up the file association key which maps a file extension
	// to a file class
    wsprintf(szKey, ".%s", lpszExtension);

	lcb = sizeof(szClass);
    lResult = RegQueryValue(HKEY_CLASSES_ROOT, szKey, szClass, &lcb);
	
#ifdef _WIN32
	ASSERT(lResult != ERROR_MORE_DATA);
#endif
	if (lResult != ERROR_SUCCESS || lcb <= 1)
		return FALSE;

    strClass = szClass;
    return TRUE;
}

//  Return value must be freed by caller.
char *InventMime(const char *pExtension)
{
    LPSTR   lpszMimeType = NULL;
    char    szKey[_MAX_EXT + 1];  // space for '.'

    if (!pExtension)
        return NULL;

    // Build the file association key. It must begin with a '.'
    wsprintf(szKey, ".%s", pExtension);
    
#ifdef XP_WIN32
    //  See if the extension has a mime type (Content Type).
    HKEY hExt = NULL;
    LONG lCheckOpen = RegOpenKeyEx(HKEY_CLASSES_ROOT, szKey, 0, KEY_QUERY_VALUE, &hExt);

    if (lCheckOpen == ERROR_SUCCESS) {
        DWORD	dwType;
        DWORD	cbData;
        LONG    lResult;
         
        // See how much space we need
        cbData = 0;
        lResult = RegQueryValueEx(hExt, "Content Type", NULL, &dwType, NULL, &cbData);
        if (lResult == ERROR_SUCCESS && cbData > 1) {
            lpszMimeType = (LPSTR)XP_ALLOC(cbData);
            if (!lpszMimeType) {
                VERIFY(RegCloseKey(hExt) == ERROR_SUCCESS);
                return NULL;
            }

            // Get the string
            lResult = RegQueryValueEx(hExt, "Content Type", NULL, &dwType, (LPBYTE)lpszMimeType, &cbData);
            ASSERT(lResult == ERROR_SUCCESS);
        }

        VERIFY(RegCloseKey(hExt) == ERROR_SUCCESS);
    }
#endif

	//  Finally, default to fake generated mime type
    if (!lpszMimeType) {
        CString strClass;
        
        // Only do this is there's a class associated with the extension
        if (GetClassName(pExtension, strClass) && !strClass.IsEmpty())
            lpszMimeType = PR_smprintf("%s%s", SZ_WINASSOC, (LPCSTR)strClass);
    }

    return lpszMimeType;
}

//  Return value must be freed by caller.
char *InventDescription(const char *pExtension)
{
    CString strClass;

    if (!pExtension)
        return NULL;

    // Get the class name
    if (GetClassName(pExtension, strClass) && !strClass.IsEmpty()) {
        LONG    cbData;
        LONG    lResult;

#ifdef _WIN32
        // See how much space we need
        cbData = 0;
        lResult = RegQueryValue(HKEY_CLASSES_ROOT, (LPCSTR)strClass, NULL, &cbData);
        if (lResult == ERROR_SUCCESS && cbData > 1) {
            LPSTR   lpszDescription = (LPSTR)XP_ALLOC(cbData);

            if (!lpszDescription)
                return NULL;

            // Get the string
            VERIFY(RegQueryValue(HKEY_CLASSES_ROOT, (LPCSTR)strClass, lpszDescription, &cbData) == ERROR_SUCCESS);
            return lpszDescription;
        }
#else
		// Win 3.1 RegQueryValue() doesn't support asking for the size of the data
		char	szDescription[128];

		// Get the string
		cbData = sizeof(szDescription);
		lResult = RegQueryValue(HKEY_CLASSES_ROOT, (LPCSTR)strClass, szDescription, &cbData);
		return lResult == ERROR_SUCCESS ? XP_STRDUP(szDescription) : NULL;
#endif
    }

    return NULL;
}

static BOOL
HasShellOpenCommand(LPCSTR lpszFileClass)
{
	char	szKey[_MAX_PATH];
	HKEY	hKey;

	// See if there's a shell/open key specified for the file class
	PR_snprintf(szKey, sizeof(szKey), "%s\\shell\\open", lpszFileClass);

	if (RegOpenKey(HKEY_CLASSES_ROOT, szKey, &hKey) == ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return TRUE;
	}

	return FALSE;
}

// Create a front-end data structure if necessary, and set how_handle as
// HANDLE_SHELLEXECUTE if there's a shell\open command for the file extension.
// It will also set the description if there isn't already one
void ShellHelper(NET_cdataStruct *pNet, const char *pExtension)
{
    if(pNet != NULL && pNet->ci.fe_data == NULL)    {
        //  Create front end counter-part.
        CHelperApp *pApp = new CHelperApp();
        if(pApp)    {
	        pApp->cd_item = pNet;
	        pNet->ci.fe_data = (void *)pApp;

            pApp->bChanged = FALSE;
            pApp->bNewType = FALSE;
            pApp->bChangedExts = FALSE;

            // Don't set how_handle to HANDLE_SHELLEXECUTE unless there's a file
            // type class associated with the extension which has a shell\open
			// command
			//
			// Assume that we can't shell execute it
			pApp->how_handle = HANDLE_UNKNOWN;

            if (GetClassName(pExtension, pApp->strFileClass)) {
				// XXX - We really should handle verbs other than Open. FindExecutable()
				// and ShellExecute() don't either, but ShellExecuteEx() does...
				if (HasShellOpenCommand((LPCSTR)pApp->strFileClass)) {
					pApp->how_handle = HANDLE_SHELLEXECUTE;
					pApp->csCmd = MIME_SHELLEXECUTE;
				}
            }


            //  application/octet-stream is always save.
            if(!stricmp(pNet->ci.type, APPLICATION_OCTET_STREAM)) {
                    pApp->how_handle = HANDLE_SAVE;
            }

	        theApp.m_HelperListByType.SetAt(pNet->ci.type, pApp);
        }

        //  Give it a description if not already set.
        if(pExtension != NULL && pNet->ci.desc == NULL)   {
            pNet->ci.desc = InventDescription(pExtension);
        }
    }
}

// Given a file extension and its associated MIME type (may be NULL),
// make sure there's a corresponding entry in the netlib list of
// NET_cdataStruct structures
//
// There must be an extension, and the extension must not begin with
// a leading '.'
NET_cdataStruct *
ProcessFileExtension(const char *pExtension, const char *ccpMimeType)
{
	NET_cdataStruct *pExistingItem = NULL;

    //  Must have an extension to be considered, must not begin with a period.
    if(pExtension == NULL)  {
        return NULL;
    }
    else if(*pExtension == '.')  {
        ASSERT(0);
        return NULL;
    }

    // See if the extension is already in the netlib list. If it is AND it has a MIME
	// type then call ShellHelper() which will set how_handle to HANDLE_SHELLEXECUTE
    // if there is a shell\open command for the extension
    XP_List *pTypesList = cinfo_MasterListPointer();
    NET_cdataStruct *pListEntry = NULL;
    while ((pListEntry = (NET_cdataStruct *)XP_ListNextObject(pTypesList)))	{
        if(pListEntry->ci.type != NULL || pListEntry->ci.encoding != NULL)	{
            for(int iFind = 0; iFind < pListEntry->num_exts; iFind++)	{
                if(stricmp(pExtension, pListEntry->exts[iFind]) == 0)	{
                    // This call will create a front-end data structure if
                    // necessary, and set how to handle as HANDLE_SHELLEXECUTE if
                    // there's a shell\open command for the file extension. It will
                    // also set the description if there isn't already one
                    if(pListEntry->ci.type)
						ShellHelper(pListEntry, pExtension);

					// Remember this item
					pExistingItem = pListEntry;
                }
            }
        }
    }

	if(pExistingItem && pExistingItem->ci.encoding)
	{
		/* don't overwrite existing encodings with nonsense types */
		return NULL;
	}

    //  If no mime type was passed in, invent one.
    const char *pMimeType = ccpMimeType;
    BOOL bFreeMimeType = FALSE;
    if(pMimeType == NULL)   {
        pMimeType = (const char *)InventMime(pExtension);
        if(pMimeType)   {
            bFreeMimeType = TRUE;
        }
        else    {
            return NULL;
        }
    }

    // Ignore extensions in the registry that have content type "application/x-msdownload"
    if (bFreeMimeType && stricmp(pMimeType, "application/x-msdownload") == 0) {
        XP_FREE((void *)pMimeType);
        pMimeType = XP_STRDUP(APPLICATION_OCTET_STREAM);
    }
    
    //  If mime type already exists in the list, must add extention to it.
    pTypesList = cinfo_MasterListPointer();
    pListEntry = NULL;
    BOOL bExisted = FALSE;
    while ((pListEntry = (NET_cdataStruct *)XP_ListNextObject(pTypesList)))	{
        if(pListEntry->ci.type != NULL)	{
            if(stricmp(pListEntry->ci.type, pMimeType) == 0)	{
                bExisted = TRUE;

				// Check if the extension is already in the list
				if (!ExtensionIsInList(pListEntry, pExtension)) {
					char **ppTest = (char **)XP_REALLOC((void *)(pListEntry->exts), sizeof(char *) * (pListEntry->num_exts + 1));
					if(ppTest)  {
						pListEntry->exts = ppTest;
						pListEntry->exts[pListEntry->num_exts] = XP_STRDUP(pExtension);
						if(pListEntry->exts[pListEntry->num_exts])    {
							pListEntry->num_exts++;
	
							// This call will create a front-end data structure if
							// necessary, and set how to handle as HANDLE_SHELLEXECUTE if
							// there's a shell\open command for the file extension. It will
							// also set the description if there isn't already one
							ShellHelper(pListEntry, pExtension);
						}
					}
                }
            }
        }
    }
    if(bExisted)    {
        if(bFreeMimeType)   {
            XP_FREE((void *)pMimeType);
            pMimeType = NULL;
        }
        return pListEntry;
    }

    // There's no entry in the netlib list for this MIME type so create a new entry. Don't
	// do this if there's an existing entry in the netlib list with the same extension and
	// we made up the MIME type (it's a fake generated MIME type)
	if (pExistingItem && strncmp(pMimeType, SZ_WINASSOC, strlen(SZ_WINASSOC)) == 0)
	{
   		if(bFreeMimeType)   {
       		XP_FREE((void *)pMimeType);
   		}
		return pExistingItem;
	}

    NET_cdataStruct *pNew = NET_cdataCreate();
    if(pNew)    {
        pNew->ci.type = XP_STRDUP(pMimeType);
        if(pNew->ci.type)   {
            pNew->num_exts = 1;
            pNew->exts = (char **)XP_ALLOC(sizeof(char *));
            if(pNew->exts)  {
                pNew->exts[0] = XP_STRDUP(pExtension);
                if(pNew->exts[0])   {
                    NET_cdataAdd(pNew);
                }
                else    {
                    NET_cdataFree(pNew);
                    pNew = NULL;
                }
            }
            else    {
                NET_cdataFree(pNew);
                pNew = NULL;
            }
        }
        else    {
            NET_cdataFree(pNew);
            pNew = NULL;
        }

        if(pNew)    {
            // This call will create a front-end data structure if
            // necessary, and set how to handle as HANDLE_SHELLEXECUTE if
            // there's a shell\open command for the file extension. It will
            // also set the description if there isn't already one
            ShellHelper(pNew, pExtension);
        }
    }

    if(bFreeMimeType)   {
        XP_FREE((void *)pMimeType);
        pMimeType = NULL;
    }

	return pNew;
}

// This routines looks at every file type association in the registry.
// For each file extension it calls ProcessFileExtension()
void registry_GenericFileTypes()
{
    char aExtension[MAX_PATH + 1];
    memset(aExtension, 0, sizeof(aExtension));
    DWORD dwExtKey = 0;
    LONG lCheckEnum = ERROR_SUCCESS;

    do  {
        lCheckEnum = RegEnumKey(HKEY_CLASSES_ROOT, dwExtKey++, aExtension, sizeof(aExtension));
        if(lCheckEnum == ERROR_SUCCESS && aExtension[0] == '.') {
			ProcessFileExtension(&aExtension[1], NULL);
        }
    }
    while(lCheckEnum == ERROR_SUCCESS);
}

#ifndef _WIN32
// This routines looks at every file type association in the WIN.INI file.
// For each file extenion it calls ProcessFileExtension()
void winini_GenericFileTypes()
{
    const char *pSection = "Extensions";
    int iAllocSize = 1024 * 16;
    char *pBuffer = (char *)XP_ALLOC(iAllocSize);
    if(pBuffer) {
        memset(pBuffer, 0, iAllocSize);
        if(0 != GetProfileString(pSection, NULL, "", pBuffer, iAllocSize))  {
            char *pTraverse = pBuffer;
            char aExt[_MAX_PATH];
            memset(aExt, 0, sizeof(aExt));
            while(*pTraverse != '\0')   {
                strcpy(aExt, pTraverse);
                pTraverse += strlen(aExt) + 1;
                ProcessFileExtension(aExt, NULL);
            }
        }

        XP_FREE((void *)pBuffer);
        pBuffer = NULL;
    }
}
#endif

// See if there are any user defined MIME types that need to be added to the
// netlib list. These are stored in the Viewers section. Add them first so
// they're handled like the types in mktypes.h
static void
fe_UserDefinedFileTypes()
{
	for (int idx=0 ; ; idx++) {	
		char 	szBuf[20];
		
		sprintf(szBuf, "TYPE%d", idx);
		CString csType(theApp.GetProfileString("Viewers", szBuf));
		
		// XXX - we expect the list of user defined items to be contiguously numbered
		// starting at 0, and so we stop when we get back an empty value. We should
		// really handle the case where there are holes in the numbering, e.g. an
		// uninstaller removed one of the items...
		if (csType.IsEmpty())
			break;
     	
		// See whether the MIME type is already in the netlib list
		XP_List	   		*pTypesList = cinfo_MasterListPointer();
		NET_cdataStruct *pcdata;
		while ((pcdata = (NET_cdataStruct *)XP_ListNextObject(pTypesList)))	{
			if (pcdata->ci.type != NULL && (stricmp(pcdata->ci.type, (LPCSTR)csType) == 0))
				break;
		}

		if (!pcdata) {
			// Create a new netlib type
			pcdata = NET_cdataCreate();
			pcdata->ci.type = XP_STRDUP((LPCSTR)csType);

			// Add it to the netlib list
			NET_cdataAdd(pcdata);

			// Note: don't look in the Viewers section to see how the MIME type should be
			// configured. We'll do that last after checking the registry
		}

		if (pcdata) {
			// Look in the Suffixes section to get the list of extensions associated with
			// this MIME type
			fe_SetExtensionList(pcdata);
		}
	}
	
	theApp.m_iNumTypesInINIFile = idx;
}

//Begin CRN_MIME
NET_cdataStruct *
FileExtensionInNetlibList(const char *pExtension, const char *ccpMimeType)
{
	NET_cdataStruct *pExistingItem = NULL;

    //  Must have an extension to be considered, must not begin with a period.
    if(pExtension == NULL)  {
        return NULL;
    }
    else if(*pExtension == '.')  {
        ASSERT(0);
        return NULL;
    }

    // See if the extension is already in the netlib list. If it is AND it has a MIME
	// type then return the NET_cdataStruct pointer
    XP_List *pTypesList = cinfo_MasterListPointer();
    NET_cdataStruct *pListEntry = NULL;
    while ((pListEntry = (NET_cdataStruct *)XP_ListNextObject(pTypesList)))	{
        if(pListEntry->ci.type != NULL)	{
            for(int iFind = 0; iFind < pListEntry->num_exts; iFind++)	{
                if(stricmp(pExtension, pListEntry->exts[iFind]) == 0)	{
					// Remember this item
					pExistingItem = pListEntry;
                }
            }
        }
    }

	return pExistingItem;
}

const char *GetMimeCmd(int load_action)
{
	switch(load_action) 
	{
		case HANDLE_SAVE: 
			return MIME_SAVE;

		case HANDLE_SHELLEXECUTE: 
			return MIME_SHELLEXECUTE;

		case HANDLE_UNKNOWN:
			return MIME_SHELLEXECUTE;	//????? What should this be set to ???

		case HANDLE_VIA_PLUGIN:
			return MIME_SHELLEXECUTE;	//????? What should this be set to ???

		default:
			return MIME_SHELLEXECUTE;	//????? What should this be set to ???			
	}	
}

int GetPrefLoadAction(char * pPrefix)
{
	//Get load_action, if any
	int32 load_action;
	char szPref[512];

	wsprintf(szPref, "%s.load_action", pPrefix);
	int iRet = PREF_GetIntPref(szPref, &load_action);
	if(iRet == PREF_NOERROR)
	{
		switch(load_action) 
		{
			case 1: //Save
				iRet = HANDLE_SAVE;
				break;
			case 2: //Launch 
				iRet = HANDLE_SHELLEXECUTE;
				break;
			case 3://Unknown
				iRet = HANDLE_UNKNOWN;
				break;
			case 4://Plug-In
				iRet = HANDLE_VIA_PLUGIN;
				break;
			default:
				iRet = HANDLE_SHELLEXECUTE;
				break;
		}
	}	
	else
		iRet = HANDLE_SHELLEXECUTE;

	return iRet;
}

void fe_ChangeDescription(NET_cdataStruct *pcdata, LPCSTR lpszExt, LPCSTR lpszDescription)
{
	// See if the Description has changed
	if (strcmpi(pcdata->ci.desc, lpszDescription) != 0) {

       CString strClass;

		// Get the class name
		if (GetClassName(lpszExt, strClass) && !strClass.IsEmpty()) {
			// Set the description
			RegSetValue(HKEY_CLASSES_ROOT, strClass, REG_SZ, lpszDescription, lstrlen(lpszDescription));

			// Now go ahead and change the description in the netlib data structure
			StrAllocCopy(pcdata->ci.desc, lpszDescription);
		}
	}
}


void fe_UpdateMControlMimeTypes(void) 
{
	CString str;
	char * child_list;
	int bOK = PREF_NOERROR;
	char * type = NULL;
	char * ext = NULL;
	char * desc = NULL;
	char * open_cmd = NULL;


	if ( PREF_CreateChildList("mime", &child_list) == 0 )
	{	
		char szPref[512];
		int index = 0;
		char *child = NULL;
		while (child = PREF_NextChild(child_list, &index)) 
		{
			TRACE(child);TRACE("\r\n");	
			
			wsprintf(szPref, "%s.mimetype", child);
			bOK = PREF_CopyCharPref(szPref, &type);
			if (bOK == PREF_NOERROR) 
			{
				wsprintf(szPref, "%s.extension", child);
				bOK = PREF_CopyCharPref(szPref, &ext);
				if(bOK == PREF_NOERROR)
				{
					wsprintf(szPref, "%s.description", child);
					bOK = PREF_CopyCharPref(szPref, &desc);
					if(bOK == PREF_NOERROR)
					{
						wsprintf(szPref, "%s.win_appname", child);
						bOK = PREF_CopyCharPref(szPref, &open_cmd);
					}
				}
			}
			//Register new type only if all the fields are specified.
			if(type && type[0] && ext && ext[0] && desc && desc[0] && open_cmd && open_cmd[0])
			{
				//char szJunk[256];
				//wsprintf(szJunk, "%s - %s - %s - %s\r\n", (type && type[0]) ? type : "", (ext && ext[0]) ? ext : "", (desc && desc[0]) ? desc : "", (open_cmd && open_cmd[0]) ? open_cmd : "");
				//TRACE(szJunk);
				
				NET_cdataStruct *pcdata= NULL;

				int load_action = GetPrefLoadAction(child);

				//Check and see if we already have this file extension with the mime type
				pcdata = FileExtensionInNetlibList(ext, type);
				if(pcdata == NULL)
				{
					pcdata = fe_NewFileType(desc, ext, type, open_cmd);
					CHelperApp *pApp = (CHelperApp *)pcdata->ci.fe_data;
					if(pApp)
					{
						pApp->how_handle = load_action;
						pApp->csCmd = GetMimeCmd(load_action);
						pApp->csMimePrefPrefix = child;
					}
				}
				else
				{
					CHelperApp *pApp = (CHelperApp *)pcdata->ci.fe_data;
					if(pApp)
					{
						pApp->csMimePrefPrefix = child;
					}

					fe_ChangeFileType(pcdata, type, load_action, open_cmd);
					fe_ChangeDescription(pcdata, ext, desc);
				}
			}

			if(type && type[0])
			{
				XP_FREE(type);
				type = NULL;
			}
			if(ext && ext[0])
			{
				XP_FREE(ext);
				ext = NULL;
			}
			if(desc && desc[0])
			{
				XP_FREE(desc);
				desc = NULL;
			}
			if(open_cmd && open_cmd[0])
			{
				XP_FREE(open_cmd);
				open_cmd = NULL;
			}
		}
		XP_FREE(child_list);
	}

}
//End CRN_MIME

// This routine updates the netlib list of NET_cdataStruct objects with information
// found in the registry, WIN.INI file, and the Netscape specific information
// (the Viewers and Suffixes section)
void fe_InitFileFormatTypes(void) {
	// See if there are any user defined MIME types that need to be added to the
	// netlib list. These are stored in the Viewers section. Add them first so
	// they're handled like the types in mktypes.h
	//
	// This way when we look in the registry we'll find shell execute handlers for them
	fe_UserDefinedFileTypes();

    // This call is going to look at every file extension in the registry and 
    // update existing netlib structures that have matching file extenions and
    // matching MIME types. It will also create a new netlib structure if necessary
    registry_GenericFileTypes();

#ifndef _WIN32
    // This call is going to look at every file association in WIN.INI and 
    // update existing netlib structures that have matching file extenions. It
    // will also create a new netlib structure if necessary
    winini_GenericFileTypes();
#endif

	NET_cdataStruct *cd_item;
    XP_List   * infolist = cinfo_MasterListPointer();;  // Get beginning of the list

	// The last thing we do is use the Netscape specific information. This means looking
	// at the Viewers and Suffixes sections
	//
	// Do this for every entry in the netlib list
	while ((cd_item = (NET_cdataStruct *)XP_ListNextObject(infolist))) {  // iterate through the list  
		if (cd_item->ci.type) {  // if it is a mime type
			// Look in the Viewers section to see how the MIME type should be configured.
			// This allows us to override anything we found in the registry, e.g. user wants to
			// Save to disk or open as an OLE server
			fe_AddTypeToList(cd_item);

			// Look in the Suffixes section to get the list of extensions associated with
			// this MIME type
			fe_SetExtensionList(cd_item);
		}
	}
}

void fe_CleanupFileFormatTypes(void) 
{
	NET_cdataStruct *cd_item;
    XP_List   * infolist = cinfo_MasterListPointer();;  // Get beginning of the list
	CHelperApp * app;
		    
	while ((cd_item = (NET_cdataStruct *)XP_ListNextObject(infolist))) {  // iterate through the list  
		if (cd_item->ci.type) {  // if it is a mime type  
			app = (CHelperApp *)cd_item->ci.fe_data;
			
			if (app) {
				if (app->bChanged) 				
					theApp.WriteProfileString("Viewers",cd_item->ci.type,app->csCmd);
				delete app;
			}				
		}
	}
}
//
// Add a user defined mime type to our list of helper applications
//
CHelperApp * fe_AddNewFileFormatType(const char *mime_type,const char *subtype) 
{
	NET_cdataStruct * cd_item;
    XP_List   		* infolist = cinfo_MasterListPointer();;  // Get beginning of the list
	CString 		  csMime;

	if (!infolist) return NULL;
	cd_item = (NET_cdataStruct *)XP_ALLOC(sizeof(NET_cdataStruct));
	memset(cd_item,0,sizeof(NET_cdataStruct));
	csMime += mime_type;
	csMime += '/';
	csMime += subtype;
	cd_item->ci.type = XP_STRDUP((const char *)csMime);

	CHelperApp * app = new CHelperApp();
	app->bChanged = TRUE;
	app->bNewType = TRUE;
	app->bChangedExts = FALSE;	
	app->how_handle = HANDLE_UNKNOWN;
	app->cd_item = cd_item;
	app->csCmd = "";
	cd_item->ci.fe_data = app;  // store the helper app object

    // store in FE's list of types
	theApp.m_HelperListByType.SetAt(cd_item->ci.type, app);

    // tell the netlib about it
	XP_ListAddObjectToEnd(infolist,cd_item); 
	return app;
}

// Sets the shell open command string value for the given file class
void
SetShellOpenCommand(LPCSTR lpszFileClass, LPCSTR lpszCmdString)
{
	char	szKey[_MAX_PATH];
	HKEY	hKey;
	LONG	lResult;

	// Build the subkey string
	wsprintf(szKey, "%s\\shell\\open\\command", lpszFileClass);

	// Update the shell\open\command for the file type class
	lResult = RegCreateKey(HKEY_CLASSES_ROOT, szKey, &hKey);

	if (lResult == ERROR_SUCCESS) {
		RegSetValue(hKey, NULL, REG_SZ, lpszCmdString, lstrlen(lpszCmdString));
		RegCloseKey(hKey);
	}
}

#ifdef _WIN32
// We expect the extension to begin with '.'
static void
AddToMIMEDatabase(LPCSTR lpszType, LPCSTR lpszExt)
{
	char	szKey[_MAX_PATH];
	HKEY	hKey;

	PR_snprintf(szKey, sizeof(szKey), "MIME\\Database\\Content Type\\%s", lpszType);

	// What we're really doing is setting the default extension for the
	// content type
	//
	// Since we don't have a UI for specifying the default extension when creating
	// a new file type, only add this is there's nothing already there
	if (RegCreateKey(HKEY_CLASSES_ROOT, szKey, &hKey) == ERROR_SUCCESS) {
		DWORD	cbData;
		LONG	lResult;

		// See if there's anything there
		cbData = 0;
		lResult = RegQueryValueEx(hKey, "Extension", NULL, NULL, NULL, &cbData);

		if (lResult != ERROR_SUCCESS || cbData <= 1)
			RegSetValueEx(hKey, "Extension", 0, REG_SZ, (CONST BYTE *)lpszExt, lstrlen(lpszExt) + 1);

		// Close the key
		RegCloseKey(hKey);
	}
}
#endif

// Adds a new content type to the list of Netscape specific types, and adds lpszExtension
// to the list of extensions for the MIME type. lpszExtenion should have a leading '.'
static void
AddNetscapeMimeType(LPCSTR lpszMimeType, LPCSTR lpszExtension)
{
    char szBuf[20];
    
    // Add this to the list of user-defined MIME types
    sprintf(szBuf, "TYPE%d", theApp.m_iNumTypesInINIFile++);
    theApp.WriteProfileString("Viewers", szBuf, lpszMimeType);

	// Get the list of extensions for the MIME type
	CString strExtList(theApp.GetProfileString("Suffixes", lpszMimeType));

    ASSERT(lpszExtension && (*lpszExtension == '.'));

	strExtList.TrimLeft();
	strExtList.TrimRight();
	if (strExtList.IsEmpty()) {
		theApp.WriteProfileString("Suffixes", lpszMimeType, lpszExtension);

	} else if (!ExtensionIsInList(strExtList, lpszExtension + 1)) {
		// Add the extension to the end of the list
		strExtList += ',';
		strExtList += lpszExtension;
		theApp.WriteProfileString("Suffixes", lpszMimeType, (LPCSTR)strExtList);
	}
}

// Routine to add a new file type. Note that this routine is also called
// when the user is adding an additional MIME type to an existing file
// extension
NET_cdataStruct *
fe_NewFileType(LPCSTR lpszDescription,
			   LPCSTR lpszExtension,
			   LPCSTR lpszMimeType,
			   LPCSTR lpszOpenCmd)
{
	char	szExtKey[_MAX_PATH];
	CString	strFileClass;
	HKEY	hKey;
#ifdef _WIN32
    BOOL    bAlreadyHasMimeType;
#endif

	// Create a key for the file type extension. There may already be a key for
	// this extension; that's okay this will open it
	wsprintf(szExtKey, ".%s", lpszExtension);
	if (RegCreateKey(HKEY_CLASSES_ROOT, szExtKey, &hKey) != ERROR_SUCCESS)
        return NULL;

	// Set the file type class associated with the extension. Note that we ONLY
	// do this if there is no file type class. If there's already a file type class
	// we assume that we're just adding another MIME type for this file type
	if (!GetClassName(lpszExtension, strFileClass)) {
		// Create a file type class
		strFileClass = lpszExtension;
		strFileClass += "file";
	
		// Set the file type class
		RegSetValue(hKey, NULL, REG_SZ, (LPCSTR)strFileClass, strFileClass.GetLength());
	}

#ifdef _WIN32
	// Add the Content Type for the extension. Only do this if there isn't already a
	// content type specified for the extension. Note: unfortunately Microsoft only
	// allows a single content type for a particular extension. If there's already a
	// content type that's okay, we'll add the additional content type to the MIME database
	LONG	lResult;
	DWORD	cbData;

	ASSERT(lpszMimeType);
	cbData = 0;
	lResult = RegQueryValueEx(hKey, "Content Type", NULL, NULL, NULL, &cbData);
    bAlreadyHasMimeType = (lResult == ERROR_SUCCESS && cbData > 1);

	if (!bAlreadyHasMimeType)
		RegSetValueEx(hKey, "Content Type", 0, REG_SZ, (CONST BYTE *)lpszMimeType, lstrlen(lpszMimeType) + 1);
#endif
	RegCloseKey(hKey);

	// Create a key for the file type class. If there's already a key this will open
	// it
	RegCreateKey(HKEY_CLASSES_ROOT, (LPCSTR)strFileClass, &hKey);

	// Set the description
	RegSetValue(hKey, NULL, REG_SZ, lpszDescription, lstrlen(lpszDescription));
	RegCloseKey(hKey);

	// Add the shell Open command
	SetShellOpenCommand((LPCSTR)strFileClass, lpszOpenCmd);

	// Update the MIME database information
#ifdef _WIN32
	AddToMIMEDatabase(lpszMimeType, szExtKey);
	
	// If there's already a MIME type and we're adding an additional type, then
	// add it to the Netscape specific information in the Suffixes section
	if (bAlreadyHasMimeType)
		AddNetscapeMimeType(lpszMimeType, szExtKey);
#else
	AddNetscapeMimeType(lpszMimeType, szExtKey);
#endif

	// Create the NET_cdataStruct structure and associated CHelperApp object
    NET_cdataStruct *pcdata = NET_cdataCreate();

    if (pcdata) {
        pcdata->ci.desc = XP_STRDUP(lpszDescription);
        pcdata->ci.type = XP_STRDUP(lpszMimeType);
        pcdata->num_exts = 0;
        pcdata->exts = (char **)XP_ALLOC(sizeof(char *));
        if (pcdata->exts) {
            pcdata->exts[0] = XP_STRDUP(lpszExtension);
            
            if (pcdata->exts[0]) {
                pcdata->num_exts = 1;

            } else {
                XP_FREE(pcdata->exts);
                pcdata->exts = 0;
            }
        }

        // Add it to the netlib list
        NET_cdataAdd(pcdata);

        // Create the front-end data structure
        CHelperApp *pApp = new CHelperApp;

        if (pApp) {
			// Initialize the object
            pApp->bChanged = FALSE;
            pApp->bNewType = FALSE;
            pApp->bChangedExts = FALSE;
            pApp->how_handle = HANDLE_SHELLEXECUTE;
            pApp->csCmd = MIME_SHELLEXECUTE;
			pApp->strFileClass = strFileClass;

			// Front-end and netlib data structures need to point to each other
	        pApp->cd_item = pcdata;
	        pcdata->ci.fe_data = (void *)pApp;

			// Store it in the global list ordered by MIME type
	        theApp.m_HelperListByType.SetAt(pcdata->ci.type, pApp);
        }
    }

	return pcdata;
}

#ifdef _WIN32
// Routine to recursively delete a key with subkeys. Needed under Win NT only
LONG
RegDeleteKeyNT(HKEY hStartKey, LPCSTR lpszKeyName)
{
	HKEY    hKey;
	LONG	lResult;

	// Do not allow NULL or empty key name
	if (!lpszKeyName || *lpszKeyName == '\0')
		return ERROR_BADKEY;

	lResult = RegOpenKeyEx(hStartKey, lpszKeyName, 0, KEY_ENUMERATE_SUB_KEYS | DELETE, &hKey);
	if (lResult != ERROR_SUCCESS)
		return lResult;

	 while (lResult == ERROR_SUCCESS) {
		char		szName[256];
		DWORD		cbName = sizeof(szName);
		FILETIME	ftLastWriteTime;

		// Recursively delete each of the subkeys
		lResult = RegEnumKeyEx(hKey, 0, szName, &cbName, NULL, NULL, NULL, &ftLastWriteTime);

		if (lResult == ERROR_SUCCESS)
			lResult = RegDeleteKeyNT(hKey, szName);
	 }

	 RegCloseKey(hKey);
	 return RegDeleteKey(hStartKey, lpszKeyName);
} 
#endif

// Returns the count of the number of NET_cdataStruct structures that have
// lpszFileClass as their file type class
static int
OccurencesOfFileClass(LPCSTR lpszFileClass)
{
    XP_List 		*pTypesList = cinfo_MasterListPointer();
    NET_cdataStruct *pcdata = NULL;
	int				 nResult = 0;

    while ((pcdata = (NET_cdataStruct *)XP_ListNextObject(pTypesList)))	{
		if (pcdata->ci.fe_data) {
			CHelperApp *pHelperApp = (CHelperApp *)pcdata->ci.fe_data;

			if (pHelperApp->strFileClass == lpszFileClass)
				nResult++;
		}
    }

	return nResult;
}

// Removes a file type
BOOL
fe_RemoveFileType(NET_cdataStruct *pcdata)
{
	CHelperApp	*pHelperApp = (CHelperApp *)pcdata->ci.fe_data;
	
	if (pHelperApp && (pHelperApp->how_handle == HANDLE_SHELLEXECUTE || pHelperApp->how_handle == HANDLE_BY_OLE)) {
		// Remove the registry keys
		for (int i = 0; i < pcdata->num_exts; i++) {
			CString	strClass;
	
			// Get the file type class
			if (GetClassName(pcdata->exts[i], strClass)) {
				// Remove the subkey for the file type class. Don't delete the file type class
				// unless this is the only netlib entry that has this file type class. The
				// reason for this is that we arrange items by MIME type and not by file type class
				// like Windows does. The user is asking to remove a particular MIME type and
				// it's associated extensions; only remove the file type class if this is the only
				// item with that file type class
				ASSERT(pHelperApp->strFileClass == strClass);
				if (OccurencesOfFileClass((LPCSTR)strClass) <= 1) {
	#ifdef _WIN32
					RegDeleteKeyNT(HKEY_CLASSES_ROOT, (LPCSTR)strClass);
	#else
					RegDeleteKey(HKEY_CLASSES_ROOT, (LPCSTR)strClass);
	#endif
				}
			}
	
			// Remove the key for the file type extension
			char	szBuf[_MAX_PATH];
	
			PR_snprintf(szBuf, sizeof(szBuf), ".%s", pcdata->exts[i]);
	#ifdef _WIN32
			RegDeleteKeyNT(HKEY_CLASSES_ROOT, szBuf);
	#else
			RegDeleteKey(HKEY_CLASSES_ROOT, szBuf);
	#endif
	
	#ifdef _WIN32
			// Clean up the MIME database. Only remove this key if the default
			// extension matches this extension
			if (pcdata->ci.type) {
				HKEY	hKey;
				LONG	lResult;
	
				PR_snprintf(szBuf, sizeof(szBuf), "MIME\\Database\\Content Type\\%s",
					pcdata->ci.type);
	
				if (RegOpenKeyEx(HKEY_CLASSES_ROOT, szBuf, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
					BOOL	bDeleteKey = FALSE;
					DWORD	cbData;
					char	szDefaultExt[_MAX_EXT];
	
					// Get the extension associated with this MIME type
					cbData = sizeof(szDefaultExt);
					lResult = RegQueryValueEx(hKey, "Extension", NULL, NULL, (LPBYTE)szDefaultExt, &cbData);
					if (lResult == ERROR_SUCCESS && cbData > 2) {
						// Only delete the key if the extension matches one of the extensions in our list
						bDeleteKey = ExtensionIsInList(pcdata, &szDefaultExt[1]);
					}
	
					RegCloseKey(hKey);
	
					if (bDeleteKey)
						RegDeleteKeyNT(HKEY_CLASSES_ROOT, szBuf);
				}
			}
	#endif
		}
	}

	// Cleanup up any Netscape specific associations
	if (pcdata->ci.type) {
		theApp.WriteProfileString("Suffixes", pcdata->ci.type, NULL);
		theApp.WriteProfileString("Viewers", pcdata->ci.type, NULL);
	}
	
	// Clean up the FE data structures
	if (pHelperApp) {
		// Remove the helper app from the global list
		if (pcdata->ci.type)
			theApp.m_HelperListByType.RemoveKey(pcdata->ci.type);

		delete pHelperApp;
		pcdata->ci.fe_data = NULL;
	}

	// Remove the item from the cdata list. This frees it as well
	NET_cdataRemove(pcdata);
	return TRUE;
}

static void
ChangeMIMEType(NET_cdataStruct *pcdata, LPCSTR lpszMimeType, int nHowToHandle)
{
	// We need to check both the Netscape specific information, and we also
	// need to check the registry. First check the Netscape specific information
    CString strValue;
	
    // First thing to handle is the Viewers section.  See if there's an application
    // associated with this MIME type
	strValue = theApp.GetProfileString("Viewers", pcdata->ci.type, NULL);
	if (!strValue.IsEmpty()) {
		// Rename the key by removing the old key and creating a new key with the same value
		theApp.WriteProfileString("Viewers", pcdata->ci.type, NULL);
		theApp.WriteProfileString("Viewers", lpszMimeType, (LPCSTR)strValue);
	}

	// Check if there is a "TYPE%d" value associated with this MIME type
	for (int i = 0; i < theApp.m_iNumTypesInINIFile; i++) {
		char szBuf[20];

		sprintf(szBuf, "TYPE%d" ,i);
		strValue = theApp.GetProfileString("Viewers", szBuf, NULL);
		if (strValue.CompareNoCase(pcdata->ci.type) == 0) {
			// Rename the key by removing the old key and creating a new key with the same value
			theApp.WriteProfileString("Viewers", szBuf, NULL);
			theApp.WriteProfileString("Viewers", szBuf, lpszMimeType);
			break;
		}
	}
	
    // See if there are file extensions specified for the type
	strValue = theApp.GetProfileString("Suffixes", pcdata->ci.type, NULL);
	if (!strValue.IsEmpty()) {
		// Rename the key by removing the old key and creating a new key with the same value
		theApp.WriteProfileString("Suffixes", pcdata->ci.type, NULL);
		theApp.WriteProfileString("Suffixes", lpszMimeType, (LPCSTR)strValue);
	}

#ifdef _WIN32
	char	szKey[_MAX_PATH];
	HKEY	hKey;
	LONG	lResult;

	// Check the registry. We need to check the MIME database, and the Content Type associated
	// with each of the file extensions
	PR_snprintf(szKey, sizeof(szKey), "MIME\\Database\\Content Type\\%s", pcdata->ci.type);

	// Get the extension that's associated with this MIME type
	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, szKey, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
		BOOL	bDeleteKey = FALSE;
		DWORD	cbData;
		char	szDefaultExt[_MAX_EXT];

		// Get the key value
		cbData = sizeof(szDefaultExt);
		lResult = RegQueryValueEx(hKey, "Extension", NULL, NULL, (LPBYTE)szDefaultExt, &cbData);
		RegCloseKey(hKey);
		
		if (lResult == ERROR_SUCCESS && cbData > 1) {
			// Only rename the key if the extension matches one of the extensions in our list
			if (ExtensionIsInList(pcdata, &szDefaultExt[1])) {
				// Rename the key by deleting it and creating a new key
				RegDeleteKey(HKEY_CLASSES_ROOT, szKey);
				AddToMIMEDatabase(lpszMimeType, szDefaultExt);
			}
		}
	
	} else {
		char	szDefaultExt[_MAX_EXT];
		
		// There's no existing MIME type entry so go ahead and create a new key. Use
		// the first extension in the list
		wsprintf(szDefaultExt, ".%s", pcdata->exts[0]);
		AddToMIMEDatabase(lpszMimeType, szDefaultExt);
	}
	
	// Rename the Content Type values associated with the extensions
	for (i = 0; i < pcdata->num_exts; i++) {
		char	szKey[_MAX_EXT];
		HKEY	hKey;
		LONG	lResult;

		// Build the file association key. It must begin with a '.'
		wsprintf(szKey, ".%s", pcdata->exts[i]);

		// Open the key for the file extension
		lResult = RegOpenKeyEx(HKEY_CLASSES_ROOT, szKey, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &hKey);
		if (lResult == ERROR_SUCCESS) {
			char	szContentType[256];
			DWORD	cbData;
			DWORD	dwType;

			// Only change the MIME type for the extension if the MIME type that's currently
			// there matches
			cbData = sizeof(szContentType);
			lResult = RegQueryValueEx(hKey, "Content Type", NULL, &dwType, (LPBYTE)szContentType, &cbData);
			if (lResult == ERROR_SUCCESS && cbData > 1) {
				if (lstrcmpi(szContentType, pcdata->ci.type) == 0) {
					// Change the MIME type
					RegSetValueEx(hKey, "Content Type", 0, REG_SZ, (CONST BYTE *)lpszMimeType, lstrlen(lpszMimeType) + 1);
				}

			} else {
				// There is no existing MIME type, so go ahead and add one
				RegSetValueEx(hKey, "Content Type", 0, REG_SZ, (CONST BYTE *)lpszMimeType, lstrlen(lpszMimeType) + 1);
			}

			RegCloseKey(hKey);
		}
	}
#endif

    // Update the FE list ordered by MIME type
    theApp.m_HelperListByType.RemoveKey(pcdata->ci.type);
	theApp.m_HelperListByType.SetAt(lpszMimeType, (CHelperApp *)pcdata->ci.fe_data);
	
    // Now go ahead and change the MIME type in the netlib data structure
	StrAllocCopy(pcdata->ci.type, lpszMimeType);
}

BOOL
fe_ChangeFileType(NET_cdataStruct *pcdata, LPCSTR lpszMimeType, int nHowToHandle, LPCSTR lpszOpenCmd)
{
	CHelperApp *pHelperApp = (CHelperApp *)pcdata->ci.fe_data;
    CString     strFileClass;

    // See if the MIME type changed
	if (strcmpi(pcdata->ci.type, lpszMimeType) != 0) {
		ChangeMIMEType(pcdata, lpszMimeType, nHowToHandle);
	}

    // See if how we handle it changed
    if (pHelperApp->how_handle != nHowToHandle) {
        pHelperApp->how_handle = nHowToHandle;

        if (pcdata->ci.type) {
            // Almost everything is stored in the registry now except for
            // the special types that Netscape can handle internally
            if (strcmp(pcdata->ci.type, IMAGE_GIF) == 0) {
                if (pHelperApp->how_handle == HANDLE_VIA_NETSCAPE)
                    NET_RegisterContentTypeConverter(IMAGE_GIF, FO_PRESENT, NULL, IL_ViewStream);
                else {
					if (pHelperApp->how_handle == HANDLE_SHELLEXECUTE)
						pHelperApp->how_handle = HANDLE_EXTERNAL;  // save in Netscape specific information
					NET_RegisterContentTypeConverter(IMAGE_GIF, FO_PRESENT, NULL , external_viewer_disk_stream);
				}
			}

			if (strcmp(pcdata->ci.type, IMAGE_JPG) == 0) {
				if (pHelperApp->how_handle == HANDLE_VIA_NETSCAPE)
					NET_RegisterContentTypeConverter(IMAGE_JPG, FO_PRESENT, NULL, IL_ViewStream);
				else {
					if (pHelperApp->how_handle == HANDLE_SHELLEXECUTE)
						pHelperApp->how_handle = HANDLE_EXTERNAL;  // save in Netscape specific information
					NET_RegisterContentTypeConverter(IMAGE_JPG, FO_PRESENT, NULL , external_viewer_disk_stream);
				}
			}
        
			if (strcmp(pcdata->ci.type, IMAGE_XBM) == 0) {
				if (pHelperApp->how_handle == HANDLE_VIA_NETSCAPE)
					NET_RegisterContentTypeConverter(IMAGE_XBM, FO_PRESENT, NULL, IL_ViewStream);
				else {
					if (pHelperApp->how_handle == HANDLE_SHELLEXECUTE)
						pHelperApp->how_handle = HANDLE_EXTERNAL;  // save in Netscape specific information
					NET_RegisterContentTypeConverter(IMAGE_XBM, FO_PRESENT, NULL , external_viewer_disk_stream);
				}
			}
		}

		// Make sure we don't leave old Netscape specific information lying around. Note that
		// for types that Netscape can handle internally it's assumed we're handling it unless
		// it's marked as HANDLE_EXTERNAL in the Netscape specific information
		if (pHelperApp->how_handle == HANDLE_EXTERNAL || pHelperApp->how_handle == HANDLE_SAVE) {
			// This gets saved in fe_CleanupFileFormatTypes()
			if (pHelperApp->how_handle == HANDLE_SAVE)
				pHelperApp->csCmd = MIME_SAVE;
			pHelperApp->bChanged = TRUE;
		
		} else {
			// Remove any existing entry for this MIME type. If the MIME type isn't listed in the
			// Viewers section, then we use the default behavior which is HANDLE_SHELLEXECUTE
            theApp.WriteProfileString("Viewers", pcdata->ci.type, NULL);
		}
	}

	// See if the Open command has changed
	switch (pHelperApp->how_handle) {
		case HANDLE_EXTERNAL:
			if (pHelperApp->csCmd.Compare(lpszOpenCmd) != 0) {
				pHelperApp->csCmd = lpszOpenCmd;

				// Update the Netscape specific association
				if (pcdata->ci.type)
					theApp.WriteProfileString("Viewers", pcdata->ci.type, lpszOpenCmd);
			}
			break;

		case HANDLE_SHELLEXECUTE:
			if (GetClassName(pcdata->exts[0], strFileClass) && !strFileClass.IsEmpty())
				SetShellOpenCommand(strFileClass, lpszOpenCmd);
			break;

		case HANDLE_BY_OLE:
			fe_SetHandleByOLE(pcdata->ci.type, pHelperApp, TRUE);
			break;
		default:
			break;
	}
    return TRUE;
}

// exts - Array of Pointers of extension.
// numofExt - number of extension in this array.
// return - TRUE if these extension can be handle by OLE, otherwise FALSE.
BOOL fe_CanHandleByOLE(char** exts, short numOfExt)
{
	char aExtension[MAX_PATH + 1];
	BOOL canHandleByOLE = FALSE;
    HKEY    hkSubCLSID ;
    HKEY    hTempKey;
    LONG    cb;
    TCHAR	szName[MAX_PATH + 1];
    TCHAR	 szBuf[MAX_PATH + 1];
    TCHAR	 szTempCLSID[MAX_PATH + 1];

    cb = sizeof(szName);
	
	for (int i = 0; i < numOfExt; i++) {
		strcpy(aExtension, exts[i]);  // reset for next extension.
        if ((RegQueryValue( HKEY_CLASSES_ROOT, (LPTSTR)aExtension, szName, &cb) == ERROR_SUCCESS) &&
            (RegOpenKey( HKEY_CLASSES_ROOT, szName, &hkSubCLSID ) == ERROR_SUCCESS)) {
		    cb = sizeof(szName);
			// get the clsid key for this extension.
            if (RegQueryValue(hkSubCLSID, "CLSID", szTempCLSID, &cb) == ERROR_SUCCESS) {
                wsprintf( szBuf, _T("CLSID\\%s\\Insertable"), (LPTSTR)szTempCLSID) ;
                if (RegOpenKey( HKEY_CLASSES_ROOT, szBuf, &hTempKey ) == ERROR_SUCCESS)	{
					canHandleByOLE = TRUE;
                    RegCloseKey( hTempKey ) ;
                }
            }
            RegCloseKey( hkSubCLSID ) ;
        }
	}
	return canHandleByOLE;
}


BOOL fe_SetHandleByOLE(char* mimeType, CHelperApp* app, BOOL handleByOLE)
{
	if (handleByOLE) {
		app->how_handle = HANDLE_BY_OLE;
		app->csCmd = MIME_OLE;
		return theApp.WriteProfileString("Viewers", mimeType, MIME_OLE);
	}
	else {
		app->how_handle = HANDLE_SHELLEXECUTE;
		app->csCmd = MIME_SHELLEXECUTE;
		return theApp.WriteProfileString("Viewers", mimeType, NULL);
	}
}

BOOL fe_IsHandleByOLE(char* mimeType)
{
	CString csCmd = theApp.GetProfileString("Viewers", mimeType);  
	if (csCmd.Compare( MIME_OLE))
		return FALSE;
	else return TRUE;
}

#ifdef XP_WIN32
//This function is taken from the PE file Profile.cpp

BOOL  CopyRegKeys(HKEY  hKeyOldName,
				   HKEY  hKeyNewName,
				   DWORD subkeys,
				   DWORD maxSubKeyLen,
				   DWORD maxClassLen,
				   DWORD values,
				   DWORD maxValueNameLen,
				   DWORD maxValueLen,
				   const char *OldPath,
				   const char *NewPath)
{

	BOOL Err = FALSE;
	DWORD index;


	// first loop through and copies all the value keys

	if (values > 0) {

		DWORD valueNameSize = maxValueNameLen + 1;
		char *valueName = (char *)malloc(sizeof(char) * valueNameSize);
		DWORD dataSize = maxValueLen + 1;
		unsigned char *data = (unsigned char *)malloc(sizeof(char) * dataSize);
		DWORD type;

		if ((valueName) && (data)) {

			for (index=0; index<values; index++) {

				// gets each value name and its data
				if (ERROR_SUCCESS == RegEnumValue(hKeyOldName, index, valueName, &valueNameSize, NULL, &type, data, &dataSize)) {
				
					// create value in our new profile key
					if (ERROR_SUCCESS != RegSetValueEx(hKeyNewName, valueName, 0, type, data, dataSize)) {
						// unknown err occured... what do we do?
						Err = TRUE;
						return FALSE;
					}
				}

				valueNameSize = maxValueNameLen + 1;
				dataSize = maxValueLen + 1;
			}

			free(valueName);
			free(data);
		}
	}


	// next, we need to creates subkey folders

	if (subkeys > 0) {

		char OldSubkeyPath[260];
		char NewSubkeyPath[260];
		HKEY hkeyOldSubkey;
		HKEY hkeyNewSubkey;


		for (index=0; index<subkeys; index++) {

			DWORD subkeyNameSize = maxSubKeyLen + 1;
			char *subkeyName = (char *)malloc(sizeof(char) * subkeyNameSize);

			if (subkeyName) {

				// gets each subkey name
				if (ERROR_SUCCESS == RegEnumKey(hKeyOldName, index, subkeyName, subkeyNameSize)) {
				
					// create subkey in our new profile keypath
					if (ERROR_SUCCESS != RegCreateKey(hKeyNewName, subkeyName, &hkeyNewSubkey))  {
						Err = TRUE;
						return FALSE;
					}

					// now the subkey is created, we need to get hkey value for oldpath and 
					// then copy everything in the keys again

					// construct key path
					strcpy((char *)OldSubkeyPath, OldPath);
					strcat((char *)OldSubkeyPath, "\\");
					strcat((char *)OldSubkeyPath, subkeyName);

					strcpy((char *)NewSubkeyPath, NewPath);
					strcat((char *)NewSubkeyPath, "\\");
					strcat((char *)NewSubkeyPath, subkeyName);

					free(subkeyName);


					// now... try to start the copying process for our subkey here
					// first, gets the hkey for the old subkey profile path
					if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, (char *)OldSubkeyPath, NULL, KEY_ALL_ACCESS, &hkeyOldSubkey)) {
			
						DWORD SubKeys;
						DWORD MaxSubKeyLen;
						DWORD MaxClassLen;
						DWORD Values;
						DWORD MaxValueNameLen;
						DWORD MaxValueLen;
						DWORD SecurityDescriptor;
						FILETIME LastWriteTime;

						// get some information about this old profile subkey 
						if (ERROR_SUCCESS == RegQueryInfoKey(hkeyOldSubkey, NULL, NULL, NULL, &SubKeys, &MaxSubKeyLen, &MaxClassLen, &Values, &MaxValueNameLen, &MaxValueLen, &SecurityDescriptor, &LastWriteTime)) {

							// copy the values & key stuff

							Err = CopyRegKeys(hkeyOldSubkey, hkeyNewSubkey, SubKeys, MaxSubKeyLen, MaxClassLen, Values, MaxValueNameLen, MaxValueLen, (char *)OldSubkeyPath, (char *)NewSubkeyPath);

							if (!Err)
								return FALSE;

						}

						RegCloseKey(hkeyOldSubkey);
					}
				}
			}
		}
	}

	return TRUE;
}

#endif
