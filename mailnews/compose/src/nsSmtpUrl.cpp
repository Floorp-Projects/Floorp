/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"

#include "nsIURI.h"
#include "nsSmtpUrl.h"
#include "nsString.h"

extern "C" {
	char * NET_SACopy (char **destination, const char *source);
	char * NET_SACat (char **destination, const char *source);
}

nsresult NS_NewSmtpUrl(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (aInstancePtrResult)
	{
		nsSmtpUrl * smtpUrl = new nsSmtpUrl(); 
		if (smtpUrl)
			return smtpUrl->QueryInterface(nsCOMTypeInfo<nsISmtpUrl>::GetIID(), aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

nsSmtpUrl::nsSmtpUrl() : nsMsgMailNewsUrl()
{
	// nsISmtpUrl specific state...
	m_toPart = nsnull;
	m_ccPart = nsnull;
	m_subjectPart = nsnull;
	m_newsgroupPart = nsnull;
	m_newsHostPart = nsnull;
	m_referencePart = nsnull;
	m_attachmentPart = nsnull;
	m_bodyPart = nsnull;
	m_bccPart = nsnull;
	m_followUpToPart = nsnull;
	m_fromPart = nsnull;
	m_htmlPart = nsnull;
	m_organizationPart = nsnull;
	m_replyToPart = nsnull;
	m_priorityPart = nsnull;
	m_fileName = nsnull;
	m_isPostMessage = PR_TRUE;
}
 
nsSmtpUrl::~nsSmtpUrl()
{
	CleanupSmtpState(); 
	PR_FREEIF(m_toPart);
}
  
NS_IMPL_ISUPPORTS_INHERITED(nsSmtpUrl, nsMsgMailNewsUrl, nsISmtpUrl)  

////////////////////////////////////////////////////////////////////////////////////
// Begin nsISmtpUrl specific support

////////////////////////////////////////////////////////////////////////////////////


/* parse special headers and stuff from the search data in the
   URL address.  This data is of the form

	mailto:TO_FIELD?FIELD1=VALUE1&FIELD2=VALUE2

   where TO_FIELD may be empty, VALUEn may (for now) only be
   one of "cc", "bcc", "subject", "newsgroups", "references",
   and "attachment".

   "to" is allowed as a field/value pair as well, for consistency.
 */
nsresult nsSmtpUrl::CleanupSmtpState()
{
	PR_FREEIF(m_ccPart);
	PR_FREEIF(m_subjectPart);
	PR_FREEIF(m_newsgroupPart);
	PR_FREEIF(m_newsHostPart);
	PR_FREEIF(m_referencePart);
	PR_FREEIF(m_attachmentPart);
	PR_FREEIF(m_bodyPart);
	PR_FREEIF(m_bccPart);
	PR_FREEIF(m_followUpToPart);
	PR_FREEIF(m_fromPart);
	PR_FREEIF(m_htmlPart);
	PR_FREEIF(m_organizationPart);
	PR_FREEIF(m_replyToPart);
	PR_FREEIF(m_priorityPart);

	return NS_OK;
}
nsresult nsSmtpUrl::ParseMessageToPost(char * searchPart)

{
	char *rest = searchPart;
	// okay, first, free up all of our old search part state.....
	CleanupSmtpState();
#ifdef UNREADY_CODE	
	HG27293
#endif

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
					if (!PL_strcasecmp (token, "attachment"))
						m_attachmentPart = PL_strdup(value);
					break;
				  case 'B':
					if (!PL_strcasecmp (token, "bcc"))
					{
						if (m_bccPart && *m_bccPart)
						{
							NET_SACat (&m_bccPart, ", ");
							NET_SACat (&m_bccPart, value);
						}
						else
							m_bccPart = PL_strdup(value); 
					}
					else if (!PL_strcasecmp (token, "body"))
					{
						if (m_bodyPart && *m_bodyPart)
						{
							NET_SACat (&m_bodyPart, "\n");
							NET_SACat (&m_bodyPart, value);
						}
						else
							m_bodyPart = PL_strdup(value);
					}
					break;
				  case 'C': 
					if (!PL_strcasecmp (token, "cc"))
					{
						if (m_ccPart && *m_ccPart)
						{
							NET_SACat (&m_ccPart, ", ");
							NET_SACat (&m_ccPart, value);
						}
						else
							m_ccPart = PL_strdup(value);
					}
					break;
#ifdef UNREADY_CODE
				  HG68946
#endif
				  case 'F': 
					if (!PL_strcasecmp (token, "followup-to"))
						m_followUpToPart = PL_strdup(value);
					else if (!PL_strcasecmp (token, "from"))
						m_fromPart = PL_strdup(value);
					else if (!PL_strcasecmp (token, "force-plain-text"))
						m_forcePlainText = PR_TRUE;
					break;
				  case 'H':
					  if (!PL_strcasecmp(token, "html-part"))
						  m_htmlPart = PL_strdup(value);
				  case 'N':
					if (!PL_strcasecmp (token, "newsgroups"))
						m_newsgroupPart = PL_strdup(value);
					else if (!PL_strcasecmp (token, "newshost"))
						m_newsHostPart = PL_strdup(value);
					break;
				  case 'O':
					if (!PL_strcasecmp (token, "organization"))
						m_organizationPart = PL_strdup(value);
					break;
				  case 'R':
					if (!PL_strcasecmp (token, "references"))
						m_referencePart = PL_strdup(value);
					else if (!PL_strcasecmp (token, "reply-to"))
						m_replyToPart = PL_strdup(value);
					break;
				  case 'S':
					if(!PL_strcasecmp (token, "subject"))
						m_subjectPart = PL_strdup(value);
#ifdef UNREADY_CODE
					HG11764
#endif
					break;
				  case 'P':
					if (!PL_strcasecmp (token, "priority"))
						m_priorityPart = PL_strdup(value);
					break;
				  case 'T':
					if (!PL_strcasecmp (token, "to"))
					  {
						if (m_toPart && *m_toPart)
						  {
							NET_SACat (&m_toPart, ", ");
							NET_SACat (&m_toPart, value);
						  }
						else
							m_toPart = PL_strdup(value);
					  }
					break;
				  
			} // end of switch statement...
			
			if (eq)
				  *eq = '='; /* put it back */
				token = nsCRT::strtok(rest, "&", &rest);
		} // while we still have part of the url to parse...
	} // if rest && *rest

	// Now escape any fields that need escaped...
	if (m_toPart)
		nsUnescape(m_toPart);
	if (m_ccPart)
		nsUnescape(m_ccPart);
	if (m_subjectPart)
		nsUnescape(m_subjectPart);
	if (m_newsgroupPart)
		nsUnescape(m_newsgroupPart);
	if (m_referencePart)
		nsUnescape(m_referencePart);
	if (m_attachmentPart)
		nsUnescape(m_attachmentPart);
	if (m_bodyPart)
		nsUnescape(m_bodyPart);
	if (m_newsHostPart)
		nsUnescape(m_newsHostPart);

	return NS_OK;
}


