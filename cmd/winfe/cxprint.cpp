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

#include "cxprint.h"
#include "apipage.h"	// page setup api
#include "netsvw.h"
#include "npapi.h"
#include "np.h"
#include "feimage.h"
#include "il_icons.h"
#include "netsprnt.h"
#include "cntritem.h"
#include "intl_csi.h"
#include "prefapi.h"

BOOL CPrintCX::m_bGlobalBlockDisplay = FALSE;

//	A callback function from the print manager.
//	See CDC::SetAbortProc for more information
BOOL CALLBACK EXPORT PrintAbortProc(HDC hDC, int iStatus)	{
	//	See if we've cancelled.....  Only we can't.
	//	This wasn't designed apparently to handle more than one print job per app
	//		at a time.

    //  Turn off display for a moment in all print contexts.
    //  This keeps us from going re-etrant into GDI.
    BOOL bOldBlock = CPrintCX::m_bGlobalBlockDisplay;
    CPrintCX::m_bGlobalBlockDisplay = TRUE;
	
	//	Here we shove all messages on through.
	MSG msg;
	while(::PeekMessage(&msg, NULL, NULL, NULL, PM_NOREMOVE))	{
		if(!theApp.NSPumpMessage()) {
            //  Restore display flag.
            CPrintCX::m_bGlobalBlockDisplay = bOldBlock;

			//	Get out if WM_QUIT received.
			return(FALSE);
		}
	}

    //  Restore display flag.
    CPrintCX::m_bGlobalBlockDisplay = bOldBlock;

	//	Just continue printing.
	return(TRUE);
}

extern "C" void FE_Print(const char *pUrl)  {
    CPrintCX::AutomatedPrint(pUrl, NULL, NULL, NULL);
}

//  Print with no user intervention.
//  Null on an argument means default.
void CPrintCX::AutomatedPrint(const char *pDocument, const char *pPrinter, const char *pDriver, const char *pPort)  {
    TRACE("Automated Print job starting (%s, %s, %s, %s)\n", pDocument, pPrinter, pDriver, pPort);

    //  Our document is probably local or a shortcut, convert to something networthy.
	CString csUrl;
	WFE_ConvertFile2Url(csUrl, pDocument);

    //  Create the actual URL struct.
    URL_Struct *pUrl = NET_CreateURLStruct(csUrl, NET_DONT_RELOAD);
    if(pUrl == NULL)    {
        return;
    }

	//	Create a new print job.
	CPrintCX *pPrint = new CPrintCX(pUrl);

	//	Set up the print job.
	//	Here, basically we need to do the necessary work that AFX
	//		normally does to generate a proper print DC.
	pPrint->m_pcpiPrintJob = new CPrintInfo;

	//	Wether or not we'll actually continue with this.
	BOOL bFailure = FALSE;

	//	Do the print setup process by manually fetching the DC.
	TRY	{
        //  Are we attempting the default printer or do we already know which printer.
        if(pPrinter == NULL || pDriver == NULL || pPort == NULL)    {
            //  Use a default, if we can figure one out.
			if (!theApp.GetPrinterDeviceDefaults(&pPrint->m_pcpiPrintJob->m_pPD->m_pd))  {
                AfxThrowResourceException();
			}

			if (pPrint->m_pcpiPrintJob->m_pPD->m_pd.hDC == NULL) {
				// call CreatePrinterDC if DC was not created by above
				if (pPrint->m_pcpiPrintJob->m_pPD->CreatePrinterDC() == NULL)    {
                    AfxThrowResourceException();
                }
				pPrint->m_hdcPrint = pPrint->m_pcpiPrintJob->m_pPD->m_pd.hDC;
			}
        }
        else    {
            if(!(pPrint->m_hdcPrint = ::CreateDC(pDriver, pPrinter, pPort, NULL)))   {
                AfxThrowResourceException();
            }
        }

        //	Set up the DC and print info.
        pPrint->m_pcpiPrintJob->m_pPD->m_pd.nFromPage = 1;
        pPrint->m_pcpiPrintJob->m_pPD->m_pd.nToPage = (unsigned short)-1;
        pPrint->m_pcpiPrintJob->m_pPD->m_pd.nCopies = 1;    
    }
	CATCH(CException, e)	{
		//	View went away while print dialog was up and caused a GPF.
		//	This isn't an elegant fix.
		bFailure = TRUE;
	}
	END_CATCH

	if(bFailure)	{
		//	We're not going to be printing, user hit cancel.
		pPrint->DestroyContext();
		NET_FreeURLStruct(pUrl);
		return;
	}

	//	Gleen relevant information from the CDC and the CPrintInfo.
	pPrint->Initialize(FALSE);

	//	Ready to split off and do the print job.
	pPrint->CommencePrinting(pUrl);

	//	We leave the context all alone, it will eventually destroy
	//		itself when the print job is completed.
}

void CPrintCX::PrintAnchorObject(URL_Struct *pUrl, CView *pView, SHIST_SavedData *pSavedData, char *pDisplayUrl) {
//	Purpose:	Indirect constructor for CPrintCX objects.
//	Arguments:	pUrl	The URL to print.
//				pView	The view originating the print request.
//              pSavedData  The form element data we should copy.
//	Returns:	void
//	Comments:	This function effectively takes a print job and moves
//					it outside of the view by putting it into a new
//					context.
//	Revision History:
//		05-27-95	created GAB

	//	Create a new print job.
	CPrintCX *pPrint = new CPrintCX(pUrl, pSavedData, pDisplayUrl);

	//	Set up the print job.
	pPrint->m_pcpiPrintJob = new CPrintInfo;

	//	Wether or not we'll actually continue with this.
	BOOL bFailure = FALSE;

    //	Do the normal print setup process thorugh a print dialog.
    int iCLSID;
    NPEmbeddedApp *pPlugins;
	TRY	{
        //  CACHE some info while we're safe with the view going away.
        MWContext *pViewContext = ((CNetscapeView*)pView)->GetContext()->GetContext();

        // The mfc print dlg code uses our hidden frame for the owner.  The owner should
        // be the active frame (or one of its popups).  Otherwise the frame is not disabled,
        // effectively removing the modal aspect of the dialog.
        CWnd *pSaveWnd = theApp.m_pActiveWnd;
        theApp.m_pActiveWnd = CWnd::GetActiveWindow();
      
		if(pView->DoPreparePrinting(pPrint->m_pcpiPrintJob) == FALSE)	{
			bFailure = TRUE;
		}
        else if(pPrint->m_pcpiPrintJob->m_pPD == NULL ||
            pPrint->m_pcpiPrintJob->m_pPD->m_pd.hDC == NULL)    {

            //  Whoa!  DoPreparePrinting succeeded, but there is no DC???
            //  This happens when a printer driver is installed for a network
            //      printer (like a NetWare printer), and the user is not
            //      logged into that network.
            //  Give the user some type of error....

            theApp.m_pActiveWnd = pSaveWnd;
            
		    CString temp;
		    temp.LoadString(IDS_CANTPRINT_LOGIN);
		    FE_Alert(pViewContext, temp);

            bFailure = TRUE;
        }
        theApp.m_pActiveWnd = pSaveWnd;    
        
        //  If the context is no longer in the list of contexts, get out as the view is gone.
        if(XP_IsContextInList(pViewContext))    {
            iCLSID = INTL_DefaultDocCharSetID(pViewContext);
            pPlugins = ((CNetscapeView*)pView)->GetContext()->GetContext()->pluginList;
        }
        else    {
            //  Fail now, the view is gone.
            bFailure = TRUE;
        }
	}
	CATCH(CException, e)	{
		//	View went away while print dialog was up and caused a GPF.
		//	This isn't an elegant fix.
		bFailure = TRUE;
	}
	END_CATCH

	if(bFailure)	{
		//	We're not going to be printing, user hit cancel.
		pPrint->DestroyContext();
		NET_FreeURLStruct(pUrl);
		return;
	}

	//	Set up the DC.
	pPrint->m_hdcPrint = pPrint->m_pcpiPrintJob->m_pPD->m_pd.hDC;

	//	Gleen relevant information from the CDC and the CPrintInfo.
	pPrint->Initialize(FALSE);

	// Pass old document charset id to print context
	// This will make i18n converter do right thing for each context
	pPrint->m_iCSID = iCLSID;

	if(	iCLSID != CS_LATIN1)
	{
		INTL_CharSetInfo print_csi = LO_GetDocumentCharacterSetInfo(pPrint->GetContext());
		INTL_SetCSIDocCSID(print_csi, iCLSID);
		INTL_SetCSIWinCSID(print_csi, INTL_DocToWinCharSetID(iCLSID));
	}
	//	Ready to split off and do the print job.
	pPrint->CommencePrinting(pUrl);

	//	We leave the context all alone, it will eventually destroy
	//		itself when the print job is completed.
}

void CPrintCX::PreviewAnchorObject(CPrintCX *& pPreview, URL_Struct *pUrl, CView *pView, CDC *pDC, 
                                   CPrintInfo *pInfo, SHIST_SavedData *pSavedData, char *pDisplayUrl)
{
//	Purpose:	Indirect constructor for CPrintCX objects doing preview.
//	Arguments:	pPreview	A reference to a pointer which will will assign in the
//								context just prior to the network load.
//				pAnchor	The URL to print.
//				pHist	The history entry of the print job in the other context.
//				pView	The view originating the print request.
//				pDC		The DC of the preview (we don't use our own).
//				pInfo	The print job information.
//              pSavedData  Form element data we should copy.
//	Returns:	CPrintCX	The print context doing the preview (we won't
//					be destroying ourselves, they will be destroying us).
//	Comments:	This function effectively takes a print job and moves
//					it outside of the view by putting it into a new
//					context.
//				pPreview is needed as a reference and not a retval since assignment
//					of the variable needs to happen before we return (reentrancy
//					problems in the calling function avoidance).
//	Revision History:
//		06-14-95	created GAB

	//	Create a new print job.
	CPrintCX *pPrint = new CPrintCX(pUrl, pSavedData, pDisplayUrl);

	//	Set up the print job.
	pPrint->m_bPreview = TRUE;
	pPrint->m_previewDC = pDC;
	pPrint->m_pcpiPrintJob = pInfo;

	//	Set our preview view.
	pPrint->m_pPreviewView = pView;

	//	Get information from the CDC and CPrintInfo.
	pPrint->Initialize(FALSE);

	//	Assign what used to be the return value before we actually do the load
	//		from the network.
	//	We leave the context all alone, it will eventually get destroyed
	//		by those which created us.
	pPreview = pPrint;

	// Pass old document charset id to print context
	// This will make i18n converter do right thing for each context
	MWContext *context = ((CNetscapeView*)pView)->GetContext()->GetContext();
	pPrint->m_iCSID = INTL_DefaultDocCharSetID(context);

	if(	pPrint->m_iCSID != CS_LATIN1) 
	{
		INTL_CharSetInfo view_csi = LO_GetDocumentCharacterSetInfo(context);
		INTL_CharSetInfo print_csi = LO_GetDocumentCharacterSetInfo(pPrint->GetContext());
		INTL_SetCSIDocCSID(print_csi, INTL_GetCSIDocCSID(view_csi));
		INTL_SetCSIWinCSID(print_csi, INTL_GetCSIWinCSID(view_csi));
	}

	pPrint->CommencePrinting(pUrl);
}

