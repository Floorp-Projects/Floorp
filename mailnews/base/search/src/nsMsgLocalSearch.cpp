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

// Implementation of db search for POP and offline IMAP mail folders

#include "msgCore.h"
#include "nsIMsgDatabase.h"
#include "nsMsgSearchCore.h"
#include "nsMsgLocalSearch.h"
#include "nsIStreamListener.h"
#include "nsParseMailbox.h"
#include "nsMsgSearchBoolExpression.h"
#include "nsMsgSearchTerm.h"
#include "nsMsgResultElement.h"

extern "C"
{
    extern int MK_MSG_SEARCH_STATUS;
    extern int MK_MSG_CANT_SEARCH_IF_NO_SUMMARY;
    extern int MK_MSG_SEARCH_HITS_NOT_IN_DB;
}


//----------------------------------------------------------------------------
// Class definitions for the boolean expression structure....
//----------------------------------------------------------------------------
nsMsgSearchBoolExpression::nsMsgSearchBoolExpression() 
{
    m_term = nsnull;
    m_boolOp = nsMsgSearchBooleanOp::BooleanAND;
    m_evalValue = FALSE;
    m_leftChild = nsnull;
    m_rightChild = nsnull;
}

nsMsgSearchBoolExpression::nsMsgSearchBoolExpression (nsMsgSearchTerm * newTerm, PRBool evalValue, char * encodingStr) 
// we are creating an expression which contains a single search term (newTerm) 
// and the search term's IMAP or NNTP encoding value for online search expressions AND
// a boolean evaluation value which is used for offline search expressions.
{
    m_term = newTerm;
    m_encodingStr = encodingStr;
    m_evalValue = evalValue;

    // this expression does not contain sub expressions
    m_leftChild = nsnull;
    m_rightChild = nsnull;
}


nsMsgSearchBoolExpression::nsMsgSearchBoolExpression (nsMsgSearchBoolExpression * expr1, nsMsgSearchBoolExpression * expr2, nsMsgSearchBooleanOperator boolOp)
// we are creating an expression which contains two sub expressions and a boolean operator used to combine
// them.
{
    m_leftChild = expr1;
    m_rightChild = expr2;
    m_boolOp = boolOp;

    m_term = nsnull;
    m_evalValue = FALSE;
}

nsMsgSearchBoolExpression::~nsMsgSearchBoolExpression()
{
    // we must recursively destroy all sub expressions before we destroy ourself.....We leave search terms alone!
    if (m_leftChild)
        delete m_leftChild;
    if (m_rightChild)
        delete m_rightChild;
}

nsMsgSearchBoolExpression * nsMsgSearchBoolExpression::AddSearchTerm(nsMsgSearchTerm * newTerm, char * encodingStr)
// appropriately add the search term to the current expression and return a pointer to the
// new expression. The encodingStr is the IMAP/NNTP encoding string for newTerm.
{
    return leftToRightAddTerm(newTerm,FALSE,encodingStr);
}

nsMsgSearchBoolExpression * nsMsgSearchBoolExpression::AddSearchTerm(nsMsgSearchTerm * newTerm, PRBool evalValue)
// appropriately add the search term to the current expression
// Returns: a pointer to the new expression which includes this new search term
{
    return leftToRightAddTerm(newTerm, evalValue,nsnull);   // currently we build our expressions to
                                                          // evaluate left to right.
}

nsMsgSearchBoolExpression * nsMsgSearchBoolExpression::leftToRightAddTerm(nsMsgSearchTerm * newTerm, PRBool evalValue, char * encodingStr)
{
    // we have a base case where this is the first term being added to the expression:
    if (!m_term && !m_leftChild && !m_rightChild)
    {
        m_term = newTerm;
        m_evalValue = evalValue;
        m_encodingStr = encodingStr;
        return this;
    }

    nsMsgSearchBoolExpression * tempExpr = new nsMsgSearchBoolExpression (newTerm,evalValue,encodingStr);
    if (tempExpr)  // make sure creation succeeded
    {
        nsMsgSearchBoolExpression * newExpr = new nsMsgSearchBoolExpression (this, tempExpr, newTerm->GetBooleanOp());  
        if (newExpr)
            return newExpr;
        else
            delete tempExpr;    // clean up memory allocation in case of failure
    }
    return this;   // in case we failed to create a new expression, return self
}


