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

#include "npapi.h"
#include "np.h"
#include "npassoc.h"
#include "edview.h"
#include "prefapi.h"
#include "hiddenfr.h"
#include "extgen.h"

#define ASYNC_DNS 
#ifndef _AFXDLL
#define new DEBUG_NEW  // MSVC Debugging new...goes to regular new in release mode
#endif


CMapStringToOb DNSCacheMap(sizeof(CDNSObj));
extern char *FE_FindFileExt(char * path);

/* Remove all objects from the dns cache map */
PUBLIC void FE_DeleteDNSList(MWContext * currentContext)
{
	POSITION pos;
	CString key;
	CDNSObj *obj = NULL;

	/* Get the starting position. Actually, whether the position returned is actually the start
	   is irrelevant. */
	pos = DNSCacheMap.GetStartPosition();
	while (pos)
	{
		/* GetNextAssoc sets pos to another position in the map. Again, we don't care where
		   we just want to know that another position exists so we can continue removing objs. */
		DNSCacheMap.GetNextAssoc(pos, key, (CObject *&)obj);
		
		/* Only delete from the list if the current context owns the particular lookup. */
		if(obj->context == currentContext)
		{
			/* Only delete the dns object if there are no
			more sockets in the sock list */
			if (XP_ListCount(obj->m_sock_list) == 0) {
				DNSCacheMap.RemoveKey(key);
				delete obj;
			}
		}
	}
}

int FE_AsyncDNSLookup(MWContext *context, char * host_port, PRHostEnt ** hoststruct_ptr_ptr, PRFileDesc *socket)
{
    //  Initialize our DNS objects.
    CDNSObj * dns_obj=NULL;
    *hoststruct_ptr_ptr = NULL;

    //  Look up and see if the host is in our cached DNS lookups and if so....
    if (DNSCacheMap.Lookup(host_port,(CObject *&)dns_obj)) {
		//  See if the cached value was an error, and if so, return an error.
        //  Should we retry the lookup if it was?
		if (dns_obj->m_iError != 0 && dns_obj->i_finished != 0) {  // DNS error
            WSASetLastError(dns_obj->m_iError);
            DNSCacheMap.RemoveKey(host_port);
			// Only delete the dns object if there are no
			// more sockets in the sock list
            if (dns_obj && XP_ListCount(dns_obj->m_sock_list) == 0) 
				delete dns_obj;

            return -1;
        }
        //  If this isn't NULL, then we have a good match.  Return it as such and clear the context.
        else if (dns_obj->m_hostent->h_name != NULL && dns_obj->i_finished != 0) {
            *hoststruct_ptr_ptr = (PRHostEnt *)dns_obj->m_hostent;
            return 0;
        }
        //  There isn't an error, and there isn't a host name, return that we are waiting for the
        //      lookup....  This doesn't make clear sense, unless you take into account that the
        //      hostent structure is empty when another window is also looking this same host up,
        //      so we return that we are waiting too.
        else {
			/* see if the socket we are looking up is already in
			 * the socket list.  If it isn't, add it.
			 * The socket list provides us with a way to
			 * send events to NET_ProcessNet for each socket
			 * waiting for a lookup
			 */
			XP_List * list_ptr = dns_obj->m_sock_list;
			PRFileDesc *tmp_sock;

			if(list_ptr)
				list_ptr = list_ptr->next;

			while(list_ptr) {
				tmp_sock = (PRFileDesc *) list_ptr->object;

				if(tmp_sock == socket)
					return MK_WAITING_FOR_LOOKUP;

				list_ptr = list_ptr->next;
			}

			/* socket not in the list, add it */
			XP_ListAddObject(dns_obj->m_sock_list, (void *) socket);
			return MK_WAITING_FOR_LOOKUP;
        }   
    } else {
		//  There is no cache entry, begin the async dns lookup.
		//  Capture the current window handle, we pass this to the async code, for passing
		//      back a message when the lookup is complete.
		//  Actually, just pass the application's main window to avoid
		//      needing a frame window to do DNS lookups.
		HWND hWndFrame = AfxGetApp()->m_pMainWnd->m_hWnd;
    
		//  Create and initialize our dns object.  Allocating hostent struct, host name and port string,
		//      and error code from the lookup.
		dns_obj = new CDNSObj();
		dns_obj->m_hostent = (struct hostent *)XP_ALLOC(MAXGETHOSTSTRUCT * sizeof(char));
		if(dns_obj->m_hostent) {
		    memset(dns_obj->m_hostent, 0, MAXGETHOSTSTRUCT * sizeof(char));
		}
        dns_obj->m_host = XP_STRDUP(host_port);
		dns_obj->m_sock_list = XP_ListNew();
		dns_obj->context = context;
		XP_ListAddObject(dns_obj->m_sock_list, (void *) socket);

		//  Insert the entry into the cached DNS lookups.
		//  Also, set the context, and actually begin the DNS lookup.
		//  Return that we're waiting for it to complete.

		if( !(dns_obj->m_handle = WSAAsyncGetHostByName(hWndFrame,
					msg_FoundDNS,
					dns_obj->m_host,
					(char *)dns_obj->m_hostent,
					MAXGETHOSTSTRUCT) ) ) {
			delete dns_obj;
			return -1;
		}

		DNSCacheMap.SetAt(host_port,dns_obj);

		return MK_WAITING_FOR_LOOKUP;
	}
}

