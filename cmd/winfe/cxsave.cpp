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

// cxsave.cpp : implementation file
//

#include "stdafx.h"
#include "msgcom.h"
#include "cxsave.h"
#include "extgen.h"
#include "intl_csi.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

extern "C" int MK_DISK_FULL;		// defined in allxpstr.c
extern char *FE_FindFileExt(char * path);
extern void FE_LongNameToDosName(char* dest, char* source);
extern void CheckLegalFileName(char* full_path);

/////////////////////////////////////////////////////////////////////////////
// CSaveCX dialog

BOOL CSaveCX::SaveAnchorObject(const char *pAnchor, History_entry *pHist, int16 iCSID, CWnd *pParent, char *pFileName)
{
	//	Indirect constructor for serializing object.

	//	Allocate the dialog.
	CSaveCX *pSaveCX = new CSaveCX(pAnchor, NULL, pParent);

	pSaveCX->m_iCSID = iCSID;

    // see if we've been given a file to load
    if(pFileName)	{
        pSaveCX->m_csFileName = pFileName;
	}

	//	Assign over the history entry.
	pSaveCX->m_pHist = pHist;

	//	See if we can create the dialog.
	BOOL bCreate = pSaveCX->CanCreate();

	if(bCreate == TRUE)	{
		//	Create the dialog.
		pSaveCX->DoCreate();
		return(TRUE);
	}
	else	{
		//	No need to destroy a window, none created.
		return(FALSE);
	}
}

// Kludge to avoid including cxsave.h in edview.cpp (freaks out Win16 compiler)
BOOL wfe_SaveAnchorAsText(const char *pAnchor, History_entry *pHist,  CWnd *pParent, char *pFileName)
{
    return CSaveCX::SaveAnchorAsText(pAnchor, pHist, pParent, pFileName);
}

// Similar to above, but special version for Composer
//  to allow converting current HTML to text output
BOOL CSaveCX::SaveAnchorAsText(const char *pAnchor, History_entry *pHist,  CWnd *pParent, char *pFileName)
{
	//	Allocate the dialog.
	CSaveCX *pSaveCX = new CSaveCX(pAnchor, NULL, pParent);
    if( !pSaveCX  || !pFileName )
        return FALSE;

    // see if we've been given a file to load
    pSaveCX->m_csFileName = pFileName;

    // We already know to save as Text, so set this here
    // (CanCreate will not prompt for filename - we already selected it)
    pSaveCX->m_iFileType = TXT;

	//	Assign over the history entry.
	pSaveCX->m_pHist = pHist;

	//	See if we can create the dialog.
	BOOL bCreate = pSaveCX->CanCreate();

	if(bCreate == TRUE)	{
		//	Create the dialog.
		pSaveCX->DoCreate();
		return(TRUE);
	}
	else	{
		//	No need to destroy a window, none created.
		return(FALSE);
	}
}

NET_StreamClass *CSaveCX::SaveUrlObject(URL_Struct *pUrl, CWnd *pParent, char * pFileName)	
{
	//	Indirect constructor for serializing object.

	//	See if it's safe to convert the URL to this context.
	if(NET_IsSafeForNewContext(pUrl) == FALSE)	{
		return(NULL);
	}
  

	//	Allocate the dialog.
	CSaveCX *pSaveCX = new CSaveCX(pUrl->address, NULL, pParent);

    // see if we've been given a file to load
    if(pFileName)	{
        pSaveCX->m_csFileName = pFileName;
	}

	//	See if it's OK to further create the dialog.
	pSaveCX->SetUrl(pUrl);
	if(pSaveCX->CanCreate() == FALSE) {
		return(NULL);
	}

	//	Transfer the URL to the new context.
	//	This ties together the dialog and the URL.
	if(0 != NET_SetNewContext(pUrl, pSaveCX->GetContext(), CFE_GetUrlExitRoutine))	{
		//	Couldn't transfer.
		ASSERT(0);
		pSaveCX->DestroyContext();
		return(NULL);
	}

	//	Manually bind the stream that will handle the download of
	//		the URL, and return that stream.
	//	This ties together the stream and dialog.
	NET_StreamClass *pRetval = ContextSaveStream(FO_SAVE_AS, NULL, pUrl, pSaveCX->GetContext());
	if(pRetval == NULL)	{
		//	Couldn't create the stream.
		pSaveCX->DestroyContext();
		return(NULL);
	}

	//	Create the dialog, the viewable portions.
	pSaveCX->DoCreate();

	return(pRetval);
}

NET_StreamClass *CSaveCX::ViewUrlObject(URL_Struct *pUrl, const char *pViewer, CWnd *pParent)	{
	//	Indirect constructor for externally viewing object.

	//	See if it's safe to convert the URL to this context.
	if(NET_IsSafeForNewContext(pUrl) == FALSE)	{
		return(NULL);
	}

	//	Allocate the dialog.
    CSaveCX *pSaveCX = NULL;
    if(pViewer != NULL && strlen(pViewer))  {
	    pSaveCX = new CSaveCX(pUrl->address, pViewer, pParent);
    }
    else    {
        //  Shell execute style.
        pSaveCX = new CSaveCX(pUrl->address, "ShellExecute", pParent);
    }

	//	See if it's OK to further create the dialog.
	if(pSaveCX->CanCreate(pUrl) == FALSE)	{
		return(NULL);
	}

	//	Transfer the URL to the new context.
	//	This ties together the dialog and the URL.
	if(0 != NET_SetNewContext(pUrl, pSaveCX->GetContext(), CFE_GetUrlExitRoutine))	{
		//	Couldn't transfer.
		ASSERT(0);
		pSaveCX->DestroyContext();
		return(NULL);
	}
	pSaveCX->SetUrl(pUrl);

	//	Manually bind the stream that will handle the download of
	//		the URL, and return that stream.
	//	This ties together the stream and dialog.
	NET_StreamClass *pRetval = ContextSaveStream(FO_SAVE_AS, NULL, pUrl, pSaveCX->GetContext());
	if(pRetval == NULL)	{
		//	Couldn't create the stream.
		pSaveCX->DestroyContext();
		return(NULL);
	}

	//	Create the dialog, the viewable portions.
	pSaveCX->DoCreate();

	return(pRetval);
}

