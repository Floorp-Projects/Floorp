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
#include "WizardMachine.h"
#include "fstream.h"
//#include "ImageDialog.h"
#include "ImgDlg.h"
#include "SumDlg.h"
#include "NavText.h"
#include "NewDialog.h"
#include "NewConfigDialog.h"
#include "ProgDlgThread.h"
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

	(AfxGetMainWnd( ))->SetWindowText(CurrentNode->localVars->title);

	if (!(CurrentNode->isWidgetsSorted)) {
		SortWidgetsForTabOrder();
	}

	CreateControls();
	DisplayControls();

	CPropSheet* pSheet = (CPropSheet*) GetParent();
	ASSERT_VALID(pSheet);

	if (CurrentNode->localVars->functionality == "BuildInstallers")
	{
		pSheet->GetDlgItem(ID_WIZNEXT)->SetWindowText("Build &Installers");
		isBuildInstaller = TRUE;
	}
	else { 
		isBuildInstaller = FALSE;
		pSheet->GetDlgItem(ID_WIZNEXT)->SetWindowText("&Next >");
	}

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
		theApp.GoToPrevNode();

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
		theApp.GoToNextNode();
	
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

BOOL CWizardUI::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	for(int i=0; i < CurrentNode->numWidgets; i++)
	{
		WIDGET* curWidget = CurrentNode->pageWidgets[i];
		if (curWidget->widgetID == (int)wParam) 
		{
			if (curWidget->action.dll == "NULL") 
			{
				if (curWidget->action.function == "command")
				{
					UpdateGlobals();
					char params[MAX_SIZE];
					
					CString function;
					strcpy(params, curWidget->action.parameters);
					CString orgParams = CString(params);
					CString setBack;
					int numCommands=0;
					char target[MID_SIZE] = {'\0'};
					char baseCommand[MID_SIZE] = {'\0'};
					char *args[MAX_SIZE];
					BOOL informAct = FALSE;


					char *commands[MIN_SIZE];
					

					commands[0] = (char *) GlobalAlloc(0, MAX_SIZE * sizeof(char));
					commands[0] = strtok(params, ";");
					setBack = CString(commands[0]);	

					int i=0;
					while (commands[i])
					{
						i++;
						commands[i] = strtok(NULL, ";");
						if (commands[i])
						{
							setBack += ";" + CString(commands[i]);
							
							if(!(strcmp(commands[i], "inform")))
							{
								informAct = TRUE;
							}
						}
					}
					strcpy(curWidget->action.parameters, (char *) (LPCTSTR) setBack);
					numCommands = i;
					
					if (curWidget->target != "")
					{
						strcpy(target, curWidget->target);
					}

					WIDGET* tmpWidget = theApp.findWidget((char*) (LPCTSTR)curWidget->target);
					CString tmpFunction = tmpWidget->action.function;
					CString tmpParams = CString(tmpWidget->action.parameters);
	
					char localPath[MAX_SIZE] = {'\0'};

					if (strrchr(tmpParams,'\\')) {
						strncpy(localPath, tmpParams, strlen(tmpParams) - strlen(strrchr(tmpParams,'\\')));							
					}
					

					char *commandList[MIN_SIZE];
					int commandListLength = 0;
					BOOL abortProcessing = FALSE;
					BOOL newEntry = FALSE;
					BOOL commandBuilt = FALSE;
					CString entryName;

					for (int j=0; j < numCommands; j++)
					{
						commandListLength = 0;
						if (!abortProcessing)
						{
							int numArgs = 0;


							int x=0;
							args[x] = (char *) GlobalAlloc(0, MAX_SIZE * sizeof(char));
							args[x] = strtok((char *)(LPCTSTR) commands[j], " ");	
	
							commandList[commandListLength] = (char *) GlobalAlloc(0, MAX_SIZE * sizeof(char));
	
							while (args[x])
							{
								x++;
								args[x] = strtok(NULL, " ");
								if (args[x])
								{
									setBack += " " + CString(args[x]);
								}
							}
					
							numArgs = x;

							if ((strstr(args[0], "ConfigDialog")))
							{
								CNewDialog newDlg;
								newDlg.DoModal();
								entryName = newDlg.GetData();
								newEntry = TRUE;
							}
							if (newEntry && entryName == "")
							{
								abortProcessing = TRUE;
							}
							else
							{
								if (!newEntry)
								{
									for (int k=0; k < numArgs; k++)
									{
										if (!(strstr(args[k], "%")))
										{
											if (!commandBuilt)
											{
												strcpy(commandList[commandListLength], args[k]);
											}
											else
											{
												strcat(commandList[commandListLength], args[k]);
											}
											strcat(commandList[commandListLength]," ");
											commandBuilt = TRUE;

											if (k+1 == numArgs)
												commandListLength++;
										}
										else
										{
											args[k]++;
											args[k][strlen(args[k])-1] = '\0';
														
											WIDGET* aWidget = theApp.findWidget(args[k]);
											
											if (aWidget)
											{
												if (aWidget->type == "ListBox")
												{
													CString valueSet = aWidget->value;

													char *values[MIN_SIZE];
				
													int numValues=0;
													values[numValues] = (char *) GlobalAlloc(0, MAX_SIZE * sizeof(char));
													values[numValues] = strtok((char *)(LPCTSTR)valueSet, ",");
													while (values[numValues])
													{
														numValues++;
														values[numValues] = strtok(NULL, ",");
													}							
													
													if (strstr(commandList[0]," ")) {
														strncpy(baseCommand, commandList[0], strlen(commandList[0]) - (strlen(strstr(commandList[0]," "))) );
														strcat(baseCommand, " ");
													}

													for (int index=0;  index < numValues; index++)
													{
			
														char valueBuffer[MAX_SIZE] = {'\0'};
														((CListBox*)aWidget->control)->GetText(atoi(values[index]), valueBuffer);
		
														if (index >0)
														{
															commandList[commandListLength] = (char *) GlobalAlloc(0, MAX_SIZE * sizeof(char));
															strcpy(commandList[commandListLength], baseCommand);
															strcat(commandList[commandListLength], currDirPath);
															if (localPath) {
																strcat(commandList[commandListLength], localPath);
															}
															strcat(commandList[commandListLength], "\\");
															strcat(commandList[commandListLength], valueBuffer);
															strcat(commandList[commandListLength], " ");	
															commandListLength++;
														}
														else
														{
															strcat(commandList[commandListLength], currDirPath);
															if (localPath) {
																strcat(commandList[commandListLength], localPath);
															}
															strcat(commandList[commandListLength], "\\");
															strcat(commandList[commandListLength], valueBuffer);
															strcat(commandList[commandListLength], " ");
															
															if (k+1 == numArgs)
															{
																commandListLength++;
															}
														}
													}
												}
												else
												{
													strcpy(commandList[commandListLength], (char *) (LPCTSTR) aWidget->value);
													strcat(commandList[commandListLength], " ");

													if (k+1 == numArgs)
													{
														commandListLength++;
													}
												}
											}
											else if (CString(args[k]) == "newEntry")
											{
												strcat(commandList[commandListLength], currDirPath);
												if (localPath) {
													strcat(commandList[commandListLength], localPath);
												}
												strcat(commandList[commandListLength], "\\");
												strcat(commandList[commandListLength], (char *) (LPCTSTR) entryName);
												
												if (k+1 == numArgs)
												{
													commandListLength++;
												}
											}
										}

									}	
								}
							}
						}
						newEntry = FALSE;
						for (int listNum =0; listNum < commandListLength; listNum++)
						{
							//system(commandList[listNum]);
							STARTUPINFO	startupInfo; 
							PROCESS_INFORMATION	processInfo; 

							memset(&startupInfo, '\0', sizeof(startupInfo));
							memset(&processInfo, '\0', sizeof(processInfo));

							startupInfo.cb = sizeof(STARTUPINFO);
							startupInfo.dwFlags = STARTF_USESHOWWINDOW;
							startupInfo.wShowWindow = SW_SHOW;

							BOOL executionSuccessful = CreateProcess(NULL, commandList[listNum], NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, NULL, &startupInfo, &processInfo); 
							DWORD error = GetLastError();
							WaitForSingleObject(processInfo.hProcess, INFINITE);
						}

						theApp.GenerateList(tmpFunction, tmpWidget, tmpParams);
					}
						
					if (informAct)
					{
						CWnd myWnd;
						char infoPath[MAX_SIZE] = {'\0'};
						strcpy(infoPath, currDirPath);
						if (localPath) {
							strcat(infoPath, localPath);
						}

						if (entryName != "") {
							myWnd.MessageBox( entryName + " is saved in " + CString(infoPath), "Information", MB_OK);
						}
					}	
				}
				else if (curWidget->action.function == "DisplayImage") {
					// This is to dsiplay an image in a separate dialog
					CImgDlg imgDlg(curWidget->action.parameters);
					int retVal = imgDlg.DoModal();
				}
				else if (curWidget->action.function == "ShowSum") {
					// This is to see if this works 
//							CWnd Mywnd;
//							Mywnd.MessageBox("hello","hello",MB_OK);
							CSumDlg sumdlg;
							int retVal = sumdlg.DoModal();

							
				}

				else if (curWidget->action.function == "BrowseFile") {
					// This is to browse to a file
					CFileDialog fileDlg(TRUE, NULL, NULL, OFN_OVERWRITEPROMPT, NULL, NULL);
					int retVal = fileDlg.DoModal();
				
					CString fullFileName = fileDlg.GetPathName();
					WIDGET* editWidget = CurrentNode->pageWidgets[i-1];
					((CEdit*)editWidget->control)->SetWindowText(fullFileName);
				}
				else if (curWidget->action.function == "BrowseDir") {
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
							WIDGET* editWidget = CurrentNode->pageWidgets[i-1];
							((CEdit*)editWidget->control)->SetWindowText(szPath);
						}
					 }
	
					 free( bi.pszDisplayName );
				}
				else if (curWidget->action.function == "NewConfig") {
					CNewConfigDialog newDlg;
					newDlg.DoModal();
					CString configField = newDlg.GetConfigName();
					CString newDir = CString(customizationPath); 
					newDir += configField;
					_mkdir(newDir);
					
					/**
					char srcCache[250];
					char destCache[250];
					strcpy(srcCache, Path);
					strcat(srcCache, "cck.che");
					strcpy(destCache, newDir);
					strcat(destCache, "\\cck.che");
					CopyFile(srcCache, destCache, FALSE);
					**/

					WIDGET* tmpWidget = theApp.findWidget((char*) (LPCTSTR)curWidget->target);
					CString tmpFunction = tmpWidget->action.function;
					CString params = CString(tmpWidget->action.parameters);
					theApp.GenerateList(tmpFunction, tmpWidget, params);	
					
					((CComboBox*)tmpWidget->control)->SelectString(0, configField);


					// remembering the widget name for subsequent .che file operations
					//customizationWidgetName = tmpWidget->name;
				}
				else if ((curWidget->action.function == "SortByName")
							|| (curWidget->action.function == "SortByPhone"))
				{
					WIDGET* listWidget = theApp.findWidget("AcctSetupListBox");
					int count = ((CListBox*)(listWidget->control))->GetCount();
					char* items[MAX_SIZE];

					for (int i = 0; i < count; i++) {
						items[i] = new char[MAX_SIZE];
						((CListBox*)(listWidget->control))->GetText(i, items[i]);
					}


					int x = 0;
					int y = 0;
					if (curWidget->action.function == "SortByName") {
						int ch = '.';
						char* pDest;
						int result;
						CString str1;
						CString str2;
						CString tmpStr;

					    for (x = count-1; x >= 0; x--) {
							BOOL flipped = FALSE;

							for (int y = 0; y < x; y++) {
								// number 1
								pDest = strchr(items[y], ch);
								result = pDest - items[y] + 1;
								tmpStr = items[y];
								str1 = tmpStr.Left(result);
							
								// number 2
								pDest = strchr(items[y+1], ch);
								result = pDest - items[y+1] + 1;
								tmpStr = items[y+1];
								str2 = tmpStr.Left(result);

								if (str1 > str2) 
								{
									char tmpItem[MAX_SIZE];
									strcpy(tmpItem, items[y]);

									strcpy(items[y], items[y+1]);
									strcpy(items[y+1], tmpItem);
	
									flipped = TRUE;
								}
							}

							if (!flipped) {
								break;
							}
						}
					}
					
					if (curWidget->action.function == "SortByPhone") {
						char* s1 = new char[MAX_SIZE];
						char* s2 = new char[MAX_SIZE];

					    for (x = count-1; x >= 0; x--) {
							BOOL flipped = FALSE;

							for (int y = 0; y < x; y++) {
								// number 1
								s1 = strstr(items[y], "(");
							
								// number 2
								s2 = strstr(items[y+1], "(");

								if (CString(s1) > CString(s2)) 
								{
									char tmpItem[MAX_SIZE];
									strcpy(tmpItem, items[y]);

									strcpy(items[y], items[y+1]);
									strcpy(items[y+1], tmpItem);
	
									flipped = TRUE;
								}
							}

							if (!flipped) {
								break;
							}
						}
					}

					((CListBox*)(listWidget->control))->ResetContent();
					for (int k = 0; k < count; k++) {
						((CListBox*)(listWidget->control))->AddString(CString(items[k]));
					}
				}
			}
			else 
			{
				/**
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
				**/
			}
			break;
		}
	}

	return CPropertyPage::OnCommand(wParam, lParam);
}

