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

// WizardMachine.h : main header file for the WIZARDMACHINE application
//

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols
//#include "WizardMachineDlg.h"
#include "WizardTypes.h"


//NODE WizardTree;
void CreateRshell();
//extern WIDGET GlobalWidgetArray[1000];

//extern int GlobalArrayIndex;

/////////////////////////////////////////////////////////////////////////////
// CWizardMachineApp:
// See WizardMachine.cpp for the implementation of this class
//
class CWizardMachineApp : public CWinApp
{
public:
	CWizardMachineApp();
	void InitializeTree(CString rootIniFile);
	NODE* CreateNode(NODE *parentNode, CString iniFileName);
	void CreateNode();
	void SetGlobalDefaults();
	void CreateFirstLeaf(NODE *parent, CString iniFile);
	BOOL SectionFound(CString iniFile, CString section);
	int  GetBufferCount(char *buffer);
	char* GetBufferElement(const char *buffer, int index);
	char* GetSectionBuffer(CString iniFile, CString section);
	void ExecuteAction(char action);
	BOOL GoToNextNode();
	BOOL GoToPrevNode();
	void ExitApp();
	BOOL CheckForPreBuiltPath();
	FILE* OpenAFile(CString outputFile, CString mode);
	void PrintNodeInfo(NODE* node);
	void CheckIniFileExistence(CString file);
	BOOL FileExists(CString file);
	BOOL FillGlobalWidgetArray(CString file);
	BOOL FillGlobalWidgetArray();
	void CreateNewCache();
	BOOL IsLastNode(NODE* treeNode);
	BOOL IsFirstNode(NODE* treeNode);
	CString GetGlobalOptions(CString theName);
	void BuildWidget(WIDGET* aWidget, CString iniSection, CString iniFile, int pageBaseIndex, BOOL readValue);
//	void BuildHelpWidget(WIDGET* aWidget, CString iniSection, CString iniFile, int pageBaseIndex);
	void HelpWiz();
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWizardMachineApp)
	public:
	virtual BOOL InitInstance();
  virtual int ExitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CWizardMachineApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
