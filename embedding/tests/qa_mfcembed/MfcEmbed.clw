; CLW file contains information for the MFC ClassWizard

[General Info]
Version=1
LastClass=CBrowserView
LastTemplate=CDialog
NewFileInclude1=#include "stdafx.h"
NewFileInclude2=#include "mfcembed.h"
LastPage=0

ClassCount=10
Class1=CBrowserFrame
Class2=CBrowserView
Class3=CMfcEmbedApp
Class4=CPreferences
Class5=CStartupPrefsPage
Class6=CPrintProgressDialog
Class7=CNewProfileDlg
Class8=CRenameProfileDlg
Class9=CProfilesDlg
Class10=CUrlDialog

ResourceCount=17
Resource1=IDR_MAINFRAME
Resource2=IDR_CTXMENU_LINK
Resource3=IDR_CTXMENU_TEXT
Resource4=IDR_CTXMENU_DOCUMENT
Resource5=IDR_CTXMENU_IMAGE
Resource6=IDD_ABOUTBOX
Resource7=IDD_PROMPT_PASSWORD_DIALOG
Resource8=IDD_PROMPT_USERPASS_DIALOG
Resource9=IDD_PROFILES
Resource10=IDD_PROFILE_NEW
Resource11=IDD_PROFILE_RENAME
Resource12=IDD_FINDDLG
Resource13=IDD_PRINT_PROGRESS_DIALOG
Resource14=IDD_PREFS_START_PAGE
Resource15=IDD_URLDIALOG
Resource16=IDD_PROMPT_DIALOG
Resource17=IDD_DIALOG1

[CLS:CBrowserFrame]
Type=0
BaseClass=CFrameWnd
HeaderFile=BrowserFrm.h
ImplementationFile=BrowserFrm.cpp
LastObject=CBrowserFrame

[CLS:CBrowserView]
Type=0
BaseClass=CWnd
HeaderFile=BrowserView.h
ImplementationFile=BrowserView.cpp
Filter=W
VirtualFilter=WC
LastObject=ID_VERIFYBUGS_70228

[CLS:CMfcEmbedApp]
Type=0
BaseClass=CWinApp
HeaderFile=MfcEmbed.h
ImplementationFile=MfcEmbed.cpp
LastObject=CMfcEmbedApp

[CLS:CPreferences]
Type=0
BaseClass=CPropertySheet
HeaderFile=Preferences.h
ImplementationFile=Preferences.cpp

[CLS:CStartupPrefsPage]
Type=0
BaseClass=CPropertyPage
HeaderFile=Preferences.h
ImplementationFile=Preferences.cpp

[CLS:CPrintProgressDialog]
Type=0
BaseClass=CDialog
HeaderFile=PrintProgressDialog.h
ImplementationFile=PrintProgressDialog.cpp

[CLS:CNewProfileDlg]
Type=0
BaseClass=CDialog
HeaderFile=ProfilesDlg.h
ImplementationFile=ProfilesDlg.cpp

[CLS:CRenameProfileDlg]
Type=0
BaseClass=CDialog
HeaderFile=ProfilesDlg.h
ImplementationFile=ProfilesDlg.cpp

[CLS:CProfilesDlg]
Type=0
BaseClass=CDialog
HeaderFile=ProfilesDlg.h
ImplementationFile=ProfilesDlg.cpp

[CLS:CUrlDialog]
Type=0
BaseClass=CDialog
HeaderFile=UrlDialog.h
ImplementationFile=UrlDialog.cpp

[DLG:IDD_PREFS_START_PAGE]
Type=1
Class=CStartupPrefsPage
ControlCount=7
Control1=IDC_STATIC,button,1342177287
Control2=IDC_RADIO_BLANK_PAGE,button,1342308361
Control3=IDC_RADIO_HOME_PAGE,button,1342177289
Control4=IDC_STATIC,button,1342177287
Control5=IDC_STATIC,static,1342308352
Control6=IDC_STATIC,static,1342308352
Control7=IDC_EDIT_HOMEPAGE,edit,1350631552

