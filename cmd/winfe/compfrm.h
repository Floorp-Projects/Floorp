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

/* COMPFRM.H - header file for the ComposeFrame class. 
 */
#ifndef __COMPFRM_H
#define __COMPFRM_H

#include "msgcom.h"
#include "mainfrm.h"
#include "edframe.h"
#include "compmisc.h"
#include "apiaddr.h"
#include "statbar.h"

// Message used to synchronize setting the initial keyboard focus in the
// compose window. This message is posted to the compose window by 
// FE_CreateCompositionPane just before returning.
#define WM_COMP_SET_INITIAL_FOCUS   WM_TOOLCONTROLLER + 1

#define ID_ENCRYPTED            1011
#define ID_SIGNED               1012

// rhp - Flags for MAPI operations...
#define MAPI_IGNORE             0
#define MAPI_SEND               1
#define MAPI_SAVE               2


// Forward declarations

class CNSAddressList;           // address list widget
class CEditToolBarController;   // HTML toolbar controlling object
class CComposeBar;              // embedded address/attachment area

// CComposeFrame class declaration

class CComposeFrame : public CGenericFrame
{
	DECLARE_DYNCREATE(CComposeFrame)
    
protected:
	CComposeFrame();                // protected constructor used by dynamic creation
   ~CComposeFrame(); 

 	 MSG_Pane *m_pComposePane;       // Backend supplied pane context
	CNetscapeStatusBar m_barStatus;
    CComposeBar * m_pComposeBar;    // address/attachment block widget
    BOOL m_bUseHtml;
    BOOL m_bInitialized;
    CEditToolBarController * m_pToolBarController;

    // these are used for plain text
    CComposeEdit m_Editor;          // regular text editor - to be replaced by Gold editor
	int32 m_quoteSel;					// current plain text qutoing position 
    CBlankWnd m_EditorParent;       // controls resizing the edit control (going away when Gold integrated)

    CWnd * m_pFocus;                // field which has focus in control
	HFONT m_cfTextFont;             // font to use in the edit control
    MSG_HEADER_SET m_SavedHeaders;  // hackery to redraw headers once we become visible and

    MWContext * m_pOldContext;
    MSG_CompositionFields * m_pFields;
	 char *m_pInitialText;                   // initial text
    BOOL m_bWrapLongLines;
    int m_cxChar;

    int  m_bMAPISendMode;        // rhp - for MAPI Send Operations

public:
    // data access functions 
    inline int GetCharWidth() { return m_cxChar; }
    inline BOOL GetWrapLongLines() { return m_bWrapLongLines; }
    inline void SetComposeStuff(MWContext *pOld, MSG_CompositionFields * pFields)
	    { m_pOldContext = pOld; m_pFields = pFields; }
    inline BOOL UseHtml(void)                       { return m_bUseHtml; }
    inline CWnd * GetFocusField(void)               { return m_pFocus; }
    inline void SetFocusField(CWnd * pwnd = NULL)   { m_pFocus = pwnd; }
    inline MSG_Pane * GetMsgPane(void)              { return m_pComposePane; }
    inline void SetToolBarController(CEditToolBarController * pController = NULL) {
    	m_pToolBarController = pController;
	 }    
    inline CEditToolBarController * GetToolBarController(void) {
    	return m_pToolBarController;
	 }
    inline CComposeEdit * GetEditor(void)           { return &m_Editor; }
	inline int32 GetQuoteSel(void)					{ return m_quoteSel; }
	inline void SetQuoteSel(int32 sel)				{ m_quoteSel = sel; }
    inline CBlankWnd * GetEditorParent(void)        { return &m_EditorParent; }
    inline CComposeBar * GetComposeBar(void)        { return m_pComposeBar; }
    inline void SetComposeBar(CComposeBar *pBar = NULL) { m_pComposeBar = pBar; }
    inline void SetSavedHeaders(MSG_HEADER_SET headers) { m_SavedHeaders = headers; }
    inline MSG_HEADER_SET GetSavedHeaders(void)     { return m_SavedHeaders; }
    inline BOOL Initialized(void)                   { return m_bInitialized; }
    
    LPADDRESSCONTROL GetAddressWidgetInterface();
    
    // public interface
	void SetQuoteSelection(void);
    void SetMsgPane(MSG_Pane * pPane = NULL);
    void SetModified(BOOL bvalue);
    void CompleteComposeInitialization(void);       // gold specific stuff
    void GoldDoneLoading(void);
    void InsertInitialText(void);
	void SetInitialText(const char *pText);
	inline const char *GetInitialText() { return m_pInitialText; };
    virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra,
	   AFX_CMDHANDLERINFO* pHandlerInfo);

#ifdef XP_WIN32
	virtual int16 GetTitleWinCSID();	// jliu
