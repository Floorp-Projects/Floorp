/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Tsai <jt95070@netscape.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "msgCore.h"
#include "nsMsgMdnGenerator.h"
#include "nsImapCore.h"
#include "nsIMsgImapMailFolder.h"
#include "nsMsgMimeCID.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"
#include "nsCRT.h"
#include "nsMimeTypes.h"
#include "prmem.h"
#include "prprf.h"
#include "nsMsgI18N.h"
#include "nsMailHeaders.h"
#include "nsMsgLocalFolderHdrs.h"
#include "nsNetCID.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIRequest.h"
#include "nsMsgKeyArray.h"
#include "nsISmtpService.h"  // for actually sending the message...
#include "nsMsgCompCID.h"
#include "nsIPrompt.h"
#include "nsIMsgCompUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIStringBundle.h"
#include "nsDirectoryServiceDefs.h"
#include "nsMsgUtils.h"
#include "nsNativeCharsetUtils.h"

#define MDN_NOT_IN_TO_CC          ((int) 0x0001)
#define MDN_OUTSIDE_DOMAIN        ((int) 0x0002)

#define HEADER_RETURN_PATH          "Return-Path"
#define HEADER_DISPOSITION_NOTIFICATION_TO  "Disposition-Notification-To"
#define HEADER_APPARENTLY_TO        "Apparently-To"
#define HEADER_ORIGINAL_RECIPIENT     "Original-Recipient"
#define HEADER_REPORTING_UA                 "Reporting-UA"
#define HEADER_MDN_GATEWAY                  "MDN-Gateway"
#define HEADER_FINAL_RECIPIENT              "Final-Recipient"
#define HEADER_DISPOSITION                  "Disposition"
#define HEADER_ORIGINAL_MESSAGE_ID          "Original-Message-ID"
#define HEADER_FAILURE                      "Failure"
#define HEADER_ERROR                        "Error"
#define HEADER_WARNING                      "Warning"
#define HEADER_RETURN_RECEIPT_TO            "Return-Receipt-To"
#define HEADER_X_ACCEPT_LANGUAGE      "X-Accept-Language"

#define PUSH_N_FREE_STRING(p) \
  do { if (p) { rv = WriteString(p); PR_smprintf_free(p); p=0; \
           if (NS_FAILED(rv)) return rv; } \
     else { return NS_ERROR_OUT_OF_MEMORY; } } while (0)

// String bundle for mdn. Class static.
#define MDN_STRINGBUNDLE_URL "chrome://messenger/locale/msgmdn.properties"

#if defined(DEBUG_jefft)
#define DEBUG_MDN(s) printf("%s\n", s)
#else
#define DEBUG_MDN(s) 
#endif

// machine parsible string; should not be localized
char DispositionTypes[7][16] = {
    "displayed",
    "dispatched",
    "processed",
    "deleted",
    "denied",
    "failed",
    ""
};

NS_IMPL_ISUPPORTS2(nsMsgMdnGenerator, nsIMsgMdnGenerator, nsIUrlListener)

nsMsgMdnGenerator::nsMsgMdnGenerator()
{
    m_disposeType = eDisplayed;
    m_outputStream = nsnull;
    m_reallySendMdn = PR_FALSE;
    m_autoSend = PR_FALSE;
    m_autoAction = PR_FALSE;
    m_mdnEnabled = PR_FALSE;
    m_notInToCcOp = eNeverSendOp;
    m_outsideDomainOp = eNeverSendOp;
    m_otherOp = eNeverSendOp;
}

nsMsgMdnGenerator::~nsMsgMdnGenerator()
{
}

nsresult nsMsgMdnGenerator::FormatStringFromName(const PRUnichar *aName, 
                                                 const PRUnichar *aString, 
                                                 PRUnichar **aResultString)
{
    DEBUG_MDN("nsMsgMdnGenerator::FormatStringFromName");
    nsresult rv;

    nsCOMPtr<nsIStringBundleService>
        bundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr <nsIStringBundle> bundle;
    rv = bundleService->CreateBundle(MDN_STRINGBUNDLE_URL, 
                                     getter_AddRefs(bundle));
    NS_ENSURE_SUCCESS(rv,rv);

    const PRUnichar *formatStrings[1] = { aString };
    rv = bundle->FormatStringFromName(aName,
                    formatStrings, 1, aResultString);
    NS_ENSURE_SUCCESS(rv,rv);
    return rv;
}