void CPrintCX::ScreenToPrint(POINT* point, int num)
{
	POINT *tp = point;
	for (int i = 0; i < num; i++, tp++) {
		tp->x = tp->x * (printRes.cx / screenRes.cx);
		tp->y = tp->y * (printRes.cy / screenRes.cy);
	}	
}

CPrintCX::CPrintCX(URL_Struct *pUrl, SHIST_SavedData *pSavedData, char *pDisplayUrl)	{
//	Purpose:	Construct a context for printing.
//	Arguments:	pAnchor	The anchor we're printing
//              pSavedData  The form data we want to copy to the print context.
//              pDisplayUrl The URL text to display in header/footer
//	Returns:	none
//	Comments:	Sets the context type, etc.
//	Revision History:
//		05-27-95	created GAB

 	TRACE("Creating CPrintCX %p\n", this);

    //  Clear URL position.
    //  If we don't, when the page is scrolled we can't print -- weird.
    //  Do not handle named anchors (strip them out), same reasons.
    if(pUrl)    {
        pUrl->position_tag = 0;
        if(pUrl->address) {
            if(strrchr(pUrl->address, '#')) {
                *strrchr(pUrl->address, '#') = '\0';
            }
        }
    }

	// this is a hack to save the layout data for embedlist.  For embed layout will
	// make a copy of the pSavedData.  We need to free the memory here.
	m_embedList = (lo_SavedEmbedListData*)pUrl->savedData.EmbedList;
	m_cxType = Print;
	GetContext()->type = MWContextPrint;

	m_hdcPrint = NULL;
	m_pcpiPrintJob = NULL;
	p_TimeOut = NULL;
    //  Haven't printed anything yet.
    m_iLastPagePrinted = 0;

	//	Save the anchor we're printing; might be used in DOCINFO
	int len = strlen(pUrl->address);
	m_pAnchor = new char[len + 1];

	XP_STRNCPY_SAFE(m_pAnchor, pUrl->address, len + 1);

#ifdef EDITOR
    // This is the Url address we are want to display
    // in header or footers. m_pAnchor may be a local temp file
    if(pDisplayUrl && strrchr(pDisplayUrl, '#'))
    {
        *strrchr(pDisplayUrl, '#') = '\0';
    }
    m_pDisplayUrl = pDisplayUrl ? XP_STRDUP(pDisplayUrl) : NULL;
#else
    m_pDisplayUrl = NULL;
#endif

	//	No status dialog as of yet.
	m_pStatusDialog = NULL;

	//	User hasn't cancelled yet.
	m_bCancel = FALSE;

	//	We start off blocking all display calls
    m_iDisplayMode = BLOCK_DISPLAY;

	//	Are we a preview context?
	m_bPreview = FALSE;
	m_pPreviewView = NULL;


	//	Do we need to call the start doc member yet again?
	m_bNeedStartDoc = TRUE;

	//	How we end the document, in AbortDoc or EndDoc
	//	Start off with aborting (no output).
	m_bAbort = TRUE;

    m_bAllConnectionCompleteCalled = FALSE;
    m_bFinishedLayoutCalled = FALSE;
    m_bFormatStarted = FALSE;

    // Font used for Header and Footer, should depending on current docuemnt charset
   	m_hFont = NULL;
    m_iFontCSID = CS_LATIN1;
    m_iCSID = CS_LATIN1;

    m_pDrawable = NULL;
    
    m_lDocWidth = 0;
    m_lDocHeight = 0;
	m_bHandleCancel = FALSE;

    //  Finally, copy the form data if present.
    if(pSavedData && pSavedData->FormList)  {
        TRACE("Cloning form data for print job.\n");
        LO_CloneFormData(pSavedData, GetDocumentContext(), pUrl);
    }
}

CPrintCX::~CPrintCX()	{
//	Purpose:	Destruct a print context
//	Arguments:	void
//	Returns:	none
//	Comments:	Clean up any printing resources allocated.
//	Revision History:
//		05-27-95	created GAB

	TRACE("Deleting CPrintCX %p\n", this);

	if(m_pStatusDialog != NULL)	{
		m_pStatusDialog->DestroyWindow();
		delete m_pStatusDialog;
	}

    if (m_hFont) {
        //  The font has correctly unselected itself from the DC
        //      in the written code in this file, such that we do
        //      not need to unselect it before deletion.
        ::DeleteObject(m_hFont);
        m_hFont = NULL;
    }


	if(m_cplCaptured.IsEmpty() == FALSE)	{
		POSITION rIndex = m_cplCaptured.GetHeadPosition();
		LTRB *pFreeMe;
		while(rIndex != NULL)	{
			pFreeMe = (LTRB *)m_cplCaptured.GetNext(rIndex);
			delete pFreeMe;
		}

		m_cplCaptured.RemoveAll();
	}

	if(m_cplPages.IsEmpty() == FALSE)	{
		POSITION rIndex = m_cplPages.GetHeadPosition();
		LTRB *pFreeMe;
		while(rIndex != NULL)	{
			pFreeMe = (LTRB *)m_cplPages.GetNext(rIndex);
			delete pFreeMe;
		}

		m_cplPages.RemoveAll();
	}

	if(IsPrintPreview() == FALSE)	{
		//	Need to release our print job information.
		//	If we didn't need to start a document, then we started one in some manner,
		//		decide how we should end it.
		if(m_bNeedStartDoc == FALSE)	{
			if(m_bAbort == TRUE)	{
				::AbortDoc(m_hdcPrint);
			}
			else	{
				::EndDoc(m_hdcPrint);
			}
		}
        if(m_hdcPrint)  {
            ::DeleteDC(m_hdcPrint);
        }

		delete m_pcpiPrintJob;
	}
    m_hdcPrint = NULL;
    m_pcpiPrintJob = NULL;

	if(m_pAnchor)   {
		delete [] m_pAnchor;
        m_pAnchor = NULL;
    }
    XP_FREEIF(m_pDisplayUrl);

    if (m_pDrawable) {
        delete m_pDrawable;
        m_pDrawable = NULL;
    }

	// MWH - this is a hack to free the embed list that layout make copy from the original
	// SavedData.  I removed the freeing from lib\layout\layfree.c lo_FreeDocumentEmbedListData.
	// and free the data here.  This will fix an OLE printing problem.  The problem is when a .doc file
	// is on the net, i.e. http://....//xxx.doc.  When layout free the EmbedList in 
	// lo_FreeDocumentEmbedListData will cause the page not printed.  Since for printing we need
	// to use the cached data.

	if (m_embedList && (m_embedList->embed_data_list != NULL)) {
		int32 i;
		lo_EmbedDataElement* embed_data_list;

		PA_LOCK(embed_data_list, lo_EmbedDataElement*, m_embedList->embed_data_list);
		for (i=0; i < m_embedList->embed_count; i++)
		{
			if (embed_data_list[i].freeProc && embed_data_list[i].data)
				(*(embed_data_list[i].freeProc))(GetContext(), embed_data_list[i].data);
		}
		PA_UNLOCK(m_embedList->embed_data_list);
		PA_FREE(m_embedList->embed_data_list);

		m_embedList->embed_count = 0;
		m_embedList->embed_data_list = NULL;

	}
	if (p_TimeOut) {
		FE_ClearTimeout(p_TimeOut);  // kill the timer.
		p_TimeOut = 0;
	}
}

HDC CPrintCX::GetAttribDC()
{
    HDC hRetval = NULL;
    if(IsPrintPreview())    {
        hRetval = m_previewDC->m_hAttribDC;
    }
    else    {
        hRetval = m_hdcPrint;
    }

    return(hRetval);
}


void CPrintCX::ReleaseContextDC(HDC pDC)	{
//	Purpose:	Release a DC previously gotten through GetContextDC.
//	Arguments:	pDC	The CDC to release.
//	Returns:	void
//	Comments:	We don't do anything here, as the CDC is static
//					throughout the print job.
//	Revision History:
//		05-27-95	created GAB
}