PRBool nsMsgSearchBoolExpression::OfflineEvaluate()
// returns TRUE or FALSE depending on what the current expression evaluates to. Since this is
// offline, when we created the expression we stored an evaluation value for each search term in 
// the expression. These are the values we use to determine if the expression is TRUE or FALSE.
{
    if (m_term) // do we contain just a search term?
        return m_evalValue;
    
    // otherwise we must recursively determine the value of our sub expressions
    PRBool result1 = TRUE;    // always default to false positives
    PRBool result2 = TRUE;
    
    if (m_leftChild)
        result1 = m_leftChild->OfflineEvaluate();
    if (m_rightChild)
        result2 = m_rightChild->OfflineEvaluate();

    if (m_boolOp == nsMsgSearchBooleanOp::BooleanOR)
    {
        if (result1 || result2)
            return TRUE;
    }
    
    if (m_boolOp == nsMsgSearchBooleanOp::BooleanAND)
    {
        if (result1 && result2)
            return TRUE;
    }

    return FALSE;
}

// ### Maybe we can get rid of these because of our use of nsString???
// constants used for online searching with IMAP/NNTP encoded search terms.
// the + 1 is to account for null terminators we add at each stage of assembling the expression...
const int sizeOfORTerm = 6+1;  // 6 bytes if we are combining two sub expressions with an OR term
const int sizeOfANDTerm = 1+1; // 1 byte if we are combining two sub expressions with an AND term

PRInt32 nsMsgSearchBoolExpression::CalcEncodeStrSize()
// recursively examine each sub expression and calculate a final size for the entire IMAP/NNTP encoding 
{
    if (!m_term && (!m_leftChild || !m_rightChild))   // is the expression empty?
        return 0;    
    if (m_term)  // are we a leaf node?
        return m_encodingStr.Length();
    if (m_boolOp == nsMsgSearchBooleanOp::BooleanOR)
        return sizeOfORTerm + m_leftChild->CalcEncodeStrSize() + m_rightChild->CalcEncodeStrSize();
    if (m_boolOp == nsMsgSearchBooleanOp::BooleanAND)
        return sizeOfANDTerm + m_leftChild->CalcEncodeStrSize() + m_rightChild->CalcEncodeStrSize();
    return 0;
}


PRInt32 nsMsgSearchBoolExpression::GenerateEncodeStr(nsCString * buffer)
// recurively combine sub expressions to form a single IMAP/NNTP encoded string 
{
    if ((!m_term && (!m_leftChild || !m_rightChild))) // is expression empty?
        return 0;
    
    if (m_term) // are we a leaf expression?
    {
        *buffer += m_encodingStr;
        return m_encodingStr.Length();
    }
    
    // add encode strings of each sub expression
    PRInt32 numBytesAdded = 0;
    if (m_boolOp == nsMsgSearchBooleanOp::BooleanOR) 
    {
        *buffer += " (OR";

        numBytesAdded = m_leftChild->GenerateEncodeStr(buffer);  // insert left expression into the buffer
        numBytesAdded = m_rightChild->GenerateEncodeStr(buffer);  // insert right expression into the buffer
        
        // HACK ALERT!!! if last returned character in the buffer is now a ' ' then we need to remove it because we don't want
        // a ' ' to preceded the closing paren in the OR encoding.
        if (buffer->CharAt(numBytesAdded-1) == ' ')
		{
            buffer->Truncate(buffer->Length() - 1);
		}
        
        *buffer += ')';
    }
    
    if (m_boolOp == nsMsgSearchBooleanOp::BooleanAND)
    {
        buffer[0] = '\0';
        numBytesAdded = m_leftChild->GenerateEncodeStr(buffer); // insert left expression

        numBytesAdded = m_rightChild->GenerateEncodeStr(buffer);
    }
    return 0;
}



