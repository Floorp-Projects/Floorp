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

// NSAdrLst.h : header file
//

#ifndef __NSADRLST_H__
#define __NSADRLST_H__

#include "nsadrtyp.h"
#include "nsadrnam.h"
#include "apiaddr.h"
#include "abcom.h"
#include "apimsg.h"
#include "mailmisc.h"

#define NS_ADDRESSFONTSIZE          8

class CListNameCompletionEntryList;

class CNSAddressTypeInfo {
protected:
    BOOL m_bHidden;
    BOOL m_bExclusive;
    BOOL m_bExpand;
    UINT m_pidBitmap;
    DWORD m_dwUserData;
    char * m_pszValue;
public:
    CNSAddressTypeInfo(UINT pidBitmap = 0, BOOL bHidden = FALSE, BOOL bExclusive = FALSE, DWORD dwUserData = 0, BOOL bExpand = TRUE)
    {
        m_pidBitmap = pidBitmap;
        m_bExclusive = bExclusive;
        m_bHidden = bHidden;
        m_dwUserData = dwUserData;
        m_pszValue = NULL;
        m_bExpand = bExpand;
    }
    ~CNSAddressTypeInfo()
    {
        if (m_pszValue)
            free(m_pszValue);
    }
    inline void SetValue(const char * pszValue) { 
        if (m_pszValue)
            free(m_pszValue);
        m_pszValue = NULL;
        if (pszValue && strlen(pszValue))
            m_pszValue = strdup(pszValue); 
    }
    inline char * GetValue(void) { return m_pszValue; }
    inline BOOL GetHidden(void) { return m_bHidden; }
    inline BOOL GetExclusive(void) { return m_bExclusive; }
    inline BOOL GetExpand(void) { return m_bExpand; }
    inline void SetHidden(BOOL bVal) { m_bHidden = bVal; }
    inline void SetExclusive(BOOL bVal) { m_bExclusive = bVal; }
    inline void SetExpand(BOOL bVal) { m_bExpand = bVal; }
    inline UINT GetBitmap(void) { return m_pidBitmap; }
    inline DWORD GetUserData(void) { return m_dwUserData; }
};

class CNSAddressInfo {
protected:
	char * szType;
	char * szName;
	UINT idBitmap;
	ULONG idEntry;
    BOOL bAllowExpansion;
	AB_NameCompletionCookie* pCookie;
	int nNumCompletionResults;
	MSG_Pane *pPickerPane;
	BOOL bActiveSearch;
	NameCompletionEnum eNCEnum;


  BOOL m_bEntrySelected;

public:
	CNSAddressInfo() 
	{
		szType = NULL;
		szName = NULL;
		idBitmap = 0;
		idEntry = 0xffffffff;
        bAllowExpansion = FALSE;
		pCookie = NULL;
		nNumCompletionResults = -1;
		pPickerPane = NULL;
		bActiveSearch = FALSE;
		eNCEnum = NC_NameComplete;
		

    m_bEntrySelected = FALSE;

  }
	~CNSAddressInfo()
	{
		if (szType)
			free(szType);
		if (szName)
			free(szName);
		if (pCookie)
			AB_FreeNameCompletionCookie(pCookie);
	}
	char * GetType(void) { return szType; }
	char * GetName(void) { return szName; }
	UINT GetBitmap(void) { return idBitmap; }
	ULONG GetEntryID(void) { return idEntry; }
    BOOL GetExpansion(void) { return bAllowExpansion; }
    void SetExpansion(BOOL bExpand) { bAllowExpansion = bExpand; }

    BOOL getEntrySelectedState()
    {
      return m_bEntrySelected;
    }

    void setEntrySelectedState(BOOL bEntrySelected)
    {
      m_bEntrySelected = bEntrySelected;
    }

