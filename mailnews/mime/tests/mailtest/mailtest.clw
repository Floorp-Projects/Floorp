; CLW file contains information for the MFC ClassWizard

[General Info]
Version=1
LastClass=CFileOpen
LastTemplate=CFileDialog
NewFileInclude1=#include "stdafx.h"
NewFileInclude2=#include "mailtest.h"

ClassCount=7
Class1=CMailtestApp
Class2=CMailtestDlg
Class3=CAboutDlg

ResourceCount=4
Resource1=IDR_MENU
Resource2=IDR_MAINFRAME
Resource3=IDD_ABOUTBOX
Class5=OpenFile
Class6=COpenFile
Class7=CFileOpen
Resource4=IDD_MAILTEST_DIALOG

[CLS:CMailtestApp]
Type=0
HeaderFile=mailtest.h
ImplementationFile=mailtest.cpp
Filter=N

[CLS:CMailtestDlg]
Type=0
HeaderFile=mailtestDlg.h
ImplementationFile=mailtestDlg.cpp
Filter=W
BaseClass=CDialog
VirtualFilter=dWC

[CLS:CAboutDlg]
Type=0
HeaderFile=mailtestDlg.h
ImplementationFile=mailtestDlg.cpp
Filter=D

[DLG:IDD_ABOUTBOX]
Type=1
Class=CAboutDlg
ControlCount=4
Control1=IDC_STATIC,static,1342308480
Control2=IDC_STATIC,static,1342308352
Control3=IDOK,button,1342373889
Control4=IDC_STATIC,static,1342177283

[DLG:IDD_MAILTEST_DIALOG]
Type=1
Class=CMailtestDlg
ControlCount=4
Control1=IDC_BROWSER_MARKER,static,1342177288
Control2=IDC_HEADERS,listbox,1352728835
Control3=IDC_URL,edit,1350631552
Control4=IDC_BUTTON1,button,1342242816

[MNU:IDR_MENU]
Type=1
Class=?
Command1=ID_OPEN_MENU
Command2=ID_EXIT_MENU
Command3=ID_HELP_MENU
Command4=ID_ABOUT_MENU
CommandCount=4

[CLS:OpenFile]
Type=0
HeaderFile=OpenFile.h
ImplementationFile=OpenFile.cpp
BaseClass=CFileDialog
Filter=D

[CLS:COpenFile]
Type=0
HeaderFile=OpenFile.h
ImplementationFile=OpenFile.cpp
BaseClass=CFileDialog
Filter=D

[CLS:CFileOpen]
Type=0
HeaderFile=FileOpen.h
ImplementationFile=FileOpen.cpp
BaseClass=CFileDialog
Filter=D

