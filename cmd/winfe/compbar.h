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
#ifndef __COMPBAR_H
#define __COMPBAR_H

#include <afxwin.h>
#include <afxext.h>
#include "rosetta.h"
#include "compstd.h"
#include "msgcom.h"
#include "addrbook.h"
#include "apiaddr.h"
#include "apiimg.h"
#include "collapse.h"
#include "tooltip.h"

class CComposeEdit;
class CNSAddressList;
class CNSAttachmentList;

#define MAX_TIPS    4

class CComposeSubjectEdit;

class CComposeBar : public CDialogBar,
		    public IAddressParent
{
protected:
    class CNSComposeToolInfo 
    {
    public:
   	CRect m_rect;
    UINT m_idText;
   	WPARAM m_idCommand;
    CString m_csToolTip;
   	void Initialize(UINT idText, CRect & rect, WPARAM idCommand)
   	{
   	    m_csToolTip = szLoadString(idText);
        m_idText = idText;
   	    m_rect = rect;
   	    m_idCommand = idCommand;
   	}
    };
    LPUNKNOWN m_pUnkImage;
    LPUNKNOWN m_pUnkAddressControl;
    LPIMAGEMAP m_pIImage;
public:
	CNSToolTip2  * m_pToolTip;
    CNSCollapser collapser;
    LPADDRESSCONTROL m_pIAddressList;
    LPUNKNOWN m_pUnkAddress;
    int m_iMinSize;
    int m_iMaxSize;
    int m_iHeight;
    int m_iBoxHeight;
    int m_cxChar;
    int m_iFirstX;
    int m_iSelectedTab;
    int m_iPriorityIdx;
    char * m_pszMessageFormat;
    char * m_pszCharSet;
    BOOL m_bReceipt;
    BOOL m_bEncrypted;
    BOOL m_bSigned;
    BOOL m_bAttachVCard;
    BOOL m_bUse8Bit;
    BOOL m_bUseUUENCODE;
    int m_iTotalAttachments;
    int m_iPrevHeight;
    CNSComposeToolInfo m_ToolTipInfo[MAX_TIPS];
    HFONT m_cfTextFont;
    HFONT m_cfStaticFont;
    HFONT m_cfSubjectFont;
    CComposeEdit * m_pComposeEdit;
#ifdef BUTTONS
    CWnd * m_pButton[3];
#endif
    CButton * m_pReturnReceipt;
    CButton * m_pEncrypted;
    CButton * m_pSigned;
    CButton * m_pUse8Bit;
    CButton * m_pUseUUENCODE;

    CNSAttachmentList * m_pAttachmentList;
    CWnd * m_pWidget;

    CComposeSubjectEdit * m_pSubjectEdit;
    CStatic * m_pPriorityText;
    CStatic * m_pSubjectEditText;
    CStatic * m_pMessageFormatText;
    CComboBox * m_pPriority;
    CComboBox * m_pMessageFormat;
    CNSAttachDropTarget * m_pDropTarget;

	MWContext *m_pContext;

    virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz );
    void Enable3d(BOOL bEnable);
   
