/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols
#include "WizardTypes.h"


/////////////////////////////////////////////////////////////////////////////
// CInterpret:
// See Interpret.cpp for the implementation of this class
//
class CInterpret
{
public:
	CInterpret();
	~CInterpret();
	BOOL NewConfig(WIDGET *curWidget, CString globalsName, CString DialogTitle);
	BOOL BrowseFile(WIDGET *curWidget);
	CString BrowseDir(WIDGET *curWidget);
	void GenerateList(CString action, WIDGET* curWidget, CString ext);
	BOOL Progress();  // Not actually used right now
	BOOL ShowSection(WIDGET *curWidget);
	BOOL IterateListBox(char *parms);
	CString replaceVars(CString str, char *listval);
	CString replaceVars(char *str, char *listval);
	BOOL CallDLL(char *dll, char *proc, char *parms, WIDGET *curWidget);
	BOOL interpret(char *cmds, WIDGET *curWidget);
	BOOL interpret(CString cmds, WIDGET *curWidget);
	BOOL CInterpret::GetRegistryKey( HKEY key, const char *subkey, char *retdata );
	BOOL CInterpret::OpenBrowser(const char *url);
	BOOL CInterpret::OpenViewer(const char *url);
	CString CInterpret::GetTrimFile(CString filePath);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWizardMachineApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation
	DLLINFO m_DLLs;
};


/////////////////////////////////////////////////////////////////////////////

