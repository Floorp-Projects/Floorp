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


// WizardMachine.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "WizardMachine.h"
#include <iostream.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include "HelpDlg.h"
#include "WizHelp.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CWizardMachineApp

BEGIN_MESSAGE_MAP(CWizardMachineApp, CWinApp)
	//{{AFX_MSG_MAP(CWizardMachineApp)
		// NOTE - the Class\ard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWizardMachineApp::HelpWiz)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWizardMachineApp construction

CWizardMachineApp::CWizardMachineApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CWizardMachineApp object
CPropSheet* PropertySheetDlg;

CWizardMachineApp theApp;
NODE *GlobalDefaults;
NODE *WizardTree;
NODE *CurrentNode;
WIDGET GlobalWidgetArray[1000];

int GlobalArrayIndex;
int CurrentPageBaseIndex=100;

CString Path;
char currDirPath[MAX_SIZE];
char iniFilePath[MAX_SIZE];
char imagesPath[MAX_SIZE];
char rootPath[MAX_SIZE];
char customizationPath[MAX_SIZE];

CString CacheFile;
CString CachePath;
BOOL UseCache = FALSE;
extern CSingleLock prevLock;
extern BOOL isBuildInstaller;
//Output file pointer for QA purposes
// Optimize here on global pointers. This many not needed.
FILE *out, *globs, *filePtr;;

/////////////////////////////////////////////////////////////////////////////
// CWizardMachineApp initialization

