/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Terry Hayes <thayes@netscape.com>
 *   Javier Delgadillo <javi@netscape.com>
 *   Oleg Romashin <romaxa@gmail.com>
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
/**
 * Derived from nsNSSDialogs http://landfill.mozilla.org/mxr-test/seamonkey/source/security/manager/pki/src/nsNSSDialogs.cpp
 */
#include <strings.h>
#include "nsIURI.h"
#include "EmbedPrivate.h"
#include "nsServiceManagerUtils.h"
#include "nsIWebNavigationInfo.h"
#include "nsDocShellCID.h"
#include "nsCOMPtr.h"
#ifdef MOZILLA_INTERNAL_API
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#else
#include "nsStringAPI.h"
#endif
#include "nsIPrompt.h"
#include "nsIDOMWindowInternal.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsILocaleService.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "EmbedCertificates.h"
#include "nsIKeygenThread.h"
#include "nsIX509CertValidity.h"
#include "nsICRLInfo.h"
#include "gtkmozembed.h"

#define PIPSTRING_BUNDLE_URL "chrome://pippki/locale/pippki.properties"

static NS_DEFINE_CID(kCStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);

EmbedCertificates::EmbedCertificates(void)
{
}

EmbedCertificates::~EmbedCertificates()
{
}

NS_IMPL_THREADSAFE_ADDREF(EmbedCertificates)
NS_IMPL_THREADSAFE_RELEASE(EmbedCertificates)
NS_INTERFACE_MAP_BEGIN(EmbedCertificates)
NS_INTERFACE_MAP_ENTRY(nsITokenPasswordDialogs)
NS_INTERFACE_MAP_ENTRY(nsIBadCertListener)
#ifdef BAD_CERT_LISTENER2
NS_INTERFACE_MAP_ENTRY(nsIBadCertListener2)
#endif
NS_INTERFACE_MAP_ENTRY(nsICertificateDialogs)
NS_INTERFACE_MAP_ENTRY(nsIClientAuthDialogs)
NS_INTERFACE_MAP_ENTRY(nsICertPickDialogs)
NS_INTERFACE_MAP_ENTRY(nsITokenDialogs)
NS_INTERFACE_MAP_ENTRY(nsIGeneratingKeypairInfoDialogs)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMCryptoDialogs)
NS_INTERFACE_MAP_END

nsresult
EmbedCertificates::Init(void)
{
  nsresult rv;
  nsCOMPtr<nsIStringBundleService> service = do_GetService(kCStringBundleServiceCID, &rv);
  if (NS_FAILED(rv)) return NS_OK;
  rv = service->CreateBundle(PIPSTRING_BUNDLE_URL,
                             getter_AddRefs(mPIPStringBundle));
  return NS_OK;
}

nsresult
EmbedCertificates::SetPassword(nsIInterfaceRequestor *ctx,
                          const PRUnichar *tokenName, PRBool* _canceled)
{
  *_canceled = PR_FALSE;
  return NS_OK;
}

