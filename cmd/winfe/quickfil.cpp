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

//		This file contains the implementations of the various Bookmark Quick
//		File classes.


/**	INCLUDE	**/
#include "stdafx.h"
#include "cxicon.h"
#include "quickfil.h"

#ifdef _DEBUG
#undef THIS_FILE
static char	BASED_CODE THIS_FILE[] = __FILE__;
#endif

typedef struct {
	UINT nCommand;
	int  nPosition;
} MnemonicStruct;


/****************************************************************************
*
*	Class: CTreeItem
*
*	DESCRIPTION:
*		This class provides an abstract object for representing an item
*		within a hierarchical tree.
*
****************************************************************************/

/****************************************************************************
*
*	CTreeItem::CTreeItem
*
*	PARAMETERS:
*		pImg		- image to be drawn with this item (NULL if none)
*		uID			- unique item identifier
*		strLabel	- text label for this item
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Constructor.
*
****************************************************************************/

CTreeItem::CTreeItem(CBitmap * pUnselectedImg, CBitmap *pSelectedImg, const UINT uID, const CString & strLabel, CMenu *pSubMenu)
{
	m_pUnselectedImg = pUnselectedImg;
	m_pSelectedImg = pSelectedImg;

	m_uID = uID;
	m_strLabel = strLabel;

	m_unselectedBitmapSize.cx = m_unselectedBitmapSize.cy = 0;
	m_selectedBitmapSize.cx = m_selectedBitmapSize.cy = 0;
	m_textSize.cx = m_textSize.cy = 0;
	m_accelSize.cx = m_accelSize.cy =0;
	m_pSubMenu = pSubMenu;

} // END OF	FUNCTION CTreeItem::CTreeItem()

/****************************************************************************
*
*	CTreeItem::~CTreeItem
*
*	PARAMETERS:
*		N/A
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Destructor.
*
****************************************************************************/

CTreeItem::~CTreeItem()
{
	if(m_pSubMenu)
		delete m_pSubMenu;
} // END OF	FUNCTION CTreeItem::~CTreeItem()


/****************************************************************************
*
*	Class: CTreeItemList
*
*	DESCRIPTION:
*		This is a collection class for holding CTreeItem objects.
*
****************************************************************************/

/****************************************************************************
*
*	CTreeItemList::CTreeItemList
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Constructor
*
****************************************************************************/

CTreeItemList::CTreeItemList()
{

} // END OF	FUNCTION CTreeItemList::CTreeItemList()

/****************************************************************************
*
*	CTreeItemList::~CTreeItemList
*
*	PARAMETERS:
*		N/A
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Destructor. We provide auto-cleanup for the objects in the collection.
*
****************************************************************************/

CTreeItemList::~CTreeItemList()
{
	int nCount = GetSize();

	for (int i = 0; i < nCount; i++)
	{
		CTreeItem * pItem = Get(i);
		if (pItem != NULL)
		{
			delete pItem;
		}
	}
	RemoveAll();


} // END OF	FUNCTION CTreeItemList::~CTreeItemList()


/****************************************************************************
*
*	Class: CTreeMenu
*
*	DESCRIPTION:
*		This class provides a custom, self drawing menu object for displaying
*		graphics as well as text in a menu item.
*
****************************************************************************/

/****************************************************************************
*
*	CONSTANTS
*
****************************************************************************/
static int nIMG_SPACE = 3;	// Gap between image and text in menu item

/****************************************************************************
*
*	CTreeMenu::CTreeMenu
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Constructor.
*
****************************************************************************/

CTreeMenu::CTreeMenu()
{
	m_WidestImage=0;
	m_WidestText=0;
	m_WidestAccelerator=0;
	m_pParent = NULL;

} // END OF	FUNCTION CTreeMenu::CTreeMenu()

CTreeMenu::~CTreeMenu()
{
	m_itemList.RemoveAll();

	DeleteMnemonics();

}

void CTreeMenu::DeleteMnemonics(void)
{
	POSITION pos;
	WORD dwKey;
	MnemonicStruct *pMnemonic = NULL;

	// also destroy bitmaps;
	pos = m_mnemonicMap.GetStartPosition();

	while(pos)
	{
		m_mnemonicMap.GetNextAssoc(pos, dwKey,(void *&)pMnemonic);
		delete pMnemonic;
	}

	m_mnemonicMap.RemoveAll();

}