void CPrintCX::Initialize(BOOL bOwnDC, RECT *pRect, BOOL bInitialPalette, BOOL bNewMemDC)	{
//	Purpose:	Initialize any print parameters from preferences and
//					print setup.
//	Arguments:	void
//	Returns:	void
//	Comments:	All instance specific print parameters should be
//					initialized here.
//				A CDC is required to do this.
//	Revision History:
//		05-30-95	created GAB

	//	Call the base
	HDC hdc = GetContextDC();
#ifdef XP_WIN32
	m_hOrgPrintDC = hdc;
	m_printBk = FALSE;
	m_hOffscrnDC = 0;
#endif

	CDCCX::Initialize(bOwnDC, pRect, bInitialPalette, bNewMemDC);

	CDC * pAttrDC;
	pAttrDC = theApp.m_pMainWnd->GetDC();
	if (!IsPrintPreview())	{
		SetMappingMode(hdc);
		m_lConvertX = GetContext()->convertPixX = 1440 / pAttrDC->GetDeviceCaps(LOGPIXELSX);
	    m_lConvertY = GetContext()->convertPixY = 1440 / pAttrDC->GetDeviceCaps(LOGPIXELSY);
		screenRes.cx = 0; // we don't care.
		screenRes.cy = 0;
		printRes.cx = 0;
		printRes.cy = 0;
#ifdef XP_WIN32
		PREF_GetBoolPref("browser.print_background",&m_printBk);
#endif
	}
	else {
		screenRes.cx = ::GetDeviceCaps(m_previewDC->GetSafeHdc(), LOGPIXELSX);
		screenRes.cy = ::GetDeviceCaps(m_previewDC->GetSafeHdc(), LOGPIXELSY);
		printRes.cx = ::GetDeviceCaps(m_previewDC->m_hAttribDC, LOGPIXELSX);
		printRes.cy = ::GetDeviceCaps(m_previewDC->m_hAttribDC, LOGPIXELSY);
		m_lConvertX = GetContext()->convertPixX = printRes.cx  / screenRes.cx;
		m_lConvertY = GetContext()->convertPixY = printRes.cy / screenRes.cy;

	}
	//	Have our FE conversions (class local for speed) be the same.
    ApiPageSetup(api,0);


	//	Determine and set the page size.
	SIZE csPageSize;
	api->GetMargins ( &m_lLeftMargin, &m_lRightMargin, &m_lTopMargin, &m_lBottomMargin );
	if (!IsPrintPreview()) {
		csPageSize.cx = ::GetDeviceCaps(hdc, HORZRES);
		csPageSize.cy = ::GetDeviceCaps(hdc, VERTRES);
#ifdef XP_WIN32
		// MWH - The calculation below is to prepare a memory DC for printing
		// background.  I can not use printer's resoultion to calculate the size of bitmap
		// for this memoryDC, because 600 dpi * 24 bit/pixel * width * height will cause
		// the bitmap to be too big.  I'm going to use the screen resolution here
		// to figure the size of the bitmap for my memoryDC.  When it is time to 
		// display the bitmap.  DisplayPixmap will scale the image from 96 dpi to
		// 600 dpi.
		if (m_printBk) {
			int xres, yres;
			xres = pAttrDC->GetDeviceCaps(LOGPIXELSX);
			yres = pAttrDC->GetDeviceCaps(LOGPIXELSY);
			int width, height;
			width = (csPageSize.cx +  GetDeviceCaps(hdc, LOGPIXELSX)) / GetDeviceCaps(hdc, LOGPIXELSX);
			height = (csPageSize.cy +  GetDeviceCaps(hdc, LOGPIXELSY)) / GetDeviceCaps(hdc, LOGPIXELSY);
			m_offscrnWidth = width * xres;
			m_offscrnHeight = height * yres;

			m_hOffscrnDC = ::CreateCompatibleDC(pAttrDC->GetSafeHdc());
			if ( m_hOffscrnDC) {
				BITMAPINFO lpbmi;
				lpbmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
				lpbmi.bmiHeader.biWidth = m_offscrnWidth;
				lpbmi.bmiHeader.biHeight = m_offscrnHeight;
				lpbmi.bmiHeader.biPlanes = 1;
				lpbmi.bmiHeader.biBitCount = 24;
				lpbmi.bmiHeader.biCompression = BI_RGB;
				lpbmi.bmiHeader.biSizeImage = 0;
				lpbmi.bmiHeader.biXPelsPerMeter = 0;
				lpbmi.bmiHeader.biYPelsPerMeter = 0;

				lpbmi.bmiHeader.biClrUsed = 0;        // default value    
				lpbmi.bmiHeader.biClrImportant = 0;   // all important
				void* ptr;
				m_hOffScrnBitmap = ::CreateDIBSection(pAttrDC->GetSafeHdc(),&lpbmi, DIB_RGB_COLORS,  
					&ptr, NULL, NULL);
 
				::SetMapMode(m_hOffscrnDC, MM_TEXT);
				if (m_hOffScrnBitmap) {
					m_saveBitmap = (HBITMAP)::SelectObject(m_hOffscrnDC, ( HGDIOBJ)m_hOffScrnBitmap );
				}
				else {
					m_printBk = FALSE;
					::DeleteDC(m_hOffscrnDC);
					m_hOffscrnDC = 0;

				}
			}
			else {// Can not display Background, just print forground instead.
				m_printBk = FALSE;
			}
		}
#endif
	    ::DPtoLP(hdc, (POINT*)&csPageSize, 1);
	}
	else {
		m_lLeftMargin = (m_lLeftMargin * printRes.cx) / 1440;
		m_lRightMargin = (m_lRightMargin * printRes.cx) / 1440;
		m_lTopMargin = (m_lTopMargin * printRes.cy) / 1440;
		m_lBottomMargin = (m_lBottomMargin * printRes.cy) / 1440;

		csPageSize.cx = ::GetDeviceCaps(m_previewDC->m_hAttribDC, HORZRES);
 	    csPageSize.cy = ::GetDeviceCaps(m_previewDC->m_hAttribDC, VERTRES);

	}

	theApp.m_pMainWnd->ReleaseDC(pAttrDC);
	ReleaseContextDC(hdc);
	m_lPageHeight = csPageSize.cy;
	m_lPageWidth = csPageSize.cx;
	m_lWidth = csPageSize.cx - (m_lRightMargin + m_lLeftMargin);
	m_lHeight = csPageSize.cy - (m_lBottomMargin + m_lTopMargin);

    CreateHeaderFooterFont();

    TEXTMETRIC tm;
    if(m_hFont) {
        HFONT hOldFont = (HFONT)::SelectObject(hdc, m_hFont);
        ::GetTextMetrics(hdc, &tm);
        ::SelectObject(hdc, hOldFont);
    }
    else {
        AfxMessageBox(IDS_NOPRINTERFONT);
        ::GetTextMetrics(hdc, &tm);
    }

    m_iMaxWidth = CASTINT(( m_lPageWidth / tm.tmAveCharWidth ) / 2);

	//	What are we doing about colors?
	m_bBlackText = api->BlackText();
	m_bBlackLines = api->BlackLines();

	//	What are we doing about drawing?
	m_bSolidLines = api->SolidLines();
	m_bBitmaps = TRUE;
	m_bBackground = FALSE;
    m_bReverseOrder = api->ReverseOrder() ? TRUE : FALSE;

	//	Headers and footers?
	m_bNumber = api->Footer() ? TRUE : FALSE;
	m_bTitle = api->Header() ? TRUE : FALSE;
}

BOOL CPrintCX::AdjustRect(LTRB& Rect)
{
	LTRB orgRect(Rect);
	//  Adjust for margins.
	Rect.left += m_lLeftMargin;
	Rect.top += m_lTopMargin;
	Rect.right += m_lLeftMargin;
	Rect.bottom += m_lTopMargin;
	//	Adjust the Y coordinate for the current page we are printing.
	POSITION rIndex = m_cplPages.FindIndex(m_pcpiPrintJob->m_nCurPage - 1);
	if(rIndex == NULL)	{
		//	This could be called during incremental display.
		//	Don't allow drawing at that point.
		return(FALSE);
	}
	LTRB *pPage = (LTRB *)m_cplPages.GetAt(rIndex);
	Rect.top -= pPage->top;
	Rect.bottom -= pPage->top;

	//	Form a Rect of the current page, in the same coords as the element.
	//	The bottom of the page is determined on a page by page basis.
	LTRB Page;
	Page.left = m_lLeftMargin;
	Page.top = m_lTopMargin;
	Page.right = Page.left + pPage->Width();
	Page.bottom = Page.top + pPage->Height();

	//	Part of this element must fall within these coordinates to be drawn.
	//	Clipping will take care of partial display.
	if(Rect.top >= Page.bottom || Rect.bottom <= Page.top ||
		Rect.right <= Page.left || Rect.left >= Page.right)	{
		if(CanBlockDisplay())	{
			return(FALSE);
		}
	}
	// MHW - if we are printing background image, and the current printing
	// operation is to the temparary offscreen, need to addjust	the rect to 
	// the coordinate according to the offscreen dc.
#ifdef XP_WIN32
	if (m_printBk && m_hdcPrint == m_hOffscrnDC) {
		orgRect.top -= pPage->top;
		orgRect.bottom -= pPage->top;
		Rect.left = (orgRect.left + m_lConvertX) / m_lConvertX;
		Rect.top = (orgRect.top + m_lConvertY)/ m_lConvertY;
		Rect.right = (orgRect.right + m_lConvertX) / m_lConvertX;
		Rect.bottom = (orgRect.bottom + m_lConvertY)/  m_lConvertY;

	}
#endif
	return (TRUE);
}

BOOL CPrintCX::ResolveElement(LTRB& Rect, LO_FormElementStruct *pFormElement)	{
    CDCCX::ResolveElement(Rect, pFormElement);
	return AdjustRect(Rect);
}

BOOL CPrintCX::ResolveElement(LTRB& Rect, int32 x, int32 y, int32 x_offset, int32 y_offset,
								int32 width, int32 height)
{
	CDCCX::ResolveElement(Rect, x, y, x_offset, y_offset, width, height);
	return AdjustRect(Rect);
}

BOOL CPrintCX::ResolveElement(LTRB& Rect, NI_Pixmap *pImage, int32 lX, int32 lY, 
								int32 orgx, int32 orgy, 
								uint32 ulWidth, uint32 ulHeight)
{
	//	Call the base first.
	CDCCX::ResolveElement(Rect, pImage, lX, lY, orgx, orgy, ulWidth, ulHeight);
	return AdjustRect(Rect);
}

BOOL CPrintCX::ResolveElement(LTRB& Rect, LO_EmbedStruct *pEmbed, int iLocation, Bool bWindowed)	{
	//	Call the base first.
	CDCCX::ResolveElement(Rect, pEmbed, iLocation, bWindowed);
	return AdjustRect(Rect);
}

BOOL CPrintCX::ResolveLineSolid()	{
	return(m_bSolidLines);
}

COLORREF CPrintCX::ResolveBGColor(unsigned uRed, unsigned uGreen, unsigned uBlue)	{
	//	See if we're allowing the background color to be set.
	if(m_bBackground == FALSE)	{
		//	Nope, set it to be white.
		return(RGB(255,255,255));
	}
	else	{
		//	Do the default action.
		return(CDCCX::ResolveBGColor(uRed, uGreen, uBlue));
	}
}

BOOL CPrintCX::ResolveHRSolid(LO_HorizRuleStruct *pHorizRule)	{
	//	See if we're allowing 3D HRs.
	if(m_bSolidLines == TRUE)	{
		return(TRUE);
	}
	else	{
		//	Do the default.
		return(CDCCX::ResolveHRSolid(pHorizRule));
	}
}

void CPrintCX::ResolveTransparentColor(unsigned uRed, unsigned uGreen, unsigned uBlue)	{
	//	See what we're doing with the background.
	if(m_bBackground == FALSE)	{
		//	Set it to white.
		CDCCX::ResolveTransparentColor(255, 255, 255);
	}
	else	{
		//	Set it to whatever.
		CDCCX::ResolveTransparentColor(uRed, uGreen, uBlue);
	}
}

COLORREF CPrintCX::ResolveDarkLineColor()	{
	//	See if we're drawing black lines.
	if(m_bBlackLines == TRUE)	{
		return(RGB(0,0,0));
	}
	else	{
		//	Do the default.
		return(CDCCX::ResolveDarkLineColor());
	}
}

COLORREF CPrintCX::ResolveLightLineColor()	{
	//	See if we're drawing black lines.
	if(m_bBlackLines == TRUE)	{
		return(RGB(0,0,0));
	}
	else	{
		//	Do the default.
		return(CDCCX::ResolveLightLineColor());
	}
}

COLORREF CPrintCX::ResolveBorderColor(LO_TextAttr *pAttr)	{
	//	See if we're drawing black lines.
	if(m_bBlackLines == TRUE)	{
		return(RGB(0,0,0));
	}
	else	{
		//	Do the default.
		return(CDCCX::ResolveBorderColor(pAttr));
	}
}

COLORREF CPrintCX::ResolveTextColor(LO_TextAttr *pAttr)	{
	//	If we're only printing black text, then only select the color black.
	//	Otherwise, simply call the base.
	COLORREF rgbRetval;
	if(m_bBlackText == TRUE)	{
		rgbRetval = RGB(0,0,0);
	}
	else	{
		rgbRetval = CDCCX::ResolveTextColor(pAttr);
	}

	return(rgbRetval);
}