CDNSObj::CDNSObj()
{
    //  make sure we're clean.
    m_hostent = NULL;
    m_host = NULL;
    m_handle = NULL;
	context = NULL;
    m_iError = 0;
    i_finished = 0;
    m_sock_list = 0;
}

CDNSObj::~CDNSObj()
{
    if(m_hostent)   {
        XP_FREE(m_hostent);
        m_hostent = NULL;
    }
    if(m_host)  {
        XP_FREE(m_host);
        m_host = NULL;
    }

	if (m_sock_list) {
		while (XP_ListRemoveTopObject(m_sock_list))
			;
		XP_ListDestroy(m_sock_list);
	}
}


#ifdef DEBUG
char * 
NOT_NULL (const char *x) 
{
    if(!x) {
        TRACE("Panic! -- An assert of NOT_NULL is about to fail\n");    
    }
    ASSERT(x);
    return (char *)x;
}
#endif

// converts a URL to a local (8+3) filename
// the return must be freed by caller

char * fe_URLtoLocalName(const char * url, const char *pMimeType) 
{
    CString *csURL, csExt,csName;

    //  Determine the extension.
    char aExt[_MAX_EXT];
    size_t stExt = 0;
    DWORD dwFlags = 0;
    
#ifdef XP_WIN16
    dwFlags = EXT_DOT_THREE;
#endif
    aExt[0] = '\0';
    stExt = EXT_Invent(aExt, sizeof(aExt), dwFlags, url, pMimeType);
    csExt = aExt;
    
    int idx;
    csURL = new CString(url);
    int iQuestionMark = csURL->Find('?');
    int iPound = csURL->Find('#');
    int iEnd;

    if ((iQuestionMark > 0) && (iPound > 0))    // wow both!
        *csURL = csURL->Left(min(iQuestionMark,iPound));
    else if ((iQuestionMark > 0) || (iPound > 0)) {  // 1 or the other
        if (iQuestionMark > 0)
            iEnd = iQuestionMark;
        else if (iPound > 0)
            iEnd = iPound;
        *csURL = csURL->Left(iEnd);         
    }

    int iDot = csURL->ReverseFind('.');
    int iSlash = csURL->ReverseFind('/');

    if (iSlash > 0) {
        if (iDot < iSlash) {
            if( iSlash == csURL->GetLength() -1 ){
                // CLM: If here, there was no filename after last slash
                //      so lets try to make a name from end of URL path
                *csURL = csURL->Left(iSlash);
                iSlash = csURL->ReverseFind('/');
            }
            if (iSlash > 0) {
                csName = csURL->Right(csURL->GetLength() - iSlash -1);
            }
            iDot = 0;
        }
        else csName = csURL->Mid(iSlash+1, iDot-iSlash-1);
    }
    
    //  Should we really return NULL?
    //  There will be cases that don't fit, and does this mean that they won't be able to save a file
    //      locally since a name can't be formulated?
    if (csName.IsEmpty())   {
        delete(csURL);
        return NULL;
    }

#ifdef XP_WIN16 
    csName = csName.Left(8);
#endif
    char *name = csName.GetBuffer(0);

    // replace extra periods in 8 character name with underscores   
    for (idx =0 ; idx < csName.GetLength(); idx++) {
        if ((name[idx] == '.')||(name[idx] == ':')) name[idx] = '_';
    }
    csName.ReleaseBuffer(-1);
    
    *csURL = csName + csExt;
    
    char *cp_retval = strdup((const char *)*csURL);
    delete csURL;
    return(cp_retval);
}

char* FE_URLToLocalName( char *pStr ){
    return fe_URLtoLocalName( (const char*)pStr, NULL);
}

#ifdef XP_WIN16

PUBLIC void * WIN16_malloc(unsigned long size)
{

    if((size <= 0) || (size > 64000)) {
        TRACE("WIN16_malloc() FUN FUN FUN %ld\n", size); 
        return(NULL);
    }           
    
    void *Ret = malloc((size_t) size);
    
    if (!Ret) 
        TRACE("WIN16_malloc() failed to alloc %ld bytes\n", size);  
    
    return(Ret);

}

PUBLIC void * WIN16_realloc(void * ptr, unsigned long size)
{

    if((size <= 0) || (size > 64000)) {
        TRACE("WIN16_realloc() FUN FUN FUN %ld\n", size);
        return(NULL);
    }           

    void * Ret = realloc(ptr, (size_t) size);
    
    if (!Ret) 
        TRACE("WIN16_realloc() failed to realloc %ld bytes\n", size);   
    
    return(Ret);

}