NET_StreamClass *CSaveCX::OleStreamObject(NET_StreamClass *pOleStream, URL_Struct *pUrl, const char *pViewer, CWnd *pParent)	{
	//	Indirect constructor for externally viewing object.

	//	See if it's safe to convert the URL to this context.
	if(NET_IsSafeForNewContext(pUrl) == FALSE)	{
		return(NULL);
	}

	//	Allocate the dialog.
	CSaveCX *pSaveCX = new CSaveCX(pUrl->address, pViewer, pParent);

	//	See if it's OK to further create the dialog.
	if(pSaveCX->CanCreate() == FALSE)	{
		return(NULL);
	}

	//	Transfer the URL to the new context.
	//	This ties together the dialog and the URL.
	if(0 != NET_SetNewContext(pUrl, pSaveCX->GetContext(), CFE_GetUrlExitRoutine))	{
		//	Couldn't transfer.
		ASSERT(0);
		return(NULL);
	}
	pSaveCX->SetUrl(pUrl);

	//	Speically set the secondary stream.
	pSaveCX->SetSecondaryStream(pOleStream);

	//	Manually bind the stream that will handle the download of
	//		the URL, and return that stream.
	//	This ties together the stream and dialog.
	NET_StreamClass *pRetval = ContextSaveStream(FO_SAVE_AS, NULL, pUrl, pSaveCX->GetContext());
	if(pRetval == NULL)	{
		//	Couldn't create the stream.
		return(NULL);
	}

	//	Create the dialog, the viewable portions.
	pSaveCX->DoCreate();

	return(pRetval);
}

CSaveCX::CSaveCX(const char *pAnchor, const char *pViewer, CWnd *pParent)
	: CDialog(CSaveCX::IDD, pParent)
{
    iLastPercent = 0;
    tLastBarTime = tLastTime = 0;
	m_iFileType = ALL;

    m_pSink = NULL;

	//	We're not loading a URL yet.
	m_pUrl = NULL;

	//	There's no history entry.
	m_pHist = NULL;

	//	There's no secondary stream yet.
	m_pSecondaryStream = NULL;

    //  We haven't been interrupted.
    m_bInterrupted = FALSE;

    //  We haven't been abortioned.
    m_bAborted = FALSE;

	//  We aren't saving to memory yet
	m_bSavingToGlobal = FALSE;

    //  Set the context type.
    m_cxType = Save;
	GetContext()->type = MWContextSaveToDisk;

	//	No progress information as of yet.
	m_lOldPercent = 0;

	// Do not know the character set yet
	m_iCSID = 0;

    //  Set the anchor, and our viewer if possible.
    //  We won't resolve any issues until later, when we can safely
    //      fall out if the user chooses cancel in a file dialog or something.
    if(pAnchor != NULL) {
        m_csAnchor = pAnchor;
    }
    if(pViewer != NULL) {
        m_csViewer = pViewer;
    }

    //  Set our parent window.
    m_pParent = pParent;
#ifdef DEBUG
    //  CWnd must not be a temporary object.
    if(m_pParent) {
        ASSERT(FromHandlePermanent(m_pParent->GetSafeHwnd()));
    }
#endif
	//{{AFX_DATA_INIT(CSaveCX)
	m_csAction = _T("");
	m_csDestination = _T("");
	m_csLocation = _T("");
	m_csProgress = _T("");
	m_csTimeLeft = _T("");
	m_csPercentComplete = _T("");
	//}}AFX_DATA_INIT
}

// m_pUrl, if it exists will have been freed in CFE_GetUrlExitRoutine()
CSaveCX::~CSaveCX() 
{
	//	Clean up any memory that's not handled before this.
	//	We specifically allocated the save_as_name in the XP context.
	//		for file saves only.
	if(GetContext()->save_as_name != NULL)	{
		free(GetContext()->save_as_name);
		GetContext()->save_as_name = NULL;	//	In case someone else wants to free it, clear it.
	}

    //  Clear back pointer.
    m_pParent = NULL;
}


void CSaveCX::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	//{{AFX_DATA_MAP(CSaveCX)
	DDX_Text(pDX, IDC_ACTION, m_csAction);
	DDX_Text(pDX, IDC_DESTINATION, m_csDestination);
	DDX_Text(pDX, IDC_LOCATION, m_csLocation);
	DDX_Text(pDX, IDC_PROGRESS, m_csProgress);
	DDX_Text(pDX, IDC_TIMELEFT, m_csTimeLeft);
	DDX_Text(pDX, IDC_PERCENTCOMPLETE, m_csPercentComplete);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CSaveCX, CDialog)
	//{{AFX_MSG_MAP(CSaveCX)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_SIZE()
	ON_WM_SYSCOMMAND()
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CSaveCX message handlers

void CSaveCX::OnCancel() 
{
	// TODO: Add extra cleanup here

    //  Mark that the user hit the cancel button, so that we can get
    //      rid of the temp files when everything's kosher.
    //  Do this before the interrupt, or other code may not know this fact.
    m_bInterrupted = TRUE;

    //  Stop the load.
	//	This variable should never be invalid while this message handler can be
	//		called.
	XP_InterruptContext(GetContext());
}

HCURSOR CSaveCX::OnQueryDragIcon()	{
	//	Return the icon that will show up when dragged.
	HICON hIcon = theApp.LoadIcon(IDR_MAINFRAME);
	ASSERT(hIcon);
	return((HCURSOR)hIcon);
}

BOOL CSaveCX::OnEraseBkgnd(CDC *pDC)	{
	if(IsIconic() == TRUE)	{
		return(TRUE);
	}

	return(CDialog::OnEraseBkgnd(pDC));
}

void CSaveCX::OnSysCommand(UINT nID, LPARAM lParam)	{
	//	Don't maximize ourselves
	if(nID == SC_MAXIMIZE)	{
		return;
	}

	CDialog::OnSysCommand(nID, lParam);
}

