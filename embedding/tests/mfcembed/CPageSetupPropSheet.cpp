/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rod Spears <rods@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// CPageSetupPropSheet.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "CPageSetupPropSheet.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPageSetupPropSheet

IMPLEMENT_DYNAMIC(CPageSetupPropSheet, CPropertySheet)

CPageSetupPropSheet::CPageSetupPropSheet(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
	  AddControlPages();
}

CPageSetupPropSheet::CPageSetupPropSheet(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
	  AddControlPages();
}

CPageSetupPropSheet::~CPageSetupPropSheet()
{
}


BEGIN_MESSAGE_MAP(CPageSetupPropSheet, CPropertySheet)
	//{{AFX_MSG_MAP(CPageSetupPropSheet)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPageSetupPropSheet message handlers

/////////////////////////////////////////////////////////////////////////////
void CPageSetupPropSheet::AddControlPages()
{
	  m_psh.dwFlags &= ~PSH_HASHELP;  // Lose the Help button
	  AddPage(&m_FormatOptionTab);
	  AddPage(&m_MarginHeaderFooterTab);
}

/////////////////////////////////////////////////////////////////////////////
static float GetFloatFromStr(const TCHAR * aStr, float aMaxVal = 1.0)
{
    float val;
    _stscanf(aStr, _T("%f"), &val);
    if (val <= aMaxVal) 
    {
        return val;
    } 
    else 
    {
        return 0.5;
    }
}

/////////////////////////////////////////////////////////////////////////////
static PRUnichar* GetUnicodeFromCString(const CString& aStr)
{
#ifdef _UNICODE
    nsString str(aStr);
#else
    nsString str; str.AssignWithConversion(aStr);
#endif
    return ToNewUnicode(str);
}

/////////////////////////////////////////////////////////////////////////////
void CPageSetupPropSheet::SetPrintSettingsValues(nsIPrintSettings* aPrintSettings) 
{ 
  m_FormatOptionTab.m_PrintSettings = aPrintSettings;

  if (aPrintSettings != NULL) 
  {
    double top, left, right, bottom;
    aPrintSettings->GetMarginTop(&top);
    aPrintSettings->GetMarginLeft(&left);
    aPrintSettings->GetMarginRight(&right);
    aPrintSettings->GetMarginBottom(&bottom);

    char buf[16];
    sprintf(buf, "%5.2f", top);
    m_MarginHeaderFooterTab.m_TopMarginText = buf;
    sprintf(buf, "%5.2f", left);
    m_MarginHeaderFooterTab.m_LeftMarginText = buf;
    sprintf(buf, "%5.2f", right);
    m_MarginHeaderFooterTab.m_RightMarginText = buf;
    sprintf(buf, "%5.2f", bottom);
    m_MarginHeaderFooterTab.m_BottomMarginText = buf;

    PRInt32 orientation;
    aPrintSettings->GetOrientation(&orientation);
    m_FormatOptionTab.m_IsLandScape = orientation != nsIPrintSettings::kPortraitOrientation;

    double scaling;
    aPrintSettings->GetScaling(&scaling);
	  m_FormatOptionTab.m_Scaling = int(scaling * 100.0);

    PRBool boolVal;
    aPrintSettings->GetPrintBGColors(&boolVal);
    m_FormatOptionTab.m_BGColors = boolVal == PR_TRUE;
    aPrintSettings->GetPrintBGImages(&boolVal);
    m_FormatOptionTab.m_BGImages = boolVal == PR_TRUE;

    PRUnichar* uStr;
    aPrintSettings->GetHeaderStrLeft(&uStr);
		m_MarginHeaderFooterTab.m_HeaderLeftText = NS_LossyConvertUCS2toASCII(uStr).get();
    if (uStr != nsnull) nsMemory::Free(uStr);

    aPrintSettings->GetHeaderStrCenter(&uStr);
		m_MarginHeaderFooterTab.m_HeaderCenterText = NS_LossyConvertUCS2toASCII(uStr).get();
    if (uStr != nsnull) nsMemory::Free(uStr);

    aPrintSettings->GetHeaderStrRight(&uStr);
		m_MarginHeaderFooterTab.m_HeaderRightText = NS_LossyConvertUCS2toASCII(uStr).get();
    if (uStr != nsnull) nsMemory::Free(uStr);

    aPrintSettings->GetFooterStrLeft(&uStr);
		m_MarginHeaderFooterTab.m_FooterLeftText = NS_LossyConvertUCS2toASCII(uStr).get();
    if (uStr != nsnull) nsMemory::Free(uStr);

    aPrintSettings->GetFooterStrCenter(&uStr);
		m_MarginHeaderFooterTab.m_FooterCenterText = NS_LossyConvertUCS2toASCII(uStr).get();
    if (uStr != nsnull) nsMemory::Free(uStr);

    aPrintSettings->GetFooterStrRight(&uStr);
		m_MarginHeaderFooterTab.m_FooterRightText = NS_LossyConvertUCS2toASCII(uStr).get();
    if (uStr != nsnull) nsMemory::Free(uStr);
  }
}

