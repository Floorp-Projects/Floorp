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
#include "mmsystem.h"
#include "apiapi.h"
#ifdef MOZ_MAIL_NEWS
#include "msgcom.h"
#include "apimsg.h"
#endif

#ifdef MOZ_MAIL_NEWS
#include "msgcom.h"
#endif
#include "fe_proto.h"
#include "hiddenfr.h"
#ifdef MOZ_MAIL_NEWS
#include "mailfrm.h"
#include "mailmisc.h"
#include "mnprefs.h"
#include "compfrm.h"
#endif
#include "template.h"
#include "prefapi.h"
#ifdef MOZ_MAIL_NEWS
#include "winbif.h"
#endif

/* This function is defined in msgfrm.cpp
   Declare here to avoid including the whole header file */
#ifdef MOZ_MAIL_NEWS   
MSG_Master *WFE_MSGGetMaster();
#endif 

extern "C" MSG_Master* FE_GetMaster(){
#ifdef MOZ_MAIL_NEWS 
	return WFE_MSGGetMaster();
#else
   return(NULL);
#endif /* MOZ_MAIL_NEWS */
}

// prototypes
BOOL SetBiffInfoNum(LPCSTR pKey, uint32 num);



//
// I'm sure this already exists...
//
CGenericFrame *wfe_FrameFromXPContext(MWContext * pXPCX)
{
	if(!pXPCX)
		return(NULL);

	CFrameGlue * pFrameGlue = GetFrame(pXPCX);
	if(pFrameGlue) {
		CFrameWnd * pFrame = pFrameGlue->GetFrameWnd();
		if(pFrame && pFrame->IsKindOf(RUNTIME_CLASS(CGenericFrame)))
			return((CGenericFrame*)pFrame);
	}

	return(NULL);
}

extern "C" int FE_GetURL(MWContext *pContext, URL_Struct *pUrl)	
{
	if(!pContext || !ABSTRACTCX(pContext))
		return(-1);
#ifdef EDITOR
    // The only circumstance in which we load a URL
    //   into Editor when called from XP code is during
    //   Publishing: we have "files to post"
    if( EDT_IS_EDITOR(pContext) && pUrl->files_to_post == NULL ){
        // So switch to or create a Browser frame/context
        // TODO: A possibly bad side effect is that changing
        //   frames will obscure the last active frame 
        //   (and  possibly a preference dialog)
        CGenericFrame* pFrame = (CGenericFrame*)FEU_GetLastActiveFrame(MWContextBrowser);
        pContext = NULL;
        if( pFrame && pFrame->GetMainContext() ){
            pContext = pFrame->GetMainContext()->GetContext();
            if( pContext ){
                // Find previously-active Browser frame
                if ( pFrame->IsIconic() ){
                    pFrame->ShowWindow(SW_RESTORE);
                }
                pFrame->SetActiveWindow();
            }
        } else {
            // Create a new Browser frame
            pContext = CFE_CreateNewDocWindow(NULL, NULL);
        }
        if( !pContext ){
            return -1;
        }
    }
#endif
    return(ABSTRACTCX(pContext)->GetUrl(pUrl, FO_CACHE_AND_PRESENT));
}

//
// The must return a unix style path.
//   We'll have to figure out how to convert Win9x "\\fuzzhead\d-drive\span\mail"
//   into a unix path at some point.
//
extern "C" const char* FE_GetFolderDirectory(MWContext *pContext)	
{
#ifdef MOZ_MAIL_NEWS
    static char pStr[_MAX_PATH];
	pStr[0] = '\0';

	if (!g_MsgPrefs.m_csMailDir.IsEmpty())
	{
	   	sprintf(pStr, "/%s", (LPCTSTR) g_MsgPrefs.m_csMailDir);
    
	    for(char * p = pStr; p && *p; p++)
	        if(*p == '\\')
	            *p = '/';
	        else if(*p == ':')
	            *p = '|'; 
	}

    return(pStr);
#else  // MOZ_MAIL_NEWS
   return("");
#endif // MOZ_MAIL_NEWS   
       
}