nsresult nsMsgMdnGenerator::GetStringFromName(const PRUnichar *aName,
                                               PRUnichar **aResultString)
{
    DEBUG_MDN("nsMsgMdnGenerator::GetStringFromName");
    nsresult rv;

    nsCOMPtr<nsIStringBundleService>
        bundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr <nsIStringBundle> bundle;
    rv = bundleService->CreateBundle(MDN_STRINGBUNDLE_URL, 
                                     getter_AddRefs(bundle));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = bundle->GetStringFromName(aName, aResultString);
    NS_ENSURE_SUCCESS(rv,rv);
    return rv;
}

nsresult nsMsgMdnGenerator::StoreMDNSentFlag(nsIMsgFolder *folder,
                                             nsMsgKey key)
{
    DEBUG_MDN("nsMsgMdnGenerator::StoreMDNSentFlag");
    
    // Store the $MDNSent flag if the folder is an Imap Mail Folder
    // otherwise, do nothing.
    nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(folder);
    if (!imapFolder)
      return NS_OK;

    nsMsgKeyArray keyArray;
    keyArray.Add(key);
    return imapFolder->StoreImapFlags(kImapMsgMDNSentFlag, PR_TRUE,
                               keyArray.GetArray(), keyArray.GetSize(), nsnull);
}

PRBool nsMsgMdnGenerator::ProcessSendMode()
{
    DEBUG_MDN("nsMsgMdnGenerator::ProcessSendMode");
    PRInt32 miscState = 0;
    
    if (m_identity)
    {
        m_identity->GetEmail(getter_Copies(m_email));
        if (!m_email)
            return m_reallySendMdn;

        const char *accountDomain = strchr(m_email.get(), '@');
        if (!accountDomain)
            return m_reallySendMdn;

        if (MailAddrMatch(m_email.get(), m_dntRrt.get())) // return address is self, don't send
          return PR_FALSE;

        // *** fix me see Bug 132504 for more information
        // *** what if the message has been filtered to different account
        if (!PL_strcasestr(m_dntRrt, accountDomain))
            miscState |= MDN_OUTSIDE_DOMAIN;
        if (NotInToOrCc())
            miscState |= MDN_NOT_IN_TO_CC;
        m_reallySendMdn = PR_TRUE;
        // *********
        // How are we gona deal with the auto forwarding issues? Some server
        // didn't bother to add addition header or modify existing header to
        // thev message when forwarding. They simply copy the exact same
        // message to another user's mailbox. Some change To: to
        // Apparently-To: 
        // Unfortunately, there is nothing we can do. It's out of our control.
        // *********
        // starting from lowest denominator to highest
        if (!miscState)
        {   // under normal situation: recipent is in to and cc list,
            // and the sender is from the same domain
            switch (m_otherOp) 
            {
            default:
            case eNeverSendOp:
                m_reallySendMdn = PR_FALSE;
                break;
            case eAutoSendOp:
                m_autoSend = PR_TRUE;
                break;
            case eAskMeOp:
                m_autoSend = PR_FALSE;
                break;
            case eDeniedOp:
                m_autoSend = PR_TRUE;
                m_disposeType = eDenied;
                break;
            }
        }
        else if (miscState == (MDN_OUTSIDE_DOMAIN | MDN_NOT_IN_TO_CC))
        {
            if (m_outsideDomainOp != m_notInToCcOp)
            {
                m_autoSend = PR_FALSE; // ambiguous; always ask user
            }
            else
            {
                switch (m_outsideDomainOp) 
                {
                default:
                case eNeverSendOp:
                    m_reallySendMdn = PR_FALSE;
                    break;
                case eAutoSendOp:
                    m_autoSend = PR_TRUE;
                    break;
                case eAskMeOp:
                    m_autoSend = PR_FALSE;
                    break;
                }
            }
        }
        else if (miscState & MDN_OUTSIDE_DOMAIN)
        {
            switch (m_outsideDomainOp) 
            {
            default:
            case eNeverSendOp:
                m_reallySendMdn = PR_FALSE;
                break;
            case eAutoSendOp:
                m_autoSend = PR_TRUE;
                break;
            case eAskMeOp:
                m_autoSend = PR_FALSE;
                break;
            }
        }
        else if (miscState & MDN_NOT_IN_TO_CC)
        {
            switch (m_notInToCcOp) 
            {
            default:
            case eNeverSendOp:
                m_reallySendMdn = PR_FALSE;
                break;
            case eAutoSendOp:
                m_autoSend = PR_TRUE;
                break;
            case eAskMeOp:
                m_autoSend = PR_FALSE;
                break;
            }
        }
    }
    return m_reallySendMdn;
}

