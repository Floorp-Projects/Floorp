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

// dlghtmrp.cpp : implementation file
//

#include "stdafx.h"
#include "msgcom.h"
#include "dlghtmrp.h"
#include "nethelp.h"
#include "xp_help.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////////////
///CListBoxRecipients list box for Prefers HTML and Does not Prefer HTML

CListBoxRecipients::CListBoxRecipients(): CListBox()
{
}

int CListBoxRecipients::CompareItem(LPCOMPAREITEMSTRUCT lpCompareItemStruct)
{
	int value1 = ((MSG_RecipientList*)(lpCompareItemStruct->itemData1))->value;
	int value2 = ((MSG_RecipientList*)(lpCompareItemStruct->itemData2))->value;

	if (value1 < value2)
	{
		return -1; //sorts before
	}
	else if (value1 == value2)
		{	
			return  0; //sorts same
		}	
	else 
	{
		return 1;   //sorts after
	}
}

void CListBoxRecipients::DeleteItem( LPDELETEITEMSTRUCT lpDeleteItemStruct)
{
	//not implemented
}

void CListBoxRecipients::SetColumnPositions(int iPosIndex, int iPosName, int iPosStatus)
{
	m_iPosIndex = iPosIndex;
	m_iPosName = iPosName;
	m_iPosStatus = iPosStatus;
}


void CListBoxRecipients::DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct )
{
	HDC hDC = lpDrawItemStruct->hDC;
	RECT rcItem = lpDrawItemStruct->rcItem;
	MSG_RecipientList *itemData = (MSG_RecipientList *) lpDrawItemStruct->itemData;
	HBRUSH hBrushFill = NULL;
	COLORREF oldBk, oldText;

	if ( lpDrawItemStruct->itemState & ODS_SELECTED ) {
		hBrushFill = ::CreateSolidBrush( GetSysColor( COLOR_HIGHLIGHT ) );
		oldBk = ::SetBkColor( hDC, GetSysColor( COLOR_HIGHLIGHT ) );
		oldText = ::SetTextColor( hDC, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
	} else {
		hBrushFill = ::CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
		oldBk = ::SetBkColor( hDC, GetSysColor( COLOR_WINDOW ) );
		oldText = ::SetTextColor( hDC, GetSysColor( COLOR_WINDOWTEXT ) );
	}

	if ( lpDrawItemStruct->itemID != -1 && itemData) {
		RECT rcTemp = rcItem;
		::FillRect( hDC, &rcItem, hBrushFill );

		::DrawText( hDC, itemData->name, -1, 
					&rcTemp, DT_VCENTER|DT_LEFT );
	}

	if ( lpDrawItemStruct->itemAction & ODA_FOCUS ) {
		::DrawFocusRect( hDC, &lpDrawItemStruct->rcItem );
	}

	if (hBrushFill)
		VERIFY( ::DeleteObject( hBrushFill ) );

	::SetBkColor( hDC, oldBk );
	::SetTextColor( hDC, oldText ); 
}

/////////////////////////////////////////////////////////////////////////////
// CHtmlRecipientsDlg dialog

CHtmlRecipientsDlg::CHtmlRecipientsDlg(MSG_Pane* pComposePane, 
   								       MSG_RecipientList* nohtml,
								       MSG_RecipientList* htmlok,
								       CWnd* pParent /*=NULL*/)
	: CDialog(CHtmlRecipientsDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CHtmlRecipientsDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	m_pComposePane = pComposePane;
	m_pNoHtml = nohtml;
	m_pHtmlOk = htmlok;
}


void CHtmlRecipientsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CHtmlRecipientsDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CHtmlRecipientsDlg, CDialog)
	//{{AFX_MSG_MAP(CHtmlRecipientsDlg)
	ON_BN_CLICKED(IDC_HELP_RECIPIENTS, OnHelp)
	ON_BN_CLICKED(ID_BTN_ADD, OnBtnAdd)
	ON_BN_CLICKED(ID_BTN_REMOVE, OnBtnRemove)
	ON_LBN_SETFOCUS(IDC_LIST1, OnSetfocusList1)
	ON_LBN_SETFOCUS(IDC_LIST2, OnSetfocusList2)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CHtmlRecipientsDlg message handlers

