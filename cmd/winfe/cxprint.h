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

#ifndef __PrintContext_H
//	Avoid include redundancy
//
#define __PrintContext_H

//	Purpose:	Provide a context specifically for printing
//	Comments:

//	Required Includes
//
#include "cxdc.h"
#include "cxprndlg.h"
#include "drawable.h"

//	Constants
//

#define POS_CENTER	1
#define POS_LEFT	2
#define POS_RIGHT 	3

#define	POS_HEADER	1
#define	POS_FOOTER	2

typedef enum {
    BLOCK_DISPLAY,
    CAPTURE_POSITION,
    DISPLAY
} PrinterDisplayMode;

//	Structures
//
class CPrintCX : public CDCCX	{
//	Construction, destruction, indirect construction
public:
	CPrintCX(URL_Struct *pUrl, SHIST_SavedData *pSavedData = NULL, char *pDisplayUrl = NULL);
	~CPrintCX();
	virtual void DestroyContext();
	static void PrintAnchorObject(URL_Struct *pUrl, CView *pView, 
                                  SHIST_SavedData *pSavedData = NULL,  char *pDisplayUrl = NULL);
	static void PreviewAnchorObject(CPrintCX *& pPreview, URL_Struct *pUrl, CView *pView, CDC* pDC, 
                                    CPrintInfo *pInfo, SHIST_SavedData *pSavedData = NULL, char *pDisplayUrl = NULL);
    static void AutomatedPrint(const char *pDocument, const char *pPrinter, const char *pDriver, const char *pPort);

//	The anchor we're printing.
protected:
	char *m_pAnchor;
// The actual anchor name to use for Header/Footer
// (In Composer: we may be printing from a temporary local file)
    char *m_pDisplayUrl;

    HFONT m_hFont;
    int m_iFontCSID;
    int m_iMaxWidth;
	int m_offscrnWidth;
	int m_offscrnHeight;

//	CDC Access
private:
	HDC m_hdcPrint;
#ifdef XP_WIN32
	HDC m_hOffscrnDC;
	HDC m_hOrgPrintDC;
	HDC m_hOtherPrintDC;
	HBITMAP m_hOffScrnBitmap;
	HBITMAP m_saveBitmap;
	BOOL m_printBk;
	void SubOffscreenPrintDC() {m_hOrgPrintDC = m_hdcPrint; m_hdcPrint = m_hOffscrnDC;} 
	void RestorePrintDC() { m_hdcPrint = m_hOrgPrintDC;}
#endif
	CDC *m_previewDC;
	lo_SavedEmbedListData* m_embedList;		/* to save the savedData from the URL struct.*/

public:
	virtual HDC GetContextDC(){
		if(IsPrintPreview()) 
			return m_previewDC->GetSafeHdc();
		else    
			return m_hdcPrint;
	}

	virtual BOOL IsDeviceDC();
#ifdef XP_WIN32
	BOOL IsPrintingBackground() {return m_printBk;}
#endif
	virtual HDC GetAttribDC();
	virtual void ReleaseContextDC(HDC pDC);


//	Post Initialization
public:
	virtual void Initialize(BOOL bOwnDC, RECT *pRect = NULL, BOOL bInitialPalette = TRUE, BOOL bNewMemDC = TRUE);
private:
	//	Margins
	int32 m_lLeftMargin;
	int32 m_lTopMargin;
	int32 m_lRightMargin;
	int32 m_lBottomMargin;

	//	True page dimensions
	int32 m_lPageHeight;
	int32 m_lPageWidth;

	// Document dimensions
	int32 m_lDocWidth;
	int32 m_lDocHeight;
    
	//	Color contingencies
	BOOL m_bBlackText;	//	All text as black?
	BOOL m_bBlackLines;	//	All line drawing as black?

	//	Drawing contingencies
	BOOL m_bSolidLines;	//	All lines are solid?
	BOOL m_bBackground;	//	Should we draw the backgrounds?
	BOOL m_bBitmaps;	//	Should we print bitmaps?
    BOOL m_bReverseOrder;   //  Print in reverse order?

