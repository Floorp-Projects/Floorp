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

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif

#include "nsIURL.h"
#include "nsSmtpUrl.h"

#include "nsINetService.h"  /* XXX: NS_FALSE */
#include "nsString.h"

extern "C" {
	char * NET_SACopy (char **destination, const char *source);
	char * NET_SACat (char **destination, const char *source);
};

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

nsSmtpUrl::nsSmtpUrl() : nsMsgMailNewsUrl(), 
    m_userPassword(""),
    m_userName(""),
    m_fileName("")
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

	m_userNameString = nsnull;
 
    m_port = SMTP_PORT;
}
 
nsSmtpUrl::~nsSmtpUrl()
{
	CleanupSmtpState(); 
	delete [] m_userNameString;
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



// XXX recode to use nsString api's
// XXX don't bother with port numbers
// XXX don't bother with ref's
// XXX null pointer checks are incomplete

nsresult nsSmtpUrl::ParseUrl(const nsString& aSpec)
{
    // XXX hack!
    char* cSpec = aSpec.ToNewCString();

    NS_LOCK_INSTANCE();

    PR_FREEIF(m_protocol);
    PR_FREEIF(m_host);
    PR_FREEIF(m_ref);
	PR_FREEIF(m_search);

    m_port = SMTP_PORT;

    // Strip out reference and search info
    char* ref = strpbrk(cSpec, "#?");
    if (nsnull != ref) {
        char* search = nsnull;
        if ('#' == *ref) {
            search = PL_strchr(ref + 1, '?');
            if (nsnull != search) {
                *search++ = '\0';
            }

            PRIntn hashLen = PL_strlen(ref + 1);
            if (0 != hashLen) {
                m_ref = (char*) PR_Malloc(hashLen + 1);
                PL_strcpy(m_ref, ref + 1);
            }      
        }
        else {
            search = ref + 1;
        }

        if (search) {
            // The rest is the search
            PRIntn searchLen = PL_strlen(search);
            if (0 != searchLen) {
                m_search = (char*) PR_Malloc(searchLen + 1);
                PL_strcpy(m_search, search);
            }      
        }

        // XXX Terminate string at start of reference or search
        *ref = '\0';
    }

    // The URL is considered absolute if and only if it begins with a
    // protocol spec. A protocol spec is an alphanumeric string of 1 or
    // more characters that is terminated with a colon.
    PRBool isAbsolute = PR_FALSE;
    char* cp = nsnull;
    char* ap = cSpec;
    char ch;
    while (0 != (ch = *ap)) {
        if (((ch >= 'a') && (ch <= 'z')) ||
            ((ch >= 'A') && (ch <= 'Z')) ||
            ((ch >= '0') && (ch <= '9'))) {
            ap++;
            continue;
        }
        if ((ch == ':') && (ap - cSpec >= 2)) {
            isAbsolute = PR_TRUE;
            cp = ap;
            break;
        }
        break;
    }

	// absolute spec
    PR_FREEIF(m_spec);
    PRInt32 slen = aSpec.Length();
    m_spec = (char *) PR_Malloc(slen + 1);
    aSpec.ToCString(m_spec, slen+1);

    // get protocol first
    PRInt32 plen = cp - cSpec;
    m_protocol = (char*) PR_Malloc(plen + 1);
    PL_strncpy(m_protocol, cSpec, plen);
    m_protocol[plen] = 0;
    cp++;                               // eat : in protocol

    // skip over one, two or three slashes
	if (*cp == '/') 
	{
		cp++;
        if (*cp == '/') 
		{
           cp++;
           if (*cp == '/') 
			   cp++;
        } 
		else 
		{
            delete cSpec;

            NS_UNLOCK_INSTANCE();
            return NS_ERROR_ILLEGAL_VALUE;
        }

        const char* cp0 = cp;
        // To field follows protocol for smtp style urls
        cp = PL_strpbrk(cp, "/:");
      
        if (nsnull == cp) 
		{
			 // There is only a host name
             PRInt32 hlen = PL_strlen(cp0);
             m_toPart = (char*) PR_Malloc(hlen + 1);
             PL_strcpy(m_toPart, cp0);
		}
		else 
		{
			// host name came first....the recipients are really just after
			// the host name....so cp points to "/recip1,recip2,..."

			// grab the host name if it is there....
			PRInt32 hlen = cp - cp0;
			if (hlen > 0)
			{
				m_host = (char*) PR_Malloc(hlen + 1);
				PL_strncpy(m_host, cp0, hlen);
				m_host[hlen] = 0;
			}
			
			// what about the port?
			if (*cp == ':') // then port follows the ':"
			{
				// We have a port number
                cp0 = cp+1;
                cp = PL_strchr(cp, '/');
                m_port = strtol(cp0, (char **)nsnull, 10);
			}
			
			if (*cp == '/')
				cp++;
			if (cp)
				m_toPart = PL_strdup(cp);			
		}
	}

	// now parse out the search field...
	ParseMessageToPost(m_search);
    delete cSpec;

    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

void nsSmtpUrl::ReconstructSpec(void)
{
    PR_FREEIF(m_spec);

    char portBuffer[10];
    if (-1 != m_port) {
        PR_snprintf(portBuffer, 10, ":%d", m_port);
    }
    else {
        portBuffer[0] = '\0';
    }

    PRInt32 plen = PL_strlen(m_protocol) + PL_strlen(m_host) +
        PL_strlen(portBuffer) + PL_strlen(m_file) + 4;
    if (m_ref) {
        plen += 1 + PL_strlen(m_ref);
    }
    if (m_search) {
        plen += 1 + PL_strlen(m_search);
    }

    m_spec = (char *) PR_Malloc(plen + 1);
    PR_snprintf(m_spec, plen, "%s://%s%s%s", 
                m_protocol, ((nsnull != m_host) ? m_host : ""), portBuffer,
                m_file);

    if (m_ref) {
        PL_strcat(m_spec, "#");
        PL_strcat(m_spec, m_ref);
    }
    if (m_search) {
        PL_strcat(m_spec, "?");
        PL_strcat(m_spec, m_search);
    }

	// and as a last step, parse the search field as it contains the message (if the messasge
	// is in the url....
	ParseMessageToPost(m_search);
}

nsresult nsSmtpUrl::GetUserEmailAddress(const char ** aUserName)
{
	nsresult rv = NS_OK;
	if (aUserName)
		*aUserName = m_userNameString;
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv;
}

nsresult nsSmtpUrl::GetUserPassword(const nsString ** aUserPassword)
{
	nsresult rv = NS_OK;
	if (aUserPassword)
		*aUserPassword = &m_userPassword;
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv;
}

nsresult nsSmtpUrl::SetUserEmailAddress(const nsString& aUserName)
{
	nsresult rv = NS_OK;
	if (aUserName.GetUnicode())
	{
		m_userName = aUserName;
		if (m_userNameString)
			delete [] m_userNameString;
		m_userNameString = m_userName.ToNewCString();
	}

	return rv;
}
	
nsresult nsSmtpUrl::SetUserPassword(const nsString& aUserPassword)
{
	nsresult rv = NS_OK;
	if (aUserPassword.GetUnicode())
	{
		m_userPassword = aUserPassword;
	}

	return rv;
}

nsresult nsSmtpUrl::GetMessageContents(const char ** aToPart, const char ** aCcPart, const char ** aBccPart, 
		const char ** aFromPart, const char ** aFollowUpToPart, const char ** aOrganizationPart, 
		const char ** aReplyToPart, const char ** aSubjectPart, const char ** aBodyPart, const char ** aHtmlPart, 
		const char ** aReferencePart, const char ** aAttachmentPart, const char ** aPriorityPart, 
		const char ** aNewsgroupPart, const char ** aNewsHostPart, PRBool * aForcePlainText)
{
	if (aToPart)
		*aToPart = m_toPart;
	if (aCcPart)
		*aCcPart = m_ccPart;
	if (aBccPart)
		*aBccPart = m_bccPart;
	if (aFromPart)
		*aFromPart = m_fromPart;
	if (aFollowUpToPart)
		*aFollowUpToPart = m_followUpToPart;
	if (aOrganizationPart)
		*aOrganizationPart = m_organizationPart;
	if (aReplyToPart)
		*aReplyToPart = m_replyToPart;
	if (aSubjectPart)
		*aSubjectPart = m_subjectPart;
	if (aBodyPart)
		*aBodyPart = m_bodyPart;
	if (aHtmlPart)
		*aHtmlPart = m_htmlPart;
	if (aReferencePart)
		*aReferencePart = m_referencePart;
	if (aAttachmentPart)
		*aAttachmentPart = m_attachmentPart;
	if (aPriorityPart)
		*aPriorityPart = m_priorityPart;
	if (aNewsgroupPart)
		*aNewsgroupPart = m_newsgroupPart;
	if (aNewsHostPart)
		*aNewsHostPart = m_newsHostPart;
	if (aForcePlainText)
		*aForcePlainText = m_forcePlainText;
	return NS_OK;
}

// Caller must call PR_FREE on list when it is done with it. This list is a list of all
// recipients to send the email to. each name is NULL terminated...
nsresult nsSmtpUrl::GetAllRecipients(char ** aRecipientsList)
{
	if (aRecipientsList)
		*aRecipientsList = m_toPart ? PL_strdup(m_toPart) : nsnull;
	return NS_OK;
}

// is the url a post message url or a bring up the compose window url? 
nsresult nsSmtpUrl::IsPostMessage(PRBool * aPostMessage)
{
	if (aPostMessage)
		*aPostMessage = PR_TRUE;
	return NS_OK;
}
	
// used to set the url as a post message url...
nsresult nsSmtpUrl::SetPostMessage(PRBool aPostMessage)
{
	return NS_OK;
}

// the message can be stored in a file....allow accessors for getting and setting
// the file name to post...
nsresult nsSmtpUrl::SetPostMessageFile(const nsFilePath& aFileName)
{
	nsresult rv = NS_OK;
	if (aFileName)
		m_fileName = aFileName;

	return rv;
}

nsresult nsSmtpUrl::GetPostMessageFile(const nsFilePath ** aFileName)
{
	nsresult rv = NS_OK;
	if (aFileName)
		*aFileName = &m_fileName;
	
	return rv;
}
