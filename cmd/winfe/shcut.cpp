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
#include "string.h"
#include "shcut.h"

CInternetShortcut::CInternetShortcut (  const char * pszFilename, char * pszURL )
{
#if defined(XP_WIN32) && _MSC_VER >= 1100

	HRESULT hres;
	IUniformResourceLocator * purl;

	hres = CoCreateInstance (
		CLSID_InternetShortcut, NULL, CLSCTX_INPROC_SERVER,
		IID_IUniformResourceLocator, (void **)&purl );

	if ( SUCCEEDED(hres) )
	{
		IPersistFile * ppf;
		WORD wsz[MAX_PATH];

		MultiByteToWideChar ( CP_ACP, 0, pszFilename, -1, wsz, MAX_PATH );
		hres = purl->QueryInterface ( IID_IPersistFile, (void **)&ppf );

		if ( SUCCEEDED(hres) )
		{
			m_filename = pszFilename;						
			if ( ( pszFilename != NULL ) && ( pszURL == NULL ) )
			{
				char * pszURL;
				IMalloc * pmalloc;
				hres = ppf->Load ((LPCOLESTR)wsz, STGM_READ );
				
				if ( SUCCEEDED(hres) )
				{
					purl->GetURL ( (PSTR*)&pszURL );
					m_URL = pszURL;
					hres = CoGetMalloc ( MEMCTX_TASK, (LPMALLOC*)&pmalloc);
					if ( SUCCEEDED(hres) )
					{
						if ( pmalloc->DidAlloc ( pszURL ) == 1 )
							pmalloc->Free( pszURL );
						pmalloc->Release ( );
					}
				}
			}
			else
			{
				m_URL = pszURL;
				purl->SetURL ( pszURL, 0 );
				hres = ppf->Save ((LPCOLESTR)wsz, STGM_READ );
				
				if ( !SUCCEEDED(hres) )
				{
					switch ( hres ) 
					{
						case URL_E_INVALID_SYNTAX:
							AfxMessageBox ( szLoadString(IDS_URL_INVALID) );
							break;

						case URL_E_UNREGISTERED_PROTOCOL:
							AfxMessageBox ( szLoadString(IDS_URL_UNREGISTERED) );
							break;

						default:
							AfxMessageBox ( szLoadString(IDS_URL_SAVEFAILED) );
							break;
					}
				}
				
				ppf->SaveCompleted ((LPCOLESTR)wsz);
			}
			ppf->Release ( );			
		}
		purl->Release ( );
	}
#endif /* XP_WIN32 */
}

void CInternetShortcut::GetURL ( char * urlBuffer, int urlBuffSize )
{
	strncpy ( urlBuffer, m_URL, urlBuffSize );
}

CInternetShortcut::CInternetShortcut ( void )
{
	m_URL = _T("");
	m_filename = _T("");
}

BOOL CInternetShortcut::ShellSupport ( )
{
#if defined(XP_WIN16) || _MSC_VER < 1100

    return(FALSE);

#else

	int supportsInternetShortcuts = -1;
	if ( supportsInternetShortcuts == -1 )
	{
		HRESULT hres;
		IUniformResourceLocator * purl;

		hres = CoCreateInstance (
			CLSID_InternetShortcut, NULL, CLSCTX_INPROC_SERVER,
			IID_IUniformResourceLocator, (void **)&purl );

		if ( SUCCEEDED(hres) )
		{
			purl->Release ( );
			supportsInternetShortcuts = (int)TRUE;	
		}
		else
			supportsInternetShortcuts = (int)FALSE;		
	}
	return (BOOL) supportsInternetShortcuts;

#endif

}


HRESULT ResolveShortCut ( HWND hwnd, LPCSTR pszShortcutFile, LPSTR pszPath)
{
#if defined(XP_WIN32) && _MSC_VER >= 1100

  HRESULT hres;
  IShellLink* psl;
  WIN32_FIND_DATA wfd;

  *pszPath = 0;   // assume failure

  // Get a pointer to the IShellLink interface.
  hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                            IID_IShellLink, (void**)&psl);
  if (SUCCEEDED(hres))
  {
    IPersistFile* ppf;
    // Get a pointer to the IPersistFile interface.
    hres = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
    if (SUCCEEDED(hres))
    {
      WORD wsz[MAX_PATH];

      // Ensure string is Unicode.
      MultiByteToWideChar(CP_ACP, 0, pszShortcutFile, -1, wsz,
                                 MAX_PATH);

	hres = ppf->Load((LPCOLESTR)wsz, STGM_READ);
	 
     if (SUCCEEDED(hres))
     {
        char szGotPath[MAX_PATH];
       // Resolve the link.
       hres = psl->Resolve(hwnd, SLR_ANY_MATCH);
       if (SUCCEEDED(hres))
       {
          strcpy(szGotPath, pszShortcutFile);
          // Get the path to the link target.
          hres = psl->GetPath(szGotPath, MAX_PATH, (WIN32_FIND_DATA *)&wfd, 
                 SLGP_SHORTPATH );
          if (!SUCCEEDED(hres))
             AfxMessageBox( szLoadString(IDS_GETPATH_FAILED) );
          strcpy ( pszPath, szGotPath );
        }
      }
      // Release pointer to IPersistFile interface.
      ppf->Release();
    }
    // Release pointer to IShellLink interface.
    psl->Release();
  }
  return hres;

