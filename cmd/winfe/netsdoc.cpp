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

// implementation of the CNetscapeDoc class
//

#include "stdafx.h"

#include "cntritem.h"
#include "feembed.h"
#include "netsdoc.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNetscapeDoc

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CNetscapeDoc, CGenericDoc)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CNetscapeDoc, CGenericDoc)
	//{{AFX_MSG_MAP(CNetscapeDoc)
	//}}AFX_MSG_MAP
	//	Enable default OLE container implementation
END_MESSAGE_MAP()

#ifndef _AFXDLL
#define new DEBUG_NEW  // MSVC Debugging new...goes to regular new in release mode
#endif

/////////////////////////////////////////////////////////////////////////////
// CNetscapeDoc construction/destruction

CNetscapeDoc::CNetscapeDoc()
{
}

CNetscapeDoc::~CNetscapeDoc()
{
}

/////////////////////////////////////////////////////////////////////////////
// CNetscapeDoc serialization

void CNetscapeDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
	
	//	Calling the base class CGenericDoc enables serialization
	//		of the conainer document's COleclientItem objects.
	CGenericDoc::Serialize(ar);
}


/////////////////////////////////////////////////////////////////////////////
// CNetscapeDoc diagnostics

#ifdef _DEBUG
void CNetscapeDoc::AssertValid() const
{
	CGenericDoc::AssertValid();
}

void CNetscapeDoc::Dump(CDumpContext& dc) const
{
	CGenericDoc::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CNetscapeDoc commands
