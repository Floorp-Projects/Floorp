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

#include "cxstubs.h"
#ifdef MOZ_MAIL_NEWS
#include "mailpriv.h"
#endif
#include "timer.h"
#include "dialog.h"

CStubsCX::CStubsCX()    {
//	Purpose:    Create a CStubCX
//	Arguments:  void
//	Returns:    none
//	Comments:   Really of no use, just set the context type.
//	Revision History:
//      05-01-95    created GAB
//

    m_cxType = Stubs;
	m_pOuterUnk = NULL;
	m_ulRefCount = 1; // Never auto delete
}

//	Purpose:    Create a CStubCX
//	Arguments:  void
//	Returns:    none
//	Comments:   Really of no use, just set the context type.
//	Revision History:
//      05-01-95    created GABby
//
CStubsCX::CStubsCX(ContextType ctMyType, MWContextType XPType)
{
    m_cxType = ctMyType;
    GetContext()->type = XPType;
	m_pOuterUnk = NULL;
	m_ulRefCount = 1; // Never auto delete
}

CStubsCX::CStubsCX( LPUNKNOWN pOuterUnk )    {
//	Purpose:    Create a CStubCX
//	Arguments:  void
//	Returns:    none
//	Comments:   Really of no use, just set the context type.
//	Revision History:
//      05-01-95    created GAB
//
	m_pOuterUnk = pOuterUnk;
	m_ulRefCount = 0;

    m_cxType = Stubs;
}

//	Purpose:    Create a CStubCX
//	Arguments:  void
//	Returns:    none
//	Comments:   Really of no use, just set the context type.
//	Revision History:
//      05-01-95    created GABby
//
CStubsCX::CStubsCX( LPUNKNOWN pOuterUnk, ContextType ctMyType, MWContextType XPType)
{
	m_pOuterUnk = pOuterUnk;
	m_ulRefCount = 0;

    m_cxType = ctMyType;
    GetContext()->type = XPType;
}

//	Purpose:    Release outer unknown
//	Arguments:  void
//	Returns:    none
//	Comments:
//	Revision History:
//      05-01-95    created GAB
//
CStubsCX::~CStubsCX()   {
}


//
// Utility to parse a URL and return the domain part.
//
// e.g., IN:   http://www.netscape.com/comprod/netscape_products.html 
//       OUT:  www.netscape.com
//
static BOOL GetDomainFromURL( const char *pszURL, char *pszDomain )
{
    if( !pszURL || !*pszURL || !pszDomain )
    {
        return FALSE;
    }
    
    char szProtocolDelimiter[] = _T("://");
    
    char *pszCsr = _tcsstr( pszURL, szProtocolDelimiter );
    if( !pszCsr )
    {
        return FALSE;
    }
    
    pszCsr += _tcslen( szProtocolDelimiter );
    if( !*pszCsr )
    {
        return FALSE;
    }
    
    char *pszCsrEnd = _tcschr( pszCsr, '/' );
    
    USHORT uLen = pszCsrEnd ? (pszCsrEnd - pszCsr) : _tcslen( pszCsr );
    
    _tcsncpy( pszDomain, pszCsr, uLen );
    pszDomain[uLen] = 0;
    
    return TRUE;
}

void MakeSecureTitle( CAbstractCX *pCX, CString &csTitle )
{
    if( !pCX )
    {
        return;
    }
    
    char szDomain[1024];
    *szDomain = 0;
    URL_Struct *pURL = pCX->CreateUrlFromHist();
    if( pURL )
    {
        GetDomainFromURL( pURL->address, szDomain );
        NET_FreeURLStruct( pURL );
    }
    
    csTitle = _T("");
    if( *szDomain )
    {
        csTitle = szDomain;
        csTitle += _T(" - ");
    }
    csTitle += szLoadString( IDS_JSAPP );
}

//  Below are all the stubs of the class.
//  They are completely empty, some returning some default values which
//      can change if need be.
//  If you need special functionality in your higher level class, then
//      override these functions in that class, and leave these functions be!
//  05-01-95    created GAB

