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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsSmtpUrl.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "nsEscape.h"
#include "nsIMimeConverter.h"
#include "nsMsgMimeCID.h"
#include "nsISupportsObsolete.h"

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

/////////////////////////////////////////////////////////////////////////////////////
// mailto url definition
/////////////////////////////////////////////////////////////////////////////////////
nsMailtoUrl::nsMailtoUrl()
{
  mFormat = nsIMsgCompFormat::Default;
  m_baseURL = do_CreateInstance(kSimpleURICID);
}

nsMailtoUrl::~nsMailtoUrl()
{
}

NS_IMPL_ISUPPORTS2(nsMailtoUrl, nsIMailtoUrl, nsIURI)

nsresult nsMailtoUrl::ParseMailtoUrl(char * searchPart)
{
	char *rest = searchPart;
	// okay, first, free up all of our old search part state.....
	CleanupMailtoState();

	if (rest && *rest == '?')
	{
 		/* start past the '?' */
		rest++;
	}

	if (rest)
	{
    char *token = nsCRT::strtok(rest, "&", &rest);
		while (token && *token)
		{
			char *value = 0;
      char *eq = PL_strchr(token, '=');
			if (eq)
			{
				value = eq+1;
				*eq = 0;
			}
			
			switch (nsCRT::ToUpper(*token))
			{
/* DO NOT support attachment= in mailto urls. This poses a security fire hole!!! 
				case 'A':
          if (!nsCRT::strcasecmp (token, "attachment"))
					  m_attachmentPart = value;
				  break;
*/
				case 'B':
				  if (!nsCRT::strcasecmp (token, "bcc"))
				  {
					  if (!m_bccPart.IsEmpty())
            {
               m_bccPart += ", ";
               m_bccPart += value;
            }
            else
					    m_bccPart = value; 
          }
					else if (!nsCRT::strcasecmp (token, "body"))
					{
            if (!m_bodyPart.IsEmpty())
            {
              m_bodyPart +="\n";
              m_bodyPart += value;
            }
            else
              m_bodyPart = value;
          }
          break;
        case 'C': 
					if (!nsCRT::strcasecmp  (token, "cc"))
					{
						if (!m_ccPart.IsEmpty())
						{
              m_ccPart += ", ";
              m_ccPart += value;
						}
						else
							m_ccPart = value;
					}
          break;
        case 'F': 
					if (!nsCRT::strcasecmp (token, "followup-to"))
						m_followUpToPart = value;
					else if (!nsCRT::strcasecmp (token, "from"))
						m_fromPart = value;
					break;
        case 'H':
          if (!nsCRT::strcasecmp(token, "html-part") || !nsCRT::strcasecmp (token, "html-body"))
          {
            // m_htmlPart holds the body for both html-part and html-body.
            m_htmlPart = value;
            mFormat = nsIMsgCompFormat::HTML;
          }
          break;
				case 'N':
					if (!nsCRT::strcasecmp (token, "newsgroups"))
						m_newsgroupPart = value;
					else if (!nsCRT::strcasecmp (token, "newshost"))
						m_newsHostPart = value;
				  break;
				case 'O':
					if (!nsCRT::strcasecmp (token, "organization"))
						m_organizationPart = value;
					break;
        case 'R':
					if (!nsCRT::strcasecmp (token, "references"))
						m_referencePart = value;
					else if (!nsCRT::strcasecmp (token, "reply-to"))
						m_replyToPart = value;
					break;
				case 'S':
					if(!nsCRT::strcasecmp (token, "subject"))
						m_subjectPart = value;
					break;
				case 'P':
					if (!nsCRT::strcasecmp (token, "priority"))
						m_priorityPart = PL_strdup(value);
					break;
				case 'T':
					if (!nsCRT::strcasecmp (token, "to"))
				  {
						if (!m_toPart.IsEmpty())
						{
              m_toPart += ", ";
              m_toPart += value;
						}
						else
							m_toPart = value;
					}
					break;
        default:
          break;
      } // end of switch statement...
			
			if (eq)
				  *eq = '='; /* put it back */
				token = nsCRT::strtok(rest, "&", &rest);
		} // while we still have part of the url to parse...
	} // if rest && *rest

  nsCOMPtr<nsIMimeConverter> mimeConverter = do_GetService(NS_MIME_CONVERTER_CONTRACTID);
  char *decodedString;

  // Now unescape any fields that need escaped...
	if (!m_toPart.IsEmpty())
  {
		nsUnescape(m_toPart.BeginWriting());
    if (mimeConverter)
    {
      if (NS_SUCCEEDED(mimeConverter->DecodeMimeHeader(m_toPart.get(),
                                                       &decodedString,
                                                       "UTF-8", PR_FALSE))
                                                       && decodedString)
        m_toPart.Adopt(decodedString);
    }
  }
	if (!m_ccPart.IsEmpty())
  {
		nsUnescape(m_ccPart.BeginWriting());
    if (mimeConverter)
    {
      if (NS_SUCCEEDED(mimeConverter->DecodeMimeHeader(m_ccPart.get(),
                                                       &decodedString,
                                                       "UTF-8", PR_FALSE))
                                                       && decodedString)
        m_ccPart.Adopt(decodedString);
    }
  }
	if (!m_subjectPart.IsEmpty())
  {
    nsUnescape(m_subjectPart.BeginWriting());
    if (mimeConverter)
    {
      if (NS_SUCCEEDED(mimeConverter->DecodeMimeHeader(m_subjectPart.get(),
                                                       &decodedString,
                                                       "UTF-8", PR_FALSE))
                                                       && decodedString)
        m_subjectPart.Adopt(decodedString);
    }
  }
	if (!m_newsgroupPart.IsEmpty())
		nsUnescape(m_newsgroupPart.BeginWriting());
	if (!m_referencePart.IsEmpty())
		nsUnescape(m_referencePart.BeginWriting());
	if (!m_bodyPart.IsEmpty())
  {
		nsUnescape(m_bodyPart.BeginWriting());
    if (mimeConverter)
    {
      if (NS_SUCCEEDED(mimeConverter->DecodeMimeHeader(m_bodyPart.get(),
                                                       &decodedString,
                                                       "UTF-8", PR_FALSE,
                                                       PR_FALSE))
                                                       && decodedString)
        m_bodyPart.Adopt(decodedString);
    }
  }
	if (!m_newsHostPart.IsEmpty())
		nsUnescape(m_newsHostPart.BeginWriting());

	return NS_OK;
}


