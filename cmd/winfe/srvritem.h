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

#ifndef __ServerItem_H
#define __ServerItem_H
// interface of the CNetscapeSrvrItem class
//

class CNetscapeSrvrItem : public COleServerItem
{
	DECLARE_DYNAMIC(CNetscapeSrvrItem)

// Constructors
public:
	CNetscapeSrvrItem(CGenericDoc* pContainerDoc);

// Attributes
	CGenericDoc* GetDocument() const
		{ return (CGenericDoc*)COleServerItem::GetDocument(); }
	BOOL IsShowUI() {return  m_ShowUI;}

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNetscapeSrvrItem)
	public:
	virtual BOOL OnDraw(CDC* pDC, CSize& rSize);
	virtual BOOL OnGetExtent(DVASPECT dwDrawAspect, CSize& rSize);
	virtual BOOL OnSetExtent(DVASPECT nDrawAspect, const CSize& size);
	//}}AFX_VIRTUAL

// Implementation
public:
	~CNetscapeSrvrItem();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
	void OnDoVerb(LONG iVerb);

protected:
	BOOL m_ShowUI;
	virtual void Serialize(CArchive& ar);   // overridden for document i/o

//	Loading and unloading of OLE menus only when needed.
private:
	static int m_iOLEMenuLock;
	void LoadOLEMenus();
	void UnloadOLEMenus();
};

/////////////////////////////////////////////////////////////////////////////
#endif // __ServerItem_H
