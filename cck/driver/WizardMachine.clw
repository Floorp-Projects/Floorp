; CLW file contains information for the MFC ClassWizard

[General Info]
Version=1
LastClass=CWizHelp
LastTemplate=CDialog
NewFileInclude1=#include "stdafx.h"
NewFileInclude2=#include "WizardMachine.h"

ClassCount=15
Class1=CWizardMachineApp
Class2=CWizardMachineDlg
Class3=CAboutDlg

ResourceCount=16
Resource1=IDD_HELP_DLG
Resource2=IDR_MAINFRAME
Resource3=IDD_IMG_DLG
Resource4=IDD_SUMMARY
Class4=CWizardUI
Class5=CPropSheet
Resource5=IDD_NEW_DIALOG
Class6=CImageDialog
Class7=CNavText
Resource6=IDD_BASE_DIALOG
Resource7=IDD_PROGRESS_DLG
Resource8=IDD_IMAGE_DIALOG
Resource9=IDD_DIALOG1
Class8=CProgressDialog
Class9=CProgDlgThread
Resource10=IDD_CREATE_DIALOG
Class10=CCreateDialog
Resource11=IDD_DIALOG112
Class11=CNewDialog
Class12=CHelpDlg
Class13=CImgDlg
Resource12=IDD_WIZARDMACHINE_DIALOG
Resource13=IDD_NEWCONFIG_DIALOG
Resource14=IDD_ABOUTBOX
Class14=CSumDlg
Resource15=1536
Class15=CWizHelp
Resource16=IDD_WIZ_HELP

[CLS:CWizardMachineApp]
Type=0
HeaderFile=WizardMachine.h
ImplementationFile=WizardMachine.cpp
Filter=N
LastObject=CWizardMachineApp

[CLS:CWizardMachineDlg]
Type=0
HeaderFile=WizardMachineDlg.h
ImplementationFile=WizardMachineDlg.cpp
Filter=D
LastObject=CWizardMachineDlg
BaseClass=CDialog
VirtualFilter=dWC

[CLS:CAboutDlg]
Type=0
HeaderFile=WizardMachineDlg.h
ImplementationFile=WizardMachineDlg.cpp
Filter=D
LastObject=CAboutDlg

[DLG:IDD_ABOUTBOX]
Type=1
Class=CAboutDlg
ControlCount=4
Control1=IDC_STATIC,static,1342177283
Control2=IDC_STATIC,static,1342308480
Control3=IDC_STATIC,static,1342308352
Control4=IDOK,button,1342373889

[DLG:IDD_WIZARDMACHINE_DIALOG]
Type=1
Class=CWizardMachineDlg
ControlCount=4
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816
Control3=IDC_STATIC,static,1342308352
Control4=IDC_STATIC,static,1342308352

[DLG:IDD_BASE_DIALOG]
Type=1
Class=CWizardUI
ControlCount=0

[CLS:CWizardUI]
Type=0
HeaderFile=WizardUI.h
ImplementationFile=WizardUI.cpp
BaseClass=CPropertyPage
Filter=D
VirtualFilter=idWC
LastObject=CWizardUI

[CLS:CPropSheet]
Type=0
HeaderFile=PropSheet.h
ImplementationFile=PropSheet.cpp
BaseClass=CPropertySheet
Filter=W
LastObject=CPropSheet

[DLG:1536]
Type=1
Class=?
ControlCount=7
Control1=1120,listbox,1084297299
Control2=65535,static,1342308352
Control3=1088,static,1342308480
Control4=1091,static,1342308352
Control5=1137,combobox,1352729427
Control6=IDOK,button,1342373889
Control7=IDCANCEL,button,1342373888

[DLG:IDD_IMAGE_DIALOG]
Type=1
Class=CImageDialog
ControlCount=2
Control1=IDCANCEL,button,1342242816
Control2=IDC_BUTTON1,button,1342242816

[CLS:CImageDialog]
Type=0
HeaderFile=ImageDialog.h
ImplementationFile=ImageDialog.cpp
BaseClass=CDialog
Filter=D
VirtualFilter=dWC
LastObject=CImageDialog