extern "C" uint32
FE_DiskSpaceAvailable (MWContext *context, const char *lpszPath )
{
	char aDrive[_MAX_DRIVE];
	_splitpath( lpszPath, aDrive, NULL, NULL, NULL);

	if(aDrive[0] == '\0')	{

        // The back end is always trying to pass us paths that look
        //   like /c|/netscape/mail.  See if we've got one of them
        if(strlen(lpszPath) > 2 && lpszPath[0] == '/' && lpszPath[2] == '|') {
            aDrive[0] = lpszPath[1];
            aDrive[1] = ':';
            aDrive[2] = '\0';
        }
        else {
            // Return bogus large number and hope for the best
            TRACE("Unable to determine drive, can't check free disk space.\n");
            return 1L<<30; 
        }
    }

	CString csDrive = aDrive;
	csDrive += '\\';
	TRACE("Checking drive %s disk space\n", (const char *)csDrive);

	DWORD dwSectorsPerCluster = 0;
	DWORD dwBytesPerSector = 0;
	DWORD dwFreeClusters = 0;
	DWORD dwTotalClusters = 0;
	if(GetDiskFreeSpace(csDrive, 
						&dwSectorsPerCluster, 
						&dwBytesPerSector, 
						&dwFreeClusters, 
						&dwTotalClusters) == FALSE)	{
		TRACE("Couldn't get free disk space...\n");
		return 1L<<30; // Return bogus large number and hope for the best
	}

	//	We can now figure free disk space.
	DWORD dwFreeSpace = dwFreeClusters * dwSectorsPerCluster * dwBytesPerSector;
	TRACE("%lu free bytes\n", dwFreeSpace);

 	return dwFreeSpace;
}

//
// Generate a temp file name for a file that can eventually maybe be renamed
//   to pFileName.  We could do something more intelligent
//   about putting it in the right directory and all that but not today
//
extern "C" char *FE_GetTempFileFor(MWContext *pContext, const char *pFileName, XP_FileType ftype, XP_FileType *rettype)	
{
    char * pString = WH_TempFileName(ftype, NULL, NULL);
    if(!pString)
        return(NULL);

    if(rettype)
        *rettype = xpTemporary;

	return(pString);
}

#ifdef MOZ_MAIL_NEWS
extern "C" void FE_MsgShowHeaders(MSG_Pane *pPane, MSG_HEADER_SET mhsHeaders)	
{
}

extern "C" void FE_UpdateCompToolbar(MSG_Pane *pPane)	
{
  CComposeFrame * pCompose = (CComposeFrame *) MSG_GetFEData(pPane);
  if ( pCompose && pCompose->GetMainContext() && !MSG_DeliveryInProgress(pPane))
  {
    // Stop animation in corner and (more importantly) kill the progress bar.
    MWContext *pContext = pCompose->GetMainContext()->GetContext();
    if (pContext && !XP_IsContextBusy(pContext))
    {
      pCompose->GetChrome()->StopAnimation();

      pCompose->UpdateComposeWindowForMAPI(); // rhp - for MAPI operations
    }
  }
}
#endif /* MOZ_MAIL_NEWS */

