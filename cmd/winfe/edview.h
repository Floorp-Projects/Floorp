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

// edview.h : interface of the CNetscapeEditView class
//
/////////////////////////////////////////////////////////////////////////////
#ifdef EDITOR

#ifndef EDVIEW_H
#define EDVIEW_H

#include "pa_tags.h"   // Needed for P_MAX
#include "edttypes.h"
#include "eddialog.h"
#include "netsvw.h"

#include "libcnv.h"//for convert image result defs

#define ED_TB_BUTTON_WIDTH   25 // Window's "Standard" = 24
#define ED_TB_BUTTON_HEIGHT  23 //   "                   22
#define ED_TB_BITMAP_WIDTH   18
#define ED_TB_BITMAP_HEIGHT  16

// Get either the topmost frame or the active dialog above it
// Use as parent for all new dialogs to get correct focus layering
#define GET_DLG_PARENT(pView) (pView->GetFrame()->GetFrameWnd()->GetLastActivePopup())

///////////////////////////////////////////////////////////////////

struct CCaret {
    BOOL bEnabled;      // caret has been created.
    BOOL cShown;        // current semaphore count of ShowCarets
    int width;          // width of caret
    int height;         // height of caret
    int x;              // current x position
    int y;              // current y position
    CCaret(): bEnabled(0), cShown(0){}
};

typedef struct _ED_FORMATSTATE {
    TagType  nParagraphFormat;           // Only 1 para style at a time
    int      iFontSize;                  // 1 - 7 mapped onto -2 to +4
    int      iFontIndex;                 // Index to font name in combo box
    COLORREF crFontColor;
	BOOL	 bParaFormatMaybeChanged;	 // Flags for efficient updating of combo boxes
    BOOL     bFontSizeMaybeChanged;
    BOOL     bFontColorMaybeChanged;
    BOOL     bFontFaceMaybeChanged;
} ED_FORMATSTATE, *LPED_FORMATSTATE;

/////////////////////////////////////////////////////////////////////////////
// 
class CEditViewDropTarget : public COleDropTarget
{
public:
//Construction
    CEditViewDropTarget();
    CPoint      m_cLastPoint;
    DWORD       m_dwLastKeyState;
    DROPEFFECT  m_LastDropEffect;
    UINT        m_nDragType;    
    BOOL        OnDrop(CWnd *, COleDataObject *, DROPEFFECT, CPoint);
    DROPEFFECT  OnDragEnter(CWnd *, COleDataObject *, DWORD, CPoint);
    DROPEFFECT  OnDragOver(CWnd *, COleDataObject *, DWORD, CPoint);
    void        OnDragLeave(CWnd *);
private:
    int m_nIsOKDrop;
};
class CImagePage;
class CDocColorPage;

////////////////////////////////////////////////////////////////////////////////
// Edit Document view
// All stuff specific to editing the main document viewing area
//
class CNetscapeEditView : public CNetscapeView
{
public:
    // Klutzy method to tell these dialog pages
    //   when the image they inserted is finished
    //   and what is its filename. (Needed for Apply button)
    CImagePage*      m_pImagePage;
    CDocColorPage*   m_pColorPage;
    //
    static UINT m_converrmsg[NUM_CONVERR];

    // Needed to process Ctrl+equals key
    //  and process Font Face on the menu
    BOOL PreTranslateMessage(MSG *pMsg);
    
    // Note: Separate class than CViewDropTarget used by CNetscapeView
    CEditViewDropTarget * m_pEditDropTarget;

    // Overridden to handle dynamic plugin menus and menus with ranges.
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

	// returns editor plugin info for a speficied plugin menu id. (TRUE if sucessful)
    BOOL GetPluginInfo(UINT MenuId, uint32 *CategoryId, uint32 *PluginId);

    // Global for simple access by EdtSaveToTempCallback:
    // EdtSaveToTempCallback puts temp filename here
    char   *m_pTempFilename;  

protected: // create from serialization only
    DECLARE_DYNCREATE(CNetscapeEditView)
    CNetscapeEditView();
    ~CNetscapeEditView();

