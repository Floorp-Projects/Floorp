/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Rod Spears <rods@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

// CPageSetupPropSheet.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "CPageSetupPropSheet.h"
#include "nsMemory.h"

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
    nsEmbedString str(aStr);
#else
    nsEmbedString str;
    NS_CStringToUTF16(nsEmbedCString(aStr), NS_CSTRING_ENCODING_ASCII, str);
#endif
    return NS_StringCloneData(str);
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
		m_MarginHeaderFooterTab.m_HeaderLeftText = uStr;
    if (uStr != nsnull) nsMemory::Free(uStr);

    aPrintSettings->GetHeaderStrCenter(&uStr);
		m_MarginHeaderFooterTab.m_HeaderCenterText = uStr;
    if (uStr != nsnull) nsMemory::Free(uStr);

    aPrintSettings->GetHeaderStrRight(&uStr);
		m_MarginHeaderFooterTab.m_HeaderRightText = uStr;
    if (uStr != nsnull) nsMemory::Free(uStr);

    aPrintSettings->GetFooterStrLeft(&uStr);
		m_MarginHeaderFooterTab.m_FooterLeftText = uStr;
    if (uStr != nsnull) nsMemory::Free(uStr);

    aPrintSettings->GetFooterStrCenter(&uStr);
		m_MarginHeaderFooterTab.m_FooterCenterText = uStr;
    if (uStr != nsnull) nsMemory::Free(uStr);

    aPrintSettings->GetFooterStrRight(&uStr);
		m_MarginHeaderFooterTab.m_FooterRightText = uStr;
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

