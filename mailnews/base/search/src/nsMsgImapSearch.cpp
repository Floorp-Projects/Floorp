/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

// Implementation of search for IMAP mail folders

//-----------------------------------------------------------------------------
//---------- Adapter class for searching online (IMAP) folders ----------------
//-----------------------------------------------------------------------------

const char *msg_SearchOnlineMail::m_kSearchTemplate  = "SEARCH%s%s";


msg_SearchOnlineMail::msg_SearchOnlineMail (MSG_ScopeTerm *scope, MSG_SearchTermArray &termList) : msg_SearchAdapter (scope, termList)
{
    m_encoding = NULL;
}


msg_SearchOnlineMail::~msg_SearchOnlineMail () 
{
    XP_FREEIF(m_encoding);
}


MSG_SearchError msg_SearchOnlineMail::ValidateTerms ()
{
    MSG_SearchError err = msg_SearchAdapter::ValidateTerms ();
    
    if (SearchError_Success == err)
    {
        // ### mwelch Figure out the charsets to use 
        //            for the search terms and targets.
        PRInt16 src_csid, dst_csid;
        GetSearchCSIDs(src_csid, dst_csid);

        // do IMAP specific validation
        char *tmpEncoding = NULL;
        err = Encode (&tmpEncoding, m_searchTerms, src_csid, dst_csid);
        if (SearchError_Success == err)
        {
            // we are searching an online folder, right?
            PR_ASSERT(m_scope->m_folder->GetType() == FOLDER_IMAPMAIL);
            MSG_IMAPFolderInfoMail *imapFolder = (MSG_IMAPFolderInfoMail *) m_scope->m_folder;
            m_encoding = CreateImapSearchUrl(imapFolder->GetHostName(),
                                                      imapFolder->GetOnlineName(),
                                                      imapFolder->GetOnlineHierarchySeparator(),
                                                      tmpEncoding,
                                                      PR_TRUE);    // return UIDs
            delete [] tmpEncoding;
        }
        else 
            if (err == SearchError_ScopeAgreement)
                PR_ASSERT(PR_FALSE);
    }
    
    return err;
}


const char *msg_SearchOnlineMail::GetEncoding ()
{
    return m_encoding;
}


void msg_SearchOnlineMail::PreExitFunction (URL_Struct * /*url*/, int status, MWContext *context)
{
    MSG_SearchFrame *frame = MSG_SearchFrame::FromContext (context);
    msg_SearchAdapter *adapter = frame->GetRunningAdapter();

    if (status == MK_INTERRUPTED)
    {
        adapter->Abort();
        frame->EndCylonMode();
    }
    else
    {
        frame->m_idxRunningScope++;
        if (frame->m_idxRunningScope >= frame->m_scopeList.GetSize())
            frame->EndCylonMode();
    }
}