	void SetName(const char *ptr = NULL) 
	{
		if (szName)
			free(szName);
		if (ptr)
			szName = strdup(ptr);
		else
			szName = NULL;
	}
	void SetType(const char * ptr = NULL)
	{
      char * temp = szType;
		if (ptr)
			szType = strdup(ptr);
		else
			szType = NULL;
		if (temp)
			free(temp);
	}
	void SetBitmap(UINT id = 0) 
	{
		idBitmap = id;
	}
	void SetEntryID(unsigned long id = 0xffffffff) 
	{
		idEntry = id;
	}

	void SetNameCompletionCookie(AB_NameCompletionCookie *cookie)
	{
		pCookie = cookie;
	}

	AB_NameCompletionCookie *GetNameCompletionCookie()
	{
		return pCookie;
	}

	void SetNumNameCompletionResults(int nNumResults)
	{
		nNumCompletionResults = nNumResults;
	}

	int GetNumNameCompletionResults()
	{
		return nNumCompletionResults;
	}

	void SetPickerPane(MSG_Pane *pickerPane)
	{
		pPickerPane = pickerPane;
	}

	MSG_Pane *GetPickerPane()
	{
		return pPickerPane;
	}

	void SetActiveSearch(BOOL activeSearch)
	{
		bActiveSearch = activeSearch;
	}

	BOOL GetActiveSearch()
	{
		return bActiveSearch;
	}

	void SetNameCompletionEnum(NameCompletionEnum ncEnum)
	{
		eNCEnum = ncEnum;
	}

	NameCompletionEnum GetNameCompletionEnum()
	{
		return eNCEnum;
	}


};

/////////////////////////////////////////////////////////////////////////
// These structures and methods are used to set, add, and retrieve
// address list entries.  The structure AND strings are copied.  The
// index is 0 based.

typedef struct
{
	LPCTSTR szType;		// must be in list of address choices
	LPCTSTR szName;
	UINT idBitmap;	    // may be null to use bitmap provider API
	unsigned long idEntry;
} NSAddressListEntry;

/////////////////////////////////////////////////////////////////
// Helper object to deal with the whole entry selection
// 
class CEntrySelector
{
private:
  int m_iIndex;

public:
  BOOL m_bPostponedTillButtonUp;
  int m_iX;
  int m_iY;

public:
  CEntrySelector() : m_iIndex(-1), m_bPostponedTillButtonUp(FALSE) {}
  ~CEntrySelector(){}

  void LButtonDown(int iIndex) {m_iIndex = iIndex;}
  void LButtonUp() {m_iIndex = -1;}
  BOOL WhatEntryBitmapClicked() {return m_iIndex;}
};

// name completion context
class CListNameCompletionCX: public CStubsCX
{
protected:
	CListNameCompletionEntryList* m_pOwnerList;

	int32 m_lPercent;
	BOOL m_bAnimated;

public:
	CListNameCompletionCX(CListNameCompletionEntryList *pOwnerList);

public:
	void SetOwnerList(CListNameCompletionEntryList *pOwnerList);
	int32 QueryProgressPercent();
	void SetProgressBarPercent(MWContext *pContext, int32 lPercent);

	void Progress(MWContext *pContext, const char *pMessage);
	void AllConnectionsComplete(MWContext *pContext);

	void UpdateStopState( MWContext *pContext );
    
	CWnd *GetDialogOwner() const;
};

/////////////////////////////////////////////////////////////////////////////
// CListNamecompletionEntryMailFrame
class CListNameCompletionEntryMailFrame: public IMailFrame{
public:
	CListNameCompletionEntryList *m_pParentList;
	unsigned long m_ulRefCount;

	// Support for IMailFrame
	virtual CMailNewsFrame *GetMailNewsFrame();
	virtual MSG_Pane *GetPane();
	virtual void PaneChanged( MSG_Pane *pane, XP_Bool asynchronous, 
							  MSG_PANE_CHANGED_NOTIFY_CODE, int32 value);
	virtual void AttachmentCount(MSG_Pane *messagepane, void* closure,
								 int32 numattachments, XP_Bool finishedloading) {};
	virtual void UserWantsToSeeAttachments(MSG_Pane *messagepane, void *closure) {};

	// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);

	CListNameCompletionEntryMailFrame(CListNameCompletionEntryList *pParentList ) {
		m_ulRefCount = 0;
		m_pParentList = pParentList;
	}


};

/////////////////////////////////////////////////////////////////////////////
// CNameCompletionEntryList

class CListNameCompletionEntryList: public IMsgList{
public:
	MSG_Pane *m_pPickerPane;
	unsigned long m_ulRefCount;
	BOOL m_bSearching;
	CListNameCompletionEntryMailFrame *m_pMailFrame;
	CNSAddressList *m_pList;


public:
// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);

// IMsgList Interface
	virtual void ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									 MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									 int32 num);
	virtual void GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus);
	virtual void SelectItem( MSG_Pane* pane, int item );

	virtual void CopyMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo) {}
	virtual void MoveMessagesInto( MSG_Pane *pane, MSG_ViewIndex *indices, int count,
								   MSG_FolderInfo *folderInfo) {}


	//other functions
	void SetProgressBarPercent(int32 lPercent);
	void SetStatusText(const char* pMessage);
	void AllConnectionsComplete(MWContext *pContext);
	CWnd *GetOwnerWindow();

	CListNameCompletionEntryList( MSG_Pane *pPickerPane, CNSAddressList* pList ) {
		m_ulRefCount = 0;
		m_pPickerPane = pPickerPane;
		m_pList = pList;
		m_bSearching = FALSE;
		m_pMailFrame = new CListNameCompletionEntryMailFrame(this);
	}
};

/////////////////////////////////////////////////////////////////////////////
// CNSAddressList window