void CSaveCX::OnSize(UINT nType, int cx, int cy) 
{
	//	Change any maximize request to a normal request.
	if(nType == SIZE_MAXIMIZED)	{
		nType = SIZE_RESTORED;
	}

	CDialog::OnSize(nType, cx, cy);
}

void CSaveCX::OnPaint() 
{
	TRY	{
		CPaintDC dc(this);

		//	Check to see if we need to draw our icon.
		if(IsIconic() != FALSE)	{
			HICON hIcon = theApp.LoadIcon(IDR_MAINFRAME);
			ASSERT(hIcon);
			dc.DrawIcon(2, 2, hIcon);

			//	Also, make sure the window title is correct.
			char aTitle[64];
			//CLM: Removed hard-coded strings - be kind to International folks!
			if(IsSaving() == TRUE)	{
				sprintf(aTitle, szLoadString(IDS_SAVE_SAVINGLOCATION), m_lOldPercent);
			}
			else	{
				sprintf(aTitle, szLoadString(IDS_SAVE_VIEWLOCATION), m_lOldPercent);
			}
			SetWindowText(aTitle);
		}
		else	{
			//	Call the progress routine.
			//	This will cause the painting of the progress bar to be
			//		correct.
			Progress(m_lOldPercent);

			//	Also, make sure the window title is correct.
			if(IsSaving() == TRUE)	{
				SetWindowText(szLoadString(IDS_SAVING_LOCATION));
			}
			else	{
				SetWindowText(szLoadString(IDS_VIEWING_LOCATION));
			}
		}

	// Do not call CDialog::OnPaint() for painting messages
	}
	CATCH(CException, e)	{
		//	Something went wrong.
		return;
	}
	END_CATCH
}

BOOL CSaveCX::Creator()
{
    BOOL bRetval = FALSE;
    CWnd *pParent = m_pParent;
    CWnd desktop;
    
    //  If no parent, use the desktop.
    //  Should not be temporary CWnd returned from CWnd::GetDesktopWindow
    //      as can theoretically be released if a URL completes and calls
    //      the idle code during creation.
    if(!pParent) {
        desktop.Attach(::GetDesktopWindow());
        pParent = &desktop;
    }
    
    //  Do it.
    bRetval = Create(CSaveCX::IDD, pParent);
    
    //  If we used the desktop, be sure to unattach before CWnd goes out
    //      of scope.
    if(pParent == &desktop) {
        pParent->Detach();
        pParent = NULL;
    }
    
    return(bRetval);
}