// stolen from offline mail, talk to phil
MSG_SearchError msg_SearchOnlineMail::AddResultElement (NeoMessageHdr *pHeaders)
{
    MSG_SearchError err = SearchError_Success;

    MSG_ResultElement *newResult = new MSG_ResultElement (this);

    if (newResult)
    {
        PR_ASSERT (newResult);

        // This isn't very general. Just add the headers we think we'll be interested in
        // to the list of attributes per result element.
        MSG_SearchValue *pValue = new MSG_SearchValue;
        if (pValue)
        {
            ENeoString subject;
            pValue->attribute = attribSubject;
            char *reString = (pHeaders->GetFlags() & kHasRe) ? "Re:" : "";
            pHeaders->GetSubject(subject);
            pValue->u.string = PR_smprintf ("%s%s", reString, (const char*) subject); // hack. invoke cast operator by force
            newResult->AddValue (pValue);
        }
        pValue = new MSG_SearchValue;
        if (pValue)
        {
            pValue->attribute = attribSender;
            pValue->u.string = (char*) PR_Malloc(64);
            if (pValue->u.string)
            {
                pHeaders->GetAuthor(pValue->u.string, 64);
                newResult->AddValue (pValue);
            }
            else
                err = SearchError_OutOfMemory;
        }
        pValue = new MSG_SearchValue;
        if (pValue)
        {
            pValue->attribute = attribDate;
            pValue->u.date = pHeaders->GetDate();
            newResult->AddValue (pValue);
        }
        pValue = new MSG_SearchValue;
        if (pValue)
        {
            pValue->attribute = attribMsgStatus;
            pValue->u.msgStatus = pHeaders->GetFlags();
            newResult->AddValue (pValue);
        }
        pValue = new MSG_SearchValue;
        if (pValue)
        {
            pValue->attribute = attribPriority;
            pValue->u.priority = pHeaders->GetPriority();
            newResult->AddValue (pValue);
        }
        pValue = new MSG_SearchValue;
        if (pValue)
        {
            pValue->attribute = attribLocation;
            pValue->u.string = nsCRT::strdup(m_scope->m_folder->GetName());
            newResult->AddValue (pValue);
        }
        pValue = new MSG_SearchValue;
        if (pValue)
        {
            pValue->attribute = attribMessageKey;
            pValue->u.key = pHeaders->GetMessageKey();
            newResult->AddValue (pValue);
        }
        pValue = new MSG_SearchValue;
        if (pValue)
        {
            pValue->attribute = attribSize;
            pValue->u.size = pHeaders->GetByteLength();
            newResult->AddValue (pValue);
        }
        if (!pValue)
            err = SearchError_OutOfMemory;
        m_scope->m_frame->AddResultElement (newResult);
    }
    return err;
}


#define WHITESPACE " \015\012"     // token delimiter

SEARCH_API void MSG_AddImapSearchHit (MWContext *context, const char *resultLine)
{
    MSG_SearchFrame *frame = MSG_SearchFrame::FromContext (context);
    if (frame)
    {   // this search happened because of a search dialog
        msg_SearchOnlineMail *adapter = (msg_SearchOnlineMail *) frame->GetRunningAdapter();
        if (adapter)
        {
            // open the relevant IMAP db
            MailDB *imapDB = NULL;
            PRBool wasCreated = PR_FALSE;
            ImapMailDB::Open(adapter->m_scope->m_folder->GetMailFolderInfo()->GetPathname(),
                             PR_FALSE,     // do not create if not found
                             &imapDB,
                             adapter->m_scope->m_folder->GetMaster(),
                             &wasCreated);
                             
            if (imapDB)
            {
                // expect search results in the form of "* SEARCH <hit> <hit> ..."
                char *tokenString = nsCRT::strdup(resultLine);
                if (tokenString)
                {
                    char *currentPosition = strcasestr(tokenString, "SEARCH");
                    if (currentPosition)
                    {
                        currentPosition += nsCRT::strlen("SEARCH");
                        
                        PRBool shownUpdateAlert = PR_FALSE;
                        char *hitUidToken = XP_STRTOK(currentPosition, WHITESPACE);
                        while (hitUidToken)
                        {
                            long naturalLong; // %l is 64 bits on OSF1
                            sscanf(hitUidToken, "%ld", &naturalLong);
                            MessageKey hitUid = (MessageKey) naturalLong;
                            
                            MailMessageHdr *hitHeader = imapDB->GetMailHdrForKey(hitUid);
                            if (hitHeader)
                                adapter->AddResultElement(hitHeader);
                            else if (!shownUpdateAlert)
                            {
                                FE_Alert(context, XP_GetString(MK_MSG_SEARCH_HITS_NOT_IN_DB));
                                shownUpdateAlert = PR_TRUE;
                            }
                            
                            hitUidToken = XP_STRTOK(NULL, WHITESPACE);
                        }
                    }
                    
                    XP_FREE(tokenString);
                }
                
                imapDB->Close();
            }
        }
    }
    else
    {
        PR_ASSERT(PR_FALSE); // apparently, this was left over from trying to do filtering on the server
    }
}