//
// Prompt the user for a local file name
//
extern "C" int FE_PromptForFileName(MWContext *context,
								 const char *prompt_string,
								 const char *default_path,
								 XP_Bool file_must_exist_p,
								 XP_Bool directories_allowed_p,
								 ReadFileNameCallbackFunction fn,
								 void *closure)
{
	HWND hWnd = NULL;
	CGenericFrame * pWnd = wfe_FrameFromXPContext(context);
	if(pWnd)
		hWnd = pWnd->m_hWnd;
	else if (context) {	// XXX WHS Do what we really should have done
		CWnd *pWnd2 = (ABSTRACTCX(context))->GetDialogOwner();
		ASSERT(pWnd2 != CWnd::GetDesktopWindow());
		if (pWnd2 && pWnd2 != CWnd::GetDesktopWindow())
			hWnd = pWnd2->m_hWnd;
	}
	ASSERT(hWnd); // Bad!

    char * pFile = NULL;
    if (file_must_exist_p)
    {
        // Doing a file open
        // check for PKCS 12 files 
        if(default_path && !strcmp(default_path, "*.p12")) {
            default_path = NULL;
            pFile = wfe_GetExistingFileName(hWnd, (char *)prompt_string, P12,
                file_must_exist_p, FALSE);
        } else {
            //  Changed to prompt for ALL, instead defaulting to first filter.
            pFile = wfe_GetExistingFileName(hWnd , (char *)prompt_string, ALL,
                file_must_exist_p, FALSE);
        }
    }  /* end if */
	else if (context->type == MWContextRDFSlave)
	{
		CString cs;
		cs.LoadString(IDS_UNTITLED);
		// Since someone undid my work for untyped title strings.
		cs += ".rdf";
        pFile = wfe_GetSaveFileName(hWnd, (char *)prompt_string, (char *) (LPCTSTR) cs, 0);
	} 

#ifdef MOZ_MAIL_NEWS
	else if (directories_allowed_p) {
		CDirDialog directory(ABSTRACTCX(context)->GetDialogOwner(), default_path);
        if (IDOK == directory.DoModal())
    	{
            CString fileName = directory.GetPathName();
			/* There is a reason for this k5bg bogusness */
			/* Whatever well-meaning person coded CDirPicker in mnprefs.cpp did it wrong */
			/* this is a hidden string we are getting rid of */
            int nPos = fileName.GetLength() - strlen("\\k5bg");
            CString pathName(fileName.Left(nPos));
            pFile = XP_STRDUP(pathName);
	    }
    }
    else
    {
		CString cs;

        // Doing a file save
		if ((context->type == MWContextMail) || 
			(context->type == MWContextNews) ||
			(context->type == MWContextMailMsg) ||
			(context->type == MWContextNewsMsg) ) {
			// Suggest an untyped file name
			cs.LoadString(IDS_UNTITLED);
			// Since someone undid my work for untyped title strings.
			cs += ".txt";
        	pFile = wfe_GetSaveFileName(hWnd, (char *)prompt_string, (char *) (LPCTSTR) cs, 0);
		} else if (context->type == MWContextBookmarks) {
			// Suggest the default bookmarks file
        	pFile = wfe_GetSaveFileName(hWnd, (char *)prompt_string, (char *)(const char *)theApp.m_pBookmarkFile, 0);
		} else if((default_path != NULL) && !strcmp(default_path, "*.p12")) {
			int p12type = P12;
			default_path = NULL;
        	pFile = wfe_GetSaveFileName(hWnd, (char *)prompt_string, (char *)default_path, &p12type);
		} else {
        	pFile = wfe_GetSaveFileName(hWnd, (char *)prompt_string, (char *)default_path, 0);
		}
    }  /* end else */
#endif

	if(!pFile)
		return(-1);

	fn(context, pFile, closure);

    return(0);
}

extern "C" int FE_PromptForNewsHost (MWContext *context,
								 const char *prompt_string,
								 ReadFileNameCallbackFunction fn,
								 void *closure)
{

    char * pString = FE_Prompt(context, prompt_string, "");

    if(pString) {

        // make sure we have a correct looking news URL
        if(strncmp(pString, "news://", 7) && strncmp(pString, "snews://", 8)) {
            char * pNewString = (char *) XP_ALLOC(10 + XP_STRLEN(pString));
            sprintf(pNewString, "news://%s", pString);
            XP_FREE(pString);
            pString = pNewString;
        }

        fn(context, pString, closure);
    }

    return(0);
}

#ifdef MOZ_MAIL_NEWS
extern "C" ABook * FE_GetAddressBook ( MSG_Pane *pPane)
{
    return theApp.m_pABook;
}


extern "C" XP_List * FE_GetDirServers ()
{
    return theApp.m_directories;
}


extern "C" MWContext * FE_GetAddressBookContext ( MSG_Pane *pPane, XP_Bool viewnow )
{
    return theApp.m_pAddressContext;
}
#endif

//
// Send back the current user's email address
//
extern "C" const char *FE_UsersMailAddress()
{
#ifdef MOZ_MAIL_NEWS
    const char * addr = g_MsgPrefs.m_csUsersEmailAddr;

    if(addr && *addr)
        return(addr);
#else
   static char * prefStr = NULL;
   XP_FREEIF(prefStr);   
	PREF_CopyCharPref("mail.identity.useremail",&prefStr);
   return(prefStr);
#endif // MOZ_MAIL_NEWS        
   return("");
}