//	Determine if it's OK to create the dialog for the transfer.
BOOL CSaveCX::CanCreate(URL_Struct* pUrl)	
{
    if(m_csAnchor.IsEmpty()) {
		DestroyContext();
        return(FALSE);
    }

	//	Set some information for the dialog.
	m_csLocation = m_csAnchor;
	WFE_CondenseURL(m_csLocation, 40, FALSE);
  
    //  Are we asking them for a file name or stream? (not externally viewing)
    if(IsSaving() == TRUE)    {

		// Don't ask for filename stuff if saving to stream.
		if (!IsSavingToGlobal()) {
			// Query for a filename if we don't have one already
			if(m_csFileName.IsEmpty()) {

#ifdef MOZ_MAIL_NEWS
				char * pSuggested = MimeGuessURLContentName(GetContext(),m_csAnchor);
				if (!pSuggested)
#else
        char *            
#endif /* MOZ_MAIL_NEWS */            
					pSuggested = fe_URLtoLocalName(m_csAnchor, pUrl ? pUrl->content_type : NULL);

				char *pUserName = wfe_GetSaveFileName(NULL, szLoadString(IDS_SAVE_AS), pSuggested, &m_iFileType);

				if(pSuggested)  {
					XP_FREE(pSuggested);
				}

				if(pUserName == NULL) {
					DestroyContext();
					return(FALSE);
				}

				m_csFileName = pUserName;
				XP_FREE(pUserName);
			}

			//CLM: CHECK FOR SOURCE == DESTINATION
			char * pSource = NULL;
			XP_ConvertUrlToLocalFile(m_csAnchor, &pSource);
			if( pSource && !m_csFileName.CompareNoCase(pSource) ){
    			::MessageBox(0, szLoadString( IDS_SOURCE_SAMEAS_DEST),
    					   szLoadString(IDS_SAVING_LOCATION), MB_OK | MB_ICONEXCLAMATION );
    			DestroyContext();
    			XP_FREE(pSource);
    			return(FALSE);
			}
			if( pSource) XP_FREE(pSource);

			//	We need to copy the file name into the XP context's save_as_name,
			//		so that other older code that depends on this will work (DDE).
			if(GetContext()->save_as_name != NULL)	{
				ASSERT(FALSE);	//	Why isn't this NULL???
				free(GetContext()->save_as_name);
				GetContext()->save_as_name = NULL;
			}
			GetContext()->save_as_name = strdup((const char *)m_csFileName);
		}

		//	Set some information in the dialog.
		m_csAction.LoadString(IDS_SAVE_SAVING); //  = "Saving:";
		m_csDestination = m_csFileName;
		WFE_CondenseURL(m_csDestination, 40, FALSE);

		//	Create the viewable portions of the dialog.
		Creator();

		//	Set its title.
		SetWindowText(szLoadString(IDS_SAVING_LOCATION));

		//	Only ask for a URL if we don't already have one assigned
		//		to us.
		if(m_pUrl == NULL)	{
		    //  we've got a file name to save into.
		    //  the type of the file is set.
		    //  it would appear that everything is ready, ask for the URL.
			//	We can only do this when saving.  Other methods must
			//		manually set the URL.
			if(m_pHist == NULL)	{
			    m_pUrl = NET_CreateURLStruct(m_csAnchor, NET_DONT_RELOAD);

			} else {
				//	We are going to use the history entry of the other context
				//		instead.  In this manner, we can properly get the form
				//		data set for the load.
				m_pUrl = SHIST_CreateURLStructFromHistoryEntry(GetContext(), m_pHist);

				// We need to make a copy of the form data, because we are in a
				// different context
				if (m_pUrl) {
					SHIST_SavedData	savedData;

					memcpy(&savedData, &m_pUrl->savedData, sizeof(SHIST_SavedData));
					memset(&m_pUrl->savedData, 0, sizeof(SHIST_SavedData));
					LO_CloneFormData(&savedData, GetDocumentContext(), m_pUrl);
				}
			}

		    switch(m_iFileType) {
				case TXT:
					//  We need to ask the text front end to handle.
					TranslateText(m_pUrl, m_csFileName);
					break;

				default:
					//  We handle ourselves.
					//  Will send through a particular format out stream.
					//	WE EXPECT NETLIB TO CALL OUR EXIT ROUTINE AND ALLCONNECTIONS COMPLETE
					//		IN ALL CASES.
					if(-1 == GetUrl(m_pUrl, FO_CACHE_AND_SAVE_AS))	{
						return(FALSE);
					}
					break;
		    }
		}
    }
    else    {
        //  We don't really care about the file type.
        m_iFileType = ALL;
        
        //  Formulate a file name that will sink data from the net.
        //  Security rist to let path information in externally provided
        //      filename (content disposition, filename =)
        char *pFormulateName;
        if (pUrl && pUrl->content_name != NULL &&
            strstr(pUrl->content_name, "../") == NULL &&
            strstr(pUrl->content_name, "..\\") == NULL) {
            pFormulateName = XP_STRDUP(pUrl->content_name);
        }
        else {
            pFormulateName = fe_URLtoLocalName(m_csAnchor, pUrl ? pUrl->content_type : NULL);
        }
            
        char *pLocalName = NULL;
        if(((CNetscapeApp *)AfxGetApp())->m_pTempDir != NULL && pFormulateName != NULL) {
            StrAllocCopy(pLocalName, ((CNetscapeApp *)AfxGetApp())->m_pTempDir);
            StrAllocCat(pLocalName, "\\");
			CheckLegalFileName(pFormulateName);
#ifdef XP_WIN16
			char dosName[13]; //8.3 with '\0' total 13 chars
			FE_LongNameToDosName(dosName, pFormulateName);
            StrAllocCat(pLocalName, dosName);
#else 
            StrAllocCat(pLocalName, pFormulateName);
#endif

            //  If this file exists, then we must attempt another temp file.
            if(-1 != _access(pLocalName, 0))    {
				int type = NET_URL_Type(pUrl->address);
				// bug 63751 for Mail/News attachment
				if ((type == MAILBOX_TYPE_URL) || (type == NEWS_TYPE_URL) || (type == IMAP_TYPE_URL))  
	#ifdef XP_WIN16
					pLocalName = GetMailNewsTempFileName(pLocalName, dosName);
	#else 
					pLocalName = GetMailNewsTempFileName(pLocalName);
	#endif
				else	{
					//  Retain the extension.
					char aExt[_MAX_EXT];
					DWORD dwFlags = 0;
					size_t stExt = 0;
                
					aExt[0] = '\0';
	#ifdef XP_WIN16
					dwFlags |= EXT_DOT_THREE;
	#endif
					stExt = EXT_Invent(aExt, sizeof(aExt), dwFlags, pLocalName, pUrl ? pUrl->content_type : NULL);
                
					if(pLocalName) {
						XP_FREE(pLocalName);
						pLocalName = NULL;
					}
					pLocalName = WH_TempFileName(xpTemporary, "V", aExt);
				}
            }
        }
        else    {
            //  Retain the extension.
            char aExt[_MAX_EXT];
            DWORD dwFlags = 0;
            size_t stExt = 0;
            
            aExt[0] = '\0';
#ifdef XP_WIN16
            dwFlags |= EXT_DOT_THREE;
#endif
            stExt = EXT_Invent(aExt, sizeof(aExt), dwFlags, m_csAnchor, pUrl ? pUrl->content_type : NULL);
            
            if(pLocalName) {
                XP_FREE(pLocalName);
                pLocalName = NULL;
            }
            pLocalName = WH_TempFileName(xpTemporary, "V", aExt);
        }

        if(pFormulateName != NULL)  {
            XP_FREE(pFormulateName);
            pFormulateName = NULL;
        }

        m_csFileName = pLocalName;

		//	Set some information for the dialog.
		m_csAction.LoadString(IDS_SAVE_VIEWER); // "Viewer:"
		m_csDestination = GetViewer();
		WFE_CondenseURL(m_csDestination, 40, FALSE);

		//	Create the viewable portions of the dialog.
		Creator();

		//	Set its title.
		SetWindowText(szLoadString(IDS_VIEWING_LOCATION));

		//	We must wait for the URL to be assigned in manually.
		//	DO NOT CREATE ONE.
    }

    tFirstTime = theApp.GetTime();

    // made it
    return(TRUE);

}

// for bug 63751
// pFileName is intended for Win16 to check the prefix length of the 
// filename since pTempPath is a full path.  Make sure you pass in a valid pointer
//  If the prefix is shorter,
// we append the number number after the prefix, otherwise we replace the 
// last char for the number
char* CSaveCX::GetMailNewsTempFileName(char* pTempPath, char *pFileName)	
{
	int nUniqueNo = 0;
	char tempName[MAX_PATH + 3];
	char extension[MAX_PATH];
	char* pExt = NULL;

	strcpy(tempName, pTempPath);

#ifdef XP_WIN16
	if (pFileName) {
		pExt = FE_FindFileExt(pFileName);
		*pExt = '\0';
	}
#endif

	pExt = FE_FindFileExt(tempName);
	strcpy(extension, pExt);
	*pExt = '\0';

    //  Get a name, If this file exists, then we attempt another temp file.
    do {
		char number[3];
		int numLen;

		nUniqueNo += 1;
		numLen = sprintf(number, "%d", nUniqueNo);

#ifdef XP_WIN16
		*pExt = '\0';
		if (strlen(pFileName) < (8 - numLen)) {
			strcat(pExt, number);	//append if we can
			*(pExt + numLen) = '\0';
		}
		else  {
			strncpy((pExt - numLen), number, numLen);
			*pExt = '\0';
		}
#else
		strncpy(pExt, number, numLen);
		*(pExt + numLen) = '\0';
#endif
        strcat(tempName, extension);

    } while (-1 != _access(tempName, 0)); 
	
    if(pTempPath != NULL)  {
        XP_FREE(pTempPath);
        pTempPath = NULL;
    }
    StrAllocCopy(pTempPath, tempName);

	return pTempPath;
}