MSG_SearchError msg_SearchOnlineMail::Search ()
{
    // we should never end up here for a purely online
    // folder.  We might for an offline IMAP folder.
    MSG_SearchError err = SearchError_NotImplemented;

    return err;
}


MSG_SearchError msg_SearchOnlineMail::Encode (char **ppEncoding, MSG_SearchTermArray &searchTerms, PRInt16 src_csid, PRInt16 dest_csid)
{
    *ppEncoding = NULL;
    char *imapTerms = NULL;

	//check if searchTerms are ascii only
	PRBool asciiOnly = PR_TRUE;
	if ( !(src_csid & CODESET_MASK == STATEFUL || src_csid & CODESET_MASK == WIDECHAR) )   //assume all single/multiple bytes charset has ascii as subset
	{
		int termCount = searchTerms.GetSize();
		int i = 0;

		for (i = 0; i < termCount && asciiOnly; i++)
		{
			MSG_SearchTerm *term = searchTerms.GetAt(i);
			if (IsStringAttribute(term->m_attribute))
			{
				char *pchar = term->m_value.u.string;
				for (; *pchar ; pchar++)
				{
					if (*pchar & 0x80)
					{
						asciiOnly = PR_FALSE;
						break;
					}
				}
			}
		}
	}
	else
		asciiOnly = PR_FALSE;

	// Get the optional CHARSET parameter, in case we need it.
    char *csname = GetImapCharsetParam(asciiOnly ? CS_ASCII : dest_csid);


    MSG_SearchError err = msg_SearchAdapter::EncodeImap (&imapTerms,searchTerms, src_csid, asciiOnly ? CS_ASCII : dest_csid, PR_FALSE);
    if (SearchError_Success == err)
    {
        int len = nsCRT::strlen(m_kSearchTemplate) + nsCRT::strlen(imapTerms) + (csname ? nsCRT::strlen(csname) : 0) + 1;
        *ppEncoding = new char [len];
        if (*ppEncoding)
        {
            PR_snprintf (*ppEncoding, len, m_kSearchTemplate,
                         csname ? csname : "", imapTerms);
        }
        else
            err = SearchError_OutOfMemory;
    }
    XP_FREEIF(csname);
    return err;
}


