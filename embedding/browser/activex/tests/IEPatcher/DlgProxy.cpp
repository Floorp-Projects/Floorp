// DlgProxy.cpp : implementation file
//

#include "stdafx.h"
#include "IEPatcher.h"
#include "DlgProxy.h"
#include "IEPatcherDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CIEPatcherDlgAutoProxy

IMPLEMENT_DYNCREATE(CIEPatcherDlgAutoProxy, CCmdTarget)

CIEPatcherDlgAutoProxy::CIEPatcherDlgAutoProxy()
{
	EnableAutomation();
	
	// To keep the application running as long as an OLE automation 
	//	object is active, the constructor calls AfxOleLockApp.
	AfxOleLockApp();

	// Get access to the dialog through the application's
	//  main window pointer.  Set the proxy's internal pointer
	//  to point to the dialog, and set the dialog's back pointer to
	//  this proxy.
	ASSERT (AfxGetApp()->m_pMainWnd != NULL);
	ASSERT_VALID (AfxGetApp()->m_pMainWnd);
	ASSERT_KINDOF(CIEPatcherDlg, AfxGetApp()->m_pMainWnd);
	m_pDialog = (CIEPatcherDlg*) AfxGetApp()->m_pMainWnd;
	m_pDialog->m_pAutoProxy = this;
}

CIEPatcherDlgAutoProxy::~CIEPatcherDlgAutoProxy()
{
	// To terminate the application when all objects created with
	// 	with OLE automation, the destructor calls AfxOleUnlockApp.
	//  Among other things, this will destroy the main dialog
	AfxOleUnlockApp();
}

void CIEPatcherDlgAutoProxy::OnFinalRelease()
{
	// When the last reference for an automation object is released
	// OnFinalRelease is called.  The base class will automatically
	// deletes the object.  Add additional cleanup required for your
	// object before calling the base class.

	CCmdTarget::OnFinalRelease();
}

BEGIN_MESSAGE_MAP(CIEPatcherDlgAutoProxy, CCmdTarget)
	//{{AFX_MSG_MAP(CIEPatcherDlgAutoProxy)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BEGIN_DISPATCH_MAP(CIEPatcherDlgAutoProxy, CCmdTarget)
	//{{AFX_DISPATCH_MAP(CIEPatcherDlgAutoProxy)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_DISPATCH_MAP
END_DISPATCH_MAP()

// Note: we add support for IID_IIEPatcher to support typesafe binding
//  from VBA.  This IID must match the GUID that is attached to the 
//  dispinterface in the .ODL file.

// {A6031677-3B36-11D2-B44D-00600819607E}
static const IID IID_IIEPatcher =
{ 0xa6031677, 0x3b36, 0x11d2, { 0xb4, 0x4d, 0x0, 0x60, 0x8, 0x19, 0x60, 0x7e } };

BEGIN_INTERFACE_MAP(CIEPatcherDlgAutoProxy, CCmdTarget)
	INTERFACE_PART(CIEPatcherDlgAutoProxy, IID_IIEPatcher, Dispatch)
END_INTERFACE_MAP()

// The IMPLEMENT_OLECREATE2 macro is defined in StdAfx.h of this project
// {A6031675-3B36-11D2-B44D-00600819607E}
IMPLEMENT_OLECREATE2(CIEPatcherDlgAutoProxy, "IEPatcher.Application", 0xa6031675, 0x3b36, 0x11d2, 0xb4, 0x4d, 0x0, 0x60, 0x8, 0x19, 0x60, 0x7e)

/////////////////////////////////////////////////////////////////////////////
// CIEPatcherDlgAutoProxy message handlers