class CNSAddressList :  public  CListBox,
                        public  CGenericObject,
                        public  IAddressControl
{
protected:
    BOOL                    m_bParse;
	BOOL					m_bExpansion;
    BOOL                    m_bCreated;
    HBRUSH                  m_hBrushNormal;
    HPEN                    m_hPenNormal, m_hPenGrid, m_hPenGrey;
	CNSAddressTypeControl	* m_pAddressTypeList;
    CNSAddressNameEditField	* m_pNameField;
	int						m_nCurrentSelection;
	int						m_iFieldControlWidth;
	int						m_iBitmapWidth;
    int                     m_iTypeBitmapWidth;
	BOOL					m_bGridLines;
	BOOL					m_bArrowDown;
    BOOL                    m_bDrawTypeList;
    HFONT                   m_hTextFont;
    int                     m_iDefaultBitmapId;
    int                     m_lastIndex;
    LPADDRESSPARENT         m_pIAddressParent;
    int                     m_iItemHeight;
	MWContext *				m_pContext;
	CListNameCompletionCX *	m_pCX;
	MSG_Pane *				m_pPickerPane;
	IMsgList*				m_pINameCompList;
   
  CEntrySelector m_EntrySelector;
  HCURSOR m_hCursorBackup;
  BOOL m_bDragging;

public:
	CNSAddressList();	// Construction
	virtual ~CNSAddressList();

    // IUnknown
	STDMETHODIMP QueryInterface(REFIID,LPVOID *);
    STDMETHODIMP_(ULONG) Release(void);

    // IAddressControl
   virtual int GetItemFromPoint(LPPOINT point);
   virtual BOOL AddAddressType(char * pszChoice, UINT pidBitmap = 0, BOOL bExpande = TRUE,
        BOOL bHidden = FALSE,BOOL bExclusive = FALSE,DWORD dwUserData = 0);
   virtual void SetDefaultBitmapId(int id = 0) { m_iDefaultBitmapId = id; }
   virtual int GetDefaultBitmapId(void) { return m_iDefaultBitmapId; }
   virtual BOOL RemoveSelection(int nIndex = -1);
	virtual BOOL DeleteEntry( int nIndex );
	virtual int	FindEntry( int nStart, LPCTSTR lpszName );
	virtual BOOL Create(CWnd *pParent, int id = 1000);
	virtual CListBox * GetAddressTypeComboBox( void );
	virtual CEdit * GetAddressNameField( void );
	virtual void SetItemName(int nIndex, char * text);
	virtual void SetItemBitmap(int nIndex, UINT id);
	virtual void SetItemEntryID(int nIndex, unsigned long id);
   virtual void SetControlParent(LPADDRESSPARENT pIAddressParent);
   virtual int GetAddressList (LPNSADDRESSLIST * ppAddressList);
   virtual int SetAddressList (LPNSADDRESSLIST pAddressList, int count);
   virtual CListBox * GetListBox(void) { return (CListBox *)this; }
   virtual BOOL IsCreated(void) { return m_bCreated; }
	virtual int AppendEntry(BOOL expandName, LPCTSTR szType, LPCTSTR szName, UINT idBitmap, unsigned long idEntry );	
	virtual int InsertEntry( int nIndex, BOOL expandName, LPCTSTR szType, LPCTSTR szName, UINT idBitmap, unsigned long idEntry );
	virtual BOOL SetEntry( int nIndex, 
        LPCTSTR szType, LPCTSTR szName, UINT idBitmap, unsigned long idEntry);
	virtual BOOL GetEntry( int nIndex, 
        char **szType, char **szName, UINT *idBitmap, unsigned long *idEntry);
   virtual void GetTypeInfo(int nIndex, ADDRESS_TYPE_FLAG flag, void ** value);
	virtual int SetSel(int nIndex, BOOL bSelect);
    virtual void EnableParsing(BOOL bParse);
	virtual BOOL GetEnableParsing();
	virtual void EnableExpansion(BOOL bExpansion);
	virtual BOOL GetEnableExpansion();
    virtual void SetCSID (int16 csid);
	virtual void SetContext(MWContext *pContext);
	virtual MWContext *GetContext();
	virtual void ShowNameCompletionPicker(CWnd* pParent);
	virtual void StartNameCompletion(int nIndex = -1);
	virtual void StopNameCompletion(int nIndex = -1, BOOL bEraseCookie = TRUE);
	virtual void StartNameExpansion(int nIndex = -1);
	virtual void SetEntryHasNameCompletion(BOOL bHasNameCompletion = TRUE, int nIndex = -1);
	virtual BOOL GetEntryHasNameCompletion(int nIndex = -1);
	// cookies for name completion -1 = active selection
	virtual void SetNameCompletionCookieInfo(AB_NameCompletionCookie *pCookie, int nNumResults, 
											 NameCompletionEnum ncEnum, int nIndex = -1);
	virtual void GetNameCompletionCookieInfo(AB_NameCompletionCookie **pCookie, int *pNumResults, int nIndex = -1);
	virtual void SearchStarted();
	virtual void SearchStopped();
	virtual void SetProgressBarPercent(int32 lPercent);
	virtual void SetStatusText(const char* pMessage);
	virtual CWnd *GetOwnerWindow();



protected:

	int     AppendEntry( NSAddressListEntry *pAddressEntry = NULL, BOOL expandName = TRUE );	// to end of list, NULL for empty entry
	int     InsertEntry( int nIndex, NSAddressListEntry *pAddressEntry, BOOL expandName = TRUE);
	BOOL    SetEntry( int nIndex, NSAddressListEntry *pAddressEntry );
	BOOL    GetEntry( int nIndex, NSAddressListEntry *pAddressEntry );
	void    EnableGridLines( BOOL bEnable );
    void    DrawEntryBitmap(int iSel, CNSAddressInfo * pAddress = NULL, CDC * pDC = NULL, BOOL bErase = TRUE);
    int     GetTypeFieldLength(void);
    BOOL    ParseAddressEntry(int nSelection);
    void    SetEditField(char * text) { m_pNameField->SetWindowText(text); }
	void    SingleHeaderCommand(int nID);
	void    HeaderCommand(int nID);
	void    UpdateHeaderType(void);
	void    UpdateHeaderContents(void);
    void    DisplayTypeList(int item = -1);
   inline int  GetActiveSelection() { return GetCurSel(); }
	int     SetActiveSelection(int);


  void selectEntry(int iIndex, BOOL bState);
  BOOL isEntrySelected(int iIndex);
  void selectAllEntries(BOOL bState);
  int getEntryMultipleSelectionStatus(BOOL * pbContinuous, int * piFirst, int * piLast);

    virtual LRESULT DefWindowProc( UINT message, WPARAM wParam, LPARAM lParam );

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNSAddressList)
	public:
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	virtual void DeleteItem(LPDELETEITEMSTRUCT lpDeleteItemStruct);
	//}}AFX_VIRTUAL


    int     GetItemRect(HWND hwnd, int nIndex, LPRECT lpRect) const;
	UINT	ItemFromPoint(HWND hwnd, LPPOINT lpPoint, BOOL * bOutside) const;		

  BOOL isPointInItemBitmap(LPPOINT pPoint, int iIndex);
  void onKeyDown(int iVirtKey, DWORD dwFlags);
  void onMouseMove(HWND m_hWnd, WORD wFlags, int iX, int iY);

    BOOL    DoCommand( HWND hwnd, WPARAM wParam, LPARAM lParam );

	BOOL	OnKeyPress( CWnd *pChildControl, UINT nChar, UINT nRepCnt, UINT nFlags );
	void	DrawAddress( int nIndex, CRect &rect, CDC *pDC, BOOL bSelect = FALSE );
	void	DrawGridLine(CRect &rect, CDC *pDC);
	void	ComputeFieldWidths(CDC * pDC);

	BOOL    DoEraseBkgnd(HWND hwnd, HDC hdc);
	void    DoSetFocus(HWND);
	void    DoKillFocus(HWND);
	void    DoLButtonUp(HWND hwnd, UINT nFlags, LPPOINT point);
	void    DoLButtonDown(HWND hwmd, UINT nFlags, LPPOINT point);
	void    DoVScroll(HWND hwnd, UINT nSBCode, UINT nPos);
	void    DoChildLostFocus();
    int	    DoNotifySelectionChange(BOOL bShowPicker = TRUE);
    void    DoDisplayTypeList();

    friend class CNSAddressNameEditField;
    friend class CNSAddressTypeControl;

};

class FENameCompletionCookieInfo {
protected:
	CNSAddressList *m_pList;
	int				m_nIndex;

public:
	FENameCompletionCookieInfo()
	{
		m_pList = NULL;
		m_nIndex = -1;
	}

	void SetList(CNSAddressList* pList) {m_pList = pList;}
	CNSAddressList *GetList() { return m_pList;}

	void SetIndex(int nIndex) { m_nIndex = nIndex;}
	int  GetIndex() { return m_nIndex;}
};

void DrawTransparentBitmap(HDC hdc, HBITMAP hBitmap, short xStart, short yStart, COLORREF cTransparentColor );
void NS_FillSolidRect(HDC hdc, LPCRECT crRect, COLORREF rgbFill);
void NS_Draw3dRect(HDC hdc, LPCRECT crRect, COLORREF rgbTL, COLORREF rgbBR);
void NS_DrawRaisedRect( HDC hDC, LPRECT lpRect );
void NS_DrawLoweredRect( HDC hDC, LPRECT lpRect );
void NS_Draw3DButtonRect( HDC hDC, LPRECT lpRect, BOOL bPushed );

#endif __NSADRLST_H__	// end define of CNSAddressList