/****************************************************************************
*
*	CTreeMenu::AddItem
*
*	PARAMETERS:
*		pItem		- pointer to abstract item
*		hSubMenu	- handle of sub-menu is this item is a new node
*
*	RETURNS:
*		TRUE if successful.
*
*	DESCRIPTION:
*		This function is called to add a new menu item to the main trunk. The
*		new item can also be a branch, if hSubMenu is set properly.
*****************************************************************************/

BOOL CTreeMenu::AddItem(CTreeItem * pItem, int nPosition /*=0*/, CTreeMenu *pSubMenu /*= NULL*/, 
						void* pIcon /* = NULL */, IconType nIconType /* = BUILTIN_BITMAP */)
{
	BOOL bRtn = FALSE;
	
	
	m_itemList.Add(pItem);

	pItem->SetIcon(pIcon, nIconType);

	if (pSubMenu != NULL)
	{
		
		bRtn = InsertMenu(nPosition, MF_BYPOSITION | MF_OWNERDRAW | MF_POPUP, (UINT)pSubMenu->GetSafeHmenu(),
			(const char *)pItem);
	}
	else
	{
		bRtn = InsertMenu(nPosition, MF_BYPOSITION | MF_OWNERDRAW, pItem->GetID(), (const char *)pItem);
	}
	
	return(bRtn);

} // END OF	FUNCTION CTreeMenu::AddItem()

CMenu * CTreeMenu::FindMenu(CString& menuName)
{
	int nSize = m_itemList.GetSize();

	for(int i = 0; i < nSize; i ++)
	{
		CTreeItem *pItem = (CTreeItem*)m_itemList[i];

		if(pItem->GetLabel() == menuName)
			return pItem->GetSubMenu();
	}

	return NULL;

}
/****************************************************************************
*
*	CTreeMenu::ClearItems
*
*	PARAMETERS:
*		start:		Clear all elements starting with this index 
*		fromEnd:	Delete up until this many elements from end
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Removes all items from the itemList starting with start until
*		fromEnd elements from the end
*
****************************************************************************/

void CTreeMenu::ClearItems(int start, int fromEnd)
{

	m_itemList.RemoveAt(start, m_itemList.GetSize() - fromEnd - start);
}

void CTreeMenu::AddMnemonic(TCHAR cMnemonic, UINT nCommand, int nPosition)
{
	MnemonicStruct *pMnemonic = new MnemonicStruct;

	pMnemonic->nCommand = nCommand;
	pMnemonic->nPosition = nPosition;

	m_mnemonicMap.SetAt(toupper(cMnemonic), pMnemonic);

}

BOOL CTreeMenu::GetMnemonic(TCHAR cMnemonic, UINT &nCommand, int &nPosition)
{
	MnemonicStruct *pMnemonic;

	if(m_mnemonicMap.Lookup(toupper(cMnemonic), (void*&)pMnemonic) )
	{
		nCommand = pMnemonic->nCommand;
		nPosition = pMnemonic->nPosition;
		return TRUE;
	}
	else
		return FALSE;
}

/****************************************************************************
*
*	CTreeMenu::CalculateItemDimensions
*
*	PARAMETERS:
*		N/A
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		For each item in the menu, this calculates the size of that item's
*		bitmap, menuitem text, and menuitem accelerator text.  In addition,
*		this keeps track of the widest bitmap, text, and accelerator text in
*		the menu.  This is done so that the accelerator can appear in a
*		reasonable position on the menu and assuring that enough space will
*		given to the menu item to draw itself
*
****************************************************************************/

