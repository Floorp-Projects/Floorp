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

/* imgcnvdl.cpp*/
/* new dialog box class derived from Troy Chevalier's CDialog class*/

// CImageConversionDialog.cpp : implementation file
//

#include "stdafx.h"
#include "edtrcdll.h"//to get g_instance
#include "platform.h"
#include "edtdlgs.h"
#include "imgcnvdl.h"
#include <assert.h>
#include "windowsx.h"
#include "xp_help.h"

// This function is used to enumerate the SaveAs dialog's children
// (in order to update the file name field when the type changes).
static BOOL CALLBACK updateFileNameField( HWND hwnd, LPARAM lParam ) {
    CImageConversionDialog *pdlg = (CImageConversionDialog*)lParam;
    // Test direct children, only.
    if ( GetParent( hwnd ) == GetParent(pdlg->m_hwndDlg) ) {
        // Get control's class name.
        CString temp;
        LPTSTR pClassName = temp.BufferSetLength( 16 );
        GetClassName( hwnd, pClassName, 16 );
        // Check if this is the edit field.
        if ( temp == "Edit" ) {
            // Get the text.
            char text[ MAX_PATH ];
            GetWindowText( hwnd, text, sizeof text );
            CString filename = text;
            int i = filename.ReverseFind( '.' );
            if ( i != -1 ) {
                // Get extension (past last '.').
                temp = (LPCTSTR)filename + i + 1;
                // Compare to what we provided.
                if ( pdlg->m_outfilevalue == temp ) {
                    // User didn't change it, remember extension we are providing this time.
                    pdlg->m_outfilevalue = pdlg->m_Doptionarray[pdlg->m_Doutputimagetype].m_pfileextention;
                    // Attach new extension to base part of name.
                    filename.SetAt( i+1, 0 ); // Truncate old extension.
                    temp = (LPCTSTR)filename;
                    temp += pdlg->m_outfilevalue;
                    // Put into edit field.
                    SetWindowText( hwnd, (LPCTSTR)temp );
                } else {
                    // User typed their own thing, leave it alone.
                }
            } else {
                // No extension, so we won't tack one on.
            }
            // Quit after we've seen the edit field.
            return FALSE;
        }
    }
    // Go on to next child.
    return TRUE;
}