#else
  return 0;
#endif
}

// Add internet shortcut info to the data source

void DragInternetShortcut ( 
    COleDataSource * pDataSource,       // OleDataSource object to attach data
    LPCSTR lpszTitle,                    // URL description
    LPCSTR lpszAddress )                 // URL
{
#ifdef WIN32
    CInternetShortcut InternetShortcut;
    if ( !InternetShortcut.ShellSupport ( ) )   // support for internet shortcuts?
        return;                                 

    // who frees this guy?
    // allocate memory for FILEDESCRIPTOR object which contains a list of droppable
    // files        
    HGLOBAL hfgd = GlobalAlloc(GMEM_ZEROINIT|GMEM_SHARE,sizeof(FILEGROUPDESCRIPTOR));
    if ( !hfgd ) return;
    LPFILEGROUPDESCRIPTOR lpfgd = (LPFILEGROUPDESCRIPTOR) GlobalLock ( hfgd );

    char szFilename[ _MAX_PATH ];               // shortcut filename
	static char * invalidChars = ",\\/:*?<>|\"~";   // invalid long filename characters
	unsigned pos;                               // array bad character position
	char szNewTitle[ _MAX_PATH ];               // newly created acceptable title
    
    // if there is a specified and there is some text there, use it
    if ( lpszTitle && strlen ( lpszTitle ) ) {
        strncpy ( szFilename, lpszTitle, _MAX_PATH - 50 );
    }
    else {
        // no title specified, use the host part of the url
        char * pHost = NET_ParseURL ( lpszAddress, GET_HOST_PART );
        if ( pHost )
            strncpy ( szFilename, pHost, _MAX_PATH - 50 );
        else
            return;
        XP_FREE(pHost);
    }

    szFilename[_MAX_PATH - 51] = '\0';
    // scan the title string to look for invalid long filename characters
	for ( pos = 0; pos < strlen ( invalidChars ); pos++ )
		if ( strchr ( szFilename, invalidChars[ pos ] ) )
		{

            int i, j;
            for ( i = j = 0; i < (int)strlen ( szFilename ); i++ )
                if ( !strchr ( invalidChars, szFilename[ i ] ) )
                    szNewTitle[ j++ ] = szFilename[ i ];
            szNewTitle[ j ] = '\0';
    		break;
	    }

	if ( pos == strlen ( invalidChars ) )
		strcpy ( szNewTitle, szFilename );

    // if no title, parse the url, .URL extensions imply an internet 
    // shortcut file
    PR_snprintf(szFilename, sizeof(szFilename), "%s %s.URL", szLoadString(IDS_SHORTCUT_TO), szNewTitle);

    // one file in the file descriptor block
    lpfgd->cItems = 1;
    lpfgd->fgd[0].dwFlags = FD_LINKUI;
    strcpy ( lpfgd->fgd[0].cFileName, szFilename );
    GlobalUnlock ( hfgd );
    
    // register the file descriptor clipboard format
    UINT cfFileDescriptor = RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);
    pDataSource->CacheGlobalData(cfFileDescriptor,hfgd);

    // register the clipboard format for file contents
    UINT cfFileContents = RegisterClipboardFormat ( CFSTR_FILECONTENTS );

    // FORMATETC supplies format information beyond the standard clipboard
    // formats.  Set up a file contents block with the lindex indicating 
    // the file at position zero in the file descriptor block
    FORMATETC fmetc = { 0, NULL, DVASPECT_CONTENT, 0, TYMED_FILE };
    fmetc.cfFormat = cfFileContents;

    // temporary file contents storage
    char * lpStr = (char *)malloc ( 1024 );

    // create the file contents for the internet shortcut file... luckily
    // this is just a text file.  The nIconIndex should actually be retreiving
    // the default icon index
    PR_snprintf( lpStr, 1024, "[InternetShortcut]\nURL=%s", lpszAddress );

    // global alloc the file contents block and move the contents into
    // shared memory and free the temporary contents block
    HGLOBAL hContents = GlobalAlloc ( GMEM_ZEROINIT|GMEM_SHARE, strlen (lpStr)+1 );
    LPSTR lpszContents = (LPSTR) GlobalLock ( hContents );
    strcpy ( lpszContents, lpStr );
    free ( lpStr );
    GlobalUnlock ( hContents );

    // add the new the contents to the data source
    pDataSource->CacheGlobalData(cfFileContents,hContents,&fmetc);