void CTreeMenu::CalculateItemDimensions(void)
{

	CSize unselectedBitmapSize, selectedBitmapSize, textSize, acceleratorSize;
	CString label;

	m_WidestImage=0;
	m_WidestText=0;
	m_WidestAccelerator=0;

	int numItems = m_itemList.GetSize();

	for(int i=0; i<numItems; i++)
	{
		CTreeItem *item = (CTreeItem*)m_itemList.Get(i);

		unselectedBitmapSize=MeasureBitmap(item->GetUnselectedImage());
		item->SetUnselectedBitmapSize(unselectedBitmapSize);

		selectedBitmapSize=MeasureBitmap(item->GetSelectedImage());
		item->SetSelectedBitmapSize(selectedBitmapSize);

		if(unselectedBitmapSize.cx > m_WidestImage)
			m_WidestImage = unselectedBitmapSize.cx;

		if(selectedBitmapSize.cx > m_WidestImage)
			m_WidestImage = selectedBitmapSize.cx;

		label = item->GetLabel();

		//Figure out if there is an accelerator	
		int tabPos=label.Find('\t');

		if(tabPos!=-1)
		{
			textSize = MeasureText(label.Left(tabPos));
			acceleratorSize = MeasureText(label.Right(label.GetLength() - (tabPos +1)));
			item->SetAcceleratorSize(acceleratorSize);

			if(acceleratorSize.cx > m_WidestAccelerator)
				m_WidestAccelerator = acceleratorSize.cx;

		}
		else
			textSize = MeasureText(label);

		if(textSize.cx > m_WidestText)
			m_WidestText = textSize.cx;

		item->SetTextSize(textSize);
	}
}

/****************************************************************************
*
*	CTreeMenu::MeasureBitmap
*
*	PARAMETERS:
*		pBitmap:  The bitmap to measure
*
*	RETURNS:
*		The size of the bitmap
*
*	DESCRIPTION:
*		Given a bitmap, this returns its dimensions
*
****************************************************************************/

CSize CTreeMenu::MeasureBitmap(CBitmap *pBitmap)
{

	CSize size;

	if (pBitmap != NULL)
	{
		BITMAP bmp;
		pBitmap->GetObject(sizeof(bmp), &bmp);

		size.cx = bmp.bmWidth;
		size.cy = bmp.bmHeight;
	}
	else
		size.cx = size.cy = 0;

	return size;


}


/****************************************************************************
*
*	CTreeMenu::MeasureText
*
*	PARAMETERS:
*		text:  The CString text to be measured
*
*	RETURNS:
*		Returns the size of the text in the current font
*
*	DESCRIPTION:
*		Given a CString this returns the dimensions of that text
*
****************************************************************************/

CSize CTreeMenu::MeasureText(CString text)
{

	HFONT MenuFont;

#if defined(WIN32)
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof(NONCLIENTMETRICS);
	if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0))
	{
		MenuFont = theApp.CreateAppFont( ncm.lfMenuFont );
	}
	else
	{
		MenuFont = (HFONT)::GetStockObject(SYSTEM_FONT);
	}
#else	// Win16
		MenuFont = (HFONT)::GetStockObject(SYSTEM_FONT);
#endif	// WIN32

	CDC dc;
	dc.CreateCompatibleDC(NULL);
	CFont * pOldFont = dc.SelectObject(CFont::FromHandle(MenuFont));
	CSize sizeTxt = dc.GetTabbedTextExtent((LPCSTR)text, strlen(text), 0, NULL);
	dc.SelectObject(pOldFont);

	return sizeTxt;

}

/****************************************************************************
*
*	CTreeMenu::MeasureItem
*
*	PARAMETERS:
*		lpMI	- pointer to LPMEASUREITEMSTRUCT
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We must override this function to inform the system of our menu
*		item dimensions, since we're owner draw style.
*
****************************************************************************/
#define ACCELERATOR_TAB 5		//Leave some space between the text and the accelerator
#define ACCELERATOR_RIGHT 15	//Leave a right margin between end of
								//menu item and end of accelerator
#define MENU_LEFT_MARGIN 7		//Leave a left margin for text with no bitmaps
void CTreeMenu::MeasureItem(LPMEASUREITEMSTRUCT lpMI)
{
	CTreeItem * pItem = (CTreeItem *)(lpMI->itemData);
	ASSERT(pItem != NULL);
	
	// First, get all of the dimensions
	CSize sizeUnselectedImg=pItem->GetUnselectedBitmapSize();
	CSize sizeSelectedImg=pItem->GetSelectedBitmapSize();
	CSize sizeText=pItem->GetTextSize();
	CSize sizeAccelerator=pItem->GetAcceleratorSize();

	//if there's no bitmap we need to provide a margin for the text
	lpMI->itemWidth = (m_WidestImage == 0) ? MENU_LEFT_MARGIN : 0;
	//Return the values depending upon whether there is an accelerator
	if(sizeAccelerator.cx !=0)
	{
		lpMI->itemWidth += m_WidestImage + m_WidestText + sizeAccelerator.cx + ACCELERATOR_TAB + ACCELERATOR_RIGHT + (2*nIMG_SPACE);
	}
	else
		lpMI->itemWidth += m_WidestImage + sizeText.cx + (nIMG_SPACE * 2);

	int nImageHeight = max(sizeUnselectedImg.cy, sizeSelectedImg.cy);
	lpMI->itemHeight = max(nImageHeight, sizeText.cy) + 4;
	
} // END OF	FUNCTION CTreeMenu::MeasureItem()