PRBool nsMsgMdnGenerator::MailAddrMatch(const char *addr1, const char *addr2)
{
    // Comparing two email addresses returns true if matched; local/account
    // part comparison is case sensitive; domain part comparison is case
    // insensitive 
    DEBUG_MDN("nsMsgMdnGenerator::MailAddrMatch");
    PRBool isMatched = PR_TRUE;
    const char *atSign1 = nsnull, *atSign2 = nsnull;
    const char *lt = nsnull, *local1 = nsnull, *local2 = nsnull;
    const char *end1 = nsnull, *end2 = nsnull;

    if (!addr1 || !addr2)
        return PR_FALSE;
    
    lt = strchr(addr1, '<');
    local1 = !lt ? addr1 : lt+1;
    lt = strchr(addr2, '<');
    local2 = !lt ? addr2 : lt+1;
    end1 = strchr(local1, '>');
    if (!end1)
        end1 = addr1 + strlen(addr1);
    end2 = strchr(local2, '>');
    if (!end2)
        end2 = addr2 + strlen(addr2);
    atSign1 = strchr(local1, '@');
    atSign2 = strchr(local2, '@');
    if (!atSign1 || !atSign2 // ill formed addr spec
        || (atSign1 - local1) != (atSign2 - local2))
        isMatched = PR_FALSE;
    else if (strncmp(local1, local2, (atSign1-local1))) // case sensitive
        // compare for local part
        isMatched = PR_FALSE;
    else if ((end1 - atSign1) != (end2 - atSign2) ||
             PL_strncasecmp(atSign1, atSign2, (end1-atSign1))) // case
        // insensitive compare for domain part
        isMatched = PR_FALSE;
    return isMatched;
}

PRBool nsMsgMdnGenerator::NotInToOrCc()
{
    DEBUG_MDN("nsMsgMdnGenerator::NotInToOrCc");
    nsXPIDLCString reply_to;
    nsXPIDLCString to;
    nsXPIDLCString cc;

    m_identity->GetReplyTo(getter_Copies(reply_to));
    m_headers->ExtractHeader(HEADER_TO, PR_TRUE, getter_Copies(to));
    m_headers->ExtractHeader(HEADER_CC, PR_TRUE, getter_Copies(cc));
  
  // start with a simple check
  if ((to.Length() && PL_strcasestr(to.get(), m_email.get())) || 
      (cc.Length() && PL_strcasestr(cc.get(), m_email.get()))) {
      return PR_FALSE;
  }

  if ((reply_to.Length() && to.Length() && PL_strcasestr(to.get(), reply_to.get())) ||
      (reply_to.Length() && cc.Length() && PL_strcasestr(cc.get(), reply_to.get()))) {
      return PR_FALSE;
  }
  return PR_TRUE;
}

PRBool nsMsgMdnGenerator::ValidateReturnPath()
{
    DEBUG_MDN("nsMsgMdnGenerator::ValidateReturnPath");
    // ValidateReturnPath applies to Automatic Send Mode only. If we were not
    // in auto send mode we simply by passing the check
    if (!m_autoSend)
        return m_reallySendMdn;
    
    nsXPIDLCString returnPath;
    m_headers->ExtractHeader(HEADER_RETURN_PATH, PR_FALSE,
                             getter_Copies(returnPath));
    if (!returnPath || !*returnPath)
    {
        m_autoSend = PR_FALSE;
        return m_reallySendMdn;
    }
    m_autoSend = MailAddrMatch(returnPath, m_dntRrt);
    return m_reallySendMdn;
}

nsresult nsMsgMdnGenerator::CreateMdnMsg()
{
    DEBUG_MDN("nsMsgMdnGenerator::CreateMdnMsg");
    nsresult rv;
    if (!m_autoSend)
    {
        nsCOMPtr<nsIPrompt> dialog;
        rv = m_window->GetPromptDialog(getter_AddRefs(dialog));
        if (NS_SUCCEEDED(rv))
        {
            nsXPIDLString wishToSend;
            rv = GetStringFromName(
                NS_LITERAL_STRING("MsgMdnWishToSend").get(),
                getter_Copies(wishToSend));
            if (NS_SUCCEEDED(rv))
            {
                PRBool bVal = PR_FALSE;
                rv = dialog->Confirm(nsnull, wishToSend, &bVal);
                if (NS_SUCCEEDED(rv))
                    m_reallySendMdn = bVal;
            }
        }
    }
    if (!m_reallySendMdn)
        return NS_OK;

    

    nsCOMPtr<nsIFile> tmpFile;
    rv = GetSpecialDirectoryWithFileName(NS_OS_TEMP_DIR,
                                         "mdnmsg",
                                         getter_AddRefs(tmpFile));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = tmpFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 00600);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_NewFileSpecFromIFile(tmpFile, getter_AddRefs(m_fileSpec));

    NS_ASSERTION(NS_SUCCEEDED(rv),"creating mdn: failed to create");
    if (NS_FAILED(rv)) 
        return NS_OK;

    rv = m_fileSpec->GetOutputStream(getter_AddRefs(m_outputStream));
    NS_ASSERTION(NS_SUCCEEDED(rv),"creating mdn: failed to output stream");
    if (NS_FAILED(rv)) 
        return NS_OK;

    rv = CreateFirstPart();
    if (NS_SUCCEEDED(rv))
    {
        rv = CreateSecondPart();
        if (NS_SUCCEEDED(rv))
            rv = CreateThirdPart();
    }

    if (m_outputStream)
    {
        m_outputStream->Flush();
        m_outputStream->Close();
    }
    if (m_fileSpec)
        m_fileSpec->CloseStream();
    if (NS_FAILED(rv))
        m_fileSpec->Delete(PR_FALSE);
    else
        rv = SendMdnMsg();

    return NS_OK;
}

