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

// netscape.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"

#define XP_CPLUSPLUS  // temporary hack - jsw

// XP Includes
#include "np.h"
#include "cvffc.h"

#include "prefapi.h"

#ifdef XP_WIN32
#include "shcut.h"
#endif

#include "libevent.h"
#include "wfemsg.h"

// Misc Includes
#include "res\appicon.h"
#include "custom.h"
#include "dialog.h"
#include "ngdwtrst.h"
#include "oleregis.h"
#include "sysinfo.h"
#include "timer.h"
#include "winproto.h"
#include "cmdparse.h"
#include "ddecmd.h"
#include "apiapi.h"
#include "apipage.h"
#include "logindg.h"

#include "hiddenfr.h"
#include "cxprint.h"
#include "cxicon.h"
#include "navfram.h"
#include "secnav.h"

#include "pw_public.h"
extern "C"      {
#include "cookies.h"
};
#ifdef MOZ_LOC_INDEP
#include "li_public.h"
#endif /* MOZ_LOC_INDEP */

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

#ifndef VER_PLATFORM_WIN32_WINDOWS
#define VER_PLATFORM_WIN32_WINDOWS      1
#endif

#ifdef __BORLANDC__
	#define _mkdir mkdir
#endif

// Registry key constants
static const CString strMARKUP_KEY = "NetscapeMarkup";
static const CString strOPEN_CMD_FMT = "%s\\shell\\open\\command";
static const CString strDDE_EXEC_FMT = "%s\\shell\\open\\ddeexec";
static const CString strDDE_APP_FMT = "%s\\shell\\open\\ddeexec\\Application";
static const CString strDDE_APP_NAME = "NSShell";
static const CString strDDE_OLDAPP_NAME = "Netscape";
static const CString strDEF_ICON_FMT = "%s\\DefaultIcon";
static const CString strDDE_EXEC_VALUE = "%1";
static const CString strDDE_TOPIC_FMT = "%s\\shell\\open\\ddeexec\\Topic";
static const CString strEDIT_CMD_FMT = "%s\\shell\\edit\\command";

// li_stuff
static int LIActive = 0;


#if defined( _DEBUG) && defined( XP_WIN32 )
int
CNetscapeApp::InitConsoleWindow(void)
{

    STARTUPINFO si = {0};		  // initialize all members to zero
    si.cb          = sizeof(STARTUPINFO);
    AllocConsole();                       // get that console window
    freopen("CONOUT$", "a", stderr);      // redirect stderr to console

    // Warn user
    fputs( "Netscape stderr output window\n"
	   "DO NOT CLOSE THIS WINDOW, it will terminate Netscape\n",
	   stderr);
    // launch command interpreter to keep window open
    return CreateProcess(NULL,            // module name
	   "cmd.exe",                     // gotta run something
	    NULL,                         // process security attributes
	    NULL,                         // thread  security attributes
	    TRUE,                         // process inherits handles
	    CREATE_SUSPENDED,             // creation flags
	    NULL,                         // new environment
	    NULL,                         // current directory
	    &si,                          //
	    &m_pi);                       //
}
#endif



/////////////////////////////////////////////////////////////////////////////
// CNetscapeApp construction

CNetscapeApp::CNetscapeApp()
{
    //  Initially allow UNC file access.
    m_bUNC = TRUE;
    
    // No slave context yet.
    m_pSlaveCX = NULL;
    m_pRDFCX = NULL;
    
    //  Explicitly clear the idle binding to timeout code.
    m_bIdleProcessTimeouts = FALSE;

    //  Not in init instance.
    m_bInInitInstance = FALSE;   //  Flag for frames created to use m_iFrameCmdShow
	m_bInGetCriticalFiles = FALSE;  // Flag for frames created while getting early files 
									// These are just HTML dialogs and need not have toolbars, etc
    m_iFrameCmdShow = -1;

    // No windows / frames or Hotlist yet
    m_bDontLoadHome 	     = FALSE;
    m_pMainWnd 		     = NULL;
    m_pUpdateWhatsNewWnd     = NULL;
    m_pFrameList 	     = NULL;
    m_pHiddenFrame 	     = NULL; 
    m_pBookmarks 	     = NULL;
    m_pNetcasterWindow 	     = NULL;
    m_iTemplateLocationCount = 0;
    pIconDC = pImageDC 	     = NULL;
    m_ParentAppWindow 	     = NULL;
    m_bChildWindow 	     = FALSE;
    m_bParseTelnetURLs       = FALSE;
    m_bNetworkProfile        = FALSE;

    // no dialup stuff yet
    m_bKioskMode		= FALSE; 
    m_bSuperKioskMode		= FALSE; 
#ifdef MOZ_MAIL_NEWS
    m_bCreateMail		= FALSE;
    m_bCreateNews		= FALSE;
#endif /* MOZ_MAIL_NEWS */
    m_bCreateNetcaster	= FALSE;
#ifdef MOZ_MAIL_NEWS
	m_bCreateInbox		= FALSE;//causes the inbox to be started
	m_bCreateFolders	= FALSE;//causes the folders frame window to be started
	m_bCreateFolder		= FALSE;//causes a particular folder in the folders frame window to open
	m_bCreateCompose	= FALSE;//causes the compose window to be opened, 3 different possibilities
#endif /* MOZ_MAIL_NEWS */
#ifdef EDITOR
	m_bCreateEdit		= FALSE;//brings up the edit window
#endif /* EDITOR */
#ifdef MOZ_MAIL_NEWS
	m_bCreateAddress	= FALSE;//openes the address book window
#endif /* MOZ_MAIL_NEWS */
	m_bCreateLDIF_IMPORT		= FALSE;//imports an LDIF file
    m_bCreateLDIF_EXPORT		= FALSE;//imports an LDIF file
	m_bCreateBrowser	= FALSE;//forces the browser to launch despite preferences settings.
	m_bHasArguments		= FALSE;//signals that switch parameters need parsing 
	m_bAccountSetupStartupJava = FALSE;

	m_bCreateNewProfile = FALSE;
	m_bProfileManager	= FALSE;
	m_bCreateJavaDebugAgent = FALSE;
	m_bAccountSetup     = FALSE;
	m_bAlwaysDockTaskBar = FALSE;
	m_bShowNetscapeButton = FALSE;

    // No global java event queue yet...
    mozilla_event_queue      = NULL;

    m_hPostalLib             = NULL;
    m_fnOpenMailSession      = NULL;
    m_fnComposeMailMessage   = NULL;
    m_fnUnRegisterMailClient = NULL;
    m_fnShowMailBox          = NULL; 
    m_fnShowMessageCenter    = NULL; 
    m_fnCloseMailSession     = NULL; 
    m_fnGetMenuItemString    = NULL; 
    m_bInitMapi 	     = TRUE;
    m_bExitStatus 	     = FALSE;
    InitTime();

#ifdef XP_WIN16
	m_nMsgLast = WM_NULL;
	::GetCursorPos(&m_ptCursorLast);
#endif

    /* This is the very first TRACE call in the client.
     * If we want the output to work outside the debugger, 
     * we must pass the output file handle to the C runtime debug 
     * output routines.  Note: "CONOUT$" is the name of "/dev/tty"
     */
#if defined( _DEBUG) && defined( XP_WIN32 )
    m_pi.hProcess = 0;
    char * mozTraceFileName = getenv("MOZTRCFILE");
    if (mozTraceFileName) {
	HANDLE hTraceFile = INVALID_HANDLE_VALUE;
	int success;

	if (!strcmp( "CONOUT$", mozTraceFileName)) {
	    success = InitConsoleWindow();
	} else {
//   This freopen doesn't work, so do it the hard way, use CreateFile.
//          success = (NULL != freopen(mozTraceFileName, "a", stderr));
	    hTraceFile = CreateFile(
		    mozTraceFileName,
		    /* GENERIC_READ    | */ GENERIC_WRITE,
		    /* FILE_SHARE_READ | */ FILE_SHARE_WRITE,
		    NULL, OPEN_ALWAYS, 
		    FILE_ATTRIBUTE_NORMAL /* | FILE_FLAG_WRITE_THROUGH */,
		    NULL);
	    success = (hTraceFile != INVALID_HANDLE_VALUE);
	    if (success) {
		SetStdHandle(STD_ERROR_HANDLE, hTraceFile);
	    }
	}
	if (success) {
	    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
	    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	} else {
	    LPVOID lpMsgBuf;
	    DWORD lastErr = GetLastError();

	    FormatMessage( 
	    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
	    NULL,
	    lastErr,
	    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
	    (LPTSTR) &lpMsgBuf,
	    0,
	    NULL );

	    // Display the string.
	    MessageBox( NULL, (char *)lpMsgBuf, "Create trace file", 
		    MB_OK|MB_ICONINFORMATION );

	    // Free the buffer.
	    LocalFree( lpMsgBuf );
	}
    }

#endif
    TRACE("Netscape App Constructor! \n");
}