void CSaveCX::Progress(long lPercentage)	{
	//	return if there's nothing to do.
	if(lPercentage == 0)	{
		return;
	}

    // update the progress meter every second (at least)
    int i;
    for ( i = iLastPercent; i < lPercentage; i++ )
        m_ProgressMeter.StepIt ( );
    if ( lPercentage > iLastPercent )
    {
        time_t t;
        t = theApp.GetTime();
        if ( t - tLastBarTime )
        {
            tLastBarTime = t;
            // print out the percent complete as well
            char szPercent[8];
            wsprintf(szPercent,"%d%%", (int) LOWORD(lPercentage));
  	        SetDlgItemText(IDC_PERCENTCOMPLETE, szPercent );
        }
    }
    iLastPercent = LOWORD(lPercentage);
}

void CSaveCX::SetProgressBarPercent(MWContext *pContext, int32 lPercentage)	{
	//	Make sure there's something to do here.
	if(m_lOldPercent == lPercentage || lPercentage == 0)	{
		return;
	}

	m_lOldPercent = lPercentage;
	
	Progress(lPercentage);

	if(IsIconic() == TRUE)	{
		char aTitle[64];
		if(IsSaving() == TRUE)	{
			sprintf(aTitle, szLoadString(IDS_SAVE_SAVINGLOCATION), lPercentage);
		}
		else	{
			sprintf(aTitle, szLoadString(IDS_SAVE_VIEWLOCATION), lPercentage);
		}
		SetWindowText(aTitle);
	}
}

CWnd *CSaveCX::GetDialogOwner() const	{
	//	Return this dialog as the owner of any other dialogs that
	//		may be created in base classes.
	//	No need to call the base.
	return((CWnd *)this);
}

void CSaveCX::GetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext)	{
	//	Call the base.
	CStubsCX::GetUrlExitRoutine(pUrl, iStatus, pContext);

	//	If the URL is changine context, then handle correctly by not freeing.
	if(iStatus == MK_CHANGING_CONTEXT)	{
		m_pUrl = NULL;
	}

	//	Just destroy the window, object gets destroyed in all connections complete.
	//	Fun's over.
	DestroyWindow();
}

void CSaveCX::TextTranslationExitRoutine(PrintSetup *pTextFE)	{
	//	Call the base.
	CStubsCX::TextTranslationExitRoutine(pTextFE);

	//	Destroy this object, object doesn't get destroyed in all connections complete.
	//	Fun's over.
	DestroyWindow();
	DestroyContext();
}

void CSaveCX::AllConnectionsComplete(MWContext *pContext)	{
	//	Go ahead and delete this context.
	//	Won't be doing anything else.
	DestroyContext();
}

void CSaveCX::Progress(MWContext *pContext, const char *pMessage)	{
	//	Set our progress message.
    if(pMessage && !strchr(pMessage,'%') && ::IsWindow(m_hWnd)) {
    	SetDlgItemText(IDC_PROGRESS, pMessage);
    }
}

void CSaveCX::GraphProgress(MWContext *pContext, URL_Struct *pURL, int32 lBytesReceived, int32 lBytesSinceLastTime, int32 lContentLength)	{
	//	call the base.
	CStubsCX::GraphProgress(pContext, pURL, lBytesReceived, lBytesSinceLastTime, lContentLength);

    // update the progress message after at least 1 second has passed
    char szMessage[50];
    unsigned long lKReceived = lBytesReceived / 1024;
    time_t t, tdiff;
    t = theApp.GetTime();
    if ( t - tLastTime )
    {
        tLastTime = t;
        tdiff = t - tFirstTime;
        if ( !tdiff ) 
            tdiff = 1;
        unsigned long bytes_sec = lBytesReceived / tdiff;
        if ( !bytes_sec )
            bytes_sec = 1;
        if ( lContentLength == 0 )
            wsprintf ( szMessage, "%ldK of Unknown (at %ld.%ldK/sec)",
                (long)lKReceived, 
                (long)(bytes_sec / 1000L), 
                (long)( ( bytes_sec % 1000L ) / 100 ) );
        else
            wsprintf ( szMessage, "%ldK of %ldK (at %ld.%ldK/sec)",
                (long)lKReceived, 
                (long)lContentLength / 1024L, 
                (long)(bytes_sec / 1000L), 
                (long)( ( bytes_sec % 1000L ) / 100 ) );
        CFE_Progress(pContext, szMessage );

        if ( lContentLength == 0 )
        {
            // do we know the content length?
            if ( !lKReceived )
                SetDlgItemText ( IDC_TIMELEFT, szLoadString(IDS_UNKNOWN_TIMELEFT));
        }
        else
        {
            char szTime[10];
            time_t tleft = ( lContentLength - lBytesReceived ) / bytes_sec;
            wsprintf ( szTime, "%02ld:%02ld:%02ld", 
                (long)(tleft / 3600L),
                (long)(( tleft % 3600L ) / 60L),
                (long)(( tleft % 3600L ) % 60L ));
            SetDlgItemText ( IDC_TIMELEFT, szTime );
        }
    }

	//	Draw our progress bar from this information.
	CFE_SetProgressBarPercent( pContext, lContentLength != 0 ? lBytesReceived * 100 / lContentLength : 0 );
}