void CPrintCX::DisplayIcon(int32 x, int32 y, int icon_number)
{
    if (GetDisplayMode() == BLOCK_DISPLAY)
        return;
	else if (GetDisplayMode() == CAPTURE_POSITION)	{
        int32 lOrgX, lOrgY;
        int32 width, height;

        GetDrawingOrigin(&lOrgX, &lOrgY);
        GetIconDimensions(&width, &height, icon_number);
        
        width *= m_lConvertX;
        height *= m_lConvertY;

		//	Capture the coordinates of this element.
		Capture(x + lOrgX, y + lOrgY, width, height);

		//	We don't actually allow display while we are capturing the area.
		return;
	}

	//	Block drawing of icons if we don't allow it.
	//	They might be attempting to determine the size of an icon.
	if(m_bBitmaps == TRUE || x != NULL || y != NULL)	{
#ifdef XP_WIN32
		if (m_printBk && !IsPrintPreview()) {
			SubOffscreenPrintDC();
			CDCCX::DisplayIcon(x, y, 
								icon_number);
			RestorePrintDC();
		}
#endif
		CDCCX::DisplayIcon(x, y, 
							icon_number);
	}
}

void CPrintCX::LayoutNewDocument(MWContext *pContext, URL_Struct *pURL, int32 *pWidth, int32 *pHeight, int32 *pmWidth, int32 *pmHeight)	{
//	Purpose:	This formally sets up margins and such with the Layout engine.
//	Arguments:	pContext	The context under which this is loading.
//				pURL		The loading URL.
//				pWidth		The width of the page (adjust for margins).
//				pHeight		The height of the page (we report this as huge)
//								so that we can figure it manually later.
//				pmWidth		Margin width, we report as 0.
//				pmHeight	Margin height, we report as 0.
//	Returns:	void
//	Comments:	The origin of the print job must always remain 0,0 for this
//					to work correctly.
//				It is important to note that we are handling all margins
//					ourselves rather than letting the layout engine attempt
//					to use them.  This is done for finer control though harder.
//	Revision History:
//		06-09-95	created GAB

	//	Call the base, though we'll override any values.
	CDCCX::LayoutNewDocument(pContext, pURL, pWidth, pHeight, pmWidth, pmHeight);

	//	Assign the page width, but adjust for our internal margins.
	//	This should be the only place where we use the right margin.
	*pWidth = m_lPageWidth - (m_lLeftMargin + m_lRightMargin);

	//	Assign the page height, but adjust for our internal margins.
	//	This should be the only place where we use the bottom margin.
	*pHeight = m_lPageHeight - (m_lTopMargin + m_lBottomMargin);

	//	Make sure layout doesn't attempt to use any margins itself, regardless.
	*pmWidth = 0;
	*pmHeight = 0;
}

void CPrintCX::CommencePrinting(URL_Struct *pUrl)	{
//	Purpose:	Officially start a print job.
//	Arguments:	pAnchor	The anchor for which to print.
//	Returns:	void
//	Comments:	This won't print forms with post data....
//				This has always been the case in all versions.
//				If such a need exists, I don't see what would be so
//					hard about doing it, but layout has mythical
//					tendencies to not handle this correctly.
//	Revision History:
//		05-30-95	created GAB

	//	Create the print status dialog if not in preview.
	if(IsPrintPreview() == FALSE)	{
		m_pStatusDialog = new CPrintStatusDialog(NULL, this);
		m_pStatusDialog->m_csLocation = pUrl->address;
		WFE_CondenseURL(m_pStatusDialog->m_csLocation, 40, FALSE);
		m_pStatusDialog->Create(CPrintStatusDialog::IDD, NULL);
		StartDoc();

		//	Also attempt to start the document for the first time.
		//	This is to help the cannot start print job problem, we are experienced
		//		with technical diffculties.
	}

    MWContext *pContext = GetContext();	
    HDC hdc = GetContextDC();	
    CL_Drawable *pPrinterDrawable = NULL;
    
    // Create a drawable that represents the printer
    m_pDrawable = new CPrinterDrawable(hdc, m_lLeftMargin, m_lRightMargin,
                                       m_lTopMargin, m_lBottomMargin, this);
    if (m_pDrawable)
        pPrinterDrawable = CL_NewDrawable(CASTUINT(0), 
                                          CASTUINT(0),
                                          CL_PRINTER,
                                          &wfe_drawable_vtable,
                                          (void *)m_pDrawable);

    if (pPrinterDrawable) {
        // Create a compositor for the printer context
        // The window size is initially 0,0,0,0. It's modified when
        // we know the size of the document.
        pContext->compositor = CL_NewCompositor(pPrinterDrawable,
                                                NULL,
                                                0, 0, 
                                                0, 0,
                                                0);
    
        if (pContext->compositor)
            CL_SetCompositorDrawingMethod(pContext->compositor,
                                          CL_DRAWING_METHOD_BACK_TO_FRONT_ONLY);
        else
            CL_DestroyDrawable(pPrinterDrawable);
    }
    else
        pContext->compositor = NULL;
	ReleaseContextDC(hdc);

	//	Create the URL which will handle the load, and ask for it.
	GetUrl(pUrl, FO_CACHE_AND_PRINT);
}

CWnd *CPrintCX::GetDialogOwner() const	{
//	Purpose:	Returns the owner of any dialogs which we will create.
//	Arguments:	void
//	Returns:	CWnd *	The dialog owner
//	Comments:	Returns the print status dialog as the owner, so it
//					must be present for this to work.
//	Revision History:
//		05-30-95	created GAB

	if(IsPrintPreview() == FALSE)	{
		return(m_pStatusDialog);
	}
	else	{
		return(((CGenericView *)m_pPreviewView)->GetFrame()->GetFrameWnd());
	}
}

int CPrintCX::StartDoc()	{
	//	For modularity purposes only.
	//	This is called from several places.

	//	If we're in preview, or if we've already started the document, just
	//		return success.
	if(IsPrintPreview() == TRUE || m_bNeedStartDoc == FALSE || IsDestroyed() == TRUE)	{
		//	Don't do this if we don't need it.
		return(0);
	}

	//	Attempt to start the print job.
	HDC pDC = GetContextDC();

	//	Set the docinfo structure up.
	//	Be sure to first clear the structure to 0, this may or may
	//		not cause StartDoc to fail unexplainably if we don't.
	//	MSDN: Q135119
	memset(&m_docInfo, 0, sizeof(m_docInfo));
	m_docInfo.cbSize = sizeof(m_docInfo);
	MWContext *pct = GetContext();
    // DocName may be the "DisplayUrl" used by Composer when
    //  we are printing/previewing a temporary file URL 
    m_docInfo.lpszDocName = ((pct->title != NULL) ? pct->title : 
                              (m_pDisplayUrl ? (const char *)m_pDisplayUrl : (const char *)m_pAnchor));
	m_docInfo.lpszOutput = NULL;

	//	Hold the document name to 31 characters.
	CString cs31 = m_docInfo.lpszDocName;
	if(cs31.GetLength() > 31)	{
		cs31.ReleaseBuffer(31);
	}
	m_docInfo.lpszDocName = (const char *)cs31;

	int iRetval = ::StartDoc(pDC, &m_docInfo);

	if(iRetval >= 0)	{
		//	We got through.
		m_bNeedStartDoc = FALSE;
	}
	ReleaseContextDC(pDC);

	return(iRetval);
}

// Record document height and width
void CPrintCX::SetDocDimension(MWContext *pContext, int iLocation, int32 lWidth, int32 lHeight)   {
    CDCCX::SetDocDimension(pContext, iLocation, lWidth, lHeight);
    
    m_lDocWidth = lWidth;
    m_lDocHeight = lHeight;
}

void CPrintCX::CapturePositions()
{
    CL_Compositor *compositor;
    

    //  For capturing, we want all the elements in a document to display.
    //  Since we can't size the compositor window to be the size of the
    //  document (the compositor window must have coordinates that fit into
    //  a 16-bit coordinate space), we take snapshots. In other words,
    //  we make the compositor window the size of the physical page and
    //  keep scrolling down till capture the entire document.
    compositor = GetContext()->compositor;
    if (compositor) {
        XP_Rect rect;
        int n;

        // The compositor window is the size of the page (minus margins)
        CL_ResizeCompositorWindow(compositor, m_lWidth, m_lHeight);
        
        rect.left = 0;
        rect.top = 0;
        rect.right = m_lWidth;
        rect.bottom = m_lHeight;

        // We display all the elements twice. This is to deal with the
        // fact that certain elements (images, embeds and applets for
        // instance) are always displayed in a compositor pass after
        // the containing HTML content. We only capture during the
        // second pass.
        for (n = 0; n < 2; n++) {
            // Till we've covered the entire document, we keep scrolling
            // down and taking snapshots
            for (m_lCaptureScrollOffset = 0; 
                 m_lCaptureScrollOffset <= m_lDocHeight; 
                 m_lCaptureScrollOffset += m_lHeight) {
                CL_ScrollCompositorWindow(compositor, 
                                          0, m_lCaptureScrollOffset);
                CL_RefreshWindowRect(GetContext()->compositor, &rect);
            }
            m_iDisplayMode = CAPTURE_POSITION;
        }

        CL_ScrollCompositorWindow(compositor, 0, 0);
    }
    else {
        //	Set that we want all  Display* calls to capture document positions
        m_iDisplayMode = CAPTURE_POSITION;
        LO_RefreshArea(GetDocumentContext(), 0, 0, 0x7FFFFFFF, 0x7FFFFFFF);
    }
}

void CPrintCX::GetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext)
{
    /* bad status: finish printing now */
    if((iStatus < 0) && !m_bFormatStarted)
        FormatAndPrintPages(pContext);

    CDCCX::GetUrlExitRoutine(pUrl, iStatus, pContext);
}

static void DeferredDestroyPrintContext(void *cx)
{
	((CPrintCX*)(cx))->p_TimeOut = 0;
	((CPrintCX*)(cx))->CDCCX::DestroyContext();
}

/* Avoid destroying the document out from underneath layout when DestroyContext() is
   called (indirectly) from layout, as in FormatAndPrintPages() */
void CPrintCX::DestroyContext()	{
	m_bHandleCancel = TRUE;
	//Trun back off the display.
    m_iDisplayMode = BLOCK_DISPLAY;
	p_TimeOut = FE_SetTimeout(DeferredDestroyPrintContext, this, 0);
#ifdef XP_WIN32
	::SelectObject(m_hOffscrnDC, ( HGDIOBJ)m_saveBitmap );
	::DeleteObject(m_hOffScrnBitmap);
	if (m_hOffscrnDC)
		::DeleteDC(m_hOffscrnDC);
#endif
}