//---------------- Adapter class for searching offline IMAP folders -----------
//-----------------------------------------------------------------------------
nsMsgSearchIMAPOfflineMail::nsMsgSearchIMAPOfflineMail (nsMsgSearchScopeTerm *scope, nsMsgSearchTermArray &termList) : nsMsgSearchOfflineMail(scope, termList)
{ 

}                                                                                                                                                                                                                                                                                                                                                                                                                                   


nsMsgSearchIMAPOfflineMail::~nsMsgSearchIMAPOfflineMail()
{

}

nsresult nsMsgSearchIMAPOfflineMail::ValidateTerms ()
{
    // most of this was copied from MSG_SearchOffline::ValidateTerms()....Difference: When using IMAP offline, we do not
    // have a mail folder to validate.
    
    nsresult err = NS_OK;
#ifdef HAVE_SEARCH_PORT
	err = nsMsgSearchOfflineMail::ValidateTerms ();
    if (NS_OK == err)
    {
        // Mail folder must exist. Don't worry about the summary file now since we may
        // have to regenerate the index later
//      XP_StatStruct fileStatus;
//      if (!XP_Stat (m_scope->GetMailPath(), &fileStatus, xpMailFolder))
//      {
            // Make sure the terms themselves are valid
            nsMsgSearchValidityTable *table = nsnull;
            err = gValidityMgr.GetTable (nsMsgSearchValidityManager::offlineMail, &table);
            if (NS_OK == err)
            {
                NS_ASSERTION (table, "found validity table");
                err = table->ValidateTerms (m_searchTerms);
            }
//      }
//      else
//          NS_ASSERTION(0);
    }
#endif
    return err;
}



//-----------------------------------------------------------------------------
//---------------- Adapter class for searching offline folders ----------------
//-----------------------------------------------------------------------------


nsMsgSearchOfflineMail::nsMsgSearchOfflineMail (nsMsgSearchScopeTerm *scope, nsMsgSearchTermArray &termList) : nsMsgSearchAdapter (scope, termList)
{
    m_db = nsnull;
    m_listContext = nsnull;

    m_mailboxParser = nsnull;
    m_parserState = kOpenFolderState;
}


nsMsgSearchOfflineMail::~nsMsgSearchOfflineMail ()
{
    // Database should have been closed when the scope term finished. 
    NS_ASSERTION(!m_db, "db not closed");
}


nsresult nsMsgSearchOfflineMail::ValidateTerms ()
{
    nsresult err = NS_OK;
#ifdef HAVE_SEARCH_PORT
	err = nsMsgSearchAdapter::ValidateTerms ();
    if (NS_OK == err)
    {
        // Mail folder must exist. Don't worry about the summary file now since we may
        // have to regenerate the index later
        XP_StatStruct fileStatus;
        if (!XP_Stat (m_scope->GetMailPath(), &fileStatus, xpMailFolder))
        {
            // Make sure the terms themselves are valid
            nsMsgSearchValidityTable *table = nsnull;
            err = gValidityMgr.GetTable (nsMsgSearchValidityManager::offlineMail, &table);
            if (NS_OK == err)
            {
                NS_ASSERTION (table, "didn't get validity table");
                err = table->ValidateTerms (m_searchTerms);
            }
        }
        else
            NS_ASSERTION(PR_FALSE, "local folder doesn't exist");
    }
#endif // HAVE_SEARCH_PORT
    return err;
}


