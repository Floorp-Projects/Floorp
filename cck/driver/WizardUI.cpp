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


// WizardUI.cpp : implementation file
//

#include "stdafx.h"
#include "afxmt.h"

#include "WizardMachine.h"
#include "fstream.h"
#include "ImgDlg.h"
#include "SumDlg.h"
#include "NavText.h"
#include "NewDialog.h"
#include "NewConfigDialog.h"
#include "ProgDlgThread.h"
#include "PropSheet.h"
#include "WizardUI.h"
#include "Interpret.h"

#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
// The following is included to make 
// the browse for a dir code compile
#include <shlobj.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CWizardUI property page
extern CWizardMachineApp theApp;
extern CInterpret *theInterpreter;
extern NODE *CurrentNode;
extern HBITMAP hBitmap;
extern CString Path;
extern char iniFilePath[MAX_SIZE];

extern BOOL inNext;
extern BOOL inPrev;
extern NODE* WizardTree;
extern WIDGET GlobalWidgetArray[1000];
extern int GlobalArrayIndex;
extern char currDirPath[MAX_SIZE];
extern char customizationPath[MAX_SIZE];

extern BOOL IsNewValue;

extern _declspec (dllimport) WIDGET ptr_ga[1000];
CCriticalSection nextSyncCodeSegment;
CCriticalSection prevSyncCodeSegment;

CSingleLock nextLock(&nextSyncCodeSegment);
CSingleLock prevLock(&prevSyncCodeSegment);

BOOL isBuildInstaller;
BOOL isCDLayoutCreated;

//extern CProgDialog myProgDialog;

IMPLEMENT_DYNCREATE(CWizardUI, CPropertyPage)

CWizardUI::CWizardUI() : CPropertyPage(CWizardUI::IDD)
{
	//{{AFX_DATA_INIT(CWizardUI)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CWizardUI::~CWizardUI()
{
}

void CWizardUI::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CWizardUI)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CWizardUI, CPropertyPage)
	//{{AFX_MSG_MAP(CWizardUI)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_CTLCOLOR()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWizardUI message handlers

BOOL CWizardUI::OnSetActive() 
{
	char* TempTitle = (char *)(LPCTSTR)(CurrentNode->localVars->title);
	CString WizTitle = theInterpreter->replaceVars(TempTitle,NULL);
	SetForegroundWindow();
	(AfxGetMainWnd( ))->SetWindowText(WizTitle);
	if (!(CurrentNode->isWidgetsSorted)) {
		SortWidgetsForTabOrder();
	}

	CreateControls();
	DisplayControls();

	CPropSheet* pSheet = (CPropSheet*) GetParent();
	ASSERT_VALID(pSheet);

	pSheet->GetDlgItem(ID_WIZBACK)->SetWindowText(CurrentNode->localVars->wizbut->back);
	pSheet->GetDlgItem(ID_WIZNEXT)->SetWindowText(CurrentNode->localVars->wizbut->next);
	pSheet->GetDlgItem(IDCANCEL)->SetWindowText(CurrentNode->localVars->wizbut->cancel);

	// Using the ini files to set the value as mentioned above 
	//	instead of using the commented out code below 

	// !!! Use an OnEnter for this instead !!!
/*	if (CurrentNode->localVars->functionality == "BuildInstallers")
	{
		pSheet->GetDlgItem(ID_WIZNEXT)->SetWindowText("Build &Installers");
		isBuildInstaller = TRUE;
	}
	else { 
		isBuildInstaller = FALSE;
		pSheet->GetDlgItem(ID_WIZNEXT)->SetWindowText("&Next >");
	}
*/
	if (theApp.IsLastNode(CurrentNode)) {
		pSheet->SetWizardButtons(PSWIZB_BACK | PSWIZB_FINISH);
	}
	else if (theApp.IsFirstNode(CurrentNode)) {
		pSheet->SetWizardButtons(PSWIZB_NEXT);
	}
	else {
		pSheet->SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);
	}

	// TODO: Add your specialized code here and/or call the base class
//	SetModified(1);
	return CPropertyPage::OnSetActive();
}


BOOL CWizardUI::OnKillActive() 
{
	// TODO: Add your specialized code here and/or call the base class

	return CPropertyPage::OnKillActive();
}


LRESULT CWizardUI::OnWizardBack() 
{
	// TODO: Add your specialized code here and/or call the base class
	if (!prevLock.IsLocked())
	{
		prevLock.Lock();

		UpdateGlobals();
		DestroyCurrentScreenWidgets();
		while (!theApp.GoToPrevNode())
			0; /* do nothing */

		prevLock.Unlock();
	}
	
	return CPropertyPage::OnWizardBack();
}