public:

    CEdit * m_pSubject;
    BOOL m_bClosed;
    BOOL m_bHidden;
    BOOL m_bSizing;
    BOOL m_bCanSize;
    int  m_iY;
    
    CComposeBar (MWContext *pContext );
    ~CComposeBar ( );
    
    LPADDRESSCONTROL GetAddressWidgetInterface();

    void ShowTab(int idx);
    int GetTab();
    void AttachFile(void);
    void AttachUrl(void);
    inline BOOL IsCollapsed(void) { return m_bClosed; }

    void SetComposeEdit ( CComposeEdit * pEdit )
    {
    	m_pComposeEdit = pEdit;
    }
    CComposeEdit * GetComposeEdit ( void )
    {
    	return m_pComposeEdit;
    }
	void SetCSID(int m_iCSID);
    HG29792
	void SetReturnReceipt(BOOL bReceipt) { m_bReceipt = bReceipt; }
	void SetUse8Bit(BOOL bUse8Bit) { m_bUse8Bit = bUse8Bit; }
	void SetUseUUENCODE(BOOL bUseUUENCODE) {m_bUseUUENCODE = bUseUUENCODE; }
	BOOL GetReturnReceipt(void) { return m_bReceipt; }
	BOOL GetUse8Bit(void) { return m_bUse8Bit; }
	BOOL GetUseUUENCODE(void) { return m_bUseUUENCODE; }
    void Draw3DStaticEdgeSimulation(CDC & dc, CRect &rect, BOOL bReverse = FALSE);
    void DrawVerticalTab(CDC &, int, CRect &);
    BOOL IsAttachmentsMailOnly(void);
	BOOL TabControl(BOOL bShift = FALSE, BOOL bControl = FALSE, CWnd * pWnd = (CWnd*)TABCTRL_HOME);
    void CalcFieldLayout(void);
    void DisplayHeaders ( MSG_HEADER_SET );
    int GetHeightNeeded ( void );
    void CreateAddressingBlock(void);
    void CreateStandardFields(void);
    void CreateAddressPage(void);
    void CreateAttachmentsPage(void);
    void CreateOptionsPage(void);
    void DestroyOptionsPage(void);
    void DestroyAddressPage(void);
    void DestroyStandardFields(void);
    void DestroyAttachmentsPage(void);
    void UpdateFixedSize ( );
    void UpdateHeaderInfo ( void );
	void UpdateRecipientInfo ( char *pTo, char *pCc, char *pBcc );
    int  GetTotalAttachments(void);
    void UpdateAttachmentInfo(int nTotal = -1);
    void TabChanging(int tab);
    void TabChanged(int tab);
	void GetWidgetRect(CRect &WinRect, CRect &rect);
    BOOL GetAttachMyCard() { return m_bAttachVCard; }
    void SetAttachMyCard(BOOL bAttach) { m_bAttachVCard = bAttach; }
    void Cleanup(void);
    void UpdateSecurityOptions(void);

    virtual void AddedItem (HWND hwnd, LONG id,int index);
    virtual int	 ChangedItem (char * pString, int index, HWND hwnd, char ** ppszFullName, unsigned long* entryID = NULL, UINT* bitmapID = NULL);
    virtual void DeletedItem (HWND hwnd, LONG id,int index);
    virtual char * NameCompletion (char *);
	  virtual void StartNameCompletionSearch();
	  virtual void StopNameCompletionSearch();
	  virtual void SetProgressBarPercent(int32 lPercent);
	  virtual void SetStatusText(const char* pMessage	);
	  virtual CWnd *GetOwnerWindow();

    virtual int OnToolHitTest( CPoint point, TOOLINFO* pTI ) const;
    BOOL ProcessVCardData(COleDataObject * pDataObject,CPoint &point);
    BOOL ProcessAddressBookIndexFormat(COleDataObject *pDataObject, DROPEFFECT effect,
									   CPoint &point);
    BOOL AddURLToAddressPane(COleDataObject * pDataObject, CPoint &point, LPSTR szURL);

    void OnAttachTab(void);
    void OnAddressTab(void);
    void OnOptionsTab(void);
    void OnCollapse(void);
    void OnToggleShow(void);
    inline BOOL IsVisible() {return !m_bHidden;}

protected:
#ifdef XP_WIN16
    BOOL PreTranslateMessage( MSG* pMsg );
#endif
    void UpdateOptionsInfo();
  
    afx_msg LRESULT OnSizeParent(WPARAM wParam, LPARAM lParam);
    afx_msg void    OnDropFiles( HDROP hDropInfo );
    afx_msg void OnMouseMove( UINT nFlags, CPoint point );
    afx_msg int  OnCreate ( LPCREATESTRUCT );
    afx_msg void OnPaint();
    afx_msg BOOL OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message );
    afx_msg void OnButtonAttach(void);
    afx_msg void OnSize( UINT nType, int cx, int cy );
    afx_msg LONG OnLeavingLastField(UINT, LONG);
    afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
    afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnTimer( UINT  nIDEvent );
    afx_msg void OnUpdateToolBar(void);
    afx_msg void OnUpdateOptions(void);

    DECLARE_MESSAGE_MAP()

    friend class CComposeFrame;

};

#endif