void CStubsCX::Alert(MWContext *pContext, const char *pMessage)	{
	//	We need to bring up an alert box.
	//	Override if this isn't what you need.
#ifdef XP_WIN16
    if(sysInfo.IsWin4() == FALSE)   {
        //  On win16 (under 3.1), enourmous strings cause thrashing of the heap
        //      when combined with MessageBox.
        //  See what we can do....
    }
#endif

	theApp.m_splash.SafeHide();

    CWnd * pWnd = GetDialogOwner();

#ifdef MOZ_MAIL_NEWS
    //  WM_REQUESTPARENT is mailpriv.h only
    if (!pWnd->IsWindowVisible())
    {
	   CWnd * pParent = (CWnd *)pWnd->SendMessage(WM_REQUESTPARENT);
	   if (pParent)
		   pWnd = pParent;
    }
#endif // MOZ_MAIL_NEWS
    
    CString csTitle = _T("");
    if( pContext && pContext->bJavaScriptCalling )
    {
        MakeSecureTitle( this, csTitle );
    }
    else
    {
        csTitle = szLoadString( AFX_IDS_APP_TITLE );        
    }
    pWnd->MessageBox( pMessage, (LPCSTR)csTitle, MB_TASKMODAL | MB_ICONEXCLAMATION | MB_OK );
}

STDMETHODIMP CStubsCX::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
    if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) this;
	else if (IsEqualIID(refiid,IID_IMWContext))
   		*ppv = (LPMWCONTEXT) this;
	else if (m_pOuterUnk) 
		return m_pOuterUnk->QueryInterface( refiid, ppv );

	if (*ppv != NULL) {
   		((LPUNKNOWN) *ppv)->AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CStubsCX::AddRef(void)
{
	if (m_pOuterUnk) 
		m_pOuterUnk->AddRef();
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CStubsCX::Release(void)
{
	ULONG ulRef;
	if (m_pOuterUnk) 
		m_pOuterUnk->Release();
	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) {
		ApiApiPtr(api);
		api->RemoveInstance(this);
		delete this;   	
	}
	return ulRef;   	
}

void CStubsCX::AllConnectionsComplete(MWContext *pContext)	{
}

void CStubsCX::UpdateStopState(MWContext *pContext) {
}

void CStubsCX::BeginPreSection(MWContext *pContext)	{
}

void CStubsCX::ClearCallNetlibAllTheTime(MWContext *pContext)	{
}

void CStubsCX::ClearView(MWContext *pContext, int iView)	{
}

XP_Bool CStubsCX::Confirm(MWContext *pContext, const char *pConfirmMessage)	{
	//	We need to ask for confirmation from the user.
	//	Override if this isn't what you need.
    
    CString csTitle = _T("");
    if( pContext && pContext->bJavaScriptCalling )
    {
        MakeSecureTitle( this, csTitle );
    }
    else
    {
        csTitle = szLoadString( AFX_IDS_APP_TITLE );        
    }
    
    int iStatus = GetDialogOwner()->MessageBox( pConfirmMessage, (LPCSTR)csTitle, MB_ICONQUESTION | MB_OKCANCEL );
    return(iStatus == IDOK);
}

MWContext *CStubsCX::CreateNewDocWindow(MWContext *pContext, URL_Struct *pURL)	{
    return(NULL);
}

void CStubsCX::DisplayBullet(MWContext *pContext, int iLocation, LO_BullettStruct *pBullet)	{
}

void CStubsCX::DisplayEdge(MWContext *pContext, int iLocation, LO_EdgeStruct *pEdge)	{
}

void CStubsCX::DisplayEmbed(MWContext *pContext, int iLocation, LO_EmbedStruct *pEmbed)	{
}

void CStubsCX::DisplayFormElement(MWContext *pContext, int iLocation, LO_FormElementStruct *pFormElement)	{
}

void CStubsCX::DisplayBorder(MWContext *pContext, int iLocation, int x, int y, int width, int height, int bw, LO_Color *color, LO_LineStyle style)	{
}

void CStubsCX::DisplayFeedback(MWContext *pContext, int iLocation, LO_Element *pElement)	{
}

void CStubsCX::DisplayHR(MWContext *pContext, int iLocation, LO_HorizRuleStruct *pHorizRule)	{
}

void CStubsCX::DisplayLineFeed(MWContext *pContext, int iLocation, LO_LinefeedStruct *pLineFeed, XP_Bool clear)	{
}

void CStubsCX::DisplaySubDoc(MWContext *pContext, int iLocation, LO_SubDocStruct *pSubDoc)	{
}

void CStubsCX::DisplayCell(MWContext *pContext, int iLocation, LO_CellStruct *pCell)	{
}

void CStubsCX::DisplaySubtext(MWContext *pContext, int iLocation, LO_TextStruct *pText, int32 lStartPos, int32 lEndPos, XP_Bool clear)    {
}

void CStubsCX::DisplayTable(MWContext *pContext, int iLocation, LO_TableStruct *pTable)	{
}

void CStubsCX::DisplayText(MWContext *pContext, int iLocation, LO_TextStruct *pText, XP_Bool clear)   {
}

void CStubsCX::EraseBackground(MWContext *pContext, int iLocation, int32 x, int32 y, uint32 width, uint32 height, LO_Color *pColor)   {
}

void CStubsCX::SetDrawable(MWContext *pContext, CL_Drawable *drawable)
{
}

void CStubsCX::EnableClicking(MWContext *pContext)	{
}

void CStubsCX::EndPreSection(MWContext *pContext)	{
}

int CStubsCX::FileSortMethod(MWContext *pContext)	{
    return(pContext->fileSort);
}

void CStubsCX::FinishedLayout(MWContext *pContext)	{
}

void CStubsCX::FormTextIsSubmit(MWContext *pContext, LO_FormElementStruct *pFormElement)	{
}

void CStubsCX::FreeEdgeElement(MWContext *pContext, LO_EdgeStruct *pEdge)	{
}

void CStubsCX::CreateEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp) {
}

