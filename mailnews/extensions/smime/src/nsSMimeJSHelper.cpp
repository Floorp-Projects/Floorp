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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nspr.h"
#include "nsSMimeJSHelper.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsReadableUtils.h"
#include "nsLiteralString.h"
#include "nsString.h"
#include "nsIMsgHeaderParser.h"
#include "nsIX509CertDB.h"
#include "nsIX509CertValidity.h"
#include "nsIServiceManager.h"
#include "nsPromiseFlatString.h"
#include "nsCRT.h"

NS_IMPL_ISUPPORTS1(nsSMimeJSHelper, nsISMimeJSHelper)

nsSMimeJSHelper::nsSMimeJSHelper()
{
}

nsSMimeJSHelper::~nsSMimeJSHelper()
{
}

NS_IMETHODIMP nsSMimeJSHelper::GetRecipientCertsInfo(
    nsIMsgCompFields *compFields, 
    PRUint32 *count, 
    PRUnichar ***emailAddresses, 
    PRInt32 **certVerification, 
    PRUnichar ***certIssuedInfos, 
    PRUnichar ***certExpiresInfos, 
    nsIX509Cert ***certs,
    PRBool *canEncrypt)
{
  NS_ENSURE_ARG_POINTER(count);
  *count = 0;
  
  NS_ENSURE_ARG_POINTER(emailAddresses);
  NS_ENSURE_ARG_POINTER(certVerification);
  NS_ENSURE_ARG_POINTER(certIssuedInfos);
  NS_ENSURE_ARG_POINTER(certExpiresInfos);
  NS_ENSURE_ARG_POINTER(certs);
  NS_ENSURE_ARG_POINTER(canEncrypt);

  NS_ENSURE_ARG_POINTER(compFields);

  PRUint32 mailbox_count;
  char *mailbox_list;

  nsresult rv = getMailboxList(compFields, &mailbox_count, &mailbox_list);
  if (NS_FAILED(rv))
    return rv;

  if (!mailbox_list)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIX509CertDB> certdb = do_GetService(NS_X509CERTDB_CONTRACTID);

  *count = mailbox_count;
  *canEncrypt = PR_FALSE;
  rv = NS_OK;

  if (mailbox_count)
  {
    PRUnichar **outEA = NS_STATIC_CAST(PRUnichar **, nsMemory::Alloc(mailbox_count * sizeof(PRUnichar *)));
    PRInt32 *outCV = NS_STATIC_CAST(PRInt32 *, nsMemory::Alloc(mailbox_count * sizeof(PRInt32)));
    PRUnichar **outCII = NS_STATIC_CAST(PRUnichar **, nsMemory::Alloc(mailbox_count * sizeof(PRUnichar *)));
    PRUnichar **outCEI = NS_STATIC_CAST(PRUnichar **, nsMemory::Alloc(mailbox_count * sizeof(PRUnichar *)));
    nsIX509Cert **outCerts = NS_STATIC_CAST(nsIX509Cert **, nsMemory::Alloc(mailbox_count * sizeof(nsIX509Cert *)));

    if (!outEA || !outCV || !outCII || !outCEI || !outCerts)
    {
      nsMemory::Free(outEA);
      nsMemory::Free(outCV);
      nsMemory::Free(outCII);
      nsMemory::Free(outCEI);
      nsMemory::Free(outCerts);
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
    else
    {
      PRUnichar **iEA = outEA;
      PRInt32 *iCV = outCV;
      PRUnichar **iCII = outCII;
      PRUnichar **iCEI = outCEI;
      nsIX509Cert **iCert = outCerts;

      PRBool found_blocker = PR_FALSE;
      PRBool memory_failure = PR_FALSE;

      const char *walk = mailbox_list;

      // To understand this loop, especially the "+= strlen +1", look at the documentation
      // of ParseHeaderAddresses. Basically, it returns a list of zero terminated strings.
      for (PRUint32 i = 0;
          i < mailbox_count;
          ++i, ++iEA, ++iCV, ++iCII, ++iCEI, ++iCert, walk += strlen(walk) + 1)
      {
        *iCert = nsnull;
        *iCV = 0;
        *iCII = nsnull;
        *iCEI = nsnull;

        if (memory_failure) {
          *iEA = nsnull;
          continue;
        }

        nsDependentCString email(walk);
        *iEA = ToNewUnicode(email);
        if (!*iEA) {
          memory_failure = PR_TRUE;
          continue;
        }

        nsCString email_lowercase;
        ToLowerCase(email, email_lowercase);

        nsCOMPtr<nsIX509Cert> cert;
        if (NS_SUCCEEDED(certdb->FindCertByEmailAddress(nsnull, email_lowercase.get(), getter_AddRefs(cert))) 
            && cert)
        {
          *iCert = cert;
          NS_ADDREF(*iCert);
          
          PRUint32 verification_result;
          
          if (NS_FAILED(
              cert->VerifyForUsage(nsIX509Cert::CERT_USAGE_EmailRecipient, &verification_result)))
          {
            *iCV = nsIX509Cert::NOT_VERIFIED_UNKNOWN;
            found_blocker = PR_TRUE;
          }
          else
          {
            *iCV = verification_result;
            
            if (verification_result != nsIX509Cert::VERIFIED_OK)
            {
              found_blocker = PR_TRUE;
            }
          }
          
          nsCOMPtr<nsIX509CertValidity> validity;
          rv = cert->GetValidity(getter_AddRefs(validity));

          if (NS_SUCCEEDED(rv)) {
            nsXPIDLString id, ed;

            if (NS_SUCCEEDED(validity->GetNotBeforeLocalDay(id)))
            {
              *iCII = ToNewUnicode(id);
              if (!*iCII) {
                memory_failure = PR_TRUE;
                continue;
              }
            }

            if (NS_SUCCEEDED(validity->GetNotAfterLocalDay(ed)))
            {
              *iCEI = ToNewUnicode(ed);
              if (!*iCEI) {
                memory_failure = PR_TRUE;
                continue;
              }
            }
          }
        }
        else
        {
          found_blocker = PR_TRUE;
        }
      }

      if (memory_failure) {
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mailbox_count, outEA);
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mailbox_count, outCII);
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mailbox_count, outCEI);
        NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(mailbox_count, outCerts);
        nsMemory::Free(outCV);
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
      else {
        if (mailbox_count > 0 && !found_blocker)
        {
          *canEncrypt = PR_TRUE;
        }

        *emailAddresses = outEA;
        *certVerification = outCV;
        *certIssuedInfos = outCII;
        *certExpiresInfos = outCEI;
        *certs = outCerts;
      }
    }
  }

  if (mailbox_list) {
    nsMemory::Free(mailbox_list);
  }
  return rv;
}