void CNetscapeApp::InitTime()
{
    m_ttStartTime = ::time(NULL);
    m_dwLastMsgTick = m_dwStartTick = m_dwMsgTick = ::GetTickCount();
}

BOOL
CNetscapeApp::LoadPageSetupOptions()
{
	ApiPageSetup(api,0);
	api->SetMargins ( 
		(long) GetProfileInt ( "Page Setup", "Left", 720 ), 
		(long) GetProfileInt ( "Page Setup", "Right", 720 ),
		(long) GetProfileInt ( "Page Setup", "Top", 720 ),  
		(long) GetProfileInt ( "Page Setup", "Bottom", 720 ) );
	api->Header ( GetProfileInt ( 
		"Page Setup", "Header", PRINT_TITLE | PRINT_URL ) );
	api->Footer ( GetProfileInt ( 
		"Page Setup","Footer", PRINT_PAGECOUNT | PRINT_PAGENO | PRINT_DATE ) );
	api->SolidLines ( GetProfileInt ( "Page Setup", "SolidLines", 0 ) );
	api->BlackLines ( GetProfileInt ( "Page Setup", "BlackLines", 0 ) );
	api->BlackText ( GetProfileInt ( "Page Setup", "BlackText", 0 ) );
    api->ReverseOrder ( GetProfileInt ( "Page Setup", "Reverse", 0 ) );
    return TRUE;
}

void
CNetscapeApp::SavePageSetupOptions()
{
	ApiPageSetup(api,0);
	long mleft, mright, mtop, mbottom;

	// save margins
	api->GetMargins ( &mleft, &mright, &mtop, &mbottom );
	theApp.WriteProfileInt ( "Page Setup", "left",  LOWORD(mleft) );
	theApp.WriteProfileInt ( "Page Setup", "right", LOWORD(mright) );
	theApp.WriteProfileInt ( "Page Setup", "top",   LOWORD(mtop) );
	theApp.WriteProfileInt ( "Page Setup", "bottom",LOWORD(mbottom) );

	// save miscellaneous page setup flags
	theApp.WriteProfileInt ( "Page Setup", "Header", api->Header ( ) );
	theApp.WriteProfileInt ( "Page Setup", "Footer", api->Footer ( ) );
	theApp.WriteProfileInt ( "Page Setup", "SolidLines", api->SolidLines ( ) );
	theApp.WriteProfileInt ( "Page Setup", "BlackText", api->BlackText ( ) );
	theApp.WriteProfileInt ( "Page Setup", "BlackLines", api->BlackLines ( ) );
    theApp.WriteProfileInt ( "Page Setup", "Reverse", api->ReverseOrder ( ) );
}

//
// Parse the command line for INI file location and home page
//
void CNetscapeApp::parseCommandLine(char * commandLine)
{
	// assume no startup URL or profile
	m_CmdLineLoadURL = NULL;
	m_CmdLineProfile = NULL;

	if(IsRuntimeSwitch("-P",FALSE)) {
		// extract the quoted profile name..the format is -P"jonm@netscape.com"
		CString csCommandLine = commandLine;
		int iFirstQuote = csCommandLine.Find("-P\"");
		if( iFirstQuote != -1 ){
			iFirstQuote += 2;
			csCommandLine = csCommandLine.Mid(iFirstQuote+1);
			int iLastQuote = csCommandLine.Find('\"');
			if( iLastQuote == -1 ){
				// No last quote -- remove first quote from command line
				strcpy( (commandLine+iFirstQuote), (commandLine+iFirstQuote+1));
			} else {
				m_CmdLineProfile = XP_STRDUP(csCommandLine.Left(iLastQuote));
				// Remove the extracted string from command line
				strcpy( (commandLine+iFirstQuote), (commandLine+iFirstQuote+iLastQuote+2));
			}
			IsRuntimeSwitch("-P",TRUE);  // now remove the -P from the command line
		}
	}
    // Extract a filepath enclosed in quotes
    // MUST have both quotes, or we ignore it and strip away the first quote
    CString csCommandLine = commandLine;
    int iFirstQuote = csCommandLine.Find('\"');
    if( iFirstQuote != -1 ){
        csCommandLine = csCommandLine.Mid(iFirstQuote+1);
        int iLastQuote = csCommandLine.Find('\"');
        if( iLastQuote == -1 ){
            // No last quote -- remove first quote from command line
            strcpy( (commandLine+iFirstQuote), (commandLine+iFirstQuote+1));
        } else {
            m_CmdLineLoadURL = XP_STRDUP(csCommandLine.Left(iLastQuote));
            // Remove the extracted string from command line
            strcpy( (commandLine+iFirstQuote), (commandLine+iFirstQuote+iLastQuote+2));
        }
    }

    //  Need way to track if used command line -i switch.
    //
    BOOL bIniOption = FALSE;

	//	Check for the INI file location.
	CString csINI;
	CString csParentHwnd;

    // kiosk mode
    if(IsRuntimeSwitch("-k", TRUE))
        m_bKioskMode = TRUE;
        
    // super kiosk mode
    if(IsRuntimeSwitch("-sk", TRUE)) {
        m_bKioskMode = TRUE;
        m_bSuperKioskMode = TRUE;
    }
        
    // load home page?
    if(IsRuntimeSwitch("-H", TRUE))
        m_bDontLoadHome = TRUE;

#ifdef MOZ_MAIL_NEWS
    if(IsRuntimeSwitch("-mail",TRUE))
        m_bCreateMail = TRUE;
#endif /* MOZ_MAIL_NEWS */

	if(IsRuntimeSwitch("-netcaster",TRUE))
        m_bCreateNetcaster = TRUE;

#ifdef MOZ_MAIL_NEWS
    if(IsRuntimeSwitch("-news",TRUE))
        m_bCreateNews = TRUE;
#endif /* MOZ_MAIL_NEWS */

	if(IsRuntimeSwitch("-new_profile",TRUE))
        m_bCreateNewProfile = TRUE;

	if(IsRuntimeSwitch("-profile_manager",TRUE))
        m_bProfileManager = TRUE;

    if (IsRuntimeSwitch("-parse_telnet"))
        m_bParseTelnetURLs = TRUE;

#if defined(OJI) || defined(JAVA)
	if(IsRuntimeSwitch("-javadebug",TRUE)) {
		m_bCreateJavaDebugAgent = TRUE;
	}
#endif

    //This is a rather hideous hack to allow Navigator as a child window.
    //It is turned off until such time as it is deemed safe and absolutely necessary.
    /*if(IsRuntimeSwitch("-child",FALSE)) {
	csParentHwnd = RuntimeStringSwitch("-child");
	sscanf(csParentHwnd, "%x", &m_ParentAppWindow);
    } */

	if(IsRuntimeSwitch("-new_account",TRUE)){
        m_bAccountSetup = TRUE;
		m_bAlwaysDockTaskBar = TRUE;
		m_bAccountSetupStartupJava = TRUE;
	}


	if(IsRuntimeSwitch("-start_java",TRUE))
		m_bAccountSetupStartupJava = TRUE;


    // do *NOT* let /I be a run time switch because IsRuntimeSwitch
    //   is currently not smart enough to deal with http://foo/index.html
    //   correctly
	if(IsRuntimeSwitch("-I", FALSE)) {
		csINI = RuntimeStringSwitch("-I");
	}

#ifdef XP_WIN16

	if(csINI.IsEmpty() == FALSE)	{
		//	Make sure it's a good location.
		if(FEU_SanityCheckFile(csINI) == FALSE)	{
			MessageBox(NULL, szLoadString(IDS_INVALID_INIFILE), szLoadString(AFX_IDS_APP_TITLE), MB_OK);
		}
		else	{
			//	This is our new INI file
			theApp.m_pszProfileName = (const char *)XP_STRDUP(csINI);
			bIniOption = TRUE;
		}
	}
#endif

#ifdef EDITOR
	// This is used to tst for edit startup and 
	// is only TRUE if we are a Gold version
    m_bCmdEdit = FALSE;
    CString csEditFile;
// Look for Start-Editor switch
	if(IsRuntimeSwitch("-EDIT",FALSE))	{
		csEditFile = RuntimeStringSwitch("-EDIT");
        m_bCmdEdit = TRUE;
	}
    if ( m_bCmdEdit ) {
        // Copy the returnd string back to the command line
       	XP_STRCPY( commandLine, LPCSTR(csEditFile) );
    }
#endif // EDITOR

 	//  Pointer into command line as we parse it
	//
	char *curLoc = m_lpCmdLine;

	//	Skip the white space.
	while(curLoc && *curLoc != '\0' && isspace(*curLoc))
	    curLoc++;

    if( m_CmdLineLoadURL == NULL ){
	    // still stuff left --- assume its a file to load.
	    // if someone dragged a file onto us this will be a DOS path
	    //   when we get around to calling OnNormalLoad() to actually
	    //   load the page it will get interpreted correctly.
	    if(curLoc && *curLoc) {

            // if first char is a double quote don't copy it over
            if(curLoc[0] == '\"')
    		    m_CmdLineLoadURL = XP_STRDUP(&(curLoc[1]));
            else
    		    m_CmdLineLoadURL = XP_STRDUP(curLoc);

            // Win9x conveniently brackets the filename in double quotes
            //   convert them to spaces and let the netlib strip them out
            for(char * c = m_CmdLineLoadURL; c && *c; c++)
                if(*c == '\"')
                    *c = ' ';
        }
    }

#ifdef XP_WIN16
	
	//	If they didn't use the command line -i to specify
	//		the location of the INI file, attempt to use
	//		the location specified in the win.ini file.
	//	Scope the function call correctly or you will get
	//		the wrong information.
	//	The return buffer will never be empty.
	//
	//	Revised:
	//		Order of INI preference is as follows:
	//			command line
	//			win.ini
	//			current directory
	//			windows directory
	//		After resolution from win.ini down, then make
	//			sure the the win.ini file actually contains
	//			the correct reference; otherwise replace it.
	//
	if(!bIniOption)	{
		const char *cp_NotInIni = "Garrett 'Archibald Cox' Blythe";
        const char *cp_WinIniSection = XP_STRDUP(szLoadString(IDS_WIN_INI_SECTION));
        const char *cp_IniFile = XP_STRDUP(szLoadString(IDS_INI_FILE_NAME));
        const char *cp_WinIniEntry = "ini";
    
        //  Figure out the app's path for the default value.
        //  Tack on netscape.ini on the end.
        //
        auto char ca_default[_MAX_PATH];
        ::GetModuleFileName(m_hInstance, ca_default, _MAX_PATH);
        auto char *cp_lastslash = ::strrchr(ca_default, '\\') + 1;
        ::strcpy(cp_lastslash, cp_IniFile);
    

        //  Check the win.ini file for an entry pointing to the ini file.
        //
        auto char ca_iniBuff[_MAX_PATH];
        ::GetProfileString(cp_WinIniSection, cp_WinIniEntry, cp_NotInIni, ca_iniBuff, _MAX_PATH);
    
        //  If it isn't located in the ini file, set a flag,
        //    and copy over a default file name.
        //
        auto int i_UpdateWinIni = FALSE;
        if(::strcmp(cp_NotInIni, ca_iniBuff) == 0)  {
            i_UpdateWinIni = TRUE;
            ::strcpy(ca_iniBuff, ca_default);
        }
    
        m_pszProfileName = (const char *)XP_STRDUP(ca_iniBuff);
    
        //  Does it exist?
        //
        if(FEU_SanityCheckFile(m_pszProfileName) == FALSE)  {
            //  Well, shoot.
            //  Attempt to use the windows directory for the INI file.
            //
            char ca_windir[_MAX_PATH];
            ::GetWindowsDirectory(ca_windir, _MAX_PATH);
            if(ca_windir[::strlen(ca_windir) - 1] != '\\')  {
                ::strcat(ca_windir, "\\");
            }
            ::strcat(ca_windir, cp_IniFile);
      
            if(FEU_SanityCheckFile(ca_windir) == FALSE) {
                //  Unable to locate any INI file.
                //  We are going to use the original default, because
                //    we know the directory exists.
                //  Update the win.ini file regardless; there is no good location anyhow.
                //
                XP_FREE((void *)m_pszProfileName);
                m_pszProfileName = (const char *)XP_STRDUP(ca_default);
                i_UpdateWinIni = TRUE;
            } else  {
                //  Use INI in windows directory.
                //
                XP_FREE((void *)m_pszProfileName);
                m_pszProfileName = (const char *)XP_STRDUP(ca_windir);

                //  Add this to the win.ini file to avoid confusion
                //    in the future.
                //
                i_UpdateWinIni = TRUE;        
            }
        }
    
        //  Update the win.ini file if need be.
        //
        if(i_UpdateWinIni == TRUE)
            ::WriteProfileString(cp_WinIniSection, cp_WinIniEntry, m_pszProfileName);
    }

#endif // XP_WIN16

} // CNetscapeApp::parseCommandLine

