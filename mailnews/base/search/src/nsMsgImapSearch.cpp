/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "msgCore.h"
#include "nsMsgSearchAdapter.h"
#include "nsXPIDLString.h"
#include "nsMsgSearchScopeTerm.h"
#include "nsMsgResultElement.h"
#include "nsMsgSearchTerm.h"
#include "nsIMsgHdr.h"
#include "nsMsgSearchImap.h"
// Implementation of search for IMAP mail folders


nsMsgSearchOnlineMail::nsMsgSearchOnlineMail (nsMsgSearchScopeTerm *scope, nsMsgSearchTermArray &termList) : nsMsgSearchAdapter (scope, termList)
{
}


nsMsgSearchOnlineMail::~nsMsgSearchOnlineMail () 
{
}


nsresult nsMsgSearchOnlineMail::ValidateTerms ()
{
  nsresult err = nsMsgSearchAdapter::ValidateTerms ();
  
  if (NS_SUCCEEDED(err))
  {
      // ### mwelch Figure out the charsets to use 
      //            for the search terms and targets.
      nsString srcCharset, dstCharset;
      GetSearchCharsets(srcCharset, dstCharset);

      // do IMAP specific validation
      err = Encode (&m_encoding, m_searchTerms, srcCharset.GetUnicode(), dstCharset.GetUnicode());
      NS_ASSERTION(NS_SUCCEEDED(err), "failed to encode imap search");
  }
  
  return err;
}


NS_IMETHODIMP nsMsgSearchOnlineMail::GetEncoding (char **result)
{
  *result = m_encoding.ToNewCString();
  return NS_OK;
}

nsresult nsMsgSearchOnlineMail::AddResultElement (nsIMsgDBHdr *pHeaders)
{
    nsresult err = NS_OK;

    nsMsgResultElement *newResult = new nsMsgResultElement (this);

    NS_ASSERTION (newResult, "out of memory getting result element");
    if (newResult)
    {

        // This isn't very general. Just add the headers we think we'll be interested in
        // to the list of attributes per result element.
        nsMsgSearchValue *pValue = new nsMsgSearchValue;
        if (pValue)
        {
            nsXPIDLCString subject;
            PRUint32 flags;
            pValue->attribute = nsMsgSearchAttrib::Subject;
            pHeaders->GetFlags(&flags);
            char *reString = (flags & MSG_FLAG_HAS_RE) ? "Re:" : "";
            pHeaders->GetSubject(getter_Copies(subject));
            pValue->u.string = PR_smprintf ("%s%s", reString, (const char*) subject); // hack. invoke cast operator by force
            newResult->AddValue (pValue);
        }
        pValue = new nsMsgSearchValue;
        if (pValue)
        {
            nsXPIDLCString author;
            pValue->attribute = nsMsgSearchAttrib::Sender;
            pValue->u.string = (char*) PR_Malloc(64);
            if (pValue->u.string)
            {
                pHeaders->GetAuthor(getter_Copies(author));
                PL_strncpy(pValue->u.string, (const char *) author, 64);
                newResult->AddValue (pValue);
            }
            else
                err = NS_ERROR_OUT_OF_MEMORY;
        }
        pValue = new nsMsgSearchValue;
        if (pValue)
        {
            pValue->attribute = nsMsgSearchAttrib::Date;
            pHeaders->GetDate(&pValue->u.date);
            newResult->AddValue (pValue);
        }
        pValue = new nsMsgSearchValue;
        if (pValue)
        {
            pValue->attribute = nsMsgSearchAttrib::MsgStatus;
            pHeaders->GetFlags(&pValue->u.msgStatus);
            newResult->AddValue (pValue);
        }
        pValue = new nsMsgSearchValue;
        if (pValue)
        {
            pValue->attribute = nsMsgSearchAttrib::Priority;
            pHeaders->GetPriority(&pValue->u.priority);
            newResult->AddValue (pValue);
        }
        pValue = new nsMsgSearchValue;
        if (pValue)
        {
          nsXPIDLString folderName;
            pValue->attribute = nsMsgSearchAttrib::Location;
            m_scope->m_folder->GetName(getter_Copies(folderName));
           // pValue->u.wString = nsCRT::strdup((const PRUnichar *) folderName);
            newResult->AddValue (pValue);
        }
        pValue = new nsMsgSearchValue;
        if (pValue)
        {
            pValue->attribute = nsMsgSearchAttrib::MessageKey;
            pHeaders->GetMessageKey(&pValue->u.key);
            newResult->AddValue (pValue);
        }
        pValue = new nsMsgSearchValue;
        if (pValue)
        {
            pValue->attribute = nsMsgSearchAttrib::Size;
            pHeaders->GetMessageSize(&pValue->u.size);
            newResult->AddValue (pValue);
        }
        if (!pValue)
            err = NS_ERROR_OUT_OF_MEMORY;
        m_scope->m_searchSession->AddResultElement (newResult);
    }
    return err;
}