void CPrintCX::FormatAndPrintPages(MWContext *pContext)	{
	//	Check to see if the user cancelled the job, if so, we're done.
	if(m_bCancel == TRUE )	{
		TRACE("Destroying canceled print job.\n");
		if (!m_bHandleCancel)
			DestroyContext();
		return;
	}

    m_bFormatStarted = TRUE;

    CapturePositions();

    CL_ResizeCompositorWindow(GetContext()->compositor, m_lWidth, m_lHeight);

    //      Now we're ready to actually start drawing the display elements
    m_iDisplayMode = DISPLAY;
        
	//	Do the entire print job now.

	//	Calculating page boundries.
	//	All layout information has been detained by the print job while
	//		loading the first time.
	FormatPages();

	//	If there are no pages, get out now.
	if(m_cplPages.IsEmpty() == TRUE)	{
		CString temp;
		temp.LoadString(IDS_NOPAGETOPRINT);
		FE_Alert(GetContext(), temp);
		if(IsPrintPreview() == FALSE)	{
			DestroyContext();
		}
		return;
	}

	//	One last try.
	//	Do we need to attempt to start the document?
	if(0 > StartDoc())	{
		CString temp;
		temp.LoadString(IDS_NOPRINTJOB);
		CFE_Alert(GetContext(), temp);
		DestroyContext();
		return;
	}

	//	If we're not in preview, then we do the real thing.
	if(IsPrintPreview() == FALSE)	{
		//	Attempt to start the print job.
		HDC pDC = GetContextDC();

		//	Set the callback function.
		::SetAbortProc(pDC, PrintAbortProc);

		m_bAbort = FALSE;	//	We switch to wanting to use EndDoc now....

		//	Here is the main print loop.
        UINT uFromPage = m_pcpiPrintJob->GetFromPage();
        UINT uToPage = m_pcpiPrintJob->GetToPage();
        if(m_bReverseOrder) {
            uFromPage = m_pcpiPrintJob->GetToPage();
            uToPage = m_pcpiPrintJob->GetFromPage();
        }
        if(uFromPage > (UINT)m_cplPages.GetCount())   {
            uFromPage = m_cplPages.GetCount();
        }
        if(uToPage > (UINT)m_cplPages.GetCount())   {
            uToPage = m_cplPages.GetCount();
        }

		int iStep = uToPage >= uFromPage ? 1 : -1;
		for(m_pcpiPrintJob->m_nCurPage = uFromPage;
			m_pcpiPrintJob->m_nCurPage - iStep != uToPage;
			m_pcpiPrintJob->m_nCurPage += iStep)	{

			//	See if the user wanted to abort the print job.
			if(m_bCancel == TRUE)	{
				m_bAbort = TRUE;
				break;
			}

			//	See if the page we are about to print actually exists.
			//	If not, it's time to stop the print job.
			POSITION rIndex = m_cplPages.FindIndex(m_pcpiPrintJob->m_nCurPage - 1);
			if(rIndex == NULL)	{
				//	Time to stop printing.  The specified page isn't found.
				m_pcpiPrintJob->m_bContinuePrinting = FALSE;
			}

			//	See if we are to kick out of the print job (done).
			if(m_pcpiPrintJob->m_bContinuePrinting == FALSE)	{
				break;
			}

			//	Let the DC know a new page is starting.
			if(0 > ::StartPage(pDC))	{
				//	Some type of error occurred.
				m_bAbort = TRUE;
				break;
			}

			//	Print the page
			PrintPage(m_pcpiPrintJob->m_nCurPage);

			//	Let the DC know a page is ending.
			if(0 > ::EndPage(pDC))	{
				//	Some type of error occurred.
				m_bAbort = TRUE;
				break;
			}
		}

		//	Set this to make sure everyone knows we aren't continuing to print.
		m_pcpiPrintJob->m_bContinuePrinting = FALSE;

		//	Done, remove this object.
		//	End doc or abort doc are called in the destructor.
		ReleaseContextDC(pDC);
		DestroyContext();
	}
	else	{
		//	Otherwise, we tell the owning view to refresh itself.
		((CGenericView *)m_pPreviewView)->GetFrame()->GetFrameWnd()->Invalidate();
	}
}

void CPrintCX::FinishedLayout(MWContext *pContext)	{

	//	Call the base.
	CDCCX::FinishedLayout(pContext);

    if(IsDestroyed())
        return;

    m_bFinishedLayoutCalled = TRUE;

    if(m_bAllConnectionCompleteCalled && !m_bFormatStarted)
    {
        FormatAndPrintPages(pContext);
    }
}

void CPrintCX::AllConnectionsComplete(MWContext *pContext)	{

	//	Call the base.
	CDCCX::AllConnectionsComplete(pContext);

    if(IsDestroyed())
        return;

    m_bAllConnectionCompleteCalled = TRUE;

    if(m_bFinishedLayoutCalled && !m_bFormatStarted)
    {
        FormatAndPrintPages(pContext);
    }
}

void CPrintCX::Progress(MWContext *pContext, const char *pMessage)	{
//	Purpose:	Display the progression of the load.
//	Arguments:	pContext	The context loading a URL.
//				pMessage	The message to display.
//	Returns:	void
//	Comments:	Updates the CPrintStatus dialog mainly.
//	Revision History:
//		05-31-95	created GAB

	//	Call the base.
	CDCCX::Progress(pContext, pMessage);

	//	Simply set the progress text in the dialog.
	if(pMessage != NULL)	{
		if(IsPrintPreview() == FALSE)	{
			m_pStatusDialog->SetDlgItemText(IDC_PROGRESS, pMessage);
		}
		else	{
            //	Get the frame from the preview view, and have it do some
            //		progress for us.
            LPCHROME pChrome = ((CGenericView *)m_pPreviewView)->GetFrame()->GetChrome();
            if(pChrome) {
                LPNSSTATUSBAR pIStatusBar = NULL;
                pChrome->QueryInterface( IID_INSStatusBar, (LPVOID *) &pIStatusBar );
                if ( pIStatusBar ) {
                    pIStatusBar->SetStatusText( pMessage );
                    pIStatusBar->Release();
                }
            }
		}
	}
}

void CPrintCX::CancelPrintJob()	{
//	Purpose:	Cancels the current print job.
//	Arguments:	void
//	Returns:	void
//	Comments:	This should fully cancel a print job at any stage.
//	Revision History:
//		05-31-95	created GAB

	if(m_bCancel == FALSE)	{
		TRACE("Marking the print job as cancelled.\n");

		//	First, set a member stating that we are cancelling the job.
		m_bCancel = TRUE;

		//	Second, set a member in the print job saying that the load
		//		has been interrupted.
		m_pcpiPrintJob->m_bContinuePrinting = FALSE;

		//	If we're loading, then we interrupt that load.
		XP_InterruptContext(GetContext());
	}
}

void CPrintCX::FormatPages()	{
//	Purpose:	Take the information regarding all elements to be
//					laid out and determine which elements fall on which
//					page.
//	Arguments:	void
//	Returns:	void
//	Comments:	Herein is how to properly print tables....
//	Revision History:
//		06-09-95	created GAB

	TRACE("Figuring page boundries\n");
	CString temp;
	temp.LoadString(IDS_FORMATTING);
	FE_Progress(GetContext(), temp);

	//	Figure out the dimensions of the page taking into account margins.
	//	We can't and don't care about width.
	//	We use positive height to match layout.

	int32 lHeight = m_lPageHeight - (m_lTopMargin + m_lBottomMargin);
	int32 lWidth = m_lPageWidth - (m_lLeftMargin + m_lRightMargin);

	//	The algorithm here is as follows:
	//		Find all boundry elements. A boundry element, is an element cut
	//			off by the bottom of a page.
	//		A boundry element, if not at the top of a page, consitutes starting
	//			a new page.
	//		When a boundry element starts a new page, all elements starting
	//			below that element go on the next page with the boundry element.

	CPtrList cplCurPage;
	LTRB PageRect(0, 0, lWidth, lHeight);
	LTRB *pPage;
	LTRB *pBoundry;
	LTRB *pElement;
	POSITION rIndex, rOldIndex;
	BOOL bUseSmartClipping;
	int32 iOldPageBottom;
	BOOL bUsedClipping;

	while(m_cplCaptured.IsEmpty() == FALSE)	{
		//	First, go through all captured elements, and move any which begin
		//		on the current page into a separate list.
		rIndex = m_cplCaptured.GetHeadPosition();
		while(rIndex != NULL)	{
			rOldIndex = rIndex;
			pElement = (LTRB *)m_cplCaptured.GetNext(rIndex);
			
			if(pElement->top <= PageRect.bottom)	{
				m_cplCaptured.RemoveAt(rOldIndex);
				cplCurPage.AddTail((void *)pElement);
			}
		}

        //  We'll want to attempt to use a smarter algorithm in most cases,
        //      such that we do not cut lines in half.  However, we will
        //      give up on that approach if we lose over half of the page
        //      height.
        bUseSmartClipping = TRUE;
        iOldPageBottom = PageRect.bottom;
        bUsedClipping = FALSE;

        while(1) {
            //	Further, find in that new list all boundries, and remember
            //		the beginning of the worst one.
            //	Remember, that if it starts at the beginning of a page or before
            //		the beginning of this page, then there is nothing we can do.
            //	Also, don't use elements longer than a single page by themselves as a
            //		boundry.
            pBoundry = NULL;
            rIndex = cplCurPage.GetHeadPosition();
            while(rIndex != NULL)	{
                pElement = (LTRB *)cplCurPage.GetNext(rIndex);
                if(pElement->bottom > PageRect.bottom && pElement->top > PageRect.top)	{
                    //	Make sure the element won't fit on a page by itself before we
                    //		approve it as a boundry.
                    if(pElement->Height() > PageRect.Height())	{
                        //	To high to fit on a page.  Don't use as a boundry.
                        continue;
                    }

                    BOOL bSetBoundry = FALSE;
                    if(NULL == pBoundry)	{
                        pBoundry = pElement;
                        bSetBoundry = TRUE;
                    }
                    else if(pBoundry->top > pElement->top)	{
                        pBoundry = pElement;					
                        bSetBoundry = TRUE;
                    }

                    if(bSetBoundry && bUseSmartClipping) {
                        //  Adjust the bottom of the page to the boundry.
                        //  Also, reset the loop, to catch any new betrayers.
                        PageRect.bottom = pBoundry->top;
                        rIndex = cplCurPage.GetHeadPosition();
                        bUsedClipping = TRUE;
                    }
                }
            }

            //  If we lost over half the page, redo the algorithm
            //      not using smart clipping.
            if(bUsedClipping && PageRect.Height() < (lHeight / 2))   {
                bUsedClipping = FALSE;
                bUseSmartClipping = FALSE;
                PageRect.bottom = iOldPageBottom;
                continue;
            }
            break;
        }

		//	Next move all elements falling below the boundry (if it exists)
		//		back into the captured element list.
		if(pBoundry != NULL)	{
			rIndex = cplCurPage.GetHeadPosition();
			while(rIndex != NULL)	{
				rOldIndex = rIndex;
				pElement = (LTRB *)cplCurPage.GetNext(rIndex);
				if(pElement->top >= pBoundry->top)	{
					cplCurPage.RemoveAt(rOldIndex);
					m_cplCaptured.AddTail((void *)pElement);
				}
			}
		}

		//	Also, we need to move all elements falling below this new boundry
		//		back into the captured list so that they are counted as yet
		//		another page. (think: 1 element that is 50 pages long....)
		rIndex = cplCurPage.GetHeadPosition();
		while(rIndex != NULL)	{
			rOldIndex = rIndex;
			pElement = (LTRB *)cplCurPage.GetNext(rIndex);
			if(pElement->bottom > PageRect.bottom)	{
				cplCurPage.RemoveAt(rOldIndex);
				m_cplCaptured.AddTail((void *)pElement);
			}
		}

		//	Cause all current page elements to be destroyed.
		rIndex = cplCurPage.GetHeadPosition();
		while(rIndex != NULL)	{
			rOldIndex = rIndex;
			pElement = (LTRB *)cplCurPage.GetNext(rIndex);
			cplCurPage.RemoveAt(rOldIndex);
			delete pElement;
		}

		//	Create the new page entry (duplicate of the current page).
		//	This will be used later in PrintPage.
		pPage = new LTRB(PageRect.left, PageRect.top, PageRect.right,
			PageRect.bottom);
		m_cplPages.AddTail((void *)pPage);

		//	Update the value of the current page to be the next page.
		PageRect.top = PageRect.bottom + 1;
		PageRect.bottom = PageRect.top + lHeight;
	}
}

