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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "msgCore.h"

#include "nsIURI.h"
#include "nsSmtpUrl.h"
#include "nsString.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

/////////////////////////////////////////////////////////////////////////////////////
// mailto url definition
/////////////////////////////////////////////////////////////////////////////////////
nsMailtoUrl::nsMailtoUrl()
{
  NS_INIT_ISUPPORTS();
  m_forcePlainText = PR_FALSE;
  nsComponentManager::CreateInstance(kSimpleURICID, nsnull, 
                                     NS_GET_IID(nsIURI), 
                                     (void **) getter_AddRefs(m_baseURL));
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
				case 'A':
          if (!nsCRT::strcasecmp (token, "attachment"))
					  m_attachmentPart = value;
				  break;
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
					else if (!nsCRT::strcasecmp (token, "force-plain-text"))
						m_forcePlainText = PR_TRUE;
					break;
        case 'H':
				  if (!nsCRT::strcasecmp(token, "html-part"))
						  m_htmlPart = value;
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
      } // end of switch statement...
			
			if (eq)
				  *eq = '='; /* put it back */
				token = nsCRT::strtok(rest, "&", &rest);
		} // while we still have part of the url to parse...
	} // if rest && *rest

	// Now unescape any fields that need escaped...
	if (!m_toPart.IsEmpty())
		nsUnescape(m_toPart);
	if (!m_ccPart.IsEmpty())
		nsUnescape(m_ccPart);
	if (!m_subjectPart.IsEmpty())
		nsUnescape(m_subjectPart);
	if (!m_newsgroupPart.IsEmpty())
		nsUnescape(m_newsgroupPart);
	if (!m_referencePart.IsEmpty())
		nsUnescape(m_referencePart);
	if (!m_attachmentPart.IsEmpty())
		nsUnescape(m_attachmentPart);
	if (!m_bodyPart.IsEmpty())
		nsUnescape(m_bodyPart);
	if (!m_newsHostPart.IsEmpty())
		nsUnescape(m_newsHostPart);

	return NS_OK;
}


NS_IMETHODIMP nsMailtoUrl::SetSpec(const char * aSpec)
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
    m_attachmentPart = "";
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
  nsXPIDLCString aPath;
  m_baseURL->GetPath(getter_Copies(aPath));
  if (aPath)
    m_toPart.Assign(aPath);

  PRInt32 startOfSearchPart = m_toPart.FindChar('?');
  if (startOfSearchPart > 0)
  {
    // now parse out the search field...
    nsCAutoString searchPart;
    PRUint32 numExtraChars = m_toPart.Mid(searchPart, startOfSearchPart, -1);
    if (!searchPart.IsEmpty())
    {
		  ParseMailtoUrl(searchPart);
      // now we need to strip off the search part from the
      // to part....
      m_toPart.Cut(startOfSearchPart, numExtraChars);
    }
	}
  else if (!m_toPart.IsEmpty())
  {
    nsUnescape(m_toPart);
  }

  return rv;
}

