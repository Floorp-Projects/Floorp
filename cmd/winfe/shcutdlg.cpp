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

// Shortcut dialog implementation.   
// 06/22/95 (jre)

#include "stdafx.h"

#include "shcut.h"
#include "shcutdlg.h"
#ifdef XP_WIN32
#include <regstr.h>
#include <shlobj.h>
#endif

BEGIN_MESSAGE_MAP(CShortcutDialog, CDialog)
END_MESSAGE_MAP()

// Internet shortcut class.  If a title is specified and it has no invalid
// long filename characters, create "Shortcut to xxxx", where xxxx is the
// specified title.  

CShortcutDialog::CShortcutDialog ( 
	CWnd * pParentWnd, char * pszTitle, 	// shortcut name
	char * pszURL ) :  						// url to create shortcut to
		CDialog ( IDD_INTERNETSHORTCUT, pParentWnd )
{

	// invalid long filename characters
	static char * invalidChars = ",\\/:*?<>|\"~";

	if ( pszURL )
	{
		unsigned pos;
		char szNewURL[ _MAX_PATH ];

		// scan the title string to look for invalid long filename characters
		for ( pos = 0; pos < strlen ( invalidChars ); pos++ )
			if ( strchr ( pszTitle, invalidChars[ pos ] ) )
			{

                int i, j;
                for ( i = j = 0; i < CASTINT(strlen ( pszTitle )); i++ )
                    if ( !strchr ( invalidChars, pszTitle[ i ] ) )
                        szNewURL[ j++ ] = pszTitle[ i ];
                szNewURL[ j ] = '\0';
				break;
			}

		// if no invalid long filename characters where found, use the
		// title that was specified

		if ( pos == strlen ( invalidChars ) )
			strcpy ( szNewURL, pszTitle );

		// create and move the shortcut name and url into the data transfer
		// block for the MFC dialog
		VERIFY(m_ShortcutName.LoadString(IDS_SHORTCUT_TO));
		m_ShortcutName += ' ';
		m_ShortcutName += szNewURL;
		m_URL = CString(pszURL);
	}
	else
		m_URL = _T("");

	m_Description = _T("");
}


// returns the shortcut name after the user has potentially edited it

const char * CShortcutDialog::GetDescription ( ) 
{ 
    const char * ptr = (const char*)m_Description;
	return ptr;
}


// returns the URL 

const char * CShortcutDialog::GetURL ( ) 
{
	const char * ptr = (const char *)m_URL;
    return ptr;
}

// transfers data from the dialog into our local class

void CShortcutDialog::DoDataExchange ( CDataExchange * pDX )
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX,  IDC_INTERNETSHORTCUTLINK, m_URL);
	DDX_Text(pDX,  IDC_INTERNETSHORTCUTDESCRIPTION, m_ShortcutName);
}


// initializes the dialog to contain the data which is initially specified
// in our dialog class

BOOL CShortcutDialog::OnInitDialog ()
{
	CDialog::OnInitDialog();
	CEdit * pShortcut = (CEdit *) GetDlgItem(IDC_INTERNETSHORTCUTLINK); 
	if(pShortcut) 
		pShortcut->SetWindowText((const char *)m_URL);
	CEdit * pDescription = (CEdit *)GetDlgItem(IDC_INTERNETSHORTCUTDESCRIPTION);
	if ( pDescription )
	{
		// sets shortcut name as the initial field with focus
		pDescription->SetWindowText((const char *)m_ShortcutName);
		pDescription->SetFocus();
		pDescription->SetSel(0, -1);
		return (0);
	}

	return(1);
	
}

// when user selects the ok button, transfer the data from the controls into
// our local class and create a valid shortcut long filename.

void CShortcutDialog::OnOK ( )
{

#if defined(XP_WIN32)

    char szFilename[_MAX_PATH];
    DWORD   cbData;
    HKEY    hKey;

	UpdateData ( );		// transfer data from fields to class members

	// if user has chosen the "place on desktop" flag, create a .URL file 
	// name on the desktop. Note that we should use the registry to find
	// the path
    if (RegOpenKeyEx(HKEY_CURRENT_USER, REGSTR_PATH_SPECIAL_FOLDERS, NULL, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
		return;  // not much we can do in this case

	cbData = sizeof(szFilename);
    RegQueryValueEx(hKey, "Desktop", NULL, NULL, (LPBYTE)szFilename, &cbData);
	RegCloseKey(hKey);

	// Make sure the path ends with a '\'
	CString	strPath = szFilename;

	if (strPath.GetLength() == 0)
		return;  // nothing we can do with this path

	if (strPath[strPath.GetLength() - 1] != '\\')
		strPath += '\\';

	// Add the shortcut title and append the .URL extension
	strPath += m_ShortcutName + ".URL";

	// assign the new description to the internal data member
	m_Description = strPath;

#endif /* XP_WIN32 */

	CDialog::OnOK ( );
}
