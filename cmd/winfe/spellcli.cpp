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

#include "stdafx.h"
// the dialog box & string resources are in edtrcdll DLL
#include "edtrcdll\src\resource.h"
//#include "limits.h"
#include "edview.h"
#include "edt.h"
#include "spellcli.h"
#include "prefapi.h"
#include "edtrccln.h"
#include "nethelp.h"
#include "xp_help.h"

// typedef's for the spell checker library public API's

typedef ISpellChecker* (FAR SCAPI *SC_CREATE_PROC)();
typedef void (FAR SCAPI *SC_DESTROY_PROC)(ISpellChecker *pSpellChecker);

// Preference strings
static LPCTSTR LanguagePref = "SpellChecker.DefaultLanguage";
static LPCTSTR DialectPref = "SpellChecker.DefaultDialect";

static void ShowSpellCheckerError(CWnd *pParentWnd, int ErrorCode);

// Spell Checker loading and initialization error codes
#define SPELL_ERR_NOT_INSTALLED     1
#define SPELL_ERR_COULD_NOT_LOAD    2
#define SPELL_ERR_INSTANCE_ACTIVE   3
#define SPELL_ERR_CORRUPT           4

// Returns the spell checker install directory

static LPCTSTR GetSpellCheckerDir()
{
    static CString Dir;

    if (Dir.IsEmpty())
    {
        char ModuleFile[_MAX_PATH];

        if (GetModuleFileName(AfxGetInstanceHandle(), ModuleFile, sizeof(ModuleFile)))
        {
            Dir = ModuleFile;
            int i = Dir.ReverseFind('\\');
            if (i != -1)
            {
                Dir.SetAt(i, 0);
                // The += operator doesn't copy at the new EOS location if I don't do this.
                Dir = (LPCTSTR)Dir;   
                Dir += "\\spellchk\\";
            }
            else
                Dir.Empty();
        }
    }

    return Dir;
}

static LPCTSTR GetSpellCheckerDllPath()
{
	static CString Path;
	
	Path = GetSpellCheckerDir();   
	
#ifdef XP_WIN32
	Path += "sp3240.dll";
#else
	Path += "sp1640.dll";
#endif
	
	return Path;
}               

// Returns the personal dictionary directory

static LPCTSTR GetPersonalDicDir()
{
    static CString Dir;
    
    if (Dir.IsEmpty())
    {
        Dir = (theApp.m_UserDirectory.IsEmpty() ? (LPCTSTR)GetSpellCheckerDir() : (LPCTSTR)theApp.m_UserDirectory);

        // make sure there is a '\' at the end
        if (Dir[Dir.GetLength() - 1] != '\\')
            Dir += "\\";
    }

    return Dir;
}

// Returns the personal dictionary filename

static LPCTSTR GetPersonalDicFilename()
{
    static CString Filename;

    if (Filename.IsEmpty())
    {
        char *Pref = NULL;
        PREF_CopyCharPref("SpellChecker.PersonalDictionary", &Pref);

        if (Pref == NULL || *Pref == 0)
        {
            Filename = GetPersonalDicDir();
            Filename += "custom.dic";
        }
        else
            Filename = Pref;

        if (Pref != NULL)
            XP_FREE(Pref);
    }

    return Filename;
}

// This function returns the "default language & dialect" values from the registry. These
// default settings are only used when more than one languages/dialects are available.
// We only need to handle the cases where the marketing spec requires multiple languages/
// dialects.
static void GetDefaultLanguage(int32 &Language, int32 &Dialect)
{
	CString csLanguage = theApp.GetProfileString("Spelling", "Default Language", NULL);
    if (csLanguage.IsEmpty())
        return;

    // For the English version.
    if (stricmp(csLanguage, "US English") == 0)
    {
        Language = L_ENGLISH;
        Dialect = D_US_ENGLISH;
    }
    else if (stricmp(csLanguage, "UK English") == 0)
    {
        Language = L_ENGLISH;
        Dialect = D_UK_ENGLISH;
    }

    // For the Portuguese version.
    else if (stricmp(csLanguage, "Brazilian Portuguese") == 0)
    {
        Language = L_PORTUGUESE;
        Dialect = D_BRAZILIAN;
    }
    else if (stricmp(csLanguage, "European Portuguese") == 0)
    {
        Language = L_PORTUGUESE;
        Dialect = D_EUROPEAN;
    }

    // For the Spanish version.
    else if (stricmp(csLanguage, "Spanish") == 0)
    {
        Language = L_SPANISH;
        Dialect = D_DEFAULT;
    }
    else if (stricmp(csLanguage, "Catalan") == 0)
    {
        Language = L_CATALAN;
        Dialect = D_DEFAULT;
    }
}

#ifdef XP_WIN32
#define IsValidHandle(h)	(h != NULL)
#else
#define IsValidHandle(h)	(h > HINSTANCE_ERROR)
#endif

static void ShowNoSpellCheckerDlg(CWnd *pParentWnd);


