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


// WizardUI.cpp : implementation file
//

#include "stdafx.h"
#include "afxmt.h"

#include "globals.h"
#include "WizardMachine.h"
#include "fstream.h"
#include "ImgDlg.h"
#include "SumDlg.h"
#include "NavText.h"
#include "NewDialog.h"
#include "ProgDlgThread.h"
#include "PropSheet.h"
#include "WizardUI.h"
#include "Interpret.h"

#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>

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
extern char currDirPath[MAX_SIZE];
extern char customizationPath[MAX_SIZE];

extern BOOL IsNewValue;
BOOL Validate = TRUE;
CString numericMessage;

extern _declspec (dllimport) WIDGET ptr_ga[1000];
CCriticalSection nextSyncCodeSegment;
CCriticalSection prevSyncCodeSegment;

CSingleLock nextLock(&nextSyncCodeSegment);
CSingleLock prevLock(&prevSyncCodeSegment);

BOOL isBuildInstaller;
BOOL isCDLayoutCreated;
UINT nID;
UINT wNotifyCode;

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
	//we used to check for a particular value previously and then set the values 
	// not doing it any more cuz - the wizard neednt know the values of the buttons in inifiles
	
	if (theApp.IsLastNode(CurrentNode)) 
	{
		pSheet->SetWizardButtons(PSWIZB_BACK | PSWIZB_FINISH);
		pSheet->GetDlgItem(ID_WIZFINISH)->SetWindowText(CurrentNode->localVars->wizbut->next);

	}
	else if (theApp.IsFirstNode(CurrentNode)) 
	{
		pSheet->SetWizardButtons(PSWIZB_NEXT);
	}
	else 
	{
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
	if (!Validate)
	{
		AfxMessageBox(numericMessage, MB_OK);
		UpdateGlobals();
		DestroyCurrentScreenWidgets();
		return FALSE;
	}
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
	if (!Validate)
	{
		AfxMessageBox(numericMessage, MB_OK);
		UpdateGlobals();
		DestroyCurrentScreenWidgets();
		return FALSE;
	}

	if (!nextLock.IsLocked())
	{
		nextLock.Lock();
#if 0
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
#endif	
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
	WIDGET* listWidget = findWidget((char*) (LPCTSTR)curWidget->target);
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

BOOL CWizardUI::SetDescription(WIDGET *w) 
{
	WIDGET *t = findWidget((char *) (LPCTSTR) w->target);
	if (!t || (t->type != "Text" && t->type != "BoldText" && t->type != "GlobalText"))
		return FALSE;

	int selected = 0;
	if (w->type == "ListBox")
		selected = ((CListBox *)w->control)->GetCurSel();
	else if (w->type == "CheckListBox")
		selected = ((CCheckListBox *)w->control)->GetCurSel();
	else if ((w->type == "ComboBox") || (w->type == "DropBox"))
		selected = ((CComboBox *)w->control)->GetCurSel();
	
	if (selected == -1 || selected >= w->numOfOptDesc)
		return FALSE;

	((CEdit *)t->control)->SetWindowText(w->optDesc.value[selected]);

	return TRUE;
}

BOOL CWizardUI::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	nID = LOWORD(wParam);
	wNotifyCode = HIWORD(wParam);
	WIDGET *w;

	// Get screen values exchanged
	UpdateData(TRUE);

	for(int i=0; i < CurrentNode->numWidgets; i++)
	{
		WIDGET* curWidget = CurrentNode->pageWidgets[i];
		if (curWidget->widgetID != nID) 
			continue;
		if (curWidget->type == "EditBox")
		{
			if (wNotifyCode == EN_KILLFOCUS)
				if(((CEdit*)curWidget->control)->GetModify())
				{
					if (((CString)(curWidget->action.onCommand)).GetLength() > 0)
					theInterpreter->interpret(curWidget->action.onCommand, curWidget);
//					AfxMessageBox("modified", MB_OK);
//					((CEdit*)curWidget->control)->SetModify(FALSE);

				}
		}
		else
		{
			if (curWidget->action.onCommand)
			{
				// Save any screen changes to affected widgets
				w = findWidget(curWidget->target);
				if (w)
					w->value = GetScreenValue(w);
				curWidget->value = GetScreenValue(curWidget);

				// Evaluate new state
				theInterpreter->interpret(curWidget->action.onCommand, curWidget);

				// Reflect new state into screen
				UpdateScreenWidget(curWidget);
				if (w)
					UpdateScreenWidget(w);
			}

			if (curWidget->numOfOptDesc > 0)
				SetDescription(curWidget);

			
		}
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
	qsort(CurrentNode->pageWidgets, CurrentNode->numWidgets, sizeof(WIDGET *), TabSort);
	CurrentNode->isWidgetsSorted = TRUE;
}

void CWizardUI::EnableWidget(WIDGET *curWidget) 
{
	// all controls are enabled by default, only do something if not enabled...
	int enabled = TRUE;
	if (!curWidget->action.onInit.IsEmpty())
	{
		// Cheat the interpret overhead since this is called a lot!
		CString enableStr = theInterpreter->replaceVars(curWidget->action.onInit,NULL);	
		if (enableStr == "Enable(0)")
			enabled = FALSE;
		else if (enableStr == "Enable2()")
			enabled = FALSE;
		curWidget->control->EnableWindow(enabled);
	}
}

void CWizardUI::UpdateScreenWidget(WIDGET *curWidget)
{
	int  selRv = CB_ERR;
	int	selectedSomething = FALSE;

	if(curWidget->type == "CheckListBox")
	{
		// This is needed because we don't save this value anywhere
		int selected = ((CCheckListBox*)curWidget->control)->GetCurSel();
		if (selected == CB_ERR)
			selected = 0;

		((CCheckListBox*)curWidget->control)->ResetContent();

		for (int i = 0; i < curWidget->numOfOptions; i++) 
			if (curWidget->options.value[i])
				((CCheckListBox*)curWidget->control)->AddString(curWidget->options.value[i]);

		if (curWidget->value && curWidget->value != "")
		{
			char indices[MAX_SIZE];
			int i;

			strcpy(indices, curWidget->value); 
			char *s = strtok(indices, ",");
			for (; s; s=strtok(NULL, ","))
			{
				i = ((CCheckListBox*)curWidget->control)->FindString(0, s);
				if (i != -1)
				{
					if (strcmp(s,"Navigator")==0)
					{
						((CCheckListBox*)curWidget->control)->SetCheck(i, 2);
						((CCheckListBox*)curWidget->control)->Enable(i, FALSE);
					}
					else
						((CCheckListBox*)curWidget->control)->SetCheck(i, 1);
				}
			}
		}

		selRv = ((CCheckListBox*)curWidget->control)->SetCurSel(selected);
		if (selRv == CB_ERR)
			((CCheckListBox*)curWidget->control)->SetCurSel(0);

		if (curWidget->numOfOptDesc > 0)
			SetDescription(curWidget);
	}
	if(curWidget->type == "ListBox")
	{
		((CListBox*)curWidget->control)->ResetContent();

		for (int i = 0; i < curWidget->numOfOptions; i++) 
			if (curWidget->options.value[i])
				((CListBox*)curWidget->control)->AddString(curWidget->options.value[i]);

		if (curWidget->value && curWidget->value != "")
		{
			char indices[MAX_SIZE];

			strcpy(indices, curWidget->value); 
			char *s = strtok(indices, ",");
			for (; s; s=strtok(NULL, ","))
			{
				selRv = ((CListBox*)curWidget->control)->SelectString(0, s);
				if (selRv != LB_ERR)
					selectedSomething = TRUE;
			}
		}
		if (!selectedSomething)
			selRv = ((CListBox*)curWidget->control)->SetCurSel(0);
	}
	else if((curWidget->type == "ComboBox") || (curWidget->type == "DropBox"))
	{
		((CComboBox*)curWidget->control)->ResetContent();

		for (int i = 0; i < curWidget->numOfOptions; i++) 
			if (curWidget->options.value[i])
				((CComboBox*)curWidget->control)->AddString(curWidget->options.value[i]);

		if (curWidget->value && curWidget->value != "")
		{
			selRv = ((CComboBox*)curWidget->control)->SelectString(-1, curWidget->value);

			CString rootPath1 = GetGlobal("Root");
			CString tpath1 = rootPath1 + "nscpxpiLinux";
			int direxist = GetFileAttributes(tpath1);
			if (strcmp((curWidget->value),"Windows") == 0)
			{
				WIDGET *widg = findWidget("LinuxPath");
				if (widg)
				widg->control->EnableWindow(FALSE);
				widg = findWidget("Text7888");
				if (widg)
				widg->control->EnableWindow(FALSE);
				widg = findWidget("Button9888");
				if (widg)
				widg->control->EnableWindow(FALSE);
			}
			else if ((strcmp(curWidget->value,"Linux")==0) && (direxist == -1))
			{
				WIDGET *widg = findWidget("LinuxPath");
				if (widg)
				widg->control->EnableWindow(TRUE);
				widg = findWidget("Text7888");
				if (widg)
				widg->control->EnableWindow(TRUE);
				widg = findWidget("Button9888");
				if (widg)
				widg->control->EnableWindow(TRUE);
			}
			else if ((strcmp(curWidget->value,"Linux")==0) && (direxist != -1))
			{
				WIDGET *widg = findWidget("LinuxPath");
				if (widg)
					widg->control->EnableWindow(FALSE);
				widg = findWidget("Text7888");
				if (widg)
					widg->control->EnableWindow(FALSE);
				widg = findWidget("Button9888");
				if (widg)
					widg->control->EnableWindow(FALSE);
			}		
		}

		if (selRv == CB_ERR)
			selRv = ((CComboBox*)curWidget->control)->SetCurSel(0);
	}
}

void CWizardUI::CreateControls() 
{
	int rv = TRUE;

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
		UINT ID = curWidget->widgetID;

		CRect tmpRect = CRect(s_x, s_y, (s_x + s_width), (s_y + s_height));

		if (widgetType == "Text" || widgetType == "BoldText") {
			curWidget->control = new CStatic;
			rv = ((CStatic*)curWidget->control)->Create(curWidget->value, SS_LEFT, tmpRect, this, ID);
		}
		else if (widgetType == "GlobalText") {
			curWidget->control = new CStatic;
			curWidget->value = theInterpreter->replaceVars(curWidget->value,NULL);
			rv = ((CStatic*)curWidget->control)->Create(curWidget->value, SS_LEFT, tmpRect, this, ID);
		}

		else if (widgetType == "DynamicText") {
			curWidget->control = new CStatic;
			curWidget->display = theInterpreter->GetTrimFile(curWidget->value);
			rv = ((CStatic*)curWidget->control)->Create(curWidget->display, SS_LEFT, tmpRect, this, ID);
		}

		else if (widgetType == "Navigation Text") {
			curWidget->control = new CNavText;
			rv = ((CNavText*)curWidget->control)->Create(curWidget->value, SS_LEFT, tmpRect, this, ID);
		}
		else if (widgetType == "EditBox") {
			curWidget->control = new CEdit;//Added new style parameter ES_AUTOHSCROLL- to allow *GASP* SCROLLING!!
/*			if (rv = ((CEdit*)curWidget->control)->CreateEx(WS_EX_CLIENTEDGE, 
													_T("EDIT"), 
													NULL,
													WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |ES_AUTOHSCROLL ,
													curWidget->location.x,
													curWidget->location.y,
													curWidget->size.width,
													curWidget->size.height,
													m_hWnd, ID, 0 ))
*/
			if (rv = ((CEdit*)curWidget->control)->CreateEx(WS_EX_CLIENTEDGE, 
													_T("EDIT"), 
													NULL,
													WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |ES_AUTOHSCROLL ,
													tmpRect,this, ID, 0 ))
			{
				//Set maximum number of characters allowed per line - limit set to 200
				((CEdit*)curWidget->control)->SetLimitText(int(curWidget->fieldlen.length));
				((CEdit*)curWidget->control)->SetWindowText(curWidget->value);
				((CEdit*)curWidget->control)->SetModify(FALSE);
			}
		}
		else if (widgetType == "Button") {
			curWidget->control = new CButton;
			rv = ((CButton*)curWidget->control)->Create(curWidget->value, BS_PUSHBUTTON | WS_TABSTOP, tmpRect, this, ID);
		}
		else if (widgetType == "RadioButton") {
			curWidget->control = new CButton;
			rv = ((CButton*)curWidget->control)->Create(curWidget->title, BS_AUTORADIOBUTTON | WS_TABSTOP, tmpRect, this, ID);

			//char* widgetName = new char[sizeof(curWidget->name)];
			char widgetName[MID_SIZE];
			strcpy(widgetName, curWidget->name);

			CString theVal = GetGlobal(curWidget->group);

			//int newLineIndex = theVal.ReverseFind('\n');
			//if (newLineIndex > -1)
				//theVal.SetAt(newLineIndex, '\0');
//			if (rv)
//			AfxMessageBox(theVal+curWidget->group,MB_OK);
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

			WIDGET* rWidget = findWidget((char *) (LPCTSTR) curWidget->group);

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
		}
		else if (widgetType == "RadioButton1") {
			curWidget->control = new CButton;
			rv = ((CButton*)curWidget->control)->Create(curWidget->title, BS_AUTORADIOBUTTON | WS_TABSTOP, tmpRect, this, ID);
			((CButton*)curWidget->control)->SetCheck(atoi(curWidget->value));
		}
		else if (widgetType == "RadioButton2") {
			curWidget->control = new CButton;
			rv = ((CButton*)curWidget->control)->Create(curWidget->title, BS_AUTORADIOBUTTON | WS_TABSTOP, tmpRect, this, ID);
			((CButton*)curWidget->control)->SetCheck(atoi(curWidget->value));
		}
		else if (widgetType == "CheckBox") {
			curWidget->control = new CButton;
			rv = ((CButton*)curWidget->control)->Create(curWidget->title, BS_AUTOCHECKBOX | WS_TABSTOP, tmpRect, this, ID);
			((CButton*)curWidget->control)->SetCheck(atoi(curWidget->value));
		}
		else if (widgetType == "ListBox") 
		{
			curWidget->control = new CListBox;
			rv = ((CListBox*)curWidget->control)->Create(
				LBS_STANDARD | // LBS_MULTIPLESEL | 
				WS_HSCROLL | WS_VSCROLL | WS_TABSTOP, 
				tmpRect, this, ID);
			((CListBox*)curWidget->control)->ModifyStyleEx(NULL, WS_EX_CLIENTEDGE, 0);

		}
		else if (widgetType == "CheckListBox") 
		{
			// Don't use LBS_STANDARD since it includes LBS_SORT which we don't want!
			curWidget->control = new CCheckListBox;
			rv = ((CCheckListBox*)curWidget->control)->Create(
				LBS_HASSTRINGS | LBS_OWNERDRAWFIXED | LBS_NOTIFY |
				WS_HSCROLL | WS_VSCROLL | WS_BORDER | WS_TABSTOP, 
				tmpRect, this, ID);
			((CCheckListBox*)curWidget->control)->ModifyStyleEx(NULL, WS_EX_CLIENTEDGE, 0);

		}
		else if ((widgetType == "ComboBox") ||(widgetType == "DropBox")) {
			curWidget->control = new CComboBox;
			rv = ((CComboBox*)curWidget->control)->Create(CBS_DROPDOWNLIST | WS_TABSTOP|WS_VSCROLL   , tmpRect, this, ID);
		}
		else if ((widgetType == "GroupBox") || (widgetType == "BoldGroup")) {
			curWidget->control = new CButton;
			rv = ((CButton*)curWidget->control)->Create(curWidget->value, BS_GROUPBOX, tmpRect, this, ID);
		}
		else if (widgetType == "ProgressBar") {
			curWidget->control = new CProgressCtrl;
			rv = ((CProgressCtrl*)curWidget->control)->Create(WS_TABSTOP, tmpRect, this, ID);
		}


		if (!rv)
		{
			CWnd myWnd;
			myWnd.MessageBox("Create failed on a widget.", widgetType, MB_OK);
		}
		else

		// Set the font of the widget
		if (curWidget->type == "BoldText")
		{
			curWidget->control->SetFont(m_pNavFont);
		}
		else if (curWidget->type == "BoldGroup")
		{
			curWidget->control->SetFont(m_pNavFont);
		}

		else if (curWidget->control && curWidget->control->m_hWnd)
		{
			curWidget->control->SetFont(m_pFont);
		}
	}

	// Handle initializations AFTER all controls created...
	for (x = 0; x < m_pControlCount; x++) 
	{
		WIDGET* curWidget = CurrentNode->pageWidgets[x];
		CString widgetType = curWidget->type;

		if (!curWidget->action.onInit.IsEmpty())
			theInterpreter->interpret(curWidget->action.onInit, curWidget);

		UpdateScreenWidget(curWidget);
	}

	// Handle enablement AFTER all initializations completed...
	for (x = 0; x < m_pControlCount; x++) 
		EnableWidget(CurrentNode->pageWidgets[x]);
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
		
		if (curWidget->control && curWidget->control->m_hWnd)
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

	if (!curWidget->control || !curWidget->control->m_hWnd)
		rv = curWidget->value; // !!! Fix this so we're not copying strings all the time
								// Should be able to just pass in an "assign" boolean
	else if (widgetType == "CheckBox") {
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
		int state = ((CButton*)curWidget->control)->GetState() & 0x0003;
		if (state == 2) 
			state = 0;
			
		CString allOptions;
		CString setBack;
			
		WIDGET* rWidget = findWidget((char *) (LPCTSTR) curWidget->group);

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
	else if (widgetType == "RadioButton1") {
		int state = ((CButton*)curWidget->control)->GetState() & 0x0003;
		if (state == 2) 
			state = 0;

		char temp[MIN_SIZE];

		itoa(state, temp, 10);

		rv = CString(temp);
	}
	else if (widgetType == "RadioButton2") {
		int state = ((CButton*)curWidget->control)->GetState() & 0x0003;
		if (state == 2) 
			state = 0;

		char temp[MIN_SIZE];

		itoa(state, temp, 10);

		rv = CString(temp);
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

		if (FALSE /* MULTIPLESEL something or other */ )
		{
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
		else
		{
			int selected = ((CListBox *)curWidget->control)->GetCurSel();
			if (selected == LB_ERR)
				rv = "";
			else
				((CListBox *)curWidget->control)->GetText(selected, rv);
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
	else if ((widgetType == "ComboBox") || (widgetType == "DropBox"))
	{
		int selectedIndex = ((CComboBox*)curWidget->control)->GetCurSel();
		if (selectedIndex != -1)
		{
			char tmpStr[MIN_SIZE];
			((CComboBox*)curWidget->control)->GetLBText(selectedIndex, tmpStr);
			
			CString rootPath1 = GetGlobal("Root");
			CString tpath1 = rootPath1 + "nscpxpiLinux";
			int direxist = GetFileAttributes(tpath1);
			if ((strcmp(tmpStr,"Windows")) == 0)
			{
				WIDGET *widg = findWidget("LinuxPath");
				if (widg)
				widg->control->EnableWindow(FALSE);
				widg = findWidget("Text7888");
				if (widg)
				widg->control->EnableWindow(FALSE);
				widg = findWidget("Button9888");
				if (widg)
				widg->control->EnableWindow(FALSE);
			}
			else if ((strcmp(tmpStr,"Linux")==0) && (direxist == -1))
			{
				WIDGET *widg = findWidget("LinuxPath");
				if (widg)
				widg->control->EnableWindow(TRUE);
				widg = findWidget("Text7888");
				if (widg)
				widg->control->EnableWindow(TRUE);
				widg = findWidget("Button9888");
				if (widg)
				widg->control->EnableWindow(TRUE);
			}
			else
			{}
			rv = CString(tmpStr);
		}
	}
/*	else if (widgetType == "DynamicText") 
	{
		char myLine[MAX_SIZE];
		curWidget->control->GetWindowText(myLine, 250);
		
		CString line = (CString)myLine;
		if((line.Compare(curWidget->display)) == 0)
		rv = curWidget->value;
			
	}
*/
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
	
	//CString animatedLogo = GetGlobal("AnimatedLogoURL");

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