nsresult nsMsgMdnGenerator::CreateFirstPart()
{
    DEBUG_MDN("nsMsgMdnGenerator::CreateFirstPart");
    char *convbuf = nsnull, *tmpBuffer = nsnull;
    char *parm = nsnull;
    nsXPIDLString firstPart1;
    nsXPIDLString firstPart2;
    nsresult rv = NS_OK;
    nsCOMPtr <nsIMsgCompUtils> compUtils;
    
    if (!m_mimeSeparator)
    {
      compUtils = do_GetService(NS_MSGCOMPUTILS_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = compUtils->MimeMakeSeparator("mdn", getter_Copies(m_mimeSeparator));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (!m_mimeSeparator)
        return NS_ERROR_OUT_OF_MEMORY;

    tmpBuffer = (char *) PR_CALLOC(256);

    if (!tmpBuffer)
        return NS_ERROR_OUT_OF_MEMORY;

    PRExplodedTime now;
    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &now);
    
    int gmtoffset = (now.tm_params.tp_gmt_offset + now.tm_params.tp_dst_offset)
        / 60;
  /* Use PR_FormatTimeUSEnglish() to format the date in US English format,
     then figure out what our local GMT offset is, and append it (since
     PR_FormatTimeUSEnglish() can't do that.) Generate four digit years as
     per RFC 1123 (superceding RFC 822.)
  */
    PR_FormatTimeUSEnglish(tmpBuffer, 100,
                           "Date: %a, %d %b %Y %H:%M:%S ",
                           &now);

    PR_snprintf(tmpBuffer + strlen(tmpBuffer), 100,
                "%c%02d%02d" CRLF,
                (gmtoffset >= 0 ? '+' : '-'),
                ((gmtoffset >= 0 ? gmtoffset : -gmtoffset) / 60),
                ((gmtoffset >= 0 ? gmtoffset : -gmtoffset) % 60));

    rv = WriteString(tmpBuffer);
    PR_Free(tmpBuffer);
    if (NS_FAILED(rv)) 
        return rv;
    
    PRBool conformToStandard = PR_FALSE;
    if (compUtils)
      compUtils->GetMsgMimeConformToStandard(&conformToStandard);

    convbuf = nsMsgI18NEncodeMimePartIIStr(
        m_email.get(), PR_TRUE, m_charset.get(), 0,
        conformToStandard);
    parm = PR_smprintf("From: %s" CRLF, convbuf ? convbuf : m_email.get());

    rv = FormatStringFromName(NS_LITERAL_STRING("MsgMdnMsgSentTo").get(), NS_ConvertASCIItoUTF16(m_email).get(),
                            getter_Copies(firstPart1));
    if (NS_FAILED(rv)) 
        return rv;

    PUSH_N_FREE_STRING (parm);
    
    PR_Free(convbuf);
    
    if (compUtils)
    {
      nsXPIDLCString msgId;
      rv = compUtils->MsgGenerateMessageId(m_identity, getter_Copies(msgId));
      tmpBuffer = PR_smprintf("Message-ID: %s" CRLF, msgId.get());
      PUSH_N_FREE_STRING(tmpBuffer);
    }

    nsXPIDLString receipt_string;
    switch (m_disposeType)
    {
    case nsIMsgMdnGenerator::eDisplayed:
        rv = GetStringFromName(
            NS_LITERAL_STRING("MdnDisplayedReceipt").get(),
            getter_Copies(receipt_string));
        break;
    case nsIMsgMdnGenerator::eDispatched:
        rv = GetStringFromName(
            NS_LITERAL_STRING("MdnDispatchedReceipt").get(),
            getter_Copies(receipt_string));
        break;
    case nsIMsgMdnGenerator::eProcessed:
        rv = GetStringFromName(
            NS_LITERAL_STRING("MdnProcessedReceipt").get(),
            getter_Copies(receipt_string));
        break;
    case nsIMsgMdnGenerator::eDeleted:
        rv = GetStringFromName(
            NS_LITERAL_STRING("MdnDeletedReceipt").get(),
            getter_Copies(receipt_string));
        break;
    case nsIMsgMdnGenerator::eDenied:
        rv = GetStringFromName(
            NS_LITERAL_STRING("MdnDeniedReceipt").get(),
            getter_Copies(receipt_string));
        break;
    case nsIMsgMdnGenerator::eFailed:
        rv = GetStringFromName(
            NS_LITERAL_STRING("MdnFailedReceipt").get(),
            getter_Copies(receipt_string));
        break;
    default:
        rv = NS_ERROR_INVALID_ARG;
        break;
    }

    if (NS_FAILED(rv)) 
        return rv;
    
    receipt_string.Append(NS_LITERAL_STRING(" - "));
        
    char * encodedReceiptString = nsMsgI18NEncodeMimePartIIStr(NS_ConvertUTF16toUTF8(receipt_string).get(), PR_FALSE, 
                                                               "UTF-8", 0, conformToStandard);
   
    nsXPIDLCString subject;
    m_headers->ExtractHeader(HEADER_SUBJECT, PR_FALSE, getter_Copies(subject));
    convbuf = nsMsgI18NEncodeMimePartIIStr(subject.Length() ? subject.get() : "[no subject]", 
                                           PR_FALSE, m_charset.get(), 0, conformToStandard);
    tmpBuffer = PR_smprintf("Subject: %s%s" CRLF, 
                             encodedReceiptString,
                            (convbuf ? convbuf : (subject.Length() ? subject.get() : 
                              "[no subject]")));

    PUSH_N_FREE_STRING(tmpBuffer);
    PR_Free(convbuf);
    PR_Free(encodedReceiptString);

    convbuf = nsMsgI18NEncodeMimePartIIStr(m_dntRrt, PR_TRUE, m_charset.get(), 0, conformToStandard);
    tmpBuffer = PR_smprintf("To: %s" CRLF, convbuf ? convbuf :
                            m_dntRrt.get());
    PUSH_N_FREE_STRING(tmpBuffer);

    PR_Free(convbuf);

  // *** This is not in the spec. I am adding this so we could do
  // threading
    m_headers->ExtractHeader(HEADER_MESSAGE_ID, PR_FALSE,
                             getter_Copies(m_messageId));
    
    if (!m_messageId.IsEmpty())
    {
      if (*m_messageId.get() == '<')
          tmpBuffer = PR_smprintf("References: %s" CRLF, m_messageId.get());
      else
          tmpBuffer = PR_smprintf("References: <%s>" CRLF, m_messageId.get());
      PUSH_N_FREE_STRING(tmpBuffer);
    }
    tmpBuffer = PR_smprintf("%s" CRLF, "MIME-Version: 1.0");
    PUSH_N_FREE_STRING(tmpBuffer);

    tmpBuffer = PR_smprintf("Content-Type: multipart/report; \
report-type=disposition-notification;\r\n\tboundary=\"%s\"" CRLF CRLF,
                            m_mimeSeparator.get()); 
    PUSH_N_FREE_STRING(tmpBuffer);
    
    tmpBuffer = PR_smprintf("--%s" CRLF, m_mimeSeparator.get());
    PUSH_N_FREE_STRING(tmpBuffer);

    tmpBuffer = PR_smprintf("Content-Type: text/plain; charset=UTF-8" CRLF);
    PUSH_N_FREE_STRING(tmpBuffer);

    tmpBuffer = PR_smprintf("Content-Transfer-Encoding: %s" CRLF CRLF,
                            ENCODING_8BIT);
    PUSH_N_FREE_STRING(tmpBuffer);
  
    if (!firstPart1.IsEmpty())
    {
        tmpBuffer = PR_smprintf("%s" CRLF CRLF, NS_ConvertUTF16toUTF8(firstPart1).get());
        PUSH_N_FREE_STRING(tmpBuffer);
    }

    switch (m_disposeType)
    {
    case nsIMsgMdnGenerator::eDisplayed:
        rv = GetStringFromName(
            NS_LITERAL_STRING("MsgMdnDisplayed").get(),
            getter_Copies(firstPart2));
        break;
    case nsIMsgMdnGenerator::eDispatched:
        rv = GetStringFromName(
            NS_LITERAL_STRING("MsgMdnDisptched").get(),
            getter_Copies(firstPart2));
        break;
    case nsIMsgMdnGenerator::eProcessed:
        rv = GetStringFromName(
            NS_LITERAL_STRING("MsgMdnProcessed").get(),
            getter_Copies(firstPart2));
        break;
    case nsIMsgMdnGenerator::eDeleted:
        rv = GetStringFromName(
            NS_LITERAL_STRING("MsgMdnDeleted").get(),
            getter_Copies(firstPart2));
        break;
    case nsIMsgMdnGenerator::eDenied:
        rv = GetStringFromName(
            NS_LITERAL_STRING("MsgMdnDenied").get(),
            getter_Copies(firstPart2));
        break;
    case nsIMsgMdnGenerator::eFailed:
        rv = GetStringFromName(
            NS_LITERAL_STRING("MsgMdnFailed").get(),
            getter_Copies(firstPart2));
        break;
    default:
        rv = NS_ERROR_INVALID_ARG;
        break;
    }
    
    if (NS_FAILED(rv)) 
        return rv;
    
    if (!firstPart2.IsEmpty())
    {
        tmpBuffer = 
            PR_smprintf("%s" CRLF CRLF, 
                        NS_ConvertUTF16toUTF8(firstPart2).get()); 
        PUSH_N_FREE_STRING(tmpBuffer);
    }
    
    return rv;
}

nsresult nsMsgMdnGenerator::CreateSecondPart()
{
    DEBUG_MDN("nsMsgMdnGenerator::CreateSecondPart");
    char *tmpBuffer = nsnull;
    char *convbuf = nsnull;
    nsresult rv = NS_OK;
    nsCOMPtr <nsIMsgCompUtils> compUtils;
    PRBool conformToStandard = PR_FALSE;
    
    tmpBuffer = PR_smprintf("--%s" CRLF, m_mimeSeparator.get());
    PUSH_N_FREE_STRING(tmpBuffer);
    
    tmpBuffer = PR_smprintf("%s" CRLF, "Content-Type: message/disposition-notification; name=\042MDNPart2.txt\042");
    PUSH_N_FREE_STRING(tmpBuffer);
    
    tmpBuffer = PR_smprintf("%s" CRLF, "Content-Disposition: inline");
    PUSH_N_FREE_STRING(tmpBuffer);
    
    tmpBuffer = PR_smprintf("Content-Transfer-Encoding: %s" CRLF CRLF,
                            ENCODING_7BIT);
    PUSH_N_FREE_STRING(tmpBuffer);
    
    nsCOMPtr<nsIHttpProtocolHandler> pHTTPHandler = 
        do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv); 
    if (NS_SUCCEEDED(rv) && pHTTPHandler)
    {
        nsCAutoString userAgentString;
        pHTTPHandler->GetUserAgent(userAgentString);

        if (!userAgentString.IsEmpty())
        {
            tmpBuffer = PR_smprintf("Reporting-UA: %s" CRLF,
                                    userAgentString.get());
            PUSH_N_FREE_STRING(tmpBuffer);
        }
    }

    nsXPIDLCString originalRecipient;
    m_headers->ExtractHeader(HEADER_ORIGINAL_RECIPIENT, PR_FALSE,
                             getter_Copies(originalRecipient));
    
    if (originalRecipient && *originalRecipient)
    {
        tmpBuffer = PR_smprintf("Original-Recipient: %s" CRLF,
                                originalRecipient.get());
        PUSH_N_FREE_STRING(tmpBuffer);
    }

    compUtils = do_GetService(NS_MSGCOMPUTILS_CONTRACTID, &rv);
    if (compUtils)
      compUtils->GetMsgMimeConformToStandard(&conformToStandard);

    convbuf = nsMsgI18NEncodeMimePartIIStr(
        m_email.get(), PR_TRUE, m_charset.get(), 0,
        conformToStandard);
    tmpBuffer = PR_smprintf("Final-Recipient: rfc822;%s" CRLF, convbuf ?
                            convbuf : m_email.get()); 
    PUSH_N_FREE_STRING(tmpBuffer);

    PR_Free (convbuf);

    if (*m_messageId.get() == '<')
        tmpBuffer = PR_smprintf("Original-Message-ID: %s" CRLF, m_messageId.get());
    else
        tmpBuffer = PR_smprintf("Original-Message-ID: <%s>" CRLF, m_messageId.get());
    PUSH_N_FREE_STRING(tmpBuffer);
    
    tmpBuffer = PR_smprintf("Disposition: %s/%s; %s" CRLF CRLF,
                            (m_autoAction ? "automatic-action" :
                             "manual-action"),
                            (m_autoSend ? "MDN-sent-automatically" :
                             "MDN-sent-manually"), 
                            DispositionTypes[(int) m_disposeType]);
    PUSH_N_FREE_STRING(tmpBuffer);
  
    return rv;
}

