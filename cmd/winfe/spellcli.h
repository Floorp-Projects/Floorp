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

#ifndef _SPELLCLI_H
#define _SPELLCLI_H

#include "spellchk.h"			// spell checker library public header file
// the dialog box & string resources are in edtrcdll DLL
#include "edtrcdll\src\resource.h"

/**************************************************************************
 * Public classes and functions.
 **************************************************************************/

// forward declaration of the dialog box class
class CSpellCheckerDlg;
class CPersonalDictionaryDlg;

class CSpellCheckerClient
{
public:
    CSpellCheckerClient(CWnd *pParentWnd); 
	~CSpellCheckerClient();

    // Starts checking the document for spelling errors.
    // Returns: 0 = success, non-zero = failure
    int ProcessDocument();

protected:
    // Functions to be provided by the derived classes
    virtual XP_HUGE_CHAR_PTR GetBuffer() = 0;
    virtual BOOL GetSelection(int32 &SelStart, int32 &SelEnd) = 0;
    virtual char *GetFirstError() = 0;
    virtual char *GetNextError() = 0;
    virtual void ReplaceHilitedText(const char *NewText, int AllInstances) = 0;
    virtual void IgnoreHilitedText(int AllInstances) = 0;
    virtual void RemoveAllErrorHilites() {};

    // Displays the modal personal dictionary dialog box
    void ShowPersonalDictionaryDlg(CWnd *pParent = NULL);

    // Restarts checking the document. Called by the dialog box when the
    // user changes the default language.
    int ReprocessDocument();

protected:
    HINSTANCE               m_hSpellCheckerDll;
    CWnd                    *m_pParentWnd;
	ISpellChecker		    *m_pSpellChecker;
    CPersonalDictionaryDlg  *m_pDictionaryDlg;
    int32                   m_BufferSize;
    int32                   m_SelStart, m_SelEnd;

    static int              m_InstanceCount;

    friend class CSpellCheckerDlg;
    friend class CHtmlSpellChecker;
    friend class CPlainTextSpellChecker;
};

class CHtmlSpellChecker : public CSpellCheckerClient
{
public:
    CHtmlSpellChecker(MWContext *pMWContext, CNetscapeEditView *pHtmlView); 

    // Starts checking the document for spelling errors.
    // Returns: 0 = success, non-zero = failure
    int ProcessDocument() {return CSpellCheckerClient::ProcessDocument(); };

protected:
    // Implementation of base class virtuals.
    XP_HUGE_CHAR_PTR GetBuffer();
    BOOL GetSelection(int32 &SelStart, int32 &SelEnd);
    char *GetFirstError();
    char *GetNextError();
    void ReplaceHilitedText(const char *NewText, int AllInstances);
    void IgnoreHilitedText(int AllInstances);
    void RemoveAllErrorHilites();

private:
    MWContext           *m_pMWContext;
    CNetscapeEditView   *m_pView;
};

class CPlainTextSpellChecker : public CSpellCheckerClient
{
public:
    CPlainTextSpellChecker(CEdit *pTextView); 

    // Starts checking the document for spelling errors.
    // Returns: 0 = success, non-zero = failure
    int ProcessDocument() {return CSpellCheckerClient::ProcessDocument(); };

protected:
    // Implentation of base class virtuals.
    XP_HUGE_CHAR_PTR GetBuffer();
    BOOL GetSelection(int32 &SelStart, int32 &SelEnd);
    char *GetFirstError();
    char *GetNextError();
    void ReplaceHilitedText(const char *NewText, int AllInstances);
    void IgnoreHilitedText(int AllInstances);

private:
	CEdit   *m_pTextView;
};


/**************************************************************************
 * Dialog box implementation classes
 **************************************************************************/

/////////////////////////////////////////////////////////////////////////////
// CSpellCheckerDlg dialog

