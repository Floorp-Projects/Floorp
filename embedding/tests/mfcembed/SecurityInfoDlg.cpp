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

#include "stdafx.h"
#include "SecurityInfoDlg.h"
#include "nsIX509Cert.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// SecurityInfoDlg dialog

SecurityInfoDlg::SecurityInfoDlg(nsISSLStatus* sslStatus, CWnd* pParent /*=NULL*/)
    : CDialog(SecurityInfoDlg::IDD, pParent)
{
    m_ISSLStatus = sslStatus;
}


void SecurityInfoDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_ENCRYPTION, m_Encryption);
}

BEGIN_MESSAGE_MAP(SecurityInfoDlg, CDialog)
    ON_BN_CLICKED(IDC_VIEW_CERT, OnViewCert)
END_MESSAGE_MAP()

BOOL SecurityInfoDlg::OnInitDialog() 
{
    CDialog::OnInitDialog();

    nsXPIDLCString encryptionAlgorithm;
    m_ISSLStatus->GetCipherName(getter_Copies(encryptionAlgorithm));
   
    PRUint32 encryptionStrength = 0;
    m_ISSLStatus->GetSecretKeyLength(&encryptionStrength);

    int iResID = IDS_ENCRYPTION_NONE;
    if (encryptionStrength >= 90)
        iResID = IDS_ENCRYPTION_HIGH_GRADE;
    else if (encryptionStrength > 0)
        iResID = IDS_ENCRYPTION_LOW_GRADE;

    CString strEncryption;
    if(iResID == IDS_ENCRYPTION_NONE)
        strEncryption.LoadString(iResID);
    else
        strEncryption.FormatMessage(iResID, encryptionAlgorithm.get(),
                            encryptionStrength);

    m_Encryption.SetWindowText(strEncryption);

    return TRUE;  // return TRUE unless you set the focus to a control
}

void SecurityInfoDlg::OnOK() 
{
    CDialog::OnOK();
}

void SecurityInfoDlg::OnViewCert() 
{
    OnOK();

    ViewCertDlg dlg(m_ISSLStatus, this);
    dlg.DoModal();
}

/////////////////////////////////////////////////////////////////////////////
// ViewCertDlg dialog

ViewCertDlg::ViewCertDlg(nsISSLStatus* sslStatus, CWnd* pParent /*=NULL*/)
    : CDialog(ViewCertDlg::IDD, pParent)
{
    m_ISSLStatus = sslStatus;
}


void ViewCertDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_CERT_SERIAL, m_CertSerial);
    DDX_Control(pDX, IDC_CERT_ISSUE_DATE, m_CertIssued);
    DDX_Control(pDX, IDC_CERT_EXPIRY_DATE, m_CertExpires);
    DDX_Control(pDX, IDC_CERT_OWNER, m_CertOwner);
    DDX_Control(pDX, IDC_CERT_ISSUER, m_CertIssuer);
    DDX_Control(pDX, IDC_CERT_SHA1_FINGERPRINT, m_CertSha1Fingerprint);
    DDX_Control(pDX, IDC_CERT_MD5_FINGERPRINT, m_CertMd5Fingerprint);
}

BEGIN_MESSAGE_MAP(ViewCertDlg, CDialog)
END_MESSAGE_MAP()

BOOL ViewCertDlg::OnInitDialog() 
{
    CDialog::OnInitDialog();

    nsCOMPtr<nsIX509Cert> serverCert;
    nsresult rv = m_ISSLStatus->GetServerCert(getter_AddRefs(serverCert));
    if (NS_FAILED(rv)) return TRUE;

    USES_CONVERSION;

    // The cert issuer name and the subject names returned will be
    // in the "Distinguised Name(DN)" format specified in RFC 1779
    // 
    // The following string Replaces attempt to convert the
    // DN into a user friendly name for our diplay purposes
    //
    // This is a quick and dirty version of the ldap_dn2ufn() function
    // in the Netscape's LDAP SDK

    nsXPIDLString issuerName;
    serverCert->GetIssuerName(getter_Copies(issuerName));
    CString strIssuer = W2T(issuerName.get());
    strIssuer.Replace("OU=", "");
    strIssuer.Replace("O=", "\n");
    strIssuer.Replace("L=", "\n");
    strIssuer.Replace("E=", "");
    strIssuer.Replace("CN=", "");
    strIssuer.Replace(", ", "");
    strIssuer.Replace("ST=", ", ");
    strIssuer.Replace("C=", ", ");
    m_CertIssuer.SetWindowText(strIssuer);

    nsXPIDLString subjectName;
    serverCert->GetSubjectName(getter_Copies(subjectName));
    CString strOwner = W2T(subjectName.get());
    strOwner.Replace("CN=", "");
    strOwner.Replace("OU=", "\n");
    strOwner.Replace("O=", "\n");
    strOwner.Replace("L=", "\n");
    strOwner.Replace(", ", "");
    strOwner.Replace("ST=", ", ");
    strOwner.Replace("C=", ", ");
    m_CertOwner.SetWindowText(strOwner);

    nsXPIDLString serialNumber;
    serverCert->GetSerialNumber(getter_Copies(serialNumber));
    m_CertSerial.SetWindowText(W2T(serialNumber.get()));
    
    nsXPIDLString issuedDate;
    serverCert->GetIssuedDate(getter_Copies(issuedDate));
    m_CertIssued.SetWindowText(W2T(issuedDate.get()));

    nsXPIDLString expiresDate;
    serverCert->GetExpiresDate(getter_Copies(expiresDate));
    m_CertExpires.SetWindowText(W2T(expiresDate.get()));

    nsXPIDLString sha1FingerPrint;
    serverCert->GetSha1Fingerprint(getter_Copies(sha1FingerPrint));
    m_CertSha1Fingerprint.SetWindowText(W2T(sha1FingerPrint.get()));

    nsXPIDLString md5FingerPrint;
    serverCert->GetMd5Fingerprint(getter_Copies(md5FingerPrint));
    m_CertMd5Fingerprint.SetWindowText(W2T(md5FingerPrint.get()));

    return TRUE;  // return TRUE unless you set the focus to a control
}