[CLS:CNavText]
Type=0
HeaderFile=NavText.h
ImplementationFile=NavText.cpp
BaseClass=CStatic
Filter=W
VirtualFilter=WC
LastObject=CNavText

[DLG:IDD_NEWCONFIG_DIALOG]
Type=1
Class=?
ControlCount=5
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816
Control3=IDC_STATIC,static,1342308352
Control4=IDC_EDIT1,edit,1350631552
Control5=IDC_STATIC,static,1342308352

[DLG:IDD_DIALOG112]
Type=1
Class=?
ControlCount=5
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816
Control3=IDC_STATIC,static,1342308352
Control4=IDC_EDIT1,edit,1350631552
Control5=IDC_STATIC,static,1342308352

[DLG:IDD_PROGRESS_DLG]
Type=1
Class=CProgressDialog
ControlCount=2
Control1=IDC_PROGRESS1,msctls_progress32,1350565888
Control2=IDC_PROGESSTEXT_STATIC,static,1342308353

[CLS:CProgressDialog]
Type=0
HeaderFile=ProgressDialog.h
ImplementationFile=ProgressDialog.cpp
BaseClass=CDialog
Filter=D
LastObject=CProgressDialog
VirtualFilter=dWC

[CLS:CProgDlgThread]
Type=0
HeaderFile=ProgDlgThread.h
ImplementationFile=ProgDlgThread.cpp
BaseClass=CWinThread
Filter=N
LastObject=CProgDlgThread

[DLG:IDD_CREATE_DIALOG]
Type=1
Class=CCreateDialog
ControlCount=0

[CLS:CCreateDialog]
Type=0
HeaderFile=CreateDialog.h
ImplementationFile=CreateDialog.cpp
BaseClass=CDialog
Filter=D
LastObject=CCreateDialog
VirtualFilter=dWC

[DLG:IDD_NEW_DIALOG]
Type=1
Class=CNewDialog
ControlCount=5
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816
Control3=IDC_EDIT1,edit,1350631552
Control4=IDC_TITLE_TEXT,static,1342308352
Control5=IDC_BASE_TEXT,static,1342308352

[CLS:CNewDialog]
Type=0
HeaderFile=NewDialog.h
ImplementationFile=NewDialog.cpp
BaseClass=CDialog
Filter=D
LastObject=CNewDialog
VirtualFilter=dWC

[DLG:IDD_IMG_DLG]
Type=1
Class=CImgDlg
ControlCount=2
Control1=IDC_HELP_BUTTON,button,1342242817
Control2=IDCANCEL,button,1342242816

[CLS:CImgDlg]
Type=0
HeaderFile=ImgDlg.h
ImplementationFile=ImgDlg.cpp
BaseClass=CDialog
Filter=D
VirtualFilter=dWC
LastObject=CImgDlg

[DLG:IDD_HELP_DLG]
Type=1
Class=CHelpDlg
ControlCount=2
Control1=IDCANCEL,button,1342242816
Control2=IDC_EDIT1,edit,1353779332

[DLG:IDD_DIALOG1]
Type=1
Class=?
ControlCount=2
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816

[CLS:CHelpDlg]
Type=0
HeaderFile=HelpDlg.h
ImplementationFile=HelpDlg.cpp
BaseClass=CDialog
Filter=D
VirtualFilter=dWC
LastObject=CHelpDlg

[DLG:IDD_SUMMARY]
Type=1
Class=CSumDlg
ControlCount=2
Control1=IDCANCEL,button,1342242816
Control2=IDC_EDIT1,edit,1353779396

[CLS:CSumDlg]
Type=0
HeaderFile=SumDlg.h
ImplementationFile=SumDlg.cpp
BaseClass=CDialog
Filter=D
VirtualFilter=dWC
LastObject=IDC_EDIT1

[DLG:IDD_WIZ_HELP]
Type=1
Class=CWizHelp
ControlCount=1
Control1=IDCANCEL,button,1342242816

[CLS:CWizHelp]
Type=0
HeaderFile=WizHelp.h
ImplementationFile=WizHelp.cpp
BaseClass=CDialog
Filter=D
VirtualFilter=dWC
LastObject=CWizHelp

