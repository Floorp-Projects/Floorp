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
            char *reString = (flags & MSG_FLAG_HAS_RE) ? (char *)"Re:" : (char *)"";
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
            nsCOMPtr<nsIMsgFolder> folder;
            err = m_scope->GetFolder(getter_AddRefs(folder));
            if (NS_SUCCEEDED(err) && folder)
              folder->GetName(getter_Copies(folderName));
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
        nsCOMPtr<nsIMsgSearchSession> searchSession;
        m_scope->GetSearchSession(getter_AddRefs(searchSession));
        if (searchSession)
          searchSession->AddResultElement(newResult);
        //XXXX alecf do not checkin without fixing!        m_scope->m_searchSession->AddResultElement (newResult);
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


nsresult nsMsgSearchValidityManager::InitOfflineMailTable ()
{
    PR_ASSERT (nsnull == m_offlineMailTable);
    nsresult err = NewTable (&m_offlineMailTable);

    if (NS_SUCCEEDED(err))
    {
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);

        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Isnt, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Isnt, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::BeginsWith, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::BeginsWith, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::EndsWith, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::EndsWith, 1);

        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Isnt, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Isnt, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::BeginsWith, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::BeginsWith, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::EndsWith, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::EndsWith, 1);

        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::BeginsWith, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::BeginsWith, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::EndsWith, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::EndsWith, 1);

        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);

        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Contains, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Contains, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::DoesntContain, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::DoesntContain, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Isnt, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Isnt, 1);

        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);

        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsHigherThan, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsHigherThan, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsLowerThan, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsLowerThan, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Priority, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Priority, nsMsgSearchOp::Is, 1);

        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);

//      m_offlineMailTable->SetValidButNotShown (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsHigherThan, 1);
//      m_offlineMailTable->SetValidButNotShown (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLowerThan,  1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan,  1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is,  1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is, 1);

        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);   // added for arbitrary headers
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
        m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Isnt, 1);
        m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Isnt, 1);

    }

    return err;
}


nsresult nsMsgSearchValidityManager::InitOnlineMailTable ()
{
    PR_ASSERT (nsnull == m_onlineMailTable);
    nsresult err = NewTable (&m_onlineMailTable);

    if (NS_SUCCEEDED(err))
    {
        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);

        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);

        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);

        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);

        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);

        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Contains, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Contains, 1);
        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::DoesntContain, 1);

        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);

        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);

        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan,  1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is, 1);
        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is, 1);

        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);   // added for arbitrary headers
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
        m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);

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
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);

        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Is, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Is, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Isnt, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Isnt, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::BeginsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::BeginsWith, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::EndsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::EndsWith, 1);

        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Is, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Is, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Isnt, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Isnt, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::BeginsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::BeginsWith, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::EndsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::EndsWith, 1);

        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::BeginsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::BeginsWith, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::EndsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::EndsWith, 1);

        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);

        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);

        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsHigherThan, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsHigherThan, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsLowerThan, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsLowerThan, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Priority, nsMsgSearchOp::Is, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Priority, nsMsgSearchOp::Is, 1);

        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);

        m_onlineMailFilterTable->SetValidButNotShown (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
        m_onlineMailFilterTable->SetValidButNotShown (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan,  1);
        m_onlineMailFilterTable->SetValidButNotShown (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is, 1);

        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);   // added for arbitrary headers
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
        m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Isnt, 1);
        m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Isnt, 1);

    }

    return err;
}

