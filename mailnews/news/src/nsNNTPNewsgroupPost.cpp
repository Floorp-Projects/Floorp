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

// for CRLF
#include "fe_proto.h" 

#define HEADER_FROM				0
#define HEADER_NEWSGROUPS		1
#define HEADER_SUBJECT			2

// set this to the last required header
#define HEADER_LAST_REQUIRED	HEADER_SUBJECT

#define HEADER_PATH				3
#define HEADER_DATE				4

#define HEADER_REPLYTO			5
#define HEADER_SENDER			6
#define HEADER_FOLLOWUPTO		7
#define HEADER_DATERECEIVED		8
#define HEADER_EXPIRES			9
#define HEADER_CONTROL			10
#define HEADER_DISTRIBUTION		11
#define HEADER_ORGANIZATION		12
#define HEADER_REFERENCES		13

// stuff that's required to be in the message,
// but probably generated on the server
#define HEADER_RELAYVERSION		14
#define HEADER_POSTINGVERSION	15
#define HEADER_MESSAGEID	    16

// keep this in sync with the above
#define HEADER_LAST             HEADER_MESSAGEID


class nsNNTPNewsgroupPost : public nsINNTPNewsgroupPost {
    
public:
    nsNNTPNewsgroupPost();
    virtual ~nsNNTPNewsgroupPost();
    
    NS_DECL_ISUPPORTS
    
    // Required headers
    NS_IMPL_CLASS_GETSET_STR(RelayVersion, m_header[HEADER_RELAYVERSION]);
    NS_IMPL_CLASS_GETSET_STR(PostingVersion, m_header[HEADER_POSTINGVERSION]);
    NS_IMPL_CLASS_GETSET_STR(From, m_header[HEADER_FROM]);
    NS_IMPL_CLASS_GETSET_STR(Date, m_header[HEADER_DATE]);
    NS_IMPL_CLASS_GETSET_STR(Subject, m_header[HEADER_SUBJECT]);
    
    NS_IMETHOD AddNewsgroup(const char *newsgroupName);
    NS_IMPL_CLASS_GETTER_STR(GetNewsgroups, m_header[HEADER_NEWSGROUPS]);
    
    NS_IMETHOD GetMessageID(char * *aMessageID);
    
    NS_IMPL_CLASS_GETSET_STR(Path, m_header[HEADER_PATH]);

    // Optional Headers
    NS_IMPL_CLASS_GETSET_STR(ReplyTo, m_header[HEADER_REPLYTO]);
    NS_IMPL_CLASS_GETSET_STR(Sender, m_header[HEADER_SENDER]);
    NS_IMPL_CLASS_GETSET_STR(FollowupTo, m_header[HEADER_FOLLOWUPTO]);
    NS_IMPL_CLASS_GETSET_STR(DateRecieved, m_header[HEADER_DATERECEIVED]);
    NS_IMPL_CLASS_GETSET_STR(Expires, m_header[HEADER_EXPIRES]);
    NS_IMPL_CLASS_GETSET_STR(Control, m_header[HEADER_CONTROL]);
    NS_IMPL_CLASS_GETSET_STR(Distribution, m_header[HEADER_DISTRIBUTION]);
    NS_IMPL_CLASS_GETSET_STR(Organization, m_header[HEADER_ORGANIZATION]);
    NS_IMPL_CLASS_GETSET_STR(Body, m_body);    

    NS_IMETHOD AddReference(const char *referenceID);
    NS_IMPL_CLASS_GETTER_STR(GetReferences, m_header[HEADER_REFERENCES]);


    NS_IMETHOD isValid(PRBool *_retval);
    
    NS_IMETHOD MakeControlCancel(const char *messageID);

    NS_IMPL_CLASS_GETTER(GetIsControl, PRBool, m_isControl);

    NS_IMETHOD GetFullMessage(char **message);
    
    // helper routines
    static char *appendAndAlloc(char *string, const char *newSubstring,
                                PRBool withComma);
private:
    
    char *m_header[HEADER_LAST+1];
    static const char *m_headerName[HEADER_LAST+1];
    
    char *m_body;
    char *m_messageBuffer;
    PRBool m_isControl;
};