LRESULT CWizardUI::OnWizardNext() 
{
	// TODO: Add your specialized code here and/or call the base class
	if (!nextLock.IsLocked())
	{
		nextLock.Lock();

		if (isBuildInstaller) {
			isCDLayoutCreated = FALSE;

			/**
			VERIFY(hModule = ::LoadLibrary("IBEngine.dll"));
			VERIFY(
				pMyDllPath =
					(MYDLLPATH*)::GetProcAddress(
					(HMODULE) hModule, "SetPath")
			);

			(*pMyDllPath)((char*)(LPCTSTR)Path);
			LoadGlobals();

			VERIFY(
				pMyFunction =
				(MYFUNCTION*)::GetProcAddress(
				(HMODULE) hModule, "BuildInstallers")
			);
 			(*pMyFunction)();
			**/
			MessageBox("CD Image would be created", "OK", MB_OK);
			isBuildInstaller = FALSE;
		}

		UpdateGlobals();
		DestroyCurrentScreenWidgets();
		while (!theApp.GoToNextNode())
			0; /* do nothing */
	
		nextLock.Unlock();
	}
	
	return CPropertyPage::OnWizardNext();

}

void CWizardUI::OnPaint() 
{
	CPaintDC dc(this);
	//dc(this); // device context for painting
	
	// TODO: Add your message handler code here
	
	CRect rect(0, 0, 4, 8);
	MapDialogRect(&rect);

	int baseWidth = rect.Width();
	int baseHeight = rect.Height();

	if (containsImage) {
		CClientDC cdc(this);
		HBITMAP hbmpOld;
		CDC dcMem;

		dcMem.CreateCompatibleDC(&cdc);

		if (CurrentNode) {
			for(int i=0; i < CurrentNode->numImages; i++)
			{
				hbmpOld = (HBITMAP)::SelectObject(dcMem, CurrentNode->images[i]->hBitmap);

				dc.BitBlt((int)((float)(CurrentNode->images[i]->location.x) * (float)baseWidth / 4.0),
					(int)((float)(CurrentNode->images[i]->location.y) * (float)baseHeight / 8.0),
					(int)((float)(CurrentNode->images[i]->size.width) * (float)baseWidth / 4.0),
					(int)((float)(CurrentNode->images[i]->size.height) * (float)baseHeight / 8.0),
					&dcMem,  
					0, 
					0, 
					SRCCOPY);
			}
		}
	}

	// Do not call CPropertyPage::OnPaint() for painting messages
}

BOOL CWizardUI::ActCommand(WIDGET *curWidget)
{
	return TRUE;
}

BOOL CWizardUI::SortList(WIDGET *curWidget) 
{
#ifdef ACTUALLYNEEDTODOSOMETHINGLIKETHIS
	WIDGET* listWidget = theApp.findWidget((char*) (LPCTSTR)curWidget->target);
	int count = ((CListBox*)(listWidget->control))->GetCount();
	char* items[MAX_SIZE];

	for (int i = 0; i < count; i++) {
		items[i] = new char[MAX_SIZE];
		((CListBox*)(listWidget->control))->GetText(i, items[i]);
	}


	// Please use qsort() if this code becomes active again...
	
	if (curWidget->action.function == "SortByName") 
	{
	}
					
	else if (curWidget->action.function == "SortByPhone")
	{
	}

	((CListBox*)(listWidget->control))->ResetContent();
	for (int k = 0; k < count; k++) {
		((CListBox*)(listWidget->control))->AddString(CString(items[k]));
	}
#endif
	return TRUE;
}

BOOL CWizardUI::NewConfig(WIDGET *curWidget, CString globalsName) 
{
	// This doesn't really belong here...
	WIN32_FIND_DATA data;
	HANDLE d;

	CNewConfigDialog newDlg;
	newDlg.DoModal();
	CString configField = newDlg.GetConfigName();
	CString newDir = CString(customizationPath); 
	newDir += configField;

	d = FindFirstFile((const char *) newDir, &data);
	if (d == INVALID_HANDLE_VALUE)
		_mkdir(newDir);
	else
	{
		CWnd myWnd;
		myWnd.MessageBox("That configuration already exists.", "Error", MB_OK);
		return FALSE;
	}

					
	WIDGET* tmpWidget = theApp.findWidget((char*) (LPCTSTR)curWidget->target);
	if (!tmpWidget)
		return FALSE;

	/*
	CString tmpFunction = tmpWidget->action.function;
	CString params = theInterpreter->replaceVars(tmpWidget->action.parameters,NULL);
	theApp.GenerateList(tmpFunction, tmpWidget, params);	
	*/
	if (tmpWidget->action.onInit)
		theInterpreter->interpret(tmpWidget->action.onInit, curWidget);
					
	((CComboBox*)tmpWidget->control)->SelectString(0, configField);

	theApp.SetGlobal(globalsName, configField);

	return TRUE;
}

