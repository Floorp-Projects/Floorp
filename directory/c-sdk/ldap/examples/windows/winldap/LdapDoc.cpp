// LdapDoc.cpp : implementation of the LdapDoc class
//

#include "stdafx.h"
#include "winldap.h"

#include "LdapDoc.h"

#ifdef _DEBUG
#ifdef _WIN32
#define new DEBUG_NEW
#endif
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// LdapDoc

IMPLEMENT_DYNCREATE(LdapDoc, CDocument)

BEGIN_MESSAGE_MAP(LdapDoc, CDocument)
	//{{AFX_MSG_MAP(LdapDoc)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// LdapDoc construction/destruction

LdapDoc::LdapDoc()
{
}

LdapDoc::~LdapDoc()
{
}

BOOL LdapDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	SetTitle( "" );
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// LdapDoc serialization

void LdapDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
	}
	else
	{
	}
}

/////////////////////////////////////////////////////////////////////////////
// LdapDoc diagnostics

#ifdef _DEBUG
void LdapDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void LdapDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// LdapDoc commands