/****************************************************************************
*
*	CTreeMenu::DrawItem
*
*	PARAMETERS:
*		lpDI	- pointer to LPDRAWITEMSTRUCT
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We must override this function to draw our image and text for the
*		menu item, since we're owner draw style.
*
****************************************************************************/

void CTreeMenu::DrawItem(LPDRAWITEMSTRUCT lpDI)
{
	// Extract the goods from lpDI
	CTreeItem * pItem = (CTreeItem *)(lpDI->itemData);
	ASSERT(pItem != NULL);
	CDC * pDC = CDC::FromHandle(lpDI->hDC);
	CRect rc(&(lpDI->rcItem));

	// Get proper colors and paint the background first
	CBrush brBG;
	COLORREF rgbTxt;
	if (lpDI->itemState & ODS_SELECTED)
	{
		brBG.CreateSolidBrush(::GetSysColor(COLOR_HIGHLIGHT));
		rgbTxt = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
	}
	else
	{
		brBG.CreateSolidBrush(::GetSysColor(COLOR_MENU));
		rgbTxt = ::GetSysColor(COLOR_MENUTEXT);
	}
	pDC->FillRect(&rc, &brBG);
	
	// Now paint the bitmap, left side of menu
	rc.left += nIMG_SPACE;
	CSize imgSize = DrawImage(pDC, rc, pItem, lpDI->itemState & ODS_SELECTED);
	
	// And now the text
	HFONT MenuFont;
#if defined(WIN32)
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof(NONCLIENTMETRICS);
	if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0))
	{
		MenuFont = theApp.CreateAppFont( ncm.lfMenuFont );
	}
	else
	{
		MenuFont = (HFONT)::GetStockObject(SYSTEM_FONT);
	}
#else	// Win16
	MenuFont = (HFONT)::GetStockObject(SYSTEM_FONT);
#endif	// WIN32

	CFont * pOldFont = pDC->SelectObject(CFont::FromHandle(MenuFont));
	int nOldBkMode = pDC->SetBkMode(TRANSPARENT);
	COLORREF rgbOldTxt = pDC->SetTextColor(rgbTxt);

	//Figure out where text should go in relation to bitmaps
	if(m_WidestImage == 0)
		rc.left += MENU_LEFT_MARGIN;

	rc.left += imgSize.cx + nIMG_SPACE + (m_WidestImage-imgSize.cx);

	//Deal with the case where there is a tab for an accelerator
	CString label=pItem->GetLabel();
	int tabPos=label.Find('\t');

	if(tabPos!=-1)
	{
		pDC->DrawText(label.Left(tabPos), -1, &rc, DT_LEFT | DT_VCENTER  |DT_SINGLELINE);
		//Give the accelerator some room on the right
		rc.right-=ACCELERATOR_RIGHT + (m_WidestAccelerator - pItem->GetAcceleratorSize().cx);
		pDC->DrawText(label.Right(label.GetLength()-(tabPos+1)), -1, &rc, DT_RIGHT | DT_VCENTER  |DT_SINGLELINE);
	}
	else
	{
		pDC->DrawText(label, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE |DT_EXPANDTABS);
	}
	
	pDC->SetBkMode(nOldBkMode);
	pDC->SetTextColor(rgbOldTxt);
	pDC->SelectObject(pOldFont);
	
} // END OF	FUNCTION CTreeMenu::DrawItem()

/****************************************************************************
*
*	CTreeMenu::DrawImage
*
*	PARAMETERS:
*		pDC		- pointer to device context to draw on
*		rect	- bounding rectangle to draw in
*		pItem	- pointer to CTreeItem that contains the data
*		bIsSelected - is this item selected
*
*	RETURNS:
*		width of the image, in pixels
*
*	DESCRIPTION:
*		Protected helper function for drawing the bitmap image in a menu
*		item. We return the size of the painted image so the caller can
*		position the menu text after the image.
*
****************************************************************************/