BOOL CWizardUI::BrowseFile(WIDGET *curWidget) 
{
	// This is to browse to a file
	CFileDialog fileDlg(TRUE, NULL, NULL, OFN_OVERWRITEPROMPT, NULL, NULL);
	int retVal = fileDlg.DoModal();
	CString fullFileName="";


	//Checking to see if the open file dialog did get a value or was merely cancelled.
	//If it was cancelled then the value of the edit box is not changed.
	if (fileDlg.GetPathName() != "")
	{	
		fullFileName = fileDlg.GetPathName();
		WIDGET* tmpWidget = theApp.findWidget((char*) (LPCTSTR)curWidget->target);
		if (tmpWidget && (CEdit*)tmpWidget->control)
			((CEdit*)tmpWidget->control)->SetWindowText(fullFileName);
	}
	return TRUE;
}

BOOL CWizardUI::BrowseDir(WIDGET *curWidget) 
{
	// The following code is used to browse to a dir
	// CFileDialog does not allow this
	
	BROWSEINFO bi;
	char szPath[MAX_PATH];
	char szTitle[] = "Select Directory";
	bi.hwndOwner = AfxGetMainWnd()->m_hWnd;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = (char*)malloc(MAX_PATH);
	bi.lpszTitle = szTitle;

	// Enable this line to browse for a directory
	bi.ulFlags = BIF_RETURNONLYFSDIRS;
				
	// Enable this line to browse for a computer
	bi.lpfn = NULL;
	bi.lParam = NULL;
	LPITEMIDLIST pidl= SHBrowseForFolder(&bi);


	if(pidl != NULL)
	{
		SHGetPathFromIDList(pidl,szPath);
		if( bi.ulFlags & BIF_BROWSEFORCOMPUTER )
		{
			// bi.pszDisplayName variable contains the computer name
		}
		else if( bi.ulFlags & BIF_RETURNONLYFSDIRS )
		{
			// szPath variable contains the path
			WIDGET* tmpWidget = theApp.findWidget((char*) (LPCTSTR)curWidget->target);
			if (tmpWidget)
				((CEdit*)tmpWidget->control)->SetWindowText(szPath);
		}
	 }
	
	free( bi.pszDisplayName );
	return TRUE;
}

BOOL CWizardUI::Progress() 
{
#ifdef SUPPORTINGIBPROGRESS
	CProgressDialog progressDlg(this);
	progressDlg.Create(IDD_PROGRESS_DLG);
	CProgressDialog *pProgressDlg = &progressDlg;
				
	//CRuntimeClass *pProgDlgThread = RUNTIME_CLASS(CProgDlgThread);	//This is the multi-threading stuff for the progress dialog
	//AfxBeginThread(pProgDlgThread);

	pProgressDlg->m_ProgressText.SetWindowText("Creating a CD Layout...");
	pProgressDlg->m_ProgressBar.SetPos(0);
	pProgressDlg->m_ProgressBar.SetRange(0,4);
	pProgressDlg->m_ProgressBar.SetStep(1);
				
	if (curWidget->action.dll == "IBEngine.dll") {
		VERIFY(hModule = ::LoadLibrary("IBEngine.dll"));

		VERIFY(
			pMyDllPath =
			(MYDLLPATH*)::GetProcAddress(
			(HMODULE) hModule, "SetPath")
		);

		(*pMyDllPath)((char*)(LPCTSTR)Path);

		pProgressDlg->m_ProgressText.SetWindowText("Loading Globals...");
		LoadGlobals();
		pProgressDlg->m_ProgressBar.StepIt();
		pProgressDlg->UpdateWindow();

		pProgressDlg->m_ProgressText.SetWindowText("Reading files...");
		ReadIniFile();
		pProgressDlg->m_ProgressBar.StepIt();
		pProgressDlg->UpdateWindow();

		pProgressDlg->m_ProgressText.SetWindowText("Merging files...");
		MergeFiles();
		pProgressDlg->m_ProgressBar.StepIt();
		pProgressDlg->UpdateWindow();

		pProgressDlg->m_ProgressText.SetWindowText("Creating CD Layout...");
		CreateMedia();
		pProgressDlg->m_ProgressBar.StepIt();
		pProgressDlg->UpdateWindow();

		MessageBox("CD Directory created", "OK", MB_OK);
	}
#endif

	return TRUE;
}