nsresult nsMsgMdnGenerator::CreateThirdPart()
{
    DEBUG_MDN("nsMsgMdnGenerator::CreateThirdPart");
    char *tmpBuffer = nsnull;
    nsresult rv = NS_OK;

    tmpBuffer = PR_smprintf("--%s" CRLF, m_mimeSeparator.get());
    PUSH_N_FREE_STRING(tmpBuffer);

    tmpBuffer = PR_smprintf("%s" CRLF, "Content-Type: text/rfc822-headers; name=\042MDNPart3.txt\042");
    PUSH_N_FREE_STRING(tmpBuffer);

    tmpBuffer = PR_smprintf("%s" CRLF, "Content-Transfer-Encoding: 7bit");
    PUSH_N_FREE_STRING(tmpBuffer);

    tmpBuffer = PR_smprintf("%s" CRLF CRLF, "Content-Disposition: inline");
    PUSH_N_FREE_STRING(tmpBuffer);
   
    rv = OutputAllHeaders();

    if (NS_FAILED(rv)) 
        return rv;

    rv = WriteString(CRLF);
    if (NS_FAILED(rv)) 
        return rv;

    tmpBuffer = PR_smprintf("--%s--" CRLF, m_mimeSeparator.get());
    PUSH_N_FREE_STRING(tmpBuffer);

    return rv;
}