NS_IMETHODIMP nsMailtoUrl::SetSpec(const nsACString &aSpec)
{
  m_baseURL->SetSpec(aSpec);
	return ParseUrl();
}

nsresult nsMailtoUrl::CleanupMailtoState()
{
    m_ccPart = "";
    m_subjectPart = "";
    m_newsgroupPart = "";
    m_newsHostPart = ""; 
    m_referencePart = "";
    m_bodyPart = "";
    m_bccPart = "";
    m_followUpToPart = "";
    m_fromPart = "";
    m_htmlPart = "";
    m_organizationPart = "";
    m_replyToPart = "";
    m_priorityPart = "";
	return NS_OK;
}

nsresult nsMailtoUrl::ParseUrl()
{
	nsresult rv = NS_OK;

  // we can get the path from the simple url.....
  nsCAutoString aPath;
  m_baseURL->GetPath(aPath);
  m_toPart.Assign(aPath);

  PRInt32 startOfSearchPart = m_toPart.FindChar('?');
  if (startOfSearchPart >= 0)
  {
    // now parse out the search field...
    nsCAutoString searchPart;
    PRUint32 numExtraChars = m_toPart.Right(searchPart,
                                            m_toPart.Length() -
                                            startOfSearchPart);
    if (!searchPart.IsEmpty())
    {
      // now we need to strip off the search part from the
      // to part....
      m_toPart.Cut(startOfSearchPart, numExtraChars);
      ParseMailtoUrl(searchPart.BeginWriting());
    }
	}
  else if (!m_toPart.IsEmpty())
  {
    nsUnescape(m_toPart.BeginWriting());
  }

  return rv;
}