/****************************************************************************
*
*	CNetscapeApp::CheckDefaultBrowser
*
*	PARAMETERS:
*		none
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This function checks the registry to see if we are still registered
*		as the "default browser" (listed under http key and other extensions),
*		since the MS Explorer has a nasty habit of kicking us out! If not,
*		we prompt the user to restore our registry settings and make us
*		the default again.
*
****************************************************************************/

void CNetscapeApp::CheckDefaultBrowser()
{
// We're only worrying about this for Win32
#ifdef XP_WIN32
    
	// Construct the App's owned and lost list.  Used to determine exactly which
	// file types were lost.  This construction should perform the comparison and
	// update the lost list.  The result will be written out to the registry.
	m_OwnedAndLostList.ConstructLists();
	
	MakeDefaultBrowser();
	
	m_OwnedAndLostList.WriteLists();

#endif // XP_WIN32
} // END OF	FUNCTION CNetscapeApp::CheckDefaultBrowser()


void CNetscapeApp::MakeDefaultBrowser()
{
#ifdef XP_WIN32

	// Pop up a list containing the lost file types.
	CDefaultBrowserDlg dlg;
	CPtrArray* lostList = m_OwnedAndLostList.GetLostList();
	if (theApp.m_OwnedAndLostList.NonemptyLostIgnoredIntersection())
		dlg.DoModal();

#endif // XP_WIN32
}

int CNetscapeApp::RuntimeIntSwitch(const char *pSwitch)	{
	int iRetval = -1;

	//	Search for the switch in the command line.
	char *pFound = strcasestr(m_lpCmdLine, pSwitch);
	if(pFound == NULL)	{
		return(iRetval);
	}

	//	Okay, found it.  Now, go beyond it, and return the value following as an int.
	char *pStart = pFound;
	pFound += strlen(pSwitch);
	while(isspace(*pFound))	{
		pFound++;
	}
	sscanf(pFound, "%d", &iRetval);

	//	Take the entire thing out of the command line.
	while(FALSE == isspace(*pFound) && *pFound != '\0')	{
		pFound++;
	}
	char *pTravEnd = pFound;
	char *pTraverse = pStart;

	*pTraverse = *pTravEnd;
	while(*pTraverse != '\0')	{
		pTraverse++;
		pTravEnd++;
		*pTraverse = *pTravEnd;
	}

	return(iRetval);
}

static BOOL IsCommand( const char *pszCmdLine, const char *pszOffset, const char *pszSwitch )
{
    //
    // Let's be somewhat smart and make sure this is really a command and not e.g., /netscape/<command>stuff.html
    // For now we just ensure the command is sandwiched between spaces e.g., "netscape.exe /<command> /somewhere/doc.html"
    //
    
    if( *pszOffset != '/' )
    {
        return TRUE;
    }
    
    if( pszOffset != pszCmdLine )
    {
        if( !isspace( *(pszOffset-1) ) )
        {
            // Must have a preceding space if not the beginning of the cmd line
            
            return FALSE;
        }
    }    
    
    if( !isspace( *(pszOffset + _tcslen( pszSwitch )) ) )
    {
        // Must have a traling space
        
        return FALSE;
    }
    
    return TRUE;
}