void CHtmlRecipientsDlg::OnSetfocusList1() 
{
	// TODO: Add your control notification handler code here
	CWnd *pWnd = GetDlgItem(ID_BTN_ADD);
	if (pWnd)
		pWnd->EnableWindow(TRUE);
	pWnd = GetDlgItem(ID_BTN_REMOVE);
	if (pWnd)
		pWnd->EnableWindow(FALSE);
	
}



void CHtmlRecipientsDlg::OnSetfocusList2() 
{
	CWnd *pWnd = GetDlgItem(ID_BTN_REMOVE);
	if (pWnd)
		pWnd->EnableWindow(TRUE);
	pWnd = GetDlgItem(ID_BTN_ADD);
	if (pWnd)
		pWnd->EnableWindow(FALSE);
	
}


void CHtmlRecipientsDlg::OnCancel() 
{
	MSG_ResultsRecipients(m_pComposePane,
						 TRUE,
						 0, /* List of IDs, terminated
										  with a negative entry. */
						 0/* Another list of IDs. */
						 );
	CDialog::OnCancel();
}


void CHtmlRecipientsDlg::OnOK() 
{
	BOOL bFailure = FALSE;

	int nCount1 = m_ListBox1.GetCount();
	int nCount2 = m_ListBox2.GetCount();

	int32 *pNoHTML = new int32[nCount1 +1];
	int32 *pHtmlOK = new int32[nCount2 +1];
    
	MSG_RecipientList *itemData = NULL;

	if (!pNoHTML || !pHtmlOK)
		bFailure = TRUE;
	else
	{
		//get the first list
		for ( int i = 0; i < nCount1; i++)
		{
			itemData = (MSG_RecipientList *)m_ListBox1.GetItemData(i);
			if (itemData)
				pNoHTML[i] = itemData->value;
			else
			{
				bFailure = TRUE;
				break;
			}
		}
		//terminate the array
		if (!bFailure)
			pNoHTML[nCount1] = -1;

		
		//get the second list
		for ( i = 0; i < nCount2; i++)
		{
			itemData = (MSG_RecipientList *)m_ListBox2.GetItemData(i);
			if (itemData)
				pHtmlOK[i] = itemData->value;
			else
			{
				bFailure = TRUE;
				break;
			}
		}
		//terminate the second array
		if (!bFailure)
			pHtmlOK[nCount2] = -1;

	}



	if (!bFailure)
	{	//send back the lists
		MSG_ResultsRecipients(m_pComposePane,
						 FALSE,
						 pNoHTML, /* List of IDs, terminated
									  with a negative entry. */
						 pHtmlOK/* Another list of IDs. */
						 );
		CDialog::OnOK();
	}
	else
	{
		MSG_ResultsRecipients(m_pComposePane,
						 TRUE,
						 0, /* List of IDs, terminated
									  with a negative entry. */
						 0/* Another list of IDs. */
						 );
		CDialog::OnOK();
		//Failed to modify recipients list!
	}
	//clean up the lists
	if (pNoHTML && pHtmlOK)
	{
		delete [] pNoHTML;
		delete [] pHtmlOK;
	}
}

void CHtmlRecipientsDlg::OnHelp() 
{
	NetHelp(HELP_HTML_MAIL_QUESTION_RECIPIENT);	
}

void CHtmlRecipientsDlg::OnBtnAdd() 
{
	CWnd *pWnd = GetDlgItem(ID_BTN_ADD);
	CWnd *pListWnd2 = GetDlgItem(IDC_LIST2);

	if (pWnd)
	{
		if (pWnd->IsWindowEnabled())
		{
			int nCount = m_ListBox1.GetSelCount();
			if (nCount <= 0 || nCount == LB_ERR) 
				return;
			
			LPINT lpIndexes = new int[nCount];
			
			int err = m_ListBox1.GetSelItems(nCount,lpIndexes);
			
			if (err == LB_ERR)
			{
				if (lpIndexes)
					delete  [] lpIndexes;
				return;
			}
			if (lpIndexes && pListWnd2)
			{
				MSG_RecipientList* itemData = NULL;
				for (int i = 0; i < nCount; i++)
				{
					itemData = (MSG_RecipientList*)m_ListBox1.GetItemData(lpIndexes[i]);
					if (itemData)
					{
						::SendMessage( pListWnd2->m_hWnd, LB_ADDSTRING, (WPARAM) 0, (LPARAM) itemData);
					}
				}

				for ( i = nCount -1 ; i >= 0; i--)
				{
					::SendMessage(m_ListBox1.m_hWnd, LB_DELETESTRING, lpIndexes[i], (LPARAM) NULL);
				}

				delete  [] lpIndexes;
			}
		}
	}
}