BOOL CWizardUI::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	// Get screen values exchanged
	UpdateData(TRUE);

	for(int i=0; i < CurrentNode->numWidgets; i++)
	{
		WIDGET* curWidget = CurrentNode->pageWidgets[i];
		if (curWidget->widgetID != (int)wParam) 
			continue;

		if (curWidget->action.onCommand)
			theInterpreter->interpret(curWidget->action.onCommand, curWidget);

		break;
	}

	return CPropertyPage::OnCommand(wParam, lParam);
}

// This is a comparison function for the TabOrder qsort() call
// Return -1 for "less than", 0 for "equals", and 1 for "greater than"
int TabSort(const void *w1, const void *w2)
{
	WIDGET *widget1 = *((WIDGET **) w1);
	WIDGET *widget2 = *((WIDGET **) w2);

	// Primary key is y coordinate
	if (widget1->location.y > widget2->location.y)
		return 1;

	if (widget1->location.y < widget2->location.y)
		return -1;

	// Primary key is equal, Secondary key is x coordinate
	return (widget1->location.x - widget2->location.x);
}

void CWizardUI::SortWidgetsForTabOrder()
{
#ifdef USEOLDSORTCODEINSTEADOFBUILTINFUNCTION
	// Sort on y-coordinate
	int x = 0;
	int y = 0;
	int count = CurrentNode->numWidgets;
	for (x = count-1; x >= 0; x--) {
		BOOL flipped = FALSE;

		for (int y = 0; y < x; y++) {
			WIDGET* widgetOne = CurrentNode->pageWidgets[y];
			WIDGET* widgetTwo = CurrentNode->pageWidgets[y+1];

			if (widgetOne->location.y > widgetTwo->location.y) 
			{
				WIDGET* T = widgetOne;
				CurrentNode->pageWidgets[y] = widgetTwo;
				CurrentNode->pageWidgets[y+1] = T;

				flipped = TRUE;
			}
		}

		if (!flipped) {
			break;
		}
	}

	// Sort on x-coordinate
	x = 0;
	y = 0;
	for (x = count-1; x >= 0; x--) {
		BOOL flipped = FALSE;

		for (int y = 0; y < x; y++) {
			WIDGET* widgetOne = CurrentNode->pageWidgets[y];
			WIDGET* widgetTwo = CurrentNode->pageWidgets[y+1];

			if (widgetOne->location.y == widgetTwo->location.y) 
			{
				if (widgetOne->location.x > widgetTwo->location.x) 
				{
					WIDGET* T = widgetOne;
					CurrentNode->pageWidgets[y] = widgetTwo;
					CurrentNode->pageWidgets[y+1] = T;
	
					flipped = TRUE;
				}
			}
		}

		if (!flipped) {
			break;
		}
	}
#endif

	qsort(CurrentNode->pageWidgets, CurrentNode->numWidgets, sizeof(WIDGET *), TabSort);
	CurrentNode->isWidgetsSorted = TRUE;
}

void CWizardUI::EnableWidget(WIDGET *curWidget) 
{
	// all controls are enabled by default, only do something if not enabled...
	int enabled = TRUE;
	if (curWidget->action.onInit)
	{
		CString enableStr = theInterpreter->replaceVars(curWidget->action.onInit,NULL);				
		// Cheat the interpret overhead since this is called a lot!
		if (enableStr == "Enable(0)")
			enabled = FALSE;
		curWidget->control->EnableWindow(enabled);
	}
}