nsresult nsMsgMdnGenerator::OutputAllHeaders()
{ 
    DEBUG_MDN("nsMsgMdnGenerator::OutputAllHeaders");
    nsXPIDLCString all_headers;
    PRInt32 all_headers_size = 0;
    nsresult rv = NS_OK;
    
    rv = m_headers->GetAllHeaders(getter_Copies(all_headers));
    if (NS_FAILED(rv)) 
        return rv;
    all_headers_size = all_headers.Length();
    char *buf = (char *) all_headers.get(), 
        *buf_end = (char *) all_headers.get()+all_headers_size;
    char *start = buf, *end = buf;
    PRInt32 count = 0;
  
    while (buf < buf_end)
    {
        switch (*buf)
        {
        case 0:
            if (*(buf+1) == nsCRT::LF)
            {
                // *buf = nsCRT::CR;
                end = buf;
            }
            else if (*(buf+1) == 0)
            {
                // the case of message id
                *buf = '>';
            }
            break;
        case nsCRT::CR:
            end = buf;
            *buf = 0;
            break;
        case nsCRT::LF:
            if (buf > start && *(buf-1) == 0)
            {
                start = buf + 1;
                end = start;
            }
            else
            {
                end = buf;
            }
            *buf = 0;
            break;
        default:
            break;
        }
        buf++;
    
        if (end > start && *end == 0)
        {
            // strip out private X-Mozilla-Status header & X-Mozilla-Draft-Info && envelope header
            if (!PL_strncasecmp(start, X_MOZILLA_STATUS, X_MOZILLA_STATUS_LEN)
                || !PL_strncasecmp(start, X_MOZILLA_DRAFT_INFO, X_MOZILLA_DRAFT_INFO_LEN)
                || !PL_strncasecmp(start, "From ", 5))
            {
                while ( end < buf_end && 
                        (*end == nsCRT::LF || *end == nsCRT::CR || *end == 0))
                    end++;
                start = end;
            }
            else
            {
                NS_ASSERTION (*end == 0, "content of end should be null");
                rv = WriteString(start);
                if (NS_FAILED(rv)) 
                    return rv;
                rv = WriteString(CRLF);
                while ( end < buf_end && 
                        (*end == nsCRT::LF || *end == nsCRT::CR || *end == 0))
                    end++;
                start = end;
            }
            buf = start;
        }
    }
    return count;
}

