/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsCRT.h"
#include "nsMsgCompFields.h"
#include "nsMsgCompFieldsFact.h"
#include "nsIPref.h"
#include "nsMsgI18N.h"
#include "nsMsgComposeStringBundle.h"
#include "nsMsgRecipientArray.h"
#include "nsIMsgHeaderParser.h"
#include "nsMsgCompUtils.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kHeaderParserCID, NS_MSGHEADERPARSER_CID);

/* this function will be used by the factory to generate an Message Compose Fields Object....*/
nsresult NS_NewMsgCompFields(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMsgCompFields* pCompFields = new nsMsgCompFields();
		if (pCompFields)
			return pCompFields->QueryInterface(aIID, aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_THREADSAFE_ISUPPORTS(nsMsgCompFields, NS_GET_IID(nsIMsgCompFields));

nsMsgCompFields::nsMsgCompFields()
{
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 

	PRInt16 i;
	PRBool bReturnReceiptOn = PR_FALSE;

	for (i = 0; i < MAX_HEADERS; i ++)
		m_headers[i] = NULL;
	m_body = NULL;
	m_forwardurl = NULL;
	m_numforward = 0;
	m_maxforward = 0;
	for (i = 0; i < MSG_LAST_BOOL_HEADER_MASK; i ++)
		m_boolHeaders[i] = PR_FALSE;
	m_force_plain_text = PR_FALSE;
	m_multipart_alt = PR_FALSE;
	m_receiptType = 0;

  if (NS_SUCCEEDED(rv) && prefs) 
  {
    prefs->GetBoolPref("mail.request.return_receipt_on", &bReturnReceiptOn);
	  prefs->GetIntPref("mail.request.return_receipt", &m_receiptType);
  }
	SetReturnReceipt (bReturnReceiptOn);
	m_internalCharSet.AssignWithConversion(msgCompHeaderInternalCharset());

	NS_INIT_REFCNT();
}


nsMsgCompFields::~nsMsgCompFields()
{
	PRInt16 i;
	for (i = 0; i < MAX_HEADERS; i ++)
		PR_FREEIF(m_headers[i]);

    PR_FREEIF(m_body);

	for (i = 0; i < m_numforward; i++)
		delete [] m_forwardurl[i];

	delete [] m_forwardurl;
}


nsresult nsMsgCompFields::Copy(nsIMsgCompFields* pMsgCompFields)
{
	nsMsgCompFields * pFields = (nsMsgCompFields*)pMsgCompFields;

	PRInt16 i;
	for (i = 0; i < MAX_HEADERS; i ++) {
		if (pFields->m_headers[i])
			m_headers[i] = nsCRT::strdup(pFields->m_headers[i]);
	}
    if (pFields->m_body)
		m_body = nsCRT::strdup(pFields->m_body);

	for (i = 0; i < pFields->m_numforward; i++)
		AddForwardURL(pFields->m_forwardurl[i]);

	for (i = 0; i < MSG_LAST_BOOL_HEADER_MASK; i ++)
        m_boolHeaders[i] = pFields->m_boolHeaders[i];

	m_force_plain_text = pFields->m_force_plain_text;
	m_multipart_alt = pFields->m_multipart_alt;
	m_receiptType = pFields->m_receiptType;
	m_internalCharSet = pFields->m_internalCharSet;
    m_draftID = pFields->m_draftID;

	return NS_OK;
}



nsresult nsMsgCompFields::SetAsciiHeader(PRInt32 header, const char *value)
{
  int status = 0;
	int i = DecodeHeader(header);
	if (i >= 0) {
		char* old = m_headers[i]; /* Done with careful paranoia, in case the
								     value given is the old value (or worse,
								     a substring of the old value, as does
								     happen here and there.) */
		if (value != old) {
			if (value) {
				m_headers[i] = nsCRT::strdup(value);
				if (!m_headers[i]) 
				   status = NS_ERROR_OUT_OF_MEMORY;
			} else 
				m_headers[i] = NULL;
			PR_FREEIF(old);
		}
	}

	return status;
}

const char* nsMsgCompFields::GetHeader(PRInt32 header)
{
    int i = DecodeHeader(header);
    if (i >= 0) {
		return m_headers[i] ? m_headers[i] : "";
    }
    return NULL;
}

nsresult nsMsgCompFields::SetHeader(PRInt32 header, const PRUnichar *value)
{
	char* cString;
	ConvertFromUnicode(m_internalCharSet, nsAutoString(value), &cString);
	nsresult rv = SetAsciiHeader(header, cString);
	PR_Free(cString);
	
	return rv;
}

nsresult nsMsgCompFields::GetHeader(PRInt32 header, PRUnichar **_retval)
{
	nsString unicodeStr;
	const char* cString = GetHeader(header);
	ConvertToUnicode(m_internalCharSet, cString, unicodeStr);
	*_retval = unicodeStr.ToNewUnicode();
	return NS_OK;
}

nsresult nsMsgCompFields::SetBoolHeader(PRInt32 header, PRBool bValue)
{
	NS_ASSERTION ((int) header >= (int) MSG_RETURN_RECEIPT_BOOL_HEADER_MASK &&
			   (int) header < (int) MSG_LAST_BOOL_HEADER_MASK, "invalid header index");

	if ( (int) header < (int) MSG_RETURN_RECEIPT_BOOL_HEADER_MASK ||
		 (int) header >= (int) MSG_LAST_BOOL_HEADER_MASK )
		 return NS_ERROR_FAILURE;

	m_boolHeaders[header] = bValue;

	return NS_OK;
}

nsresult nsMsgCompFields::GetBoolHeader(PRInt32 header, PRBool *_retval)
{
	NS_PRECONDITION(nsnull != _retval, "nsnull ptr");

	*_retval = GetBoolHeader(header);
	return NS_OK;
}

PRBool nsMsgCompFields::GetBoolHeader(PRInt32 header)
{
	NS_ASSERTION ((int) header >= (int) MSG_RETURN_RECEIPT_BOOL_HEADER_MASK &&
			   (int) header < (int) MSG_LAST_BOOL_HEADER_MASK, "invalid header index");

	if ( (int) header < (int) MSG_RETURN_RECEIPT_BOOL_HEADER_MASK ||
		 (int) header >= (int) MSG_LAST_BOOL_HEADER_MASK )
		 return PR_FALSE;

	return m_boolHeaders[header];
}

nsresult nsMsgCompFields::SetFrom(const PRUnichar *value)
{
	return SetHeader(MSG_FROM_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetFrom(PRUnichar **_retval)
{
	return GetHeader(MSG_FROM_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetReplyTo(const PRUnichar *value)
{
	return SetHeader(MSG_REPLY_TO_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetReplyTo(PRUnichar **_retval)
{
	return GetHeader(MSG_REPLY_TO_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetTo(const PRUnichar *value)
{
	return SetHeader(MSG_TO_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetTo(PRUnichar **_retval)
{
	return GetHeader(MSG_TO_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetCc(const PRUnichar *value)
{
	return SetHeader(MSG_CC_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetCc(PRUnichar **_retval)
{
	return GetHeader(MSG_CC_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetBcc(const PRUnichar *value)
{
	return SetHeader(MSG_BCC_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetBcc(PRUnichar **_retval)
{
	return GetHeader(MSG_BCC_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetFcc(const PRUnichar *value)
{
	return SetHeader(MSG_FCC_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetFcc(PRUnichar **_retval)
{
	return GetHeader(MSG_FCC_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetFcc2(const PRUnichar *value)
{
	return SetHeader(MSG_FCC2_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetFcc2(PRUnichar **_retval)
{
	return GetHeader(MSG_FCC2_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetNewsFcc(const PRUnichar *value)
{
	return SetHeader(MSG_NEWS_FCC_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetNewsFcc(PRUnichar **_retval)
{
	return GetHeader(MSG_NEWS_FCC_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetNewsBcc(const PRUnichar *value)
{
	return SetHeader(MSG_NEWS_BCC_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetNewsBcc(PRUnichar **_retval)
{
	return GetHeader(MSG_NEWS_BCC_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetNewsgroups(const PRUnichar *value)
{
	return SetHeader(MSG_NEWSGROUPS_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetNewsgroups(PRUnichar **_retval)
{
	return GetHeader(MSG_NEWSGROUPS_HEADER_MASK, _retval);
}

NS_IMETHODIMP nsMsgCompFields::SetNewshost(const PRUnichar *value)
{
	return SetHeader(MSG_NEWSPOSTURL_HEADER_MASK, value);
}

NS_IMETHODIMP nsMsgCompFields::GetNewshost(PRUnichar **_retval)
{
	return GetHeader(MSG_NEWSPOSTURL_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetFollowupTo(const PRUnichar *value)
{
	return SetHeader(MSG_FOLLOWUP_TO_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetFollowupTo(PRUnichar **_retval)
{
	return GetHeader(MSG_FOLLOWUP_TO_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetSubject(const PRUnichar *value)
{
	return SetHeader(MSG_SUBJECT_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetSubject(PRUnichar **_retval)
{
	return GetHeader(MSG_SUBJECT_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetAttachments(const PRUnichar *value)
{
	return SetHeader(MSG_ATTACHMENTS_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetAttachments(PRUnichar **_retval)
{
	return GetHeader(MSG_ATTACHMENTS_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetOrganization(const PRUnichar *value)
{
	return SetHeader(MSG_ORGANIZATION_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetOrganization(PRUnichar **_retval)
{
	return GetHeader(MSG_ORGANIZATION_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetReferences(const PRUnichar *value)
{
	return SetHeader(MSG_REFERENCES_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetReferences(PRUnichar **_retval)
{
	return GetHeader(MSG_REFERENCES_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetOtherRandomHeaders(const PRUnichar *value)
{
	return SetHeader(MSG_OTHERRANDOMHEADERS_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetOtherRandomHeaders(PRUnichar **_retval)
{
	return GetHeader(MSG_OTHERRANDOMHEADERS_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetNewspostUrl(const PRUnichar *value)
{
	return SetHeader(MSG_NEWSPOSTURL_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetNewspostUrl(PRUnichar **_retval)
{
	return GetHeader(MSG_NEWSPOSTURL_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetDefaultBody(const PRUnichar *value)
{
	return SetHeader(MSG_DEFAULTBODY_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetDefaultBody(PRUnichar **_retval)
{
	return GetHeader(MSG_DEFAULTBODY_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetPriority(const PRUnichar *value)
{
	return SetHeader(MSG_PRIORITY_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetPriority(PRUnichar **_retval)
{
	return GetHeader(MSG_PRIORITY_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetMessageEncoding(const PRUnichar *value)
{
	return SetHeader(MSG_MESSAGE_ENCODING_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetMessageEncoding(PRUnichar **_retval)
{
	return GetHeader(MSG_MESSAGE_ENCODING_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetCharacterSet(const PRUnichar *value)
{
	return SetHeader(MSG_CHARACTER_SET_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetCharacterSet(PRUnichar **_retval)
{
	return GetHeader(MSG_CHARACTER_SET_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetMessageId(const PRUnichar *value)
{
	return SetHeader(MSG_MESSAGE_ID_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetMessageId(PRUnichar **_retval)
{
	return GetHeader(MSG_MESSAGE_ID_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetHTMLPart(const PRUnichar *value)
{
	return SetHeader(MSG_HTML_PART_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetHTMLPart(PRUnichar **_retval)
{
	return GetHeader(MSG_HTML_PART_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetTemplateName(const PRUnichar *value)
{
	return SetHeader(MSG_X_TEMPLATE_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetTemplateName(PRUnichar **_retval)
{
	return GetHeader(MSG_X_TEMPLATE_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetDraftId(const PRUnichar *value)
{
    m_draftID = value;
    return NS_OK;
}

nsresult nsMsgCompFields::GetDraftId(PRUnichar **_retval)
{
    if (_retval)
    {
        *_retval = m_draftID.ToNewUnicode();
        return NS_OK;
    }
    return NS_ERROR_NULL_POINTER;
}

nsresult nsMsgCompFields::SetReturnReceipt(PRBool value)
{
	return SetBoolHeader(MSG_RETURN_RECEIPT_BOOL_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetReturnReceipt(PRBool *_retval)
{
	return GetBoolHeader(MSG_RETURN_RECEIPT_BOOL_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetAttachVCard(PRBool value)
{
	return SetBoolHeader(MSG_ATTACH_VCARD_BOOL_HEADER_MASK, value);
}

nsresult nsMsgCompFields::GetAttachVCard(PRBool *_retval)
{
	return GetBoolHeader(MSG_ATTACH_VCARD_BOOL_HEADER_MASK, _retval);
}

nsresult   
nsMsgCompFields::SetUUEncodeAttachments(PRBool value)
{
	return SetBoolHeader(MSG_UUENCODE_BINARY_BOOL_HEADER_MASK, value);
}

nsresult   
nsMsgCompFields::GetUUEncodeAttachments(PRBool *_retval)
{
	return GetBoolHeader(MSG_UUENCODE_BINARY_BOOL_HEADER_MASK, _retval);
}

nsresult   
nsMsgCompFields::SetTheForcePlainText(PRBool value)
{
  m_force_plain_text = value;
  return NS_OK;  
}

nsresult   
nsMsgCompFields::GetTheForcePlainText(PRBool *_retval)
{
  *_retval = m_force_plain_text;
  return NS_OK;    
}

nsresult   
nsMsgCompFields::SetUseMultipartAlternativeFlag(PRBool value)
{
  m_multipart_alt = value;
  return NS_OK;    
}

nsresult   
nsMsgCompFields::GetUseMultipartAlternativeFlag(PRBool *_retval)
{
  *_retval = m_multipart_alt;
  return NS_OK;
}


nsresult nsMsgCompFields::SetBody(const PRUnichar *value)
{
	long retval = 0;

    PR_FREEIF(m_body);
    if (value) {
		char* cString;
		ConvertFromUnicode(m_internalCharSet, nsAutoString(value), &cString);
		m_body = cString;
		if (!m_body)
			retval = NS_ERROR_OUT_OF_MEMORY;
    }
    return retval;
}

nsresult nsMsgCompFields::GetBody(PRUnichar **_retval)
{
	NS_PRECONDITION(nsnull != _retval, "nsnull ptr");

	nsString unicodeStr;
	const char* cString = GetBody();
	ConvertToUnicode(m_internalCharSet, cString, unicodeStr);
	*_retval = unicodeStr.ToNewUnicode();

	return NS_OK;
}

nsresult nsMsgCompFields::SetBody(const char *value)
{
	long retval = 0;

    PR_FREEIF(m_body);
    if (value) {
		m_body = nsCRT::strdup(value);
		if (!m_body)
			retval = NS_ERROR_OUT_OF_MEMORY;
    }
    return retval;
}

const char* nsMsgCompFields::GetBody()
{
    return m_body ? m_body : "";
}


nsresult nsMsgCompFields::AppendBody(char* value)
{
    if (!value || !*value)
		return 0;
 
	if (!m_body) {
		return SetBody(value);
    } else {
		char* tmp = (char*) PR_Malloc(nsCRT::strlen(m_body) + nsCRT::strlen(value) + 1);
		if (tmp) {
			tmp = nsCRT::strdup(m_body);
			PL_strcat(tmp, value);
			PR_Free(m_body);
			m_body = tmp;
		} else {
			return NS_ERROR_OUT_OF_MEMORY;
		}
    }
    return 0;
}
    
nsresult nsMsgCompFields::DecodeHeader(MSG_HEADER_SET header)
{
    int result;
 
	switch(header) {
    case MSG_FROM_HEADER_MASK				: result = 0;		break;
    case MSG_REPLY_TO_HEADER_MASK			: result = 1;		break;
    case MSG_TO_HEADER_MASK					: result = 2;		break;
    case MSG_CC_HEADER_MASK					: result = 3;		break;
    case MSG_BCC_HEADER_MASK				: result = 4;		break;
    case MSG_FCC_HEADER_MASK				: result = 5;		break;
    case MSG_NEWSGROUPS_HEADER_MASK			: result = 6;		break;
    case MSG_FOLLOWUP_TO_HEADER_MASK		: result = 7;		break;
    case MSG_SUBJECT_HEADER_MASK			: result = 8;		break;
    case MSG_ATTACHMENTS_HEADER_MASK		: result = 9;		break;
    case MSG_ORGANIZATION_HEADER_MASK		: result = 10;		break;
    case MSG_REFERENCES_HEADER_MASK			: result = 11;		break;
    case MSG_OTHERRANDOMHEADERS_HEADER_MASK	: result = 12;		break;
    case MSG_NEWSPOSTURL_HEADER_MASK		: result = 13;		break;
    case MSG_PRIORITY_HEADER_MASK			: result = 14;		break;
	case MSG_NEWS_FCC_HEADER_MASK			: result = 15;		break;
	case MSG_MESSAGE_ENCODING_HEADER_MASK	: result = 16;		break;
	case MSG_CHARACTER_SET_HEADER_MASK		: result = 17;		break;
	case MSG_MESSAGE_ID_HEADER_MASK			: result = 18;		break;
	case MSG_NEWS_BCC_HEADER_MASK			: result = 19;		break;
	case MSG_HTML_PART_HEADER_MASK			: result = 20;		break;
    case MSG_DEFAULTBODY_HEADER_MASK		: result = 21;		break;
	case MSG_X_TEMPLATE_HEADER_MASK			: result = 22;		break;
	case MSG_FCC2_HEADER_MASK			: result = 23;		break;
    default:
		NS_ASSERTION(0, "invalid header index");
		result = -1;
		break;
    }

    NS_ASSERTION(result < (int)(sizeof(m_headers) / sizeof(char*)), "wrong result, review the code!");
    return result;
}

nsresult nsMsgCompFields::AddForwardURL(const char* url)
{
	NS_ASSERTION(url && *url, "empty url");
	if (!url || !*url)
		return NS_ERROR_NULL_POINTER;

	if (m_numforward >= m_maxforward) {
		m_maxforward += 10;
		char** tmp = new char* [m_maxforward];
		if (!tmp)
			return NS_ERROR_OUT_OF_MEMORY;
		for (PRInt32 i=0 ; i<m_numforward ; i++) {
			tmp[i] = m_forwardurl[i];
		}
		delete [] m_forwardurl;
		m_forwardurl = tmp;
	}
	m_forwardurl[m_numforward] = new char[nsCRT::strlen(url) + 1];
	if (!m_forwardurl[m_numforward])
		return NS_ERROR_OUT_OF_MEMORY;
	m_forwardurl[m_numforward] = nsCRT::strdup(url);
	m_numforward++;
	return 0;
}

PRInt32 nsMsgCompFields::GetNumForwardURL()
{
	return m_numforward;
}

const char* nsMsgCompFields::GetForwardURL(PRInt32 which)
{
	NS_ASSERTION(which >= 0 && which < m_numforward, "parameter out of range");
	if (which >= 0 && which < m_numforward) {
		return m_forwardurl[which];
	}
	return NULL;
}

nsresult nsMsgCompFields::SplitRecipients(const PRUnichar *recipients, PRBool emailAddressOnly, nsIMsgRecipientArray **_retval)
{
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
			    nsCAutoString temp; temp.AssignWithConversion(recipients);
				  recipientsStr = PL_strdup(temp);
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

					rv = pAddrArray->AppendString(aRecipient.GetUnicode(), &aBool);
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

nsresult nsMsgCompFields::SplitRecipientsEx(const PRUnichar *recipients, nsIMsgRecipientArray ** fullAddrsArray, nsIMsgRecipientArray ** emailsArray)
{
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
			  nsCAutoString temp; temp.AssignWithConversion(recipients);
				recipientsStr = PL_strdup(temp);
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
              
					  rv = pAddrsArray->AppendString(aRecipient.GetUnicode(), &aBool);
					  if (NS_FAILED(rv))
						  return rv;
					}

          if (pEmailsArray)
          {
            rv = ConvertToUnicode(NS_ConvertASCIItoUCS2(msgCompHeaderInternalCharset()), pAddresses, aRecipient);
            if (NS_FAILED(rv))
              return rv;
					  rv = pEmailsArray->AppendString(aRecipient.GetUnicode(), &aBool);
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

nsresult nsMsgCompFields::ConvertBodyToPlainText()
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
				rv = SetBody(body.GetUnicode());
		}
	}
	return rv;
}