#define WHITESPACE " \015\012"     // token delimiter
#if 0  // this code belongs in imap parser or protocol object
void MSG_AddImapSearchHit (MWContext *context, const char *resultLine)
{
    MSG_SearchFrame *frame = MSG_SearchFrame::FromContext (context);
    if (frame)
    {   // this search happened because of a search dialog
        msg_SearchOnlineMail *adapter = (msg_SearchOnlineMail *) frame->GetRunningAdapter();
        if (adapter)
        {
            // open the relevant IMAP db
            MailDB *imapDB = nsnull;
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
                            
                            hitUidToken = XP_STRTOK(nsnull, WHITESPACE);
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
        NS_ASSERTION(PR_FALSE, "missing search frame?"); // apparently, this was left over from trying to do filtering on the server
    }
}
#endif // 0

nsresult nsMsgSearchOnlineMail::Search ()
{
    // we should never end up here for a purely online
    // folder.  We might for an offline IMAP folder.
    nsresult err = NS_ERROR_NOT_IMPLEMENTED;

    return err;
}


nsresult nsMsgSearchOnlineMail::Encode (nsCString *pEncoding, nsMsgSearchTermArray &searchTerms, const PRUnichar *srcCharset, const PRUnichar *destCharset)
{
    char *imapTerms = nsnull;

	//check if searchTerms are ascii only
	PRBool asciiOnly = PR_TRUE;
  // ### what's this mean in the NWO?????

	if (PR_TRUE) // !(srcCharset & CODESET_MASK == STATEFUL || srcCharset & CODESET_MASK == WIDECHAR) )   //assume all single/multiple bytes charset has ascii as subset
	{
		int termCount = searchTerms.Count();
		int i = 0;

		for (i = 0; i < termCount && asciiOnly; i++)
		{
			nsMsgSearchTerm *term = searchTerms.ElementAt(i);
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

  nsAutoString usAsciiCharSet = "us-ascii";
	// Get the optional CHARSET parameter, in case we need it.
  char *csname = GetImapCharsetParam(asciiOnly ? usAsciiCharSet.GetUnicode() : destCharset);

  nsresult err = nsMsgSearchAdapter::EncodeImap (&imapTerms, searchTerms, srcCharset, asciiOnly ?  usAsciiCharSet.GetUnicode(): destCharset, PR_FALSE);
  if (NS_SUCCEEDED(err))
  {
    pEncoding->Append("SEARCH");
    if (csname)
      pEncoding->Append(csname);
    pEncoding->Append(imapTerms);
  }
  PR_FREEIF(csname);
  return err;
}


#if 0
nsresult nsMsgSearchValidityManager::InitOfflineMailTable ()
{
    PR_ASSERT (nsnull == m_offlineMailTable);
    nsresult err = NewTable (&m_offlineMailTable);

    if (NS_SUCCEEDED(err))
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


nsresult nsMsgSearchValidityManager::InitOnlineMailTable ()
{
    PR_ASSERT (nsnull == m_onlineMailTable);
    nsresult err = NewTable (&m_onlineMailTable);

    if (NS_SUCCEEDED(err))
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


nsresult nsMsgSearchValidityManager::InitOnlineMailFilterTable ()
{
    // Oh what a tangled web...
    //
    // IMAP filtering happens on the client, fundamentally using the same
    // capabilities as POP filtering. However, since we don't yet have the 
    // IMAP message body, we can't filter on body attributes. So this table
    // is supposed to be the same as offline mail, except that the body 
    // attribute is omitted

    PR_ASSERT (nsnull == m_onlineMailFilterTable);
    nsresult err = NewTable (&m_onlineMailFilterTable);

    if (NS_SUCCEEDED(err))
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

#endif
