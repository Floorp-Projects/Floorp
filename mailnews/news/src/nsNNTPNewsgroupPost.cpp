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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"    // precompiled header...

#include "nsINNTPNewsgroupPost.h"
#include "nsNNTPNewsgroupPost.h"

#include "plstr.h"
#include "prmem.h"

static NS_DEFINE_IID(kINNTPNewsgroupPostIID, NS_INNTPNEWSGROUPPOST_IID);

class nsNNTPNewsgroupPost : nsINNTPNewsgroupPost {
    
public:
    nsNNTPNewsgroupPost();

    NS_DECL_ISUPPORTS
    
    // Required headers
    NS_IMPL_CLASS_GETSET_STR(RelayVersion, m_relayVersion);
    NS_IMPL_CLASS_GETSET_STR(PostingVersion, m_postingVersion);
    NS_IMPL_CLASS_GETSET_STR(From, m_from);
    NS_IMPL_CLASS_GETSET_STR(Date, m_date);
    NS_IMPL_CLASS_GETSET_STR(Subject, m_subject);
    
    NS_IMETHOD AddNewsgroup(const char *newsgroupName);
    NS_IMPL_CLASS_GETTER_STR(GetNewsgroups, m_newsgroups);
    
    NS_IMETHOD GetMessageID(char * *aMessageID);
    
    NS_IMPL_CLASS_GETSET_STR(Path, m_path);

    // Optional Headers
    NS_IMPL_CLASS_GETSET_STR(ReplyTo, m_replyTo);
    NS_IMPL_CLASS_GETSET_STR(Sender, m_sender);
    NS_IMPL_CLASS_GETSET_STR(FollowupTo, m_followupTo);
    NS_IMPL_CLASS_GETSET_STR(DateRecieved, m_dateRecieved);
    NS_IMPL_CLASS_GETSET_STR(Expires, m_expires);
    NS_IMPL_CLASS_GETSET_STR(Control, m_control);
    NS_IMPL_CLASS_GETSET_STR(Distribution, m_distribution);
    NS_IMPL_CLASS_GETSET_STR(Organization, m_organization);
    NS_IMPL_CLASS_GETSET_STR(Body, m_body);    

    NS_IMETHOD AddReference(const char *referenceID);
    NS_IMPL_CLASS_GETTER_STR(GetReferences, m_references);


    NS_IMETHOD isValid(PRBool *_retval);
    
    NS_IMETHOD MakeControlCancel(const char *messageID);

    NS_IMPL_CLASS_GETTER(GetIsControl, PRBool, m_isControl);

    NS_IMETHOD GetFullMessage(char **message);
    
    // helper routines
    static char *appendAndAlloc(char *string, const char *newSubstring,
                                PRBool withComma);
private:

    
    char *m_relayVersion;
    char *m_postingVersion;
    char *m_from;
    char *m_date;
    char *m_newsgroups;
    char *m_messageID;
    char *m_subject;
    char *m_path;

    
    char *m_replyTo;
    char *m_sender;
    char *m_followupTo;
    char *m_dateRecieved;
    char *m_expires;
    char *m_control;
    char *m_distribution;
    char *m_organization;
    char *m_references;
    char *m_body;
    
    char *m_fullMessage;

    PRBool m_isControl;
};

NS_IMPL_ISUPPORTS(nsNNTPNewsgroupPost, kINNTPNewsgroupPostIID);

nsNNTPNewsgroupPost::nsNNTPNewsgroupPost()
{
    m_relayVersion=nsnull;
    m_postingVersion=nsnull;
    m_from=nsnull;
    m_date=nsnull;
    m_subject=nsnull;
    m_path=nsnull;
    m_replyTo=nsnull;
    m_sender=nsnull;
    m_followupTo=nsnull;
    m_dateRecieved=nsnull;
    m_expires=nsnull;
    m_control=nsnull;
    m_distribution=nsnull;
    m_organization=nsnull;
    m_body=nsnull;

    m_fullMessage=nsnull;
    m_isControl=PR_FALSE;

}

nsresult
nsNNTPNewsgroupPost::isValid(PRBool *_retval)
{
    if (!_retval) return NS_OK;

    // check if required headers exist
    *_retval = (m_relayVersion &&
                m_postingVersion &&
                m_from &&
                m_date &&
                m_subject &&
                m_newsgroups &&
                m_messageID &&
                m_path);
    return NS_OK;
}

/* XXX - I'm just guessing at how this works, see RFC850 for more */
nsresult
nsNNTPNewsgroupPost::MakeControlCancel(const char *messageID)
{
    char *new_subject = (char *)PR_Calloc(PL_strlen(messageID) +
                                          PL_strlen("CANCEL ") + 1,
                                          sizeof(char));
    PL_strcpy(new_subject, "CANCEL ");
    PL_strcat(new_subject, messageID);

    m_isControl = TRUE;
    SetSubject(new_subject);

    return NS_OK;
}

nsresult
nsNNTPNewsgroupPost::GetFullMessage(char **message)
{
    if (!message) return NS_ERROR_NULL_POINTER;
    PRBool valid=PR_FALSE;
    isValid(&valid);
    if (!valid) return NS_ERROR_NOT_INITIALIZED;

    *message="";
    return NS_OK;
}

char *
nsNNTPNewsgroupPost::appendAndAlloc(char *string,
                                    const char *newSubstring,
                                    PRBool withComma)
{
    if (!newSubstring) return NULL;
    
    if (!string) return PL_strdup(newSubstring);
    
    char *separator = withComma ? ", " : " ";
    char *oldString = string;
    
    string = (char *)PR_Calloc(PL_strlen(oldString) +
                               PL_strlen(separator) +
                               PL_strlen(newSubstring) + 1,
                               sizeof(char));
    
    PL_strcpy(string, oldString);
    PL_strcat(string, separator);
    PL_strcat(string, newSubstring);

    PR_Free(oldString);
    return string;
}

nsresult
nsNNTPNewsgroupPost::AddNewsgroup(const char *newsgroup)
{
    m_newsgroups=appendAndAlloc(m_newsgroups, newsgroup, PR_TRUE);
    return NS_OK;
}
nsresult
nsNNTPNewsgroupPost::AddReference(const char *reference)
{
    m_references=appendAndAlloc(m_references, reference, PR_FALSE);
    return NS_OK;
}

nsresult
nsNNTPNewsgroupPost::GetMessageID(char **messageID)
{
    // hack - this is where we will somehow generate the ID
    static char *fakeID = "ABCDEFG@mcom.com";
    if (!messageID) return NS_ERROR_NULL_POINTER;
    *messageID = fakeID;
    return NS_OK;
}


NS_BEGIN_EXTERN_C

nsresult NS_NewNewsgroupPost(nsINNTPNewsgroupPost **aPost)
{
    if (!aPost) return NS_ERROR_NULL_POINTER;
    nsNNTPNewsgroupPost *post = new nsNNTPNewsgroupPost();
    return post->QueryInterface(kINNTPNewsgroupPostIID,
                                (void **)aPost);
}

NS_END_EXTERN_C