nsresult nsMsgMdnGenerator::SendMdnMsg()
{
    DEBUG_MDN("nsMsgMdnGenerator::SendMdnMsg");
    nsresult rv;
    nsCOMPtr<nsISmtpService> smtpService = do_GetService(NS_SMTPSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIRequest> aRequest;
    smtpService->SendMailMessage(m_fileSpec, m_dntRrt, m_identity,
                                     nsnull, this, nsnull, nsnull, nsnull,
                                     getter_AddRefs(aRequest));
    
    return NS_OK;
}

nsresult nsMsgMdnGenerator::WriteString( const char *str )
{
  NS_ENSURE_ARG (str);
  PRUint32 len = strlen(str);
  PRUint32 wLen = 0;
  
  return m_outputStream->Write(str, len, &wLen);
}

nsresult nsMsgMdnGenerator::InitAndProcess()
{
    DEBUG_MDN("nsMsgMdnGenerator::InitAndProcess");
    nsresult rv = m_folder->GetServer(getter_AddRefs(m_server));
    nsCOMPtr<nsIMsgAccountManager> accountManager = 
        do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    if (accountManager && m_server)
    {
        if (!m_identity)
        {
          // check if this is a message delivered to the global inbox,
          // in which case we find the originating account's identity.
          nsXPIDLCString accountKey;
          m_headers->ExtractHeader(HEADER_X_MOZILLA_ACCOUNT_KEY, PR_FALSE,
                               getter_Copies(accountKey));
          nsCOMPtr <nsIMsgAccount> account;
          if (!accountKey.IsEmpty())
            accountManager->GetAccount(accountKey, getter_AddRefs(account));
          if (account)
            account->GetIncomingServer(getter_AddRefs(m_server));
          
          if (m_server)
            rv = accountManager->GetFirstIdentityForServer(m_server, getter_AddRefs(m_identity));
        }
        NS_ENSURE_SUCCESS(rv,rv);

        if (m_identity)
        {
            PRBool useCustomPrefs = PR_FALSE;
            m_identity->GetBoolAttribute("use_custom_prefs", &useCustomPrefs);
            if (useCustomPrefs)
            {
                PRBool bVal = PR_FALSE;
                m_server->GetBoolValue("mdn_report_enabled", &bVal);
                m_mdnEnabled = bVal;
                m_server->GetIntValue("mdn_not_in_to_cc", &m_notInToCcOp);
                m_server->GetIntValue("mdn_outside_domain",
                                      &m_outsideDomainOp);
                m_server->GetIntValue("mdn_other", &m_otherOp);
            }
            else
            {
                PRBool bVal = PR_FALSE;

                nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
                if (NS_FAILED(rv))
                    return rv;

                if(prefBranch)
                {
                    prefBranch->GetBoolPref("mail.mdn.report.enabled",
                                           &bVal);
                    m_mdnEnabled = bVal;
                    prefBranch->GetIntPref("mail.mdn.report.not_in_to_cc",
                                           &m_notInToCcOp);
                    prefBranch->GetIntPref("mail.mdn.report.outside_domain",
                                           &m_outsideDomainOp);
                    prefBranch->GetIntPref("mail.mdn.report.other",
                                           &m_otherOp);
                }
            }
        }
    }

    rv = m_folder->GetCharset(getter_Copies(m_charset));

    if (m_mdnEnabled)
    {
        m_headers->ExtractHeader(HEADER_DISPOSITION_NOTIFICATION_TO, PR_FALSE,
                                 getter_Copies(m_dntRrt));
        if (!m_dntRrt)
            m_headers->ExtractHeader(HEADER_RETURN_RECEIPT_TO, PR_FALSE,
                                     getter_Copies(m_dntRrt));
        if (m_dntRrt && ProcessSendMode() && ValidateReturnPath())
            rv = CreateMdnMsg();
    }
    return NS_OK;
}

NS_IMETHODIMP nsMsgMdnGenerator::Process(EDisposeType type, 
                                         nsIMsgWindow *aWindow,
                                         nsIMsgFolder *folder,
                                         nsMsgKey key,
                                         nsIMimeHeaders *headers,
                                         PRBool autoAction)
{
    DEBUG_MDN("nsMsgMdnGenerator::Process");
    NS_ENSURE_ARG_POINTER(folder);
    NS_ENSURE_ARG_POINTER(headers);
    NS_ENSURE_ARG_POINTER(aWindow);
    NS_ENSURE_TRUE(key != nsMsgKey_None, NS_ERROR_INVALID_ARG);
    m_disposeType = type;
    m_autoAction = autoAction;
    m_window = aWindow;
    m_folder = folder;
    m_headers = headers;

    nsresult rv = StoreMDNSentFlag(folder, key);
    NS_ASSERTION(NS_SUCCEEDED(rv), "StoreMDNSentFlag failed");

    rv = InitAndProcess();
    NS_ASSERTION(NS_SUCCEEDED(rv), "InitAndProcess failed");
    return NS_OK;
}

NS_IMETHODIMP nsMsgMdnGenerator::OnStartRunningUrl(nsIURI *url)
{
    DEBUG_MDN("nsMsgMdnGenerator::OnStartRunningUrl");
    return NS_OK;
}

NS_IMETHODIMP nsMsgMdnGenerator::OnStopRunningUrl(nsIURI *url, 
                                                  nsresult aExitCode)
{
    DEBUG_MDN("nsMsgMdnGenerator::OnStopRunningUrl");
    m_fileSpec->Delete(PR_FALSE);
    return NS_OK;
}