CString CNetscapeApp::RuntimeStringSwitch(const char *pSwitch, BOOL bCheckCommand)	
{
    CString csRetval;

	//	Search for the switch in the command line.
	char *pFound = strcasestr(m_lpCmdLine, pSwitch);
	
	//  Don't attempt to be smart about the command if asked not to be, as the
	//      command detector is not smart about complex switches like /printto
	//      being detected by /print.
	if( !pFound || (bCheckCommand && !IsCommand( m_lpCmdLine, pFound, pSwitch )) )	
    {
		return csRetval;
	}

	//	Okay, found it.  Now, go beyond it, and return the value following as a string.
	char *pStart = pFound;
	pFound += strlen(pSwitch);
	while(isspace(*pFound))	{
		pFound++;
	}

	//	Take the entire thing out of the command line.
    //  Respect quotes (spaces allowed inside of quotes).
    BOOL bQuote = FALSE;
	while((!isspace(*pFound) || bQuote) && *pFound != '\0')	{
        if(*pFound == '\"') {
            if(bQuote)  {
                bQuote = FALSE;
            }
            else    {
                bQuote = TRUE;
            }
        }
        csRetval += *pFound;
		pFound++;
	}
	char *pTravEnd = pFound;
	char *pTraverse = pStart;

    // OK, so pTraverse points to the first character of the command line switch
    //   and pTravEnd points to the first character after the string part of the
    //   command.  While there are bits of command line left copy them over
	while(*pTravEnd != '\0')	{
		*pTraverse++ = *pTravEnd++;
	}

    // Make sure we are NULL terminated
    *pTraverse = '\0';

	return(csRetval);
}

BOOL CNetscapeApp::IsRuntimeSwitch(const char *pSwitch, BOOL bRemove)	
{
    //  Search for the switch in the command line.
    //  Don't take it out by default
    char *pFound = strcasestr(m_lpCmdLine, pSwitch);
    if(pFound == NULL ||
        // Switch must be at beginning of command line
        //   or have a space in front of it to avoid
        //   mangling filenames
        ( (pFound != m_lpCmdLine) &&
          *(pFound-1) != ' ' ) ) {
        return(FALSE);
    }

    if (bRemove) {
        // remove the flag from the command line
        char *pTravEnd = pFound + strlen(pSwitch);
        char *pTraverse = pFound;

        *pTraverse = *pTravEnd;
        while(*pTraverse != '\0')   {
            pTraverse++;
            pTravEnd++;
            *pTraverse = *pTravEnd;
        }
    }

    return(TRUE);
}


BOOL CNetscapeApp::IsRuntimeSwitch(const char *pSwitch, char *pszCommandLine, BOOL bRemove)	
{
    //  Search for the switch in the command line.
    //  Don't take it out by default
    char *pFound = strcasestr(pszCommandLine, pSwitch);
    if(pFound == NULL ||
        // Switch must be at beginning of command line
        //   or have a space in front of it to avoid
        //   mangling filenames
        ( (pFound != pszCommandLine) &&
          *(pFound-1) != ' ' ) ) {
        return(FALSE);
    }

    if (bRemove) {
        // remove the flag from the command line
        char *pTravEnd = pFound + strlen(pSwitch);
        char *pTraverse = pFound;

        *pTraverse = *pTravEnd;
        while(*pTraverse != '\0')   {
            pTraverse++;
            pTravEnd++;
            *pTraverse = *pTravEnd;
        }
    }

    return(TRUE);
}

void CNetscapeApp::StoreVersionInReg() {
    //  Figure out the versioning information about the binary.
    CString csVersion;
	char *tmpBuf;
	char *ptr = NULL;

	csVersion = ResolveShortAppVersion();

	// we need to replace square brackets with parens to store into ini file
	tmpBuf = (char *)XP_ALLOC(csVersion.GetLength() + 1);
	strcpy(tmpBuf,csVersion);
	
	// point to start
	ptr = tmpBuf;
	while (ptr && *ptr) {
		if (*ptr == '[') *ptr = '(';
		if (*ptr == ']') *ptr = ')';
		ptr++;
	}

	long result;
#ifdef XP_WIN32
	HKEY hKeyRet = NULL;
	CString csSub = "SOFTWARE\\Netscape\\Netscape Navigator\\";

	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						csSub,
						NULL,
						KEY_SET_VALUE,
						&hKeyRet);

	if (result == ERROR_SUCCESS) {
		RegSetValueEx(hKeyRet,"CurrentVersion",NULL,REG_SZ,(const BYTE *)tmpBuf,strlen(tmpBuf)+1);
		RegCloseKey(hKeyRet);
	}
#else
	CString csNSCPini;
	login_GetIniFilePath(csNSCPini);

	result = ::WritePrivateProfileString("Netscape Navigator", "CurrentVersion", tmpBuf ,csNSCPini);   
#endif
	if (tmpBuf) XP_FREE(tmpBuf);
	return;

}

CString CNetscapeApp::ResolveShortAppVersion()   {
    //  Figure out the versioning information about the binary.
    CString csVersion;
    char *tmpversionBuf;

	tmpversionBuf = (char *)XP_ALLOC(15);
	//Loading from netscape.exe instead of resdll.dll since the string has been moved #109455
    LoadString(::AfxGetInstanceHandle(), IDS_APP_VERSION, tmpversionBuf, 15);
	csVersion = tmpversionBuf;
	csVersion += " [";
	csVersion += XP_AppLanguage;
	csVersion += "]";
	if (tmpversionBuf) XP_FREE(tmpversionBuf);
	return csVersion;
}

CString CNetscapeApp::ResolveAppVersion()   {
    //  Figure out the versioning information about the binary.
    CString csVersion;
    
	csVersion = ResolveShortAppVersion();

    //  Start building the return value.
    //  Version and Platform.
	BOOL bShowBuildBits = FALSE;
    CString csReturn;
    csReturn += csVersion;

	char *pCustAgent = NULL;
	PREF_CopyConfigString("user_agent",&pCustAgent);
	if (pCustAgent) {
		if (*pCustAgent) {
			csReturn += "C-";
			csReturn += pCustAgent;
			csReturn += ' ';
		}
		XP_FREE(pCustAgent);
	}

	csReturn += " (Win";

	if(sysInfo.m_bWin16 == TRUE)	{
		if(sysInfo.m_bWin32s == TRUE)	{
			csReturn += "32s";
			bShowBuildBits = TRUE;
		}
		else if(sysInfo.m_bWinNT == TRUE)	{
			csReturn += "NT";
			bShowBuildBits = TRUE;
		}
		else if(sysInfo.m_bWin4 == TRUE)	{
			csReturn += "95";
			bShowBuildBits = TRUE;
		}
		else	{
			csReturn += "16";
		}
	}
	else if(sysInfo.m_bWin32 == TRUE)	{
		if(sysInfo.m_bWin32s == TRUE)	{
			csReturn += "32s";
			bShowBuildBits = TRUE;
		}
		else if(sysInfo.m_bWinNT == TRUE)	{
			csReturn += "NT";
			if(sysInfo.m_dwMajor > 4) {
				csReturn += (char)('0' + sysInfo.m_dwMajor);
			}
		}
		else if(sysInfo.m_bWin4 == TRUE)	{
			if(sysInfo.m_dwMinor >= 10) {
				csReturn += "98";
			}
			else {
				csReturn += "95";
			}
		}
		else	{
			TRACE("What OS are you on anyhow?\n");
			csReturn += "dows";
			bShowBuildBits = TRUE;
			ASSERT(0);
		}
	}
	else	{
		TRACE("What OS are you on anyhow?\n");
		csReturn += "dows";
		bShowBuildBits = TRUE;
		ASSERT(0);
	}

    csReturn += "; ";

    //  Security.
    csReturn += SECNAV_SecurityVersion(PR_FALSE);

	//	Only show the build bits if the OS is ambiguous.
	if(bShowBuildBits == TRUE)	{
    	csReturn += "; ";

    	//  Build type.
#if defined(WIN32) || defined(__WIN32__)
    	csReturn += "32bit";
#else
    	csReturn += "16bit";
#endif
	}
   
#ifndef MOZ_COMMUNICATOR_NAME
    csReturn += " ;Nav";
#endif /* MOZ_COMMUNICATOR_NAME */    

    csReturn += ")";
    return(csReturn);
}