	//	Headers/footers
	BOOL m_bNumber;	//	Number the pages?
	BOOL m_bTitle;	//	Give each page the appropriate title?

	CPrinterDrawable *m_pDrawable; // Drawable that represents the printer
    
//	Print status dialog, also the parent of any dialogs we present!
private:
	CPrintStatusDialog *m_pStatusDialog;

//	the real meat of the print process
public:
#ifdef XP_WIN32
	HDC GetOffscreenDC() {return m_hOffscrnDC;}
	virtual int GetLeftMargin() 
	{	if (m_printBk && m_hdcPrint == m_hOffscrnDC) 
				return 0;
		else return m_lLeftMargin;
	}
	virtual int GetTopMargin() 
	{	if (m_printBk && m_hdcPrint == m_hOffscrnDC) 
				return 0;
		else return m_lTopMargin;
	}
	int32  GetXConvertUnit() 
	{ 
		if (m_printBk && (m_hdcPrint == m_hOffscrnDC)) return 1;
		else return m_lConvertX;
	}
	int32  GetYConvertUnit()
	{ 
		if (m_printBk && (m_hdcPrint == m_hOffscrnDC)) return 1;
		else return m_lConvertY;
	}
#endif
	void PrintPage(int iPage, HDC pNewDC = NULL, CPrintInfo *pNewInfo = NULL);
private:
	void CommencePrinting(URL_Struct *pUrl);
	void FormatPages();
	void CapturePositions();
	void Capture(int32 lOrgX, int32 lOrgY, int32 lWidth, int32 lHeight);
	CPtrList m_cplCaptured;
	CPtrList m_cplPages;
	int32 m_lCaptureScrollOffset;

    void CreateHeaderFooterFont();

//  Useful information.
private:
    int m_iLastPagePrinted;
public:
    int PageCount();
    int LastPagePrinted();

private:
    PrinterDisplayMode m_iDisplayMode;
public:
    static BOOL m_bGlobalBlockDisplay;
    PrinterDisplayMode GetDisplayMode();

//	Output overrides.
public:
	virtual void DisplayIcon(int32 x, int32 y, int icon_number);
	BOOL	AdjustRect(LTRB& Rect);	
	virtual BOOL ResolveElement(LTRB& Rect, int32 x, int32 y, int32 x_offset, int32 y_offset,
								int32 width, int32 height);
	virtual BOOL ResolveElement(LTRB& Rect, NI_Pixmap *pImage, int32 lX, int32 lY, 
								int32 orgx, int32 orgy, 
								uint32 ulWidth, uint32 ulHeight,
                                                                int32 lScaleWidth, int32 lScaleHeight);
	virtual BOOL ResolveElement(LTRB& Rect, LO_EmbedStruct *pEmbed, int iLocation, Bool bWindowed);
    virtual BOOL ResolveElement(LTRB& Rect, LO_FormElementStruct *pFormElement);
	virtual COLORREF ResolveTextColor(LO_TextAttr *pAttr);
	virtual COLORREF ResolveBGColor(unsigned uRed, unsigned uGreen, unsigned uBlue);
	virtual BOOL ResolveHRSolid(LO_HorizRuleStruct *pHorizRule);
	virtual BOOL ResolveLineSolid();
	virtual void ResolveTransparentColor(unsigned uRed, unsigned uGreen, unsigned uBlue);
	virtual COLORREF ResolveDarkLineColor();
	virtual COLORREF ResolveLightLineColor();
	virtual COLORREF ResolveBorderColor(LO_TextAttr *pAttr);
	virtual PRBool ResolveIncrementalImages();

