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