extern "C"  {

NET_StreamClass *ContextSaveStream(int iFormatOut, void *pDataObj, URL_Struct *pUrl, MWContext *pContext)   {
    ASSERT(iFormatOut & FO_SAVE_AS);

	/* settings to enable FTP and HTTP restart of interrupted downloads */
	if(pUrl->server_can_do_byteranges || pUrl->server_can_do_restart)
		pUrl->must_cache = TRUE;

	//	If we're saving from a context not meant to save, then intro the nasty hack.
	if(ABSTRACTCX(pContext)->GetContextType() != Save)	{
		NET_StreamClass *pStreamHack = CSaveCX::SaveUrlObject(pUrl, NULL, pContext->save_as_name);
		//	steal the save as name (avoid always saving to the same file if this happens many times).
		if(pContext->save_as_name)	{
			free(pContext->save_as_name);
			pContext->save_as_name = NULL;
		}
		return(pStreamHack);
	}

    //  We're to save a stream to disk or a secondary stream.
    //  First thing to do is find our CSaveCX object.
    CSaveCX *pSaveCX = VOID2CX(pContext->fe.cx, CSaveCX);

    //  Create the stream which will do the actual work.
    NET_StreamClass *pStream = NET_NewStream("Context save THIS buddy",
        ContextSaveWrite,
        ContextSaveComplete,
        ContextSaveAbort,
        ContextSaveReady,
        CX2VOID(pSaveCX, CSaveCX),
        pContext);

    if(pStream == NULL) {
        ASSERT(0);
        return(NULL);
    }

	//	Don't create an output file if all we're doing is proxying to
	//		another stream.
	if(pSaveCX->GetSecondaryStream() == NULL)	{
	    //  All we need to do now is save the file, the name should already be
	    //      established inside the context class.
	    ASSERT(pSaveCX->GetFileName().IsEmpty() == FALSE);

	    //  See if this is a text file.
        //  Leave as shared readable for DDE apps looking into the file early.
        int iOpenFlags = CStdioFile::modeCreate | CStdioFile::modeWrite;
 #ifdef _WIN32
 		iOpenFlags |= CStdioFile::shareDenyWrite;
 #endif

	    ASSERT(pUrl->content_type != NULL);
	    CString csContentType = pUrl->content_type;
	    csContentType = csContentType.Left(5);
	    if(csContentType == "text/")    {
	        iOpenFlags |= CStdioFile::typeText;
	    }
	    else    {
	        iOpenFlags |= CStdioFile::typeBinary;
	    }

		//	See if the URL has a content length, and if so, see if the device
		//		we'll be writing to has enough free space.
		if(FEU_ConfirmFreeDiskSpace(pSaveCX->GetContext(), pSaveCX->GetFileName(), pUrl->content_length) == FALSE)	{
			//	Not enough space to continue.
			XP_FREE(pStream);
			return(NULL);
		}

	    //  Open the file
	    CStdioFile *pSink;
	    TRY	{
	    	pSink = new CStdioFile(pSaveCX->GetFileName(), iOpenFlags);
		}
		CATCH(CException, e)	{
			CString	strMsg;

			strMsg.LoadString(IDS_NO_OPEN_WRITE);
			FE_Alert(pSaveCX->GetContext(), (LPCSTR)strMsg);
			pSink = NULL;
		}
		END_CATCH

	    if(pSink == NULL)   {
	        ASSERT(0);
	        XP_FREE(pStream);
	        return(NULL);
	    }

	    //  Let the context know where the output is going.
	    pSaveCX->SetSink(pSink);
	}

    //  We're done.
    return(pStream);
}

unsigned int ContextSaveReady(NET_StreamClass *stream)	{
	void *pDataObj=stream->data_object;
	//	Get our save context out of the data object.
	CSaveCX *pSaveCX = VOID2CX(pDataObj, CSaveCX);	

	//	See if we need to handle specially for a secondary stream.
	NET_StreamClass *pSecondary = pSaveCX->GetSecondaryStream();
	if(pSecondary == NULL)	{
		//	We should always be ready to write the maximum amount out to disk.
		return(MAX_WRITE_READY);
	}
	else	{
		return(pSecondary->is_write_ready(pSecondary));
	}
}

int ContextSaveWrite(NET_StreamClass *stream, const char *pWriteData, int32 iDataLength)	{
	void *pDataObj=stream->data_object;
	//	Get our save context out of the data object.
	CSaveCX *pSaveCX = VOID2CX(pDataObj, CSaveCX);
	CStdioFile *pSink = pSaveCX->GetSink();
	int iRetval = (UINT)iDataLength;	

	//	See if we need to handle specially for a secondary stream.
	NET_StreamClass *pSecondary = pSaveCX->GetSecondaryStream();
	if(pSecondary == NULL)	{	
		//	Write the data to disk.
		TRY	{
			pSink->Write((void *)pWriteData, (UINT)iDataLength);
		}
		CATCH(CException, e)	{
			//	Some type of error occurred.
			pSaveCX->Interrupt();

			CString	strMsg;

			strMsg.LoadString(IDS_CANT_WRITE);
			FE_Alert(pSaveCX->GetContext(), (LPCSTR)strMsg);
			iRetval = MK_DISK_FULL;
		}
		END_CATCH
	}
	else	{
		iRetval = pSecondary->put_block(pSecondary, pWriteData, iDataLength);
	}

	//	Return the amount written, or error.
	return(iRetval);
}

void ContextSaveComplete(NET_StreamClass *stream)	{
	void *pDataObj=stream->data_object;
	//	The save is done.  Close the file.
	CSaveCX *pSaveCX = VOID2CX(pDataObj, CSaveCX);	

	if(!pSaveCX->m_bAborted)
	{
		// get rid of the cache file since we don't need it for
		// ftp restarts anymore since it successfully download
		if(pSaveCX->m_pUrl)
			NET_RemoveURLFromCache(pSaveCX->m_pUrl);
	}

	//	See if we need to handle specially for a secondary stream.
	NET_StreamClass *pSecondary = pSaveCX->GetSecondaryStream();
	if(pSecondary == NULL)	{
		TRY	{
			delete(pSaveCX->GetSink());
		}
		CATCH(CException, e)	{
			//	Disk full or some other error condition for Close();
			//	Don't do anything.
			CString	strMsg;

			strMsg.LoadString(IDS_CANT_CLOSE);
			FE_Alert(pSaveCX->GetContext(), (LPCSTR)strMsg);
		}
		END_CATCH
		pSaveCX->ClearSink();

        //  If we've been interrupted, we're going to want to clean up the partial droppings
        //      on disk.
        if(pSaveCX->m_bInterrupted)  {
            TRY {
                CFile::Remove(pSaveCX->GetFileName());
            }
            CATCH(CException, e)    {
                //  Report it to the person wanting to cancel the operation.
                //  Failure...
				CString	strMsg;

				strMsg.LoadString(IDS_CANT_CLEANUP);
                FE_Alert(pSaveCX->GetContext(), (LPCSTR)strMsg);
            }
            END_CATCH
        }	
		//	If this were going to an external viewer, we'll want to remove the file on exit.
		else if(pSaveCX->IsViewing() == TRUE)	{
			FE_DeleteFileOnExit(pSaveCX->GetFileName(),  pSaveCX->GetAnchor());

			//	We'll also want to start that viewer now.
			//	It should have previously been verified as a viewer OK to spawn prior to switching
			//		to this context.
            FE_Progress(pSaveCX->GetContext(), szLoadString(IDS_SPAWNING_EXTERNAL_VIEWER));
            if(pSaveCX->IsShellExecute())   {
                FEU_Execute(pSaveCX->GetContext(), pSaveCX->GetFileName(), NULL);
            }
            else    {
#ifdef XP_WIN32
			    // Pass an 8.3 filename to the helper app in case it doesn't understand long filenames
			    char	szShortFileName[_MAX_PATH];

			    VERIFY(GetShortPathName(pSaveCX->GetFileName(), szShortFileName, sizeof(szShortFileName)) > 0);
                FEU_Execute(pSaveCX->GetContext(), pSaveCX->GetViewer(), szShortFileName);
#else
                FEU_Execute(pSaveCX->GetContext(), pSaveCX->GetViewer(), pSaveCX->GetFileName());
#endif
            }
		}
	}
	else	{
		pSecondary->complete(pSecondary);
		XP_FREE(pSecondary);
		pSaveCX->ClearSecondary();
	}
}

void ContextSaveAbort(NET_StreamClass *stream, int iStatus)	{
	void *pDataObj=stream->data_object;
	//	The save is done.  Close the file.
	CSaveCX *pSaveCX = VOID2CX(pDataObj, CSaveCX);	

	pSaveCX->m_bAborted = TRUE;

	//	See if we need to handle specially for a secondary stream.
	NET_StreamClass *pSecondary = pSaveCX->GetSecondaryStream();
	if(pSecondary == NULL)	{
		//	The load was aborted.
		//	Handle as a normally compeleted stream.
		ContextSaveComplete(stream);
	}
	else	{
		pSecondary->abort(pSecondary, iStatus);
		XP_FREE(pSecondary);
		pSaveCX->ClearSecondary();
	}
}

};