NS_IMETHODIMP nsMailtoUrl::GetMessageContents(char ** aToPart, char ** aCcPart, char ** aBccPart, 
		char ** aFromPart, char ** aFollowUpToPart, char ** aOrganizationPart, 
		char ** aReplyToPart, char ** aSubjectPart, char ** aBodyPart, char ** aHtmlPart, 
		char ** aReferencePart, char ** aAttachmentPart, char ** aPriorityPart, 
		char ** aNewsgroupPart, char ** aNewsHostPart, PRBool * aForcePlainText)
{
	if (aToPart)
		*aToPart = m_toPart.ToNewCString();
	if (aCcPart)
		*aCcPart = m_ccPart.ToNewCString();
	if (aBccPart)
		*aBccPart = m_bccPart.ToNewCString();
	if (aFromPart)
		*aFromPart = m_fromPart.ToNewCString();
	if (aFollowUpToPart)
		*aFollowUpToPart = m_followUpToPart.ToNewCString();
	if (aOrganizationPart)
		*aOrganizationPart = m_organizationPart.ToNewCString();
	if (aReplyToPart)
		*aReplyToPart = m_replyToPart.ToNewCString();
	if (aSubjectPart)
		*aSubjectPart = m_subjectPart.ToNewCString();
	if (aBodyPart)
		*aBodyPart = m_bodyPart.ToNewCString();
	if (aHtmlPart)
		*aHtmlPart = m_htmlPart.ToNewCString();
	if (aReferencePart)
		*aReferencePart = m_referencePart.ToNewCString();
	if (aAttachmentPart)
		*aAttachmentPart = m_attachmentPart.ToNewCString();
	if (aPriorityPart)
		*aPriorityPart = m_priorityPart.ToNewCString();
	if (aNewsgroupPart)
		*aNewsgroupPart = m_newsgroupPart.ToNewCString();
	if (aNewsHostPart)
		*aNewsHostPart = m_newsHostPart.ToNewCString();
	if (aForcePlainText)
		*aForcePlainText = m_forcePlainText;
	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// Begin nsIURI support
////////////////////////////////////////////////////////////////////////////////////


NS_IMETHODIMP nsMailtoUrl::GetSpec(char * *aSpec)
{
	return m_baseURL->GetSpec(aSpec);
}

NS_IMETHODIMP nsMailtoUrl::GetPrePath(char * *aPrePath)
{
	return m_baseURL->GetPrePath(aPrePath);
}

NS_IMETHODIMP nsMailtoUrl::SetPrePath(const char * aPrePath)
{
	return m_baseURL->SetPrePath(aPrePath);
}

NS_IMETHODIMP nsMailtoUrl::GetScheme(char * *aScheme)
{
	return m_baseURL->GetScheme(aScheme);
}

NS_IMETHODIMP nsMailtoUrl::SetScheme(const char * aScheme)
{
	return m_baseURL->SetScheme(aScheme);
}

NS_IMETHODIMP nsMailtoUrl::GetPreHost(char * *aPreHost)
{
	return m_baseURL->GetPreHost(aPreHost);
}

NS_IMETHODIMP nsMailtoUrl::SetPreHost(const char * aPreHost)
{
	return m_baseURL->SetPreHost(aPreHost);
}

NS_IMETHODIMP nsMailtoUrl::GetUsername(char * *aUsername)
{
	return m_baseURL->GetUsername(aUsername);
}

NS_IMETHODIMP nsMailtoUrl::SetUsername(const char * aUsername)
{
	return m_baseURL->SetUsername(aUsername);
}

NS_IMETHODIMP nsMailtoUrl::GetPassword(char * *aPassword)
{
	return m_baseURL->GetPassword(aPassword);
}

NS_IMETHODIMP nsMailtoUrl::SetPassword(const char * aPassword)
{
	return m_baseURL->SetPassword(aPassword);
}

NS_IMETHODIMP nsMailtoUrl::GetHost(char * *aHost)
{
	return m_baseURL->GetHost(aHost);
}

NS_IMETHODIMP nsMailtoUrl::SetHost(const char * aHost)
{
	return m_baseURL->SetHost(aHost);
}

NS_IMETHODIMP nsMailtoUrl::GetPort(PRInt32 *aPort)
{
	return m_baseURL->GetPort(aPort);
}

NS_IMETHODIMP nsMailtoUrl::SetPort(PRInt32 aPort)
{
	return m_baseURL->SetPort(aPort);
}

NS_IMETHODIMP nsMailtoUrl::GetPath(char * *aPath)
{
	return m_baseURL->GetPath(aPath);
}

NS_IMETHODIMP nsMailtoUrl::SetPath(const char * aPath)
{
	return m_baseURL->SetPath(aPath);
}

NS_IMETHODIMP nsMailtoUrl::SchemeIs(PRUint32 aScheme, PRBool *_retval)
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

NS_IMETHODIMP nsMailtoUrl::Resolve(const char *relativePath, char **result) 
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
  
NS_IMPL_ISUPPORTS_INHERITED(nsSmtpUrl, nsMsgMailNewsUrl, nsISmtpUrl)  

////////////////////////////////////////////////////////////////////////////////////
// Begin nsISmtpUrl specific support

////////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsSmtpUrl::SetSpec(const char * aSpec)
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
	nsXPIDLCString userName;
	rv = GetUsername(getter_Copies(userName));
	if (NS_FAILED(rv)) return rv; 
	m_userName = (const char *)userName;
 
  return NS_OK;
}

NS_IMETHODIMP
nsSmtpUrl::SetRecipients(const char * aRecipientsList)
{
  NS_ENSURE_ARG(aRecipientsList);
  m_toPart = aRecipientsList;
  if (!m_toPart.IsEmpty())
    nsUnescape(m_toPart);
	return NS_OK;
}


NS_IMETHODIMP
nsSmtpUrl::GetRecipients(char ** aRecipientsList)
{
  NS_ENSURE_ARG_POINTER(aRecipientsList);
	if (aRecipientsList)
		*aRecipientsList = m_toPart.ToNewCString();
	return NS_OK;
}

NS_IMPL_GETSET(nsSmtpUrl, PostMessage, PRBool, m_isPostMessage)

// the message can be stored in a file....allow accessors for getting and setting
// the file name to post...
NS_IMETHODIMP nsSmtpUrl::SetPostMessageFile(nsIFileSpec * aFileSpec)
{
	nsresult rv = NS_OK;
	if (aFileSpec)
		m_fileName = dont_QueryInterface(aFileSpec);
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

	m_senderIdentity = dont_QueryInterface(aSenderIdentity);
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