[DLG:IDD_PRINT_PROGRESS_DIALOG]
Type=1
Class=CPrintProgressDialog
ControlCount=3
Control1=IDCANCEL,button,1342242816
Control2=IDC_PPD_DOC_TITLE_STATIC,static,1342308352
Control3=IDC_PPD_DOC_TXT,static,1342308352

[DLG:IDD_PROFILE_NEW]
Type=1
Class=CNewProfileDlg
ControlCount=6
Control1=IDC_NEW_PROF_NAME,edit,1350631552
Control2=IDC_LOCALE_COMBO,combobox,1478557955
Control3=IDOK,button,1342242817
Control4=IDCANCEL,button,1342242816
Control5=IDC_STATIC,static,1342308354
Control6=IDC_STATIC,static,1342308354

[DLG:IDD_PROFILE_RENAME]
Type=1
Class=CRenameProfileDlg
ControlCount=4
Control1=IDC_NEW_NAME,edit,1350631552
Control2=IDOK,button,1342242817
Control3=IDCANCEL,button,1342242816
Control4=IDC_STATIC,static,1342308354

[DLG:IDD_PROFILES]
Type=1
Class=CProfilesDlg
ControlCount=7
Control1=IDC_LIST1,listbox,1352728833
Control2=IDC_PROF_RENAME,button,1342242816
Control3=IDC_PROF_DELETE,button,1342242816
Control4=IDC_PROF_NEW,button,1342242816
Control5=IDOK,button,1342242817
Control6=IDCANCEL,button,1342242816
Control7=IDC_CHECK_ASK_AT_START,button,1342242819

[DLG:IDD_URLDIALOG]
Type=1
Class=CUrlDialog
ControlCount=4
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816
Control3=IDC_ENTER_URLTEXT,static,1342308352
Control4=IDC_URLFIELD,edit,1350631552

[MNU:IDR_MAINFRAME]
Type=1
Class=?
Command1=ID_NEW_BROWSER
Command2=ID_FILE_OPEN
Command3=ID_FILE_SAVE_AS
Command4=ID_FILE_PRINT
Command5=ID_APP_EXIT
Command6=ID_EDIT_CUT
Command7=ID_EDIT_COPY
Command8=ID_EDIT_PASTE
Command9=ID_EDIT_SELECT_ALL
Command10=ID_EDIT_SELECT_NONE
Command11=ID_EDIT_FIND
Command12=ID_MANAGE_PROFILES
Command13=ID_EDIT_PREFERENCES
Command14=ID_VIEW_SOURCE
Command15=ID_VIEW_INFO
Command16=ID_VIEW_TOOLBAR
Command17=ID_VIEW_STATUS_BAR
Command18=ID_NAV_BACK
Command19=ID_NAV_FORWARD
Command20=ID_NAV_HOME
Command21=ID_APP_ABOUT
Command22=ID_TESTS_CHANGEURL
Command23=ID_TESTS_GLOBALHISTORY
Command24=ID_TESTS_CREATEFILE
Command25=ID_TESTS_CREATEPROFILE
Command26=ID_INTERFACES_NSIFILE
Command27=ID_INTERFACES_NSISHISTORY
Command28=ID_TOOLS_REMOVEGHPAGE
Command29=ID_VERIFYBUGS_70228
CommandCount=29

[TB:IDR_MAINFRAME]
Type=1
Class=?
Command1=ID_NAV_BACK
Command2=ID_NAV_FORWARD
Command3=ID_NAV_RELOAD
Command4=ID_NAV_STOP
Command5=ID_NAV_HOME
Command6=ID_APP_ABOUT
CommandCount=6

[MNU:IDR_CTXMENU_DOCUMENT]
Type=1
Class=?
Command1=ID_NAV_BACK
Command2=ID_NAV_FORWARD
Command3=ID_NAV_RELOAD
Command4=ID_NAV_STOP
Command5=ID_VIEW_SOURCE
Command6=ID_VIEW_INFO
Command7=ID_FILE_SAVE_AS
Command8=ID_EDIT_SELECT_ALL
Command9=ID_EDIT_SELECT_NONE
Command10=ID_EDIT_COPY
CommandCount=10