nsresult nsMsgSearchOfflineMail::OpenSummaryFile ()
{
    nsCOMPtr <nsIMsgDatabase> mailDB ;

    nsresult err = NS_OK;
#ifdef HAVE_SEARCH_PORT
    // do password protection of local cache thing.
    if (m_scope->m_folder && m_scope->m_folder->UserNeedsToAuthenticateForFolder(FALSE) && m_scope->m_folder->GetMaster()->PromptForHostPassword(m_scope->m_frame->GetContext(), m_scope->m_folder) != 0)
    {
        m_scope->m_frame->StopRunning();
        return SearchError_ScopeDone;
    }

    nsresult dbErr = MailDB::Open (m_scope->GetMailPath(), FALSE /*create?*/, &mailDb);
    switch (dbErr)
    {
        case NS_OK:
            break;
        case NS_MSG_ERROR_FOLDER_SUMMARY_MISSING:
        case NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE:
            m_mailboxParser = new nsMsgMailboxParser (m_scope->GetMailPath());
            if (!m_mailboxParser)
                err = NS_ERROR_OUT_OF_MEMORY;
            else
            {
                // Remove the old summary file so maildb::open can create a new one
                XP_FileRemove (m_scope->GetMailPath(), xpMailFolderSummary);
                dbErr = MailDB::Open (m_scope->GetMailPath(), TRUE /*create?*/, &mailDb, TRUE /*upgrading?*/);
                NS_ASSERTION(mailDb, "couldn't opn DB");

                // Initialize the async parser to rebuild the summary file
                m_parserState = kOpenFolderState;
                m_mailboxParser->SetContext (m_scope->m_frame->GetContext());
                m_mailboxParser->SetDB (mailDb);
                m_mailboxParser->SetFolder(m_scope->m_folder);
                m_mailboxParser->SetIgnoreNonMailFolder(TRUE);
                err = NS_OK;
            }
            break;
        default:
        {
#ifdef _DEBUG
            char *buf = PR_smprintf ("Failed to open '%s' with error 0x%08lX", m_scope->m_folder->GetName(), (long) dbErr);
            FE_Alert (m_scope->m_frame->GetContext(), buf);
            XP_FREE (buf);
#endif
            err = SearchError_DBOpenFailed;
        }
    }

    if (mailDb && err == NS_OK)
        m_db = mailDb;
#endif // HAVE_SEARCH_PORT
    return err;
}


nsresult nsMsgSearchOfflineMail::BuildSummaryFile ()
{
    // State machine for rebuilding the summary file asynchronously in the 
    // middle of the already-asynchronous search.

	// ### This would be much better done with a url queue or at least chained urls.
	// I'm not sure if that's possible, however.
    nsresult err = NS_OK;
#ifdef HAVE_SEARCH_PORT
    int mkErr = 0;
    switch (m_parserState)
    {
    case kOpenFolderState:
        mkErr = m_mailboxParser->BeginOpenFolderSock (m_scope->GetMailPath(), nsnull, 0, nsnull);
        if (mkErr == MK_WAITING_FOR_CONNECTION)
            m_parserState++;
        else
            err = SummaryFileError();
        break;
    case kParseMoreState:
        mkErr = m_mailboxParser->ParseMoreFolderSock (m_scope->GetMailPath(), nsnull, 0, nsnull);
        if (mkErr == MK_CONNECTED)
            m_parserState++;
        else
            if (mkErr != MK_WAITING_FOR_CONNECTION)
                err = SummaryFileError();
        break;
    case kCloseFolderState:
        m_mailboxParser->CloseFolderSock (nsnull, nsnull, 0, nsnull);
        if (!m_mailboxParser->GetIsRealMailFolder())
        {
            // mailbox parser has already closed the db (right?)
            NS_ASSERTION(m_mailboxParser->GetDB() == 0, "parser hasn't closed DB");
            m_db = nsnull;
            err = SearchError_ScopeDone;
        }
        delete m_mailboxParser;
        m_mailboxParser = nsnull;
        // Put our regular "searching Inbox..." status text back up
        m_scope->m_frame->UpdateStatusBar(MK_nsMsgSearch_STATUS);
        break;
    }
#endif // HAVE_SEARCH_PORT
    return err;
}