void CWizardUI::SortWidgetsForTabOrder()
{
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
	CurrentNode->isWidgetsSorted = TRUE;
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

		if (widgetType == "Text") {
			curWidget->control = new CStatic;
			((CStatic*)curWidget->control)->Create(curWidget->value, SS_LEFT, tmpRect, this, ID);
		}
		else if (widgetType == "Navigation Text") {
			curWidget->control = new CNavText;
			((CNavText*)curWidget->control)->Create(curWidget->value, SS_LEFT, tmpRect, this, ID);
		}
		else if (widgetType == "EditBox") {
			curWidget->control = new CEdit;
			((CEdit*)curWidget->control)->CreateEx(WS_EX_CLIENTEDGE, 
													_T("EDIT"), 
													NULL,
													WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER,
													curWidget->location.x,
													curWidget->location.y,
													curWidget->size.width,
													curWidget->size.height,
													m_hWnd, 0, 0 );

			((CEdit*)curWidget->control)->SetWindowText(curWidget->value);
		}
		else if (widgetType == "Button") {
			curWidget->control = new CButton;
			((CButton*)curWidget->control)->Create(curWidget->value, BS_PUSHBUTTON | WS_TABSTOP, tmpRect, this, ID);
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
		}
		else if (widgetType == "CheckBox") {
			curWidget->control = new CButton;
			((CButton*)curWidget->control)->Create(curWidget->title, BS_AUTOCHECKBOX | WS_TABSTOP, tmpRect, this, ID);
			((CButton*)curWidget->control)->SetCheck(atoi(curWidget->value));
		}
		else if (widgetType == "ListBox") 
		{
			curWidget->control = new CListBox;
			((CListBox*)curWidget->control)->Create(LBS_STANDARD | LBS_MULTIPLESEL | WS_HSCROLL | WS_VSCROLL | WS_TABSTOP, tmpRect, this, ID);
			((CListBox*)curWidget->control)->ModifyStyleEx(NULL, WS_EX_CLIENTEDGE, 0);

			if (curWidget->action.function == "GenerateFileList" ||
				curWidget->action.function == "GenerateDirList")
			{
				CString ext = CString(curWidget->action.parameters);				
				theApp.GenerateList(curWidget->action.function, curWidget, ext);
			}
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

			char* options[MIN_SIZE];
			int numOfOptions = 0;
			char* s = new char[MAX_SIZE];
			s = strtok(selectedItems, ",");
			while (s) 
			{
				options[numOfOptions] = new char[MID_SIZE];
				strcpy(options[numOfOptions], s);
				s = strtok( NULL, "," );
				numOfOptions++;
			}

			for (int j=0; j < numOfOptions; j++)
				((CListBox*)curWidget->control)->SetSel(atoi(options[j]), TRUE);

		}
		else if (widgetType == "ComboBox") {
			curWidget->control = new CComboBox;
			((CComboBox*)curWidget->control)->Create(CBS_DROPDOWNLIST | WS_TABSTOP, tmpRect, this, ID);

			if (curWidget->action.function == "GenerateFileList" ||
				curWidget->action.function == "GenerateDirList")
			{
				CString ext = CString(curWidget->action.parameters);				
				theApp.GenerateList(curWidget->action.function, curWidget, ext);
			}
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

			//((CComboBox*)curWidget->control)->SelectString(0, selectedCustomization);
		}
		else if (widgetType == "GroupBox") {
			curWidget->control = new CButton;
			((CButton*)curWidget->control)->Create(curWidget->value, BS_GROUPBOX, tmpRect, this, ID);
		}
		else if (widgetType == "ProgressBar") {
			curWidget->control = new CProgressCtrl;
			((CProgressCtrl*)curWidget->control)->Create(WS_TABSTOP, tmpRect, this, ID);
		}


		// Set the font of the widget and increment the dynamically assigned ID value
		if ((curWidget->description == "Navigation Status") 
			|| (curWidget->description == "Current Page")) {
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
	for (int i = 0; i < m_pControlCount; i++) {
		curWidget = CurrentNode->pageWidgets[i];
	
	if (curWidget->control)
		BOOL retFalg = curWidget->control->DestroyWindow();
	}
}

void CWizardUI::UpdateGlobals() 
{
	UpdateData(TRUE);
	
	
	WIDGET* curWidget;
	CString widgetType;


	for (int i = 0; i < m_pControlCount; i++) 
	{
		curWidget = CurrentNode->pageWidgets[i];
		widgetType = curWidget->type;

		if (widgetType == "CheckBox") {
			int state = ((CButton*)curWidget->control)->GetState();

			char temp[MIN_SIZE];

			itoa(state, temp, 10);

			curWidget->value = CString(temp);
		}
		if (widgetType == "RadioButton")
		{
			int state = ((CButton*)curWidget->control)->GetState();
			
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
		if (widgetType == "EditBox") {
			char myLine[MAX_SIZE];
			curWidget->control->GetWindowText(myLine, 250);

			CString line = (CString)myLine;
			curWidget->value = line;
		}
		if (widgetType == "ListBox")
		{
			LPINT choices;

			choices = (int *) GlobalAlloc(0, 10 * sizeof(LPINT));
			((CListBox *)curWidget->control)->GetSelItems(10, choices);
			int count;
			count = (((CListBox *)curWidget->control))->GetSelCount();
			
			CString valStr;
			for (int i=0; i < count; i++)
			{
				char temp[10];

				itoa(choices[i], temp, 10);

				CString tmpStr = CString(temp);
				
				valStr = valStr + tmpStr;
				if ( i+1 < count)
					valStr = valStr + ",";
			}
			curWidget->value = valStr;
		}
		if (widgetType == "ComboBox")
		{
			int selectedIndex = ((CComboBox*)curWidget->control)->GetCurSel();
			char tmpStr[MIN_SIZE];
			((CComboBox*)curWidget->control)->GetLBText(selectedIndex, tmpStr);
			curWidget->value = tmpStr;
		}
	}
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

	return CPropertyPage::OnWizardFinish();
}

BOOL CWizardUI::OnApply() 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CPropertyPage::OnApply();
}