void CPrintCX::PrintTextAllign ( HDC hdc, char * szBuffer, int position, int hpos )
{
	SIZE csExtent;
	int32 Y; 
	int32 X;
    CString cs = szBuffer;
    WFE_CondenseURL ( cs, m_iMaxWidth, FALSE );

    CreateHeaderFooterFont();

    HFONT hOldFont = NULL;
    if(m_hFont) {
        hOldFont = (HFONT)::SelectObject(hdc, m_hFont);
    }

	ResolveTextExtent(hdc, cs, strlen(cs), &csExtent);

	if ( hpos == POS_FOOTER )
		Y = m_lPageHeight - csExtent.cy - 1;
	else
		Y = 0;

	switch ( position )
	{
		case POS_CENTER:
			X = m_lPageWidth / 2 - csExtent.cx / 2;
			break;
		case POS_LEFT:
			X = 0;
			break;																									  
		case POS_RIGHT:
			X = m_lPageWidth - csExtent.cx;
			break;

	}

	CIntlWin::TextOut(m_iFontCSID , hdc, CASTINT(X), CASTINT(Y), cs, strlen(cs));

    if(hOldFont) {
        ::SelectObject(hdc, hOldFont);
    }
}

void CPrintCX::PrintPage(int iPage, HDC pNewDC, CPrintInfo *pNewInfo)	{
//	Purpose:	Print a specific page.
//	Arguments:	iPage	The page which to print.
//	Returns:	void
//	Comments:	This is the real thing.  If there is no page information
//					then we get out of the print loop.
//	Revision History:
//		06-09-95	created GAB

	//	If we've got new information, use it.
	if(pNewDC != NULL)	{
		m_hdcPrint = pNewDC;
	}
	if(pNewInfo != NULL)	{
		m_pcpiPrintJob = pNewInfo;
	}

	//	Page numbers always go from 1 on up.
	//	Get the position of the page in out list.
	POSITION rIndex = m_cplPages.FindIndex(iPage - 1);
	if(rIndex == NULL)	{
		//	Time to stop printing, the specified page isn't found.
		//	Only do this when we're actually printing.
		if(IsPrintPreview() == FALSE)	{
			m_pcpiPrintJob->m_bContinuePrinting = FALSE;
		}
		return;
	}

	TRACE("Printing page %d\n", iPage);
    m_iLastPagePrinted = iPage;

	char szMessage[80];
	PR_snprintf(szMessage, sizeof(szMessage), szLoadString(IDS_PRINTING_PAGE), iPage);
	FE_Progress(GetContext(), szMessage);

	//	Get the actual page, and have layout refresh the area.
	//	This will in turn cause the display functions to be called, and this
	//		will in turn cause the display functions to call ResolveElement,
	//		and therein we adjust for margins and determine wether or not
	//		to output an actual element.
	LTRB *pPage = (LTRB *)m_cplPages.GetAt(rIndex);

	//	Save the DC state, so that clipping information can be restored to
	//		defaults after the page is printed.
	HDC hdc = GetContextDC();
	int iSaveDC = ::SaveDC(hdc);


	TRACE("LO_RefreshArea(%ld,%ld,%ld,%ld);\n", pPage->left, pPage->top,
		pPage->Width(), pPage->Height());

	//	Instead of passing the actual page params, we have layout refresh
	//		everything possible.
	//	All ResolveElement functions will have to appropriately block display
	//		for the current page.
	//	In this manner, we correctly handle elements longer than one page.
    CL_Compositor *compositor = GetContext()->compositor;
    if (compositor) {
        XP_Rect rect;
        
        CL_ScrollCompositorWindow(compositor, 0, pPage->top);
        
        rect.left = 0;
        rect.top = 0;
        rect.right = m_lWidth;
        // For the last page, we use the complete height of the physical
        // page so that, if there's a background, it won't get cut off
        // early.
        if (rIndex == m_cplPages.GetTailPosition())
            rect.bottom = m_lHeight;
        else
            rect.bottom = pPage->Height();

        CL_ResizeCompositorWindow(compositor, rect.right, rect.bottom);
        CL_RefreshWindowRect(compositor, &rect);
    }
    else {
        //	Now, make it small enough for the page only.
        ::IntersectClipRect(hdc, CASTINT(m_lLeftMargin),
                            CASTINT(m_lTopMargin),
                            CASTINT(m_lLeftMargin + pPage->Width()),
                            CASTINT(m_lTopMargin + pPage->Height()));
        LO_RefreshArea(GetDocumentContext(), 0, 0, 0x7FFFFFFF, 0x7FFFFFFF);
    }

	//	Restore the DC state (clipping region).
	::RestoreDC(hdc, iSaveDC);

	//	Headers and footers.

	//  Footers first! Footers first!

	ApiPageSetup(api,0);
	int iFooter = api->Footer ( );
	int iPosDate = 0;
	BOOL bPosTotal = 0;
	int iPosCount = 0;

	if ( iFooter & PRINT_PAGENO )
		iPosCount = POS_CENTER;

	if ( iFooter & PRINT_PAGECOUNT )
		bPosTotal = TRUE;

	if ( iFooter & PRINT_DATE )
		if ( iPosCount )
		{
			iPosCount = POS_LEFT;
			iPosDate  = POS_RIGHT;
		}
		else
			iPosDate = POS_CENTER;

	// page number
	if( iPosCount )	{
		char szBuffer[32];

		if ( bPosTotal )
			PR_snprintf(szBuffer, sizeof(szBuffer), szLoadString(IDS_PAGE_N_OF_COUNT), iPage, m_cplPages.GetCount( ));
		else
			PR_snprintf(szBuffer, sizeof(szBuffer), "%d", iPage);

		PrintTextAllign ( hdc, szBuffer, iPosCount, POS_FOOTER );

	}

	if ( iPosDate ) {
		char aBuffer[65];
		char szDate[30];
		char szTime[30];
#ifdef XP_WIN32
		GetDateFormat(LOCALE_SYSTEM_DEFAULT, DATE_SHORTDATE, NULL, NULL, szDate, 30);
		GetTimeFormat(LOCALE_SYSTEM_DEFAULT, TIME_NOSECONDS, NULL, NULL, szTime, 30);
		sprintf(aBuffer,"%s %s", szDate, szTime);
#else
		sprintf(aBuffer,"%s %s", _strdate(szDate), _strtime(szTime) );
#endif
		PrintTextAllign ( hdc, aBuffer, iPosDate, POS_FOOTER );
	}

	int iHeader = api->Header ( );
	int iPosTitle = 0;
	int iPosURL = 0;

	if ( iHeader & PRINT_TITLE )
		iPosTitle = POS_CENTER;

	if ( iHeader & PRINT_URL )
		if ( iPosTitle )
		{
			iPosTitle = POS_LEFT;
			iPosURL  = POS_RIGHT;
		}
		else
			iPosURL = POS_CENTER;

	if ( iPosTitle )
	{
		char *pTitle = GetContext()->title;
		if(pTitle != NULL)	
			PrintTextAllign ( hdc, pTitle, iPosTitle, POS_HEADER );
	}

	if ( iPosURL )
	{
        // Here's where we finally use the DisplayURL
        //   instead of the temporary file URL in Composer
		const char * pURL = m_pDisplayUrl ? m_pDisplayUrl : m_pAnchor;
		if ( pURL && *pURL ) {
			char *pszUrlType = NET_ParseURL( pURL, GET_PROTOCOL_PART );

			if ( !pszUrlType || (XP_STRCASECMP(pszUrlType, "mailbox:") && 
                                 XP_STRCASECMP(pszUrlType, "news:")    &&
                                 XP_STRCASECMP(pszUrlType, "snews:")   &&
                                 XP_STRCASECMP(pszUrlType, "imap:")) ) {
                                 
    			PrintTextAllign ( hdc, (char *)pURL, iPosURL, POS_HEADER );
            }
            
            if ( pszUrlType )
                XP_FREE( pszUrlType );
        }
	}

	ReleaseContextDC(hdc);	
}

void CPrintCX::EraseBackground(MWContext *pContext, int iLocation, 
			    int32 x, int32 y, uint32 width, uint32 height,
                               LO_Color *pColor)
{
	int32 orgx, orgy, orgWidth, orgHeight;

	orgx = x;
	orgy = y;
	orgWidth = width;
	orgHeight = height;
    if (GetDisplayMode() == DISPLAY) {
        x += m_lLeftMargin;
        y += m_lTopMargin;
        
        //	Adjust the Y coordinate for the current page we are printing.
        POSITION rIndex = m_cplPages.FindIndex(m_pcpiPrintJob->m_nCurPage - 1);
        if(rIndex == NULL)	{
            //	This could be called during incremental display.
            //	Don't allow drawing at that point.
            return;
        }
        LTRB *pPage = (LTRB *)m_cplPages.GetAt(rIndex);
        y -= pPage->top;
        
        //	Form a Rect of the current page, in the same coords as the 
        //      background. The bottom of the page is determined on a page 
        //      by page basis.
        LTRB Page;
        Page.left = m_lLeftMargin;
        Page.top = m_lTopMargin;
        Page.right = Page.left + pPage->Width();
        Page.bottom = Page.top + pPage->Height();
        
        if(y >= Page.bottom || (y+(int32)height) <= Page.top ||
           (x + (int32)width) <= Page.left || x >= Page.right)
            return;
        else {
#ifdef XP_WIN32
			if ( m_printBk && !IsPrintPreview()) {
				SubOffscreenPrintDC();
	            CDCCX::EraseBackground(pContext, iLocation, 
                                   orgx / m_lConvertX, orgy / m_lConvertY,
                                   orgWidth / m_lConvertX, orgHeight / m_lConvertY, pColor);
				RestorePrintDC();
			}
#endif
            CDCCX::EraseBackground(pContext, iLocation, 
                                   x, y,
                                   width, height, pColor);
		}
    }
}


void CPrintCX::DisplayBullet(MWContext *pContext, int iLocation, LO_BullettStruct *pBullet)	{
    if (GetDisplayMode() == BLOCK_DISPLAY)
        return;
	else if (GetDisplayMode() == CAPTURE_POSITION)	{
        int32 lOrgX, lOrgY;
        
        GetDrawingOrigin(&lOrgX, &lOrgY);
        
		//	Capture the coordinates of this element.
		Capture(pBullet->x + pBullet->x_offset + lOrgX, pBullet->y + pBullet->y_offset + lOrgY, pBullet->width, pBullet->height);

		//	We don't actually allow display while we are capturing the area.
		return;
	}

	// 	Call the base for actual display.  It never works right
	//		in Bullet Basic code for some reason.
#ifdef XP_WIN32
	if ( m_printBk && !IsPrintPreview()) {
		SubOffscreenPrintDC();
		CDCCX::DisplayBullet(pContext, iLocation, pBullet);
		RestorePrintDC();
	}
#endif
	CDCCX::DisplayBullet(pContext, iLocation, pBullet);
}

