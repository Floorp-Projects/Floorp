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

//		This file contains the declarations of the various Bookmark Quick
//		File classes.


#if	!defined(__QUICKFIL_H__)
#define	__QUICKFIL_H__

#ifndef	__AFXWIN_H__
	#error include 'stdafx.h' before including this	file for PCH
#endif

/****************************************************************************
*
*	Class: CTreeItemList
*
*	DESCRIPTION:
*		This is a collection class for holding CTreeItem objects.
*
****************************************************************************/

class CTreeItem;
#define CTreeItemListBase	CPtrArray

class CTreeItemList : public CTreeItemListBase
{
	public:
		CTreeItemList();
		virtual ~CTreeItemList();
		
		int Add(CTreeItem * pItem)
		{
			return(CTreeItemListBase::Add(pItem));
		}
		CTreeItem * Get(int nIndex) const
		{
			return((CTreeItem *)GetAt(nIndex));
		}
		
	protected:

	private:

}; // END OF CLASS CTreeItemList()


/****************************************************************************
*
*	Class: CTreeItem
*
*	DESCRIPTION:
*		This class provides an abstract object for representing an item
*		within a hierarchical tree.
*
****************************************************************************/

class CTreeItem
{
	public:
		CTreeItem(CBitmap * pUnselectedImg, CBitmap *pSelectedImg, const UINT uID, const CString & strLabel,
				  CMenu *pSubMenu = NULL);
		virtual ~CTreeItem();
		
		CTreeItemList * GetSubItems()
		{
			return(&m_SubItems);
		}

		CMenu * GetSubMenu(void)
		{
			return(m_pSubMenu);
		}

		CBitmap * GetUnselectedImage() const
		{
			return (m_pUnselectedImg);
		}

		CBitmap * GetSelectedImage() const
		{
			return(m_pSelectedImg);
		}
		const UINT GetID() const
		{
			return(m_uID);
		}

		const CString & GetLabel() const
		{
			return(m_strLabel);
		}
		
		void SetUnselectedBitmapSize(CSize size)
		{
			m_unselectedBitmapSize = size;
		}

		CSize GetUnselectedBitmapSize(void)
		{
			return m_unselectedBitmapSize;
		}

		void SetSelectedBitmapSize(CSize size)
		{
			m_selectedBitmapSize = size;
		}

		CSize GetSelectedBitmapSize(void)
		{
			return m_selectedBitmapSize;
		}

		void SetTextSize(CSize size)
		{
			m_textSize = size;
		}

		CSize GetTextSize(void)
		{
			return m_textSize;
		}

		void SetAcceleratorSize(CSize size)
		{
			m_accelSize = size;
		}

		CSize GetAcceleratorSize(void)
		{
			return m_accelSize;
		}

	// Aurora improvements to handle custom icons and arbitrary URLs.
		void	   SetIcon(void* pIcon, IconType iconType) { m_pCustomIcon = pIcon; m_nIconType = iconType; }
		HICON	   GetLocalFileIcon() { return (HICON)m_pCustomIcon; }
		CRDFImage* GetCustomIcon() { return (CRDFImage*)m_pCustomIcon; }
		IconType GetIconType() { return m_nIconType; }

	protected:
		CBitmap * m_pUnselectedImg;	// Image to be displayed for this item when unselected
		CBitmap * m_pSelectedImg;   // Image to be displayed for this item when selected
		UINT m_uID;			// Unique item identifier
		CString m_strLabel;	// Text label for this item
		
		CSize m_unselectedBitmapSize; // The size of the unselected bitmap
		CSize m_selectedBitmapSize;   // The size of the selected bitmap
		CSize m_textSize;	// The size of the text in the font when this is calculated
		CSize m_accelSize;	// The size of the accelerator in the font when this is calculated
		CTreeItemList m_SubItems;	// List of items subordinate to this one
		CMenu *m_pSubMenu;

		void* m_pCustomIcon;
		IconType m_nIconType;

	private:

}; // END OF CLASS CTreeItem()


/****************************************************************************
*
*	Class: CTreeMenu
*
*	DESCRIPTION:
*		This class provides a custom, owner draw menu object for displaying
*		graphics as well as text in a menu item. A group of these objects
*		can be used to construct a hierarchical list.
*
****************************************************************************/

#define CTreeMenuBase	CMenu

class CTreeMenu : public CTreeMenuBase, public CCustomImageObject
{
	public:
		CTreeMenu();
		~CTreeMenu();

		BOOL AddItem(CTreeItem * pItem, int nPosition = 0, CTreeMenu *pSubMenu = NULL, void* pCustomIcon = NULL, IconType m_nIconType = BUILTIN_BITMAP);
		BOOL Create()
		{
			return(CreatePopupMenu());
		}
		void ClearItems(int start, int fromEnd);
		void CalculateItemDimensions(void);
		CMenu * FindMenu(CString& menuName);
		// if nCommand is 0, then the mnemonic is for a submenu
		void AddMnemonic(TCHAR cMnemonic, UINT nCommand, int nPosition);
		// if nCommand is 0 then mnemonic is for a submenu
		// it returns FALSE if there is no mnemonic
		BOOL GetMnemonic(TCHAR cMnemonic, UINT &nCommand, int &nPosition);
		void SetParent(CWnd *pParent) { m_pParent = pParent;}

		virtual void LoadComplete(HT_Resource r) {}

	protected:
		virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMI);
		virtual void DrawItem(LPDRAWITEMSTRUCT lpDI);
		virtual CSize DrawImage(CDC * pDC, const CRect & rect, CTreeItem * pItem, BOOL bIsSelected);
		virtual void DeleteMnemonics(void);

	private:
		int m_WidestImage;
		int m_WidestText;
		int m_WidestAccelerator;
		CTreeItemList m_itemList;
		CMapWordToPtr m_mnemonicMap;
		CWnd *m_pParent;
	private:
		CSize MeasureBitmap(CBitmap *pBitmap);
		CSize MeasureText(CString text);


}; // END OF CLASS CTreeMenu()


/****************************************************************************
*
*	Class: CPopupTree
*
*	DESCRIPTION:
*		This class represents a hierarchical popup menu object that can be
*		used for selecting from a list of sub-categorized items.
*
****************************************************************************/

class CPopupTree
{
	public:
		CPopupTree(CTreeItemList * pItems);
		virtual ~CPopupTree();
		
		BOOL Activate(int nX, int nY, CWnd * pParent);
		 
	protected:
		void BuildMenu(CTreeMenu * pMenu, CTreeItemList * pItems);
		
		CTreeItemList * m_pTree;	// Contains entire tree of abstract item
									// objects. The menu is constructed from
									// this.
		
	private:
		CObArray m_GarbageList;		// Used for garbage collection, since we
									// dynamically allocate stuff within a
									// recursive function.

									
}; // END OF CLASS CPopupTree()


#endif // __QUICKFIL_H__