void CWizardUI::CreateControls() 
{
	m_pFont = new CFont; 
	m_pFont->CreateFont(8, 0, 0, 0, FW_NORMAL,
					0, 0, 0, ANSI_CHARSET,
					OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS,
					DEFAULT_QUALITY,
					DEFAULT_PITCH|FF_DONTCARE,
					"MS Sans Serif");

	m_pNavFont = new CFont; 
	m_pNavFont->CreateFont(8, 0, 0, 0, FW_BOLD,
					0, 0, 0, ANSI_CHARSET,
					OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS,
					DEFAULT_QUALITY,
					DEFAULT_PITCH|FF_DONTCARE,
					"MS Sans Serif");

	//End Font Logic Start iniFile logic
	containsImage = FALSE;
	if (CurrentNode->numImages > 0) {
		containsImage = TRUE;
	}


	m_pControlCount = CurrentNode->numWidgets;

	for (int x = 0; x < m_pControlCount; x++) 
	{
		WIDGET* curWidget = CurrentNode->pageWidgets[x];
		CString widgetType = curWidget->type;
		int s_x = curWidget->location.x;
		int s_y = curWidget->location.y;
		int s_width = curWidget->size.width;
		int s_height = curWidget->size.height;
		int ID = curWidget->widgetID;

		CRect tmpRect = CRect(s_x, s_y, (s_x + s_width), (s_y + s_height));

		if (widgetType == "Text" || widgetType == "BoldText") {
			curWidget->control = new CStatic;
			((CStatic*)curWidget->control)->Create(curWidget->value, SS_LEFT, tmpRect, this, ID);
			EnableWidget(curWidget);
		}
		else if (widgetType == "Navigation Text") {
			curWidget->control = new CNavText;
			((CNavText*)curWidget->control)->Create(curWidget->value, SS_LEFT, tmpRect, this, ID);
			EnableWidget(curWidget);
		}
		else if (widgetType == "EditBox") {
			curWidget->control = new CEdit;//Added new style parameter ES_AUTOHSCROLL- to allow *GASP* SCROLLING!!
			if (((CEdit*)curWidget->control)->CreateEx(WS_EX_CLIENTEDGE, 
													_T("EDIT"), 
													NULL,
													WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |ES_AUTOHSCROLL ,
													curWidget->location.x,
													curWidget->location.y,
													curWidget->size.width,
													curWidget->size.height,
													m_hWnd, 0, 0 ))
			{
				//Set maximum number of characters allowed per line - limit set to 200
				((CEdit*)curWidget->control)->SetLimitText(int(curWidget->fieldlen.length));
				((CEdit*)curWidget->control)->SetWindowText(curWidget->value);
				EnableWidget(curWidget);
			}
		}
		else if (widgetType == "Button") {
			curWidget->control = new CButton;
			((CButton*)curWidget->control)->Create(curWidget->value, BS_PUSHBUTTON | WS_TABSTOP, tmpRect, this, ID);
			EnableWidget(curWidget);
		}
		else if (widgetType == "RadioButton") {
			curWidget->control = new CButton;
			((CButton*)curWidget->control)->Create(curWidget->title, BS_AUTORADIOBUTTON | WS_TABSTOP, tmpRect, this, ID);

			//char* widgetName = new char[sizeof(curWidget->name)];
			char widgetName[MID_SIZE];
			strcpy(widgetName, curWidget->name);

			CString theVal = theApp.GetGlobal(curWidget->group);

			//int newLineIndex = theVal.ReverseFind('\n');
			//if (newLineIndex > -1)
				//theVal.SetAt(newLineIndex, '\0');

			theVal.TrimRight();


			CString allOptions;
			CString setBack;
			allOptions = theApp.GetGlobalOptions(curWidget->group);

			char* options[MAX_SIZE];
			int numOfOptions = 0;
			char* s = new char[MAX_SIZE];
			s = strtok((char *) (LPCTSTR) allOptions, ",");
			if (s)
			{
				setBack = CString(s);
			}
			while (s) 
			{
				setBack = setBack + ",";
				options[numOfOptions] = new char[MAX_SIZE];
				strcpy(options[numOfOptions], s);
				s = strtok( NULL, "," );
				numOfOptions++;
				if (s)
				{
					setBack = setBack + CString(s);
				}
			}

			int index = atoi((char *)(LPCTSTR)theVal);
			index--;
	
			if (index < 0) {
				index = 0;
			}

			if (setBack != "") {
				setBack.SetAt(setBack.GetLength()-1, '\0');
			}

			WIDGET* rWidget = theApp.findWidget((char *) (LPCTSTR) curWidget->group);

			rWidget->items = setBack;

			int tmpVal = atoi((char *)(LPCTSTR)theVal)-1;
			
			if (tmpVal < 0) {
				tmpVal = 0;
			}
			

			if (strcmp(options[tmpVal], widgetName) == 0) { 
				((CButton*)curWidget->control)->SetCheck(1);
			}
			else {
				((CButton*)curWidget->control)->SetCheck(0);
			}
			EnableWidget(curWidget);
		}
		else if (widgetType == "CheckBox") {
			curWidget->control = new CButton;
			((CButton*)curWidget->control)->Create(curWidget->title, BS_AUTOCHECKBOX | WS_TABSTOP, tmpRect, this, ID);
			((CButton*)curWidget->control)->SetCheck(atoi(curWidget->value));
			EnableWidget(curWidget);
		}
		else if (widgetType == "ListBox") 
		{
			curWidget->control = new CListBox;
			((CListBox*)curWidget->control)->Create(LBS_STANDARD | LBS_MULTIPLESEL | WS_HSCROLL | WS_VSCROLL | WS_TABSTOP, tmpRect, this, ID);
			((CListBox*)curWidget->control)->ModifyStyleEx(NULL, WS_EX_CLIENTEDGE, 0);

			/*
			if (curWidget->action.onInit == "GenerateFileList" ||
				curWidget->action.onInit == "GenerateDirList")
			{
				CString ext = theInterpreter->replaceVars(curWidget->action.parameters,NULL);				
				theApp.GenerateList(curWidget->action.onInit, curWidget, ext);
			}
			*/
			if (curWidget->action.onInit)
				theInterpreter->interpret(curWidget->action.onInit, curWidget);
			else
			{
				for (int i = 0; i < curWidget->numOfOptions; i++) 
				{
					if (curWidget->options.value[i])
					{
						((CListBox*)curWidget->control)->AddString(curWidget->options.value[i]);
					}
				}
			}
		
			char* selectedItems;
			selectedItems = (char *) GlobalAlloc(0, 20 * sizeof(char));
			strcpy(selectedItems, (char *) (LPCTSTR) curWidget->value);

			char *s = strtok(selectedItems, ",");
			while (s) 
			{
				((CListBox*)curWidget->control)->SelectString(0, s);
				s = strtok( NULL, "," );
			}
			EnableWidget(curWidget);
		}
		else if (widgetType == "CheckListBox") 
		{
			curWidget->control = new CCheckListBox;
			((CCheckListBox*)curWidget->control)->Create(
				LBS_STANDARD | LBS_HASSTRINGS | BS_CHECKBOX |
				LBS_OWNERDRAWFIXED | WS_HSCROLL | WS_VSCROLL | 
				WS_TABSTOP, tmpRect, this, ID);
			((CCheckListBox*)curWidget->control)->ModifyStyleEx(NULL, WS_EX_CLIENTEDGE, 0);

			/*
			if (curWidget->action.onInit == "GenerateFileList" ||
				curWidget->action.onInit == "GenerateDirList")
			{
				CString ext = theInterpreter->replaceVars(curWidget->action.parameters, NULL);
				theApp.GenerateList(curWidget->action.onInit, curWidget, ext);
			}
			*/
			if (curWidget->action.onInit)
				theInterpreter->interpret(curWidget->action.onInit, curWidget);
			else
			{
				for (int i = 0; i < curWidget->numOfOptions; i++) 
				{
					if (curWidget->options.value[i])
					{
						((CCheckListBox*)curWidget->control)->AddString(curWidget->options.value[i]);
					}
				}
			}
		
			char* selectedItems;
			selectedItems = (char *) GlobalAlloc(0, 20 * sizeof(char));
			strcpy(selectedItems, (char *) (LPCTSTR) curWidget->value);

			int i;
			char *s = strtok(selectedItems, ",");
			while (s) 
			{
				i = ((CCheckListBox*)curWidget->control)->FindString(0, s);
				if (i != -1)
					((CCheckListBox*)curWidget->control)->SetCheck(i, 1);
				s = strtok( NULL, "," );
			}
			EnableWidget(curWidget);
		}
		else if (widgetType == "ComboBox") {
			curWidget->control = new CComboBox;
			((CComboBox*)curWidget->control)->Create(CBS_DROPDOWNLIST | WS_TABSTOP, tmpRect, this, ID);

			/*
			if (curWidget->action.onInit == "GenerateFileList" ||
				curWidget->action.onInit == "GenerateDirList")
			{
				CString ext = theInterpreter->replaceVars(curWidget->action.parameters,NULL);				
				theApp.GenerateList(curWidget->action.onInit, curWidget, ext);
			}
			*/
			if (curWidget->action.onInit)
				theInterpreter->interpret(curWidget->action.onInit, curWidget);
			else
			{
				for (int i = 0; i < curWidget->numOfOptions; i++) 
				{
					if (curWidget->options.value[i])
					{
						((CComboBox*)curWidget->control)->AddString(curWidget->options.value[i]);
					}
				}
			}
			EnableWidget(curWidget);

			//((CComboBox*)curWidget->control)->SelectString(0, selectedCustomization);
		}
		else if (widgetType == "GroupBox") {
			curWidget->control = new CButton;
			((CButton*)curWidget->control)->Create(curWidget->value, BS_GROUPBOX, tmpRect, this, ID);
			EnableWidget(curWidget);
		}
		else if (widgetType == "ProgressBar") {
			curWidget->control = new CProgressCtrl;
			((CProgressCtrl*)curWidget->control)->Create(WS_TABSTOP, tmpRect, this, ID);
			EnableWidget(curWidget);
		}


		// Set the font of the widget and increment the dynamically assigned ID value
		/*
		if ((curWidget->description == "Navigation Status") 
			|| (curWidget->description == "Current Page")) {
		*/
		if (curWidget->type == "BoldText")
		{
			curWidget->control->SetFont(m_pNavFont);
		}
		else if (curWidget->control)
		{
			curWidget->control->SetFont(m_pFont);
		}
	}
}

