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

#define NS_ADDRESSFONTSIZE          8

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
public:
	CNSAddressInfo() 
	{
		szType = NULL;
		szName = NULL;
		idBitmap = 0;
		idEntry = 0xffffffff;
        bAllowExpansion = FALSE;
	}
	~CNSAddressInfo()
	{
		if (szType)
			free(szType);
		if (szName)
			free(szName);
	}
	char * GetType(void) { return szType; }
	char * GetName(void) { return szName; }
	UINT GetBitmap(void) { return idBitmap; }
	ULONG GetEntryID(void) { return idEntry; }
    BOOL GetExpansion(void) { return bAllowExpansion; }
    void SetExpansion(BOOL bExpand) { bAllowExpansion = bExpand; }
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

/////////////////////////////////////////////////////////////////////////////
// CNSAddressList window

class CNSAddressList :  public  CListBox,
                        public  CGenericObject,
                        public  IAddressControl
{
protected:
    BOOL                    m_bParse;
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
    virtual void SetCSID (int16 csid);

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
	void    HeaderCommand(int nID);
	void    UpdateHeaderType(void);
	void    UpdateHeaderContents(void);
    void    DisplayTypeList(int item = -1);
   inline int  GetActiveSelection() { return GetCurSel(); }
	int     SetActiveSelection(int);

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
    int	    DoNotifySelectionChange();
    void    DoDisplayTypeList();

    friend class CNSAddressNameEditField;
    friend class CNSAddressTypeControl;

};

void DrawTransparentBitmap(HDC hdc, HBITMAP hBitmap, short xStart, short yStart, COLORREF cTransparentColor );
void NS_FillSolidRect(HDC hdc, LPCRECT crRect, COLORREF rgbFill);
void NS_Draw3dRect(HDC hdc, LPCRECT crRect, COLORREF rgbTL, COLORREF rgbBR);
void NS_DrawRaisedRect( HDC hDC, LPRECT lpRect );
void NS_DrawLoweredRect( HDC hDC, LPRECT lpRect );
void NS_Draw3DButtonRect( HDC hDC, LPRECT lpRect, BOOL bPushed );

#endif __NSADRLST_H__	// end define of CNSAddressList
