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

#ifndef __CNTRITEM_H
#define __CNTRITEM_H
// cntritem.h : interface of the CNetscapeCntrItem class
//

class CGenericDoc;
class CGenericView;

class CNetscapeCntrItem : public COleClientItem
{
	DECLARE_SERIAL(CNetscapeCntrItem)

// Constructors
public:
	CNetscapeCntrItem(CGenericDoc* pContainer = NULL);
		// Note: pContainer is allowed to be NULL to enable IMPLEMENT_SERIALIZE.
		//  IMPLEMENT_SERIALIZE requires the class have a constructor with
		//  zero arguments.  Normally, OLE items are constructed with a
		//  non-NULL document pointer.

// Attributes
public:
    BOOL m_bLoading;    //  If we are currently loading, from the net until completed creation from file.
    BOOL m_bBroken;     //  Consider this item broken.
    BOOL m_bDelayed;    //  We have postponed loading of this item.
	BOOL m_isDirty;
	BOOL m_isFullPage;
	BOOL m_bIsOleItem;
	BOOL m_bSetExtents;


    CString m_csAddress;    //  The url that this originated from, to avoid redundant reloads.
    CString m_csFileName;   //  The local temp file this item is located in, so on destruction we can also delete this file.
	CString m_csDosName;    //  The dos file name for m_csAddress if it is a local file.
	BOOL	m_bCanSavedByOLE;
	DISPID  m_idSavedAs;

    CFile m_cfOutput;   //  Output temporary file.
    int m_iLock;    //  How many references to this item anyhow?  To avoid freeing at inappropiate times.
    CPtrList m_cplUnblock;  //  List of layout elements to unblock once the load is complete.  Flushed on a per load basis.
    CPtrList m_cplDisplay;  //  List of layout elements that we will manually update once the load is complete.  Flushed on a per load basis.
    CPtrList m_cplElements; //  List of all layout elements referencing this item.

    BOOL m_bLocationBarShowing; //  Wether or not the URL bar was present in the Frame before we were activated.
    BOOL m_bStarterBarShowing; //  Wether or not the URL bar was present in the Frame before we were activated.
	CNetscapeCntrItem*	m_pOriginalItem;  // Needed when we reuse cached existing NPEmbeddedApp(s), e.g. when printing

//	Operations
public:
	CGenericDoc* GetDocument()
		{ return (CGenericDoc *)COleClientItem::GetDocument(); }
	CGenericView* GetActiveView()
		{ return (CGenericView *)COleClientItem::GetActiveView(); }
	BOOL IsDirty() {return m_isDirty;}


// Implementation
public:
	~CNetscapeCntrItem();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
	virtual void Serialize(CArchive& ar);
	virtual void OnDeactivateUI(BOOL bUndoable);
    virtual BOOL OnShowControlBars(CFrameWnd *pFrame, BOOL bShow);
	
// TODO: We can remove this check once everyone moves to MSVC 4.0
#if defined(MSVC4)
    virtual BOOL OnUpdateFrameTitle();
#else	
    virtual void OnUpdateFrameTitle();
#endif	// MSVC4



protected:
	virtual void OnChange(OLE_NOTIFICATION wNotification, DWORD dwParam);
	virtual BOOL OnChangeItemPosition(const CRect& rectPos);
    virtual void OnGetItemPosition(CRect& rectPos);
    virtual BOOL OnGetWindowContext(CFrameWnd **ppMainFrame, CFrameWnd **ppDocFrame, LPOLEINPLACEFRAMEINFO lpFrameInfo);
	virtual BOOL CanActivate();
	virtual void OnActivate();
};

/////////////////////////////////////////////////////////////////////////////
#endif // __CNTRITEM_H