nsresult nsMsgSearchOfflineMail::SummaryFileError ()
{
#ifdef HAVE_SEARCH_PORT
    char *errTemplate = XP_GetString(MK_MSG_CANT_SEARCH_IF_NO_SUMMARY);
    if (errTemplate)
    {
        char *prompt = PR_smprintf (errTemplate, m_scope->m_folder->GetName());
        if (prompt)
        {
            FE_Alert (m_scope->m_frame->GetContext(), prompt);
            XP_FREE(prompt);
        }
    }

    // If we got a summary file error while parsing, clean up all the parser state
    if (m_mailboxParser)
    {
        m_mailboxParser->CloseFolderSock (nsnull, nsnull, 0, nsnull);
        delete m_mailboxParser;
        m_mailboxParser = nsnull;
        m_db = nsnull;
    }

#endif // HAVE_SEARCH_PORT
    return  NS_OK;// SearchError_ScopeDone;
}

nsresult nsMsgSearchOfflineMail::MatchTermsForFilter(nsIMsgDBHdr *msgToMatch,
                                                           nsMsgSearchTermArray & termList,
                                                           nsMsgSearchScopeTerm * scope,
                                                           nsIMsgDatabase * db, 
                                                           const char * headers,
                                                           PRUint32 headerSize)
{
    return MatchTerms(msgToMatch, termList, scope, db, headers, headerSize, TRUE);
}

// static method which matches a header against a list of search terms.
nsresult nsMsgSearchOfflineMail::MatchTermsForSearch(nsIMsgDBHdr *msgToMatch, 
                                                nsMsgSearchTermArray &termList,
                                                nsMsgSearchScopeTerm *scope,
                                                nsIMsgDatabase *db)
{
    return MatchTerms(msgToMatch, termList, scope, db, nsnull, 0, FALSE);
}