[MNU:IDR_CTXMENU_IMAGE]
Type=1
Class=?
Command1=ID_NAV_BACK
Command2=ID_NAV_FORWARD
Command3=ID_NAV_RELOAD
Command4=ID_NAV_STOP
Command5=ID_VIEW_SOURCE
Command6=ID_VIEW_IMAGE
Command7=ID_SAVE_IMAGE_AS
Command8=ID_FILE_SAVE_AS
CommandCount=8

[MNU:IDR_CTXMENU_LINK]
Type=1
Class=?
Command1=ID_OPEN_LINK_IN_NEW_WINDOW
Command2=ID_NAV_BACK
Command3=ID_NAV_FORWARD
Command4=ID_NAV_RELOAD
Command5=ID_NAV_STOP
Command6=ID_VIEW_SOURCE
Command7=ID_FILE_SAVE_AS
Command8=ID_SAVE_LINK_AS
Command9=ID_COPY_LINK_LOCATION
CommandCount=9

[MNU:IDR_CTXMENU_TEXT]
Type=1
Class=?
Command1=ID_EDIT_CUT
Command2=ID_EDIT_COPY
Command3=ID_EDIT_PASTE
Command4=ID_EDIT_SELECT_ALL
CommandCount=4

[ACL:IDR_MAINFRAME]
Type=1
Class=?
Command1=ID_EDIT_COPY
Command2=ID_EDIT_FIND
Command3=ID_NEW_BROWSER
Command4=ID_FILE_OPEN
Command5=ID_FILE_SAVE_AS
Command6=ID_EDIT_PASTE
Command7=ID_EDIT_UNDO
Command8=ID_EDIT_CUT
Command9=ID_NEXT_PANE
Command10=ID_PREV_PANE
Command11=ID_EDIT_COPY
Command12=ID_EDIT_PASTE
Command13=ID_EDIT_CUT
Command14=ID_EDIT_UNDO
Command15=ID_NAV_STOP
CommandCount=15

[DLG:IDD_ABOUTBOX]
Type=1
Class=?
ControlCount=4
Control1=IDC_STATIC,static,1342177283
Control2=IDC_STATIC,static,1342308480
Control3=IDC_STATIC,static,1342308352
Control4=IDOK,button,1342373889

[DLG:IDD_PROMPT_DIALOG]
Type=1
Class=?
ControlCount=5
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816
Control3=IDC_PROMPT_TEXT,static,1342308352
Control4=IDC_PROMPT_ANSWER,edit,1350631552
Control5=IDC_CHECK_SAVE_PASSWORD,button,1342242819

[DLG:IDD_PROMPT_PASSWORD_DIALOG]
Type=1
Class=?
ControlCount=5
Control1=IDC_PROMPT_TEXT,static,1342308352
Control2=IDOK,button,1342242817
Control3=IDCANCEL,button,1342242816
Control4=IDC_PASSWORD,edit,1350631584
Control5=IDC_CHECK_SAVE_PASSWORD,button,1342242819

[DLG:IDD_PROMPT_USERPASS_DIALOG]
Type=1
Class=?
ControlCount=8
Control1=IDC_PROMPT_TEXT,static,1342308352
Control2=IDC_USERNAME_LABEL,static,1342308352
Control3=IDC_USERNAME,edit,1350631552
Control4=IDC_PASSWORD_LABEL,static,1342308352
Control5=IDC_PASSWORD,edit,1350631584
Control6=IDC_CHECK_SAVE_PASSWORD,button,1342242819
Control7=IDOK,button,1342242817
Control8=IDCANCEL,button,1342242816

[DLG:IDD_FINDDLG]
Type=1
Class=?
ControlCount=8
Control1=65535,static,1342308352
Control2=IDC_FIND_EDIT,edit,1350762624
Control3=IDC_MATCH_WHOLE_WORD,button,1342373891
Control4=IDC_WRAP_AROUND,button,1342373891
Control5=IDC_MATCH_CASE,button,1342242819
Control6=IDC_SEARCH_BACKWARDS,button,1342242819
Control7=IDOK,button,1342373889
Control8=IDCANCEL,button,1342242816

[DLG:IDD_DIALOG1]
Type=1
Class=?
ControlCount=2
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816

