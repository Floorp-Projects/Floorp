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

// TabList.cpp : implementation file
//

#include "stdafx.h"
#include "tclist.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTCList

// initialize string to calculate average char width
CString CTCList::m_strCharSet = 
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

CTCList::CTCList()
{
	m_nSpacing = 2;			// init column spacing
	m_nTabs = MIN_TAB_SIZE;	// init number of tabs
	m_nAvgCharWidth = 8;	// a reasonable default
									 
	// init size of tab setting and column width arrays
	m_aTabs.SetSize (MIN_TAB_SIZE);
	m_aColWidth.SetSize (MIN_TAB_SIZE);	

	InitColWidth();			// init column width array
}

CTCList::~CTCList()
{
}


BEGIN_MESSAGE_MAP(CTCList, CListBox)
	//{{AFX_MSG_MAP(CTCList)
	ON_MESSAGE(LB_ADDSTRING,	OnAddString)
	ON_MESSAGE(LB_INSERTSTRING,	OnInsertString)
	ON_MESSAGE(LB_DELETESTRING,	OnDeleteString)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTCList message handlers

// Set the number of spaces to separate columns.
void CTCList::SetColumnSpace(int nSpacing)
{                             
	// only process if different              
	if (nSpacing != m_nSpacing) 
	{
		m_nSpacing = nSpacing;	// store the number of character spaces
		CalculateTabs();		// calculate new tab settings
		SetTabStops(m_nTabs, 
			(LPINT) &m_aTabs[0]);	// set tab settings
	}
}


// Calculate average character width. The average character width is 
// calculated using the same method used by GetDialogBaseUnits function;
// average char width = (pixel width of "ABC...Zabc...z") / 52.
// The current font is used for the calculation.

void CTCList::CalculateAvgCharWidth(CDC* pDC)
{
	CFont* pCurrentFont = GetFont();	// get font currently using for list box

	// GetFont will return NULL if using System font.
	// If using system font just call GetDialogBaseUnits.
	if (!pCurrentFont)
		m_nAvgCharWidth = LOWORD(GetDialogBaseUnits());

	// if not using system font, select font into dc and 
	// calculate average char width
	else
	{		
		// have to select object into the dc before 
		// we can calculate width of string
		CFont* pOldFont = pDC->SelectObject(pCurrentFont);

		// get width of string
		CSize size = pDC->GetTextExtent(m_strCharSet, 
			m_strCharSet.GetLength());

		// calculate average char width and round result
		m_nAvgCharWidth = MulDiv(1, size.cx, m_strCharSet.GetLength());

		// select old font back into dc
		pDC->SelectObject(pOldFont);
	}
}

// calculate tabs from maximum column array
void CTCList::CalculateTabs()
{
	UINT nIndex;	// index to array

	// first tab setting is the fist col width
	m_aTabs[0] = ((m_aColWidth[0]+(m_nAvgCharWidth*m_nSpacing)) * 4) / 
		m_nAvgCharWidth;

	// calculate each tab setting
	for (nIndex=1; nIndex < m_nTabs; nIndex++)
		m_aTabs[nIndex] = m_aTabs[nIndex-1] + 
		((m_aColWidth[nIndex]+(m_nAvgCharWidth*m_nSpacing)) * 4) / 
			m_nAvgCharWidth;
}

// re-calculate maximum column widths
void CTCList::Recalc()
{
	// we are totally recalculating, so reset size of arrays
	m_aTabs.SetSize(MIN_TAB_SIZE);		// reset tab stop array size
	m_aColWidth.SetSize(MIN_TAB_SIZE);	// reset col width array size
	m_nTabs = MIN_TAB_SIZE;			// set number of tab settings
	InitColWidth();					// init col width array

	// loop through list box and calculate col widths for each string
	CString str;
	for (int nItem = GetCount() - 1; nItem >= 0; --nItem)
	{
		GetText(nItem, str);	// get list box item string 
		CalculateColWidths(str);	// calculate column widths of string
	}

	CalculateTabs();			// calculate tab settings
}

// initialize column width to 0
void CTCList::InitColWidth()
{
	// initialize col width settings to a length of 0
	UINT nIndex;
	for (nIndex=0; nIndex < m_nTabs; nIndex++)
		m_aColWidth[nIndex] = 0;
}


// calculate column widths of string
BOOL CTCList::CalculateColWidths(LPCSTR pString, BOOL bSetWidths)
{
	BOOL bMaxColumn = FALSE;	// returns if this string contains max column
	CDC* pDC = GetDC();		// get dc of list box window
	UINT nCol=0;			// column currently processing
	
	if (pDC)
	{
		// calculate average char width
		CalculateAvgCharWidth(pDC);		

		// select current font into dc so can measure width of string
		CFont* pOldFont = pDC->SelectObject(GetFont());

		CString strRow;		// row string
		CString strCol;		// column string
		CSize size;		// size of column string
		int nIndex=0;		// index to row string
			
		strRow = pString;	// set row string to passed string

		// divide row string up into column strings
		while (nIndex != -1)
		{
			// check if need to grow tab setting and column width
			if (nCol+1 > m_nTabs)
			{
				m_aTabs.SetSize(m_nTabs+1);
				m_aColWidth.SetSize(m_nTabs+1);
				m_nTabs += 1;
			}

			// parse out column string from row string
			if ((nIndex = strRow.Find('\t')) == -1)
				strCol = strRow;
			else
			{
				strCol = strRow.Left(nIndex);
				strRow = strRow.Mid(nIndex+1);	
			}

			// get pixel width of column string			
			size = pDC->GetTextExtent(strCol, strCol.GetLength());

			// see if this is the widest column string
			if ((UINT)size.cx >= m_aColWidth[nCol])
			{
				// this string contains a longest column string
				bMaxColumn = TRUE;	

				// store information to column width array 
   				if (bSetWidths)
					m_aColWidth[nCol] = size.cx;
			}
			
			nCol++;	// move to the next column
		} 

		pDC->SelectObject(pOldFont);		
		ReleaseDC(pDC);						
	}
		
	return bMaxColumn;
}


// process the LB_INSERTSTRING message
LRESULT CTCList::OnInsertString(WPARAM wParam, LPARAM lParam)
{
	// adjust the col width array for this string
	BOOL bRet = CalculateColWidths((LPCSTR)lParam);

	// set tab stop if required
	if (bRet)
	{
		CalculateTabs();			// adjust the tab setting array
	   	SetTabStops(m_nTabs, 
	   		(LPINT)&m_aTabs[0]);	// set tab stops 
	}

	return Default();
}


// process the LB_ADDSTRING message
LRESULT CTCList::OnAddString(WPARAM wParam, LPARAM lParam)
{
	// adjust the col width array for this string
	BOOL bRet = CalculateColWidths((LPCSTR)lParam);

	// set tab stop if required
	if (bRet)
	{
		CalculateTabs();			// adjust the tab setting array
	   	SetTabStops(m_nTabs, (LPINT)&m_aTabs[0]);	// set tab stops 
	}

	return Default();
}


// process the LB_DELETESTRING message
LRESULT CTCList::OnDeleteString(WPARAM wParam, LPARAM lParam)
{
	CString	str;				
	GetText(wParam, str);	

	// see if need to recalculate tab stops
	BOOL bRecalc = CalculateColWidths(str, FALSE);

	// delete string from list box
	// pass message along to list box proc
  	LRESULT lRet = Default();

	// recalculate tab stops if required
	if (bRecalc)
	{
		Recalc();
		SetTabStops(m_nTabs, (LPINT)&m_aTabs[0]);
	}

	return lRet;		
}