void CHtmlRecipientsDlg::OnBtnRemove() 
{
	CWnd *pWnd = GetDlgItem(ID_BTN_REMOVE);
	CWnd *pListWnd1 = GetDlgItem(IDC_LIST1);

	if (pWnd)
	{
		if (pWnd->IsWindowEnabled())
		{
			int nCount = m_ListBox2.GetSelCount();
			if (nCount <= 0 || nCount == LB_ERR) 
				return;
			
			LPINT lpIndexes = new int[nCount];
			
			int err = m_ListBox2.GetSelItems(nCount,lpIndexes);
			
			if (err == LB_ERR)
			{
			    if (lpIndexes)
					delete [] lpIndexes;
				return;
			}
			if (lpIndexes && pListWnd1)
			{
				MSG_RecipientList* itemData = NULL;
				for (int i = 0; i < nCount; i++)
				{
					itemData = (MSG_RecipientList*)m_ListBox2.GetItemData(lpIndexes[i]);
					if (itemData)
					{
						::SendMessage( pListWnd1->m_hWnd, LB_ADDSTRING, (WPARAM) 0, (LPARAM) itemData);
					}

				}

				for ( i = nCount -1 ; i >= 0; i--)
				{
					::SendMessage(m_ListBox2.m_hWnd, LB_DELETESTRING, lpIndexes[i], (LPARAM) NULL);

				}

				delete [] lpIndexes;
			}
		}
	}
	
}

BOOL CHtmlRecipientsDlg::OnInitDialog() 
{
	BOOL ret = CDialog::OnInitDialog();
	
	m_ListBox1.SubclassDlgItem( IDC_LIST1, this );
	m_ListBox2.SubclassDlgItem( IDC_LIST2, this);

	RECT rcText;
	int iPosName;

	::SetRect( &rcText, 0, 0, 64, 64 );

	iPosName = rcText.right;;

	m_ListBox1.SetColumnPositions( 0, iPosName, 0 );
	m_ListBox2.SetColumnPositions( 0, iPosName, 0 );
	
	PopulateLists();
	return TRUE;  
}

BOOL CHtmlRecipientsDlg::PopulateLists()
{

	CWnd *pList1 = (CListBox *)GetDlgItem(IDC_LIST1);
	CWnd *pList2 = (CListBox *)GetDlgItem(IDC_LIST2);

	if (!pList1 || !pList2)
		return 0;

	int i = 0; //array index into recipient list
	int err;
	for (; m_pNoHtml[i].name != NULL; i++)
	{
		if (m_pNoHtml[i].value == -1 || m_pNoHtml[i].name == NULL)
			break;

		err = ::SendMessage( pList1->m_hWnd, LB_ADDSTRING, (WPARAM) 0, (LPARAM) &(m_pNoHtml[i]));
	}

	//i is reset in the for loop
	for (i = 0;	m_pHtmlOk[i].name != NULL; i++)
	{
		if (m_pHtmlOk[i].value == -1 || m_pHtmlOk[i].name == NULL)
			break;
		err = ::SendMessage( pList2->m_hWnd, LB_ADDSTRING, (WPARAM) 0, (LPARAM) &(m_pHtmlOk[i]));
	}
	return TRUE;

}


//Used to launch the dialog when passed in as a callback
int CreateRecipientsDialog(MSG_Pane* composepane, void* closure,
								  MSG_RecipientList* nohtml,
								  MSG_RecipientList* htmlok,
								  void *pWnd)
{
	CHtmlRecipientsDlg rRecipientsDlg(composepane, nohtml, htmlok, (CWnd*)pWnd);

	int ret = rRecipientsDlg.DoModal();

	return ret;
}