// SaveAs dialog hook procedure to handle certain interactions.
static UINT APIENTRY
HookProc( HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    UINT result = 0;
    OFNOTIFY *pNote;
    OPENFILENAME *pofn;
    CImageConversionDialog *pdlg;
    int index;
    switch ( msg ) {
        case WM_NOTIFY:
            pNote = (OFNOTIFY*)lParam;
            pofn  = pNote->lpOFN;
            pdlg  = (CImageConversionDialog*)pofn->lCustData;

            switch ( pNote->hdr.code ) {
                case CDN_INITDONE:
                    // Stash dialog handle.
                    pdlg->m_hwndDlg = hwndDlg;
                    break;

                case CDN_TYPECHANGE:
                    // Set new index (note that nFilterIndex is 1-based!).
                    index = pdlg->m_Doutputimagetype = pofn->nFilterIndex - 1;

                    assert( index >= 0 && index < (int)pdlg->m_Doptionarraycount );

                    // Update extension in file name field (if not modified by user).
                    // Note that hwndDlg is actually a child of the *real* dialog
                    // (don't ask me why; it's a Windoze thing, I guess).
                    EnumChildWindows( GetParent(hwndDlg), (WNDENUMPROC)updateFileNameField, (LPARAM)pdlg );
                    break;

                case CDN_HELP:
                    // Display our help.
                    if (pdlg->m_wfeiface)
                    {
                        pdlg->m_wfeiface->WinHelp(HELP_IMAGE_CONVERSION);
                        result = TRUE;
                    }
                    break;

                case CDN_FILEOK:
                    // Store resulting file name in m_Doutfilename1.
                    pdlg->m_Doutfilename1 = pofn->lpstrFile;
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    };
    return result;
}

int LOADDS
CImageConversionDialog::DoModal()
{
    // Calculate length of lpstrFilter.
    unsigned int i, len = 0;
    CString builtin, plugin;
    builtin.LoadString( g_instance, IDS_BUILT_IN );
    plugin.LoadString( g_instance, IDS_PLUGIN );
    for ( i=0;i<m_Doptionarraycount;i++)
    {
        // First string is encoder name...
        len += strlen( m_Doptionarray[i].m_pencodername );
        // ...followed by " (builtin)" or " (plugin)"...
        if (m_Doptionarray[i].m_builtin)
            len += builtin.GetLength() + 1;
        else
            len += plugin.GetLength() + 1;
        // ...followed by " (*.ext)" plus null.
        len += strlen( m_Doptionarray[i].m_pfileextention );
        len += strlen( " (*.)" ) + 1;

        // Second string is *. followed by file extension plus null.
        len += strlen( "*." ) + strlen( m_Doptionarray[i].m_pfileextention ) + 1;
    }
    // Allocate and fill lpstrFilter.
    LPTSTR lpstrFilter = new char[ len + 1 ];
    LPTSTR p = lpstrFilter;
    for ( i=0;i<m_Doptionarraycount;i++ )
    {
        // First string.
        strcpy( p, m_Doptionarray[i].m_pencodername );
        p += strlen( m_Doptionarray[i].m_pencodername );
        if (m_Doptionarray[i].m_builtin)
        {
            strcpy( p, builtin );
            p += builtin.GetLength();
        }
        else 
        {
            strcpy( p, plugin );
            p += plugin.GetLength();
        }
        strcpy( p, " (*." );
        p += strlen( " (*." );
        strcpy( p, m_Doptionarray[i].m_pfileextention );
        p += strlen( m_Doptionarray[i].m_pfileextention );
        strcpy( p, ")" );
        p += strlen( ")" ) + 1;

        // Second string.
        strcpy( p, "*." );
        p += strlen( "*." );
        strcpy( p, m_Doptionarray[i].m_pfileextention );
        p += strlen( m_Doptionarray[i].m_pfileextention ) + 1;
    }

    // Add consecutive terminating null at end.
    *p = 0;

    // Set up default file name and dir name.
    CString filename;
    CString dirname = filename = m_Doutfilename1;
    i = dirname.ReverseFind( '\\' );

    // Truncate directory at last \.
    dirname.SetAt( max( i, 0 ), 0 );

    // Get file name part.
    filename = (LPCTSTR)filename + ( i >= 0 ? i + 1 : 0 );

    // Remember the extension we will provide.
    m_outfilevalue = m_Doptionarray[m_Doutputimagetype].m_pfileextention;

    // Add default extension to file name contents.
    filename += ".";
    filename += m_outfilevalue;

    CString temp = filename;

    // Pad filename string to sufficient length.
    strcpy( filename.BufferSetLength( MAX_PATH ), temp );

    // SaveAs dialog flags.
    DWORD flags = OFN_HIDEREADONLY | 
                  OFN_NOREADONLYRETURN | 
                  OFN_PATHMUSTEXIST |
                  OFN_OVERWRITEPROMPT |
                  OFN_EXPLORER |
                  OFN_ENABLEHOOK;

    // Create standard "save as" dialog.
    OPENFILENAME ofn = { sizeof ofn,           // lStructSize; 
                         m_parent,             // hwndOwner; 
                         NULL,                 // hInstance; 
                         lpstrFilter,          // lpstrFilter; 
                         NULL,                 // lpstrCustomFilter; 
                         NULL,                 // nMaxCustFilter; 
                         m_Doutputimagetype+1, // nFilterIndex; 
                         (LPTSTR)(LPCTSTR)filename, // lpstrFile; 
                         MAX_PATH,             // nMaxFile; 
                         NULL,                 // lpstrFileTitle; 
                         NULL,                 // nMaxFileTitle; 
                         dirname,              // lpstrInitialDir; 
                         NULL,                 // lpstrTitle; 
                         flags,                // Flags; 
                         0,                    // nFileOffset; 
                         0,                    // nFileExtension; 
                         m_outfilevalue,       // lpstrDefExt; 
                         (long)(void*)this,    // lCustData; 
                         HookProc,             // lpfnHook; 
                         NULL };               // lpTemplateName;

    int result = GetSaveFileName( &ofn );

    delete lpstrFilter;

    return result;
}

/////////////////////////////////////////////////////////////////////////////
// CImageConversionDialog dialog


CImageConversionDialog::CImageConversionDialog(HWND pParent /*=NULL*/)
:m_parent(pParent)
{
	m_Doutputimagetype=0;
    m_Doptionarraycount=0;
    m_wfeiface=NULL;
}


void LOADDS 
CImageConversionDialog::setOutputFileName1(const char *p_string)
{
    m_Doutfilename1=p_string;
}


//////////////////////////////////////////////////////
//JPEG OPTIONS DIALOG
//////////////////////////////////////////////////////


#define J_LOWQUALITY 25
#define J_MEDQUALITY 75
#define J_HIGHQUALITY 100

int LOADDS 
CJpegOptionsDialog::DoModal()
{
    return CDialog::DoModal(m_parent);
}

/////////////////////////////////////////////////////////////////////////////
// CJpegOptionsDialog dialog


CJpegOptionsDialog::CJpegOptionsDialog(HWND pParent /*=NULL*/)
:CDialog(g_instance,IDD_FEJPEGOPTIONS),m_parent(pParent)
{
	m_Doutputquality=0;
}


BOOL
CJpegOptionsDialog::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
    return CDialog::OnCommand(id, hwndCtl,notifyCode);
}

BOOL
CJpegOptionsDialog::DoTransfer(BOOL bSaveAndValidate)
{
    RadioButtonTransfer(IDC_QUALITY3,m_outputquality,bSaveAndValidate);
    return TRUE;//no validation
}



/////////////////////////////////////////////////////////////////////////////
// CJpegOptionsDialog message handlers

BOOL CJpegOptionsDialog::InitDialog() 
{
	CDialog::InitDialog();
	
	// TODO: Add extra initialization here
    m_outputquality=0;
    if (m_Doutputquality<=J_LOWQUALITY)//33
        m_outputquality=2;
    else if (m_Doutputquality<=J_MEDQUALITY)//66
        m_outputquality=1;
    else 
        m_outputquality=0;//100
    DoTransfer(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CJpegOptionsDialog::OnOK() 
{
	// TODO: Add extra validation here
	DoTransfer(TRUE);
    if (2==m_outputquality)
        m_Doutputquality=J_LOWQUALITY;
    if (1==m_outputquality)
        m_Doutputquality=J_MEDQUALITY;
    if (0==m_outputquality)
        m_Doutputquality=J_HIGHQUALITY;
    EndDialog(m_hwndDlg,IDOK);
}

void CJpegOptionsDialog::OnCancel() 
{
	// TODO: Add extra cleanup here
	
    EndDialog(m_hwndDlg,IDCANCEL);
}
 