CSize CTreeMenu::DrawImage(CDC * pDC, const CRect & rect, CTreeItem * pItem,
						   BOOL bIsSelected)
{
	CSize sizeImg;
	
	sizeImg.cx = sizeImg.cy = 0;

	CBitmap * pImg = bIsSelected ? pItem->GetSelectedImage() : pItem->GetUnselectedImage();
	if (pImg != NULL)
	{
		HPALETTE hPalette= WFE_GetUIPalette(m_pParent);

		HPALETTE hOldPalette = ::SelectPalette(pDC->m_hDC, hPalette, FALSE);
		// Get image dimensions
		BITMAP bmp;
		pImg->GetObject(sizeof(bmp), &bmp);

		sizeImg.cx = bmp.bmWidth;
		sizeImg.cy = bmp.bmHeight;
		
		// Create a scratch DC and select our bitmap into it.
		CDC * pBmpDC = new CDC;
		pBmpDC->CreateCompatibleDC(pDC);
		CBitmap * pOldBmp = pBmpDC->SelectObject(pImg);
	
		// Center the image horizontally and vertically
		CPoint ptDst(rect.left + (m_WidestImage - sizeImg.cx)/2, rect.top +
			(((rect.Height() - sizeImg.cy) + 1) / 2));
			
		// Call the handy transparent blit function to paint the bitmap over
		// the background.
		if (pItem->GetIconType() == LOCAL_FILE)
		{
			HICON hIcon = pItem->GetLocalFileIcon();
			DrawIconEx(pDC->m_hDC, ptDst.x, ptDst.y, hIcon, 0, 0, 0, NULL, DI_NORMAL);
		}
		else if (pItem->GetIconType() == ARBITRARY_URL)
		{
			CRDFImage* pImage = pItem->GetCustomIcon();
			HDC hDC = pDC->m_hDC;
			int left = ptDst.x;
			int top = ptDst.y;
			int imageWidth = 16;
			int imageHeight = 16;
			COLORREF bkColor = bIsSelected  ? 
				GetSysColor(COLOR_HIGHLIGHT) : GetSysColor(COLOR_MENU);
			if (pImage && pImage->FrameLoaded()) 
			{
				// Now we draw this bad boy.
				if (pImage->m_BadImage) 
				{		 
					// display broken icon.
					HDC tempDC = ::CreateCompatibleDC(hDC);
					HBITMAP hOldBmp = (HBITMAP)::SelectObject(tempDC,  CRDFImage::m_hBadImageBitmap);
					::StretchBlt(hDC, 
						 left, top,
						imageWidth, imageHeight, 
						tempDC, 0, 0, 
						imageWidth, imageHeight, SRCCOPY);
					::SelectObject(tempDC, hOldBmp);
					::DeleteDC(tempDC);
				}
				else if (pImage->bits ) 
				{
					// Center the image. 
					long width = pImage->bmpInfo->bmiHeader.biWidth;
					long height = pImage->bmpInfo->bmiHeader.biHeight;
					int xoffset = (imageWidth-width)/2;
					int yoffset = (imageHeight-height)/2;
					if (xoffset < 0) xoffset = 0;
					if (yoffset < 0) yoffset = 0;
					if (width > imageWidth) width = imageWidth;
					if (height > imageHeight) height = imageHeight;

					HPALETTE hPal = WFE_GetUIPalette(NULL);
					HPALETTE hOldPal = ::SelectPalette(hDC, hPal, TRUE);

					::RealizePalette(hDC);
					
					if (pImage->maskbits) 
					{
						WFE_StretchDIBitsWithMask(hDC, TRUE, NULL,
							left+xoffset, top+xoffset,
							width, height,
							0, 0, width, height,
							pImage->bits, pImage->bmpInfo,
							pImage->maskbits, FALSE, bkColor);
					}
					else 
					{
						::StretchDIBits(hDC,
							left+xoffset, top+xoffset,
							width, height,
							0, 0, width, height, pImage->bits, pImage->bmpInfo, DIB_RGB_COLORS,
							SRCCOPY);
					}

					::SelectPalette(hDC, hOldPal, TRUE);
				}
			}
		}
		else ::FEU_TransBlt(pBmpDC, pDC, CPoint(0, 0), ptDst, sizeImg.cx, sizeImg.cy, hPalette, PALETTERGB(255, 0, 255));

		::SelectPalette(pDC->m_hDC, hOldPalette, TRUE);
		// Cleanup
		pBmpDC->SelectObject(pOldBmp);
		pBmpDC->DeleteDC();
		delete pBmpDC;
	}

	return(sizeImg);
	
} // END OF	FUNCTION CTreeMenu::DrawImage()
	
