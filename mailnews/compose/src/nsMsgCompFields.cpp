/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMsgCompFields.h"
#include "nsCRT.h"
#include "nsIPref.h"
#include "nsMsgI18N.h"
#include "nsMsgComposeStringBundle.h"
#include "nsMsgRecipientArray.h"
#include "nsIMsgHeaderParser.h"
#include "nsMsgCompUtils.h"
#include "prmem.h"
#include "nsIFileChannel.h"
#include "nsReadableUtils.h"

static NS_DEFINE_CID(kHeaderParserCID, NS_MSGHEADERPARSER_CID);

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_THREADSAFE_ISUPPORTS1(nsMsgCompFields, nsIMsgCompFields)

nsMsgCompFields::nsMsgCompFields()
{
  PRInt16 i;
  for (i = 0; i < MSG_MAX_HEADERS; i ++)
    m_headers[i] = nsnull;

  m_body = nsnull;

  NS_NewISupportsArray(getter_AddRefs(m_attachments));

  m_attachVCard = PR_FALSE;
  m_forcePlainText = PR_FALSE;
  m_useMultipartAlternative = PR_FALSE;
  m_uuEncodeAttachments = PR_FALSE;
  m_returnReceipt = PR_FALSE;
  m_receiptType = 0;

  nsCOMPtr<nsIPref> prefs (do_GetService(NS_PREF_CONTRACTID));
  if (prefs) 
  {
    prefs->GetBoolPref("mail.request.return_receipt_on", &m_returnReceipt);
    prefs->GetIntPref("mail.request.return_receipt", &m_receiptType);
  }
  m_internalCharSet.AssignWithConversion(msgCompHeaderInternalCharset());

  // Get the default charset from pref, use this as a mail charset.
  char * default_mail_charset = nsMsgI18NGetDefaultMailCharset();
  if (default_mail_charset)
  {
    SetCharacterSet(default_mail_charset);
    PR_Free(default_mail_charset);
  }

  NS_INIT_REFCNT();
}

nsMsgCompFields::~nsMsgCompFields()
{
  PRInt16 i;
  for (i = 0; i < MSG_MAX_HEADERS; i ++)
    PR_FREEIF(m_headers[i]);

  PR_FREEIF(m_body);
}

nsresult nsMsgCompFields::SetAsciiHeader(MsgHeaderID header, const char *value)
{
  NS_VERIFY (header >= 0 && header < MSG_MAX_HEADERS, "Invalid message header index!");

  int rv = NS_OK;
  char* old = m_headers[header]; /* Done with careful paranoia, in case the
                                    value given is the old value (or worse,
                                    a substring of the old value, as does
                                    happen here and there.)
                                  */
  if (value != old)
  {
    if (value)
    {
        m_headers[header] = nsCRT::strdup(value);
        if (!m_headers[header]) 
           rv = NS_ERROR_OUT_OF_MEMORY;
    }
    else 
      m_headers[header] = nsnull;

    PR_FREEIF(old);
  }

  return rv;
}

const char* nsMsgCompFields::GetAsciiHeader(MsgHeaderID header)
{
  NS_VERIFY (header >= 0 && header < MSG_MAX_HEADERS, "Invalid message header index!");
  return m_headers[header] ? m_headers[header] : "";
}

nsresult nsMsgCompFields::SetUnicodeHeader(MsgHeaderID header, const PRUnichar *value)
{
  char* cString;
  ConvertFromUnicode(m_internalCharSet, nsAutoString(value), &cString);
  nsresult rv = SetAsciiHeader(header, cString);
  PR_Free(cString);
  
  return rv;
}

nsresult nsMsgCompFields::GetUnicodeHeader(MsgHeaderID header, PRUnichar **_retval)
{
  nsString unicodeStr;
  ConvertToUnicode(m_internalCharSet, GetAsciiHeader(header), unicodeStr);
  *_retval = ToNewUnicode(unicodeStr);
  return NS_OK;
}