void CStubsCX::SaveEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp) {
}

void CStubsCX::RestoreEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp) {
}

void CStubsCX::DestroyEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp) {
}

void CStubsCX::FreeEmbedElement(MWContext *pContext, LO_EmbedStruct *pEmbed)	{
}

void CStubsCX::GetEmbedSize(MWContext *pContext, LO_EmbedStruct *pEmbed, NET_ReloadMethod Reload)	{
}

void CStubsCX::GetFormElementInfo(MWContext *pContext, LO_FormElementStruct *pFormElement)	{
}

void CStubsCX::GetFormElementValue(MWContext *pContext, LO_FormElementStruct *pFormElement, XP_Bool bHidden)	{
}

void CStubsCX::GetTextFrame(MWContext *pContext, LO_TextStruct *pText,
			 int32 start, int32 end, XP_Rect *frame)     {
}

int CStubsCX::GetTextInfo(MWContext *pContext, LO_TextStruct *pText, LO_TextInfo *pTextInfo)	{
    return(0);
}

void CStubsCX::GraphProgress(MWContext *pContext, URL_Struct *pURL, int32 lBytesReceived, int32 lBytesSinceLastTime, int32 lContentLength)	{
}

void CStubsCX::GraphProgressDestroy(MWContext *pContext, URL_Struct *pURL, int32 lContentLength, int32 lTotalBytesRead)	{
}

void CStubsCX::GraphProgressInit(MWContext *pContext, URL_Struct *pURL, int32 lContentLength)	{
}

void CStubsCX::LayoutNewDocument(MWContext *pContext, URL_Struct *pURL, int32 *pWidth, int32 *pHeight, int32 *pmWidth, int32 *pmHeight)	{
}

void CStubsCX::Progress(MWContext *pContext, const char *pMessage)	{
}