//
// Overload GetProfileString to get out of the registry if we are a Win32
//   application
//
CString CNetscapeApp::GetProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault)
{

#ifdef XP_WIN16

    return(CWinApp::GetProfileString(lpszSection, lpszEntry, lpszDefault)); 

#else

    HKEY  hKey;
    long result;
    DWORD type, size;
    char buffer[255];
    char * pString;
    static CString csStr;

    sprintf(buffer, "Software\\%s\\%s\\%s", "Netscape", "Netscape Navigator", lpszSection);

    csStr = lpszDefault;

    // get a pointer to this entry
    result = RegOpenKeyEx(HKEY_CURRENT_USER,
                            buffer,
                            NULL,
                            KEY_QUERY_VALUE,
                            &hKey);

    if(result != ERROR_SUCCESS)
        return(csStr);

    // see how much space we need
    size = 0;
    result = RegQueryValueEx(hKey,
                              (char *) lpszEntry,
                              NULL,
                              &type,
                              NULL,
                              &size);

    // if we didn't find it just use the default
    if((result != ERROR_SUCCESS) || (size == 0)) {
		RegCloseKey(hKey);
        return(csStr);
	}

    // allocate space to hold the string
    pString = (char *) XP_ALLOC(size * sizeof(char));

    // actually load the string now that we have the space
    result = RegQueryValueEx(hKey,
                              (char *) lpszEntry,
                              NULL,
                              &type,
                              (LPBYTE) pString,
                              &size);

    // use default id something went wrong this time
    if(result == ERROR_SUCCESS)
        csStr = pString;

    XP_FREE(pString);

    RegCloseKey(hKey);
	return(csStr);

#endif

}

//
// Overload GetProfileInt to get out of the registry if we are a Win32
//   application
//
UINT CNetscapeApp::GetProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault)
{

#ifdef XP_WIN16

    return(CWinApp::GetProfileInt(lpszSection, lpszEntry, nDefault)); 

#else

    HKEY  hKey;
    long result;
    DWORD type, size;
    char buffer[255];

    sprintf(buffer, "Software\\%s\\%s\\%s", "Netscape", "Netscape Navigator", lpszSection);

    result = RegOpenKeyEx(HKEY_CURRENT_USER,
                            buffer,
                            NULL,
                            KEY_QUERY_VALUE,
                            &hKey);

    // use default if something went wrong this time
    if(result != ERROR_SUCCESS)
        return(nDefault);

    int value;
    size = sizeof(int);
    result = RegQueryValueEx(hKey,
                              (char *) lpszEntry,
                              NULL,
                              &type,
                              (LPBYTE) &value,
                              &size);
	RegCloseKey(hKey);

    // use default if something went wrong this time
    if(result != ERROR_SUCCESS)
        return(nDefault);

    return(value);

#endif

}


//
// Overload WriteProfileString to write to the registry if we are a Win32
//   application
//
BOOL CNetscapeApp::WriteProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue)
{

#ifdef XP_WIN16

    return(CWinApp::WriteProfileString(lpszSection, lpszEntry, lpszValue)); 

#else
  
    HKEY  hKey;
    DWORD dwDisposition; 
    long result;
    char buffer[255];

    if (lpszValue == NULL) {
		sprintf(buffer, "Software\\%s\\%s\\%s", "Netscape", "Netscape Navigator", lpszSection);
	
		// Open the key to the section
		result = RegOpenKeyEx(HKEY_CURRENT_USER, buffer, NULL, KEY_WRITE, &hKey);
		
		if (result == ERROR_SUCCESS) {
			// Delete the value
			::RegDeleteValue(hKey, lpszEntry);
	
			// Close the key
			::RegCloseKey(hKey);
		}
		return TRUE;
	}

    // The MSDN CD sez RegSetValueEx() should create the key if it
    //   doesn't exist but it doesn't seem to be respecting hierarchies
    sprintf(buffer, "Software\\%s\\%s\\%s", "Netscape", "Netscape Navigator", lpszSection);
    result = RegCreateKeyEx(HKEY_CURRENT_USER,
                            buffer,
                            NULL,
                            NULL,
                            NULL,
                            KEY_ALL_ACCESS,
                            NULL,
                            &hKey,
                            &dwDisposition);

    if(result != ERROR_SUCCESS)
        return(0);

    result = RegSetValueEx(hKey,
                              lpszEntry,
                              NULL,
                              REG_SZ,
                              (LPBYTE) lpszValue,
                              XP_STRLEN(lpszValue) + 1);
	RegCloseKey(hKey);

    return(result == ERROR_SUCCESS);

#endif

}

//
// Overload WriteProfileInt to write to the registry if we are a Win32
//   application
//
BOOL CNetscapeApp::WriteProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue)
{

#ifdef XP_WIN16

    return(CWinApp::WriteProfileInt(lpszSection, lpszEntry, nValue)); 

#else
  
    HKEY  hKey;
    DWORD dwDisposition; 
    long result;
    char buffer[255];

    // The MSDN CD sez RegSetValueEx() should create the key if it
    //   doesn't exist but it doesn't seem to be respecting hierarchies
    sprintf(buffer, "Software\\%s\\%s\\%s", "Netscape", "Netscape Navigator", lpszSection);
    result = RegCreateKeyEx(HKEY_CURRENT_USER,
                            buffer,
                            NULL,
                            NULL,
                            NULL,
                            KEY_ALL_ACCESS,
                            NULL,
                            &hKey,
                            &dwDisposition);

    if(result != ERROR_SUCCESS)
        return(0);

    result = RegSetValueEx(hKey,
                              lpszEntry,
                              NULL,
                              REG_DWORD,
                              (LPBYTE) &nValue,
                              sizeof(int));
	RegCloseKey(hKey);

    return(result == ERROR_SUCCESS);

#endif

}