PUBLIC void WIN16_bcopy(char * from_ptr, char * to_ptr, unsigned long size)
{

    if((!to_ptr) || (size > 64000)) {
        TRACE("WIN16_bcopy() FUN FUN FUN %ld\n", size);
        return;
    }           
        
	memmove(to_ptr, from_ptr, (size_t) size);
}


#endif /* XP_WIN16 */


//
// Add a temporary file to the list of files that get deleted on exit
//
void FE_DeleteFileOnExit(const char *pFilename, const char *pURL)
{
    //  Remember the information in the INI file.
    CString csFileName = pFilename;
    csFileName.MakeLower();
    theApp.WriteProfileString("Temporary File URL Resolution", csFileName, pURL);
}

//
// We are exiting now.  Clean up any temp files we have generated
//
void FE_DeleteTempFilesNow()
{
    char *pBuffer = new char[8192];
    theApp.GetPrivateProfileString("Temporary File URL Resolution", NULL, "", pBuffer, 8192, AfxGetApp()->m_pszProfileName);

    //  We have a double null terminated list of file names here.
    //  Go ahead and go through each one, deleteing them all.
    char *pTraverse = pBuffer;
    int iRemoveResult = 0;
    while(*pTraverse != '\0')   {
        //  Remove this file.
        iRemoveResult = remove(pTraverse);
        ASSERT(!iRemoveResult);

        //  Go on to the next entry.
        while(*pTraverse != '\0')   {
            pTraverse++;
        }
        pTraverse++;
    }

	delete[] pBuffer;

    //  Clear out the section in the INI file also.
#ifdef XP_WIN16
    ::WritePrivateProfileString("Temporary File URL Resolution", NULL, NULL, AfxGetApp()->m_pszProfileName);
#else
    //  To clear them out in the Registry, use RegDeleteKey.
    //  Create the appropriate key.
    HKEY hKey;
    DWORD dwDisposition;
    char aBuffer[256];
    sprintf(aBuffer, "Software\\%s\\%s", "Netscape", "Netscape Navigator");
    long lResult = RegCreateKeyEx(HKEY_CURRENT_USER, aBuffer, NULL, NULL, NULL,
        KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
    if(lResult != ERROR_SUCCESS)    {
        return;
    }

    RegDeleteKey(hKey, "Temporary File URL Resolution");
    RegCloseKey(hKey);
#endif
}

//
// Since netlib is too stubborn to give us a flush cache function do it
//   by hand
//
PUBLIC void
FE_FlushCache()
{
	int32 prefInt;

    NET_SetMemoryCacheSize(0);
	PREF_GetIntPref("browser.cache.memory_cache_size",&prefInt);
    NET_SetMemoryCacheSize(prefInt * 1024);
}

void CheckLegalFileName(char* full_path)
{
	CString temp = full_path;
#ifdef XP_WIN16
	static char funnychar[13] = "\\/:*?\"<>| ,+";
#else
	static char funnychar[10] = "\\/:*?\"<>|";
#endif
	int index;
	while ((index = temp.FindOneOf(funnychar)) != -1) {
		temp.SetAt(index, '_'); // replace the illegal char with space.
	}
	strcpy(full_path, temp);
}

int SetCurrentDir(char * pDir)
{
    if( pDir ){
#ifdef XP_WIN16
    // We need to double "\" characters when setting directory
        char dir[2*_MAX_PATH];
        char * pSource = pDir;
        char * pDest = dir;
        while( *pSource ){
            *pDest++ = *pSource;
            if( *pSource == '\\' ){
                *pDest++ = '\\';
            }
            pSource++;
        }
        *pDest = '\0';
        // This is SLOW ( > 5 sec) on Win95!
        return _chdir(dir);
#else
        BOOL bRet = SetCurrentDirectory(pDir);
        return bRet ? 0 : -1;
#endif // XP_WIN16
    }
    return -1;
}

//
// Get a save file name from the user
// If caller has an idea for the name it should be passed in initial else
//   initial should be NULL
// Return a pointer (to be freed by callee) of full path name else NULL if
//   the selection was canceled
// It is up to the user to free the filename
//
//CLM: Why the ?#$% is type passed in as a pointer???
// If pDocumentTitle is not null, then edit box is added to allow user to enter document title for edited documents

MODULE_PRIVATE char * 
wfe_GetSaveFileName(HWND m_hWnd, char * prompt, char * initial, int * type, char** ppPageTitle)
{

	OPENFILENAME fname;
	char * full_path;
	char   name[2 * _MAX_FNAME];
    char   filter[256];
    char * extension = NULL;
	char * pOldExt = NULL;
	int    index = 1;
    Bool   bAppendHTML_Ext = FALSE;
    BOOL   bHtmlOnly = FALSE;
    char aIExt[_MAX_EXT];

	// We restrict saving type to HTML in the editor
    CString htm_filter_used;
	// Only pre-4.0 version show the file filter bug
	BOOL bBadNT = sysInfo.m_bWinNT && sysInfo.m_dwMajor < 4;

	if(type && *type == HTM_ONLY){
        bHtmlOnly = TRUE;
		if (bBadNT)
			htm_filter_used.LoadString(IDS_FILTERNT_HTM);
		else
#ifdef XP_WIN16
			htm_filter_used.LoadString(IDS_FILTER_HTM16);
#else
			htm_filter_used.LoadString(IDS_FILTER_HTM32);
#endif
	} else {
		if (bBadNT)
			// For NT, use same string as 16bit
			htm_filter_used.LoadString(IDS_HTM_FILTER16);
		else
#ifdef XP_WIN16
			htm_filter_used.LoadString(IDS_HTM_FILTER16);
#else
			htm_filter_used.LoadString(IDS_HTM_FILTER32);
#endif
	}

    // Default is to use the filter that includes *.htm/*.html, *.txt, and *.*
    strcpy(filter, htm_filter_used);
	
    // try to guess the type
    if(initial) {

        if( bHtmlOnly || strcasestr(initial, "htm") || strcasestr(initial, "html")) {
/*__EDITOR__*/
// Use ".html" for 32-bit versions
#ifdef XP_WIN16
            extension = "htm";
#else
            extension = "html";
#endif
            if( bHtmlOnly ){
                // This will force replacing any existing EXT with value "extension"
                bAppendHTML_Ext = TRUE;
            }

/*__EDITOR__END*/
			index = 1;
        } else if(strcasestr(initial, "gif")) {
            strcpy(filter, szLoadString(IDS_GIF_FILTER));
            extension = "gif";
			index = 1;
        } else if(strcasestr(initial, "xbm")) {
            strcpy(filter, szLoadString(IDS_XBM_FILTER));
            extension = "xbm";
			index = 1;
        } else if(strcasestr(initial, "jpg") || strcasestr(initial, "jpeg")) {
#ifdef XP_WIN16
            strcpy(filter, szLoadString(IDS_JPG_FILTER16));
#else
            strcpy(filter, szLoadString(IDS_JPG_FILTER32));
#endif
            extension = "jpg";
			index = 1;
        } else if(strcasestr(initial, "txt")) {
            strcpy(filter, htm_filter_used); //htm_filter
            extension = "txt";
			index = 2;
		} else if(strcasestr(initial, "p12")) {
			extension = "p12";
            strcpy(filter, szLoadString(IDS_PKCS12_FILTER));
        } else {
            extension = aIExt;
            aIExt[0] = '\0';
            size_t stExt = 0;
            DWORD dwFlags = EXT_NO_PERIOD;
            
#ifdef XP_WIN16
            dwFlags |= EXT_DOT_THREE;
#endif
            stExt = EXT_Invent(aIExt, sizeof(aIExt), dwFlags, initial, NULL);
            if(!stExt) {
                extension = NULL;
            }

			if(extension) {
    			strcpy(filter, szLoadString(IDS_ALL_FILTER));
            } else {
                // We have a name but no extension
                // Use the list for HTML files, *.txt, or *.*
                if ( !type || (*type == 0 || *type == ALL || *type == HTM || *type == HTM_ONLY) ) {
                    // If HTM was requested type or no type requested, then build
                    // a better suggested name
                    bAppendHTML_Ext = TRUE; 
                }
            }
            // Show *.htm as initial filter
			index = 1;
		}
    }


    if( (!initial && type && (*type == HTM || *type == HTM_ONLY)) || bAppendHTML_Ext ){
		bAppendHTML_Ext = TRUE;
		// no initial name --- assume HTML if it was requested type
		index = 1;
// Use ".html" for 32-bit versions
#ifdef XP_WIN16
        extension = "htm";
#else
        extension = "html";
#endif
	} else if (!initial && type && (*type == P12)) {
		extension = "p12";
        strcpy(filter, szLoadString(IDS_PKCS12_FILTER));
	} else if(!initial) {
        extension = "*";
        index +=2;
	}

	// Replace '\n' with '\0' before pass to FileCommon dialog 
    for (char *p = filter; *p ; p++) {
        if (*p == '\n')  
			*p = '\0';
	}
    
	// space for the full path name    
	full_path = (char *) XP_ALLOC(2 * _MAX_PATH * sizeof(char));
	if(!full_path)
		return(NULL);

	// clear stuff out
	name[0]      = '\0';
    full_path[0] = '\0';
    
	char aDrive[_MAX_DRIVE];
	char aPath[_MAX_PATH];
	char aFName[_MAX_FNAME];
	char aExt[_MAX_EXT];
	CString defaultDir;
	BOOL bUsingDefault = FALSE;
	BOOL bUsingEditorDir = FALSE;
	char *fileName = initial;
#ifdef XP_WIN16
    char dosFileName[13];
#endif

	if(fileName) {
#ifdef XP_WIN16
        // convert file name to be 8.3
	    char *ext = FE_FindFileExt(fileName);
    	// Fill with 0 to assure proper termination
        memset(dosFileName, 0, 13*sizeof(char));
	    if( ext ){
			char *firstDot = strchr(fileName, '.');
			if ((firstDot - fileName) > 8)
				// chop it to 8 characters
				firstDot = fileName+8;
			strncpy(dosFileName, fileName, (firstDot-fileName));
			strncpy(dosFileName+(firstDot-fileName), ext, 
					min(strlen(ext), 4));
			fileName = dosFileName;
        } else if( strlen(fileName) > 8 ){
		    strncpy(dosFileName, fileName, 8);
            fileName = dosFileName;
        }
#endif
		_splitpath(fileName, aDrive, aPath, aFName, aExt);
		XP_STRCPY(full_path, aFName);
        if(bAppendHTML_Ext){ 
            // Here's where we build the better suggested name
            XP_STRCAT(full_path, ".");
            XP_STRCAT(full_path, extension);
        }
        else {
		    XP_STRCAT(full_path, aExt);
        }

		defaultDir += aDrive;
		defaultDir += aPath;
	}

    char   currentDir[_MAX_PATH+1];
    char * HaveCurrentDir = NULL;

    char dir[_MAX_PATH];
	if (defaultDir.IsEmpty()) {
    	int iLen = _MAX_PATH;
	    if(bHtmlOnly){
		    PREF_GetCharPref("editor.html_directory",dir,&iLen);
		    bUsingEditorDir = TRUE;
            // Save current directory to restore later
            // This is Win16 Compatable (GetCurrentDirectory is available for Win32)
            HaveCurrentDir = _getdcwd(0, currentDir, _MAX_PATH);
        } else {
		    PREF_GetCharPref("browser.download_directory",dir,&iLen);
		    bUsingDefault = TRUE;
        }
        defaultDir = dir;
	}

	CheckLegalFileName(full_path);

	memset(&fname, 0, sizeof(fname));
	fname.lStructSize = sizeof(OPENFILENAME);
	fname.hwndOwner = m_hWnd;
	fname.lpstrFilter = filter;
    fname.lpstrCustomFilter = NULL;
	fname.nFilterIndex = index;
	fname.lpstrFile = full_path;
	fname.nMaxFile = 2 * _MAX_PATH;
	fname.lpstrFileTitle = name;
	fname.nMaxFileTitle = 2 * _MAX_FNAME;
    fname.lpstrInitialDir = defaultDir;
	fname.lpstrTitle = prompt;
	fname.lpstrDefExt = extension;
	fname.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;

    BOOL bResult = FEU_GetSaveFileName(&fname);

	// see if the user selects a file or hits cancel	
	if(bResult) {
		if (bUsingDefault || bUsingEditorDir ) {
			char *full_dir = (char *) XP_STRDUP(full_path);
			full_dir[fname.nFileOffset] = '\0';
            
			if( bUsingDefault ){
                PREF_SetCharPref("browser.download_directory", full_dir);
            } else {
                PREF_SetCharPref("editor.html_directory", full_dir);
                if( HaveCurrentDir ){
                    SetCurrentDir(currentDir);
                }
            }
			XP_FREE(full_dir);
		}
        // tell the caller if the user selected to save this as text
        // if a source file and we want to save as text pass it back else
        //   just pass back that we want to save it as source
		if(type) {
            if(filter == htm_filter_used && fname.nFilterIndex == TXT)
                *type = TXT;
            else
                *type = HTM;
        }

        // tack the extension on at the end if the name doesn't have one
        // the Win9x dialog is not really respecting the DefExt as
        //   explained in the help files
        char * pLastBit = strrchr(full_path, '\\');
        if(!pLastBit)
            pLastBit = full_path;
		else
			pLastBit++; // skip over slash

		// figure out if the user has put an extension on the path name
		char * pNewExt = strrchr(pLastBit, '.');

        // Common dialog will act incorrectly if user uses ".shtml".
        // We get the following: "fname.shtml.html" so strip off the ".html"
		if(pNewExt && !_stricmp(pNewExt, ".html")) {
            *pNewExt = '\0';
		    char * pExt = strrchr(pLastBit, '.');
            if(pExt && !_stricmp(pExt, ".shtml")){
                // We found the user's ".shtml", so return truncated version
                return(full_path);
            }
            // ".shtml" not found - restore the period
            *pNewExt = '.';
		}

		//
		// so we have a new extension that is not equal to the original one
		//

		// see if it ends .txt if so then the user really wanted to save a text file
		//   no matter what the selector is set to.  Damn Win9x guidelines
		if(pNewExt && !_stricmp(pNewExt, ".txt")) {
			if(type)
				*type = TXT;															
			return(full_path);
		}
		return(full_path);
	} else {

		// user hit cancel
		if( full_path) XP_FREE(full_path);
		return(NULL);
	}
}

// Appends the specified filter description and pattern to the end of szFilterString,
// adding double NULL at the end. szFilterString must be a double NULL terminated
// string upon entry
//
// This routine doesn't add a duplicate to the list of description/patterns that are
// already there
void wfe_AppendToFilterString(char* szFilterString, char* szDescription, char* szPattern)
{
    if(!szFilterString || !szDescription || !szPattern)
        return;

    // Find the end of the existing filter (double NULL terminated). While we're at it,
	// check for an existing entry that's an exact match
	char*	pStr = szFilterString;
	BOOL	bDescriptionMatches;

	while (*pStr != '\0') {
		// See if the description matches
		bDescriptionMatches = stricmp(pStr, szDescription) == 0;
	
		// Skip over the description and get the filter pattern
		pStr = strchr(pStr, '\0') + 1;

		// Filter description and pattern really must exist as a pair
		ASSERT(*pStr != '\0');
		if (*pStr == '\0')
			break;  // found the double NULL terminator

		// If the description matched, then check the filter pattern
		if (bDescriptionMatches && stricmp(pStr, szPattern) == 0)
			return;  // don't add a duplicate string

		// Skip over the filter pattern and get the next pair
		pStr = strchr(pStr, '\0') + 1;
	}

	// At this point we're pointing to the second NULL terminator. Add the new
	// description to the end
    strcpy(pStr, szDescription);

	// Append the filter pattern, leaving the NULL terminator at the end of the
	// description
	pStr += strlen(szDescription) + 1;
	strcpy(pStr, szPattern);

	// Add a final NULL terminator
	pStr += strlen(szPattern) + 1;
	*pStr = '\0';
}

// Appends the szFilterAppendString to the end of the szFilterString, adding
// double NULL at the end. szFilterString must be a double NULL terminated string
// upon entry. szFilterAppendString must also be a double NULL terminated string
void wfe_AppendFilterStringToFilterString(char* szFilterString,
                                          char* szFilterAppendString)
{
    if((szFilterString == NULL) || (szFilterAppendString == NULL))
        return;

    // find the end of the existing filter, (double NULL)
    // szFilterString MUST be a double NULL term'd string or bad things happen
    char* pCharFilterString = szFilterString;
    while(!((*pCharFilterString == '\0') && (*(pCharFilterString + 1) == '\0')))
        pCharFilterString++;

    if(pCharFilterString != szFilterString) // if the string is not empty
        pCharFilterString++;                // start at the second NULL

    // find the size of the szFilterAppendString
    char* pCharString = szFilterAppendString;
    int iStringSize = 0;
    while(!((*pCharString == '\0') && (*(pCharString + 1) == '\0')))
    {
        pCharString++;
        iStringSize++;
    }

    // append the szFilterAppendString to the filter
    memcpy(pCharFilterString, szFilterAppendString, iStringSize);

    // double NULL terminate the string
    pCharFilterString += iStringSize;
    *pCharFilterString = '\0', pCharFilterString++, *pCharFilterString = '\0';
}

static BOOL
IsNullPlugin(NPFileTypeAssoc* pAssociation)
{
	// The null plug-in is the default plug-in handler and has the wildcard
	// '*' as it's file extent
	return pAssociation->extentstring && strcmp(pAssociation->extentstring, "*") == 0;
}

//
// construct a filter string from all the plugins file associations, and
// from the previous filter template
//
//CLM: Modified this to allow restricting filter to just *.HTM;*.HTML
//     Use HTM_ONLY (-1) instead of HTM
//
char* wfe_ConstructFilterString(int type)
{
    NPFileTypeAssoc* pAssociation = NULL;
    if (type != HTM_ONLY){
        // get the list of associations for all plugin MIME types
        // ONLY if we are not looking for just HTM/HTML files (used for Editor)
        pAssociation = NPL_GetFileAssociation(NULL);
    }
    
    char filter[256];
    int  iFilterSize;

    // Pre-4.0 NT Common Dialog has a bug: *.htm;*.html shows ".htm" files as ".html"
	if ( sysInfo.m_bWinNT && sysInfo.m_dwMajor < 4 ) {
		if(type == HTM_ONLY)
			strcpy(filter, szLoadString(IDS_FILTERNT_HTM));
		else
			strcpy(filter, szLoadString(IDS_FILTERNT));
		iFilterSize = strlen(filter);
    } 
    else {
		if (type == HTM_ONLY)
#ifdef XP_WIN16
			strcpy(filter, szLoadString(IDS_FILTER_HTM16));
#else
			strcpy(filter, szLoadString(IDS_FILTER_HTM32));
#endif
		else if(type == P12)
			strcpy(filter, szLoadString(IDS_PKCS12_FILTER));
		else
#ifdef XP_WIN16
			strcpy(filter, szLoadString(IDS_FILTER16));
#else
			strcpy(filter, szLoadString(IDS_FILTER32));
#endif
		iFilterSize = strlen(filter);
    }
	// Replace '\n' with '\0' before pass to CommDialog
    for (char *p = filter; *p ; p++)
        if (*p == '\n')  *p = '\0';

    if(pAssociation == NULL)
    {
        char* pString = NULL;
        if(iFilterSize) {
            pString = (char *)XP_ALLOC(iFilterSize);
        }
        if(pString) {
            memcpy(pString, filter, iFilterSize);
        }
        return pString;
    }

    // Determine how much memory to allocate. The filter string consists of pairs of NULL
	// terminated strings. The first string describes the filter and the second string
	// specifies the filter pattern
    int iAccumulator = 0;
    do {
        if (pAssociation->fileType && pAssociation->extentlist) {
			// Add in space for the filter description
            iAccumulator += strlen((const char*)pAssociation->fileType);

			// NULL terminator between filter desciption and filter pattern
			iAccumulator++;

			// Determine the space needed for the filter pattern. There can be multiple
			// filter patterns per filter description by separating the filter patterns
			// with a semicolon
			for (char** ppExtentList = pAssociation->extentlist; *ppExtentList;) {
				// The extent list is just the file extension. The filter pattern should
				// include a "*."
				iAccumulator += strlen("*.") + strlen(*ppExtentList);

				// If there's another filter pattern then add a semicolon delimiter
				if (*++ppExtentList)
					iAccumulator++;
			}

			// Space for NULL terminating the filter pattern string
			iAccumulator++;
        }
        pAssociation = pAssociation->pNext;
    } while(pAssociation);

	// Add in room for the basic template (this size includes the final NULL
	// terminator)
    iAccumulator += iFilterSize;

    char* pFilterString = (char *) XP_ALLOC(iAccumulator);

	if (pFilterString) {
		// Start off with an empty filter string
		pFilterString[0] = '\0';
		pFilterString[1] = '\0';
	
		// Add the basic template
		wfe_AppendFilterStringToFilterString(pFilterString, filter);

		pAssociation = NPL_GetFileAssociation(NULL);
	
		// Add the plugin filters
		for (pAssociation = NPL_GetFileAssociation(NULL); pAssociation; pAssociation = pAssociation->pNext) {
			if (IsNullPlugin(pAssociation))
				continue;

			if (pAssociation->fileType && pAssociation->extentlist) {
				// Build the filter pattern(s). There can be multiple filter patterns for a
				// filter description by separating the filter patterns with a semicolon
				CString	strFilterPattern;
	
				for (char** ppExtentList = pAssociation->extentlist; *ppExtentList;) {
					// The extent list is just the file extension. The filter pattern should
					// include a "*."
					strFilterPattern += "*.";
					strFilterPattern += *ppExtentList;
	
					// If there's another filter pattern then add a semicolon delimiter
					if (*++ppExtentList)
						strFilterPattern += ';';
				}

				// Add the filter description and filter pattern if it doesn't already exist
				// in the list. You can end up with duplicates if there is a plug-in that registers
				// more than one MIME type for a given extension, e.g. audio/aiff and audio/x-aiff
				wfe_AppendToFilterString(pFilterString, (char*)pAssociation->fileType,
					(char*)(const char*)strFilterPattern);
			}
		}
	}

    return pFilterString;
}

#ifdef XP_WIN16
PRIVATE void wfe_InplaceToLower(char *pConvert) {
    while (pConvert && *pConvert) {
      *pConvert = XP_TO_LOWER(*pConvert);
      pConvert++;
    }
}
#endif


//
// Return the path name of a file the user has picked for a given task
// It is up to the user to free the filename
// The file must exist
// Return NULL if the user cancels
// If pOpenIntoEditor is not null, then radio buttons are added to select window type
//    set initial choice and read user's choice from this
MODULE_PRIVATE char * 
wfe_GetExistingFileName(HWND m_hWnd, char * prompt, int type, XP_Bool bMustExist, BOOL * pOpenIntoEditor)
{

    OPENFILENAME fname;
    char * full_path = NULL;
    char   name[_MAX_FNAME];
    char * defaultDir = NULL;
    char* filter = wfe_ConstructFilterString(type);

    /* initialize the OPENFILENAME struct */
    
    BOOL result;
    UINT index = (type == HTM_ONLY) ? 1 : type;

    // space for the full path name    
    full_path = (char *) XP_ALLOC(_MAX_PATH * sizeof(char));
    if(!full_path){
        XP_FREE(filter);
        return(NULL);
    }
    name[0]      = '\0';
    full_path[0] = '\0';

    char   currentDir[_MAX_PATH+1];
    char * HaveCurrentDir = NULL;
    
    char dir[_MAX_PATH];
	if(type == HTM_ONLY){
	    int iLen = _MAX_PATH;
		PREF_GetCharPref("editor.html_directory",dir,&iLen);
		defaultDir = dir;
        // Save current directory to restore later
        HaveCurrentDir = _getdcwd(0, currentDir, _MAX_PATH);
    }
    // set up the entries
    fname.lStructSize = sizeof(OPENFILENAME);
    fname.hwndOwner = m_hWnd;
    fname.lpstrFilter = filter;
    fname.lpstrCustomFilter = NULL;
    fname.nFilterIndex = index;
    fname.lpstrFile = full_path;
    fname.nMaxFile = _MAX_PATH;
    fname.lpstrFileTitle = name;
    fname.nMaxFileTitle = _MAX_FNAME;
    fname.lpstrInitialDir = defaultDir;
    fname.lpstrTitle = prompt;
    fname.Flags = OFN_HIDEREADONLY;
    fname.lpstrDefExt = NULL;

    if(bMustExist)
        fname.Flags |= OFN_FILEMUSTEXIST;


    result = FEU_GetOpenFileName(&fname);

    XP_FREE(filter);

    // see if the user selects a file or hits cancel    
    if(result) {
        // On win16 force to lower case.
#ifdef XP_WIN16
        wfe_InplaceToLower(full_path);
#endif
    	if(type == HTM_ONLY){
		    char *full_dir = (char *) XP_STRDUP(full_path);
		    full_dir[fname.nFileOffset] = '\0';
		    PREF_SetCharPref("editor.html_directory",full_dir);
		    XP_FREE(full_dir);
            if( HaveCurrentDir ){
                SetCurrentDir(currentDir);
            }
        }
        return(full_path);
    } else {
        // user hit cancel
        if(full_path) XP_FREE(full_path);
        return(NULL);
    }
}

// CLM: Similar to wfe_GetExistingFileName(),
//      but designed to select Image Files: *.jpg, *.gif
//
// Return the path name of a file the user has picked for a given task
// It is up to the user to free the filename
// Return NULL if the user cancels
//
MODULE_PRIVATE char * 
wfe_GetExistingImageFileName(HWND m_hWnd, char * prompt, XP_Bool bMustExist)
{

    OPENFILENAME fname;
    char * full_path;
    char   name[_MAX_FNAME];
	char filter[256];

#ifdef XP_WIN16
	strcpy(filter, szLoadString(IDS_IMAGE_FILTER16));
#else
	strcpy(filter, szLoadString(IDS_IMAGE_FILTER32));
#endif
	// replace '\n' with '\0' before pass to CommonDialog
    for (char *p = filter; *p ; p++)
        if (*p == '\n')  *p = '\0';


    /* initialize the OPENFILENAME struct */
    
    // space for the full path name    
    full_path = (char *) XP_ALLOC(_MAX_PATH * sizeof(char));
    if(!full_path)
        return(NULL);

    name[0]      = '\0';
    full_path[0] = '\0';

    // Save current directory to restore later
    char   currentDir[_MAX_PATH+1];
    char * HaveCurrentDir = _getdcwd(0, currentDir, _MAX_PATH);

    char defaultDir[_MAX_PATH];
    int iLen = _MAX_PATH;
    PREF_GetCharPref("editor.image_directory",defaultDir,&iLen);

    // set up the entries
    fname.lStructSize = sizeof(OPENFILENAME);
    fname.hwndOwner = m_hWnd;
    fname.lpstrFilter = filter;
    fname.lpstrCustomFilter = NULL;
    fname.nFilterIndex = 0;
    fname.lpstrFile = full_path;
    fname.nMaxFile = _MAX_PATH;
    fname.lpstrFileTitle = name;
    fname.nMaxFileTitle = _MAX_FNAME;
    fname.lpstrInitialDir = defaultDir;
    fname.lpstrTitle = prompt;
    fname.Flags = OFN_HIDEREADONLY;
    fname.lpstrDefExt = NULL;

    if(bMustExist)
        fname.Flags |= OFN_FILEMUSTEXIST;

    // see if the user selects a file or hits cancel    
    if(FEU_GetOpenFileName(&fname)) {
        // On win16 force to lower case.
#ifdef XP_WIN16
        wfe_InplaceToLower(full_path);        
#endif
		char *full_dir = (char *) XP_STRDUP(full_path);
		full_dir[fname.nFileOffset] = '\0';
		PREF_SetCharPref("editor.image_directory",full_dir);
		XP_FREE(full_dir);

        if( HaveCurrentDir ){
            SetCurrentDir(currentDir);
        }
        return(full_path);

    } else {
        // user hit cancel
        XP_FREE(full_path);
        return(NULL);
    }
}

// Just need a stub.  Always fail
//
extern "C" int dupsocket(int foo)
{
    return(-1); 
}


// INTL_ResourceCharSet(void)
//
extern "C" char *INTL_ResourceCharSet(void)
{
    return szLoadString(IDS_RESOURCE_CHARSET) ;
}

