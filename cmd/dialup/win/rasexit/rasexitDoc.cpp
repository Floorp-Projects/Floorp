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
// rasexitDoc.cpp : implementation of the CRasexitDoc class
//

#include "stdafx.h"
#include "rasexit.h"

#include "rasexitDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CRasexitDoc

IMPLEMENT_DYNCREATE(CRasexitDoc, CDocument)

BEGIN_MESSAGE_MAP(CRasexitDoc, CDocument)
	//{{AFX_MSG_MAP(CRasexitDoc)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BEGIN_DISPATCH_MAP(CRasexitDoc, CDocument)
	//{{AFX_DISPATCH_MAP(CRasexitDoc)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//      DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_DISPATCH_MAP
END_DISPATCH_MAP()

// Note: we add support for IID_IRasexi to support typesafe binding
//  from VBA.  This IID must match the GUID that is attached to the 
//  dispinterface in the .ODL file.

// {54680790-CB5B-11CF-893E-0800091AC64E}
static const IID IID_IRasexi =
{ 0x54680790, 0xcb5b, 0x11cf, { 0x89, 0x3e, 0x8, 0x0, 0x9, 0x1a, 0xc6, 0x4e } };

BEGIN_INTERFACE_MAP(CRasexitDoc, CDocument)
	INTERFACE_PART(CRasexitDoc, IID_IRasexi, Dispatch)
END_INTERFACE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRasexitDoc construction/destruction

CRasexitDoc::CRasexitDoc()
{
	// TODO: add one-time construction code here

	EnableAutomation();

	AfxOleLockApp();
}

CRasexitDoc::~CRasexitDoc()
{
	AfxOleUnlockApp();
}

BOOL CRasexitDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CRasexitDoc serialization

void CRasexitDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CRasexitDoc diagnostics

#ifdef _DEBUG
void CRasexitDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CRasexitDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CRasexitDoc commands