//
// Send back the fullname of the current user
//
extern "C" const char *FE_UsersFullName()
{   
#ifdef MOZ_MAIL_NEWS
    const char * name = g_MsgPrefs.m_csUsersFullName;

    if(name && *name)
        return(name);
    else
        return(NULL);
#else        
   static char * prefStr = NULL;
   XP_FREEIF(prefStr);   
	PREF_CopyCharPref("mail.identity.username", &prefStr);
   return(prefStr);
#endif
}

//
// Return the uesr's organization (if specified)
//
extern "C" const char *FE_UsersOrganization (void)
{
#ifdef MOZ_MAIL_NEWS
    const char * org = g_MsgPrefs.m_csUsersOrganization;

    if(org && *org)
        return(org);
    else
        return(NULL);
#else        
   static char * prefStr = NULL;
   XP_FREEIF(prefStr);   
   PREF_CopyCharPref("mail.identity.organization", &prefStr);
   return(prefStr);
#endif // MOZ_MAIL_NEWS        
}

extern "C" const char *FE_UsersSignature(void)  
{
#ifdef MOZ_MAIL_NEWS
    //  Just return the sig file that we read in on startup.
    char * pszPref;
	PREF_CopyCharPref("mail.signature_file", &pszPref);
	if (pszPref) {
		XP_FREEIF(g_MsgPrefs.m_pszUserSig);
		g_MsgPrefs.m_pszUserSig = wfe_ReadUserSig(pszPref);
	}
    XP_FREEIF(pszPref);

    return(g_MsgPrefs.m_pszUserSig);
#else
   return("");
#endif // MOZ_MAIL_NEWS   
       
}

// Read in a user's sig file
// Return NULL on error
// The string should be freed by the caller
MODULE_PRIVATE char *wfe_ReadUserSig(const char * msg)
{
#ifdef MOZ_MAIL_NEWS
    FILE * fp;
    char * buffer;

    if(!msg || !msg[0])
		return(NULL);

#ifdef XP_WIN32
    fp = fopen(msg, "rb");
#else
	// Windows uses ANSI codepage, DOS uses OEM codepage, fopen takes OEM codepage
	// That's why we need to do conversion here.
	CString oembuff = msg;
	oembuff.AnsiToOem();
    fp = fopen(oembuff, "rb");
#endif
    if(!fp)
        return(NULL);

    // go to the end
    fseek(fp, 0L, SEEK_END);  
    int32 len = ftell(fp);

    if(len < 1)
        return(NULL);

    buffer = (char *) XP_ALLOC(len + 5);
    if(!buffer)
        return(NULL);

    fseek(fp, 0L, SEEK_SET);
    len = fread(buffer, 1, CASTINT(len), fp);
    buffer[len] = '\0';

    //  Strip whitespace from the end of the buffer.
    len--;
    while(len > 0 && (isspace(buffer[len])||buffer[len]==26))  {
        buffer[len] = '\0';
        len--;
    }

    //  Add a new line to the end.
    if(len > 0) {
        strcat(buffer, "\r\n");
    }

    fclose(fp);

    return(buffer);
#else // MOZ_MAIL_NEWS
   return("");
#endif // MOZ_MAIL_NEWS
       
}

#ifdef MOZ_MAIL_NEWS
// FE function that the back-end calls when it gets a new
// IMAP highwater mark that needs to be communicated/stored
// for the stand-alone biff

#ifdef _WIN32
extern "C" uint32 FE_SetBiffInfo(MSG_BiffInfoType type, uint32 data)
{
	switch(type)
	{
		case MSG_IMAPHighWaterMark:
		{
			static const char szHighWaterKey[] = "IMAPHighWaterUID";
			SetBiffInfoNum(szHighWaterKey, data);
			break;
		}
		default:
		{
			break;
		}
	}
	return(0);
}
#endif
#endif

#ifdef MOZ_MAIL_NEWS
//
// A biff operation has just completed.
//

