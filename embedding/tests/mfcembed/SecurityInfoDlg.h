/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Chak Nanga <chak@netscape.com> 
 */

#ifndef _SECURITY_INFO_DLG_H_
#define _SECURITY_INFO_DLG_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// SecurityInfoDlg dialog

class SecurityInfoDlg : public CDialog
{
// Construction
public:
    SecurityInfoDlg(nsISSLStatus* sslStatus, CWnd* pParent = NULL);   // standard constructor
    nsCOMPtr<nsISSLStatus> m_ISSLStatus;

    enum { IDD = IDD_SECURITY_INFO_DIALOG };
    CStatic	m_Encryption;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:
    afx_msg void OnViewCert();
    virtual void OnOK();
    virtual BOOL OnInitDialog();

    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// ViewCertDlg dialog

class ViewCertDlg : public CDialog
{
public:
    ViewCertDlg(nsISSLStatus* sslStatus, CWnd* pParent = NULL);   // standard constructor
    nsCOMPtr<nsISSLStatus> m_ISSLStatus;

    enum { IDD = IDD_VIEWCERT_DIALOG };
    CStatic	m_CertSerial;
    CStatic	m_CertIssued;
    CStatic	m_CertExpires;
    CStatic	m_CertOwner;
    CStatic	m_CertIssuer;
    CStatic	m_CertSha1Fingerprint;
    CStatic	m_CertMd5Fingerprint;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:
	virtual BOOL OnInitDialog();

    DECLARE_MESSAGE_MAP()
};

#endif // _SECURITY_INFO_DLG_H_