nsresult nsMsgSearchOfflineMail::MatchTerms(nsIMsgDBHdr *msgToMatch,
                                            nsMsgSearchTermArray & termList,
                                            nsMsgSearchScopeTerm * scope,
                                            nsIMsgDatabase * db, 
                                            const char * headers,
                                            PRUint32 headerSize,
                                            PRBool Filtering) 
{
    nsresult err = NS_OK;
    nsString  recipients;
    nsString  ccList;
    nsString  matchString;
	PRUint32 msgFlags;

    // Don't even bother to look at expunged messages awaiting compression
    msgToMatch->GetFlags(&msgFlags);
	if (msgFlags & MSG_FLAG_EXPUNGED)
        err = NS_COMFALSE;

    // Loop over all terms, and match them all to this message. 

    const char *charset = nsnull; // scope->m_folder->GetFolderCSID() & ~CS_AUTO;
//    if (CS_DEFAULT == csid)
  //      csid = INTL_DefaultWinCharSetID(0);


	// ### DMB Todo - remove nsAutoCString when nsString2 lands.
    nsMsgSearchBoolExpression * expression = new nsMsgSearchBoolExpression();  // create our expression
    if (!expression)
        return NS_ERROR_OUT_OF_MEMORY;
    for (int i = 0; i < termList.Count(); i++)
    {
        nsMsgSearchTerm *pTerm = termList.ElementAt(i);
//        NS_ASSERTION (pTerm->IsValid(), "invalid search term");
        NS_ASSERTION (msgToMatch, "couldn't get term to match");

        switch (pTerm->m_attribute)
        {
        case nsMsgSearchAttrib::Sender:
            msgToMatch->GetAuthor(&matchString);
            err = pTerm->MatchRfc822String (nsAutoString(matchString,eOneByte).GetBuffer(), charset);
            break;
        case nsMsgSearchAttrib::Subject:
			{
            msgToMatch->GetSubject(&matchString /* , TRUE */);
			nsString2 singleByteString(matchString, eOneByte); 
            err = pTerm->MatchString (&singleByteString, charset);
			}
            break;
        case nsMsgSearchAttrib::ToOrCC:
        {
            nsresult errKeepGoing = pTerm->MatchAllBeforeDeciding() ? NS_OK : NS_COMFALSE;
            msgToMatch->GetRecipients(&recipients);
            err = pTerm->MatchRfc822String (nsAutoString(recipients,eOneByte).GetBuffer(), charset);
            if (errKeepGoing == err)
            {
                msgToMatch->GetCCList(&ccList);
                err = pTerm->MatchRfc822String (nsAutoString(ccList,eOneByte).GetBuffer(), charset);
            }
        }
            break;
        case nsMsgSearchAttrib::Body:
			{
				nsMsgKey messageKey;
				PRUint32 lineCount;
				msgToMatch->GetMessageKey(&messageKey);
				msgToMatch->GetLineCount(&lineCount);
	            err = pTerm->MatchBody (scope, messageKey, lineCount, charset, msgToMatch, db);
			}
            break;
        case nsMsgSearchAttrib::Date:
			{
				PRTime date;
				msgToMatch->GetDate(&date);
				err = pTerm->MatchDate (date);
			}
            break;
        case nsMsgSearchAttrib::MsgStatus:
            err = pTerm->MatchStatus (msgFlags);
            break;
        case nsMsgSearchAttrib::Priority:
			{
				nsMsgPriority msgPriority;
				msgToMatch->GetPriority(&msgPriority);
				err = pTerm->MatchPriority (msgPriority);
			}
            break;
        case nsMsgSearchAttrib::Size:
			{
				PRUint32 messageSize;
				msgToMatch->GetMessageSize(&messageSize);
				err = pTerm->MatchSize (messageSize);
			}
            break;
        case nsMsgSearchAttrib::To:
            msgToMatch->GetRecipients(&recipients);
            err = pTerm->MatchRfc822String(nsAutoString(recipients,eOneByte).GetBuffer(), charset);
            break;
        case nsMsgSearchAttrib::CC:
            msgToMatch->GetCCList(&ccList);
            err = pTerm->MatchRfc822String (nsAutoString(ccList,eOneByte).GetBuffer(), charset);
            break;
        case nsMsgSearchAttrib::AgeInDays:
			{
				PRTime date;
				msgToMatch->GetDate(&date);
	            err = pTerm->MatchAge (date);
			}
            break;
        case nsMsgSearchAttrib::OtherHeader:
			{
				PRUint32 lineCount;
				msgToMatch->GetLineCount(&lineCount);
				nsMsgKey messageKey;
				msgToMatch->GetMessageKey(&messageKey);
            err = pTerm->MatchArbitraryHeader (scope, messageKey, lineCount,charset, 
                                                msgToMatch, db, headers, headerSize, Filtering);
			}
            break;

        default:
            err = NS_ERROR_INVALID_ARG; // ### was SearchError_InvalidAttribute
        }

        if (expression && (err == NS_OK || err == NS_COMFALSE))
            expression = expression->AddSearchTerm(pTerm, (err == NS_OK));    // added the term and its value to the expression tree
        else
            return NS_ERROR_OUT_OF_MEMORY;
    }
    PRBool result = expression->OfflineEvaluate();
    delete expression;
    return result ? NS_OK : NS_COMFALSE;
}