nsresult
EmbedCertificates::GetPassword(nsIInterfaceRequestor *ctx,
                          const PRUnichar *tokenName,
                          PRUnichar **_password,
                          PRBool* _canceled)
{
  *_canceled = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
EmbedCertificates::ConfirmUnknownIssuer(nsIInterfaceRequestor *socketInfo,
                                   nsIX509Cert *cert, PRInt16 *outAddType,
                                   PRBool *_retval)
{
  *outAddType = ADD_TRUSTED_FOR_SESSION;
  *_retval    = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
EmbedCertificates::ConfirmMismatchDomain(nsIInterfaceRequestor *socketInfo,
                                    const nsACString &targetURL,
                                    nsIX509Cert *cert, PRBool *_retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
EmbedCertificates::ConfirmCertExpired(nsIInterfaceRequestor *socketInfo,
                                 nsIX509Cert *cert, PRBool *_retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
EmbedCertificates::NotifyCrlNextupdate(nsIInterfaceRequestor *socketInfo,
                                  const nsACString &targetURL, nsIX509Cert *cert)
{
  return NS_OK;
}

NS_IMETHODIMP
EmbedCertificates::CrlImportStatusDialog(nsIInterfaceRequestor *ctx, nsICRLInfo *crl)
{
  return NS_OK;
}

NS_IMETHODIMP
EmbedCertificates::ConfirmDownloadCACert(nsIInterfaceRequestor *ctx,
                                    nsIX509Cert *cert,
                                    PRUint32 *_trust,
                                    PRBool *_retval)
{
  *_retval = PR_TRUE;
  *_trust |= nsIX509CertDB::TRUSTED_SSL;
  *_trust |= nsIX509CertDB::TRUSTED_EMAIL;
  *_trust |= nsIX509CertDB::TRUSTED_OBJSIGN;
  return NS_OK;
}

NS_IMETHODIMP
EmbedCertificates::NotifyCACertExists(nsIInterfaceRequestor *ctx)
{
  return NS_OK;
}

NS_IMETHODIMP
EmbedCertificates::ChooseCertificate(
  nsIInterfaceRequestor *ctx,
  const PRUnichar *cn,
  const PRUnichar *organization,
  const PRUnichar *issuer,
  const PRUnichar **certNickList,
  const PRUnichar **certDetailsList,
  PRUint32 count,
  PRInt32 *selectedIndex,
  PRBool *canceled)
{
  *canceled = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
EmbedCertificates::PickCertificate(
  nsIInterfaceRequestor *ctx,
  const PRUnichar **certNickList,
  const PRUnichar **certDetailsList,
  PRUint32 count,
  PRInt32 *selectedIndex,
  PRBool *canceled)
{
  *canceled = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
EmbedCertificates::SetPKCS12FilePassword(
  nsIInterfaceRequestor *ctx,
  nsAString &_password,
  PRBool *_retval)
{
  /* The person who wrote this method implementation did
   * not read the contract.
   */
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
EmbedCertificates::GetPKCS12FilePassword(
  nsIInterfaceRequestor *ctx,
  nsAString &_password,
  PRBool *_retval)
{
  /* The person who wrote this method implementation did
   * not read the contract.
   */
  *_retval = PR_FALSE;
  return NS_OK;
}

/* void viewCert (in nsIX509Cert cert); */
NS_IMETHODIMP
EmbedCertificates::ViewCert(
  nsIInterfaceRequestor *ctx,
  nsIX509Cert *cert)
{
  return NS_OK;
}

NS_IMETHODIMP
EmbedCertificates::DisplayGeneratingKeypairInfo(nsIInterfaceRequestor *aCtx, nsIKeygenThread *runnable)
{
  return NS_OK;
}

NS_IMETHODIMP
EmbedCertificates::ChooseToken(
  nsIInterfaceRequestor *aCtx,
  const PRUnichar **aTokenList,
  PRUint32 aCount,
  PRUnichar **aTokenChosen,
  PRBool *aCanceled)
{
  *aCanceled = PR_FALSE;
  return NS_OK;
}

/* boolean ConfirmKeyEscrow (in nsIX509Cert escrowAuthority); */
NS_IMETHODIMP
EmbedCertificates::ConfirmKeyEscrow(nsIX509Cert *escrowAuthority, PRBool *_retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}

#ifdef BAD_CERT_LISTENER2
NS_IMETHODIMP
EmbedCertificates::ConfirmBadCertificate(
  nsIInterfaceRequestor *ctx,
  nsIX509Cert *cert,
  PRBool aSecSuccess,
  PRUint32 aError,
  PRBool *_retval)
{
  nsresult rv;
  gpointer pCert = NULL;
  guint messint = 0;
  nsCOMPtr<nsIDOMWindow> parent(do_GetInterface(ctx));

  GtkMozEmbedCommon * common = nsnull;
  GtkMozEmbed *parentWidget = GTK_MOZ_EMBED(GetGtkWidgetForDOMWindow(parent));

  if (!parentWidget) {
    EmbedCommon * embedcommon = EmbedCommon::GetInstance();
    if (embedcommon)
      common = GTK_MOZ_EMBED_COMMON(embedcommon->mCommon);
  }

  if (!(aError & nsIX509Cert::VERIFIED_OK)) {
    pCert = (gpointer)cert;
    messint = GTK_MOZ_EMBED_CERT_VERIFIED_OK;
    if (aError & nsIX509Cert::NOT_VERIFIED_UNKNOWN) {
      messint |= GTK_MOZ_EMBED_CERT_NOT_VERIFIED_UNKNOWN;
    }
    if (aError & nsIX509Cert::CERT_EXPIRED || aError & nsIX509Cert::CERT_REVOKED) {
      nsCOMPtr<nsIX509CertValidity> validity;
      rv = cert->GetValidity(getter_AddRefs(validity));
      if (NS_SUCCEEDED(rv)) {
        PRTime notBefore, notAfter, timeToUse;
        PRTime now = PR_Now();
        rv = validity->GetNotBefore(&notBefore);
        if (NS_FAILED(rv))
          return rv;
        rv = validity->GetNotAfter(&notAfter);
        if (NS_FAILED(rv))
          return rv;
        if (LL_CMP(now, >, notAfter)) {
          messint |= GTK_MOZ_EMBED_CERT_EXPIRED;
          timeToUse = notAfter;
        } else {
          messint |= GTK_MOZ_EMBED_CERT_REVOKED;
          timeToUse = notBefore;
        }
      }
    }
    if (aError & nsIX509Cert::CERT_NOT_TRUSTED) {
      messint |= GTK_MOZ_EMBED_CERT_UNTRUSTED;
    }
    if (aError & nsIX509Cert::ISSUER_UNKNOWN) {
      messint |= GTK_MOZ_EMBED_CERT_ISSUER_UNKNOWN;
    }
    if (aError & nsIX509Cert::ISSUER_NOT_TRUSTED) {
      messint |= GTK_MOZ_EMBED_CERT_ISSUER_UNTRUSTED;
    }
    if (aError & nsIX509Cert::INVALID_CA) {
      messint |= GTK_MOZ_EMBED_CERT_INVALID_CA;
    }
    if (aError & nsIX509Cert::USAGE_NOT_ALLOWED) {
    }
    PRBool retVal = PR_FALSE;
    if (common) {
      g_signal_emit_by_name(common, "certificate-error", pCert, messint, &retVal);
    }
    if (retVal == PR_TRUE) {
      *_retval = PR_FALSE;
      rv = NS_ERROR_FAILURE;
    } else {
      rv = NS_OK;
      *_retval = PR_TRUE;
    }
    pCert = NULL;
  } else {
    rv = NS_OK;
    *_retval = PR_TRUE;
  }
  return rv;
}
#endif