BOOL CWizardMachineApp::InitInstance()
{
	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	CWnd myWnd;

	//Do initializations
	//Check for comm existence
	//declare a tree for nodes

	Path = GetModulePath();

	strcpy(currDirPath, Path);

	CString outputFile = Path + "output.dat";
	out = OpenAFile(outputFile, "w");

	//char buffer[MID_SIZE];
	char **argv;
	int argc;

	// 10 argument limit
	argv = (char **) GlobalAlloc(0, sizeof(char *) * 10);

	int i=0;
	// Each argument limited to 100 chars
	argv[i] = (char *) GlobalAlloc(0, sizeof(char) * MIN_SIZE);
	argv[i] = strtok(m_lpCmdLine," ");
	while(argv[i])
	{
		argv[++i] = (char *) GlobalAlloc(0, sizeof(char) * MIN_SIZE);
		argv[i] = strtok(NULL," ");
	}
	
	argc = --i;

	CString iniFile;
	CString action;

	for (i=0; i < argc; i++)
	{
		if (!strcmp(argv[i], "-i"))
			iniFile = argv[i+1];
		if (!strcmp(argv[i], "-p"))
			action = argv[i+1];
	}

	if ((iniFile.IsEmpty()) || (iniFile.GetLength() < 5) || (iniFile.Right(4) != ".ini"))
	{
		myWnd.MessageBox("Please provide a valid inifile name.", "ERROR", MB_OK);
		fprintf(out, "----------------** TERMINATED - Invalid INI file name **---------------\n");
		exit(1);
	}

	for (i=iniFile.GetLength() -1; i >= 0 && iniFile[i] != '\\'; i--)
	{
		if (iniFile.GetAt(i) == '/' ) {
			iniFile.SetAt(i,(TCHAR)'\\');
		}
	}
	
	// Take care of absolute path to iniFiles and bitmaps
	if (iniFile[1] == ':' && iniFile[2] == '\\')
	{
		int len = 0;
		strcpy(iniFilePath, iniFile);

		int extractPosition = iniFile.ReverseFind('\\');
		extractPosition++;
		extractPosition = (iniFile.GetLength()) - extractPosition;
		iniFile = iniFile.Right(extractPosition);

		len = strlen(iniFilePath);

		while(iniFilePath[len] != '\\')
		{
			iniFilePath[len] = '\0';
			len--;
		}

		strcpy(imagesPath, iniFilePath);

		len--;

		while(imagesPath[len] != '\\')
		{
			imagesPath[len] = '\0';
			len--;
		}

		strcat(imagesPath, "bitmaps\\");
	}
	else
	{
		// Take care of relative path to iniFiles and bitmaps

		strcpy(iniFilePath, currDirPath);
		strcat(iniFilePath, "iniFiles\\");
		strcpy(imagesPath, currDirPath);
		strcat(imagesPath, "bitmaps\\");
	}

	strcpy(customizationPath, currDirPath);
	strcat(customizationPath, "customizations\\");


	CString cacheExt = ".che";
	CacheFile = CString(iniFile); 
	CacheFile = CacheFile.GetBufferSetLength(CacheFile.Find(".ini")) + cacheExt;
	CachePath = Path + CacheFile;

	char buffer[MAX_SIZE] = {'\0'};
	if (FileExists(CachePath))
	{
		UseCache = TRUE;
		FillGlobalWidgetArray(CachePath);
	}
	
	fprintf(out, "___________________________________________________________\n\n");
	fprintf(out, "ACTION SEQUENCE : %s\n", action);
	fprintf(out, "___________________________________________________________\n\n");
	
	SetGlobalDefaults();
	
	InitializeTree(CString(iniFile));
	CreateFirstLeaf(WizardTree, CString(iniFilePath) + CString(iniFile));

	//BEGIN - Command line QA enable code
	/**
	i=0;
	while(i < action.GetLength())
	{
		ExecuteAction(action.GetAt(i));
		i++;
	}
	
	CreateNewCache();

	fprintf(out, "---------------------**** END ****-------------------------\n");

	myWnd.MessageBox(CurrentNode->localVars->pageName, "Netscape", MB_OK);

	exit(0);
	**/
	//END - Command line QA enable code
	
	//CProgressDialog ProgressDialog;
	PropertySheetDlg = new CPropSheet(IDD_WIZARDMACHINE_DIALOG);	
	//CWizardMachineDlg* myWndTmp;

	CWizardUI* pages[99];	
	//CMyProgressDlg ProgressDialog(myWndTmp);
	//ProgressDialog.Create(IDD_PROGRESS_DLG);
	//CMyProgressDlg *pProgressDlg = &ProgressDialog;

	for (int x = 0; x < 99; x++) {
		pages[x] = new CWizardUI;
		PropertySheetDlg->AddPage (pages[x]);
	}

	PropertySheetDlg->SetWizardMode();
	int PageReturnValue = PropertySheetDlg->DoModal();
	delete(PropertySheetDlg);

	if (PageReturnValue == ID_WIZFINISH)
	{
		CreateRshell();
		if (!isBuildInstaller) {

			NODE* tmpNode = WizardTree->childNodes[0];
			while (!tmpNode->numWidgets) {
				tmpNode = tmpNode->childNodes[0];
			}

			CurrentNode = tmpNode;
		}
		theApp.CreateNewCache();

	}

	if (PageReturnValue == IDCANCEL)
	{
		theApp.CreateNewCache();
	}
	if (PageReturnValue == ID_HELP)
	{
		
		CWnd Mywnd;
		Mywnd.MessageBox("hello","hello",MB_OK);
	}
//

	//CreateNewCache();

	fprintf(out, "---------------------**** END ****-------------------------\n");


	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

////Add Wizard machine functions here.
//Define OnEnter function
//Define OnExit function
//Define ExitApp function
//Define GoToNextNode function
//Define GoToPrevNode function
//Define GetGlobal function
//Define SetGlobal function

void CWizardMachineApp::InitializeTree(CString rootIniFile)
{
	WizardTree = CreateNode(GlobalDefaults, rootIniFile);
	PrintNodeInfo(WizardTree);
}



//Create Node
NODE* CWizardMachineApp::CreateNode(NODE *parentNode, CString iniFile)
{

	char buffer[MAX_SIZE]= { '\0' };
	char visibility[MIN_SIZE] = {'\0'};
	CString pagesSection = "Sub Pages";
	CString varSection = "Local Variables";
	CString navCtrlSection = "Navigation Controls";
	CString widgetSection = "Widget";
	CString imageSection = "Image";
	CStringArray bufferArray;	
	NODE* NewNode = (NODE *) GlobalAlloc(0,sizeof(NODE) * 1);


	NewNode->parent = parentNode;

	iniFile = CString(iniFilePath)+iniFile; //Path+iniFile;

	CheckIniFileExistence(iniFile);

	GetPrivateProfileString(pagesSection, NULL, "", buffer, MAX_SIZE, iniFile);

	NewNode->numChildNodes = GetBufferCount(buffer);
	NewNode->currNodeIndex = 0;

	NewNode->subPages = new PAGE;

	int numHidePages = 0;
	for (int i=0; i < NewNode->numChildNodes ; i++)
	{
		CString page = GetBufferElement(buffer, i);
	
		GetPrivateProfileString(pagesSection, page, "", visibility, MAX_SIZE, iniFile);

		CString visible = CString(visibility);
		CString tmpVisible = CString(visibility);
		visible.MakeUpper();
		visible.TrimRight();
		if ( visible != "HIDE")
		{
			NewNode->subPages->pages.Add(page);
			NewNode->subPages->visibility.Add(tmpVisible);
		}
		else
		{
			numHidePages++;
		}
	}

	NewNode->numChildNodes = NewNode->numChildNodes - numHidePages;

	NewNode->childNodes = (NODE **) GlobalAlloc(0,NewNode->numChildNodes * sizeof(NODE *));

	for (i=0; i < NewNode->numChildNodes ; i++)
	{
		NewNode->childNodes[i] = NULL;
	}

	NewNode->localVars = new VARS;
	GetPrivateProfileString(varSection, "Title", "", buffer, MAX_SIZE, iniFile);
	NewNode->localVars->title = buffer;
	GetPrivateProfileString(varSection, "Caption", "", buffer, MAX_SIZE, iniFile);
	NewNode->localVars->caption = buffer;
	GetPrivateProfileString(varSection, "Function", "", buffer, MAX_SIZE, iniFile);
	NewNode->localVars->functionality = buffer;

	CString tempPageVar = "";

	int index = iniFile.ReverseFind('\\');
	if (index != -1)
	{
		for (int j=index+1 ; j < iniFile.GetLength() ; j++)
		{
			char element = iniFile.GetAt(j);
			if (element == '.')
			{
				break;
			}
			else
			{
				tempPageVar += element;
			}
		}
	}

	NewNode->localVars->pageName = tempPageVar;
	
	
	GetPrivateProfileString(varSection, "image", "", buffer, MAX_SIZE, iniFile);
	NewNode->localVars->image = buffer;
	GetPrivateProfileString(varSection, "Visibility", "", buffer, MAX_SIZE, iniFile);
	NewNode->localVars->visibility = buffer;
		
	NewNode->navControls = new CONTROLS;
	GetPrivateProfileString(navCtrlSection, "OnNext", "", buffer, MAX_SIZE, iniFile);
	NewNode->navControls->onNextAction = buffer;
	GetPrivateProfileString(navCtrlSection, "Help", "", buffer, MAX_SIZE, iniFile);
	NewNode->navControls->helpFile = buffer;
	  
	NewNode->pageWidgets = (WIDGET **) GlobalAlloc(0, MIN_SIZE * sizeof(WIDGET *));
	NewNode->numWidgets = 0;
	NewNode->currWidgetIndex = 0;

	NewNode->pageBaseIndex = CurrentPageBaseIndex;
	CurrentPageBaseIndex += MIN_SIZE;
	
	// MAX 20 images/page ....?
	NewNode->images = (IMAGE **) GlobalAlloc(0, 20 * sizeof(IMAGE *));
	NewNode->numImages = 0;
	
	char sectionBuffer[MID_SIZE];
	GetPrivateProfileString(NULL, NULL, "", sectionBuffer, MAX_SIZE, iniFile);
	char iniSection[MID_SIZE];

	int idCounter=0;
	for (i=0; i < GetBufferCount(sectionBuffer); i++)
	{
		strcpy(iniSection,GetBufferElement(sectionBuffer, i));

		if(strstr(iniSection, widgetSection))
		{
			char localBuffer[MAX_SIZE] = {'\0'};

			//GlobalWidgetArray[GlobalArrayIndex].cached = TRUE;

			GetPrivateProfileString(iniSection, "Name", "", buffer, MAX_SIZE, iniFile);
			strcpy(localBuffer, buffer);

			WIDGET* widgetEntry = findWidget(buffer);

			if (widgetEntry)
			{
				if (!widgetEntry->cached)
				{
					prevLock.Lock();
					CWnd myWnd;
					myWnd.MessageBox("Duplicate Widget Entry " + CString(buffer) + " detected : "+ NewNode->localVars->pageName , "Error", MB_ICONSTOP | MB_SYSTEMMODAL);
					prevLock.Unlock();
					exit(7);
				}
				else
				{
					BOOL readValue = (widgetEntry->value == "");
					BuildWidget(widgetEntry, iniSection, iniFile, NewNode->pageBaseIndex, readValue);
						
					widgetEntry->cached = FALSE;
					NewNode->pageWidgets[NewNode->numWidgets] = widgetEntry;
				}
			}
			else
			{
				BOOL readValue = TRUE;
				GlobalWidgetArray[GlobalArrayIndex].cached = TRUE;

				BuildWidget(&GlobalWidgetArray[GlobalArrayIndex], iniSection, iniFile, NewNode->pageBaseIndex, readValue);	
				GlobalWidgetArray[GlobalArrayIndex].cached = FALSE;

				NewNode->pageWidgets[NewNode->numWidgets] = &GlobalWidgetArray[GlobalArrayIndex];
				GlobalArrayIndex++;
			}
			
			(NewNode->numWidgets)++;

		}
		else if(strstr(iniSection, imageSection))
		{
			int index = NewNode->numImages;

			// This allows dynamic buffer allocation to CString vars
			NewNode->images[index] = new IMAGE;
			
			GetPrivateProfileString(iniSection, "Name", "", buffer, MAX_SIZE, iniFile);
			NewNode->images[index]->name = CString(imagesPath) + CString(buffer);

			GetPrivateProfileString(iniSection, "Value", "", buffer, MAX_SIZE, iniFile);
			NewNode->images[index]->value = buffer;

			GetPrivateProfileString(iniSection, "start_X", "", buffer, MAX_SIZE, iniFile);
			NewNode->images[index]->location.x = atoi(buffer);

			GetPrivateProfileString(iniSection, "start_Y", "", buffer, MAX_SIZE, iniFile);
			NewNode->images[index]->location.y = atoi(buffer);

			GetPrivateProfileString(iniSection, "width", "", buffer, MAX_SIZE, iniFile);
			NewNode->images[index]->size.width = atoi(buffer);

			GetPrivateProfileString(iniSection, "height", "", buffer, MAX_SIZE, iniFile);
			NewNode->images[index]->size.height = atoi(buffer);

			NewNode->images[index]->hBitmap = (HBITMAP)LoadImage(NULL, NewNode->images[index]->name, IMAGE_BITMAP, 0, 0,
											LR_LOADFROMFILE|LR_CREATEDIBSECTION);
		
			(NewNode->numImages)++;
		}
	}

	parentNode->childNodes[parentNode->currNodeIndex] = NewNode;

	CurrentNode = NewNode;

	//PrintNodeInfo(CurrentNode);

	return NewNode;
}

//Create Node
void CWizardMachineApp::CreateNode()
{
	GlobalDefaults = (NODE *) GlobalAlloc(0,sizeof(NODE) * 1);

	GlobalDefaults->parent = NULL;

	GlobalDefaults->childNodes = (NODE **) GlobalAlloc(0,1 * sizeof(NODE *));
	GlobalDefaults->numChildNodes = 1;
	GlobalDefaults->currNodeIndex = 0;
	GlobalDefaults->childNodes[GlobalDefaults->currNodeIndex] = NULL;

	
	GlobalDefaults->localVars = new VARS;
	GlobalDefaults->localVars->title = "Globals";
	GlobalDefaults->localVars->caption = "Globals";
	GlobalDefaults->localVars->pageName = "Globals";
	GlobalDefaults->localVars->image = "Globals";
	
	GlobalDefaults->subPages = NULL;
	GlobalDefaults->navControls = NULL;
	GlobalDefaults->pageWidgets = NULL;
	GlobalDefaults->numWidgets = 0;
	GlobalDefaults->currWidgetIndex = 0;

	CurrentNode = GlobalDefaults;

	PrintNodeInfo(CurrentNode);

}


void CWizardMachineApp::SetGlobalDefaults()
{
	CreateNode();
}

void CWizardMachineApp::CreateFirstLeaf(NODE *parent, CString iniFile)
{
	NODE* currNode = parent;
	CString section = "Sub Pages";
	char buffer[MAX_SIZE] = {'\0'};
	CString iniName;


	iniName = iniFile;

	while (SectionFound(iniName, section))
	{
		CString visibleChild;
		char localBuffer[MAX_SIZE];
		int i = 0;
		GetPrivateProfileString(section, NULL, "", buffer, MAX_SIZE, iniName);
		GetPrivateProfileString(section, GetBufferElement(buffer, i), "", localBuffer, MAX_SIZE, iniName);
		
		CString visible = CString(localBuffer);
		visible.MakeUpper();
		visible.TrimRight();

		while (visible == "HIDE")
		{
			i++;
			GetPrivateProfileString(section, GetBufferElement(buffer, i), "", localBuffer, MAX_SIZE, iniName);
			
			visible = CString(localBuffer);
			visible.MakeUpper();
		}

		visibleChild = GetBufferElement(buffer, i);

		if (visibleChild + ".ini" == ".ini")
		{
			CWnd myWnd;
			CString message = "No pages to display.....!";
			myWnd.MessageBox(message, "ERROR", MB_OK);
			fprintf(out, "------------** TERMINATED - No pages to display **----------------\n");
			exit(8);
		}

		currNode->childNodes[0] = CreateNode(currNode, visibleChild + ".ini");
		currNode = currNode->childNodes[0];
		
		iniName = iniFilePath + visibleChild + ".ini";

		PrintNodeInfo(CurrentNode);
		
	}

}

BOOL CWizardMachineApp::SectionFound(CString iniFile, CString section)
{
	char buffer[MAX_SIZE] = {'\0'};

	CheckIniFileExistence(iniFile);

	GetPrivateProfileString(section, NULL, "", buffer, MIN_SIZE, iniFile);

	return (buffer[0] != '\0');
}

int CWizardMachineApp::GetBufferCount(char *buffer)
{
	int i=0;
	int j=0;

	if (!buffer[1] && !buffer[2])
		return 0;

	BOOL streamEnd = FALSE;
	while (!streamEnd)
	{
		while (buffer[++i])
			;
		j++;
		
		if (buffer[i+1] == '\0')
				streamEnd = TRUE;
	}
	return j;
}

char* CWizardMachineApp::GetBufferElement(const char *buffer, int index)
{
	static char localBuffer[MID_SIZE];

	for (int i=0; i < MID_SIZE; i++)
		localBuffer[i] = '\0';

	int j=0, k=0;
	i=0;
	BOOL streamEnd = FALSE;
	BOOL elementFound = FALSE;
	while (!streamEnd)
	{
		while (buffer[i])
		{	
			if (j == index)
			{
				localBuffer[k] = buffer[i];
				elementFound = TRUE;
				k++;
			}
			i++;
		}
			
		if (elementFound)
		{
			break;
		}
		
		j++;
	
		if (buffer[i+1] == '\0')
				streamEnd = TRUE;
		else
				i++;
	}

	return localBuffer;

}

char* CWizardMachineApp::GetSectionBuffer(CString iniFile, CString section)
{
	static char localBuffer[MID_SIZE];

	CheckIniFileExistence(iniFile);

	for (int i=0; i < MID_SIZE; i++)
		localBuffer[i] = '\0';

	GetPrivateProfileString(section, NULL, "", localBuffer, MID_SIZE, iniFile);

	return localBuffer;

}

void CWizardMachineApp::ExecuteAction(char action)
{

	switch(action)
	{
		case 'n' : 	
			fprintf(out, "** NEXT : Begin **\n");
			fprintf(out, "-----------------------------------------------------------\n");
			GoToNextNode();
			fprintf(out, "** NEXT : End **\n");
			fprintf(out, "-----------------------------------------------------------\n");
			break;
	
		case 'b' : 	
			fprintf(out, "** BACK : Begin **\n");
			fprintf(out, "-----------------------------------------------------------\n");
			GoToPrevNode();
			fprintf(out, "** BACK : End **\n");
			fprintf(out, "-----------------------------------------------------------\n");
			break;

		case 'c':
			fprintf(out, "---------------------*** CANCEL ***------------------------\n");
			fprintf(out, "-----------------** TERMINATED - User Cancelled App **-----------------\n");
			ExitApp();
			exit(0);
			break;
		
		default :
			CWnd myWnd;
			char temp[MIN_SIZE] = {action};
			myWnd.MessageBox(strcat("Unknown Action : ", temp), "ERROR", MB_OK);
			fprintf(out, "---------------** TERMINATED - Invalid Action Element **---------------\n");
			exit(6);
	}
}

void CWizardMachineApp::GoToNextNode()
{
	//check if it is a container node
		//for now check existence of display information
		//go to first child and so on

	//----------------------------------------------------------------------------------------------
	// Handle OnNext processing before doing anything else
	//----------------------------------------------------------------------------------------------
	if (CurrentNode->navControls->onNextAction)
	{
		if (strncmp(CurrentNode->navControls->onNextAction, "Reload", 6) == 0)
		{
		}
	}

	//----------------------------------------------------------------------------------------------
	NODE* tmpParentNode;

	tmpParentNode = CurrentNode->parent;

	while (tmpParentNode->localVars->pageName != "Globals")
	{

		PrintNodeInfo(tmpParentNode);

		if ((tmpParentNode->currNodeIndex + 1) < tmpParentNode->numChildNodes)
		{
			tmpParentNode->currNodeIndex++;

			NODE* siblingNode = tmpParentNode->childNodes[tmpParentNode->currNodeIndex];

			if (siblingNode)
			{
				CurrentNode = siblingNode;
				//CurrentNode = tmpParentNode->childNodes[tmpParentNode->currNodeIndex];
			}
			else
			{
				CurrentNode = CreateNode(tmpParentNode, tmpParentNode->subPages->pages.GetAt(tmpParentNode->currNodeIndex) + ".ini");
				//tmpParentNode->childNodes[tmpParentNode->currNodeIndex] = CreateNode(tmpParentNode, tmpParentNode->subPages->pages.GetAt(tmpParentNode->currNodeIndex) + ".ini");
			}
			


			BOOL isAContainerNode;	
			BOOL haveChildNodes = TRUE;

			isAContainerNode = (CurrentNode->numWidgets == 0);
			while(isAContainerNode && haveChildNodes)
			{
				NODE* containerNode = CurrentNode;
				if (containerNode->numChildNodes > 0)
				{
					CurrentNode = containerNode->childNodes[0];
					if (!CurrentNode)
					{
						CurrentNode = CreateNode(containerNode, containerNode->subPages->pages.GetAt(0) + ".ini");
					}
					isAContainerNode = (CurrentNode->numWidgets == 0);
					PrintNodeInfo(containerNode);
					haveChildNodes = TRUE;
				}
				else
				{
					haveChildNodes = FALSE;
				}
			}
			PrintNodeInfo(CurrentNode);
			if (haveChildNodes)
			{
				break;
			}
		}
		else
		{
			tmpParentNode = tmpParentNode->parent;
		}
	}
	if (tmpParentNode->localVars->pageName == "Globals")
	{
		CWnd myWnd;
		myWnd.MessageBox("Can't go past the last node", "Error", MB_OK);
		fprintf(out, "------------** TERMINATED - Can't go past the last page **----------------\n");
		exit(10);
	}
}


void CWizardMachineApp::GoToPrevNode()
{

	//check if it is a container node
		//for now check existence of display information
		//go to last child and so on


	NODE* tmpParentNode;

	tmpParentNode = CurrentNode->parent;

	while (tmpParentNode->localVars->pageName != "Globals")
	{

		PrintNodeInfo(tmpParentNode);

		if (tmpParentNode->currNodeIndex > 0)
		{
			tmpParentNode->currNodeIndex--;

			NODE* siblingNode = tmpParentNode->childNodes[tmpParentNode->currNodeIndex];

			if (siblingNode)
			{
				CurrentNode = siblingNode;
				//CurrentNode = tmpParentNode->childNodes[tmpParentNode->currNodeIndex];
			}
			else
			{
				CurrentNode = CreateNode(tmpParentNode, tmpParentNode->subPages->pages.GetAt(tmpParentNode->currNodeIndex) + ".ini");
				//tmpParentNode->childNodes[tmpParentNode->currNodeIndex] = CreateNode(tmpParentNode, tmpParentNode->subPages->pages.GetAt(tmpParentNode->currNodeIndex) + ".ini");
			}
			
			BOOL isAContainerNode;	
			BOOL haveChildNodes = TRUE;

			isAContainerNode = (CurrentNode->numWidgets == 0);
			while(isAContainerNode && haveChildNodes)
			{
				NODE* containerNode = CurrentNode;

				if (containerNode->numChildNodes > 0)
				{
					int index = containerNode->currNodeIndex;
					CurrentNode = containerNode->childNodes[index];
					if (!CurrentNode)
					{
						CurrentNode = CreateNode(containerNode, containerNode->subPages->pages.GetAt(index) + ".ini");
					}
					isAContainerNode = (CurrentNode->numWidgets == 0);
					PrintNodeInfo(containerNode);
				}
				else
				{
					haveChildNodes = FALSE;
				}
			}

			PrintNodeInfo(CurrentNode);
			if (haveChildNodes)
			{
				break;
			}
		}
		else
		{
			tmpParentNode = tmpParentNode->parent;
		}
	}
	if (tmpParentNode->localVars->pageName == "Globals")
	{
		CWnd myWnd;
		myWnd.MessageBox("Can't go back from first node", "Error", MB_OK);
		fprintf(out, "------------** TERMINATED - Can't go back from first page **----------------\n");
		exit(9);
	}

}

void CWizardMachineApp::ExitApp()
{
	CreateNewCache();
}


FILE* CWizardMachineApp::OpenAFile(CString outputFile, CString mode)
{

	if( !( filePtr = fopen( outputFile, mode ) ) )
	{
	    CWnd myWnd;
	
		myWnd.MessageBox("Unable to open file" + outputFile, "ERROR", MB_OK);
		//fprintf(out, "--------------** TERMINATED - Can't open output file **----------------\n");
		exit( 3 );
	}

	return filePtr;
}

void CWizardMachineApp::PrintNodeInfo(NODE* node)
{
	if (node)
	{
		fprintf(out, "Node name : " + node->localVars->pageName + "\n\n");
		fprintf(out, "Number of Sub pages : %d\n", node->numChildNodes);
		fprintf(out, "Number of Widgets   : %d\n", node->numWidgets);

		/**
		for (int i=0; i< node->numWidgets; i++)
		{
			fprintf(out, "Widget%d Name        : " + node->pageWidgets[i]->name + "\n", i+1);
			fprintf(out, "Widget%d Value	    : " + node->pageWidgets[i]->value + "\n", i+1);
	
		}
		**/
		fprintf(out, "-----------------------------------------------------------\n");
	}
}

void CWizardMachineApp::CheckIniFileExistence(CString file)
{
	FILE *f;
	//BOOL CheckIniFileExistence = FALSE;
	
	if (! (f = fopen(file, "r") ))
	{
		CWnd myWnd;
		CString message = "File " + file + " could not be opened ";

		myWnd.MessageBox(message, "ERROR", MB_OK);
		fprintf(out, "------------** TERMINATED - Missing File : " + file + " **----------------\n");
		exit(4);
	}
	else
	{
		fclose(f);
	}
}

BOOL CWizardMachineApp::FileExists(CString file)
{
	FILE *f;
	
	if (! (f = fopen(file, "r") ))
		return FALSE;
	else
		return TRUE;
}

void CWizardMachineApp::FillGlobalWidgetArray(CString file)
{
	char buffer[MAX_SIZE] = {'\0'};
	CString name = "";
	CString value = "";

	globs = OpenAFile(file, "r");

	while(!feof(globs))
	{
		while(fgets(buffer, MAX_SIZE, globs))
		{
			if (strstr(buffer, "="))
			{
				name =  CString(strtok(buffer,"="));
				value = CString(strtok(NULL,"="));
				value.TrimRight();
				
				WIDGET* w = SetGlobal(name, value);
				if (w)
					w->cached = TRUE;
			}
		}
	}

	fclose(globs);
}

void CWizardMachineApp::FillGlobalWidgetArray()
{
	FillGlobalWidgetArray(CachePath);
}

void CWizardMachineApp::CreateNewCache()
{
	globs = OpenAFile(CachePath, "w");

	for(int i=0; i< GlobalArrayIndex; i++)
	{
		if (GlobalWidgetArray[i].type != "RadioButton" 
			&& GlobalWidgetArray[i].type != "Text" 
			&& GlobalWidgetArray[i].type != "Button" 
			&& GlobalWidgetArray[i].type != "GroupBox" )
		{
			if (GlobalWidgetArray[i].name != "")
			{
				fprintf(globs, GlobalWidgetArray[i].name + "=" + GlobalWidgetArray[i].value);
				fprintf(globs, "\n");
			}
		}
	}
	fclose(globs);
}

BOOL CWizardMachineApp::IsLastNode(NODE* treeNode)
{

	NODE* tmpParent;
	BOOL lastNode = TRUE;

	if (treeNode)
	{
		tmpParent = treeNode->parent;

		while(tmpParent->localVars->pageName != "Globals")
		{
			if (tmpParent->numChildNodes == (tmpParent->currNodeIndex+1))
				tmpParent = tmpParent->parent;
			else
			{
				lastNode = FALSE;
				break;
			}
		}
	}
	else
	{
		lastNode = FALSE;
	}
	return lastNode;
}

BOOL CWizardMachineApp::IsFirstNode(NODE* treeNode)
{

	NODE* tmpParent;
	NODE* tmpNode;
	BOOL firstNode = TRUE;

	tmpNode = treeNode;
	tmpParent = treeNode->parent;

	if (tmpParent->childNodes[0] == tmpNode)
	{
		while((tmpParent->localVars->pageName != "Globals"))
		{
			tmpNode = tmpParent;
			tmpParent = tmpParent->parent;

			if (!(tmpParent->childNodes[0] == tmpNode))
			{
				firstNode = FALSE;
			}
		}
	
	}
	else
	{
		firstNode = FALSE;
	}
	return firstNode;
}

CString CWizardMachineApp::GetModulePath()
{
	char currPath[MID_SIZE];
    int     i,numBytes; 
 
    // Get the path of the file that was executed 
    numBytes = GetModuleFileName(m_hInstance, currPath, MIN_SIZE);

	// get the cmd path
	// Remove the filename from the path 
	for (i=numBytes-1;i >= 0 && currPath[i] != '\\';i--);
	// Terminate command line with 0 
	if (i >= 0) 
		currPath[i+1]= '\0';

	return CString(currPath);
}

CString CWizardMachineApp::GetGlobal(CString theName)
{
	for (int i = 0; i <= GlobalArrayIndex; i++)
	{
		if (GlobalWidgetArray[i].name == theName) {
			return GlobalWidgetArray[i].value;
		}
	}

	return "";
}

CString CWizardMachineApp::GetGlobalOptions(CString theName)
{
	CString temp="";

	for (int i = 0; i <= GlobalArrayIndex; i++)
	{
		if (GlobalWidgetArray[i].name == theName) {
			if (GlobalWidgetArray[i].numOfOptions > 0) {
				for (int j = 0; j < GlobalWidgetArray[i].numOfOptions; j++) {
					strcat((char*)(LPCTSTR)(temp), GlobalWidgetArray[i].options.value[j]);
					strcat((char*)(LPCTSTR)(temp), ";");
				}
			}
			else {
				temp = GlobalWidgetArray[i].items;
			}
			break;
		}
	}

	return temp;
}

WIDGET* CWizardMachineApp::SetGlobal(CString theName, CString theValue)
{
	WIDGET* w = findWidget((char *)(LPCTSTR) theName);
	if (w == NULL)
	{
		if (++GlobalArrayIndex >= sizeof(GlobalWidgetArray))
		{
			fprintf(out, "----------------** TERMINATED - Out of Global Space **---------------\n");
			exit(11);
		}

		GlobalWidgetArray[GlobalArrayIndex].value = theValue;
		w = &GlobalWidgetArray[GlobalArrayIndex];
	}
	else 
		w->value = theValue;

	return w;
}

WIDGET* CWizardMachineApp::findWidget(char *theName)
{
	
	for (int i = 0; i <= GlobalArrayIndex; i++)
	{
		if (GlobalWidgetArray[i].name == theName) {
			return (&GlobalWidgetArray[i]);
		}
	}

	return NULL;
}

void CWizardMachineApp::BuildWidget(WIDGET* aWidget, CString iniSection, CString iniFile, int pageBaseIndex, BOOL readValue)
{
	static int idCounter = 0;
	char buffer[MAX_SIZE] = {'\0'};
	char largeBuffer[EXTD_MAX_SIZE] = {'\0'};
	
	idCounter++;

	GetPrivateProfileString(iniSection, "Name", "", buffer, MAX_SIZE, iniFile);
	aWidget->name = buffer;

	GetPrivateProfileString(iniSection, "Type", "", buffer, MAX_SIZE, iniFile);
	aWidget->type = buffer;

	GetPrivateProfileString(iniSection, "Options", "", buffer, MAX_SIZE, iniFile);
	aWidget->items = buffer;

	if (readValue)
	{
		GetPrivateProfileString(iniSection, "Value", "", largeBuffer, EXTD_MAX_SIZE, iniFile);
		aWidget->value = largeBuffer;
		aWidget->value.TrimRight();
	}

	GetPrivateProfileString(iniSection, "Title", "", buffer, MAX_SIZE, iniFile);
	aWidget->title = buffer;

	GetPrivateProfileString(iniSection, "Group", "", buffer, MAX_SIZE, iniFile);
	aWidget->group = buffer;
			
	GetPrivateProfileString(iniSection, "Target", "", buffer, MAX_SIZE, iniFile);
	aWidget->target = buffer;

	GetPrivateProfileString(iniSection, "Start_x", "", buffer, MAX_SIZE, iniFile);
	aWidget->location.x = atoi(buffer);

	GetPrivateProfileString(iniSection, "Start_y", "", buffer, MAX_SIZE, iniFile);
	aWidget->location.y = atoi(buffer);
			
	//aWidget->size = new DIMENSION;
	GetPrivateProfileString(iniSection, "Width", "", buffer, MAX_SIZE, iniFile);
	aWidget->size.width = atoi(buffer);
	GetPrivateProfileString(iniSection, "Height", "", buffer, MAX_SIZE, iniFile);
	aWidget->size.height = atoi(buffer);

	GetPrivateProfileString(iniSection, "dll", "", buffer, MAX_SIZE, iniFile);
	aWidget->action.dll = buffer;
	GetPrivateProfileString(iniSection, "function", "", buffer, MAX_SIZE, iniFile);
	aWidget->action.function = buffer;
	GetPrivateProfileString(iniSection, "parameters", "", buffer, MAX_SIZE, iniFile);
	strcpy(aWidget->action.parameters, buffer);

	/// Dynamic ID allocation
	aWidget->widgetID = pageBaseIndex + idCounter;

			
	// As the number of entries in the subsection are not known, a generic loop
	// has been created to read all existing name/value pairs in the subsection
	// and store them in the options component of the control.
	GetPrivateProfileString(iniSection, "subsection", "", buffer, MAX_SIZE, iniFile);
	if (strcmp(buffer, "") != 0) {
		char* subSection;
		subSection = new char[sizeof(buffer)];
		strcpy(subSection, buffer);

		if (aWidget->action.function == "")
		{
			int counter = 0;
			int i = 0;
			char* ComponentKey;
			char ComponentKeyBuffer[MAX_SIZE];
			if (GetPrivateProfileString(subSection, NULL, "", buffer, MAX_SIZE, iniFile) > 0)
			{
				while (buffer[i] != 0)
				{
					ComponentKey = &buffer[i];
					if (GetPrivateProfileString(subSection, ComponentKey, "", ComponentKeyBuffer, MAX_SIZE, iniFile) > 0)
					{
						aWidget->options.name[counter] = new char[sizeof(ComponentKey)];
						strcpy(aWidget->options.name[counter],ComponentKey);
						aWidget->options.value[counter] = new char[sizeof(ComponentKeyBuffer)];
						strcpy(aWidget->options.value[counter],ComponentKeyBuffer);	
						counter++;
					}
					while (buffer[i] != 0)
						i++;
					i++;
				}
			}
			aWidget->numOfOptions = counter;
		}	
		else {
			aWidget->numOfOptions = 0;
		}
	}
}

/*
void CWizardMachineApp::BuildHelpWidget(WIDGET* aWidget, CString iniSection, CString iniFile, int pageBaseIndex)
{
	static int idCounter = 0;
	char buffer[MAX_SIZE] = {'\0'};
	char largeBuffer[EXTD_MAX_SIZE] = {'\0'};
	
	idCounter++;

	GetPrivateProfileString(iniSection, "Name", "", buffer, MAX_SIZE, iniFile);
	aWidget->name = buffer;

	GetPrivateProfileString(iniSection, "Type", "", buffer, MAX_SIZE, iniFile);
	aWidget->type = buffer;

	GetPrivateProfileString(iniSection, "Options", "", buffer, MAX_SIZE, iniFile);
	aWidget->items = buffer;

	GetPrivateProfileString(iniSection, "Value", "", largeBuffer, EXTD_MAX_SIZE, iniFile);
	aWidget->value = largeBuffer;
	aWidget->value.TrimRight();
	
	GetPrivateProfileString(iniSection, "Title", "", buffer, MAX_SIZE, iniFile);
	aWidget->title = buffer;

	GetPrivateProfileString(iniSection, "Group", "", buffer, MAX_SIZE, iniFile);
	aWidget->group = buffer;
			
	GetPrivateProfileString(iniSection, "Target", "", buffer, MAX_SIZE, iniFile);
	aWidget->target = buffer;

	GetPrivateProfileString(iniSection, "Start_x", "", buffer, MAX_SIZE, iniFile);
	aWidget->location.x = atoi(buffer);

	GetPrivateProfileString(iniSection, "Start_y", "", buffer, MAX_SIZE, iniFile);
	aWidget->location.y = atoi(buffer);
			
	//aWidget->size = new DIMENSION;
	GetPrivateProfileString(iniSection, "Width", "", buffer, MAX_SIZE, iniFile);
	aWidget->size.width = atoi(buffer);
	GetPrivateProfileString(iniSection, "Height", "", buffer, MAX_SIZE, iniFile);
	aWidget->size.height = atoi(buffer);

	GetPrivateProfileString(iniSection, "dll", "", buffer, MAX_SIZE, iniFile);
	aWidget->action.dll = buffer;
	GetPrivateProfileString(iniSection, "function", "", buffer, MAX_SIZE, iniFile);
	aWidget->action.function = buffer;
	GetPrivateProfileString(iniSection, "parameters", "", buffer, MAX_SIZE, iniFile);
	strcpy(aWidget->action.parameters, buffer);

	/// Dynamic ID allocation
	aWidget->widgetID = pageBaseIndex + idCounter;

			
	// As the number of entries in the subsection are not known, a generic loop
	// has been created to read all existing name/value pairs in the subsection
	// and store them in the options component of the control.
	GetPrivateProfileString(iniSection, "subsection", "", buffer, MAX_SIZE, iniFile);
	if (strcmp(buffer, "") != 0) {
		char* subSection;
		subSection = new char[sizeof(buffer)];
		strcpy(subSection, buffer);

		if (aWidget->action.function == "")
		{
			int counter = 0;
			int i = 0;
			char* ComponentKey;
			char ComponentKeyBuffer[MAX_SIZE];
			if (GetPrivateProfileString(subSection, NULL, "", buffer, MAX_SIZE, iniFile) > 0)
			{
				while (buffer[i] != 0)
				{
					ComponentKey = &buffer[i];
					if (GetPrivateProfileString(subSection, ComponentKey, "", ComponentKeyBuffer, MAX_SIZE, iniFile) > 0)
					{
						aWidget->options.name[counter] = new char[sizeof(ComponentKey)];
						strcpy(aWidget->options.name[counter],ComponentKey);
						aWidget->options.value[counter] = new char[sizeof(ComponentKeyBuffer)];
						strcpy(aWidget->options.value[counter],ComponentKeyBuffer);	
						counter++;
					}
					while (buffer[i] != 0)
						i++;
					i++;
				}
			}
			aWidget->numOfOptions = counter;
		}	
		else {
			aWidget->numOfOptions = 0;
		}
	}
}
*/
void CWizardMachineApp::GenerateList(CString action, WIDGET* targetWidget, CString parentDirPath)
{
	WIDGET* curWidget = targetWidget;

	CFileFind fileList;
	BOOL dirFound = fileList.FindFile(CString(currDirPath) + parentDirPath);

	if(curWidget->type == "ListBox")
	{
		((CListBox*)curWidget->control)->ResetContent();
	}

	if(curWidget->type == "ComboBox")
	{
		((CComboBox*)curWidget->control)->ResetContent();
	}


	while (dirFound)
	{
	    dirFound = fileList.FindNextFile();

		if (action == "GenerateFileList") 
		{
		    if (!fileList.IsDirectory())  // skip if this is a dir
		    {
				CString tmpFile = fileList.GetFileName();
				if(curWidget->type == "ListBox")
				{
					((CListBox*)curWidget->control)->AddString(tmpFile);
				}

				if(curWidget->type == "ComboBox")
				{
					((CComboBox*)curWidget->control)->AddString(tmpFile);
				}
			}
		}

		if (action == "GenerateDirList") 
		{
		    if (fileList.IsDirectory())  // skip if this is not a dir
		    {
				CString tmpFile = fileList.GetFileName();
				if (!(tmpFile == "." || tmpFile == "..")) {
					if(curWidget->type == "ListBox")
					{
						((CListBox*)curWidget->control)->AddString(tmpFile);
					}

					if(curWidget->type == "ComboBox")
					{
						((CComboBox*)curWidget->control)->AddString(tmpFile);
					}
				}
			}
		}

	}

	fileList.Close();

	if(curWidget->type == "ListBox")
	{
		((CListBox*)curWidget->control)->SetCurSel(0);
	}
	else if(curWidget->type == "ComboBox")
	{
		((CComboBox*)curWidget->control)->SetCurSel(0);
	}
}

void CWizardMachineApp::HelpWiz()
{


			CWizHelp hlpdlg;
			hlpdlg.DoModal();
}