DWORD CNetscapeApp::GetPrivateProfileString(LPCSTR lpSectionName, LPCSTR lpKeyName,
	LPCSTR lpDefault, LPSTR lpReturnedString, DWORD nSize, LPCSTR lpFileName)	{
#ifdef XP_WIN16
	return((DWORD) ::GetPrivateProfileString(lpSectionName, lpKeyName,  lpDefault, lpReturnedString,
		CASTINT(nSize), lpFileName));
#else
	//	For safty, automatically copy two nulls into the return value.
	//	This will ensure any code looking into this string will fail.
	*lpReturnedString = *(lpReturnedString + 1) = '\0';

	//	We don't handle empty sections!
	//	However, we could with a little code.
	if(lpSectionName == NULL)	{
		ASSERT(0);
		return(0);
	}

	//	See if we're getting a full section, or just one specific key.
	if(lpKeyName != NULL)	{
		//	Name value pair requested.
		CString csValue = GetProfileString(lpSectionName, lpKeyName, lpDefault);
		if(csValue.IsEmpty() == TRUE)	{
			return(0);
		}

		//	Copy over the string.
		strncpy(lpReturnedString, csValue, nSize);

		//	End the string manually (might not have appended NULL).
		*(lpReturnedString + nSize - 1) = '\0';

		//	Return the length.
		return(strlen(lpReturnedString));
	}
	
	//	Create the appropriate key.
	//	We'll need to enumerate over everything.
	HKEY hKey;
	DWORD dwDisposition;
	char aBuffer[256];
	sprintf(aBuffer, "Software\\%s\\%s\\%s", "Netscape", "Netscape Navigator",
		lpSectionName);
	long lResult = RegCreateKeyEx(HKEY_CURRENT_USER, aBuffer, NULL, NULL, NULL,
		KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
	if(lResult != ERROR_SUCCESS)	{
		return(0);
	}

	//	Enumerate everything.
	//	We stick all values into the same string.
	//	Each is followed by a '\0'.
	//	The final one is followed by '\0''\0'.
	//	Be careful not to overrun the buffer size.
	char *pTraverse = lpReturnedString;
	char *pValue = new char[2048];
	char *pTraverseValue;
	DWORD dwValueSize;
	for(int iIndex = 0; 1; iIndex++)	{
		dwValueSize = 2048;
		if(RegEnumValue(hKey, iIndex, pValue, &dwValueSize, NULL, NULL, NULL, NULL) !=
			ERROR_SUCCESS)	{
			//	No more indexes/values left.
			break;
		}

		//	Copy this into the return value.
		pTraverseValue = pValue;
		while((DWORD)(pTraverse	- lpReturnedString) != nSize)	{
			*pTraverse = *pTraverseValue;
			pTraverse++;
			if(*pTraverseValue == '\0')	{
				break;
			}
			pTraverseValue++;
		}

		//	Check to see if we're at the end of the rope.
		if((DWORD)(pTraverse - lpReturnedString) == nSize)	{
			//	Need to set the final two chars to NULL.
			pTraverse -= 2;
			*pTraverse = '\0';
			pTraverse++;
			*pTraverse = '\0';
			pTraverse++;
			break;
		}
	}

	//	Done with the buffer and Key.
	delete[] pValue;
	RegCloseKey(hKey);

	//	Now, we make sure we have two NULLs at the end of the string.
	if(pTraverse == lpReturnedString)	{
		//	We didn't do squat.
		//	Increment this so that the return value is correct.
		pTraverse += 2;
	}
	else if((DWORD)(pTraverse - lpReturnedString) != nSize)	{
		*pTraverse = '\0';
		pTraverse++;
	}

	//	Return the number of chars copied over.
	return((DWORD)(pTraverse - lpReturnedString) - 2);
#endif
}

//
// This method re-implements the CWnd::SendMessageToDescendants() BUT
// only notifies permanent MFC windows...
//
void PASCAL CNetscapeApp::SendMessageToDescendants(HWND hWnd, UINT message,
                                                   WPARAM wParam, LPARAM lParam) 
{
	// walk through HWNDs to avoid creating temporary CWnd objects
	// unless we need to call this function recursively
	for (HWND hWndChild = ::GetTopWindow(hWnd); hWndChild != NULL;
		hWndChild = ::GetNextWindow(hWndChild, GW_HWNDNEXT))
	{
		// Don't send to non-permanent windows
		CWnd* pWnd = CWnd::FromHandlePermanent(hWndChild);
		if (pWnd != NULL)
		{
			// call window proc directly since it is a C++ window
            #ifdef XP_WIN32
			AfxCallWndProc(pWnd, pWnd->m_hWnd, message, wParam, lParam);
            #else
			_AfxCallWndProc(pWnd, pWnd->m_hWnd, message, wParam, lParam);
            #endif
    		if ( ::GetTopWindow(hWndChild) != NULL)
    		{
    			// Only recurse if current window is MFC
    			CNetscapeApp::SendMessageToDescendants(hWndChild, message, 
    			                                       wParam, lParam);
    		}
		}
	}
}

int CNetscapeApp::MessageBox(HWND hWnd,
                             LPCSTR lpszText,
                             LPCSTR lpszCaption /* = NULL */,
	                         UINT nType /* = MB_OK */)
{
	if (lpszCaption == NULL)
		lpszCaption = AfxGetAppName();

	m_splash.ShowWindow(SW_HIDE);
    int result = ::MessageBox(hWnd, lpszText, lpszCaption, nType);
    m_splash.ShowWindow(SW_SHOW);
    m_splash.UpdateWindow();

	return result;
}

//  Register misc shell extensions supported.
void CNetscapeApp::EnableShellStuff()
{
	char aPath[_MAX_PATH];
	if(::GetModuleFileName(m_hInstance, aPath, _MAX_PATH))  {
#ifdef XP_WIN32
		//	should use short file name.
		char aShortPath[_MAX_PATH + 1];
		if(::GetShortPathName(aPath, aShortPath, sizeof(aShortPath)))	{
			strcpy(aPath, aShortPath);
		}
#endif

        CString csMunge;

        //  The Print Command.
        csMunge = aPath;
        csMunge += " /print(\"%1\")";
        FEU_RegistryWizard(HKEY_CLASSES_ROOT, "NetscapeMarkup\\shell\\print\\command", csMunge);
        FEU_RegistryWizard(HKEY_CLASSES_ROOT, "NetscapeMarkup\\shell\\print\\ddeexec", "[print(\"%1\")]");
        FEU_RegistryWizard(HKEY_CLASSES_ROOT, "NetscapeMarkup\\shell\\print\\ddeexec\\Application", strDDE_APP_NAME);

        //  The PrintTo Command.
        csMunge = aPath;
        csMunge += " /printto(\"%1\",\"%2\",\"%3\",\"%4\")";
        FEU_RegistryWizard(HKEY_CLASSES_ROOT, "NetscapeMarkup\\shell\\PrintTo\\command", csMunge);
        FEU_RegistryWizard(HKEY_CLASSES_ROOT, "NetscapeMarkup\\shell\\PrintTo\\ddeexec", "[printto(\"%1\",\"%2\",\"%3\",\"%4\")]");
        FEU_RegistryWizard(HKEY_CLASSES_ROOT, "NetscapeMarkup\\shell\\PrintTo\\ddeexec\\Application", strDDE_APP_NAME);
    }
}



/////////////////////////////////////////////////////////////////////////////
// Exit the whole application
void CNetscapeApp::OnAppSuperExit()
{
    //	Dont ask user.
    CommonAppExit();
}

void CNetscapeApp::OnAppExit()
{

    // if more than one top level window prompt to make sure they
    //	 want to exit
    if(m_pFrameList && m_pFrameList->m_pNext) {
	int iExit = ::MessageBox(NULL,
				 szLoadString(IDS_CLOSEWINDOWS_EXIT),
				 szLoadString(IDS_EXIT_CONFIRMATION),
				 MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);

        // false alarm, user didn't really want to exit
	if(IDNO == iExit)
	    return;

    }

    CommonAppExit();
}


void CNetscapeApp::CommonAppExit()
{
	// Make sure each frame window really does want to close
    for (CGenericFrame *pFrame = m_pFrameList; NULL != pFrame; pFrame = pFrame->m_pNext) {
		CDocument*	pDocument = pFrame->GetActiveDocument();

	if (pDocument && !pDocument->CanCloseFrame(pFrame)) {
                        // Document doesn't want to close so don't close any of the frames

	    // Clear the flags previously set
	    //	so we prompt to save edit changes next time we try to close
	    CGenericFrame *pCurrentFrame = pFrame;
	    for (pFrame = m_pFrameList; pCurrentFrame != pFrame; pFrame = pFrame->m_pNext) {
		 pFrame->m_bSkipSaveEditChanges = FALSE;
	    }

            // We aren't going to close -- get out!
	    return;
		}
	// Set flag to prevent prompting for saving edit changes
	//  in CEditFrame::OnClose() since this was done in CanCloseFrame()
	pFrame->m_bSkipSaveEditChanges = TRUE;
	}

	//  Set wether or not the app is exiting.
    m_bExit = TRUE;

#ifdef MOZ_MAIL_NEWS
    WFE_MSGSearchClose();
    WFE_MSGLDAPSearchClose();
#endif /* MOZ_MAIL_NEWS */

    //	Go through each frame, closing it....
    CGenericFrame *pNext = NULL;
    for(pFrame = m_pFrameList; NULL != pFrame; pFrame = pNext) {

	//  Extract the frame from the list.
	pNext = pFrame->m_pNext;

	pFrame->PostMessage(WM_CLOSE);
    }
  
}

#ifdef MOZ_LOC_INDEP
// LI_STUFF start
// REMIND: This is all experimental stuff which will get cleaned up and moved when 
// we are done
void putFileClosure(void * closure, LIStatus result)
{
	LIActive = 0;
}

void putCookieFileClosure(void * closure, LIStatus result)
{
	// a cheesy place to put this. until I do clients
	LIActive --;
	
}

void getCookieFileClosure(void * closure, LIStatus result, XP_Bool fileExists)
{
	if (fileExists) {
		/* remove and reset the cookies. */
		NET_RemoveAllCookies();
		NET_ReadCookies("");
	}
}

BOOL CNetscapeApp::LIStuffEnd() {
	static BOOL beenHere = FALSE;

	/* if li is active then go through this to upload stuff, 
		when the uploads are done, isliactive gets set to false.
	*/
	if (LIActive > 0) {
		if (beenHere)
			return FALSE;

		//To save the prefs before the upload all: 
		char * prefName = WH_FileName(NULL, xpLIPrefs);
		PREF_SaveLIPrefFile(prefName);
		XP_FREEIF (prefName);

		LIMediator::uploadAll(putFileClosure, NULL);
		beenHere = TRUE;


		/* don't close the browser yet */ 
		return FALSE;
	}
	/* ok to close browser */
	return TRUE;
}

void LIStuff() {
	char *	lifilename ;
	BOOL prefBool;

	PREF_GetBoolPref("li.enabled", &LIActive);

	if (LIActive > 0) {
		PREF_GetDefaultBoolPref("li.client.bookmarks", &prefBool);
		if (prefBool) {
			//BM_SetModified(theApp.m_pBmContext, FALSE); // REMIND - still needed?
			//LIFile * file = new LIFile (theApp.m_pBookmarkFile,"bookmarks", "Bookmarks File");
			//file->startGettingIfNecessary(getBookmarkFileClosure, file);
		}
		//else
		//	BM_ReadBookmarksFromDisk(theApp.m_pBmContext, theApp.m_pBookmarkFile, NULL );
		// RDF is going to have to be hooked up here.  (Dave H.)
		// WORK STILL NEEDS TO BE DONE

		PREF_GetDefaultBoolPref("li.client.cookies",  &prefBool);
		if (prefBool) {
			lifilename = WH_FileName("", xpHTTPCookie);
			if (lifilename) {
				LIFile * file2 = new LIFile (lifilename, "cookies", "Cookies File");
				file2->startGettingIfNecessary(getCookieFileClosure, file2);
				XP_FREE(lifilename);
			}
		}
	}
	//else
	//	BM_ReadBookmarksFromDisk(theApp.m_pBmContext, theApp.m_pBookmarkFile, NULL );
}
#endif // MOZ_LOC_INDEP

/////////////////////////////////////////////////////////////////////////////
// CNetscapeApp commands

#if _MSC_VER > 1100
//  Warn others of possible logic missing from newer versions of MFC and NS code.
#pragma message(__FILE__ ": Check CNetscapeApp::[Run|NSPumpMessage|IsIdleMessage] for compatibility with new MFC source")
#endif

//  Mainly as CWinThread::Run except for the NSPumpMessage call (PumpMessage non virtual on win16).
int CNetscapeApp::Run()
{
	BOOL bIdle = TRUE;
	LONG lIdleCount = 0;

#ifdef MOZ_LOC_INDEP
	LIStuff();
#endif // MOZ_LOC_INDEP

    for(;;) {
	while(bIdle && !::PeekMessage(&m_msgCur, NULL, NULL, NULL, PM_NOREMOVE))    {
	    if (!OnIdle(lIdleCount++))
		bIdle = FALSE;
	}

#ifdef XP_WIN16
#if defined(OJI) || defined(JAVA) || defined(MOCHA)
	/*
	** On Win16 the only way for another thread to run is to explicitly
	** yield...
	*/
	extern void fe_yield(void);
	fe_yield();
#endif	/* OJI || JAVA || MOCHA */
#endif	/* XP_WIN16 */

	do  {
	    if(!NSPumpMessage())    {
		return(ExitInstance());
	    }

	    if(IsIdleMessage(&m_msgCur)) {
		bIdle = TRUE;
		lIdleCount = 0;
	    }
	} while(::PeekMessage(&m_msgCur, NULL, NULL, NULL, PM_NOREMOVE));
    }
}

//  Mainly as CWinThread::PumpMessage except for giving priority to certain events.
//  NSPumpMessage instead of PumpMessage as non-virtual on Win16
BOOL CNetscapeApp::NSPumpMessage()
{
    BOOL bRetval = FALSE;
    BOOL bPeeked = FALSE;

    //	Net dike (see hiddenfr.h).
    gNetFloodStage++;

    if((gNetFloodStage % NET_FLOWCONTROL) != 0) {
	bPeeked = bRetval = ::PeekMessage(&m_msgCur, NULL, 0, NET_MESSAGERANGE, PM_REMOVE);
	if(bPeeked && m_msgCur.message == WM_QUIT) {
	    bRetval = FALSE;
	}
    }

    //	As normal PumpMessage
    if(!bPeeked)    {
	//  Wait for message or getting a registered message.
	bRetval = ::GetMessage(&m_msgCur, NULL, NULL, NULL);
    }

    if(bRetval) {
	//  Update tick count used in GetMessageTime call.
	//  This can be used as an ID to tell wether or not a message
	//	was generated from this dispatch function.
	m_dwLastMsgTick = m_msgCur.time;

	//  Update time by MSG tickcount.
	//  Allow 30 second play period for messages before resetting clock or
	//	assuming roll over on the tick counter (49.7 days)
	if(m_msgCur.time > m_dwMsgTick) {
	    m_dwMsgTick = m_msgCur.time;
	}
	else if((m_msgCur.time + 30000) < m_dwMsgTick) {
	    InitTime();
	}

	if(m_msgCur.message != WM_KICKIDLE && !PreTranslateMessage(&m_msgCur))	{
	    ::TranslateMessage(&m_msgCur);
	    ::DispatchMessage(&m_msgCur);
	}
    }

    return(bRetval);
}

BOOL CNetscapeApp::IsIdleMessage(MSG *pMsg)
{
	ASSERT(pMsg);

	if (pMsg->message == msg_NetActivity ||
	pMsg->message == msg_FoundDNS ||
	pMsg->message == msg_ForceIOSelect ||
	pMsg->message == m_msgNSPREventNotify) {
		return FALSE;
	}

        // We have to be careful about how often we go idle. We don't want timer
	// messages (e.g. animation timer) or Winsock async notifications to cause
	// us to go idle. We need to filter WM_GETDLGCODE messages, because clicking
	// in the URL bar generates an endless stream of them
	switch (pMsg->message) {
	case WM_KEYDOWN:
	    if (pMsg->wParam == VK_SHIFT || pMsg->wParam == VK_CONTROL) {
		// Avoid consuming all of the CPU when the Shift/Control
		// keys are held down
		if (LOWORD(pMsg->lParam) > 0) {
		    return FALSE;
		}
	    }
	    break;

	    case WM_TIMER:
	    case WM_GETDLGCODE:
	    case WM_MOUSEMOVE:
	    case WM_NCMOUSEMOVE:
		    return FALSE;
	}

    //	As CWinThread::IsIdleMessage
    if(pMsg->message == WM_MOUSEMOVE || pMsg->message == WM_NCMOUSEMOVE)    {
	if (m_ptCursorLast == pMsg->pt && pMsg->message == m_nMsgLast)	{
	    return FALSE;
	}

	m_ptCursorLast = pMsg->pt;
	m_nMsgLast = pMsg->message;
	return TRUE;
    }

    return(pMsg->message != WM_PAINT && pMsg->message != 0x0118);
}

BOOL CNetscapeApp::OnIdle(LONG lCount)
{
    // call base class idle first
    BOOL bResult = CWinApp::OnIdle(lCount);

        // ZZZ: We don't need to do this in the 32-bit version, because CWinApp::InIdle
	// already sends a WM_IDLEUPDATECMDUI to each of the frames

#ifdef XP_WIN16
#if defined(OJI) || defined(JAVA) || defined(MOCHA)
    /*
    ** On Win16 the only way for another thread to run is to explicitly
    ** yield...
    */
    extern void fe_yield(void);
    fe_yield();
#endif	/* OJI || JAVA || MOCHA */

    //	Update the UI of the frames.
    if(lCount == 0)    {
	CGenericFrame * f;

	// update command buttons etc from the top down
	for(f = m_pFrameList; f; f = f->m_pNext) {
	    if (f) {
		CNetscapeApp::SendMessageToDescendants(f->m_hWnd,
				WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
	    }
	}
	return(TRUE);
    }
#endif

    //	Do we really have to do manual binding to timeout code?
    //	This is set if the code was unable to allocate a timer.
    if(m_bIdleProcessTimeouts)	{
        TRACE("Yo!: processing timeouts in idle binding.\n");
	wfe_ProcessTimeouts();
	bResult = TRUE;
    }

	// Poll the socket list and call Netlib
	if(NET_PollSockets())
		bResult = TRUE;

#ifdef MOZ_MAIL_NEWS
	// currently just to give NeoAccess a chance to do some chores
        // If we become a real CNeoApp, this won't be neccesary.
    //	--- DREAM ON ---
	MSG_OnIdle();
#endif /* MOZ_MAIL_NEWS */
    
    //  Good time to free up heap and unused COM Dlls when really going idle.
    if(bResult == FALSE) {
	CoFreeUnusedLibraries();
#ifdef XP_WIN32
	_heapmin();
#endif
    }

	return(bResult);
}

BOOL CNetscapeApp::OnDDECommand(char *pszCommand)
{
    TRACE("DDE command:  %s\n", pszCommand);

////////////  The block below belongs to Abe Jarrett /////////////
//	Intercept all commandline DDE commands before Intercepting Print commands
    if (strcasestr(pszCommand,"cmdline") != NULL)
	{
        if (ProcessCommandLineDDE(pszCommand))
            return TRUE;
        else
            return FALSE;
    }
////////////  The above belongs to Abe Jarrett  /////////////////////////
//////////////////////////////////////////////////////////////////////////////
//	Intercept all print commands before we reach the below.
    BOOL bPrint = FALSE;
    BOOL bPrintTo = FALSE;
    char *pszCompare = pszCommand + 1;

    if(strnicmp(pszCompare, "print(", 6) == 0) {
	bPrint = TRUE;
        TRACE("Handling command to print\n");
    }
    else if(strnicmp(pszCompare, "printto(", 8) == 0) {
	bPrintTo = TRUE;
        TRACE("Handling command to printto\n");
    }
    if(bPrint || bPrintTo)  {
	//  There are four arguments that we figure out for each print job.
	//  1)	What to print.
	//  2)	The destination printer
	//  3)	The printer driver used for printing
	//  4)	The destination port for printing
	//
	//  They will all be represented as strings.
	//  All but 1 can be NULL, which means to use the default setting.
	//  There should be no UI interaction except for authentication and other such
	//	backend stuff that we have no control over.
	//  Each argument is quoted.
	//  Each quoted argument is seperated by a comma.
	char *pArg1 = FEU_ExtractCommaDilimetedQuotedString(pszCommand, 1);
	char *pArg2 = FEU_ExtractCommaDilimetedQuotedString(pszCommand, 2);
	char *pArg3 = FEU_ExtractCommaDilimetedQuotedString(pszCommand, 3);
	char *pArg4 = FEU_ExtractCommaDilimetedQuotedString(pszCommand, 4);

	//  Do it.
	CPrintCX::AutomatedPrint(pArg1, pArg2, pArg3, pArg4);

	//  Get rid of any allocations that FEU did for us.
	if(pArg1)   {
	    XP_FREE(pArg1);
	}
	if(pArg2)   {
	    XP_FREE(pArg2);
	}
	if(pArg3)   {
	    XP_FREE(pArg3);
	}
	if(pArg4)   {
	    XP_FREE(pArg4);
	}

	//  Handled.
	return(TRUE);
    }

    BOOL bTranslate = TRUE;
    //	Copy the string to mess with.
    char *pDelete = XP_STRDUP(pszCommand);
    char *pOpen = pDelete;

#ifdef EDITOR
    // edit format is "[edit("%s")]" - no whitespace allowed, one per line
    if (strnicmp(pOpen, "[edit(\"", 7) == 0)   {
        pOpen += 7;
        char *pEnd = strchr(pOpen, '"');
	    if (pOpen == NULL) {
            XP_FREE(pDelete);
		    return FALSE;	    // illegally terminated
        }
	    // trim the string, and edit the file
        *pEnd = '\0';
        // This will do the WFE_ConvertFile2Url before loading URL
        FE_LoadUrl( pOpen, LOAD_URL_COMPOSER);
        XP_FREE(pDelete);
        return TRUE;
    }
#endif /* EDITOR */    

    // open format is "[open("%s")]" - no whitespace allowed, one per line
    if (strnicmp(pOpen, "[open(\"", 7) != 0)
    {
        if ( strnicmp(pOpen, "[openurl(\"", 10) != 0 ) 
        {
            XP_FREE(pDelete);   //Added by CLM - memory leak       
			return FALSE;
	    }
		else
		{
			bTranslate = FALSE;
			pOpen += 10;
		}
    }
	else
    {
		pOpen += 7;
    }

    char *pEnd = strchr(pOpen, '"');
	if (pEnd == NULL)   
    {
        XP_FREE(pDelete);
		return FALSE;	    // illegally terminated
    }
	// trim the string, and open the file
    *pEnd = '\0';

    //	Convert the name to a URL.
	CString csUrl;
	WFE_ConvertFile2Url(csUrl, pOpen);

    //	Open it in a new window.
    //	Use the first window as the provider of the context.
	URL_Struct *pUrl = NET_CreateURLStruct(csUrl, NET_DONT_RELOAD);
	if(m_pFrameList && m_pFrameList->GetMainContext())
	{
        if (bTranslate)
        {
	        MWContext *pContext = m_pFrameList->GetMainContext()->GetContext();
	        CFE_CreateNewDocWindow(pContext, pUrl);
        }
        else
        {
	        CFrameGlue * pFrame =
	        CFrameGlue::GetLastActiveFrame(MWContextBrowser);
	        if (pFrame != NULL && pFrame->GetMainContext()  && 
		        pFrame->GetMainContext()->GetContext()      &&
		        !pFrame->GetMainContext()->GetContext()->restricted_target)
	        {
		        CAbstractCX * pCX = pFrame->GetMainContext();
		        if (pCX != NULL)
		        {
                    CFrameWnd * pFrameWnd = pFrame->GetFrameWnd();
                    if( pFrameWnd ){
                        // We need to bring window to the top
                        if(pFrameWnd->IsIconic())  {
                            pFrameWnd->ShowWindow(SW_RESTORE);
                        }
                        pFrameWnd->BringWindowToTop();
                    }
			        pCX->NormalGetUrl(csUrl);
		        }  /* end if */
	        }  /* end if */
	        else
	        {
		        CFE_CreateNewDocWindow(NULL, pUrl);
	        }  /* end else */
	    }  /* end else */
    }
	else
	{
		CFE_CreateNewDocWindow(NULL, pUrl);
	}

    XP_FREE(pDelete);
    return(TRUE);
}

//
// Generic help handler
//
void CNetscapeApp::OnHelp()
{
#ifdef XP_WIN16
    if (SENT_MESSAGE!=m_helpstate)
    {
        if (m_pMainWnd)
        { 
            CWnd *t_wnd=m_pMainWnd->GetActiveWindow();
            if (t_wnd)
            {
                m_helpstate=SENT_MESSAGE;
                t_wnd->SendMessage(WM_COMMAND,ID_HELP);
                m_helpstate=NOT_SENT;
            }
        }
    }
#endif
}
void CNetscapeApp::ReleaseAppFont(HFONT logFont)
{
	XP_List *theList = 	m_appFontList;
	NsWinFont* theFont = (NsWinFont*)XP_ListNextObject(theList);
	while (theFont) {
		if (theFont->hFont == logFont)
			break;
		theFont = (NsWinFont*)XP_ListNextObject(theList);
	}
	if (theFont) {  // release the font use.
		theFont->refCount--;
		if (theFont->refCount == 0) { // no one use this font anymore, release it.
			XP_ListRemoveObject (m_appFontList, theFont);
			VERIFY(::DeleteObject(theFont->hFont));
			XP_DELETE(theFont);
			theFont = NULL;
		}
	}
}
		
HFONT CNetscapeApp::CreateAppFont(LOGFONT& logFont)
{
	NsWinFont* theFont;
	if (m_appFontList == NULL) {
		m_appFontList = XP_ListNew();
		theFont = XP_NEW( NsWinFont);
		memcpy(&theFont->logFontStruct, &logFont, sizeof(LOGFONT));
		theFont->hFont = CreateFontIndirect( &logFont );
		theFont->refCount = 1;
		XP_ListAddObject (m_appFontList, (void*)theFont);
	}
	else {
		XP_List *theList = 	m_appFontList;
		theFont = (NsWinFont*)XP_ListNextObject(theList);
		while (theFont) {
			const LOGFONT& l1 = logFont;
			const LOGFONT& l2 = theFont->logFontStruct;
			if ((memcmp(&l1, &l2, sizeof(LOGFONT) - sizeof(l1.lfFaceName)) == 0) &&
				(strcmp(l1.lfFaceName, l2.lfFaceName) == 0))
				break;
			theFont = (NsWinFont*)XP_ListNextObject(theList);
		}
		if (!theFont) {  // we do not found a font cached fot this LOGFONT yet.  
			theFont = XP_NEW( NsWinFont);
			memcpy(&theFont->logFontStruct, &logFont, sizeof(LOGFONT));
			theFont->hFont = CreateFontIndirect( &logFont );
			XP_ListAddObject (m_appFontList, (void*)theFont);
			theFont->refCount = 1;
		}
		else {
			theFont->refCount++;
		}
	}
	return theFont->hFont;
}

CNSNavFrame* CNetscapeApp::CreateNewNavCenter(CNSGenFrame* pParentFrame, BOOL useViewType, HT_ViewType viewType)
{
	CNSNavFrame* theItem;
	theItem = new CNSNavFrame();
	theItem->CreateNewNavCenter(pParentFrame, useViewType, viewType);
	return theItem;
}