void CWizardUI::DisplayControls() 
{
	CRect rect(0, 0, 4, 8);
	MapDialogRect(&rect);

	int baseWidth = rect.Width();
	int baseHeight = rect.Height();

	extern NODE *CurrentNode;

	WIDGET* curWidget;
	for (int i = m_pControlCount-1; i >= 0; i--) {
		curWidget = CurrentNode->pageWidgets[i];
		
		if (curWidget->control)
		{
			curWidget->control->SetWindowPos(
				&wndTop,
				(int)((float)(curWidget->location.x) * (float)baseWidth / 4.0),
				(int)((float)(curWidget->location.y) * (float)baseHeight / 8.0),
				(int)((float)(curWidget->size.width) * (float)baseWidth / 4.0),
				(int)((float)(curWidget->size.height) * (float)baseHeight / 8.0),
				SWP_SHOWWINDOW);
		}
	}
}

void CWizardUI::DestroyCurrentScreenWidgets() 
{
	WIDGET* curWidget;
	for (int i = 0; i < m_pControlCount; i++) 
	{
		curWidget = CurrentNode->pageWidgets[i];
	
		if (curWidget->control)
		{
			BOOL retFalg = curWidget->control->DestroyWindow();
			delete curWidget->control;
			curWidget->control = NULL;
		}
	}
}