MSG_SearchError msg_SearchValidityManager::InitOfflineMailTable ()
{
    PR_ASSERT (NULL == m_offlineMailTable);
    MSG_SearchError err = NewTable (&m_offlineMailTable);

    if (SearchError_Success == err)
    {
        m_offlineMailTable->SetAvailable (attribSender, opContains, 1);
        m_offlineMailTable->SetEnabled   (attribSender, opContains, 1);
        m_offlineMailTable->SetAvailable (attribSender, opDoesntContain, 1);
        m_offlineMailTable->SetEnabled   (attribSender, opDoesntContain, 1);
        m_offlineMailTable->SetAvailable (attribSender, opIs, 1);
        m_offlineMailTable->SetEnabled   (attribSender, opIs, 1);
        m_offlineMailTable->SetAvailable (attribSender, opIsnt, 1);
        m_offlineMailTable->SetEnabled   (attribSender, opIsnt, 1);
        m_offlineMailTable->SetAvailable (attribSender, opBeginsWith, 1);
        m_offlineMailTable->SetEnabled   (attribSender, opBeginsWith, 1);
        m_offlineMailTable->SetAvailable (attribSender, opEndsWith, 1);
        m_offlineMailTable->SetEnabled   (attribSender, opEndsWith, 1);

        m_offlineMailTable->SetAvailable (attribTo, opContains, 1);
        m_offlineMailTable->SetEnabled   (attribTo, opContains, 1);
        m_offlineMailTable->SetAvailable (attribTo, opDoesntContain, 1);
        m_offlineMailTable->SetEnabled   (attribTo, opDoesntContain, 1);
        m_offlineMailTable->SetAvailable (attribTo, opIs, 1);
        m_offlineMailTable->SetEnabled   (attribTo, opIs, 1);
        m_offlineMailTable->SetAvailable (attribTo, opIsnt, 1);
        m_offlineMailTable->SetEnabled   (attribTo, opIsnt, 1);
        m_offlineMailTable->SetAvailable (attribTo, opBeginsWith, 1);
        m_offlineMailTable->SetEnabled   (attribTo, opBeginsWith, 1);
        m_offlineMailTable->SetAvailable (attribTo, opEndsWith, 1);
        m_offlineMailTable->SetEnabled   (attribTo, opEndsWith, 1);

        m_offlineMailTable->SetAvailable (attribCC, opContains, 1);
        m_offlineMailTable->SetEnabled   (attribCC, opContains, 1);
        m_offlineMailTable->SetAvailable (attribCC, opDoesntContain, 1);
        m_offlineMailTable->SetEnabled   (attribCC, opDoesntContain, 1);
        m_offlineMailTable->SetAvailable (attribCC, opIs, 1);
        m_offlineMailTable->SetEnabled   (attribCC, opIs, 1);
        m_offlineMailTable->SetAvailable (attribCC, opIsnt, 1);
        m_offlineMailTable->SetEnabled   (attribCC, opIsnt, 1);
        m_offlineMailTable->SetAvailable (attribCC, opBeginsWith, 1);
        m_offlineMailTable->SetEnabled   (attribCC, opBeginsWith, 1);
        m_offlineMailTable->SetAvailable (attribCC, opEndsWith, 1);
        m_offlineMailTable->SetEnabled   (attribCC, opEndsWith, 1);

        m_offlineMailTable->SetAvailable (attribToOrCC, opContains, 1);
        m_offlineMailTable->SetEnabled   (attribToOrCC, opContains, 1);
        m_offlineMailTable->SetAvailable (attribToOrCC, opDoesntContain, 1);
        m_offlineMailTable->SetEnabled   (attribToOrCC, opDoesntContain, 1);
        m_offlineMailTable->SetAvailable (attribToOrCC, opBeginsWith, 1);
        m_offlineMailTable->SetEnabled   (attribToOrCC, opBeginsWith, 1);
        m_offlineMailTable->SetAvailable (attribToOrCC, opEndsWith, 1);
        m_offlineMailTable->SetEnabled   (attribToOrCC, opEndsWith, 1);

        m_offlineMailTable->SetAvailable (attribSubject, opContains, 1);
        m_offlineMailTable->SetEnabled   (attribSubject, opContains, 1);
        m_offlineMailTable->SetAvailable (attribSubject, opDoesntContain, 1);
        m_offlineMailTable->SetEnabled   (attribSubject, opDoesntContain, 1);
        m_offlineMailTable->SetAvailable (attribSubject, opIs, 1);
        m_offlineMailTable->SetEnabled   (attribSubject, opIs, 1);
        m_offlineMailTable->SetAvailable (attribSubject, opIsnt, 1);
        m_offlineMailTable->SetEnabled   (attribSubject, opIsnt, 1);
        m_offlineMailTable->SetAvailable (attribSubject, opBeginsWith, 1);
        m_offlineMailTable->SetEnabled   (attribSubject, opBeginsWith, 1);
        m_offlineMailTable->SetAvailable (attribSubject, opEndsWith, 1);
        m_offlineMailTable->SetEnabled   (attribSubject, opEndsWith, 1);

        m_offlineMailTable->SetAvailable (attribBody, opContains, 1);
        m_offlineMailTable->SetEnabled   (attribBody, opContains, 1);
        m_offlineMailTable->SetAvailable (attribBody, opDoesntContain, 1);
        m_offlineMailTable->SetEnabled   (attribBody, opDoesntContain, 1);
        m_offlineMailTable->SetAvailable (attribBody, opIs, 1);
        m_offlineMailTable->SetEnabled   (attribBody, opIs, 1);
        m_offlineMailTable->SetAvailable (attribBody, opIsnt, 1);
        m_offlineMailTable->SetEnabled   (attribBody, opIsnt, 1);

        m_offlineMailTable->SetAvailable (attribDate, opIsBefore, 1);
        m_offlineMailTable->SetEnabled   (attribDate, opIsBefore, 1);
        m_offlineMailTable->SetAvailable (attribDate, opIsAfter, 1);
        m_offlineMailTable->SetEnabled   (attribDate, opIsAfter, 1);
        m_offlineMailTable->SetAvailable (attribDate, opIs, 1);
        m_offlineMailTable->SetEnabled   (attribDate, opIs, 1);
        m_offlineMailTable->SetAvailable (attribDate, opIsnt, 1);
        m_offlineMailTable->SetEnabled   (attribDate, opIsnt, 1);

        m_offlineMailTable->SetAvailable (attribPriority, opIsHigherThan, 1);
        m_offlineMailTable->SetEnabled   (attribPriority, opIsHigherThan, 1);
        m_offlineMailTable->SetAvailable (attribPriority, opIsLowerThan, 1);
        m_offlineMailTable->SetEnabled   (attribPriority, opIsLowerThan, 1);
        m_offlineMailTable->SetAvailable (attribPriority, opIs, 1);
        m_offlineMailTable->SetEnabled   (attribPriority, opIs, 1);

        m_offlineMailTable->SetAvailable (attribMsgStatus, opIs, 1);
        m_offlineMailTable->SetEnabled   (attribMsgStatus, opIs, 1);
        m_offlineMailTable->SetAvailable (attribMsgStatus, opIsnt, 1);
        m_offlineMailTable->SetEnabled   (attribMsgStatus, opIsnt, 1);

//      m_offlineMailTable->SetValidButNotShown (attribAgeInDays, opIsHigherThan, 1);
//      m_offlineMailTable->SetValidButNotShown (attribAgeInDays, opIsLowerThan,  1);
        m_offlineMailTable->SetAvailable (attribAgeInDays, opIsGreaterThan, 1);
        m_offlineMailTable->SetEnabled   (attribAgeInDays, opIsGreaterThan, 1);
        m_offlineMailTable->SetAvailable (attribAgeInDays, opIsLessThan,  1);
        m_offlineMailTable->SetEnabled   (attribAgeInDays, opIsLessThan, 1);
        m_offlineMailTable->SetAvailable (attribAgeInDays, opIs,  1);
        m_offlineMailTable->SetEnabled   (attribAgeInDays, opIs, 1);

        m_offlineMailTable->SetAvailable (attribOtherHeader, opContains, 1);   // added for arbitrary headers
        m_offlineMailTable->SetEnabled   (attribOtherHeader, opContains, 1);
        m_offlineMailTable->SetAvailable (attribOtherHeader, opDoesntContain, 1);
        m_offlineMailTable->SetEnabled   (attribOtherHeader, opDoesntContain, 1);
        m_offlineMailTable->SetAvailable (attribOtherHeader, opIs, 1);
        m_offlineMailTable->SetEnabled   (attribOtherHeader, opIs, 1);
        m_offlineMailTable->SetAvailable (attribOtherHeader, opIsnt, 1);
        m_offlineMailTable->SetEnabled   (attribOtherHeader, opIsnt, 1);

    }

    return err;
}


