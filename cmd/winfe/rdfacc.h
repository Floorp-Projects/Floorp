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

#ifndef RDFACC_H
#define RDFACC_H

#include "cxstubs.h"

class CCustomImageObject;
class NSNavCenterImage;

enum IconType { BUILTIN_BITMAP, LOCAL_FILE, ARBITRARY_URL }; // TODO: Use this icon type for the tree AND the toolbars

class CHTFEData
{
private:
	CHTFEData(); // Disallow the instantiation of objects of this class.

public:
	
	static void FlushIconInfo() 
	{ 
		// Just remove the HICONs from the local file cache.
		m_LocalFileCache.RemoveAll();  
		
		// Need to iterate over all the elements in the custom URL cache and destroy the images.
		POSITION pos = m_CustomURLCache.GetStartPosition();
		void* pData;
		CString key;
		while (pos != NULL)
		{
			m_CustomURLCache.GetNextAssoc(pos, key, pData);
			NSNavCenterImage* pImage = (NSNavCenterImage*)pData;
			delete pImage;
		}
	}

	static CMapStringToPtr m_LocalFileCache; 
	  // Hashed on file extension e.g., .html would hold the icon for HTML files.
	  // Special keys: "open folder" and "closed folder" are used to hold the folder icons.
	  //			   file URLs are used to represent drives, e.g., file:///a|/
	  //			   "no extension" is used to represent files with no extension

	static CMapStringToPtr m_CustomURLCache;
	  // Hashed on the URL of the image.  Holds pointers to NSNavCenterImages.
};

class CRDFColumn : public CObject
{
private:
	char* name; // The column's name.
	int width; // The column's width
	void* token; // The column token
	uint32 tokenType; // The column token type
	CString buffer; // A temp. storage buffer for column data

public:
	CRDFColumn(char* n, int w, void* t, uint32 tt)
		:name(n), width(w), token(t), tokenType(tt) {}

	uint32 GetDataType() const { return tokenType; }
	char* GetName() { return name; }
	void* GetToken() { return token; }
	CString& GetStorageBuffer() { return buffer; }
};

class CRDFMenuCommand : public CObject
{
private:
	HT_MenuCmd htCommandID;
	char* name;

public:
	CRDFMenuCommand(char* n, HT_MenuCmd ht)
		:name(n), htCommandID(ht) {}

	HT_MenuCmd GetHTCommand() { return htCommandID; }
};

class CRDFCommandMap
{
private:
	CObArray commandMap; // Contains a mapping of idents to objects.  This is used
						 // for columns and for menus.
public:
	CRDFCommandMap()
	{
	}

private:
	CRDFCommandMap(const CRDFCommandMap& la)
	{} // Object cannot be copied

public:
	~CRDFCommandMap() 
	{
		Clear();
	}
	
	int AddCommand(CObject* theObject)
	{
		return commandMap.Add(theObject);
	}

	CObject* GetCommand(int i) { return commandMap[i]; }

	void Clear()
	{
		int count = commandMap.GetSize();
		for (int i = count-1; i >= 0; i--)
		{
			CObject* obj = commandMap[i];
			commandMap.RemoveAt(i);
			delete obj;
		}
	}
};

class CRDFOutliner;

class CRDFEditWnd : public CEdit
{
protected:
	CRDFOutliner* m_pOwner;	
public:
    CRDFEditWnd(CRDFOutliner* owner) { m_pOwner = owner; }
    ~CRDFEditWnd() {};

	virtual BOOL PreTranslateMessage ( MSG * msg );

protected:
		// Generated message map functions
	//{{AFX_MSG(CRDFEditWnd)
	afx_msg void OnDestroy();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnKillFocus( CWnd* pNewWnd );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

// The RDF Context
class CRDFCX : public CStubsCX
{
private:
	CWnd* m_pCurrentRDFWindow;

public:
	CRDFCX(ContextType ctMyType, MWContextType XPType) 
		:CStubsCX(ctMyType, XPType) { m_pCurrentRDFWindow = NULL; };

	virtual CWnd *GetDialogOwner() const;

	void TrackRDFWindow(CWnd* pWnd) { m_pCurrentRDFWindow = pWnd; };

};

BOOL IsLocalFile(const char* pURL);
BOOL IsExecutableURL(const char* pURL);
HICON DrawLocalFileIcon(HT_Resource r, int left, int top, HDC hdc);
NSNavCenterImage* DrawArbitraryURL(HT_Resource r, int left, int top, int imageWidth, 
								   int imageHeight, HDC hDC, COLORREF bkColor,
								   CCustomImageObject* pObject, BOOL largeIcon);

HICON FetchLocalFileIcon(HT_Resource r);
NSNavCenterImage* FetchCustomIcon(HT_Resource r, CCustomImageObject* pObject, BOOL largeIcon);
IconType DetermineIconType(HT_Resource pNode, BOOL largeIcon);

void PaintBackground(HDC hdc, CRect rect, NSNavCenterImage* pImage, int ySrcOffset = -1);
		// This function tiles and paints the background image in the tree.

#endif // RDFACC_H