BOOL CSaveCX::OnInitDialog() 
{
	CDialog::OnInitDialog();

    m_ProgressMeter.SubclassDlgItem( IDC_PROGRESSMETER, this );

	//	Set some information for the dialog.
	m_csLocation = m_csAnchor;
    CStatic * pStatic = (CStatic *)GetDlgItem(IDC_LOCATION);
    int iLocWidth = 40;
    if ( pStatic != NULL )
    {
        CRect rect;
        TEXTMETRIC tm;
        pStatic->GetClientRect( &rect );
        CDC * pdc =  pStatic->GetDC();
        pdc->GetTextMetrics ( &tm );
        iLocWidth = rect.Width() / tm.tmAveCharWidth;
        pStatic->ReleaseDC ( pdc );
    };


	WFE_CondenseURL(m_csLocation, iLocWidth, FALSE);
    SetDlgItemText (IDC_LOCATION, m_csLocation );

	WFE_CondenseURL(m_csDestination, iLocWidth, FALSE);
    SetDlgItemText (IDC_DESTINATION, m_csDestination );

#ifdef PMETER
    m_ProgressCtl.SetRange ( 0, 100 );
    m_ProgressCtl.SetStep ( 1 );
#endif
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

CString CSaveCX::GetViewer() const
{
    CString csRetval = m_csViewer;

    if(IsShellExecute())  {
        char aViewer[_MAX_PATH];
        memset(aViewer, 0, sizeof(aViewer));
        
        BOOL bViewer = FEU_FindExecutable(GetFileName(), aViewer, FALSE);
        if(bViewer)  {
            csRetval = aViewer;
        }
    }

    return(csRetval);
}

/////////////////////////////////////////////////////////////////////
//
// CSaveAttachmentStream
//
// for attachment download for drag-and-drop
//

class CSaveAttachmentStream {
protected:
	HGLOBAL m_hGlobal;
	int32 m_nPosition;
	int32 m_nFileSize;
	int32 m_nBufferSize;

	BOOL *m_pbStatus;
	NET_StreamClass *m_pStream;
	
	int16 m_mail_csid;
	int16 m_win_csid;
	CCCDataObject m_converter;
	XP_Bool m_doConvert;

public:
	CSaveAttachmentStream(MWContext *pContext, HGLOBAL hGlobal, LPCTSTR lpszUrl, LPCTSTR lpszTitle, BOOL *pbStatus);
	~CSaveAttachmentStream();

	NET_StreamClass *GetStream() const { return m_pStream; }

	// Stream Stuff
	int Write(const char *pWriteData, int32 iDataLength);
	void Complete();
	void Abort(int iStatus);

	// Global Stuff
	BOOL Grow(int32 nAdd);
	BOOL SetSize(int32 nSize);
	void SetupINTLConverter();
};

extern "C" {

unsigned int AttachmentSaveReady(NET_StreamClass *stream)
{	
	return MAX_WRITE_READY;
}

int AttachmentSaveWrite(NET_StreamClass *stream, const char *pWriteData, int32 iDataLength)
{
	void *pDataObj=stream->data_object;
	CSaveAttachmentStream *pSaveCX = (CSaveAttachmentStream *) pDataObj;	
	return pSaveCX->Write(pWriteData, iDataLength);
}

void AttachmentSaveComplete(NET_StreamClass *stream)
{
	void *pDataObj=stream->data_object;
	CSaveAttachmentStream *pSaveCX = (CSaveAttachmentStream *) pDataObj;	
	pSaveCX->Complete();
}

void AttachmentSaveAbort(NET_StreamClass *stream, int iStatus)
{
	void *pDataObj=stream->data_object;
	CSaveAttachmentStream *pSaveCX = (CSaveAttachmentStream *) pDataObj;	
	pSaveCX->Abort(iStatus);
}

}; // extern "C"

CSaveAttachmentStream::CSaveAttachmentStream(MWContext *pContext, HGLOBAL hGlobal, LPCTSTR lpszUrl, LPCTSTR lpszTitle, BOOL *pbStatus)
{
	m_hGlobal = hGlobal;
	m_nBufferSize = ::GlobalSize(m_hGlobal);
	m_nPosition = 0;
	m_nFileSize = 0;

	m_pStream = NET_NewStream("SaveAttachment",
		AttachmentSaveWrite,
		AttachmentSaveComplete,
		AttachmentSaveAbort,
		AttachmentSaveReady,
		this,
		pContext);

	m_pbStatus = pbStatus;

	if (m_pbStatus)
		*m_pbStatus = TRUE;

	m_mail_csid = 0;
	m_win_csid = 0;
	m_converter = NULL;
	m_doConvert = FALSE;
}

CSaveAttachmentStream::~CSaveAttachmentStream()
{
	if (m_converter)
	{
		INTL_DestroyCharCodeConverter(m_converter);
		m_converter = NULL;
	}
}

void CSaveAttachmentStream::SetupINTLConverter()
{
	INTL_CharSetInfo csi =
		LO_GetDocumentCharacterSetInfo(m_pStream->window_id);
	char *mime_charset = (csi == NULL) ? NULL : INTL_GetCSIMimeCharset(csi);

	if (!m_converter &&
		mime_charset && 
		*(mime_charset))
	{
		m_mail_csid = INTL_CharSetNameToID(mime_charset);
		m_win_csid = INTL_DocToWinCharSetID(m_mail_csid);
		m_converter = INTL_CreateCharCodeConverter();
		if (m_converter)
			m_doConvert = INTL_GetCharCodeConverter(m_mail_csid, m_win_csid, m_converter);
	}
}

int CSaveAttachmentStream::Write(const char *lpBuf, int32 nCount)
{
	if (nCount == 0)
		return 0;

	SetupINTLConverter();

	char *newStr = NULL;
	if(m_doConvert)
	{
		char *dupStr = (char *) XP_ALLOC(nCount+1);
		if (dupStr)
		{
			XP_MEMCPY(dupStr, lpBuf, nCount);
			*(dupStr + nCount) = 0;
		    newStr = (char *) INTL_CallCharCodeConverter(m_converter,
												(unsigned char *)dupStr,
												nCount);
			if (!newStr)
				newStr = dupStr;
			else if (newStr != dupStr)
				XP_FREE(dupStr);

			if (newStr)
			{
				lpBuf = newStr;
				nCount = INTL_GetCCCLen(m_converter);
			}
		}
	}

	if (m_nPosition + nCount > m_nBufferSize)
		Grow(m_nPosition + nCount);

	ASSERT(m_nPosition + nCount <= m_nBufferSize);

	// Safety net
	if (m_nPosition + nCount > m_nBufferSize)
	{
		XP_FREEIF(newStr);
		return 0;
	}

	BYTE *lpBuffer = (BYTE *)::GlobalLock(m_hGlobal);
	memcpy((BYTE*)lpBuffer + m_nPosition, (BYTE*)lpBuf, nCount);
	::GlobalUnlock(m_hGlobal);

	m_nPosition += nCount;
	if (m_nPosition > m_nFileSize)
		m_nFileSize = m_nPosition;

	XP_FREEIF(newStr);
	return (int) nCount;
}

void CSaveAttachmentStream::Complete()
{
	BOOL res = SetSize(m_nFileSize);

	if (m_pbStatus)
		*m_pbStatus = res;
}

void CSaveAttachmentStream::Abort(int iStatus)
{
	if (m_pbStatus)
		*m_pbStatus = FALSE;
}

BOOL CSaveAttachmentStream::Grow(int32 newLen)
{
	if (newLen > m_nBufferSize)
	{
		// grow the buffer
		int32 newBufferSize = m_nBufferSize;

		// determine new buffer size
		while (newBufferSize < newLen)
			newBufferSize += 4096;

		// allocate new buffer
		HGLOBAL hNew = ::GlobalReAlloc(m_hGlobal, newBufferSize, GMEM_MOVEABLE|GMEM_ZEROINIT);

		if (hNew == NULL)
			return FALSE;

		m_nBufferSize = newBufferSize;
	}
	return TRUE;
}

BOOL CSaveAttachmentStream::SetSize(int32 nSize)
{
	if (nSize < 1)
		return FALSE;
	
	HGLOBAL hNew = ::GlobalReAlloc(m_hGlobal, nSize, GMEM_MOVEABLE);
	
	return (hNew != NULL);
}

BOOL CSaveCX::SaveToGlobal(HGLOBAL *phGlobal, LPCSTR lpszUrl, LPCSTR lpszTitle)
{
	//	Indirect constructor for serializing object.
	BOOL bRes = TRUE;
	*phGlobal = ::GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT, 4096);

	// This would be too sad...
	if (!*phGlobal)
		return FALSE;

	//	Allocate the dialog.
	CSaveCX *pSaveCX = new CSaveCX(lpszUrl, NULL, NULL);

	pSaveCX->m_bSavingToGlobal = TRUE;
	pSaveCX->m_csFileName = lpszTitle;

	CSaveAttachmentStream *pStream = 
		new CSaveAttachmentStream(pSaveCX->GetContext(), *phGlobal, lpszUrl, lpszTitle, &bRes);

	pSaveCX->SetSecondaryStream(pStream->GetStream());

	if (pSaveCX->CanCreate()) {
		pSaveCX->DoCreate();
	} else {
		return FALSE;
	}

	FEU_BlockUntilDestroyed(pSaveCX->GetContextID());

	delete pStream;

	if (!bRes) {
		::GlobalFree(*phGlobal);
		*phGlobal = NULL;
	}

	return bRes;
}