void CPrintCX::DisplayEmbed(MWContext *pContext, int iLocation, LO_EmbedStruct *pEmbed)	{
    if (GetDisplayMode() == BLOCK_DISPLAY)
        return;
	else if (GetDisplayMode() == CAPTURE_POSITION)	{
		//	Capture the coordinates of this element.
		Capture(pEmbed->x + pEmbed->x_offset, pEmbed->y + pEmbed->y_offset, pEmbed->width, pEmbed->height);

		//	We don't actually allow display while we are capturing the area.
		return;
	}

	//	Call the base for actual display.
    NPEmbeddedApp* pEmbeddedApp = (NPEmbeddedApp*)pEmbed->FE_Data;
	if (pEmbeddedApp->type ==  NP_OLE) {
		CNetscapeCntrItem *pItem = (CNetscapeCntrItem *)pEmbeddedApp->fe_data;
		pItem->SetPrintDevice( &(m_pcpiPrintJob->m_pPD->m_pd) );
	}
	CDCCX::DisplayEmbed(pContext, iLocation, pEmbed);
}

void CPrintCX::DisplayFormElement(MWContext *pContext, int iLocation, LO_FormElementStruct *pFormElement)	{
    if (GetDisplayMode() == BLOCK_DISPLAY)
        return;
	else if (GetDisplayMode() == CAPTURE_POSITION)	{
		//	Capture the coordinates of this element.
		Capture(pFormElement->x + pFormElement->x_offset, pFormElement->y + pFormElement->y_offset, pFormElement->width, pFormElement->height);

		//	We don't actually allow display while we are capturing the area.
		return;
	}

	//	Call the base for actual display.
#ifdef XP_WIN32
	if ( m_printBk && !IsPrintPreview()) {
		SubOffscreenPrintDC();
		CDCCX::DisplayFormElement(pContext, iLocation, pFormElement);
		RestorePrintDC();
	}
#endif
	CDCCX::DisplayFormElement(pContext, iLocation, pFormElement);
}

void CPrintCX::DisplayHR(MWContext *pContext, int iLocation, LO_HorizRuleStruct *pHorizRule)	{
    if (GetDisplayMode() == BLOCK_DISPLAY)
        return;
	else if (GetDisplayMode() == CAPTURE_POSITION)	{
        int32 lOrgX, lOrgY;
        
        GetDrawingOrigin(&lOrgX, &lOrgY);
        
		//	Capture the coordinates of this element.
		Capture(pHorizRule->x + pHorizRule->x_offset + lOrgX, pHorizRule->y + pHorizRule->y_offset + lOrgY, pHorizRule->width, pHorizRule->height);

		//	We don't actually allow display while we are capturing the area.
		return;
	}

	//	Call the base for actual display.
#ifdef XP_WIN32
	if ( m_printBk && !IsPrintPreview() ) {
		SubOffscreenPrintDC();
		CDCCX::DisplayHR(pContext, iLocation, pHorizRule);
		RestorePrintDC();
	}
#endif
	CDCCX::DisplayHR(pContext, iLocation, pHorizRule);
}
BOOL CPrintCX::IsDeviceDC() 
{ 
#ifdef XP_WIN32
	if (!m_printBk) return TRUE;
	else {
		if (m_hdcPrint == m_hOrgPrintDC)
			return TRUE;
		else return FALSE;
	}
#else
	return TRUE;
#endif
}

#ifdef XP_WIN32
void CPrintCX::CopyOffscreenBitmap(NI_Pixmap* image, int32 x, int32 y, int32 x_offset, int32 y_offset, int32 width, int32 height, LTRB& Rect)
{
	LTRB sourceRect(Rect);
	ResolveElement(Rect, image, 
                   x_offset * m_lConvertX, 
                   y_offset* m_lConvertY, 
                   x * m_lConvertX, y* m_lConvertY, 
                   width * m_lConvertX, 
                   height* m_lConvertY);
	WORD nBitCount;
	nBitCount = GetBitsPerPixel();
	int	nColorTable;

	// We need to know how big the color table is. For 16-bit mode and 32-bit mode, we need to
	// allocate room for 3 double-word color masks
	if (nBitCount == 16 || nBitCount == 32)
		nColorTable = 3;
	else if (nBitCount < 16)
		nColorTable = 1 << nBitCount;
	else {
		ASSERT(nBitCount == 24);
		nColorTable = 0;
	}
	LPBITMAPINFOHEADER	lpBmi;
	// Allocate space for a BITMAPINFO structure (BITMAPINFOHEADER structure
	// plus space for the color table)

	lpBmi = (LPBITMAPINFOHEADER)calloc(sizeof(BITMAPINFOHEADER) + nColorTable * sizeof(RGBQUAD), 1);
	if (lpBmi) {
		// Initialize the BITMAPINFOHEADER structure
		lpBmi->biSize = sizeof(BITMAPINFOHEADER);
		lpBmi->biWidth = width;
		lpBmi->biHeight = height;
		lpBmi->biPlanes = 1;
		lpBmi->biCompression = BI_RGB;

		CDC *pAttrDC;
		pAttrDC = theApp.m_pMainWnd->GetDC();
		HDC tempDC = ::CreateCompatibleDC(pAttrDC->GetSafeHdc());
		lpBmi->biBitCount = nBitCount;
		// Ask the driver to tell us the number of bits we need to allocate
		BITMAPINFO lpbmi;
		lpbmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		lpbmi.bmiHeader.biWidth = lpBmi->biWidth;
		lpbmi.bmiHeader.biHeight = lpBmi->biHeight;
		lpbmi.bmiHeader.biPlanes = 1;
		lpbmi.bmiHeader.biBitCount = 24;
		lpbmi.bmiHeader.biCompression = BI_RGB;
		lpbmi.bmiHeader.biSizeImage = 0;
		lpbmi.bmiHeader.biXPelsPerMeter = 0;
		lpbmi.bmiHeader.biYPelsPerMeter = 0;

		lpbmi.bmiHeader.biClrUsed = 0;        // default value    
		lpbmi.bmiHeader.biClrImportant = 0;   // all important
		void* ptr;
		HBITMAP tempBmp = ::CreateDIBSection(pAttrDC->GetSafeHdc(),&lpbmi, DIB_RGB_COLORS,  
			&ptr, NULL, NULL);
		HBITMAP	hOldBmp = (HBITMAP)::SelectObject(tempDC, tempBmp);
		::BitBlt(tempDC, 0, 0, width, height, 
							m_hOffscrnDC,
							CASTINT(sourceRect.left),
							CASTINT(sourceRect.top),
     						SRCCOPY);
		(HBITMAP)::SelectObject(tempDC, hOldBmp);
		lpBmi->biSizeImage = ((((lpBmi->biWidth * nBitCount) + 31) & ~31) >> 3) * lpBmi->biHeight;
		// Allocate space for the bits
		LPBYTE lpBits = (LPBYTE)HugeAlloc(lpBmi->biSizeImage, 1);
		::GetDIBits(tempDC, tempBmp, 0, (int)lpBmi->biHeight, lpBits, (LPBITMAPINFO)lpBmi, DIB_RGB_COLORS);

		::StretchDIBits(GetContextDC(),
							CASTINT(Rect.left),
							CASTINT(Rect.top),
							CASTINT(Rect.right - Rect.left),
							CASTINT(Rect.bottom - Rect.top),
							0,
							0,
							CASTINT(width),
							CASTINT(height),
							lpBits,
							(BITMAPINFO*)lpBmi,
							DIB_RGB_COLORS,
     						SRCCOPY);

		::DeleteDC(tempDC);
		::DeleteObject(tempBmp);
		free(lpBits);
		free(lpBmi);
		theApp.m_pMainWnd->ReleaseDC(pAttrDC);
	}
}
#endif

int	 CPrintCX::DisplayPixmap(NI_Pixmap* image, NI_Pixmap* mask, int32 x, int32 y, int32 x_offset, int32 y_offset, int32 width, int32 height, LTRB& Rect)
{
//	LTRB Rect;
    if (GetDisplayMode() == BLOCK_DISPLAY)
        return FALSE;
	else if (GetDisplayMode() == CAPTURE_POSITION)	{
        int32 lOrgX, lOrgY;
        
        GetDrawingOrigin(&lOrgX, &lOrgY);
        
		//	Capture the coordinates of this element.
		Capture(x + (x_offset * m_lConvertX) + lOrgX, y + (y_offset * m_lConvertY) + lOrgY, width * m_lConvertX, height * m_lConvertY);

		//	We don't actually allow display while we are capturing the area.
		return FALSE;
	}
	int iSaveDC = ::SaveDC(m_hdcPrint);
	FEBitmapInfo *imageInfo;
	imageInfo = (FEBitmapInfo*) image->client_data;
#ifdef XP_WIN32
	if ( m_printBk && !IsPrintPreview()) {
		SubOffscreenPrintDC();
		CDCCX::DisplayPixmap(image, mask, 
					x, y, 
					x_offset, y_offset, 
					(imageInfo->width > width) ? imageInfo->width : width, 
					(imageInfo->height > height) ? imageInfo->height : height, 
					Rect);
		RestorePrintDC();
	}

	if (mask && m_printBk && !IsPrintPreview()) {
		CopyOffscreenBitmap(image, x, y, x_offset, y_offset, width, height, Rect);
	}
	else
		CDCCX::DisplayPixmap(image, mask, 
						x, y, 
						x_offset, y_offset, 
						(imageInfo->width > width) ? imageInfo->width : width, 
						(imageInfo->height > height) ? imageInfo->height : height, 
						Rect);
#else
	if (ResolveElement(Rect, image, 
                       (x_offset * m_lConvertX), 
                       (y_offset * m_lConvertY), 
                       x, y, 
                       (width * m_lConvertX), 
                       (height * m_lConvertY))) {
		if (IsPrintPreview())
			CDCCX::DisplayPixmap(image, mask, x, y, x_offset, 
						y_offset, width, height, Rect);
		else {
			StretchPixmap(GetContextDC(), image, 
				Rect.left,
				Rect.top,
				Rect.right - Rect.left,
				Rect.bottom - Rect.top,
				x_offset,
				y_offset, 
				width, 
				height);
		}
	}
#endif
    if(iSaveDC) {
        ::RestoreDC(m_hdcPrint, iSaveDC);
    }
	return TRUE;
}