	// Editor plugin support.
    void AddPluginMenus();					// initialize the plugin menus
    struct _PluginInfo  *m_pPluginInfo;		// list of available plugins
    UINT            m_NumPlugins;			// number of evailable plugins

// Attributes
private:
	// Store previous format states to avoid unneccessary updating
    ED_FORMATSTATE   m_EditState;

	// These should be retrieved from ColorPreferences in INI file
	COLORREF         m_crFontPalette[16];
    int              m_nFontColorCount;
    COLORREF         m_crDefColor;   
    
    // Flag to undo our auto-selecting of object on right mouse button popup menu
    BOOL             m_bAutoSelectObject;
    CEditorResourceDll m_resourcedll;
    
#ifdef _IME_COMPOSITION
    BOOL m_imebool;
    IIMEDll *m_pime;
    BOOL initializeIME();
    int findDifference(const char *p_str1,const char *p_str2,MWContext *); //-1==no difference
    ED_BufferOffset m_imeoffset;
    ED_BufferOffset m_imelength;
    CString m_oldstring;
    EDT_CharacterData *m_pchardata;
    DWORD m_imeoldcursorpos; //we need this to tell if someone hit 'ESC' or SHIFT<- or SHIFT ->
#ifdef XP_WIN32 
        int16 m_csid;//used for char set from language change
        BOOL m_utf8; //boolean to check encoding for this document.
        BOOL ImeSetFont(HWND p_hwnd,LOGFONT *p_plogfont);
#else //XP_WIN16
        void ImeCreate(HWND hWnd);
        void ImeDestroy();
        void ImeMoveConvertWin(HWND hWnd,int x,int y);
        HFONT ImeSetFont(HWND hWnd,HFONT hFont);
        void OnImeStartComposition();
        void OnImeEndComposition();
        LRESULT OnImeChangeComposition(HGLOBAL p_global);
        LRESULT insertStringEx(HGLOBAL p_global);
        LONG    m_lIMEParam;
        HGLOBAL m_hIME;
#endif//XP_WI32 else XP_WIN16
#endif //_IME_COMPOSITION

public:
    // Set TRUE when image is being loaded so we can
    //   pop-up dialog for canceling
    UINT               m_nLoadingImageCount;
    CTime              m_ctStartLoadingImageTime;
    CLoadingImageDlg*  m_pLoadingImageDlg;	

    // The History struct of the orginal HTTP location
    //   of a document saved to local disk for editing
    CString  m_csSourceUrl;
    BOOL     m_bSaveRemoteToLocal;

protected:
    ED_FileError       m_FileSaveStatus;

// Implementation
public:
	// Called by layout whenever caret position changes
	//   and whenever any formating change takes place
	void SetEditChanged();

    // Replaced by FE_CheckAndSaveDocument() and FE_SaveDocument()
    //  for easier access in different contexts
    //  (don't need to know about CNetscapeView class)
    //
    // Checks if any changes made and prompts user to save
    // Return FALSE only if user cancels out of dialog
    // BOOL CheckAndSaveDocument();
    
    // Checks for new doc or remote editing and prompts
    //  to save. Return FALSE only if user cancels out of dialog
    // Use bSaveNewDocument = FALSE to not force saving of a new document
    // BOOL SaveNonLocalDocument(BOOL bSaveNewDocument = TRUE);
    
    // Thes return FALSE only ifuser CANCELS out of dialog:
    BOOL SaveDocument();
    
    // Params are usually preferences, but may be overriden at time of saving
    BOOL SaveDocumentAs(BOOL bKeepImagesWithDoc, BOOL bAutoAdjustLinks );

    // Save current document as a temp file.
    // Returns temp Filename or NULL if there's any errors.
    // This is synchronous - waits for temp file to be saved before returning
    char* SaveToTempFile();
    char* GetTempFilename() { return m_pTempFilename;}

    // Uses to save current doc to temp file if there are unsaved changes,
    //     then converts that to text file output using Browser's CSaveCX system.
    void SaveDocumentAsText(char * pFilename);

