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
#include "nsIServiceManager.h"
#include "nsPromiseFlatString.h"

NS_IMPL_ISUPPORTS1(nsSMimeJSHelper, nsISMimeJSHelper)

nsSMimeJSHelper::nsSMimeJSHelper()
{
  NS_INIT_ISUPPORTS();
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

  nsCOMPtr<nsIX509CertDB> certdb = do_GetService(NS_X509CERTDB_CONTRACTID);
  nsresult res;
  nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID, &res);
  NS_ENSURE_SUCCESS(res,res);

  nsXPIDLString to, cc, bcc;
  compFields->GetTo(getter_Copies(to));
  compFields->GetCc(getter_Copies(cc));
  compFields->GetBcc(getter_Copies(bcc));

  nsXPIDLCString ng;
  compFields->GetNewsgroups(getter_Copies(ng));

  char *mailbox_list = nsnull;
  PRUint32 mailbox_count = 0;
  
  {
    nsCString all_recipients;
    all_recipients.Append(NS_LossyConvertUCS2toASCII(to).get());
    all_recipients.Append(NS_LossyConvertUCS2toASCII(cc).get());
    all_recipients.Append(NS_LossyConvertUCS2toASCII(bcc).get());
    all_recipients.Append(ng);
    char *unique_mailboxes = nsnull;

    {
      char *all_mailboxes = nsnull;
      parser->ExtractHeaderAddressMailboxes(nsnull, all_recipients.get(), &all_mailboxes);
      parser->RemoveDuplicateAddresses(nsnull, all_mailboxes, 0, PR_FALSE /*removeAliasesToMe*/, &unique_mailboxes);
      PR_FREEIF(all_mailboxes);
    }
    if (unique_mailboxes)
    {
      parser->ParseHeaderAddresses(nsnull, unique_mailboxes, 0, &mailbox_list, &mailbox_count);
    }
    PR_FREEIF(unique_mailboxes);
  }

  *count = mailbox_count;
  *canEncrypt = PR_FALSE;
  nsresult rv = NS_OK;

  if (mailbox_count)
  {
    PRUnichar **outEA = (PRUnichar **)nsMemory::Alloc(mailbox_count * sizeof(PRUnichar *));
    PRInt32 *outCV = (PRInt32 *)nsMemory::Alloc(mailbox_count * sizeof(PRInt32));
    PRUnichar **outCII = (PRUnichar **)nsMemory::Alloc(mailbox_count * sizeof(PRUnichar *));
    PRUnichar **outCEI = (PRUnichar **)nsMemory::Alloc(mailbox_count * sizeof(PRUnichar *));
    nsIX509Cert **outCerts = (nsIX509Cert **)nsMemory::Alloc(mailbox_count * sizeof(nsIX509Cert *));

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

      const char *walk = mailbox_list;

      for (PRUint32 i = 0;
          i < mailbox_count;
          ++i, ++iEA, ++iCV, ++iCII, ++iCEI, ++iCert, walk += nsCRT::strlen(walk) + 1)
      {
        *iEA = ToNewUnicode(nsDependentCString(walk));

        *iCert = nsnull;
        *iCV = 0;
        *iCII = nsnull;
        *iCEI = nsnull;

        nsCOMPtr<nsIX509Cert> cert;
        if (NS_SUCCEEDED(certdb->GetCertByEmailAddress(nsnull, walk, getter_AddRefs(cert))) 
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
          
          nsXPIDLString id, ed;

          if (NS_SUCCEEDED(cert->GetIssuedDate(getter_Copies(id))))
          {
            *iCII = ToNewUnicode(id);
          }
          
          if (NS_SUCCEEDED(cert->GetExpiresDate(getter_Copies(ed))))
          {
            *iCEI = ToNewUnicode(ed);
          }
        }
        else
        {
          found_blocker = PR_TRUE;
        }
      }
      
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

  PR_FREEIF(mailbox_list);
  return rv;
}