#endif

    BOOL AppendAddress(MSG_HEADER_SET header, const char * value);
    void SetType(MWContextType type);
	void SetCSID(int16 iCSID);
    void UpdateToolBar(void);
	BOOL CreateEditBars();
    void DisplayHeaders ( MSG_HEADER_SET );
	CWnd * GetEditorWnd();
    void UpdateAttachmentInfo(void);
    void SetHtmlMode(BOOL bMode = FALSE);
    void UpdateSecurityOptions(void);
    BOOL BccOnly(void);
    
	virtual void RefreshNewEncoding(int16 doc_csid, BOOL bSave = TRUE);

    // rhp - For MAPI Operations!
  inline void SetMAPISendMode(int bSendMode) { m_bMAPISendMode = bSendMode; };        // rhp - for MAPI
  inline int  GetMAPISendMode(void) { return m_bMAPISendMode; }; // rhp - for MAPI
  void        UpdateComposeWindowForMAPI(void);      // rhp - for MAPISendMail()

	// Overriden for setting the command help for editor plugin tools and edit history list
	virtual void GetMessageString(UINT MenuID, CString& Message) const;  

protected:
	virtual BOOL PreTranslateMessage( MSG* pMsg );
    virtual BOOL PreCreateWindow ( CREATESTRUCT & );
    virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext *pContext);
    virtual BOOL CanCloseFrame(void);
    
    void OnUpdateThis ( CCmdUI* pCmdUI, MSG_CommandType tMenuType );
    void MessageCommand( MSG_CommandType );
    void ConvertToPlainText();
    void ConvertToHtml();
	void DoSend( BOOL bNow = TRUE );
    int CreateHtmlToolbars();
    void CreatePlainTextEditor();
    void ShowHtmlToolbars();
    void DestroyHtmlToolbars();
    void SetEditorParent(CWnd*);
    char * PromptMessageSubject();
	void MakeComposeBarVisible();
   
    afx_msg void OnNew();
    afx_msg void OnConvert();
    afx_msg void OnAttachMyCard();
    afx_msg void OnUpdateAttachMyCard(CCmdUI * pCmdUI);
    afx_msg void OnUpdateConvert(CCmdUI*pCmdUI);
    afx_msg void OnUpdateButtonGeneral(CCmdUI* pCmdUI);
	 afx_msg LONG OnToolController(UINT,LONG);
    afx_msg void OnPasteQuote( void );
    afx_msg void OnUpdatePasteQuote(CCmdUI *);
    afx_msg void OnSelectAll(void);
    afx_msg void OnUpdateSelectAll(CCmdUI *);
    afx_msg void OnButtonTo(void);
    afx_msg void OnQuoteOriginal ( void );
    afx_msg void OnSaveAs( void );
    afx_msg void OnSaveDraft(void);
	afx_msg void OnSaveTemplate(void);
    afx_msg void OnAttachFile(void);
    afx_msg void OnCheckSpelling(void);
    afx_msg void OnUpdateCheckSpelling(CCmdUI * pCmdUI);
    afx_msg void OnDoneGoingOffline(void);
    afx_msg void OnUpdateSaveDraft( CCmdUI * pCmdUI );
	afx_msg void OnUpdateSaveTemplate( CCmdUI * pCmdUI );

    afx_msg void OnSend(void);
    afx_msg void OnSendNow(void);
    afx_msg void OnSendLater(void);
    afx_msg void OnUpdateSend(CCmdUI * pCmdUI);
    afx_msg void OnUpdateSendNow(CCmdUI * pCmdUI);
    afx_msg void OnUpdateSendLater(CCmdUI * pCmdUI);
    afx_msg void OnUpdateAttach ( CCmdUI * pCmdUI );
    afx_msg void OnShowSecurityAdvisor ();
    afx_msg void OnSetFocus(CWnd *);

    afx_msg int OnCreate ( LPCREATESTRUCT );
    afx_msg void OnClose ( void );
    afx_msg void OnDestroy(void);
    afx_msg void OnAttachUrl(void);
	 afx_msg void OnSelectAddresses(void);
    afx_msg void OnWrapLongLines(void);
    afx_msg void OnUpdateWrapLongLines(CCmdUI*pCmdUI);

    afx_msg void OnViewAddresses();
    afx_msg void OnViewAttachments();
    afx_msg void OnViewOptions();
    afx_msg void OnUpdateViewAddresses(CCmdUI * pCmdUI);
    afx_msg void OnUpdateViewAttachments(CCmdUI * pCmdUI);
    afx_msg void OnUpdateViewOptions(CCmdUI * pCmdUI);
    
    afx_msg void OnToggleMessageToolbar();
    afx_msg void OnUpdateToggleMessageToolbar(CCmdUI *pCmdUI);
    afx_msg void OnToggleAddressArea();
    afx_msg void OnUpdateToggleAddressArea(CCmdUI * pCmdUI);
    afx_msg void OnSecurity();

    afx_msg void OnAttachTab(void);
    afx_msg void OnAddressTab(void);
    afx_msg void OnOptionsTab(void);
    afx_msg void OnCollapse(void);

	afx_msg void OnUpdateSecurity(CCmdUI *pCmdUI);
	afx_msg void OnUpdateSecureStatus(CCmdUI *pCmdUI);
    afx_msg LONG OnSetInitialFocus(WPARAM wParam, LPARAM lParam);
    LRESULT OnButtonMenuOpen(WPARAM wParam, LPARAM lParam);

    afx_msg LRESULT OnFindReplace(WPARAM wParam, LPARAM lParam) ;
 	afx_msg LRESULT OnSetMessageString(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()

};

/////////////////////////////////////////////////////////////////////////////
#endif