NS_IMETHODIMP nsMailtoUrl::GetMessageContents(char ** aToPart, char ** aCcPart, char ** aBccPart, 
		char ** aFromPart, char ** aFollowUpToPart, char ** aOrganizationPart, 
		char ** aReplyToPart, char ** aSubjectPart, char ** aBodyPart, char ** aHtmlPart, 
		char ** aReferencePart, char ** aAttachmentPart, char ** aPriorityPart, 
		char ** aNewsgroupPart, char ** aNewsHostPart, MSG_ComposeFormat * aFormat)
{
	if (aToPart)
		*aToPart = ToNewCString(m_toPart);
	if (aCcPart)
		*aCcPart = ToNewCString(m_ccPart);
	if (aBccPart)
		*aBccPart = ToNewCString(m_bccPart);
	if (aFromPart)
		*aFromPart = ToNewCString(m_fromPart);
	if (aFollowUpToPart)
		*aFollowUpToPart = ToNewCString(m_followUpToPart);
	if (aOrganizationPart)
		*aOrganizationPart = ToNewCString(m_organizationPart);
	if (aReplyToPart)
		*aReplyToPart = ToNewCString(m_replyToPart);
	if (aSubjectPart)
		*aSubjectPart = ToNewCString(m_subjectPart);
	if (aBodyPart)
		*aBodyPart = ToNewCString(m_bodyPart);
	if (aHtmlPart)
		*aHtmlPart = ToNewCString(m_htmlPart);
	if (aReferencePart)
		*aReferencePart = ToNewCString(m_referencePart);
	if (aAttachmentPart)
		*aAttachmentPart = nsnull; // never pass out an attachment part as part of a mailto url
	if (aPriorityPart)
		*aPriorityPart = ToNewCString(m_priorityPart);
	if (aNewsgroupPart)
		*aNewsgroupPart = ToNewCString(m_newsgroupPart);
	if (aNewsHostPart)
		*aNewsHostPart = ToNewCString(m_newsHostPart);
	if (aFormat)
		*aFormat = mFormat;
	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// Begin nsIURI support
////////////////////////////////////////////////////////////////////////////////////


NS_IMETHODIMP nsMailtoUrl::GetSpec(nsACString &aSpec)
{
	return m_baseURL->GetSpec(aSpec);
}

NS_IMETHODIMP nsMailtoUrl::GetPrePath(nsACString &aPrePath)
{
	return m_baseURL->GetPrePath(aPrePath);
}

NS_IMETHODIMP nsMailtoUrl::GetScheme(nsACString &aScheme)
{
	return m_baseURL->GetScheme(aScheme);
}

NS_IMETHODIMP nsMailtoUrl::SetScheme(const nsACString &aScheme)
{
	m_baseURL->SetScheme(aScheme);
	return ParseUrl();
}

NS_IMETHODIMP nsMailtoUrl::GetUserPass(nsACString &aUserPass)
{
	return m_baseURL->GetUserPass(aUserPass);
}

NS_IMETHODIMP nsMailtoUrl::SetUserPass(const nsACString &aUserPass)
{
	m_baseURL->SetUserPass(aUserPass);
	return ParseUrl();
}

NS_IMETHODIMP nsMailtoUrl::GetUsername(nsACString &aUsername)
{
	return m_baseURL->GetUsername(aUsername);
}

NS_IMETHODIMP nsMailtoUrl::SetUsername(const nsACString &aUsername)
{
	m_baseURL->SetUsername(aUsername);
	return ParseUrl();
}

NS_IMETHODIMP nsMailtoUrl::GetPassword(nsACString &aPassword)
{
	return m_baseURL->GetPassword(aPassword);
}

NS_IMETHODIMP nsMailtoUrl::SetPassword(const nsACString &aPassword)
{
	m_baseURL->SetPassword(aPassword);
	return ParseUrl();
}

NS_IMETHODIMP nsMailtoUrl::GetHostPort(nsACString &aHostPort)
{
	return m_baseURL->GetHost(aHostPort);
}

NS_IMETHODIMP nsMailtoUrl::SetHostPort(const nsACString &aHostPort)
{
	m_baseURL->SetHost(aHostPort);
	return ParseUrl();
}

NS_IMETHODIMP nsMailtoUrl::GetHost(nsACString &aHost)
{
	return m_baseURL->GetHost(aHost);
}

NS_IMETHODIMP nsMailtoUrl::SetHost(const nsACString &aHost)
{
	m_baseURL->SetHost(aHost);
	return ParseUrl();
}

NS_IMETHODIMP nsMailtoUrl::GetPort(PRInt32 *aPort)
{
	return m_baseURL->GetPort(aPort);
}

NS_IMETHODIMP nsMailtoUrl::SetPort(PRInt32 aPort)
{
	m_baseURL->SetPort(aPort);
	return ParseUrl();
}

NS_IMETHODIMP nsMailtoUrl::GetPath(nsACString &aPath)
{
	return m_baseURL->GetPath(aPath);
}

NS_IMETHODIMP nsMailtoUrl::SetPath(const nsACString &aPath)
{
	m_baseURL->SetPath(aPath);
	return ParseUrl();
}

NS_IMETHODIMP nsMailtoUrl::GetAsciiHost(nsACString &aHostA)
{
	return m_baseURL->GetAsciiHost(aHostA);
}

NS_IMETHODIMP nsMailtoUrl::GetAsciiSpec(nsACString &aSpecA)
{
	return m_baseURL->GetAsciiSpec(aSpecA);
}

NS_IMETHODIMP nsMailtoUrl::GetOriginCharset(nsACString &aOriginCharset)
{
    return m_baseURL->GetOriginCharset(aOriginCharset);
}

NS_IMETHODIMP nsMailtoUrl::SchemeIs(const char *aScheme, PRBool *_retval)
{
	return m_baseURL->SchemeIs(aScheme, _retval);
}

NS_IMETHODIMP nsMailtoUrl::Equals(nsIURI *other, PRBool *_retval)
{
	return m_baseURL->Equals(other, _retval);
}

NS_IMETHODIMP nsMailtoUrl::Clone(nsIURI **_retval)
{
	return m_baseURL->Clone(_retval);
}	

NS_IMETHODIMP nsMailtoUrl::Resolve(const nsACString &relativePath, nsACString &result) 
{
	return m_baseURL->Resolve(relativePath, result);
}




/////////////////////////////////////////////////////////////////////////////////////
// smtp url definition
/////////////////////////////////////////////////////////////////////////////////////

nsSmtpUrl::nsSmtpUrl() : nsMsgMailNewsUrl()
{
  // nsISmtpUrl specific state...

  m_fileName = nsnull;
  m_isPostMessage = PR_TRUE;
}
 
nsSmtpUrl::~nsSmtpUrl()
{
}
  
NS_IMPL_ISUPPORTS_INHERITED1(nsSmtpUrl, nsMsgMailNewsUrl, nsISmtpUrl)  

////////////////////////////////////////////////////////////////////////////////////
// Begin nsISmtpUrl specific support

////////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsSmtpUrl::SetSpec(const nsACString &aSpec)
{
  nsresult rv = nsMsgMailNewsUrl::SetSpec(aSpec);
  if (NS_SUCCEEDED(rv))
    rv = ParseUrl();
  return rv;
}

// mscott - i think this function can be obsoleted and its functionality
// moved into SetSpec or an init method....
nsresult nsSmtpUrl::ParseUrl()
{
  nsresult rv = NS_OK;
  
  // set the username
  nsCAutoString userName;
  rv = GetUsername(userName);
  if (NS_FAILED(rv)) return rv; 
  m_userName = userName;
  
  return NS_OK;
}

NS_IMETHODIMP
nsSmtpUrl::SetRecipients(const char * aRecipientsList)
{
  NS_ENSURE_ARG(aRecipientsList);
  m_toPart = aRecipientsList;
  if (!m_toPart.IsEmpty())
    nsUnescape(m_toPart.BeginWriting());
  return NS_OK;
}


NS_IMETHODIMP
nsSmtpUrl::GetRecipients(char ** aRecipientsList)
{
  NS_ENSURE_ARG_POINTER(aRecipientsList);
	if (aRecipientsList)
		*aRecipientsList = ToNewCString(m_toPart);
	return NS_OK;
}

NS_IMPL_GETSET(nsSmtpUrl, PostMessage, PRBool, m_isPostMessage)

// the message can be stored in a file....allow accessors for getting and setting
// the file name to post...
NS_IMETHODIMP nsSmtpUrl::SetPostMessageFile(nsIFileSpec * aFileSpec)
{
  nsresult rv = NS_OK;
  if (aFileSpec)
    m_fileName = aFileSpec;
  else
    rv = NS_ERROR_NULL_POINTER;
  
  return rv;
}

NS_IMETHODIMP nsSmtpUrl::GetPostMessageFile(nsIFileSpec ** aFileSpec)
{
  nsresult rv = NS_OK;
  if (aFileSpec)
  {
    *aFileSpec = m_fileName;
    NS_IF_ADDREF(*aFileSpec);
  }
  else
    rv = NS_ERROR_NULL_POINTER;
  
  return rv;
}

NS_IMETHODIMP 
nsSmtpUrl::GetSenderIdentity(nsIMsgIdentity * *aSenderIdentity)
{
  NS_ENSURE_ARG_POINTER(aSenderIdentity); 
  
  *aSenderIdentity = m_senderIdentity;
  NS_ADDREF(*aSenderIdentity);
  return NS_OK;
}

NS_IMETHODIMP 
nsSmtpUrl::SetSenderIdentity(nsIMsgIdentity * aSenderIdentity) 
{
  NS_ENSURE_ARG_POINTER(aSenderIdentity);
  
  m_senderIdentity = aSenderIdentity;
  return NS_OK;
}

NS_IMETHODIMP
nsSmtpUrl::SetPrompt(nsIPrompt *aNetPrompt)
{
    NS_ENSURE_ARG_POINTER(aNetPrompt);
    m_netPrompt = aNetPrompt;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpUrl::GetPrompt(nsIPrompt **aNetPrompt)
{
    NS_ENSURE_ARG_POINTER(aNetPrompt);
    if (!m_netPrompt) return NS_ERROR_NULL_POINTER;
    *aNetPrompt = m_netPrompt;
    NS_ADDREF(*aNetPrompt);
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpUrl::SetAuthPrompt(nsIAuthPrompt *aNetAuthPrompt)
{
    NS_ENSURE_ARG_POINTER(aNetAuthPrompt);
    m_netAuthPrompt = aNetAuthPrompt;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpUrl::GetAuthPrompt(nsIAuthPrompt **aNetAuthPrompt)
{
    NS_ENSURE_ARG_POINTER(aNetAuthPrompt);
    if (!m_netAuthPrompt) return NS_ERROR_NULL_POINTER;
    *aNetAuthPrompt = m_netAuthPrompt;
    NS_ADDREF(*aNetAuthPrompt);
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpUrl::SetNotificationCallbacks(nsIInterfaceRequestor* aCallbacks)
{
    NS_ENSURE_ARG_POINTER(aCallbacks);
    m_callbacks = aCallbacks;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpUrl::GetNotificationCallbacks(nsIInterfaceRequestor** aCallbacks)
{
    NS_ENSURE_ARG_POINTER(aCallbacks);
    if (!m_callbacks) return NS_ERROR_NULL_POINTER;
    *aCallbacks = m_callbacks;
    NS_ADDREF(*aCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpUrl::SetSmtpServer(nsISmtpServer * aSmtpServer)
{
    NS_ENSURE_ARG_POINTER(aSmtpServer);
    m_smtpServer = aSmtpServer;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpUrl::GetSmtpServer(nsISmtpServer ** aSmtpServer)
{
    NS_ENSURE_ARG_POINTER(aSmtpServer);
    if (!m_smtpServer) return NS_ERROR_NULL_POINTER;
    *aSmtpServer = m_smtpServer;
    NS_ADDREF(*aSmtpServer);
    return NS_OK;
}