static LPCTSTR GetLanguageString(int Language, int Dialect)
{
    UINT StringId;

    switch (Language)
    {
    case L_CZECH:       StringId = IDS_SPELL_CZECH; break;
    case L_RUSSIAN:     StringId = IDS_SPELL_RUSSIAN; break;
    case L_CATALAN:     StringId = IDS_SPELL_CATALAN; break;
    case L_HUNGARIAN:   StringId = IDS_SPELL_HUNGARIAN; break;
    case L_FRENCH:      StringId = IDS_SPELL_FRENCH; break;
    case L_GERMAN:      StringId = IDS_SPELL_GERMAN; break;
    case L_SWEDISH:     StringId = IDS_SPELL_SWEDISH; break;
    case L_SPANISH:     StringId = IDS_SPELL_SPANISH; break;
    case L_ITALIAN:     StringId = IDS_SPELL_ITALIAN; break;
    case L_DANISH:      StringId = IDS_SPELL_DANISH; break;
    case L_DUTCH:       StringId = IDS_SPELL_DUTCH; break;

    case L_PORTUGUESE:  if (Dialect == D_BRAZILIAN)
                            StringId = IDS_SPELL_PORTUGUESE_BRAZILIAN;
                        else if (Dialect == D_EUROPEAN)
                            StringId = IDS_SPELL_PORTUGUESE_EUROPEAN;
                        else
                            StringId = IDS_SPELL_PORTUGUESE; 
                        break;

    case L_NORWEGIAN:   if (Dialect == D_BOKMAL)
                            StringId = IDS_SPELL_NORWEGIAN_BOKMAL;
                        else if (Dialect == D_NYNORSK)
                            StringId = IDS_SPELL_NORWEGIAN_NYNORSK;
                        else
                            StringId = IDS_SPELL_NORWEGIAN;
                        break;

    case L_FINNISH:     StringId = IDS_SPELL_FINNISH; break;
    case L_GREEK:       StringId = IDS_SPELL_GREEK; break;

    case L_ENGLISH:     if (Dialect == D_US_ENGLISH)
                            StringId = IDS_SPELL_ENGLISH_US;
                        else if (Dialect == D_UK_ENGLISH)
                            StringId = IDS_SPELL_ENGLISH_UK;
                        else
                            StringId = IDS_SPELL_ENGLISH;
                        break;

    case L_AFRIKAANS:   StringId = IDS_SPELL_AFRIKAANS; break;
    case L_POLISH:      StringId = IDS_SPELL_POLISH; break;

    default:            StringId = IDS_SPELL_UNKNOWN_LANGUAGE;
                        ASSERT(FALSE);
    }

	// Resource switcher object: It switches the default resource handle to the 
	// editorXX.dll, cause that's where the string resources are. The resource
    // handle is automatically restored in the destructor.
    CEditorResourceSwitcher ResourceSwitcher;

    static CString theString;
    theString.LoadString(StringId);
    return theString;
}

/////////////////////////////////////////////////////////////////////////////
// CSpellCheckerDlg dialog

CSpellCheckerDlg::CSpellCheckerDlg(ISpellChecker *pSpellChecker, CSpellCheckerClient *pClient, 
                                   CWnd* pParent /*=NULL*/)
    : CDialog(IDD, pParent)
{
	//{{AFX_DATA_INIT(CSpellCheckerDlg)
	m_NewWord = _T("");
	//}}AFX_DATA_INIT

	m_pSpellChecker = pSpellChecker;
	m_pClient = pClient;
}

void CSpellCheckerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSpellCheckerDlg)
    DDX_Control(pDX, IDC_SUGGESTIONS, m_SuggestionsListBox);
	//}}AFX_DATA_MAP
}

void CSpellCheckerDlg::SetAlternatives()
{
    CString CurrString;
    GetDlgItemText(IDC_NEW_WORD, CurrString);

	// Delete any existing strings
	m_SuggestionsListBox.ResetContent();

    // Get alternative strings
	int index;
    m_NumSuggestions = m_pSpellChecker->GetNumAlternatives(CurrString);
    for (int i = 0; i < m_NumSuggestions; i++)
    {
        char AltString[100];

        int RetVal = m_pSpellChecker->GetAlternative(i, AltString, sizeof(AltString));
        ASSERT(RetVal == 0 && strlen(AltString) > 0);

        index = m_SuggestionsListBox.AddString(AltString);
		m_SuggestionsListBox.SetItemData(index, 0);
    }

    if (m_NumSuggestions > 0)
    {
        m_SuggestionsListBox.SetCurSel(0);
        m_SuggestionsListBox.SetFocus();

        // Make the Change button the default, cause a correction is selected to be applied.
        GotoDlgCtrl(GetDlgItem(IDC_CHANGE));
    }
    else
	{
        index = m_SuggestionsListBox.AddString(m_strNoSuggestions);
		m_SuggestionsListBox.SetItemData(index, 1);

        GotoDlgCtrl(GetDlgItem(IDC_NEW_WORD));
	}
}

// posted message to set the initial focus
#define WM_SET_INITIAL_FOCUS    WM_USER + 1000