    // Save a remote document back to its original location
    BOOL SaveRemote();

    void OnImagePropDlg();

    int OnTableProperties(int iStartPage);

    // Asynchronous file save result is placed here when done.
    void SetFileSaveStatus(ED_FileError status) { m_FileSaveStatus = status; }

    // Central processing for most property dialogs
    // If Start page = -1 it is deduced from edit context
    // If nIDFirstTab = 0, page is deducd from selected object: Image or Character props are choices
    void DoProperties(int iStartPage = -1, UINT nIDFirstTab = 0);
    void ShowCaret() { if (m_pChild == NULL) ::ShowCaret(m_hWnd); }

    // Called from OnCMD, converts nID to appropriate point size
    //  for SetPointSize()
    void OnPointSize(UINT nID);
    void SetPointSize(int iPoints);
#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

    // Call this from CMainFrame::OnDropFiles to accept
    //   dropped files, use bGetDropPoint to get mouse point
    //   and position cursor.
    //   If FALSE, it drops at current caret location.
    void DropFiles( HDROP hDropInfo, BOOL bGetDropPoint = FALSE );
    
    // Common Paste handler for Clipboard or DragNdrop
    BOOL DoPasteItem(COleDataObject* pDataObject, CPoint* pPoint, BOOL bDeleteSource );

    // Caret-related stuff
    CCaret m_caret;

    // Set when someone is dragging over us
    BOOL   m_bDragOver;
    CRect  m_crLastDragRect;

    // Same calculation as CWinCX::ResolvePoint()
    void ClientToDocXY( CPoint& cPoint, int32* pX, int32* pY );
    // Checks file extension for .gif, *.jpg etc.
    BOOL CanSupportImageFile(const char * pFilename);

    // Helper for OnUpdateCommandUI messages for list items
    void UpdateListMenuItem(CCmdUI* pCmdUI, TagType t);

    // Called after changing Font Size mode to rebuild font size combo
    //  and update the value it shows
    void UpdateFontSizeCombo();

    // Delete existing menu starting at iStartItem then
    //  append a new menu with current recently-edited URLs
    void BuildEditHistoryMenu(HMENU hMenu, int iStartItem);
    