extern "C" void FE_UpdateBiff(MSG_BIFF_STATE state)
{
#ifdef _WIN32
	const char szNSNotifyWindowClassName[] = NSNotifyWindowClassName;		// do not localize this string
	const char szNSNotifyRegisterMsgStr[] = NSMailNotifyMsg;		// do not localize this string
	HWND nsNotifyHwnd = FindWindow(szNSNotifyWindowClassName, NULL);
	static UINT WM_NSMailMsg = 0;
	if (WM_NSMailMsg == 0)
	{
		WM_NSMailMsg = RegisterWindowMessage(szNSNotifyRegisterMsgStr);
	}

	NOTIFYICONDATA nid;
	if (sysInfo.m_bWin4) {
		nid.cbSize = sizeof(NOTIFYICONDATA);
		nid.hWnd = theApp.m_pHiddenFrame->GetSafeHwnd();
		nid.uID = ID_TOOLS_INBOX;
		nid.uFlags = 0;
	}

	if (state == MSG_BIFF_NoMail) {
		// tell the stand-alone biff there is no new mail anymore
		// always do this, even if this is the same as the last state
		if (nsNotifyHwnd) 
			PostMessage(nsNotifyHwnd, WM_NSMailMsg, NSMAIL_GetMailCompleted, 0);
	}
#endif

	static MSG_BIFF_STATE stateOld = MSG_BIFF_Unknown;

	if ( stateOld != state ) {
		switch ( state ) {
		case MSG_BIFF_NoMail:
#ifdef _WIN32
			if (sysInfo.m_bWin4) {
				Shell_NotifyIcon( NIM_DELETE, &nid );
			}
#endif
			theApp.GetTaskBarMgr().ReplaceTaskIcon( ID_TOOLS_INBOX, IDB_TASKBARL, IDB_TASKBARS, INBOX_ICON_INDEX);
			break;
		case MSG_BIFF_Unknown:
			theApp.GetTaskBarMgr().ReplaceTaskIcon( ID_TOOLS_INBOX, IDB_TASKBARL, IDB_TASKBARS, UNKNOWN_MAIL_ICON_INDEX);
			break;
		case MSG_BIFF_NewMail:
			XP_Bool prefBool = TRUE;
			PREF_GetBoolPref("mail.play_sound",&prefBool);

#ifdef _WIN32
			if (sysInfo.m_bWin4) {
				if (prefBool) {
					PlaySound( _T("MailBeep"), NULL, SND_ASYNC );
				}

				// Add the biff icon only if we don't find a standalone biff
				if (!nsNotifyHwnd) {
					nid.uFlags = NIF_MESSAGE|NIF_ICON|NIF_TIP;
					nid.uCallbackMessage = MSG_TASK_NOTIFY;
					nid.hIcon = ::LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE( IDI_MAIL ));
					strcpy(nid.szTip, szLoadString(IDS_YOUHAVENEWMAIL));
					Shell_NotifyIcon( NIM_ADD, &nid );
					if (nid.hIcon)
						DestroyIcon(nid.hIcon);
				}
				else {
					if (sysInfo.m_bWin4) {
						Shell_NotifyIcon( NIM_DELETE, &nid );
					}
				}
			}
			else
#endif
			{
				if (prefBool) {
					sndPlaySound( "MailBeep", SND_ASYNC );
				}
			}

			theApp.GetTaskBarMgr().ReplaceTaskIcon( ID_TOOLS_INBOX, IDB_TASKBARL, IDB_TASKBARS, NEW_MAIL_ICON_INDEX);
			break;
		}
		stateOld = state;
	}
}
#endif

//
// Save the POP3 password in the encrypted form it was handed to us
//
extern "C" void FE_RememberPopPassword(MWContext * context, const char * password)
{
#ifdef MOZ_MAIL_NEWS
    // if we aren't supposed to remember clear anything in the registry
	XP_Bool prefBool;
	XP_Bool passwordProtectLocalCache;
	PREF_GetBoolPref("mail.remember_password",&prefBool);
	PREF_GetBoolPref("mail.password_protect_local_cache", &passwordProtectLocalCache);

    if(prefBool || passwordProtectLocalCache)
		PREF_SetCharPref("mail.pop_password",(char *)password);
    else
		PREF_SetCharPref("mail.pop_password","");
#endif /* MOZ_MAIL_NEWS */
}

#ifdef MOZ_MAIL_NEWS
/////////////////////////////////////////////////////////////////////
//
// CNewsDownloadDialog
//

class CNewsDownloadDialog: public CDialog {

protected:
	enum { IDD = IDD_NEWSMAXHEADERS };

	virtual BOOL OnInitDialog( );
	virtual void DoDataExchange(CDataExchange* pDX);

public:
	int32 m_iNumMsg;
	int m_iDownloadSome;
	int32 m_iDownloadMax;
	int m_iMarkRead;