	virtual void SetDocDimension(MWContext *pContext, int iLocation, int32 lWidth, int32 lHeight);
	virtual void GetDrawingOrigin(int32 *plOrgX, int32 *plOrgY);
	virtual FE_Region GetDrawingClip();
	virtual void EraseBackground(MWContext *pContext, int iLocation, 
                                     int32 x, int32 y, uint32 width, uint32 height,
                                     LO_Color *pColor);

//	Members needed to properly implement printing.
protected:
	CPrintInfo *m_pcpiPrintJob;
	DOCINFO m_docInfo;
//  document charset id
public:
	int  m_iCSID;
	void* p_TimeOut;

//	Members need to properly implement print preview.
protected:
	CView *m_pPreviewView;
	BOOL m_bPreview;
public:
	BOOL IsPrintPreview() const	{
		return(m_bPreview);
	}

//	Overrides
public:
	//	Dialog owner
	virtual CWnd *GetDialogOwner() const;
	//	Progress messages.
	virtual void Progress(MWContext *pContext, const char *pMessage);
	//	All connections complete.
	virtual void AllConnectionsComplete(MWContext *pContext);
	virtual void FinishedLayout(MWContext *pContext);
	//	Display routines
	virtual void DisplayBullet(MWContext *pContext, int iLocation, LO_BullettStruct *pBullet);
#ifndef MOZ_NGLAYOUT
	virtual void DisplayEmbed(MWContext *pContext, int iLocation, LO_EmbedStruct *pEmbed);
	virtual void DisplayFormElement(MWContext *pContext, int iLocation, LO_FormElementStruct *pFormElement);
#endif
	virtual void DisplayHR(MWContext *pContext, int iLocation, LO_HorizRuleStruct *pHorizRule);
	virtual int DisplayPixmap(NI_Pixmap* image, NI_Pixmap* mask, int32 x, int32 y, int32 x_offset, int32 y_offset, int32 width, int32 height, int32 lScaleWidth, int32 lScaleHeight, LTRB& Rect);
	virtual void DisplayLineFeed(MWContext *pContext, int iLocation, LO_LinefeedStruct *pLineFeed, XP_Bool clear);
	virtual void DisplaySubDoc(MWContext *pContext, int iLocation, LO_SubDocStruct *pSubDoc);
	virtual void DisplayCell(MWContext *pContext, int iLocation, LO_CellStruct *pCell);
	virtual void DisplaySubtext(MWContext *pContext, int iLocation, LO_TextStruct *pText, int32 lStartPos, int32 lEndPos, XP_Bool clear);
	virtual void DisplayTable(MWContext *pContext, int iLocation, LO_TableStruct *pTable);
	virtual void DisplayText(MWContext *pContext, int iLocation, LO_TextStruct *pText, XP_Bool clear);
	virtual void DisplayPlugin(MWContext *pContext, LO_EmbedStruct *pEmbed, NPEmbeddedApp* pEmbeddedApp, int iLocation);
	virtual void DisplayWindowlessPlugin(MWContext *pContext, LO_EmbedStruct *pEmbed, NPEmbeddedApp *pEmbeddedApp, int iLocation);
#ifdef XP_WIN32
	void CopyOffscreenBitmap(NI_Pixmap* image, int32 x, int32 y, int32 x_offset, int32 y_offset, int32 width, int32 height, int32 lScaleWidth, int32 lScaleHeight, LTRB& Rect);
#endif
	//	Layout initialization respecting page size.
	virtual void LayoutNewDocument(MWContext *pContext, URL_Struct *pURL, int32 *pWidth, int32 *pHeight, int32 *pmWidth, int32 *pmHeight);

    virtual void GetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext);

//	Call this to cancel the print job.
public:
	void CancelPrintJob();
private:
	BOOL m_bCancel;
	// this is to prevent the destroycontext get call twice.

	BOOL m_bHandleCancel;	

//	These determine wether or not we need to continue to attempt to start the
//		document, and how we end the printing of the document.
//	Doesn't apply to preview.
private:
	BOOL m_bAbort;
	BOOL m_bNeedStartDoc;
	BOOL m_bAllConnectionCompleteCalled;
        BOOL m_bFormatStarted;
    BOOL m_bFinishedLayoutCalled;
    SIZE screenRes;
	SIZE printRes;
	int StartDoc();
	void PrintTextAllign ( HDC pDC, char * szBuffer, int position, int hpos );
	void ScreenToPrint(POINT* point, int num = 1);
    void FormatAndPrintPages(MWContext *context);
};

#endif // __PrintContext_H