BEGIN_MESSAGE_MAP(CSpellCheckerDlg, CDialog)
	//{{AFX_MSG_MAP(CSpellCheckerDlg)
	ON_BN_CLICKED(IDC_CHANGE, OnChange)
	ON_BN_CLICKED(IDC_IGNORE, OnIgnore)
	ON_EN_CHANGE(IDC_NEW_WORD, OnChangeNewWord)
	ON_EN_SETFOCUS(IDC_NEW_WORD, OnSetFocusNewWord)
	ON_BN_CLICKED(IDC_IGNORE_ALL, OnIgnoreAll)
	ON_BN_CLICKED(IDC_CHANGE_ALL, OnChangeAll)
	ON_BN_CLICKED(IDC_CHECK_WORD, OnCheckWord)
	ON_BN_CLICKED(IDC_ADD, OnAdd)
	ON_LBN_DBLCLK(IDC_SUGGESTIONS, OnChange)
	ON_LBN_SETFOCUS(IDC_SUGGESTIONS, OnSetFocusSuggestions)
	ON_LBN_SELCHANGE(IDC_SUGGESTIONS, OnSelChangeSuggestions)
	ON_BN_CLICKED(IDC_EDIT_DICTIONARY, OnEditDictionary)
	ON_CBN_SELENDOK(IDC_LANGUAGE, OnSelendokLanguage)
	ON_BN_CLICKED(ID_HELP, OnHelp)
	//}}AFX_MSG_MAP
    ON_MESSAGE(WM_SET_INITIAL_FOCUS, OnSetInitialFocus)
#ifdef XP_WIN32
    ON_WM_HELPINFO()
#endif //XP_WIN32
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSpellCheckerDlg message handlers