	CNewsDownloadDialog( MWContext *pXPCX, int32 iNumMsg);

protected:
	afx_msg void OnRadioClicked();
	afx_msg void OnEditFocus();
	DECLARE_MESSAGE_MAP();
};

void CNewsDownloadDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Radio(pDX, IDC_RADIO1, m_iDownloadSome);
	DDX_Text(pDX, IDC_EDIT1, m_iDownloadMax);
	DDX_Check(pDX, IDC_CHECK1, m_iMarkRead);
}

BOOL CNewsDownloadDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	CWnd *widget = GetDlgItem(IDC_STATIC1);

	CString cs;
	cs.Format(szLoadString(IDS_MSGSTODOWNLOAD),
			  (long int) m_iNumMsg );
	widget->SetWindowText(cs);

	widget = GetDlgItem( IDC_CHECK1 );
	widget->EnableWindow( m_iDownloadSome );
	return TRUE;
}

CNewsDownloadDialog::CNewsDownloadDialog( MWContext *pXPCX, int32 iNumMsg ):
	CDialog( IDD, ABSTRACTCX(pXPCX)->GetDialogOwner() )
{
	m_iNumMsg = iNumMsg;

	m_iDownloadMax = 0;
	PREF_GetIntPref("news.max_articles", &m_iDownloadMax);

	m_iDownloadSome = 0;

	XP_Bool bMarkRead = FALSE;
	PREF_GetBoolPref( "news.mark_old_read", &bMarkRead );
	m_iMarkRead = bMarkRead ? 1 : 0;
}

BEGIN_MESSAGE_MAP(CNewsDownloadDialog, CDialog)
	ON_BN_CLICKED(IDC_RADIO1, OnRadioClicked)
	ON_BN_CLICKED(IDC_RADIO2, OnRadioClicked)
	ON_EN_SETFOCUS(IDC_EDIT1, OnEditFocus)
END_MESSAGE_MAP()

void CNewsDownloadDialog::OnRadioClicked()
{
	UpdateData(TRUE);

	CWnd *widget = GetDlgItem( IDC_CHECK1 );
	widget->EnableWindow( m_iDownloadSome );
}

void CNewsDownloadDialog::OnEditFocus()
{
	CheckRadioButton(IDC_RADIO1, IDC_RADIO2, IDC_RADIO2);
	OnRadioClicked();
}

extern "C" XP_Bool FE_NewsDownloadPrompt( MWContext *context,
										  int32 numMessagesToDownload, 
										  XP_Bool *downloadAll)
{
	CNewsDownloadDialog dlg(context, numMessagesToDownload);

	if (dlg.DoModal() == IDOK ) {
		if (dlg.m_iDownloadSome) {
			*downloadAll = FALSE;
			PREF_SetIntPref("news.max_articles", dlg.m_iDownloadMax);
			PREF_SetBoolPref("news.mark_old_read", dlg.m_iMarkRead ? TRUE : FALSE);
		} else {
			*downloadAll = TRUE;
		}
		return TRUE;
	} else {
		return FALSE;
	}
}

extern "C" void FE_ListChangeStarting(MSG_Pane* pane, XP_Bool asynchronous,
									  MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									  int32 num)
{
	LPUNKNOWN pUnk = (LPUNKNOWN) MSG_GetFEData( pane );
	if (pUnk) {
		LPMSGLIST pInterface = NULL;
		pUnk->QueryInterface( IID_IMsgList, (LPVOID *) &pInterface );
		if ( pInterface ) {
			pInterface->ListChangeStarting( pane, asynchronous, notify, where, num );
			pInterface->Release();
		}
	}
}

extern "C" void FE_ListChangeFinished(MSG_Pane* pane, XP_Bool asynchronous,
									  MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									  int32 num)
{
	LPUNKNOWN pUnk = (LPUNKNOWN) MSG_GetFEData( pane );
	if (pUnk) {
		LPMSGLIST pInterface = NULL;
		pUnk->QueryInterface( IID_IMsgList, (LPVOID *) &pInterface );
		if ( pInterface ) {
			pInterface->ListChangeFinished( pane, asynchronous, notify, where, num );
			pInterface->Release();
		}
	}
}