void CPrintCX::DisplayLineFeed(MWContext *pContext, int iLocation, LO_LinefeedStruct *pLineFeed, XP_Bool iClear)	{
    if (GetDisplayMode() == BLOCK_DISPLAY)
        return;
	else if (GetDisplayMode() == CAPTURE_POSITION)	{
        int32 lOrgX, lOrgY;
        
        GetDrawingOrigin(&lOrgX, &lOrgY);
        
		//	Capture the coordinates of this element.
		Capture(pLineFeed->x + pLineFeed->x_offset + lOrgX, pLineFeed->y + pLineFeed->y_offset + lOrgY, pLineFeed->width, pLineFeed->height);

		//	We don't actually allow display while we are capturing the area.
		return;
	}

	//	Call the base for actual display.
#ifdef XP_WIN32
	if ( m_printBk && !IsPrintPreview()) {
		SubOffscreenPrintDC();
		CDCCX::DisplayLineFeed(pContext, iLocation, pLineFeed, iClear);
		RestorePrintDC();
	}
#endif
	CDCCX::DisplayLineFeed(pContext, iLocation, pLineFeed, iClear);
}

void CPrintCX::DisplaySubDoc(MWContext *pContext, int iLocation, LO_SubDocStruct *pSubDoc)	{
    if (GetDisplayMode() == BLOCK_DISPLAY)
        return;
	else if (GetDisplayMode() == CAPTURE_POSITION)	{
        int32 lOrgX, lOrgY;
        
        GetDrawingOrigin(&lOrgX, &lOrgY);
        
		//	Capture the coordinates of this element.
		Capture(pSubDoc->x + pSubDoc->x_offset + lOrgX, pSubDoc->y + pSubDoc->y_offset + lOrgY, pSubDoc->width, pSubDoc->height);

		//	We don't actually allow display while we are capturing the area.
		return;
	}

	//	Call the base for actual display.
#ifdef XP_WIN32
	if ( m_printBk && !IsPrintPreview()) {
		SubOffscreenPrintDC();
		CDCCX::DisplaySubDoc(pContext, iLocation, pSubDoc);
		RestorePrintDC();
	}
#endif
	CDCCX::DisplaySubDoc(pContext, iLocation, pSubDoc);
}

void CPrintCX::DisplayCell(MWContext *pContext, int iLocation, LO_CellStruct *pCell)	{
    if ((GetDisplayMode() == BLOCK_DISPLAY) ||
        (GetDisplayMode() == CAPTURE_POSITION))	{
        //  Actually, we don't want to capture the area of a cell,
        //      as it is already surrounded by a table.
        //  If we have to cut a table in half, then we will do so
        //      without regard to the table cells.

		//	We don't actually allow display while we are capturing the area.
		return;
	}

	//	Call the base for actual display.
#ifdef XP_WIN32
	if ( m_printBk && !IsPrintPreview()) {
		SubOffscreenPrintDC();
		CDCCX::DisplayCell(pContext, iLocation, pCell);
		RestorePrintDC();
	}
#endif
	CDCCX::DisplayCell(pContext, iLocation, pCell);
}
void CPrintCX::DisplaySubtext(MWContext *pContext, int iLocation, LO_TextStruct *pText, int32 lStartPos, int32 lEndPos, XP_Bool iClear)    {
    if ((GetDisplayMode() == BLOCK_DISPLAY) ||
        (GetDisplayMode() == CAPTURE_POSITION))	{
		//	Never capture the coordinates of this element.
		//	It is only part of a string, and DisplayText will correctly
		//		handle the coordinates.
		//	We don't actually allow display while we are capturing the area.
		return;
	}

	//	Don't need to call the base for actual display.
}

void CPrintCX::DisplayTable(MWContext *pContext, int iLocation, LO_TableStruct *pTable)	{
    if (GetDisplayMode() == BLOCK_DISPLAY)
        return;
	else if (GetDisplayMode() == CAPTURE_POSITION)	{
        int32 lOrgX, lOrgY;
        
        GetDrawingOrigin(&lOrgX, &lOrgY);
        
		//	Capture the coordinates of this element.
		Capture(pTable->x + pTable->x_offset + lOrgX, pTable->y + pTable->y_offset + lOrgY, pTable->width, pTable->height);

		//	We don't actually allow display while we are capturing the area.
		return;
	}

	//	Call the base for actual display.
#ifdef XP_WIN32
	if ( m_printBk && !IsPrintPreview()) {
		SubOffscreenPrintDC();
		CDCCX::DisplayTable(pContext, iLocation, pTable);
		RestorePrintDC();
	}
#endif
	CDCCX::DisplayTable(pContext, iLocation, pTable);
}

void CPrintCX::DisplayText(MWContext *pContext, int iLocation, LO_TextStruct *pText, XP_Bool iClear)   {
    if (GetDisplayMode() == BLOCK_DISPLAY)
        return;
	else if (GetDisplayMode() == CAPTURE_POSITION)	{
        int32 lOrgX, lOrgY;
        
        GetDrawingOrigin(&lOrgX, &lOrgY);
        
		//	Capture the coordinates of this element.
		Capture(pText->x + pText->x_offset + lOrgX, pText->y + pText->y_offset + lOrgY, pText->width, pText->height);

		//	We don't actually allow display while we are capturing the area.
		return;
	}

	//	Call the base for actual display.
#ifdef XP_WIN32
	if ( m_printBk && !IsPrintPreview()) {
		SubOffscreenPrintDC();
		LO_TextAttr* pAttr =  pText->text_attr;
	    CyaFont *pCachedFont = (CyaFont *)pAttr->FE_Data;
		// this is a hack to trick the font engine so the font size
		// will get regenerate again.  Otherwise, we will end up getting
		// the font size for printer.
		pAttr->FE_Data = 0;
		CDCCX::DisplayText(pContext, iLocation, pText, iClear);
		pAttr->FE_Data = pCachedFont;
		RestorePrintDC();
	}
#endif
	CDCCX::DisplayText(pContext, iLocation, pText, iClear);
}

void CPrintCX::Capture(int32 lOrgX, int32 lOrgY, int32 lWidth, int32 lHeight)	{
//	Purpose:	Capture the rectangle in order to layout the document properly.
//	Arguments:	lOrgX	X origin of the element.
//				lOrgY	Y origin of the element.
//				lWidth	The width of the element.
//				lHeight	The height of the lement.
//	Returns:	void
//	Comments:	The origin of the print job must always remain 0,0 for this
//					to work correctly.
//	Revision History:
//		06-09-95	created GAB

	//	Don't capture on no width and height.
	if(lWidth <= 0 || lHeight <= 0)	{
		return;
	}

    //  Don't capture areas we might already have captured
    if (lOrgY < m_lCaptureScrollOffset) {
        return;
    }
    
	//	Allocate a new rectangle to hold the information.
	LTRB *pRect = new LTRB(lOrgX, lOrgY, lOrgX + lWidth, lOrgY + lHeight);

	//	Add the rectangle to the list of elements we are considering.
	m_cplCaptured.AddTail((void *)pRect);
}

void CPrintCX::DisplayWindowlessPlugin(MWContext *pContext, 
                                       LO_EmbedStruct *pEmbed,
                                       NPEmbeddedApp *pEmbeddedApp,
                                       int iLocation)
{
    DisplayPlugin(pContext, pEmbed, pEmbeddedApp, iLocation);
}


// Resolve the dimensions of the plugin object rect and tell the plugin
// to print in that rect using the print HDC.
void CPrintCX::DisplayPlugin(MWContext *pContext, LO_EmbedStruct *pEmbed,
					         NPEmbeddedApp* pEmbeddedApp, int iLocation)
{
    // get the print area and clamp it
    LTRB Rect;
    ResolveElement(Rect, pEmbed, iLocation, NPL_IsEmbedWindowed(pEmbeddedApp));
	SafeSixteen(Rect);

    // set the print area rect
    NPPrint npPrint;
    NPWindow* pWindow = &npPrint.print.embedPrint.window;

	// Initialize struct
	npPrint.mode = NP_EMBED;

    pWindow->window = NULL;
    pWindow->y      = Rect.top;
    pWindow->x      = Rect.left;
    pWindow->width  = Rect.right - Rect.left;
    pWindow->height = Rect.bottom - Rect.top;
	pWindow->type	= NPWindowTypeWindow;

    // get the print HDC
    HDC hdc = GetContextDC();

    //  Save the state of the DC so we can restore it if the Plugin
    //      changes it's state.
    int iSaveDC = ::SaveDC(hdc);

    npPrint.print.embedPrint.platformPrint = (void*)hdc;

    (void)NPL_Print(pEmbeddedApp, &npPrint);

    //  Restore the DC's state.
    if(iSaveDC) {
        ::RestoreDC(hdc, iSaveDC);
    }

    ReleaseContextDC(hdc);
}

//	Do not allow incremental image display.
PRBool CPrintCX::ResolveIncrementalImages()
{
	return(PR_FALSE);
}

//	ReCreate Font for header and footer if possible.
VOID CPrintCX::CreateHeaderFooterFont()
{
    if (m_hFont == NULL || m_iFontCSID != INTL_DocToWinCharSetID(m_iCSID))
	{
        if(m_hFont) {
            ::DeleteObject(m_hFont);
            m_hFont = NULL;
        }
			
        HDC hdc = GetContextDC();
        LOGFONT lf;
		XP_MEMSET(&lf,0,sizeof(LOGFONT));
		lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH ;
		strcpy(lf.lfFaceName, IntlGetUIPropFaceName(m_iCSID));
		lf.lfCharSet = IntlGetLfCharset(m_iCSID);  // get global lfCharset
		long height;
        if (!IsPrintPreview() && ::GetDeviceCaps(hdc, LOGPIXELSY)) {
			height = ( ( 10 * ::GetDeviceCaps(hdc, LOGPIXELSY) ) / 72 );
			height = ( ( ( height * 1000L ) / ::GetDeviceCaps ( hdc, LOGPIXELSY ) ) * 1440L ) / 1000L;
		}
		else {
			height = ( ( 18 * printRes.cx)  / screenRes.cx );
		}
        lf.lfHeight = CASTINT(height);
		lf.lfQuality = PROOF_QUALITY;    
		lf.lfWeight = FW_NORMAL;
		m_hFont = ::CreateFontIndirect ( &lf );
		m_iFontCSID = INTL_DocToWinCharSetID(m_iCSID);
		ReleaseContextDC(hdc);
	}
}

int CPrintCX::PageCount()
{
    int iRetval = 0;
    if(!m_cplPages.IsEmpty())    {
        iRetval = m_cplPages.GetCount();
    }
    return(iRetval);
}

int CPrintCX::LastPagePrinted()
{
    return(m_iLastPagePrinted);
}

void 
CPrintCX::GetDrawingOrigin(int32 *plOrgX, int32 *plOrgY)
{
	if (m_pDrawable) 
		m_pDrawable->GetOrigin(plOrgX, plOrgY);
	else
	    *plOrgX = *plOrgY = 0;
}

FE_Region
CPrintCX::GetDrawingClip()
{
	if (m_pDrawable)
        return m_pDrawable->GetClip();
    else
        return NULL;
}

PrinterDisplayMode CPrintCX::GetDisplayMode() {
    PrinterDisplayMode pdmRetval = m_iDisplayMode;
    if(m_bGlobalBlockDisplay && pdmRetval == DISPLAY && !IsPrintPreview()) {
        //  Tell me if this ever happens, I'd like to study it -- blythe.
        ASSERT(0);
        pdmRetval = BLOCK_DISPLAY;
    }
    return(pdmRetval);
}