class CSpellCheckerDlg : public CDialog
{
// Construction
public:
	CSpellCheckerDlg(ISpellChecker *pSpellChecker, CSpellCheckerClient *pClient, CWnd* pParent = NULL);

// Dialog Data
	//{{AFX_DATA(CSpellDlg)
	enum { IDD = IDD_SPELL_CHECKER };
	CListBox	m_SuggestionsListBox;
	//}}AFX_DATA

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSpellCheckerDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSpellCheckerDlg)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	virtual BOOL OnInitDialog();
	virtual void OnCancel();
	afx_msg void OnChange();
	afx_msg void OnIgnore();
	afx_msg void OnChangeNewWord();
	afx_msg void OnSetFocusNewWord();
	afx_msg void OnSetFocusSuggestions();
	afx_msg void OnSelChangeSuggestions();
	afx_msg void OnIgnoreAll();
	afx_msg void OnChangeAll();
	afx_msg void OnAdd();
	afx_msg void OnEditDictionary();
	afx_msg void OnSelendokLanguage();
	afx_msg void OnHelp();
	afx_msg void OnCheckWord();
	//}}AFX_MSG
#ifdef XP_WIN32
    afx_msg BOOL OnHelpInfo(HELPINFO *);
#endif
	void CommonOnChange(BOOL ChangeAll);

    afx_msg LRESULT OnSetInitialFocus(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()

private:
	ISpellChecker	*m_pSpellChecker;
	CSpellCheckerClient	*m_pClient;
    CString         m_MisspelledWord;

	// Resource switcher object: It switches the default resource handle to the 
	// editorXX.dll, cause that's where the dialog box resource is. 
    CEditorResourceSwitcher m_DlgResource;

    // strings loaded from the resource DLL
    CString         m_strChange, m_strChangeAll, m_strDelete,
                    m_strDeleteAll, m_strNoSuggestions,
                    m_strStop, m_strDone, m_strCorrectSpelling;

    // The new word, either entered in the edit field or selected in 
    // the Suggestions listbox.
    CString         m_NewWord;
    int             m_NumSuggestions;

	void SetAlternatives();
    void GetFirstError();
    void GetNextError();
    void ProcessError(char *pMisspelledWord);
    void InitLanguageList();
    void ChangeState();
    void UpdateChangeButton();

#ifdef XP_WIN16
    // Win16 MFC 1.52 version of GetDlgItemText does not take a CString
    void GetDlgItemText( int nID, CString& rString ) const
            { GetDlgItem(nID)->GetWindowText(rString); }
#endif  
};

/////////////////////////////////////////////////////////////////////////////
// CPersonalDictionaryDlg dialog

class CPersonalDictionaryDlg : public CDialog
{
// Construction
public:
	CPersonalDictionaryDlg(ISpellChecker *pSpellChecker, CWnd* pParent=NULL);

// Dialog Data
	//{{AFX_DATA(CPersonalDictionaryDlg)
	enum { IDD = IDD_CUSTOM_DICTIONARY };
	CListBox	m_WordList;
	//}}AFX_DATA

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSpellCheckerDlg)
	virtual BOOL OnInitDialog();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL
    virtual void OnOK();

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSpellCheckerDlg)
	//}}AFX_MSG

    afx_msg void OnAddWord();
    afx_msg void OnReplaceWord();
    afx_msg void OnRemoveWord();
    afx_msg void OnSelChangeWordList();
    afx_msg void EnableButtons();
	afx_msg void OnHelp();
#ifdef XP_WIN32
    afx_msg BOOL OnHelpInfo(HELPINFO *);
#endif

	DECLARE_MESSAGE_MAP()

private:
	ISpellChecker	*m_pSpellChecker;

	// Resource switcher object: It switches the default resource handle to the 
	// editorXX.dll, cause that's where the dialog box resource is. 
    CEditorResourceSwitcher m_DlgResource;

#ifdef XP_WIN16
    // Win16 MFC 1.52 version of GetDlgItemText does not take a CString
    void GetDlgItemText( int nID, CString& rString ) const
            { GetDlgItem(nID)->GetWindowText(rString); }
#endif  

};

#endif