extern "C" void FE_PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
								MSG_PANE_CHANGED_NOTIFY_CODE notify, int32 value)
{
	LPUNKNOWN pUnk = (LPUNKNOWN) MSG_GetFEData( pane );
	if (pUnk) {
		LPMAILFRAME pInterface = NULL;
		pUnk->QueryInterface( IID_IMailFrame, (LPVOID *) &pInterface );
		if ( pInterface ) {
			pInterface->PaneChanged( pane, asynchronous, notify, value );
			pInterface->Release();
		}
	}
}

extern "C" XP_Bool FE_IsAltMailUsed(MWContext *pContext)
{
	return theApp.m_hPostalLib ? TRUE : FALSE;
}

#endif // MOZ_MAIL_NEWS


#ifdef _WIN32

// these are the registry functions that are used to communicate and store 
// stuff in the registry for the stand-alone biff
static const char szMasterKey[] = "Software\\Netscape\\Netscape Navigator\\%s";
static const char szMasterKeyBiff[] = "Software\\Netscape\\Netscape Navigator\\biff";
static const char szUserKey[] = "Software\\Netscape\\Netscape Navigator\\Users";
static const char szCurrentUser[] = "CurrentUser";

BOOL GetConfigInfoStr(LPCSTR pSection, LPCSTR pKey, LPSTR pBuf, int lenBuf, HKEY hMasterKey);
HKEY RegCreateParent(LPCSTR pSection, HKEY hMasterKey);
BOOL SetConfigInfoNum(LPCSTR pSection, LPCSTR pKey, uint32 num, HKEY hMasterKey);

HKEY RegOpenParent(LPCSTR pSection, HKEY hRootKey)
{
	char buf[128];
	HKEY hKey;
	if (strchr(pSection, '\\'))
	{
		lstrcpy(buf, pSection);
	}
	else
	{
		wsprintf(buf, szMasterKey, pSection);
	}
	if (RegOpenKeyEx(hRootKey, buf, 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
	{
		return(NULL);
	}
	return(hKey);
}

BOOL SetBiffInfoNum(LPCSTR pKey, uint32 num)
{
	char userName[512];
	char temp[512];
	if (GetConfigInfoStr(szUserKey, szCurrentUser, userName, sizeof(userName), HKEY_LOCAL_MACHINE))
	{
		wsprintf(temp, "%s\\%s\\biff", szUserKey, userName);
		return(SetConfigInfoNum(temp, pKey, num, HKEY_LOCAL_MACHINE));
	}
	// if the user isn't there, then store it in the global biff location
	return(SetConfigInfoNum(szMasterKeyBiff, pKey, num, HKEY_CURRENT_USER));
}

BOOL GetConfigInfoStr(LPCSTR pSection, LPCSTR pKey, LPSTR pBuf, int lenBuf, HKEY hMasterKey)
{
	HKEY hKey;
	hKey = RegOpenParent(pSection, hMasterKey);
	if (!hKey)
	{
		return(FALSE);
	}

	uint32 len = (uint32)lenBuf;
	BOOL retVal = (RegQueryValueEx(hKey, pKey, NULL, NULL, (LPBYTE)pBuf, &len) == ERROR_SUCCESS);
	RegCloseKey(hKey);
	return(retVal);
}

HKEY RegCreateParent(LPCSTR pSection, HKEY hMasterKey)
{
	char buf[128];
	HKEY hKey;
	if (strchr(pSection, '\\'))
	{
		lstrcpy(buf, pSection);
	}
	else
	{
		wsprintf(buf, szMasterKey, pSection);
	}
	if (RegCreateKey(hMasterKey, buf, &hKey) != ERROR_SUCCESS)
	{
		return(NULL);
	}
	return(hKey);
}

BOOL SetConfigInfoNum(LPCSTR pSection, LPCSTR pKey, uint32 num, HKEY hMasterKey)
{
	HKEY hKey;
	hKey = RegCreateParent(pSection, hMasterKey);
	if (!hKey)
	{
		return(FALSE);
	}

	BOOL retVal = (RegSetValueEx(hKey, pKey, 0, REG_DWORD, (LPBYTE)&num, sizeof(num)) == ERROR_SUCCESS);
	RegCloseKey(hKey);
	return(retVal);
}

#endif		// _WIN32