/****************************************************************************
*
*	Class: CPopupTree
*
*	DESCRIPTION:
*		This class represents a hierarchical popup menu object that can be
*		used for selecting from a list of sub-categorized items.
*
****************************************************************************/

/****************************************************************************
*
*	CPopupTree::CPopupTree
*
*	PARAMETERS:
*		pItems	- list of items for filling the tree
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Constructor.
*
****************************************************************************/

CPopupTree::CPopupTree(CTreeItemList * pItems)
{
	m_pTree = pItems;

} // END OF	FUNCTION CPopupTree::CPopupTree()

/****************************************************************************
*
*	CPopupTree::~CPopupTree
*
*	PARAMETERS:
*		N/A
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Destructor.
*
****************************************************************************/

CPopupTree::~CPopupTree()
{
	// Time to take out the trash! We maintain a list of objects that were
	// dynamically allocated on the heap, but couldn't be conveniently
	// deleted.
	for (int i = 0; i < m_GarbageList.GetSize(); i++)
	{
		CObject * pObject = m_GarbageList.GetAt(i);
		if (pObject != NULL)
		{
			delete pObject;
		}
	}
	m_GarbageList.RemoveAll();

} // END OF	FUNCTION CPopupTree::~CPopupTree()

/****************************************************************************
*
*	CPopupTree::Activate
*
*	PARAMETERS:
*		nX		- horizontal position (screen coordinates)
*		nY		- vertical position
*		pParent	- pointer to parent window
*
*	RETURNS:
*		TRUE if creation is successful
*
*	DESCRIPTION:
*		This function is called to display the popup menu, after construction.
*
****************************************************************************/

BOOL CPopupTree::Activate(int nX, int nY, CWnd * pParent)
{
	CTreeMenu Menu;
	BOOL bRtn = Menu.Create();
	
	// Now construct the menu from our tree
	BuildMenu(&Menu, m_pTree);
	
	if (bRtn)
	{
		bRtn = Menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, nX, nY, pParent);
	}
	
	return(bRtn);

} // END OF	FUNCTION CPopupTree::Activate()

/****************************************************************************
*
*	CPopupTree::BuildMenu
*
*	PARAMETERS:
*		pMenu	- pointer to the menu to be constructed
*		pItems	- list of item objects for building the menu
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This function is called to construct a custom menu from a list
*		of item objects. It is recursive and can be called for each node
*		that contains sub-items.
*
****************************************************************************/

void CPopupTree::BuildMenu(CTreeMenu * pMenu, CTreeItemList * pItems)
{
	for (int i = 0; i < pItems->GetSize(); i++)
	{
		CTreeItem * pItem = pItems->Get(i);
		ASSERT(pItem != NULL);
		if (pItem != NULL)
		{
			// See if this item has sub-items
			CTreeItemList * pSubItems = pItem->GetSubItems();
			if (pSubItems->GetSize() > 0)
			{
				// It does, so create a new menu and call this function
				// recursively to construct it.
				CTreeMenu * pSubMenu = new CTreeMenu;
			//	m_GarbageList.Add(pSubMenu);
				pSubMenu->Create();
				BuildMenu(pSubMenu, pSubItems);
				pSubMenu->CalculateItemDimensions();
				// Last but not least, add the new branch to the main menu
				BOOL bOk = pMenu->AddItem(pItem, 0, pSubMenu);
				ASSERT(bOk);
			}
			else
			{
				// Just add top-level item
				BOOL bOk = pMenu->AddItem(pItem);
				ASSERT(bOk);
			}
		}
	}
	pMenu->CalculateItemDimensions();
} // END OF	FUNCTION CPopupTree::BuildMenu()
