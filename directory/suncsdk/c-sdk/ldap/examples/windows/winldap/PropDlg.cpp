/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 * 
 * The contents of this file are subject to the Mozilla Public License Version 
 * 1.1 (the "License"); you may not use this file except in compliance with 
 * the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * 
 * The Original Code is Mozilla Communicator client code.
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1996-1999
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */

// PropDlg.cpp : implementation file
//

#include "stdafx.h"
#include "winldap.h"
#include "PropDlg.h"

#ifdef _DEBUG
#ifdef _WIN32
#define new DEBUG_NEW
#endif
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// PropDlg dialog


PropDlg::PropDlg(CWnd* pParent /*=NULL*/)
	: CDialog(PropDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(PropDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void PropDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(PropDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(PropDlg, CDialog)
	//{{AFX_MSG_MAP(PropDlg)
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// PropDlg message handlers

void PropDlg::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);
	
	if ( ::IsWindow( m_list.m_hWnd ) )
		m_list.SetWindowPos( &wndTop, 0, 0, cx, cy, SWP_NOZORDER );
}

BOOL PropDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	if ( m_title.GetLength() > 0 )
		SetWindowText( m_title );
	CRect rect;
	GetClientRect( &rect );
	m_list.Create( WS_CHILD|WS_VISIBLE|WS_VSCROLL|LBS_NOTIFY, rect, this, 5000 );
	
	POSITION pos;
	// Iterate through the list in head-to-tail order.
	for( pos = m_strings.GetHeadPosition(); pos != NULL; )
	{
		m_list.AddString( m_strings.GetNext( pos ) );
	}
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void PropDlg::AddLine( const char *str )
{
	m_strings.AddTail( str );
}