char *CStubsCX::Prompt(MWContext *pContext, const char *pPrompt, const char *pDefault)	{
	//	We need to prompt the user for their username.
	//	If this isn't what you need, override.
    CDialogPRMT dlgPrompt(GetDialogOwner());

    if( pContext && pContext->bJavaScriptCalling )
    {
        CString csTitle = _T("");    
        MakeSecureTitle( this, csTitle );
    	dlgPrompt.SetSecureTitle( csTitle );        
    }
    
	theApp.m_splash.SafeHide();

    return(dlgPrompt.DoModal(pPrompt, pDefault));
}

char *CStubsCX::PromptWithCaption(MWContext *pContext, const char *pCaption, const char *pPrompt, const char *pDefault)	{

    CDialogPRMT dlgPrompt(GetDialogOwner());

	theApp.m_splash.SafeHide();
    
	return dlgPrompt.DoModal(pPrompt, pDefault, pCaption);
}

char *CStubsCX::PromptPassword(MWContext *pContext, const char *pMessage)	{
	//	We need to prompt the user for their password.
	//	If this isn't what you need, override.
    CDialogPASS dlgPass(GetDialogOwner());
    
    if( pContext && pContext->bJavaScriptCalling )
    {
        CString csTitle = _T("");    
        MakeSecureTitle( this, csTitle );
    	dlgPass.SetSecureTitle( csTitle );        
    }
    
	theApp.m_splash.SafeHide();

    return(dlgPass.DoModal(pMessage));
}

XP_Bool CStubsCX::PromptUsernameAndPassword(MWContext *pContext, const char *pMessage, char **ppUsername, char **ppPassword)	{
	//	We need to prompt the user for their password and username.
	//	If this isn't what you need, override.
	int iStatus;

	CDialogUPass dlgUPass(GetDialogOwner());
    
    CString csTitle = _T("");
    if( pContext && pContext->bJavaScriptCalling )
    {
        CString csTitle = _T("");    
        MakeSecureTitle( this, csTitle );
    	dlgUPass.SetSecureTitle( csTitle );        
    }
    
	theApp.m_splash.SafeHide();

	iStatus = dlgUPass.DoModal((char *) pMessage, ppUsername, ppPassword);
    
	return(iStatus == IDOK);
}

void CStubsCX::ResetFormElement(MWContext *pContext, LO_FormElementStruct *pFormElement)	{
}

void CStubsCX::SetBackgroundColor(MWContext *pContext, uint8 cRed, uint8 cGreen, uint8 cBlue)	{
}

void CStubsCX::SetCallNetlibAllTheTime(MWContext *pContext)	{
}

void CStubsCX::SetDocDimension(MWContext *pContext, int iLocation, int32 lWidth, int32 lLength)	{
}

void CStubsCX::SetDocPosition(MWContext *pContext, int iLocation, int32 lX, int32 lY)	{
}

void CStubsCX::SetDocTitle(MWContext *pContext, char *pTitle)	{
}

void CStubsCX::SetFormElementToggle(MWContext *pContext, LO_FormElementStruct *pFormElement, XP_Bool bToggle)	{
}

void CStubsCX::SetProgressBarPercent(MWContext *pContext, int32 lPercent)	{
}

XP_Bool CStubsCX::ShowAllNewsArticles(MWContext *pContext)	{
    return(FALSE);
}

char *CStubsCX::TranslateISOText(MWContext *pContext, int iCharset, char *pISOText)	{
    return(pISOText);
}

XP_Bool CStubsCX::UseFancyFTP(MWContext *pContext)	{
	//	Go off the context settings.
	//	If this isnt' wanted, override.
    return(pContext->fancyFTP);
}

XP_Bool CStubsCX::UseFancyNewsgroupListing(MWContext *pContext)	{
	//	Go off the applications settings.
	//	If this isn't wanted, override.
//    return(!strcmp(theApp.m_pFancyNews->GetCharValue(), "yes"));
	// I don't think this is used anymore! -- jonm 1/2/97
	return TRUE;
}