NS_IMETHODIMP nsSmtpUrl::SetSpec(char * aSpec)
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
    NS_LOCK_INSTANCE();

	nsresult rv = NS_OK;

	// the recipients should consist of just the path part up to to the query part
	rv = GetFileName(&m_toPart);

	// now parse out the search field...
	char * searchPart = nsnull;
	rv = GetQuery(&searchPart);
	if (NS_SUCCEEDED(rv) && searchPart)
	{
		ParseMessageToPost(searchPart);
		nsCRT::free(searchPart);
	}

    NS_UNLOCK_INSTANCE();
    return rv;
}

nsresult nsSmtpUrl::GetUserEmailAddress(char ** aUserName)
{
	nsresult rv = NS_OK;
	if (aUserName)
		*aUserName = m_userName.ToNewCString();
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv;
}

nsresult nsSmtpUrl::SetUserEmailAddress(char * aUserName)
{
	nsresult rv = NS_OK;
	if (aUserName)
	{	
		m_userName = aUserName;
	}
	else
		rv = NS_ERROR_NULL_POINTER;

	return rv;
}
	

nsresult nsSmtpUrl::GetMessageContents(char ** aToPart, char ** aCcPart, char ** aBccPart, 
		char ** aFromPart, char ** aFollowUpToPart, char ** aOrganizationPart, 
		char ** aReplyToPart, char ** aSubjectPart, char ** aBodyPart, char ** aHtmlPart, 
		char ** aReferencePart, char ** aAttachmentPart, char ** aPriorityPart, 
		char ** aNewsgroupPart, char ** aNewsHostPart, PRBool * aForcePlainText)
{
	if (aToPart)
		*aToPart = nsCRT::strdup(m_toPart);
	if (aCcPart)
		*aCcPart = nsCRT::strdup(m_ccPart);
	if (aBccPart)
		*aBccPart = nsCRT::strdup(m_bccPart);
	if (aFromPart)
		*aFromPart = nsCRT::strdup(m_fromPart);
	if (aFollowUpToPart)
		*aFollowUpToPart = nsCRT::strdup(m_followUpToPart);
	if (aOrganizationPart)
		*aOrganizationPart = nsCRT::strdup(m_organizationPart);
	if (aReplyToPart)
		*aReplyToPart = nsCRT::strdup(m_replyToPart);
	if (aSubjectPart)
		*aSubjectPart = nsCRT::strdup(m_subjectPart);
	if (aBodyPart)
		*aBodyPart = nsCRT::strdup(m_bodyPart);
	if (aHtmlPart)
		*aHtmlPart = nsCRT::strdup(m_htmlPart);
	if (aReferencePart)
		*aReferencePart = nsCRT::strdup(m_referencePart);
	if (aAttachmentPart)
		*aAttachmentPart = nsCRT::strdup(m_attachmentPart);
	if (aPriorityPart)
		*aPriorityPart = nsCRT::strdup(m_priorityPart);
	if (aNewsgroupPart)
		*aNewsgroupPart = nsCRT::strdup(m_newsgroupPart);
	if (aNewsHostPart)
		*aNewsHostPart = nsCRT::strdup(m_newsHostPart);
	if (aForcePlainText)
		*aForcePlainText = m_forcePlainText;
	return NS_OK;
}

// Caller must call PR_FREE on list when it is done with it. This list is a list of all
// recipients to send the email to. each name is NULL terminated...
nsresult nsSmtpUrl::GetAllRecipients(char ** aRecipientsList)
{
	if (aRecipientsList)
		*aRecipientsList = m_toPart ? nsCRT::strdup(m_toPart) : nsnull;
	return NS_OK;
}

NS_IMPL_GETSET(nsSmtpUrl, PostMessage, PRBool, m_isPostMessage)

// the message can be stored in a file....allow accessors for getting and setting
// the file name to post...
nsresult nsSmtpUrl::SetPostMessageFile(nsIFileSpec * aFileSpec)
{
	nsresult rv = NS_OK;
	if (aFileSpec)
		m_fileName = dont_QueryInterface(aFileSpec);
	else
		rv = NS_ERROR_NULL_POINTER;

	return rv;
}

nsresult nsSmtpUrl::GetPostMessageFile(nsIFileSpec ** aFileSpec)
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
