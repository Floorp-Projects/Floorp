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


nsMsgSearchOnlineMail::nsMsgSearchOnlineMail (nsMsgSearchScopeTerm *scope, nsISupportsArray *termList) : nsMsgSearchAdapter (scope, termList)
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
      err = Encode (m_encoding, m_searchTerms, dstCharset.GetUnicode());
      NS_ASSERTION(NS_SUCCEEDED(err), "failed to encode imap search");
  }
  
  return err;
}


NS_IMETHODIMP nsMsgSearchOnlineMail::GetEncoding (char **result)
{
  *result = m_encoding.ToNewCString();
  return NS_OK;
}

NS_IMETHODIMP nsMsgSearchOnlineMail::AddResultElement (nsIMsgDBHdr *pHeaders)
{
    nsresult err = NS_OK;

    nsCOMPtr<nsIMsgSearchSession> searchSession;
    m_scope->GetSearchSession(getter_AddRefs(searchSession));
    if (searchSession)
    {
      nsCOMPtr <nsIMsgFolder> scopeFolder;
      err = m_scope->GetFolder(getter_AddRefs(scopeFolder));
      searchSession->AddSearchHit(pHeaders, scopeFolder);
    }
    //XXXX alecf do not checkin without fixing!        m_scope->m_searchSession->AddResultElement (newResult);
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

nsresult nsMsgSearchOnlineMail::Search (PRBool *aDone)
{
    // we should never end up here for a purely online
    // folder.  We might for an offline IMAP folder.
    nsresult err = NS_ERROR_NOT_IMPLEMENTED;

    return err;
}


nsresult nsMsgSearchOnlineMail::Encode (nsCString& pEncoding,
                                        nsISupportsArray *searchTerms,
                                        const PRUnichar *destCharset)
{
    nsXPIDLCString imapTerms;

	//check if searchTerms are ascii only
	PRBool asciiOnly = PR_TRUE;
  // ### what's this mean in the NWO?????

	if (PR_TRUE) // !(srcCharset & CODESET_MASK == STATEFUL || srcCharset & CODESET_MASK == WIDECHAR) )   //assume all single/multiple bytes charset has ascii as subset
	{
		PRUint32 termCount;
    searchTerms->Count(&termCount);
		PRUint32 i = 0;

		for (i = 0; i < termCount && asciiOnly; i++)
		{
      nsCOMPtr<nsIMsgSearchTerm> pTerm;
      searchTerms->QueryElementAt(i, NS_GET_IID(nsIMsgSearchTerm),
                               (void **)getter_AddRefs(pTerm));

      nsMsgSearchAttribValue attribute;
      pTerm->GetAttrib(&attribute);
			if (IsStringAttribute(attribute))
			{
        PRUnichar *pchar, *savepChar;
        nsCOMPtr <nsIMsgSearchValue> searchValue;

        nsresult rv = pTerm->GetValue(getter_AddRefs(searchValue));
        if (!NS_SUCCEEDED(rv) || !searchValue)
          continue;


        rv = searchValue->GetStr(&pchar);
      	if (!NS_SUCCEEDED(rv) || !pchar)
      		continue;
        savepChar = pchar;
				for (; *pchar ; pchar++)
				{
					if (*pchar & 0xFF80)
					{
						asciiOnly = PR_FALSE;
						break;
					}
				}
        nsCRT::free(savepChar);
			}
		}
	}
	else
		asciiOnly = PR_FALSE;

  nsAutoString usAsciiCharSet; usAsciiCharSet.AssignWithConversion("us-ascii");
	// Get the optional CHARSET parameter, in case we need it.
  char *csname = GetImapCharsetParam(asciiOnly ? usAsciiCharSet.GetUnicode() : destCharset);

  nsresult err = nsMsgSearchAdapter::EncodeImap (getter_Copies(imapTerms), searchTerms, asciiOnly ?  usAsciiCharSet.GetUnicode(): destCharset, PR_FALSE);
  if (NS_SUCCEEDED(err))
  {
    pEncoding.Append("SEARCH");
    if (csname)
      pEncoding.Append(csname);
    pEncoding.Append(imapTerms);
  }
  PR_FREEIF(csname);
  return err;
}


nsresult nsMsgSearchValidityManager::InitOfflineMailTable ()
{
    NS_ASSERTION (nsnull == m_offlineMailTable, "offline mail table already initted");
    nsresult err = NewTable (getter_AddRefs(m_offlineMailTable));

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
    NS_ASSERTION (nsnull == m_onlineMailTable, "Online mail table already initted!");
    nsresult err = NewTable (getter_AddRefs(m_onlineMailTable));

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

    NS_ASSERTION (nsnull == m_onlineMailFilterTable, "online filter table already initted");
    nsresult err = NewTable (getter_AddRefs(m_onlineMailFilterTable));

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