CString CWizardUI::GetScreenValue(WIDGET *curWidget) 
{
	//
	//  NOTE:  Assumes caller has already done UpdateData(TRUE);
	//

	CString widgetType = curWidget->type;
	CString rv("");

	if (widgetType == "CheckBox") {
		// Mask off everything but the checked/not checked state 
		// Ignore indeterminate state
		int state = ((CButton*)curWidget->control)->GetState() & 0x0003;
		if (state == 2) 
			state = 0;

		char temp[MIN_SIZE];

		itoa(state, temp, 10);

		rv = CString(temp);
	}
	else if (widgetType == "RadioButton")
	{
		// Mask off everything but the checked/not checked state 
		// Ignore indeterminate state
		int state = ((CButton*)curWidget->control)->GetState();
		if (state == 2) 
			state = 0;
			
		CString allOptions;
		CString setBack;
			
		WIDGET* rWidget = theApp.findWidget((char *) (LPCTSTR) curWidget->group);

		allOptions = rWidget->items;

		char* options[MAX_SIZE];
		int numOfOptions = 0;
		char* s = new char[MAX_SIZE];
		s = strtok((char *) (LPCTSTR) allOptions, ",");

		if (curWidget->name == CString(s) && state == 1)
		{
			rWidget->value = "1";
		}

		if (s)
		{
			setBack = CString(s);
		}

		int i=1;
		while (s) 
		{
			i++;
			setBack = setBack + ",";
			options[numOfOptions] = new char[MID_SIZE];
			strcpy(options[numOfOptions], s);
			s = strtok( NULL, "," );
				
			if (s)
			{
				setBack = setBack + CString(s);
			}

			char temp[MIN_SIZE];
		
			itoa(i, temp, 10);

			if (curWidget->name == CString(s) && state == 1)
			{
				rWidget->value = CString(temp);
			}
				
			numOfOptions++;
		}
	
		setBack.SetAt(setBack.GetLength()-1, '\0');

		rWidget->items = setBack;
	}
	else if (widgetType == "EditBox") {
		char myLine[MAX_SIZE];
		curWidget->control->GetWindowText(myLine, 250);

		CString line = (CString)myLine;
		rv = line;
	}
	else if (widgetType == "ListBox")
	{
		LPINT choices;

		int count;
		count = (((CListBox *)curWidget->control))->GetSelCount();
		choices = (int *) GlobalAlloc(0, count * sizeof(LPINT));
		((CListBox *)curWidget->control)->GetSelItems(count, choices);
			
		rv = "";
		CString temp;
		for (int i=0; i < count; i++)
		{
			((CListBox *)curWidget->control)->GetText(choices[i], temp);
			rv = rv + temp;
			if ( i+1 < count)
				rv += ",";
		}
	}
	else if (widgetType == "CheckListBox")
	{
		int count = (((CCheckListBox *)curWidget->control))->GetCount();
			
		rv = "";
		CString temp;
		for (int i=0; i < count; i++)
		{
			if (((CCheckListBox *)curWidget->control)->GetCheck(i))
			{
				((CCheckListBox *)curWidget->control)->GetText(i, temp);
				if ( i > 0)
					rv += ",";
				rv += temp;
			}
		}
	}
	else if (widgetType == "ComboBox")
	{
		int selectedIndex = ((CComboBox*)curWidget->control)->GetCurSel();
		if (selectedIndex != -1)
		{
			char tmpStr[MIN_SIZE];
			((CComboBox*)curWidget->control)->GetLBText(selectedIndex, tmpStr);
			rv = tmpStr;
		}
	}
	else
		rv = curWidget->value; // !!! Fix this so we're not copying strings all the time
								// Should be able to just pass in an "assign" boolean

	return rv;
}