/////////////////////////////////////////////////////////////////////////////
void CPageSetupPropSheet::GetPrintSettingsValues(nsIPrintSettings* aPrintSettings) 
{ 

    if (!aPrintSettings) return;

    aPrintSettings->SetScaling(double(m_FormatOptionTab.m_Scaling) / 100.0);
    aPrintSettings->SetPrintBGColors(m_FormatOptionTab.m_BGColors);
    aPrintSettings->SetPrintBGImages(m_FormatOptionTab.m_BGImages);

    PRInt32 orientation  = m_FormatOptionTab.m_IsLandScape?
                           nsIPrintSettings::kLandscapeOrientation:nsIPrintSettings::kPortraitOrientation;
    aPrintSettings->SetOrientation(orientation);

    short  type;
    double width;
    double height;
    m_FormatOptionTab.GetPaperSizeInfo(type, width, height);
    aPrintSettings->SetPaperSizeType(nsIPrintSettings::kPaperSizeDefined);
    aPrintSettings->SetPaperSizeUnit(type);
    aPrintSettings->SetPaperWidth(width);
    aPrintSettings->SetPaperHeight(height);

    aPrintSettings->SetMarginTop(GetFloatFromStr(m_MarginHeaderFooterTab.m_TopMarginText));
    aPrintSettings->SetMarginLeft(GetFloatFromStr(m_MarginHeaderFooterTab.m_LeftMarginText));
    aPrintSettings->SetMarginRight(GetFloatFromStr(m_MarginHeaderFooterTab.m_RightMarginText));
    aPrintSettings->SetMarginBottom(GetFloatFromStr(m_MarginHeaderFooterTab.m_BottomMarginText));

    PRUnichar* uStr;
    uStr = GetUnicodeFromCString(m_MarginHeaderFooterTab.m_HeaderLeftText);
    aPrintSettings->SetHeaderStrLeft(uStr);
    if (uStr != nsnull) nsMemory::Free(uStr);

    uStr = GetUnicodeFromCString(m_MarginHeaderFooterTab.m_HeaderCenterText);
    aPrintSettings->SetHeaderStrCenter(uStr);
    if (uStr != nsnull) nsMemory::Free(uStr);

    uStr = GetUnicodeFromCString(m_MarginHeaderFooterTab.m_HeaderRightText);
    aPrintSettings->SetHeaderStrRight(uStr);
    if (uStr != nsnull) nsMemory::Free(uStr);

    uStr = GetUnicodeFromCString(m_MarginHeaderFooterTab.m_FooterLeftText);
    aPrintSettings->SetFooterStrLeft(uStr);
    if (uStr != nsnull) nsMemory::Free(uStr);

    uStr = GetUnicodeFromCString(m_MarginHeaderFooterTab.m_FooterCenterText);
    aPrintSettings->SetFooterStrCenter(uStr);
    if (uStr != nsnull) nsMemory::Free(uStr);

    uStr = GetUnicodeFromCString(m_MarginHeaderFooterTab.m_FooterRightText);
    aPrintSettings->SetFooterStrRight(uStr);
    if (uStr != nsnull) nsMemory::Free(uStr);
}