void CStubsCX::GetJavaAppSize(MWContext *pContext, LO_JavaAppStruct *pJava, NET_ReloadMethod Reload)	{
}

void CStubsCX::DisplayJavaApp(MWContext *pContext, int iLocation, LO_JavaAppStruct *pJava)	
{
}

void CStubsCX::FreeJavaAppElement(MWContext *pContext, LJAppletData *appletD)
{
}
void CStubsCX::HideJavaAppElement(MWContext *pContext, 
                               LJAppletData *java_struct)
{
}
void CStubsCX::GetDocPosition(MWContext * pContext, int iLoc, int32 * pX, int32 * pY)  
{
}

/* java specific functions to allow delayed window creation and transparency */
void CStubsCX::HandleClippingView(MWContext *pContext, LJAppletData *appletD, int x, int y, int width, int height)
{
}
void CStubsCX::DrawJavaApp(MWContext *pContext, int iLocation, LO_JavaAppStruct *pJava)
{
}

//  URL Exit Routines
void CStubsCX::GetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext)    {
}

void CStubsCX::TextTranslationExitRoutine(PrintSetup *pTextFE)  {
}

CFrameCX::CFrameCX(ContextType ctMyType, CFrameGlue *pFrame)    
{
	m_pFrame = pFrame;
    m_cxType = ctMyType;
	//	No old progress;
	m_lOldPercent = 0;
}

CFrameCX::~CFrameCX()
{
}

void CFrameCX::SetProgressBarPercent(MWContext *pContext, int32 lPercent)	{
	//	Enusre the safety of the value.
	if(lPercent < -1)	{
		lPercent = 0;
	}
	if(lPercent > 100)	{
		lPercent = 100;
	}

	if(lPercent == 0 || m_lOldPercent == lPercent)	{
		return;
	}
	m_lOldPercent = lPercent;
	
    LPNSSTATUSBAR pIStatusBar = NULL;
    LPCHROME pChrome = GetFrame()->GetChrome();
    if(pChrome) {
        pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
        if ( pIStatusBar ) {
            pIStatusBar->SetProgress(CASTINT(lPercent));
            pIStatusBar->Release();
        }
    }
}

void CFrameCX::Progress(MWContext *pContext, const char *pMessage)	{
	LPNSSTATUSBAR pIStatusBar = NULL;
	LPCHROME pChrome = GetFrame()->GetChrome();
	if(pChrome) {
    	pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
    	if ( pIStatusBar ) {
    		pIStatusBar->SetStatusText(pMessage);
    		pIStatusBar->Release();
    	}
	}
}

int32 CFrameCX::QueryProgressPercent()	{
	return m_lOldPercent;
}

void CFrameCX::StartAnimation()
{
	LPCHROME pChrome = GetFrame()->GetChrome();
	if(pChrome) {
	    pChrome->StartAnimation();
	}
}

void CFrameCX::StopAnimation()
{
    LPCHROME pChrome = GetFrame()->GetChrome();
    if(pChrome) {
        pChrome->StopAnimation();
    }
}

void CFrameCX::AllConnectionsComplete(MWContext *pContext)    
{
    //  Call the base.
    CStubsCX::AllConnectionsComplete(pContext);

	//	Stop our frame's animation, if the main context of the frame is no longer busy.
    if(GetFrame()->GetMainContext()) {
    	if(XP_IsContextBusy(GetFrame()->GetMainContext()->GetContext()) == FALSE) {
    		//	Okay, stop the animation.
			StopAnimation();

    		//	Also, we can clear the progress bar now.
			LPNSSTATUSBAR pIStatusBar = NULL;
			LPCHROME pChrome = GetFrame()->GetChrome();
			if(pChrome) {
    			pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
    			if ( pIStatusBar ) {
    				pIStatusBar->SetProgress(0);
    				pIStatusBar->Release();
    			}
			}
		}
    }
}

CWnd *CFrameCX::GetDialogOwner() const {
	return m_pFrame->GetFrameWnd();
}