const char* nsNNTPNewsgroupPost::m_headerName[HEADER_LAST+1]=
{
    "From: ",
    "Newsgroups: ",
    "Subject: ",

    "Path: ",
    "Date: ",
    
    "Reply-To: ",
    "Sender: ",
    "Followup-To: ",
    "Date-Received: ",
    "Expires: ",
    "Control: ",
    "Distribution: ",
    "Organization: ",
    "References: ",
    
    "Relay-Version: ",
    "Posting-Version: ",
    "Message-ID: ",
};
    
NS_IMPL_ISUPPORTS(nsNNTPNewsgroupPost, nsINNTPNewsgroupPost::GetIID());

nsNNTPNewsgroupPost::nsNNTPNewsgroupPost()
{
    int i;
    for (i=0; i <= HEADER_LAST; i++)
        m_header[i]=nsnull;

    m_body=nsnull;
    m_messageBuffer=nsnull;
    
    m_isControl=PR_FALSE;
}

nsNNTPNewsgroupPost::~nsNNTPNewsgroupPost()
{
    int i;
    for (i=0; i<=HEADER_LAST; i++)
        if (m_header[i]) PR_FREEIF(m_header[i]);

    PR_FREEIF(m_body);
    PR_FREEIF(m_messageBuffer);
}

nsresult
nsNNTPNewsgroupPost::isValid(PRBool *_retval)
{
    if (!_retval) return NS_OK;

    // hack to get lazy-generated message ID to work
    char *messageid;
    GetMessageID(&messageid);

    PRBool valid=PR_TRUE;
    int i;
    for (i=0; i<=HEADER_LAST_REQUIRED;i++) {
        if (!m_header[i]) {
            valid=PR_FALSE;
            break;
        }
    }

    *_retval=valid;
    printf("nsNNTPNewsgroupPost::isValid() -> %s (%d is first invalid)\n",
           *_retval ? "TRUE": "FALSE", i);
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

    int len=0;
    int i;
    for (i=0; i<=HEADER_LAST; i++) {
        if (m_header[i]) {
            len+=PL_strlen(m_headerName[i]);
            len+=PL_strlen(m_header[i]);
            len+=2;
        }
    }
    len += PL_strlen(m_body);

    // for trailing \0 and CRLF between headers and message
    len+= PL_strlen(CRLF)+1;

    PR_FREEIF(m_messageBuffer);
    m_messageBuffer = (char *)PR_Calloc(len, sizeof(char));

    PL_strcpy(m_messageBuffer,"");
    for (i=0; i<=HEADER_LAST; i++) {
        if (m_header[i]) {
            PL_strcat(m_messageBuffer, m_headerName[i]);
            PL_strcat(m_messageBuffer, m_header[i]);
            PL_strcat(m_messageBuffer, CRLF);
        }
    }
    
    PL_strcat(m_messageBuffer, CRLF);
    PL_strcat(m_messageBuffer, m_body);
    
    *message=m_messageBuffer;

    printf("Assembled message:\n%s\n",m_messageBuffer);
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
    m_header[HEADER_NEWSGROUPS]=appendAndAlloc(m_header[HEADER_NEWSGROUPS], newsgroup, PR_TRUE);
    return NS_OK;
}
nsresult
nsNNTPNewsgroupPost::AddReference(const char *reference)
{
    m_header[HEADER_REFERENCES]=appendAndAlloc(m_header[HEADER_REFERENCES], reference, PR_FALSE);
    return NS_OK;
}

nsresult
nsNNTPNewsgroupPost::GetMessageID(char **messageID)
{
    // hack - this is where we will somehow generate the ID
    static char *fakeID = "<ABCDEFG@mcom.com>";
    //    m_header[HEADER_MESSAGEID] = fakeID;
    
    *messageID = fakeID;
    if (!messageID) return NS_ERROR_NULL_POINTER;
    return NS_OK;
}


NS_BEGIN_EXTERN_C

nsresult NS_NewNewsgroupPost(nsINNTPNewsgroupPost **aPost)
{
    if (!aPost) return NS_ERROR_NULL_POINTER;
    nsNNTPNewsgroupPost *post = new nsNNTPNewsgroupPost();
    return post->QueryInterface(nsINNTPNewsgroupPost::GetIID(),
                                (void **)aPost);
}

NS_END_EXTERN_C
