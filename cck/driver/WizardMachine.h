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

// WizardMachine.h : main header file for the WIZARDMACHINE application
//

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols
#include "WizardMachineDlg.h"

#define MIN_SIZE 256
#define MID_SIZE 512
#define MAX_SIZE 1024
#define EXTD_MAX_SIZE 10240

// Some global structures
typedef struct NVPAIR
{
	char name[MID_SIZE];
	char value[MID_SIZE];
	char options[MAX_SIZE];
	char type[MID_SIZE];
}NVPAIR;

typedef struct ACTIONSET
{
	CString event;
	CString dll;
	CString function;
	char parameters[MAX_SIZE];
}ACTIONSET;

typedef struct DIMENSION
{
	int width;
	int height;
}DIMENSION;

typedef struct OPTIONS
{
	char* name[25];
	char* value[25];
}OPTIONS;

typedef struct WIDGET
{
	CString type;
	CString name;
	CString value;
	CString title;
	CString group;
	CString target;
	CString description;
	POINT location;
	DIMENSION size;
	ACTIONSET action;
	int numOfOptions;
	OPTIONS options;
	CString items;
	BOOL cached;
	int widgetID;
	CWnd *control;
}WIDGET;


typedef struct IMAGE
{
	CString name;
	CString value;
	POINT location;
	DIMENSION size;	
	HBITMAP hBitmap;
}IMAGE;

typedef struct VARS
{
	CString title;
	CString caption;
	CString pageName;
	CString image;
	CString visibility;
	CString functionality;
}VARS;

typedef struct PAGE
{
	CStringArray pages;
	CStringArray visibility;
}PAGE;

typedef struct CONTROLS
{
	CString onNextAction;
	CString helpFile;
}CONTROLS;

typedef struct WIDGETGROUPS
{
	CString groupName;
	CString widgets;
}WIDGETGROUPS;

typedef struct NODE
{
	NODE *parent;
	NODE **childNodes;
	int numChildNodes;
	int currNodeIndex;
	VARS *localVars;
	PAGE *subPages;
	CONTROLS *navControls;
	WIDGET** pageWidgets;
	int numWidgets;
	int currWidgetIndex;
	int pageBaseIndex;
	IMAGE **images;
	int numImages;
	BOOL nodeBuilt;
	BOOL isWidgetsSorted;
}NODE;
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
	CString replaceVars(char *str);
	void GoToNextNode();
	void GoToPrevNode();
	void ExitApp();
	BOOL CheckForPreBuiltPath();
	FILE* OpenAFile(CString outputFile, CString mode);
	void PrintNodeInfo(NODE* node);
	void CheckIniFileExistence(CString file);
	BOOL FileExists(CString file);
	void FillGlobalWidgetArray(CString file);
	void FillGlobalWidgetArray();
	void CreateNewCache();
	BOOL IsLastNode(NODE* treeNode);
	BOOL IsFirstNode(NODE* treeNode);
	CString GetModulePath();
	CString GetGlobal(CString theName);
	WIDGET* SetGlobal(CString theName, CString theValue);
	CString GetGlobalOptions(CString theName);
	WIDGET* findWidget(char *name);
	void BuildWidget(WIDGET* aWidget, CString iniSection, CString iniFile, int pageBaseIndex, BOOL readValue);
//	void BuildHelpWidget(WIDGET* aWidget, CString iniSection, CString iniFile, int pageBaseIndex);
	void GenerateList(CString action, WIDGET* targetWidget, CString ext);
	void HelpWiz();
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWizardMachineApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CWizardMachineApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