BOOL CSpellCheckerDlg::OnInitDialog() 
{
    // Load strings from the resource DLL
    // The default resource handle is already set to the editor resource DLL

    m_strChange.LoadString(IDS_SPELL_CHANGE);
    m_strChangeAll.LoadString(IDS_SPELL_CHANGE_ALL);
    m_strDelete.LoadString(IDS_SPELL_DELETE);
    m_strDeleteAll.LoadString(IDS_SPELL_DELETE_ALL);
    m_strNoSuggestions.LoadString(IDS_SPELL_NO_SUGGESTIONS);
    m_strStop.LoadString(IDS_SPELL_STOP);
    m_strDone.LoadString(IDS_SPELL_DONE);
    m_strCorrectSpelling.LoadString(IDS_CORRECT_SPELLING);

    // Don't need the editor resource handle any more. Switch back to the app.
    m_DlgResource.Reset();

	CDialog::OnInitDialog();

    InitLanguageList();

    // set initial focus
    PostMessage(WM_SET_INITIAL_FOCUS, 0, 0);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

LRESULT CSpellCheckerDlg::OnSetInitialFocus(WPARAM wParam, LPARAM lParam)
{
    GetFirstError();

    return 0L;
}

void CSpellCheckerDlg::OnCancel() 
{
	CDialog::OnCancel();
}

void CSpellCheckerDlg::OnChangeNewWord() 
{
    GetDlgItem(IDC_CHANGE)->EnableWindow(TRUE);
    GetDlgItem(IDC_CHANGE_ALL)->EnableWindow(TRUE);

    GetDlgItemText(IDC_NEW_WORD, m_NewWord);

    UpdateChangeButton();       // update the label of the Change buttons

    BOOL IsEmpty = m_NewWord.IsEmpty();
    GetDlgItem(IDC_CHECK_WORD)->EnableWindow(!IsEmpty);
    GetDlgItem(IDC_ADD)->EnableWindow(!IsEmpty);
}

void CSpellCheckerDlg::OnSetFocusNewWord() 
{
    GetDlgItemText(IDC_NEW_WORD, m_NewWord);

    m_SuggestionsListBox.SetCurSel(-1);

    UpdateChangeButton();       // update the label of the Change buttons
}

void CSpellCheckerDlg::UpdateChangeButton()
{
    SetDlgItemText(IDC_CHANGE, m_NewWord.IsEmpty() ? m_strDelete : m_strChange);
    SetDlgItemText(IDC_CHANGE_ALL, m_NewWord.IsEmpty() ? m_strDeleteAll : m_strChangeAll);
}

void CSpellCheckerDlg::OnSetFocusSuggestions() 
{
    int Selection = m_SuggestionsListBox.GetCurSel();
    if (Selection != LB_ERR)
        m_SuggestionsListBox.GetText(Selection, m_NewWord);
    else if (m_NumSuggestions > 0)
    {
        m_SuggestionsListBox.SetCurSel(0);
        m_SuggestionsListBox.GetText(0, m_NewWord);
        UpdateChangeButton();       // update the label of the Change buttons
    }
    else
        GetDlgItem(IDC_NEW_WORD)->SetFocus();
}

void CSpellCheckerDlg::OnSelChangeSuggestions() 
{
    int Selection = m_SuggestionsListBox.GetCurSel();

    // check for the (no suggestions) string
    if (m_SuggestionsListBox.GetItemData(Selection) != 1)
        m_SuggestionsListBox.GetText(Selection, m_NewWord);
    else
        GetDlgItem(IDC_NEW_WORD)->SetFocus();
}

void CSpellCheckerDlg::OnIgnore() 
{
    CWaitCursor WaitCursor; // Show the Wait cursor.

	m_pClient->IgnoreHilitedText(FALSE);
	GetNextError();
}

void CSpellCheckerDlg::OnIgnoreAll() 
{
    CWaitCursor WaitCursor; // Show the Wait cursor.

    m_pSpellChecker->IgnoreWord(m_MisspelledWord);

	m_pClient->IgnoreHilitedText(TRUE);
	GetNextError();   
}

void CSpellCheckerDlg::OnChange() 
{
    // Close dialog box if no more misspelled word
    if (m_MisspelledWord.IsEmpty())
        EndDialog(IDOK);
    else
    	CommonOnChange(FALSE);	
}

void CSpellCheckerDlg::OnChangeAll() 
{
	CommonOnChange(TRUE);	
}

void CSpellCheckerDlg::CommonOnChange(BOOL ChangeAll) 
{
    CWaitCursor WaitCursor; // Show the Wait cursor.

    // pasting NULL text, that is deleting mispelled word
    m_pClient->ReplaceHilitedText(m_NewWord, ChangeAll);
	GetNextError();  
}

void CSpellCheckerDlg::OnCheckWord()
{
    CString String;
    GetDlgItemText(IDC_NEW_WORD, String);

	// If first make sure the new word is ok
	if (!String.IsEmpty()) 
	{
        // the user may have entered multiple words
        CString ErrMsg;
        char *Separator = " ";
        char *Token = XP_STRTOK((char*)(LPCTSTR)String, Separator);
        
        while (Token != NULL)
        {
            if (!m_pSpellChecker->CheckWord(Token))
            {
    		    SetAlternatives();
                return;
            }

            // get the next work in the string
            Token = XP_STRTOK(NULL, Separator);
        }

        // The new word(s) spelled correctly. Show feedback to user.
        // Display "correct spelling" in the Suggestions listbox and 
        // mark it so that the user can't select this string.
        m_SuggestionsListBox.ResetContent();
        int index = m_SuggestionsListBox.AddString(m_strCorrectSpelling);
        m_SuggestionsListBox.SetItemData(index, 1);
        m_NumSuggestions = 0;
        m_SuggestionsListBox.EnableWindow(TRUE);

        // Make the Change button the default cause the word is OK to be applied.
        GotoDlgCtrl(GetDlgItem(IDC_CHANGE));
    }
}

void CSpellCheckerDlg::OnAdd()
{
    CString String;
    GetDlgItemText(IDC_NEW_WORD, String);

    int Status = m_pSpellChecker->AddWordToPersonalDictionary(String);

    if (Status)
    {
        TRACE1("ISpellChecker::AddWordToPersonalDictionary() returned %d\n", Status);

        CString Msg;
        AfxFormatString1(Msg, IDS_ERR_ADD_WORD, GetPersonalDicFilename());
        MessageBox(Msg, NULL, MB_OK | MB_ICONSTOP);
    }
    else
    {
        m_pClient->ReplaceHilitedText(String, 
                                      String.Compare(m_MisspelledWord) == 0 ? TRUE : FALSE);
	    GetNextError(); 
    }
}

void CSpellCheckerDlg::GetNextError()
{
    ProcessError(m_pClient->GetNextError());
}

void CSpellCheckerDlg::GetFirstError()
{
    // Reenable disabled controls from the last pass. Doing this after calling 
    // ProcessError() was not setting m_NewWord properly for the first error 
    // after the Language was changed.
    m_MisspelledWord = " ";     // must be non-empty for the controls to be enabled
    ChangeState();

    ProcessError(m_pClient->GetFirstError());
}

void CSpellCheckerDlg::ProcessError(char *pMisspelledWord)
{
    m_MisspelledWord = pMisspelledWord;        // remember the misspelled word

    if (pMisspelledWord != NULL)
    {
        SetDlgItemText(IDC_NEW_WORD, pMisspelledWord);
        SetAlternatives();

        XP_FREE(pMisspelledWord);
    }    	
    else
        ChangeState();
}

void CSpellCheckerDlg::OnHelp() 
{
    NetHelp(HELP_SPELL_CHECK);
}

// OnHelpInfo - Invokes Help window when F1 key is pressed
#ifdef XP_WIN32
BOOL CSpellCheckerDlg::OnHelpInfo(HELPINFO *)//32bit messagemapping.
{
    OnHelp();
    return TRUE;
}
#endif//XP_WIN32


void CSpellCheckerDlg::OnEditDictionary()
{
    CPersonalDictionaryDlg Dlg(m_pSpellChecker, this);
    Dlg.DoModal();
}

void CSpellCheckerDlg::InitLanguageList()
{
    CComboBox *pLanguageCtrl = (CComboBox *)GetDlgItem(IDC_LANGUAGE);
    if (pLanguageCtrl == NULL)
    {
        ASSERT(FALSE);
        return;
    }

    int Language, Dialect;
    int Count = m_pSpellChecker->GetNumOfDictionaries();
    for (int i = 0; i < Count; i++)
    {
        if (m_pSpellChecker->GetDictionaryLanguage(i, Language, Dialect) == 0)
        {
            int Index = pLanguageCtrl->AddString(GetLanguageString(Language, Dialect));
            pLanguageCtrl->SetItemData(Index, MAKELONG(Language, Dialect));
        }
    }

    // Select the current default
    m_pSpellChecker->GetCurrentLanguage(Language, Dialect);

    if (Language == 0 && Dialect == 0)
        pLanguageCtrl->SetCurSel(0);
    else
    {
        DWORD ItemData = MAKELONG(Language, Dialect);
    
        for (int i = 0; i < Count; i++)
        {
            if (ItemData == pLanguageCtrl->GetItemData(i))
            {
                pLanguageCtrl->SetCurSel(i);
                break;
            }
        }
    }
}

void CSpellCheckerDlg::OnSelendokLanguage() 
{
    CComboBox *pLanguageCtrl = (CComboBox *)GetDlgItem(IDC_LANGUAGE);
    if (pLanguageCtrl == NULL)
    {
        ASSERT(FALSE);
        return;
    }

    int CurrLanguage, CurrDialect;
    int NewLanguage, NewDialect;
    if (m_pSpellChecker->GetCurrentLanguage(CurrLanguage, CurrDialect) != 0)
    {
        ASSERT(FALSE);
        return;
    }

    long ItemData = pLanguageCtrl->GetItemData(pLanguageCtrl->GetCurSel());
    NewLanguage = LOWORD(ItemData);
    NewDialect = HIWORD(ItemData);

    if (NewLanguage != CurrLanguage || NewDialect != CurrDialect)
    {
        PREF_SetIntPref(LanguagePref, NewLanguage);
        PREF_SetIntPref(DialectPref, NewDialect);

        m_pClient->ReprocessDocument();
        
        GetFirstError();
    }    	
}

void CSpellCheckerDlg::ChangeState() 
{
    BOOL HaveError = !m_MisspelledWord.IsEmpty();
    if (!HaveError)
        m_SuggestionsListBox.SetCurSel(-1);

    GetDlgItem(IDC_NEW_WORD)->EnableWindow(HaveError);
    GetDlgItem(IDC_SUGGESTIONS)->EnableWindow(HaveError);
    GetDlgItem(IDC_CHANGE_ALL)->EnableWindow(HaveError);
    GetDlgItem(IDC_IGNORE)->EnableWindow(HaveError);
    GetDlgItem(IDC_IGNORE_ALL)->EnableWindow(HaveError);
    GetDlgItem(IDC_CHECK_WORD)->EnableWindow(HaveError);
    GetDlgItem(IDC_ADD)->EnableWindow(HaveError);
    GetDlgItem(IDCANCEL)->EnableWindow(HaveError);
  
    // if no more errors, label the Change button "Done" and make it the default button
    if (!HaveError)
    {
        SetDlgItemText(IDC_CHANGE, m_strDone);
        GotoDlgCtrl(GetDlgItem(IDC_CHANGE));
    }
}

/****************************************************************************************
 * Spell Checker client class implementation.
 ****************************************************************************************/

// static instance count. We can only have a single instance of the spell checker.
int CSpellCheckerClient::m_InstanceCount = 0;

CSpellCheckerClient::CSpellCheckerClient(CWnd *pParentWnd) 
{
    m_hSpellCheckerDll = 0;
    m_pParentWnd = pParentWnd;
	m_pSpellChecker = NULL;
    m_pDictionaryDlg = NULL;
    m_BufferSize = 0;
    m_SelStart = m_SelEnd = 0;

    m_InstanceCount++;
}

CSpellCheckerClient::~CSpellCheckerClient()
{
    if (m_pSpellChecker && IsValidHandle(m_hSpellCheckerDll))
    {    	
     	SC_DESTROY_PROC SC_Destroy_proc = (SC_DESTROY_PROC)GetProcAddress(m_hSpellCheckerDll, "SC_DESTROY");
	    if (SC_Destroy_proc != NULL)
            SC_Destroy_proc(m_pSpellChecker);
        else
		    TRACE("Could not load resolve SC_DESTROY");
    }
    
    if (IsValidHandle(m_hSpellCheckerDll))
    	FreeLibrary(m_hSpellCheckerDll);

    m_InstanceCount--;
}

int CSpellCheckerClient::ProcessDocument()
{
#define Fail(ErrorCode) { ShowSpellCheckerError(m_pParentWnd, ErrorCode); return -1; }

    // Show the Wait cursor.
    CWaitCursor WaitCursor;

    // Make sure we don't have an instance of the spell checker already running.
    if (m_InstanceCount > 1)
        Fail(SPELL_ERR_INSTANCE_ACTIVE);

    // Load the DLL
    m_hSpellCheckerDll = LoadLibrary(GetSpellCheckerDllPath());
   	if (!IsValidHandle(m_hSpellCheckerDll))
    {
        // Check if the DLL exists
		if (_access(GetSpellCheckerDllPath(), 0) != 0)
			Fail(SPELL_ERR_NOT_INSTALLED)
		else
			Fail(SPELL_ERR_COULD_NOT_LOAD)
    }
 
    // resolve public API functions
	SC_CREATE_PROC SC_Create_proc = (SC_CREATE_PROC)GetProcAddress(m_hSpellCheckerDll, "SC_CREATE");
	if (SC_Create_proc == NULL)
		Fail(SPELL_ERR_CORRUPT);

    // instantiate spell checker server object
    m_pSpellChecker = (*SC_Create_proc)();
	if (m_pSpellChecker == NULL)
		Fail(SPELL_ERR_CORRUPT);

    // initialize the spell checker server
    int32 Language = 0;
    int32 Dialect = 0;

    // First see if any language preferences have been set
    PREF_GetIntPref(LanguagePref, &Language);
    PREF_GetIntPref(DialectPref, &Dialect);

    // If not, get the default language & dialect settings, if any.
    if (Language == 0)
        GetDefaultLanguage(Language, Dialect);

	if ((m_pSpellChecker)->Initialize(Language, Dialect, GetSpellCheckerDir(),  GetPersonalDicFilename()))
		Fail(SPELL_ERR_CORRUPT);

    // Get the text buffer from the document
    XP_HUGE_CHAR_PTR pBuf = GetBuffer();
    if (pBuf == NULL)
        return 0;                       // nothing to spell check

    m_BufferSize = XP_STRLEN(pBuf);     // remember the size

    // Is there a selection
    GetSelection(m_SelStart, m_SelEnd);

    // pass text buffer to the spell checker
    int retcode = m_pSpellChecker->SetBuf(pBuf, m_SelStart, m_SelEnd);
    TRACE1("m_pSpellChecker->SetBuf() returned %d\n", retcode);

    // release the buffer (the Spell Checker makes a local copy)
    XP_HUGE_FREE(pBuf);         

    if (retcode != 0)
        return -1;

    // Remove the Wait cursor, as we are ready to display the dialog box.
    WaitCursor.Restore();

    // show dialog box
    CSpellCheckerDlg Dlg(m_pSpellChecker, this, m_pParentWnd);
    int result = Dlg.DoModal() == IDOK ? 0 : -1;

    RemoveAllErrorHilites();

    return result;
}

int CSpellCheckerClient::ReprocessDocument()
{
    // Show the Wait cursor.
    CWaitCursor WaitCursor;

    if (m_pSpellChecker == NULL)
    {
        ASSERT(FALSE);
        return -1;
    }

    RemoveAllErrorHilites();

    // Get the latest language defaults
    int32 Language = 0;
    int32 Dialect = 0;

    PREF_GetIntPref(LanguagePref, &Language);
    PREF_GetIntPref(DialectPref, &Dialect);

    if (m_pSpellChecker->SetCurrentLanguage(Language, Dialect) == 0)
    {
        // Get the text buffer from the document
        XP_HUGE_CHAR_PTR pBuf = GetBuffer();
        if (pBuf == NULL)
            return 0;                       // nothing to spell check

        // If we were spell checking a selection in the previous pass,
        // adjust the selection for any corrections made in the last pass.
        if (m_SelEnd > 0)
        {
            m_SelEnd += (XP_STRLEN(pBuf) - m_BufferSize);
        }

        m_BufferSize = XP_STRLEN(pBuf);     // remember the size

        // pass text buffer to the spell checker
        int retcode = m_pSpellChecker->SetBuf(pBuf, m_SelStart, m_SelEnd);

        // release the buffer (the Spell Checker makes a local copy)
        XP_HUGE_FREE(pBuf);         

        if (retcode != 0)
            return -1;

        return 0;
    }

    return -1;
}

/////////////////////////////////////////////////////////////////////////////
// CHtmlSpellChecker - HTML document spell checker client object

CHtmlSpellChecker::CHtmlSpellChecker(MWContext *pMWContext, CNetscapeEditView *pHtmlView)
                        : CSpellCheckerClient(pHtmlView)
{
    m_pMWContext = pMWContext;
    m_pView = pHtmlView;
}

// Implementation of base class virtuals.

XP_HUGE_CHAR_PTR CHtmlSpellChecker::GetBuffer()
{
    // Get the document text
	return EDT_GetPositionalText(m_pMWContext); 
}

BOOL CHtmlSpellChecker::GetSelection(int32 &SelStart, int32 &SelEnd)
{
    char *pSelection;
   
    if ((pSelection = (char *)LO_GetSelectionText(m_pMWContext)) != NULL)
    {
        XP_FREE(pSelection);
        EDT_GetSelectionOffsets(m_pMWContext, &SelStart, &SelEnd);
        return TRUE;
    }
    else
        return FALSE;       // no selection
}

char *CHtmlSpellChecker::GetFirstError()
{
    // turn off refreshing and spell check the text.
    EDT_SetRefresh(m_pMWContext, FALSE);

    // underline the misspelled words
    EDT_CharacterData* pCharData = EDT_NewCharacterData();
    pCharData->mask = TF_SPELL;
    pCharData->values = TF_SPELL;

    unsigned long Offset, Len;
    while (m_pSpellChecker->GetNextMisspelledWord(Offset, Len) == 0)
	    EDT_SetCharacterDataAtOffset(m_pMWContext, pCharData, Offset, Len);
  
    XP_FREE(pCharData);

    // set sursor position at the beginning of document so that 
    // EDT_SelectNextMisspelledWord() to start at the beginning.
    EDT_BeginOfDocument(m_pMWContext, FALSE);
	EDT_SetRefresh(m_pMWContext, TRUE);

	m_pView->UpdateWindow();

    // Select and return the first mispelled word 
    if (EDT_SelectFirstMisspelledWord(m_pMWContext))
        return (char *)LO_GetSelectionText(m_pMWContext);
    else
        return NULL;
}

char *CHtmlSpellChecker::GetNextError()
{
    if (EDT_SelectNextMisspelledWord(m_pMWContext))
        return (char *)LO_GetSelectionText(m_pMWContext);
    else
        return FALSE;
}

void CHtmlSpellChecker::ReplaceHilitedText(const char *NewText, int AllInstances)
{
    char *pOldWord = (char *)LO_GetSelectionText(m_pMWContext);
    if (pOldWord != NULL)
    {
        EDT_ReplaceMisspelledWord(m_pMWContext, pOldWord, (char*)NewText, AllInstances);
        XP_FREE(pOldWord);
    }
    else
        ASSERT(FALSE);
}

void CHtmlSpellChecker::IgnoreHilitedText(int AllInstances)
{
    char *pOldWord = (char *)LO_GetSelectionText(m_pMWContext);
    if (pOldWord != NULL)
    {
        EDT_IgnoreMisspelledWord(m_pMWContext, pOldWord, AllInstances);
        XP_FREE(pOldWord);
    }
    else
        ASSERT(FALSE);
}

void CHtmlSpellChecker::RemoveAllErrorHilites()
{
    // ignore any unprocessed misspelled words 
    EDT_IgnoreMisspelledWord(m_pMWContext, NULL, TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// CPlainTextSpellChecker - Plain Text spell checker client object

CPlainTextSpellChecker::CPlainTextSpellChecker(CEdit *pTextView)
                        : CSpellCheckerClient(pTextView)
{
    m_pTextView = pTextView;
}

// Implentation of base class virtuals.

XP_HUGE_CHAR_PTR CPlainTextSpellChecker::GetBuffer()
{
    // Get the document text
    int32 Length = m_pTextView->GetWindowTextLength();
    if (Length == 0)
        Length = 1;     // return NULL;

    XP_HUGE_CHAR_PTR pText = (XP_HUGE_CHAR_PTR) XP_HUGE_ALLOC(Length + 1);
    if (pText != NULL)
        m_pTextView->GetWindowText(pText, Length + 1);

    return pText;
}

BOOL CPlainTextSpellChecker::GetSelection(int32 &SelStart, int32 &SelEnd)
{
    // hilight mispelled word
    int Start, End;
    m_pTextView->GetSel(Start, End);
    if (End > Start)
    {
        SelStart = Start;
        SelEnd = End;
        return TRUE;
    }
    else
        return FALSE;
}

char *CPlainTextSpellChecker::GetFirstError()
{
    return GetNextError();
}

char *CPlainTextSpellChecker::GetNextError()
{
    unsigned long Offset, Len;
    char *pMisspelledWord = NULL;

    if (m_pSpellChecker->GetNextMisspelledWord(Offset, Len) == 0)
    {
        // hilight mispelled word
        m_pTextView->SetSel((int)Offset, (int)(Offset+Len));
        m_pTextView->UpdateWindow();

        // Extract mispelled word 
        XP_HUGE_CHAR_PTR pBuf = GetBuffer();
        if (pBuf != NULL)
        {
            pMisspelledWord = (char *)XP_ALLOC(Len + 1);
            if (pMisspelledWord != NULL)
                XP_STRNCPY_SAFE(pMisspelledWord, (LPCTSTR)pBuf + Offset, Len+1);

            XP_HUGE_FREE(pBuf);
        }
    }

    return pMisspelledWord;
}

void CPlainTextSpellChecker::ReplaceHilitedText(const char *NewText, int AllInstances)
{
    if (m_pSpellChecker->ReplaceMisspelledWord(NewText, AllInstances) != 0)
        ASSERT(FALSE);      // maybe display an out of memory error

    unsigned long NewBufSize = m_pSpellChecker->GetBufSize();
    char *pNewBuf = (char *)XP_ALLOC(NewBufSize);
    if (pNewBuf == NULL)
    {
        ASSERT(FALSE);      // maybe display an out of memory error
        return;
    }
    m_pSpellChecker->GetBuf(pNewBuf, NewBufSize);
    m_pTextView->SetWindowText(pNewBuf);
    XP_FREE(pNewBuf);
}

void CPlainTextSpellChecker::IgnoreHilitedText(int AllInstances)
{
    // Unlike the HTML document, there are no error highlights to remove. 
    // However, if we need to ignore all subsequent instances of the word,
    // we need to tell the spell checker to ignore this word.

    if (AllInstances)
    {
        int StartChar, EndChar;
        m_pTextView->GetSel(StartChar, EndChar);

        CString Buf;
        m_pTextView->GetWindowText(Buf);
    
        int OldWordLen = EndChar - StartChar;
        CString OldWord;
        OldWord.Format("%.*s", OldWordLen, (LPCTSTR)Buf + StartChar);

        m_pSpellChecker->IgnoreWord(OldWord);
    }
}

void ShowSpellCheckerError(CWnd *pParentWnd, int ErrorCode)
{
    // Resource switcher object: It switches the default resource handle to the 
    // editorXX.dll, cause that's where the string resources are. The resource
    // handle is automatically restored in the destructor.
    CEditorResourceSwitcher ResourceSwitcher;

    CString Msg, Title;
    int MsgId;

    switch (ErrorCode)
    {
    case SPELL_ERR_NOT_INSTALLED:
        MsgId = IDS_SPELL_NOT_INSTALLED;
        break;
    case SPELL_ERR_COULD_NOT_LOAD:
        MsgId = IDS_SPELL_COULD_NOT_LOAD;
        break;
    case SPELL_ERR_INSTANCE_ACTIVE:
        MsgId = IDS_SPELL_INSTANCE_ACTIVE;
        break;
    case SPELL_ERR_CORRUPT:
    default:
        MsgId = IDS_SPELL_CORRUPT;
        break;
    }

    Msg.LoadString(MsgId);
    Title.LoadString(IDS_CHECK_SPELLING);
    pParentWnd->MessageBox(Msg, Title, MB_ICONSTOP | MB_OK);
}

/////////////////////////////////////////////////////////////////////////////
// CPersonalDictionaryDlg dialog

CPersonalDictionaryDlg::CPersonalDictionaryDlg(ISpellChecker *pSpellChecker, CWnd* pParent /*=NULL*/)
    : CDialog(IDD, pParent)
{
	m_pSpellChecker = pSpellChecker;
}

void CPersonalDictionaryDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPersonalDictionaryDlg)
	DDX_Control(pDX, IDC_WORD_LIST, m_WordList);
	//}}AFX_DATA_MAP
}

BOOL CPersonalDictionaryDlg::OnInitDialog() 
{
    // Don't need the editor resource handle any more. Switch back to the app.
    m_DlgResource.Reset();

	CDialog::OnInitDialog();

	char Word[128];
	if (m_pSpellChecker->GetFirstPersonalDictionaryWord(Word, sizeof(Word)) >= 0)
	{
		do 
			m_WordList.AddString(Word);
		while (m_pSpellChecker->GetNextPersonalDictionaryWord(Word, sizeof(Word)) >= 0);
	}

    EnableButtons();

	return TRUE;
}

void CPersonalDictionaryDlg::OnOK()
{
    m_pSpellChecker->ResetPersonalDictionary();

    CString Word;
    int Count = m_WordList.GetCount();

    for (int Index = 0; Index < Count; Index++)
    {
        m_WordList.GetText(Index, Word);
        ASSERT(Word.GetLength() > 0);
        m_pSpellChecker->AddWordToPersonalDictionary(Word);
    }
    
    CDialog::OnOK();
}

BEGIN_MESSAGE_MAP(CPersonalDictionaryDlg, CDialog)
	ON_BN_CLICKED(IDC_ADD, OnAddWord)
	ON_BN_CLICKED(IDC_REPLACE, OnReplaceWord)
	ON_BN_CLICKED(IDC_REMOVE, OnRemoveWord)
    ON_LBN_SELCHANGE(IDC_WORD_LIST, OnSelChangeWordList)
	ON_EN_CHANGE(IDC_NEW_WORD, EnableButtons)
	ON_BN_CLICKED(ID_HELP, OnHelp)
#ifdef XP_WIN32
    ON_WM_HELPINFO()
#endif //XP_WIN32
END_MESSAGE_MAP()

void CPersonalDictionaryDlg::OnAddWord()
{
    CString NewWord;
    GetDlgItemText(IDC_NEW_WORD, NewWord);

    if (NewWord.IsEmpty())
        return;

    int Index = m_WordList.FindStringExact(-1, NewWord);
    if (Index == LB_ERR)
    {
        // clear current selection
        for (int i = m_WordList.GetCount() - 1; i >= 0; i--)
            m_WordList.SetSel(Index, FALSE);

        Index = m_WordList.AddString(NewWord);
        m_WordList.SetSel(Index, TRUE);
    }
}

void CPersonalDictionaryDlg::OnReplaceWord()
{
    CString NewWord;
    GetDlgItemText(IDC_NEW_WORD, NewWord);

    if (NewWord.IsEmpty())
        return;

    if (m_WordList.GetSelCount() != 1)
        return;

    OnRemoveWord();
    OnAddWord();
}

void CPersonalDictionaryDlg::OnRemoveWord()
{
    CString Word;

    for (int Index = m_WordList.GetCount() - 1; Index >= 0; Index--)
    {
        if (m_WordList.GetSel(Index))
            m_WordList.DeleteString(Index);
    }

    EnableButtons();
}

void CPersonalDictionaryDlg::OnSelChangeWordList()
{
    if (m_WordList.GetSelCount() == 1)
    {
        CString Text;
        m_WordList.GetText(m_WordList.GetCurSel(), Text);
        SetDlgItemText(IDC_NEW_WORD, Text);
    }

    EnableButtons();
}

void CPersonalDictionaryDlg::EnableButtons()
{
    CString NewWord;
    GetDlgItemText(IDC_NEW_WORD, NewWord);

    BOOL    NewWordEmpty = NewWord.IsEmpty();
    int     SelCount = m_WordList.GetSelCount();

    GetDlgItem(IDC_ADD)->EnableWindow(!NewWordEmpty);
    GetDlgItem(IDC_REPLACE)->EnableWindow(!NewWordEmpty && SelCount == 1);
    GetDlgItem(IDC_REMOVE)->EnableWindow(SelCount > 0);
}

void CPersonalDictionaryDlg::OnHelp() 
{
    NetHelp(HELP_EDIT_DICTIONARY);
}

// OnHelpInfo - Invokes Help window when F1 key is pressed
#ifdef XP_WIN32
BOOL CPersonalDictionaryDlg::OnHelpInfo(HELPINFO *)//32bit messagemapping.
{
    OnHelp();
    return TRUE;
}
#endif//XP_WIN32