    // Trigger a mouse-move message to update the cursor
    void UpdateCursor();

private:
    BOOL OnLeftKey(BOOL bShift, BOOL bControl);
    BOOL OnRightKey(BOOL bShift, BOOL bControl);
    BOOL OnUpKey(BOOL bShift, BOOL bControl);
    BOOL OnDownKey(BOOL bShift, BOOL bControl);
    BOOL OnEndKey(BOOL bShift, BOOL bControl);
    BOOL OnHomeKey(BOOL bShift, BOOL bControl);
    BOOL OnInsertKey(BOOL bShift, BOOL bControl);

// Generated message map functions
protected:
    //{{AFX_MSG(CNetscapeEditView)
    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSetFocus(CWnd *);
    afx_msg void OnKillFocus(CWnd *pOldWin);
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nflags);
    afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnSelendokParagraphCombo();
	afx_msg void OnSelendokFontFaceCombo();
    afx_msg void OnSetLocalFontFace();
    afx_msg void OnRemoveFontFace();
    afx_msg void OnCancelComboBox();
    afx_msg void OnIncreaseFontSize();
    afx_msg void OnDecreaseFontSize();
    afx_msg void OnUpdateIncreaseFontSize(CCmdUI* pCmdUI);
    afx_msg void OnUpdateDecreaseFontSize(CCmdUI* pCmdUI);
    afx_msg void OnUpdateFontSizeMenu(CCmdUI* pCmdUI);
    afx_msg void OnSelendokFontSizeCombo();
    afx_msg void OnUpdateParagraphComboBox(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFontFaceComboBox(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFontSizeComboBox(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFontColorComboBox(CCmdUI* pCmdUI);
    afx_msg void OnSetFocusParagraphStyle();
    afx_msg void OnSetFocusFontFace();
    afx_msg void OnSetFocusFontSize();
    afx_msg void OnGetFontColor();
    afx_msg void OnOpenNavigatorWindow();
   	afx_msg void OnFileSaveAs();
   	afx_msg void OnFileSave();
   	afx_msg void OnEditFindReplace();
    afx_msg void HaveEditContext(CCmdUI* pCmdUI);
    afx_msg void IsNotSelected(CCmdUI* pCmdUI);
    afx_msg void OnMakeLink();
    afx_msg void OnLinkProperties();
    afx_msg void OnUpdateLinkProperties(CCmdUI* pCmdUI);
    afx_msg void OnInsertLink();
    afx_msg void OnUpdateInsertLink(CCmdUI* pCmdUI);
    afx_msg void OnRemoveLinks();
    afx_msg void OnUpdateRemoveLinks(CCmdUI* pCmdUI);
    afx_msg void OnInsertTarget();
    afx_msg void OnTargetProperties();
    afx_msg void OnUpdateTargetProperties(CCmdUI* pCmdUI);
    afx_msg void OnInsertImage();
    afx_msg void OnInsertHRule();
    afx_msg void OnInsertTag();
    afx_msg void OnTagProperties();
    afx_msg void OnUpdateTagProperties(CCmdUI* pCmdUI);
    afx_msg void OnInsertNonbreakingSpace();
    afx_msg void OnImageProperties();
    afx_msg void OnUpdateImageProperties(CCmdUI* pCmdUI);
    afx_msg void OnHRuleProperties();
    afx_msg void OnUpdateHRuleProperties(CCmdUI* pCmdUI);
    afx_msg void OnTextProperties();
    afx_msg void OnParagraphProperties();
    afx_msg void OnCharacterProperties();
    afx_msg void OnDocumentProperties();
    afx_msg void OnDocColorProperties();
    afx_msg void OnCharacterNoTextStyles();
    afx_msg void OnCharacterNone();
    afx_msg void OnCharacterFixedWidth();
    afx_msg void OnFormatIndent();
    afx_msg void OnFormatOutdent();
    afx_msg void OnAlignPopup();
    afx_msg void OnAlignLeft();
    afx_msg void OnUpdateAlignLeft(CCmdUI* pCmdUI);
    afx_msg void OnAlignRight();
    afx_msg void OnAlignCenter();
    afx_msg void OnLocalProperties();
    afx_msg void OnUpdatePropsLocal(CCmdUI* pCmdUI);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnInsertLineBreak();
    afx_msg void OnInsertBreakLeft();
    afx_msg void OnInsertBreakRight();
    afx_msg void OnInsertBreakBoth();
    afx_msg void OnUpdateInsertBreak(CCmdUI* pCmdUI);
    afx_msg void OnRemoveList();
    afx_msg void OnUnumList();
    afx_msg void OnNumList();
    afx_msg void OnBlockQuote();
    afx_msg void OnUpdateRemoveList(CCmdUI* pCmdUI);
    afx_msg void OnUpdateUnumList(CCmdUI* pCmdUI);
    afx_msg void OnUpdateNumList(CCmdUI* pCmdUI);
    afx_msg void OnSelectAll();
    afx_msg void OnSelectTable();
    afx_msg void OnSelectTableRow();
    afx_msg void OnSelectTableColumn();
    afx_msg void OnSelectTableCell();
    afx_msg void OnSelectTableAllCells();
    afx_msg void OnUpdateInTable(CCmdUI* pCmdUI);
    afx_msg void OnMergeTableCells();
    afx_msg void OnSplitTableCell();
    afx_msg void OnUpdateMergeTableCells(CCmdUI* pCmdUI);
    afx_msg void OnUpdateSplitTableCell(CCmdUI* pCmdUI);
    afx_msg void OnTableTextConvert();
    afx_msg void OnUpdateTableTextConvert(CCmdUI* pCmdUI);
    afx_msg void OnUndo();
    afx_msg void OnUpdateUndo(CCmdUI* pCmdUI);
    afx_msg void OnPublish();
    afx_msg void OnDisplayParagraphMarks();
    afx_msg void OnUpdateDisplayParagraphMarks(CCmdUI* pCmdUI);
    afx_msg void OnInsertObjectPopup();
    afx_msg void OnAlignTableLeft();
    afx_msg void OnAlignTableRight();
    afx_msg void OnAlignTableCenter();
    afx_msg void OnInsertTable();
    afx_msg void OnInsertTableOrTableProps();
    afx_msg void OnUpdateInsertTable(CCmdUI* pCmdUI);
    afx_msg void OnDeleteTable();
    afx_msg void OnInsertTableRow();
    afx_msg void OnInsertTableRowAbove();
    afx_msg void OnUpdateInsertTableRow(CCmdUI* pCmdUI);
    afx_msg void OnDeleteTableRow();
    afx_msg void OnUpdateInTableRow(CCmdUI* pCmdUI);
    afx_msg void OnInsertTableColumn();
    afx_msg void OnInsertTableColumnBefore();
    afx_msg void OnUpdateInsertTableColumn(CCmdUI* pCmdUI);
    afx_msg void OnDeleteTableColumn();
    afx_msg void OnUpdateInTableColumn(CCmdUI* pCmdUI);
    afx_msg void OnInsertTableCell();
    afx_msg void OnInsertTableCellBefore();
    afx_msg void OnUpdateInsertTableCell(CCmdUI* pCmdUI);
    afx_msg void OnDeleteTableCell();
    afx_msg void OnUpdateInTableCell(CCmdUI* pCmdUI);
    afx_msg void OnInsertTableCaption();
    afx_msg void OnUpdateInsertTableCaption(CCmdUI* pCmdUI);
    afx_msg void OnDeleteTableCaption();
    afx_msg void OnUpdateInTableCaption(CCmdUI* pCmdUI);
    afx_msg void OnToggleTableBorder();
    afx_msg void OnUpdateToggleTableBorder(CCmdUI* pCmdUI);
    afx_msg void OnToggleHeaderCell();
    afx_msg void OnUpdateToggleHeaderCell(CCmdUI* pCmdUI);
    afx_msg void OnPropsTable();
    afx_msg void OnPropsTableRow();
    afx_msg void OnPropsTableColumn();
    afx_msg void OnPropsTableCell();
    afx_msg void OnDisplayTables();
    afx_msg void OnUpdateDisplayTables(CCmdUI* pCmdUI);
    afx_msg void OnNextParagraph();
    afx_msg void OnPreviousParagraph();
    afx_msg void OnUp();
    afx_msg void OnDown();
    afx_msg void OnNextChar();
    afx_msg void OnPreviousChar();
    afx_msg void OnBeginOfLine();
    afx_msg void OnEndOfLine();
    afx_msg void OnPageUp();
    afx_msg void OnPageDown();
    afx_msg void OnNextWord();
    afx_msg void OnPreviousWord();
    afx_msg void OnSelectUp();
    afx_msg void OnSelectDown();
    afx_msg void OnSelectNextChar();
    afx_msg void OnSelectPreviousChar();
    afx_msg void OnSelectBeginOfLine();
    afx_msg void OnSelectEndOfLine();
    afx_msg void OnSelectPageUp();
    afx_msg void OnSelectPageDown();
    afx_msg void OnSelectBeginOfDocument();
    afx_msg void OnSelectEndOfDocument();
    afx_msg void OnSelectNextWord();
    afx_msg void OnSelectPreviousWord();
    // We trap these messages to do editor's copy/cut/paste
    // CAbstractCX::CopySelection() will be called if
    // view is not an Editor
    afx_msg void OnGoToDefaultPublishLocation();
    afx_msg	void OnEditCopy();
    afx_msg void OnCopyStyle();
    afx_msg void OnUpdateCopyStyle(CCmdUI* pCmdUI);
    afx_msg	void ReportCopyError(int iError);
    afx_msg	void OnEditCut();
    afx_msg	void OnEditDelete();
    afx_msg	void OnEditPaste();
    afx_msg void OnUpdateEditCut(CCmdUI* pCmdUI);
    afx_msg void OnUpdateEditCopy(CCmdUI* pCmdUI);
    afx_msg void OnUpdateEditPaste(CCmdUI* pCmdUI);
    afx_msg void OnCanInteract(CCmdUI* pCmdUI);
//    afx_msg void OnEditFindAgain();
	afx_msg void OnFileOpen();
    afx_msg void OnFileOpenURL();
    afx_msg void OnSetImageAsBackground();
    afx_msg void OnFontSizeDropDown();
    afx_msg void OnEditBarToggle();
    afx_msg void OnCharacterBarToggle();
    afx_msg void OnUpdateEditBarToggle(CCmdUI* pCmdUI);
    afx_msg void OnUpdateCharacterBarToggle(CCmdUI* pCmdUI);
    afx_msg void OnUpdateFileSave(CCmdUI* pCmdUI);
    afx_msg void OnSelectNextNonTextObject();
    afx_msg void OnUpdateEditSource(CCmdUI* pCmdUI);
	//}}AFX_MSG

    // These are called CNetscapeEditView::OnCmdMsg
    //   to avoid too many individual message-map funtions 
    void OnFontSize(UINT nID);
    void OnCharacterStyle(UINT nID);
    void OnUpdateCharacterStyle(UINT nID, CCmdUI* pCmdUI);

    // Keep these outside of AFX_MSG - they use calculated
    //   or use a range of IDs and are not understood by App/Class wizards
    afx_msg void OnFormatParagraph( UINT nID );
	afx_msg void OnUpdateParagraphMenu(CCmdUI* pCmdUI);
	afx_msg void OnUpdateParagraphControls(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCharacterControls(CCmdUI* pCmdUI);
    afx_msg void OnFontColorMenu(UINT nID);
    //afx_msg void OnUpdateFontSize(CCmdUI* pCmdUI);
    afx_msg void OnUpdateInsertMenu(CCmdUI* pCmdUI);
#ifdef _IME_COMPOSITION
    afx_msg void OnLButtonDown(UINT uFlags, CPoint cpPoint);
    #ifdef XP_WIN32
        afx_msg LRESULT OnWmeImeComposition(WPARAM wparam,LPARAM lparam);
        afx_msg LRESULT OnWmeImeStartComposition(WPARAM wparam,LPARAM lparam);
        afx_msg LRESULT OnWmeImeEndComposition(WPARAM wparam,LPARAM lparam);
        afx_msg LRESULT OnInputLanguageChange(WPARAM wparam,LPARAM lparam);
        afx_msg LRESULT OnInputLanguageChangeRequest(WPARAM wparam,LPARAM lparam);
        afx_msg LRESULT OnWmeImeKeyDown(WPARAM wparam,LPARAM lparam);
    #else
        afx_msg LRESULT OnReportIme(WPARAM wparam,LPARAM lparam);
    #endif //XP_WIN32
#endif //_IME_COMPOSITION
	afx_msg void OnCheckSpelling();
	afx_msg void OnSpellingLanguage();
    afx_msg void OnUpdateEditFindincurrent(CCmdUI* pCmdUI);
    afx_msg void OnUpdateEditFindAgain(CCmdUI* pCmdUI);
    afx_msg void OnUpdateFileDocinfo(CCmdUI* pCmdUI);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

    LRESULT OnButtonMenuOpen(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()
    void OnDocProperties(int iStartPage = -1);
};

#ifdef FEATURE_WAIT_CURSOR
#include "waitcur.i00"
#endif

#ifdef _IMAGE_CONVERT
CONVERT_IMAGERESULT FE_ImageConvertDialog(CONVERT_IMGCONTEXT *,CONVERT_IMGCONTEXT *,CONVERT_IMG_INFO *,int16,CONVERT_IMG_ARRAY imagearray);
CONVERT_IMAGERESULT FE_ImageDoneCallBack(CONVERT_IMGCONTEXT *p_outputimageContext,int16 p_numoutputs,void *p_MWContext);
void FE_ImageConvertDisplayBuffer(void *);
#endif //_IMAGE_CONVERT

#endif // EDVIEW_H
#endif // EDITOR
