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

// STyles.cpp : implementation file
//
#include "stdafx.h"
#include "styles.h"
#include "nethelp.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

//
// Draw a framed rectangle of the current color
//
void WFE_DrawSwatch(CWnd * parent, UINT ID, COLORREF color)
{
    CWnd * widget = parent->GetDlgItem(ID);
    CDC * pDC = widget->GetDC();
    CRect rect;

    // find out how much area we can draw into
    widget->GetClientRect(&rect);

    // color for the inside
    CBrush brush(color);
    CBrush * oldBrush = (CBrush *) pDC->SelectObject(&brush);

    pDC->LPtoDP(&rect);

    // flush any drawing
    widget->Invalidate();
    widget->UpdateWindow();

    // draw the frame
    pDC->Rectangle(rect);

    // select the old brush
    pDC->SelectObject(oldBrush);

    // set the background color
    pDC->SetBkColor(color);

    // give the CDC back to the system
    widget->ReleaseDC(pDC);

}

// CLM: Added params to pass in Caption ID
CNetscapePropertyPage::CNetscapePropertyPage(UINT nID, UINT nIDCaption, UINT nIDFocus)
    : CPropertyPage(nID, nIDCaption),
      m_nIDFocus(nIDFocus)
{
}

// Return with this at end of OnSetActive()
//  to set focus to a specific control
// Either pass an ID in call, or set it in constructor
BOOL CNetscapePropertyPage::SetInitialFocus( UINT nID )
{
    if ( nID || m_nIDFocus ){
        CWnd * pWnd = GetDlgItem(nID ? nID : m_nIDFocus);
        if( pWnd ){
            pWnd->SetFocus();
            return FALSE;
        }
    }
    return TRUE;
}

// Use instead of MFC's CancelToClose, which doesn't work as advertised (wrong only in Win16?)
void CNetscapePropertyPage::OkToClose()
{
    CWnd *pApply = GetParent()->GetDlgItem(ID_APPLY_NOW);

    // Do nothing if we don't have an Apply button    
    if( pApply && pApply->IsWindowVisible() ){
        // We always do this after using the Apply button
        SetModified(FALSE);

        // Get the Property sheet parent of the property page
        CWnd *pWnd = GetParent()->GetDlgItem(IDOK);

        if( pWnd ){
            // Change "OK" button text to "Close"
            pWnd->SetWindowText(szLoadString(IDS_CLOSE_BUTTON));

            // Move focus from the Apply button to the Close button
            if( GetFocus() == pApply ){
                pWnd->SetFocus();
            }
        }

        // Disable the Cancel button
        pWnd = GetParent()->GetDlgItem(IDCANCEL);
        if( pWnd ){
            pWnd->EnableWindow(FALSE);
        }
    }
}

void CNetscapePropertyPage::OnHelp() 
{
	NetHelp("PREFERENCES_GENERAL_APPEARANCE");
}
// the ID_HELP message actually goes to our parent CNetscapePropertySheet
//   which passes it along to us, can't use message map
BEGIN_MESSAGE_MAP(CNetscapePropertyPage, CPropertyPage)
END_MESSAGE_MAP()

// Called by the security library to indicate whether the user is or is
// not using a password
//
// XXX - jsw - remove me
void FE_SetPasswordEnabled(MWContext *context, PRBool usePW)
{
}
