/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
// muc.h : main header file for the MUC DLL

#ifndef __AFXWIN_H__
#error include 'stdafx.h' before including this file for PCH
#endif

/////////////////////////////////////////////////////////////////////////////
// CMucApp
// See muc.cpp for the implementation of this class
//

class CMucApp : public CWinApp
{
public:
	CMucApp();
	~CMucApp();
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMucApp)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CMucApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	virtual BOOL InitInstance();
	virtual BOOL ExitInstance();

	DECLARE_MESSAGE_MAP()

private:  
#ifdef WIN32
	OSVERSIONINFO *lpOsVersionInfo;    
#endif
};

/////////////////////////////////////////////////////////////////////////////