nsresult nsMsgSearchOfflineMail::Search ()
{
    nsresult err = NS_OK;
#ifdef HAVE_SEARCH_PORT
    nsIMsgDBHdr *pHeaders = nsnull;

    nsresult dbErr = NS_OK;

    // If we need to parse the mailbox before searching it, give another time
    // slice to the parser
    if (m_mailboxParser)
        err = BuildSummaryFile ();
    else
        // Try to open the DB lazily. This will set up a parser if one is required
        if (!m_db)
            err = OpenSummaryFile ();
    // Reparsing is unnecessary or completed
    if (m_mailboxParser == nsnull && err == NS_OK)
    {
        NS_ASSERTION (m_db, "unable to open db for search");

        if (!m_listContext)
            dbErr = m_db->ListFirst (&m_listContext, &pHeaders);
        else
            dbErr = m_db->ListNext (m_listContext, &pHeaders);
        if (eSUCCESS != dbErr)      
            err = SearchError_ScopeDone; //###phil dbErr is dropped on the floor. just note that we did have an error so we'll clean up later
        else
        {
            // Is this message a hit?
            err = MatchTermsForSearch (pHeaders, m_searchTerms, m_scope, m_db);

            // Add search hits to the results list
            if (NS_OK == err)
                AddResultElement (pHeaders);

            m_scope->m_frame->IncrementOfflineProgress();

            // Unrefer the header because holding it would hold the DB
            // open which would prevent the user from moving, renaming, or deleting 
            // the summary file
            if (nsnull != pHeaders)
                pHeaders->unrefer();
        }

    }
    else
        err = SearchError_ScopeDone; // we couldn't open up the DB. This is an unrecoverable error so mark the scope as done.

    // in the past an error here would cause an "infinite" search because the url would continue to run...
    // i.e. if we couldn't open the database, it returns an error code but the caller of this function says, oh,
    // we did not finish so continue...what we really want is to treat this current scope as done
    if (err == SearchError_ScopeDone)
        CleanUpScope(); // Do clean up for end-of-scope processing
#endif // HAVE_SEARCH_PORT
    return err;
}

void nsMsgSearchOfflineMail::CleanUpScope()
{
#ifdef HAVE_SEARCH_PORT
    // Let go of the DB when we're done with it so we don't kill the db cache
    if (m_db)
    {
        m_db->ListDone (m_listContext);
        m_db->Close();
    }
    
    m_db = nsnull;

    // If we were searching the body of the message, close the folder
    if (m_scope->m_file)
        XP_FileClose (m_scope->m_file);
    
    m_scope->m_file = nsnull;
#endif // HAVE_SEARCH_PORT
}

nsresult nsMsgSearchOfflineMail::AddResultElement (nsIMsgDBHdr *pHeaders)
{
    nsresult err = NS_OK;

    nsMsgResultElement *newResult = new nsMsgResultElement (this);

    if (newResult)
    {
        NS_ASSERTION (newResult, "out of memory adding search result");

        // This isn't very general. Just add the headers we think we'll be interested in
        // to the list of attributes per result element.
        nsMsgSearchValue *pValue = new nsMsgSearchValue;
        if (pValue)
        {
            nsString subject;
			PRUint32 msgFlags;

			// Don't even bother to look at expunged messages awaiting compression
			pHeaders->GetFlags(&msgFlags);
            pValue->attribute = nsMsgSearchAttrib::Subject;
            char *reString = (msgFlags & MSG_FLAG_HAS_RE) ? "Re: " : "";
            pHeaders->GetSubject(&subject);
            pValue->u.string = PR_smprintf ("%s%s", reString, (const char*) nsAutoCString(subject)); // hack. invoke cast operator by force
            newResult->AddValue (pValue);
        }
        pValue = new nsMsgSearchValue;
        if (pValue)
        {
            pValue->attribute = nsMsgSearchAttrib::Sender;
			nsString author;
            pHeaders->GetAuthor(&author);
			pValue->u.string = PL_strdup((const char *) nsAutoCString(author));
            newResult->AddValue (pValue);
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
            pValue->attribute = nsMsgSearchAttrib::Location;
#ifdef HAVE_SEARCH_PORT
            pValue->u.string = PL_strdup(m_scope->m_folder->GetName());
#endif
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
//        m_scope->m_frame->AddResultElement (newResult);
    }
    return err;
}

NS_IMETHODIMP
nsMsgSearchOfflineMail::Abort ()
{
    // Let go of the DB when we're done with it so we don't kill the db cache
    if (m_db)
        m_db->Close(PR_TRUE /* commit in case we downloaded new headers */);
    m_db = nsnull;

    // If we got aborted in the middle of parsing a mail folder, we should
    // free the parser object (esp. so it releases the folderInfo's semaphore)
    if (m_mailboxParser)
        delete m_mailboxParser;
    m_mailboxParser = nsnull;

    return nsMsgSearchAdapter::Abort ();
}