MSG_SearchError msg_SearchValidityManager::InitOnlineMailTable ()
{
    PR_ASSERT (NULL == m_onlineMailTable);
    MSG_SearchError err = NewTable (&m_onlineMailTable);

    if (SearchError_Success == err)
    {
        m_onlineMailTable->SetAvailable (attribSender, opContains, 1);
        m_onlineMailTable->SetEnabled   (attribSender, opContains, 1);
        m_onlineMailTable->SetAvailable (attribSender, opDoesntContain, 1);
        m_onlineMailTable->SetEnabled   (attribSender, opDoesntContain, 1);

        m_onlineMailTable->SetAvailable (attribTo, opContains, 1);
        m_onlineMailTable->SetEnabled   (attribTo, opContains, 1);
        m_onlineMailTable->SetAvailable (attribTo, opDoesntContain, 1);
        m_onlineMailTable->SetEnabled   (attribTo, opDoesntContain, 1);

        m_onlineMailTable->SetAvailable (attribCC, opContains, 1);
        m_onlineMailTable->SetEnabled   (attribCC, opContains, 1);
        m_onlineMailTable->SetAvailable (attribCC, opDoesntContain, 1);
        m_onlineMailTable->SetEnabled   (attribCC, opDoesntContain, 1);

        m_onlineMailTable->SetAvailable (attribToOrCC, opContains, 1);
        m_onlineMailTable->SetEnabled   (attribToOrCC, opContains, 1);
        m_onlineMailTable->SetAvailable (attribToOrCC, opDoesntContain, 1);
        m_onlineMailTable->SetEnabled   (attribToOrCC, opDoesntContain, 1);

        m_onlineMailTable->SetAvailable (attribSubject, opContains, 1);
        m_onlineMailTable->SetEnabled   (attribSubject, opContains, 1);
        m_onlineMailTable->SetAvailable (attribSubject, opDoesntContain, 1);
        m_onlineMailTable->SetEnabled   (attribSubject, opDoesntContain, 1);

        m_onlineMailTable->SetAvailable (attribBody, opContains, 1);
        m_onlineMailTable->SetEnabled   (attribBody, opContains, 1);
        m_onlineMailTable->SetAvailable (attribBody, opDoesntContain, 1);
        m_onlineMailTable->SetEnabled   (attribBody, opDoesntContain, 1);

        m_onlineMailTable->SetAvailable (attribDate, opIsBefore, 1);
        m_onlineMailTable->SetEnabled   (attribDate, opIsBefore, 1);
        m_onlineMailTable->SetAvailable (attribDate, opIsAfter, 1);
        m_onlineMailTable->SetEnabled   (attribDate, opIsAfter, 1);
        m_onlineMailTable->SetAvailable (attribDate, opIs, 1);
        m_onlineMailTable->SetEnabled   (attribDate, opIs, 1);
        m_onlineMailTable->SetAvailable (attribDate, opIsnt, 1);
        m_onlineMailTable->SetEnabled   (attribDate, opIsnt, 1);

        m_onlineMailTable->SetAvailable (attribMsgStatus, opIs, 1);
        m_onlineMailTable->SetEnabled   (attribMsgStatus, opIs, 1);
        m_onlineMailTable->SetAvailable (attribMsgStatus, opIsnt, 1);
        m_onlineMailTable->SetEnabled   (attribMsgStatus, opIsnt, 1);

        m_onlineMailTable->SetAvailable (attribAgeInDays, opIsGreaterThan, 1);
        m_onlineMailTable->SetEnabled   (attribAgeInDays, opIsGreaterThan, 1);
        m_onlineMailTable->SetAvailable (attribAgeInDays, opIsLessThan,  1);
        m_onlineMailTable->SetEnabled   (attribAgeInDays, opIsLessThan, 1);
        m_onlineMailTable->SetEnabled   (attribAgeInDays, opIs, 1);
        m_onlineMailTable->SetAvailable (attribAgeInDays, opIs, 1);

        m_onlineMailTable->SetAvailable (attribOtherHeader, opContains, 1);   // added for arbitrary headers
        m_onlineMailTable->SetEnabled   (attribOtherHeader, opContains, 1);
        m_onlineMailTable->SetAvailable (attribOtherHeader, opDoesntContain, 1);
        m_onlineMailTable->SetEnabled   (attribOtherHeader, opDoesntContain, 1);

    }

    return err;
}