NS_IMETHODIMP nsMsgCompFields::SetFrom(const PRUnichar *value)
{
  return SetUnicodeHeader(MSG_FROM_HEADER_ID, value);
}


NS_IMETHODIMP nsMsgCompFields::GetFrom(PRUnichar **_retval)
{
  return GetUnicodeHeader(MSG_FROM_HEADER_ID, _retval);
}

NS_IMETHODIMP nsMsgCompFields::SetReplyTo(const PRUnichar *value)
{
  return SetUnicodeHeader(MSG_REPLY_TO_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetReplyTo(PRUnichar **_retval)
{
  return GetUnicodeHeader(MSG_REPLY_TO_HEADER_ID, _retval);
}

NS_IMETHODIMP nsMsgCompFields::SetTo(const PRUnichar *value)
{
  return SetUnicodeHeader(MSG_TO_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetTo(PRUnichar **_retval)
{
  return GetUnicodeHeader(MSG_TO_HEADER_ID, _retval);
}

NS_IMETHODIMP nsMsgCompFields::SetCc(const PRUnichar *value)
{
  return SetUnicodeHeader(MSG_CC_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetCc(PRUnichar **_retval)
{
  return GetUnicodeHeader(MSG_CC_HEADER_ID, _retval);
}

NS_IMETHODIMP nsMsgCompFields::SetBcc(const PRUnichar *value)
{
  return SetUnicodeHeader(MSG_BCC_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetBcc(PRUnichar **_retval)
{
  return GetUnicodeHeader(MSG_BCC_HEADER_ID, _retval);
}

NS_IMETHODIMP nsMsgCompFields::SetFcc(const PRUnichar *value)
{
  return SetUnicodeHeader(MSG_FCC_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetFcc(PRUnichar **_retval)
{
  return GetUnicodeHeader(MSG_FCC_HEADER_ID, _retval);
}

NS_IMETHODIMP nsMsgCompFields::SetFcc2(const PRUnichar *value)
{
  return SetUnicodeHeader(MSG_FCC2_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetFcc2(PRUnichar **_retval)
{
  return GetUnicodeHeader(MSG_FCC2_HEADER_ID, _retval);
}

NS_IMETHODIMP nsMsgCompFields::SetNewsgroups(const char *value)
{
  return SetAsciiHeader(MSG_NEWSGROUPS_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetNewsgroups(char **_retval)
{
  *_retval = nsCRT::strdup(GetAsciiHeader(MSG_NEWSGROUPS_HEADER_ID));
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgCompFields::SetNewshost(const char *value)
{
  return SetAsciiHeader(MSG_NEWSPOSTURL_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetNewshost(char **_retval)
{
  *_retval = nsCRT::strdup(GetAsciiHeader(MSG_NEWSPOSTURL_HEADER_ID));
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgCompFields::SetFollowupTo(const char *value)
{
  return SetAsciiHeader(MSG_FOLLOWUP_TO_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetFollowupTo(char **_retval)
{
  *_retval = nsCRT::strdup(GetAsciiHeader(MSG_FOLLOWUP_TO_HEADER_ID));
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgCompFields::SetSubject(const PRUnichar *value)
{
  return SetUnicodeHeader(MSG_SUBJECT_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetSubject(PRUnichar **_retval)
{
  return GetUnicodeHeader(MSG_SUBJECT_HEADER_ID, _retval);
}

NS_IMETHODIMP nsMsgCompFields::SetAttachments(const char *value)
{
  NS_ASSERTION(0, "nsMsgCompFields::SetAttachments is not supported anymore, please use nsMsgCompFields::AddAttachment");
  return SetAsciiHeader(MSG_ATTACHMENTS_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::SetTemporaryFiles(const char *value)
{
  NS_ASSERTION(0, "nsMsgCompFields::SetTemporaryFiles is not supported anymore, please use nsMsgCompFields::AddAttachment");
  return SetAsciiHeader(MSG_TEMPORARY_FILES_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetAttachments(char **_retval)
{
  NS_ASSERTION(0, "nsMsgCompFields::GetAttachments is not supported anymore, please use nsMsgCompFields::GetAttachmentsArray");
  *_retval = nsCRT::strdup(GetAsciiHeader(MSG_ATTACHMENTS_HEADER_ID));
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgCompFields::GetTemporaryFiles(char **_retval)
{
  *_retval = nsCRT::strdup(GetAsciiHeader(MSG_TEMPORARY_FILES_HEADER_ID));
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgCompFields::SetOrganization(const PRUnichar *value)
{
  return SetUnicodeHeader(MSG_ORGANIZATION_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetOrganization(PRUnichar **_retval)
{
  return GetUnicodeHeader(MSG_ORGANIZATION_HEADER_ID, _retval);
}

NS_IMETHODIMP nsMsgCompFields::SetReferences(const char *value)
{
  return SetAsciiHeader(MSG_REFERENCES_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetReferences(char **_retval)
{
  *_retval = nsCRT::strdup(GetAsciiHeader(MSG_REFERENCES_HEADER_ID));
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgCompFields::SetOtherRandomHeaders(const PRUnichar *value)
{
  return SetUnicodeHeader(MSG_OTHERRANDOMHEADERS_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetOtherRandomHeaders(PRUnichar **_retval)
{
  return GetUnicodeHeader(MSG_OTHERRANDOMHEADERS_HEADER_ID, _retval);
}

NS_IMETHODIMP nsMsgCompFields::SetNewspostUrl(const char *value)
{
  return SetAsciiHeader(MSG_NEWSPOSTURL_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetNewspostUrl(char **_retval)
{
  *_retval = nsCRT::strdup(GetAsciiHeader(MSG_NEWSPOSTURL_HEADER_ID));
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgCompFields::SetPriority(const char *value)
{
  return SetAsciiHeader(MSG_PRIORITY_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetPriority(char **_retval)
{
  *_retval = nsCRT::strdup(GetAsciiHeader(MSG_PRIORITY_HEADER_ID));
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgCompFields::SetCharacterSet(const char *value)
{
  return SetAsciiHeader(MSG_CHARACTER_SET_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetCharacterSet(char **_retval)
{
  *_retval = nsCRT::strdup(GetAsciiHeader(MSG_CHARACTER_SET_HEADER_ID));
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgCompFields::SetMessageId(const char *value)
{
  return SetAsciiHeader(MSG_MESSAGE_ID_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetMessageId(char **_retval)
{
  *_retval = nsCRT::strdup(GetAsciiHeader(MSG_MESSAGE_ID_HEADER_ID));
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgCompFields::SetTemplateName(const PRUnichar *value)
{
  return SetUnicodeHeader(MSG_X_TEMPLATE_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetTemplateName(PRUnichar **_retval)
{
  return GetUnicodeHeader(MSG_X_TEMPLATE_HEADER_ID, _retval);
}

NS_IMETHODIMP nsMsgCompFields::SetDraftId(const char *value)
{
  return SetAsciiHeader(MSG_DRAFT_ID_HEADER_ID, value);
}

NS_IMETHODIMP nsMsgCompFields::GetDraftId(char **_retval)
{
  *_retval = nsCRT::strdup(GetAsciiHeader(MSG_DRAFT_ID_HEADER_ID));
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgCompFields::SetReturnReceipt(PRBool value)
{
  m_returnReceipt = value;
  return NS_OK;
}

NS_IMETHODIMP nsMsgCompFields::GetReturnReceipt(PRBool *_retval)
{
  *_retval = m_returnReceipt;
  return NS_OK;
}

NS_IMETHODIMP nsMsgCompFields::SetAttachVCard(PRBool value)
{
  m_attachVCard = value;
  return NS_OK;
}

NS_IMETHODIMP nsMsgCompFields::GetAttachVCard(PRBool *_retval)
{
  *_retval = m_attachVCard;
  return NS_OK;
}

NS_IMETHODIMP nsMsgCompFields::SetForcePlainText(PRBool value)
{
  m_forcePlainText = value;
  return NS_OK;
}

NS_IMETHODIMP nsMsgCompFields::GetForcePlainText(PRBool *_retval)
{
  *_retval = m_forcePlainText;
  return NS_OK;
}

NS_IMETHODIMP nsMsgCompFields::SetUseMultipartAlternative(PRBool value)
{
  m_useMultipartAlternative = value;
  return NS_OK;
}

NS_IMETHODIMP nsMsgCompFields::GetUseMultipartAlternative(PRBool *_retval)
{
  *_retval = m_useMultipartAlternative;
  return NS_OK;
}

NS_IMETHODIMP nsMsgCompFields::SetUuEncodeAttachments(PRBool value)
{
  m_uuEncodeAttachments = value;
  return NS_OK;
}

NS_IMETHODIMP nsMsgCompFields::GetUuEncodeAttachments(PRBool *_retval)
{
  *_retval = m_uuEncodeAttachments;
  return NS_OK;
}

NS_IMETHODIMP nsMsgCompFields::SetBody(const PRUnichar *value)
{
  PR_FREEIF(m_body);
  if (value)
  {
    char* cString;
    ConvertFromUnicode(m_internalCharSet, nsAutoString(value), &cString);
    m_body = cString;
    if (!m_body)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgCompFields::GetBody(PRUnichar **_retval)
{
  nsString unicodeStr;
  const char* cString = GetBody();
  ConvertToUnicode(m_internalCharSet, cString, unicodeStr);
  *_retval = ToNewUnicode(unicodeStr);

  return NS_OK;
}

nsresult nsMsgCompFields::SetBody(const char *value)
{
  PR_FREEIF(m_body);
  if (value)
  {
    m_body = nsCRT::strdup(value);
    if (!m_body)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

const char* nsMsgCompFields::GetBody()
{
    return m_body ? m_body : "";
}

/* readonly attribute nsISupportsArray attachmentsArray; */
NS_IMETHODIMP nsMsgCompFields::GetAttachmentsArray(nsISupportsArray * *aAttachmentsArray)
{
  NS_ENSURE_ARG_POINTER(aAttachmentsArray);
  *aAttachmentsArray = m_attachments;
  NS_IF_ADDREF(*aAttachmentsArray);
  return NS_OK;
}

/* void addAttachment (in nsIMsgAttachment attachment); */
NS_IMETHODIMP nsMsgCompFields::AddAttachment(nsIMsgAttachment *attachment)
{
  PRUint32 i;
  PRUint32 attachmentCount = 0;
  m_attachments->Count(&attachmentCount);

  //Don't add twice the same attachment.
  nsCOMPtr<nsIMsgAttachment> element;
  PRBool sameUrl;
  for (i = 0; i < attachmentCount; i ++)
  {
    m_attachments->QueryElementAt(i, NS_GET_IID(nsIMsgAttachment), getter_AddRefs(element));
    if (element)
    {
      element->EqualsUrl(attachment, &sameUrl);
      if (sameUrl)
        return NS_OK;
    }
  }

  return m_attachments->InsertElementAt(attachment, attachmentCount);
}

/* void removeAttachment (in nsIMsgAttachment attachment); */
NS_IMETHODIMP nsMsgCompFields::RemoveAttachment(nsIMsgAttachment *attachment)
{
  PRUint32 i;
  PRUint32 attachmentCount = 0;
  m_attachments->Count(&attachmentCount);

  nsCOMPtr<nsIMsgAttachment> element;
  PRBool sameUrl;
  for (i = 0; i < attachmentCount; i ++)
  {
    m_attachments->QueryElementAt(i, NS_GET_IID(nsIMsgAttachment), getter_AddRefs(element));
    if (element)
    {
      element->EqualsUrl(attachment, &sameUrl);
      if (sameUrl)
      {
        m_attachments->DeleteElementAt(i);
        break;
      }
    }
  }

  return NS_OK;
}

/* void removeAttachments (); */
NS_IMETHODIMP nsMsgCompFields::RemoveAttachments()
{
  PRUint32 i;
  PRUint32 attachmentCount = 0;
  m_attachments->Count(&attachmentCount);

  for (i = 0; i < attachmentCount; i ++)
    m_attachments->DeleteElementAt(0);

  return NS_OK;
}


// This method is called during the creation of a new window.
NS_IMETHODIMP nsMsgCompFields::SplitRecipients(const PRUnichar *recipients, PRBool emailAddressOnly, nsIMsgRecipientArray **_retval)
{
  NS_ASSERTION(recipients, "The recipient list is not supposed to be null -Fix the caller!");

  nsresult rv = NS_OK;

	if (! _retval)
		return NS_ERROR_NULL_POINTER;
	*_retval = nsnull;
		
	nsMsgRecipientArray* pAddrArray = new nsMsgRecipientArray;
	if (! pAddrArray)
		return NS_ERROR_OUT_OF_MEMORY;
	
	rv = pAddrArray->QueryInterface(NS_GET_IID(nsIMsgRecipientArray), (void **)_retval);
	if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(kHeaderParserCID);;
		if (parser)
		{
			char * recipientsStr;
			char * names;
			char * addresses;
			PRUint32 numAddresses;

			if (NS_FAILED(ConvertFromUnicode(NS_ConvertASCIItoUCS2(msgCompHeaderInternalCharset()), nsAutoString(recipients), &recipientsStr)))
			  {
				  recipientsStr = ToNewCString(nsDependentString(recipients));
				}
			
			if (! recipientsStr)
				return NS_ERROR_OUT_OF_MEMORY;

			rv= parser->ParseHeaderAddresses(msgCompHeaderInternalCharset(), recipientsStr, &names, &addresses, &numAddresses);
			if (NS_SUCCEEDED(rv))
			{
				PRUint32 i=0;
				char * pNames = names;
				char * pAddresses = addresses;
				char * fullAddress;
				nsString aRecipient;
				PRBool aBool;
				
				for (i = 0; i < numAddresses; i ++)
				{
				    if (!emailAddressOnly)
					    rv = parser->MakeFullAddress(msgCompHeaderInternalCharset(), pNames, pAddresses, &fullAddress);
					if (NS_SUCCEEDED(rv) && !emailAddressOnly)
					{
						rv = ConvertToUnicode(NS_ConvertASCIItoUCS2(msgCompHeaderInternalCharset()), fullAddress, aRecipient);
						PR_FREEIF(fullAddress);
					}
					else
						rv = ConvertToUnicode(NS_ConvertASCIItoUCS2(msgCompHeaderInternalCharset()), pAddresses, aRecipient);
					if (NS_FAILED(rv))
						break;

					rv = pAddrArray->AppendString(aRecipient.get(), &aBool);
					if (NS_FAILED(rv))
						break;
						
					pNames += PL_strlen(pNames) + 1;
					pAddresses += PL_strlen(pAddresses) + 1;
				}
			
				PR_FREEIF(names);
				PR_FREEIF(addresses);
			}
			
			PR_Free(recipientsStr);
		}
		else
			rv = NS_ERROR_FAILURE;
	}
		
	return rv;
}


//This method is called during the sending of message from NS_IMETHODIMP nsMsgCompose::CheckAndPopulateRecipients
nsresult nsMsgCompFields::SplitRecipientsEx(const PRUnichar *recipients, nsIMsgRecipientArray ** fullAddrsArray, nsIMsgRecipientArray ** emailsArray)
{
  NS_ASSERTION(recipients, "The recipient list is not supposed to be null -Fix the caller!");

  nsresult rv = NS_OK;

  nsMsgRecipientArray* pAddrsArray = nsnull;
  if (fullAddrsArray)
  {
    *fullAddrsArray = nsnull;
    pAddrsArray = new nsMsgRecipientArray;
    if (! pAddrsArray)
      return NS_ERROR_OUT_OF_MEMORY;
    rv = pAddrsArray->QueryInterface(NS_GET_IID(nsIMsgRecipientArray), (void **)fullAddrsArray);
    if (NS_FAILED(rv))
      return rv;
  }
  
  nsMsgRecipientArray* pEmailsArray = nsnull;
  if (emailsArray)
  {
    *emailsArray = nsnull;
    pEmailsArray = new nsMsgRecipientArray;
    if (! pEmailsArray)
      return NS_ERROR_OUT_OF_MEMORY;
    rv = pEmailsArray->QueryInterface(NS_GET_IID(nsIMsgRecipientArray), (void **)emailsArray);
    if (NS_FAILED(rv))
      return rv;
  }
	
	if (pAddrsArray || pEmailsArray)
	{
		nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(kHeaderParserCID);;
		if (parser)
		{
			char * recipientsStr;
			char * names;
			char * addresses;
			PRUint32 numAddresses;

			if (NS_FAILED(ConvertFromUnicode(NS_ConvertASCIItoUCS2(msgCompHeaderInternalCharset()), nsAutoString(recipients), &recipientsStr)))
			{
				recipientsStr = ToNewCString(nsDependentString(recipients));
			}
			
			if (! recipientsStr)
				return NS_ERROR_OUT_OF_MEMORY;

			rv= parser->ParseHeaderAddresses(msgCompHeaderInternalCharset(), recipientsStr, &names, &addresses, &numAddresses);
			if (NS_SUCCEEDED(rv))
			{
				PRUint32 i=0;
				char * pNames = names;
				char * pAddresses = addresses;
				char * fullAddress;
				nsString aRecipient;
				PRBool aBool;
				
        for (i = 0; i < numAddresses; i ++)
        {
          if (pAddrsArray)
          {
            rv = parser->MakeFullAddress(msgCompHeaderInternalCharset(), pNames, pAddresses, &fullAddress);
            if (NS_SUCCEEDED(rv))
            {
              rv = ConvertToUnicode(NS_ConvertASCIItoUCS2(msgCompHeaderInternalCharset()), fullAddress, aRecipient);
              PR_FREEIF(fullAddress);
            }
            else
              rv = ConvertToUnicode(NS_ConvertASCIItoUCS2(msgCompHeaderInternalCharset()), pAddresses, aRecipient);
            if (NS_FAILED(rv))
              return rv;
              
            rv = pAddrsArray->AppendString(aRecipient.get(), &aBool);
            if (NS_FAILED(rv))
              return rv;
          }

          if (pEmailsArray)
          {
            rv = ConvertToUnicode(NS_ConvertASCIItoUCS2(msgCompHeaderInternalCharset()), pAddresses, aRecipient);
            if (NS_FAILED(rv))
              return rv;
            rv = pEmailsArray->AppendString(aRecipient.get(), &aBool);
            if (NS_FAILED(rv))
              return rv;
          }

          pNames += PL_strlen(pNames) + 1;
          pAddresses += PL_strlen(pAddresses) + 1;
        }
      
        PR_FREEIF(names);
        PR_FREEIF(addresses);
      }
      
      PR_Free(recipientsStr);
    }
    else
      rv = NS_ERROR_FAILURE;
  }
    
  return rv;
}

NS_IMETHODIMP nsMsgCompFields::ConvertBodyToPlainText()
{
  nsresult rv = NS_OK;
  
  if (m_body && *m_body != 0)
  {
    PRUnichar * ubody;
    rv = GetBody(&ubody);
    if (NS_SUCCEEDED(rv))
    {
      nsString body(ubody);
      PR_Free(ubody);
      rv = ConvertBufToPlainText(body, UseFormatFlowed(GetCharacterSet()));
      if (NS_SUCCEEDED(rv))
        rv = SetBody(body.get());
    }
  }
  return rv;
}