#endif
}

void DragMultipleShortcuts(COleDataSource* pDataSource, CString* titleArray, 
						   CString* urlArray, int count)
{
#ifdef WIN32
    CInternetShortcut InternetShortcut;
    if ( !InternetShortcut.ShellSupport ( ) )   // support for internet shortcuts?
        return;                                 

    // who frees this guy?
    // allocate memory for FILEDESCRIPTOR object which contains a list of droppable
    // files        
    HGLOBAL hfgd = GlobalAlloc(GMEM_ZEROINIT|GMEM_SHARE,sizeof(FILEGROUPDESCRIPTOR)+(count-1)*sizeof(FILEDESCRIPTOR));
    if ( !hfgd ) return;
    LPFILEGROUPDESCRIPTOR lpfgd = (LPFILEGROUPDESCRIPTOR) GlobalLock ( hfgd );
	lpfgd->cItems = count;  // Init the count info.

	for (int i = 0; i < count; i++)
	{

		char szFilename[ _MAX_PATH ];               // shortcut filename
		static char * invalidChars = ",\\/:*?<>|\"~";   // invalid long filename characters
		unsigned pos;                               // array bad character position
		char szNewTitle[ _MAX_PATH ];               // newly created acceptable title
    
		LPCSTR lpszTitle = titleArray[i];
		LPCSTR lpszAddress = urlArray[i];

		// if there is a specified and there is some text there, use it
		if ( lpszTitle && strlen ( lpszTitle ) ) 
		{
			strncpy ( szFilename, lpszTitle, _MAX_PATH - 50 );
		}
		else 
		{
			// no title specified, use the host part of the url
			char * pHost = NET_ParseURL ( lpszAddress, GET_HOST_PART );
			if ( pHost )
				strncpy ( szFilename, pHost, _MAX_PATH - 50 );
			XP_FREE(pHost);
		}

		szFilename[_MAX_PATH - 51] = '\0';
		// scan the title string to look for invalid long filename characters
		for ( pos = 0; pos < strlen ( invalidChars ); pos++ )
			if ( strchr ( szFilename, invalidChars[ pos ] ) )
		{
            int i, j;
            for ( i = j = 0; i < (int)strlen ( szFilename ); i++ )
                if ( !strchr ( invalidChars, szFilename[ i ] ) )
                    szNewTitle[ j++ ] = szFilename[ i ];
            szNewTitle[ j ] = '\0';
    		break;
	    }

		if ( pos == strlen ( invalidChars ) )
			strcpy ( szNewTitle, szFilename );

		// if no title, parse the url, .URL extensions imply an internet 
		// shortcut file
		PR_snprintf(szFilename, sizeof(szFilename), "%s %s.URL", szLoadString(IDS_SHORTCUT_TO), szNewTitle);

		// one file in the file descriptor block
		
		lpfgd->fgd[i].dwFlags = FD_LINKUI;
		strcpy ( lpfgd->fgd[i].cFileName, szFilename );

	}
		
	GlobalUnlock ( hfgd );
    
	// register the file descriptor clipboard format
	UINT cfFileDescriptor = RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);
	pDataSource->CacheGlobalData(cfFileDescriptor,hfgd);
	
	for (i = 0; i < count; i++)
	{
		LPCSTR lpszAddress = urlArray[i]; // Get the item URL

		// register the clipboard format for file contents
		UINT cfFileContents = RegisterClipboardFormat ( CFSTR_FILECONTENTS );

		// FORMATETC supplies format information beyond the standard clipboard
		// formats.  Set up a file contents block with the lindex indicating 
		// the file at position i in the file descriptor block
		FORMATETC fmetc = { 0, NULL, DVASPECT_CONTENT, i, TYMED_FILE };
		fmetc.cfFormat = cfFileContents;

		// temporary file contents storage
		char * lpStr = (char *)malloc ( 1024 );

		// create the file contents for the internet shortcut file... luckily
		// this is just a text file.  The nIconIndex should actually be retreiving
		// the default icon index
		PR_snprintf( lpStr, 1024, "[InternetShortcut]\nURL=%s", lpszAddress );

		// global alloc the file contents block and move the contents into
		// shared memory and free the temporary contents block
		HGLOBAL hContents = GlobalAlloc ( GMEM_ZEROINIT|GMEM_SHARE, strlen (lpStr)+1 );
		LPSTR lpszContents = (LPSTR) GlobalLock ( hContents );
		strcpy ( lpszContents, lpStr );
		free ( lpStr );
		GlobalUnlock ( hContents );

		// add the new the contents to the data source
		pDataSource->CacheGlobalData(cfFileContents,hContents,&fmetc);
	}
#endif
	
}