MSG_SearchError msg_SearchValidityManager::InitOnlineMailFilterTable ()
{
    // Oh what a tangled web...
    //
    // IMAP filtering happens on the client, fundamentally using the same
    // capabilities as POP filtering. However, since we don't yet have the 
    // IMAP message body, we can't filter on body attributes. So this table
    // is supposed to be the same as offline mail, except that the body 
    // attribute is omitted

    PR_ASSERT (NULL == m_onlineMailFilterTable);
    MSG_SearchError err = NewTable (&m_onlineMailFilterTable);

    if (SearchError_Success == err)
    {
        m_onlineMailFilterTable->SetAvailable (attribSender, opContains, 1);
        m_onlineMailFilterTable->SetEnabled   (attribSender, opContains, 1);
        m_onlineMailFilterTable->SetAvailable (attribSender, opDoesntContain, 1);
        m_onlineMailFilterTable->SetEnabled   (attribSender, opDoesntContain, 1);
        m_onlineMailFilterTable->SetAvailable (attribSender, opIs, 1);
        m_onlineMailFilterTable->SetEnabled   (attribSender, opIs, 1);
        m_onlineMailFilterTable->SetAvailable (attribSender, opIsnt, 1);
        m_onlineMailFilterTable->SetEnabled   (attribSender, opIsnt, 1);
        m_onlineMailFilterTable->SetAvailable (attribSender, opBeginsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (attribSender, opBeginsWith, 1);
        m_onlineMailFilterTable->SetAvailable (attribSender, opEndsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (attribSender, opEndsWith, 1);

        m_onlineMailFilterTable->SetAvailable (attribTo, opContains, 1);
        m_onlineMailFilterTable->SetEnabled   (attribTo, opContains, 1);
        m_onlineMailFilterTable->SetAvailable (attribTo, opDoesntContain, 1);
        m_onlineMailFilterTable->SetEnabled   (attribTo, opDoesntContain, 1);
        m_onlineMailFilterTable->SetAvailable (attribTo, opIs, 1);
        m_onlineMailFilterTable->SetEnabled   (attribTo, opIs, 1);
        m_onlineMailFilterTable->SetAvailable (attribTo, opIsnt, 1);
        m_onlineMailFilterTable->SetEnabled   (attribTo, opIsnt, 1);
        m_onlineMailFilterTable->SetAvailable (attribTo, opBeginsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (attribTo, opBeginsWith, 1);
        m_onlineMailFilterTable->SetAvailable (attribTo, opEndsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (attribTo, opEndsWith, 1);

        m_onlineMailFilterTable->SetAvailable (attribCC, opContains, 1);
        m_onlineMailFilterTable->SetEnabled   (attribCC, opContains, 1);
        m_onlineMailFilterTable->SetAvailable (attribCC, opDoesntContain, 1);
        m_onlineMailFilterTable->SetEnabled   (attribCC, opDoesntContain, 1);
        m_onlineMailFilterTable->SetAvailable (attribCC, opIs, 1);
        m_onlineMailFilterTable->SetEnabled   (attribCC, opIs, 1);
        m_onlineMailFilterTable->SetAvailable (attribCC, opIsnt, 1);
        m_onlineMailFilterTable->SetEnabled   (attribCC, opIsnt, 1);
        m_onlineMailFilterTable->SetAvailable (attribCC, opBeginsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (attribCC, opBeginsWith, 1);
        m_onlineMailFilterTable->SetAvailable (attribCC, opEndsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (attribCC, opEndsWith, 1);

        m_onlineMailFilterTable->SetAvailable (attribToOrCC, opContains, 1);
        m_onlineMailFilterTable->SetEnabled   (attribToOrCC, opContains, 1);
        m_onlineMailFilterTable->SetAvailable (attribToOrCC, opDoesntContain, 1);
        m_onlineMailFilterTable->SetEnabled   (attribToOrCC, opDoesntContain, 1);
        m_onlineMailFilterTable->SetAvailable (attribToOrCC, opBeginsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (attribToOrCC, opBeginsWith, 1);
        m_onlineMailFilterTable->SetAvailable (attribToOrCC, opEndsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (attribToOrCC, opEndsWith, 1);

        m_onlineMailFilterTable->SetAvailable (attribSubject, opContains, 1);
        m_onlineMailFilterTable->SetEnabled   (attribSubject, opContains, 1);
        m_onlineMailFilterTable->SetAvailable (attribSubject, opDoesntContain, 1);
        m_onlineMailFilterTable->SetEnabled   (attribSubject, opDoesntContain, 1);
        m_onlineMailFilterTable->SetAvailable (attribSubject, opIs, 1);
        m_onlineMailFilterTable->SetEnabled   (attribSubject, opIs, 1);
        m_onlineMailFilterTable->SetAvailable (attribSubject, opIsnt, 1);
        m_onlineMailFilterTable->SetEnabled   (attribSubject, opIsnt, 1);
        m_onlineMailFilterTable->SetAvailable (attribSubject, opBeginsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (attribSubject, opBeginsWith, 1);
        m_onlineMailFilterTable->SetAvailable (attribSubject, opEndsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (attribSubject, opEndsWith, 1);

        m_onlineMailFilterTable->SetAvailable (attribDate, opIsBefore, 1);
        m_onlineMailFilterTable->SetEnabled   (attribDate, opIsBefore, 1);
        m_onlineMailFilterTable->SetAvailable (attribDate, opIsAfter, 1);
        m_onlineMailFilterTable->SetEnabled   (attribDate, opIsAfter, 1);
        m_onlineMailFilterTable->SetAvailable (attribDate, opIs, 1);
        m_onlineMailFilterTable->SetEnabled   (attribDate, opIs, 1);
        m_onlineMailFilterTable->SetAvailable (attribDate, opIsnt, 1);
        m_onlineMailFilterTable->SetEnabled   (attribDate, opIsnt, 1);

        m_onlineMailFilterTable->SetAvailable (attribPriority, opIsHigherThan, 1);
        m_onlineMailFilterTable->SetEnabled   (attribPriority, opIsHigherThan, 1);
        m_onlineMailFilterTable->SetAvailable (attribPriority, opIsLowerThan, 1);
        m_onlineMailFilterTable->SetEnabled   (attribPriority, opIsLowerThan, 1);
        m_onlineMailFilterTable->SetAvailable (attribPriority, opIs, 1);
        m_onlineMailFilterTable->SetEnabled   (attribPriority, opIs, 1);

        m_onlineMailFilterTable->SetAvailable (attribMsgStatus, opIs, 1);
        m_onlineMailFilterTable->SetEnabled   (attribMsgStatus, opIs, 1);
        m_onlineMailFilterTable->SetAvailable (attribMsgStatus, opIsnt, 1);
        m_onlineMailFilterTable->SetEnabled   (attribMsgStatus, opIsnt, 1);

        m_onlineMailFilterTable->SetValidButNotShown (attribAgeInDays, opIsGreaterThan, 1);
        m_onlineMailFilterTable->SetValidButNotShown (attribAgeInDays, opIsLessThan,  1);
        m_onlineMailFilterTable->SetValidButNotShown (attribAgeInDays, opIs, 1);

        m_onlineMailFilterTable->SetAvailable (attribOtherHeader, opContains, 1);   // added for arbitrary headers
        m_onlineMailFilterTable->SetEnabled   (attribOtherHeader, opContains, 1);
        m_onlineMailFilterTable->SetAvailable (attribOtherHeader, opDoesntContain, 1);
        m_onlineMailFilterTable->SetEnabled   (attribOtherHeader, opDoesntContain, 1);
        m_onlineMailFilterTable->SetAvailable (attribOtherHeader, opIs, 1);
        m_onlineMailFilterTable->SetEnabled   (attribOtherHeader, opIs, 1);
        m_onlineMailFilterTable->SetAvailable (attribOtherHeader, opIsnt, 1);
        m_onlineMailFilterTable->SetEnabled   (attribOtherHeader, opIsnt, 1);

    }

    return err;
}