void CWizardUI::UpdateGlobals() 
{
	UpdateData(TRUE);  // Get data from screen into controls
	
	
	WIDGET* curWidget;


	for (int i = 0; i < m_pControlCount; i++) 
	{
		curWidget = CurrentNode->pageWidgets[i];
		curWidget->value = GetScreenValue(curWidget);
	}
	IsNewValue = TRUE;
}

void CWizardUI::LoadGlobals()
{
	VERIFY(hGlobal = ::LoadLibrary("GlobalApi.dll"));
	VERIFY(
		pMySetGlobal =
		(MYSETGLOBAL*)::GetProcAddress(
		(HMODULE) hGlobal, "SetGlobal")
	);
	
	//CString animatedLogo = theApp.GetGlobal("AnimatedLogoURL");

	(*pMySetGlobal)("Platform", "32");
	(*pMySetGlobal)("Custom Inbox Path", (char*)(LPCTSTR)(Path+"IBDemo\\defaults\\inbox"));
	(*pMySetGlobal)("Overwrite CFG", "YES");
	(*pMySetGlobal)("Custom Readme Path", (char*)(LPCTSTR)(Path+"IBDemo\\defaults\\readme.txt"));
	(*pMySetGlobal)("Custom License Path", (char*)(LPCTSTR)(Path+"IBDemo\\defaults\\license.txt"));
	(*pMySetGlobal)("Custom CFG Path", (char*)(LPCTSTR)(Path+"IBDemo\\defaults\\netscape.cfg"));
	(*pMySetGlobal)("Custom Bookmark Path", (char*)(LPCTSTR)(Path+"IBDemo\\defaults\\bookmark.htm"));
	(*pMySetGlobal)("Custom Addressbook Path", (char*)(LPCTSTR)(Path+"IBDemo\\defaults\\pab.na2"));
	(*pMySetGlobal)("Install Splash Path", (char*)(LPCTSTR)(Path+"IBDemo\\source\\setup.bmp"));

	// The command line specifies the output path
	// If it is NULL, set a default output path
	UpdateData(TRUE);
	(*pMySetGlobal)("Output Path", (char*)(LPCTSTR)(Path+"Output"));
}

void CWizardUI::ReadIniFile() 
{
	// TODO: Add extra validation here
	
	VERIFY(
		pMyFunction =
		(MYFUNCTION*)::GetProcAddress(
		(HMODULE) hModule, "ReadMyFile")
	);
	
	(*pMyFunction)();
}

void CWizardUI::MergeFiles() 
{
	
	// TODO: Add your control notification handler code here

	VERIFY(
		pMyFunction =
		(MYFUNCTION*)::GetProcAddress(
		(HMODULE) hModule, "MergeFiles")
	);

	(*pMyFunction)();
}

void CWizardUI::CreateMedia() 
{
	
	// TODO: Add your control notification handler code here

	VERIFY(
		pMyFunction =
		(MYFUNCTION*)::GetProcAddress(
		(HMODULE) hModule, "CreateMedia")
	);

	(*pMyFunction)();
}

void CWizardUI::ExitDemo() 
{
	
	// TODO: Add extra cleanup here
	FreeLibrary(hModule);
	FreeLibrary(hGlobal);
}

BOOL CWizardUI::OnEraseBkgnd(CDC* pDC) 
{

	return 1;
}

HBRUSH CWizardUI::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) 
{
	HBRUSH hbr = CPropertyPage::OnCtlColor(pDC, pWnd, nCtlColor);

	// TODO: Return a different brush if the default is not desired
	return hbr;
}

BOOL CWizardUI::OnWizardFinish() 
{
	// TODO: Add your specialized code here and/or call the base class
	UpdateGlobals();

	if (CurrentNode->navControls->onNextAction)
		if (!theInterpreter->interpret(CurrentNode->navControls->onNextAction, NULL))
			return FALSE;
	return CPropertyPage::OnWizardFinish();
}

BOOL CWizardUI::OnApply() 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CPropertyPage::OnApply();
}