NS_IMETHODIMP nsSMimeJSHelper::GetNoCertAddresses(
    nsIMsgCompFields *compFields, 
    PRUint32 *count, 
    PRUnichar ***emailAddresses)
{
  NS_ENSURE_ARG_POINTER(count);
  *count = 0;
  
  NS_ENSURE_ARG_POINTER(emailAddresses);

  NS_ENSURE_ARG_POINTER(compFields);

  PRUint32 mailbox_count;
  char *mailbox_list;

  nsresult rv = getMailboxList(compFields, &mailbox_count, &mailbox_list);
  if (NS_FAILED(rv))
    return rv;

  if (!mailbox_list)
    return NS_ERROR_FAILURE;

  if (!mailbox_count)
  {
    *count = 0;
    *emailAddresses = nsnull;
    if (mailbox_list) {
      nsMemory::Free(mailbox_list);
    }
    return NS_OK;
  }

  nsCOMPtr<nsIX509CertDB> certdb = do_GetService(NS_X509CERTDB_CONTRACTID);

  PRUint32 missing_count = 0;
  PRBool *haveCert = new PRBool[mailbox_count];
  if (!haveCert)
  {
    if (mailbox_list) {
      nsMemory::Free(mailbox_list);
    }
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = NS_OK;

  if (mailbox_count)
  {
    const char *walk = mailbox_list;

    // To understand this loop, especially the "+= strlen +1", look at the documentation
    // of ParseHeaderAddresses. Basically, it returns a list of zero terminated strings.
    for (PRUint32 i = 0;
        i < mailbox_count;
        ++i, walk += strlen(walk) + 1)
    {
      haveCert[i] = PR_FALSE;

      nsDependentCString email(walk);
      nsCString email_lowercase;
      ToLowerCase(email, email_lowercase);

      nsCOMPtr<nsIX509Cert> cert;
      if (NS_SUCCEEDED(certdb->FindCertByEmailAddress(nsnull, email_lowercase.get(), getter_AddRefs(cert))) 
          && cert)
      {
        PRUint32 verification_result;

        if (NS_SUCCEEDED(
              cert->VerifyForUsage(nsIX509Cert::CERT_USAGE_EmailRecipient, &verification_result))
            &&
            nsIX509Cert::VERIFIED_OK == verification_result)
        {
          haveCert[i] = PR_TRUE;
        }
      }

      if (!haveCert[i])
        ++missing_count;
    }
  }

  *count = missing_count;

  if (missing_count)
  {
    PRUnichar **outEA = NS_STATIC_CAST(PRUnichar **, nsMemory::Alloc(missing_count * sizeof(PRUnichar *)));
    if (!outEA )
    {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
    else
    {
      PRUnichar **iEA = outEA;
      const char *walk = mailbox_list;

      PRBool memory_failure = PR_FALSE;

      // To understand this loop, especially the "+= strlen +1", look at the documentation
      // of ParseHeaderAddresses. Basically, it returns a list of zero terminated strings.
      for (PRUint32 i = 0;
          i < mailbox_count;
          ++i, walk += strlen(walk) + 1)
      {
        if (!haveCert[i])
        {
          if (memory_failure) {
            *iEA = nsnull;
          }
          else {
            *iEA = ToNewUnicode(nsDependentCString(walk));
            if (!*iEA) {
              memory_failure = PR_TRUE;
            }
          }
          ++iEA;
        }
      }
      
      if (memory_failure) {
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(missing_count, outEA);
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
      else {
        *emailAddresses = outEA;
      }
    }
  }
  else
  {
    *emailAddresses = nsnull;
  }

  delete [] haveCert;
  if (mailbox_list) {
    nsMemory::Free(mailbox_list);
  }
  return rv;
}

nsresult nsSMimeJSHelper::getMailboxList(nsIMsgCompFields *compFields, PRUint32 *mailbox_count, char **mailbox_list)
{
  NS_ENSURE_ARG(mailbox_count);
  NS_ENSURE_ARG(mailbox_list);

  if (!compFields)
    return NS_ERROR_INVALID_ARG;

  nsresult res;
  nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID, &res);
  if (NS_FAILED(res))
    return res;

  nsXPIDLString to, cc, bcc;
  nsXPIDLCString ng;

  res = compFields->GetTo(to);
  if (NS_FAILED(res))
    return res;

  res = compFields->GetCc(cc);
  if (NS_FAILED(res))
    return res;

  res = compFields->GetBcc(bcc);
  if (NS_FAILED(res))
    return res;

  res = compFields->GetNewsgroups(getter_Copies(ng));
  if (NS_FAILED(res))
    return res;

  *mailbox_list = nsnull;
  *mailbox_count = 0;
  
  {
    nsCString all_recipients;

    if (!to.IsEmpty()) {
      AppendUTF16toUTF8(to, all_recipients);
      all_recipients.Append(',');
    }

    if (!cc.IsEmpty()) {
      AppendUTF16toUTF8(cc, all_recipients);
      all_recipients.Append(',');
    }

    if (!bcc.IsEmpty()) {
      AppendUTF16toUTF8(bcc, all_recipients);
      all_recipients.Append(',');
    }

    all_recipients += ng;

    char *unique_mailboxes = nsnull;

    {
      char *all_mailboxes = nsnull;
      parser->ExtractHeaderAddressMailboxes(nsnull, all_recipients.get(), &all_mailboxes);
      parser->RemoveDuplicateAddresses(nsnull, all_mailboxes, 0, PR_FALSE /*removeAliasesToMe*/, &unique_mailboxes);
      if (all_mailboxes) {
        nsMemory::Free(all_mailboxes);
      }
    }
    if (unique_mailboxes)
    {
      parser->ParseHeaderAddresses(nsnull, unique_mailboxes, 0, mailbox_list, mailbox_count);
    }
    if (unique_mailboxes) {
      nsMemory::Free(unique_mailboxes);
    }
  }

  return NS_OK;
}
