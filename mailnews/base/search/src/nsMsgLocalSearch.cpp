/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIDBFolderInfo.h"

#include "nsMsgBaseCID.h"
#include "nsMsgSearchValue.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsIMsgWindow.h"
#include "nsIFileSpec.h"

static NS_DEFINE_CID(kValidityManagerCID, NS_MSGSEARCHVALIDITYMANAGER_CID);

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
    m_evalValue = PR_FALSE;
    m_leftChild = nsnull;
    m_rightChild = nsnull;
}

nsMsgSearchBoolExpression::nsMsgSearchBoolExpression (nsIMsgSearchTerm * newTerm, PRBool evalValue, char * encodingStr) 
// we are creating an expression which contains a single search term (newTerm) 
// and the search term's IMAP or NNTP encoding value for online search expressions AND
// a boolean evaluation value which is used for offline search expressions.
{
    m_term = newTerm;
    m_encodingStr = encodingStr;
    m_evalValue = evalValue;
    m_boolOp = nsMsgSearchBooleanOp::BooleanAND;

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
    m_evalValue = PR_FALSE;
}

nsMsgSearchBoolExpression::~nsMsgSearchBoolExpression()
{
    // we must recursively destroy all sub expressions before we destroy ourself.....We leave search terms alone!
    if (m_leftChild)
        delete m_leftChild;
    if (m_rightChild)
        delete m_rightChild;
}

nsMsgSearchBoolExpression *
nsMsgSearchBoolExpression::AddSearchTerm(nsIMsgSearchTerm * newTerm, char * encodingStr)
// appropriately add the search term to the current expression and return a pointer to the
// new expression. The encodingStr is the IMAP/NNTP encoding string for newTerm.
{
    return leftToRightAddTerm(newTerm,PR_FALSE,encodingStr);
}

nsMsgSearchBoolExpression *
nsMsgSearchBoolExpression::AddSearchTerm(nsIMsgSearchTerm * newTerm, PRBool evalValue)
// appropriately add the search term to the current expression
// Returns: a pointer to the new expression which includes this new search term
{
    return leftToRightAddTerm(newTerm, evalValue,nsnull);   // currently we build our expressions to
                                                          // evaluate left to right.
}

nsMsgSearchBoolExpression *
nsMsgSearchBoolExpression::leftToRightAddTerm(nsIMsgSearchTerm * newTerm, PRBool evalValue, char * encodingStr)
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
      PRBool booleanAnd;
      newTerm->GetBooleanAnd(&booleanAnd);
        nsMsgSearchBoolExpression * newExpr = new nsMsgSearchBoolExpression (this, tempExpr, booleanAnd);  
        if (newExpr)
            return newExpr;
        else
            delete tempExpr;    // clean up memory allocation in case of failure
    }
    return this;   // in case we failed to create a new expression, return self
}


PRBool nsMsgSearchBoolExpression::OfflineEvaluate()
// returns PR_TRUE or PR_FALSE depending on what the current expression evaluates to. Since this is
// offline, when we created the expression we stored an evaluation value for each search term in 
// the expression. These are the values we use to determine if the expression is PR_TRUE or PR_FALSE.
{
    if (m_term) // do we contain just a search term?
        return m_evalValue;
    
    // otherwise we must recursively determine the value of our sub expressions
    PRBool result1 = PR_TRUE;    // always default to false positives
    PRBool result2 = PR_TRUE;

    if (m_leftChild)
    {
        result1 = m_leftChild->OfflineEvaluate();
        if (m_boolOp == nsMsgSearchBooleanOp::BooleanOR && result1 ||
           (m_boolOp == nsMsgSearchBooleanOp::BooleanAND && !result1))
            return result1;
    }

    if (m_rightChild)
        result2 = m_rightChild->OfflineEvaluate();

    if (m_boolOp == nsMsgSearchBooleanOp::BooleanOR)
        return (result1 || result2) ? PR_TRUE : PR_FALSE;

    if (m_boolOp == nsMsgSearchBooleanOp::BooleanAND && result1 && result2) 
        return PR_TRUE;

    return PR_FALSE;
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


void nsMsgSearchBoolExpression::GenerateEncodeStr(nsCString * buffer)
// recurively combine sub expressions to form a single IMAP/NNTP encoded string 
{
    if ((!m_term && (!m_leftChild || !m_rightChild))) // is expression empty?
        return;
    
    if (m_term) // are we a leaf expression?
    {
        *buffer += m_encodingStr;
        return;
    }
    
    // add encode strings of each sub expression
    if (m_boolOp == nsMsgSearchBooleanOp::BooleanOR) 
    {
        *buffer += " (OR";

        m_leftChild->GenerateEncodeStr(buffer);  // insert left expression into the buffer
        m_rightChild->GenerateEncodeStr(buffer);  // insert right expression into the buffer
        
        // HACK ALERT!!! if last returned character in the buffer is now a ' ' then we need to remove it because we don't want
        // a ' ' to preceded the closing paren in the OR encoding.
        PRUint32 lastCharPos = buffer->Length() - 1;
        if (buffer->CharAt(lastCharPos) == ' ')
		{
            buffer->Truncate(lastCharPos);
		}
        
        *buffer += ')';
    }
    else if (m_boolOp == nsMsgSearchBooleanOp::BooleanAND)
    {
        m_leftChild->GenerateEncodeStr(buffer); // insert left expression
        m_rightChild->GenerateEncodeStr(buffer);
    }
    return;
}



//---------------- Adapter class for searching offline IMAP folders -----------
//-----------------------------------------------------------------------------

nsMsgSearchIMAPOfflineMail::nsMsgSearchIMAPOfflineMail (nsIMsgSearchScopeTerm *scope, nsISupportsArray  *termList) : nsMsgSearchOfflineMail(scope, termList)
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
            nsCOMPtr<nsIMsgValidityManager> validityManager =
              do_GetService(kValidityManagerCID, &err);

            NS_ENSURE_SUCCESS(rv, rv);
            
            nsCOMPtr<nsIMsgSearchValidityTable> table;
            err = validityManager->GetTable (nsMsgSearchValidityManager::offlineMail,
                                             getter_AddRefs(table));
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


NS_IMPL_ISUPPORTS_INHERITED1(nsMsgSearchOfflineMail, nsMsgSearchAdapter, nsIUrlListener)

nsMsgSearchOfflineMail::nsMsgSearchOfflineMail (nsIMsgSearchScopeTerm *scope, nsISupportsArray *termList) : nsMsgSearchAdapter (scope, termList)
{
    m_db = nsnull;
    m_listContext = nsnull;
}

nsMsgSearchOfflineMail::~nsMsgSearchOfflineMail ()
{
    // Database should have been closed when the scope term finished. 
    CleanUpScope();
    NS_ASSERTION(!m_db, "db not closed");
}


nsresult nsMsgSearchOfflineMail::ValidateTerms ()
{
    nsresult err = NS_OK;
	err = nsMsgSearchAdapter::ValidateTerms ();
#ifdef HAVE_SEARCH_PORT
    if (NS_OK == err)
    {
        // Mail folder must exist. Don't worry about the summary file now since we may
        // have to regenerate the index later
        XP_StatStruct fileStatus;
        if (!XP_Stat (m_scope->GetMailPath(), &fileStatus, xpMailFolder))
        {
            // Make sure the terms themselves are valid
            nsCOMPtr<nsIMsgValidityManager> validityManager =
              do_GetService(kValidityManagerCID, &err);

            NS_ENSURE_SUCCESS(rv, rv);
            
            nsCOMPtr<nsIMsgSearchValidityTable> table;
            err = validityManager->GetTable(nsMsgSearchValidityManager::offlineMail,
                                            getter_AddRefs(table));
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
    // do password protection of local cache thing.
#ifdef DOING_FOLDER_CACHE_PASSWORDS
    if (m_scope->m_folder && m_scope->m_folder->UserNeedsToAuthenticateForFolder(PR_FALSE) && m_scope->m_folder->GetMaster()->PromptForHostPassword(m_scope->m_frame->GetContext(), m_scope->m_folder) != 0)
    {
        m_scope->m_frame->StopRunning();
        return SearchError_ScopeDone;
    }
#endif
    nsCOMPtr <nsIDBFolderInfo>  folderInfo;
    nsCOMPtr <nsIMsgFolder> scopeFolder;
    err = m_scope->GetFolder(getter_AddRefs(scopeFolder));
    if (NS_SUCCEEDED(err) && scopeFolder)
    {
      err = scopeFolder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), &m_db);
    }
    else
      return err; // not sure why m_folder wouldn't be set.

    switch (err)
    {
        case NS_OK:
            break;
        case NS_MSG_ERROR_FOLDER_SUMMARY_MISSING:
        case NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE:
          {
            nsCOMPtr<nsIMsgLocalMailFolder> localFolder = do_QueryInterface(scopeFolder, &err);
            if (NS_SUCCEEDED(err) && localFolder)
            {
              nsCOMPtr<nsIMsgSearchSession> searchSession;
              m_scope->GetSearchSession(getter_AddRefs(searchSession));
              if (searchSession)
              {
                nsCOMPtr <nsIMsgWindow> searchWindow;

                searchSession->GetWindow(getter_AddRefs(searchWindow));
                searchSession->PauseSearch();
                localFolder->ParseFolder(searchWindow, this);
              }
            }
          }
            break;
        default:
        {
          NS_ASSERTION(PR_FALSE, "unexpected error opening db");
        }
    }

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

nsresult
nsMsgSearchOfflineMail::MatchTermsForFilter(nsIMsgDBHdr *msgToMatch,
                                            nsISupportsArray *termList,
                                            const char *defaultCharset,
                                            nsIMsgSearchScopeTerm * scope,
                                            nsIMsgDatabase * db, 
                                            const char * headers,
                                            PRUint32 headerSize,
                                            PRBool *pResult)
{
    return MatchTerms(msgToMatch, termList, defaultCharset, scope, db, headers, headerSize, PR_TRUE, pResult);
}

// static method which matches a header against a list of search terms.
nsresult
nsMsgSearchOfflineMail::MatchTermsForSearch(nsIMsgDBHdr *msgToMatch, 
                                            nsISupportsArray* termList,
                                            const char *defaultCharset,
                                            nsIMsgSearchScopeTerm *scope,
                                            nsIMsgDatabase *db,
                                            PRBool *pResult)
{

    return MatchTerms(msgToMatch, termList, defaultCharset, scope, db, nsnull, 0, PR_FALSE, pResult);
}

nsresult nsMsgSearchOfflineMail::MatchTerms(nsIMsgDBHdr *msgToMatch,
                                            nsISupportsArray * termList,
                                            const char *defaultCharset,
                                            nsIMsgSearchScopeTerm * scope,
                                            nsIMsgDatabase * db, 
                                            const char * headers,
                                            PRUint32 headerSize,
                                            PRBool Filtering,
											PRBool *pResult) 
{
    nsresult err = NS_OK;
    nsXPIDLCString  recipients;
    nsXPIDLCString  ccList;
    nsXPIDLCString  matchString;
    nsXPIDLCString  msgCharset;
    const char *charset;
    PRBool charsetOverride = PR_FALSE; /* XXX BUG 68706 */
    PRUint32 msgFlags;
    PRBool result;

	if (!pResult)
		return NS_ERROR_NULL_POINTER;

	*pResult = PR_FALSE;

    // Don't even bother to look at expunged messages awaiting compression
    msgToMatch->GetFlags(&msgFlags);
	if (msgFlags & MSG_FLAG_EXPUNGED)
        result = PR_FALSE;

    // Loop over all terms, and match them all to this message. 

    nsMsgSearchBoolExpression * expression = new nsMsgSearchBoolExpression();  // create our expression
    if (!expression)
        return NS_ERROR_OUT_OF_MEMORY;
    PRUint32 count;
    termList->Count(&count);
    for (PRUint32 i = 0; i < count; i++)
    {
        nsCOMPtr<nsIMsgSearchTerm> pTerm;
        termList->QueryElementAt(i, NS_GET_IID(nsIMsgSearchTerm),
                                 (void **)getter_AddRefs(pTerm));
//        NS_ASSERTION (pTerm->IsValid(), "invalid search term");
        NS_ASSERTION (msgToMatch, "couldn't get term to match");

        nsMsgSearchAttribValue attrib;
        pTerm->GetAttrib(&attrib);
        msgToMatch->GetCharset(getter_Copies(msgCharset));
        charset = (const char*)msgCharset;
        if (!charset || !*charset)
          charset = (const char*)defaultCharset;
        switch (attrib)
        {
        case nsMsgSearchAttrib::Sender:
          msgToMatch->GetAuthor(getter_Copies(matchString));
          err = pTerm->MatchRfc822String (matchString, charset, charsetOverride, &result);
          break;
        case nsMsgSearchAttrib::Subject:
          {
            msgToMatch->GetSubject(getter_Copies(matchString) /* , PR_TRUE */);
            err = pTerm->MatchRfc2047String (matchString, charset, charsetOverride, &result);
          }
          break;
        case nsMsgSearchAttrib::ToOrCC:
          {
            PRBool boolKeepGoing;
            pTerm->GetMatchAllBeforeDeciding(&boolKeepGoing);
            msgToMatch->GetRecipients(getter_Copies(recipients));
            err = pTerm->MatchRfc822String (recipients, charset, charsetOverride, &result);
            if (boolKeepGoing == result)
            {
              msgToMatch->GetCcList(getter_Copies(ccList));
              err = pTerm->MatchRfc822String (ccList, charset, charsetOverride, &result);
            }
          }
          break;
        case nsMsgSearchAttrib::Body:
          {
            nsMsgKey messageOffset;
            PRUint32 lineCount;
            msgToMatch->GetMessageOffset(&messageOffset);
            msgToMatch->GetLineCount(&lineCount);
            err = pTerm->MatchBody (scope, messageOffset, lineCount, charset, msgToMatch, db, &result);
          }
          break;
        case nsMsgSearchAttrib::Date:
          {
            PRTime date;
            msgToMatch->GetDate(&date);
            err = pTerm->MatchDate (date, &result);
          }
          break;
        case nsMsgSearchAttrib::MsgStatus:
          err = pTerm->MatchStatus (msgFlags, &result);
          break;
        case nsMsgSearchAttrib::Priority:
          {
            nsMsgPriorityValue msgPriority;
            msgToMatch->GetPriority(&msgPriority);
            err = pTerm->MatchPriority (msgPriority, &result);
          }
          break;
        case nsMsgSearchAttrib::Size:
          {
            PRUint32 messageSize;
            msgToMatch->GetMessageSize(&messageSize);
            err = pTerm->MatchSize (messageSize, &result);
          }
          break;
        case nsMsgSearchAttrib::To:
          msgToMatch->GetRecipients(getter_Copies(recipients));
          err = pTerm->MatchRfc822String(nsCAutoString(recipients), charset, charsetOverride, &result);
          break;
        case nsMsgSearchAttrib::CC:
          msgToMatch->GetCcList(getter_Copies(ccList));
          err = pTerm->MatchRfc822String (nsCAutoString(ccList), charset, charsetOverride, &result);
          break;
        case nsMsgSearchAttrib::AgeInDays:
          {
            PRTime date;
            msgToMatch->GetDate(&date);
            err = pTerm->MatchAge (date, &result);
          }
          break;
        default:
          if ( attrib > nsMsgSearchAttrib::OtherHeader && attrib < nsMsgSearchAttrib::kNumMsgSearchAttributes)
          {
            PRUint32 lineCount;
            msgToMatch->GetLineCount(&lineCount);
            nsMsgKey messageKey;
            msgToMatch->GetMessageOffset(&messageKey);
            err = pTerm->MatchArbitraryHeader (scope, messageKey, lineCount,charset, charsetOverride,
                                                msgToMatch, db, headers, headerSize, Filtering, &result);
          }
          else
            err = NS_ERROR_INVALID_ARG; // ### was SearchError_InvalidAttribute
        }
        if (expression && NS_SUCCEEDED(err))
            expression = expression->AddSearchTerm(pTerm, result);    // added the term and its value to the expression tree
        else
            return NS_ERROR_OUT_OF_MEMORY;
    }
    result = expression->OfflineEvaluate();
    delete expression;
	*pResult = result;
    return err;
}


nsresult nsMsgSearchOfflineMail::Search (PRBool *aDone)
{
  nsresult err = NS_OK;
  
  NS_ENSURE_ARG(aDone);
  nsresult dbErr = NS_OK;
  nsCOMPtr<nsIMsgDBHdr> msgDBHdr;
  
  const PRInt32 kNumHdrsInSlice = 500;


  *aDone = PR_FALSE;
  // Try to open the DB lazily. This will set up a parser if one is required
  if (!m_db)
    err = OpenSummaryFile ();
  if (!m_db)  // must be reparsing.
    return err;
  
  // Reparsing is unnecessary or completed
  if (NS_SUCCEEDED(err))
  {
    if (!m_listContext)
      dbErr = m_db->EnumerateMessages (getter_AddRefs(m_listContext));
    if (NS_SUCCEEDED(dbErr) && m_listContext)
    {
      for (PRInt32 hdrIndex = 0; !*aDone && hdrIndex < kNumHdrsInSlice; hdrIndex++)
      {
        nsCOMPtr<nsISupports> currentItem;
      
        dbErr = m_listContext->GetNext(getter_AddRefs(currentItem));
        if(NS_SUCCEEDED(dbErr))
        {
          msgDBHdr = do_QueryInterface(currentItem, &dbErr);
        }
        if (!NS_SUCCEEDED(dbErr))      
          *aDone = PR_TRUE; //###phil dbErr is dropped on the floor. just note that we did have an error so we'll clean up later
        else
        {
          PRBool match = PR_FALSE;
          nsAutoString nullCharset, folderCharset;
          GetSearchCharsets(nullCharset, folderCharset);
          NS_ConvertUCS2toUTF8 charset(folderCharset);
          // Is this message a hit?
          err = MatchTermsForSearch (msgDBHdr, m_searchTerms, charset.get(), m_scope, m_db, &match);
          // Add search hits to the results list
          if (NS_SUCCEEDED(err) && match)
          {
            AddResultElement (msgDBHdr);
          }
          //      m_scope->m_frame->IncrementOfflineProgress();
        }
      }
    }    
  }
  else
    *aDone = PR_TRUE; // we couldn't open up the DB. This is an unrecoverable error so mark the scope as done.
  
  // in the past an error here would cause an "infinite" search because the url would continue to run...
  // i.e. if we couldn't open the database, it returns an error code but the caller of this function says, oh,
  // we did not finish so continue...what we really want is to treat this current scope as done
  if (*aDone)
    CleanUpScope(); // Do clean up for end-of-scope processing
  return err;
}

void nsMsgSearchOfflineMail::CleanUpScope()
{
    // Let go of the DB when we're done with it so we don't kill the db cache
    if (m_db)
    {
        m_listContext = nsnull; 
        m_db->Close(PR_FALSE);
    }
    
    m_db = nsnull;

    nsCOMPtr <nsIFileSpec> fileSpec;
	nsresult rv = m_scope->GetMailPath(getter_AddRefs(fileSpec));
    PRBool isOpen = PR_FALSE;
	if (NS_SUCCEEDED(rv) && fileSpec)
	{
       fileSpec->IsStreamOpen(&isOpen);
       if (isOpen) 
         fileSpec->CloseStream();    
    }
}

NS_IMETHODIMP nsMsgSearchOfflineMail::AddResultElement (nsIMsgDBHdr *pHeaders)
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
    return err;
}

NS_IMETHODIMP
nsMsgSearchOfflineMail::Abort ()
{
    // Let go of the DB when we're done with it so we don't kill the db cache
    if (m_db)
        m_db->Close(PR_TRUE /* commit in case we downloaded new headers */);
    m_db = nsnull;
    return nsMsgSearchAdapter::Abort ();
}

/* void OnStartRunningUrl (in nsIURI url); */
NS_IMETHODIMP nsMsgSearchOfflineMail::OnStartRunningUrl(nsIURI *url)
{
    return NS_OK;
}

/* void OnStopRunningUrl (in nsIURI url, in nsresult aExitCode); */
NS_IMETHODIMP nsMsgSearchOfflineMail::OnStopRunningUrl(nsIURI *url, nsresult aExitCode)
{
  nsCOMPtr<nsIMsgSearchSession> searchSession;
  m_scope->GetSearchSession(getter_AddRefs(searchSession));
  if (searchSession)
    searchSession->ResumeSearch();

  return NS_OK;
}

nsMsgSearchOfflineNews::nsMsgSearchOfflineNews (nsIMsgSearchScopeTerm *scope, nsISupportsArray *termList) : nsMsgSearchOfflineMail (scope, termList)
{
}


nsMsgSearchOfflineNews::~nsMsgSearchOfflineNews ()
{
}


nsresult nsMsgSearchOfflineNews::OpenSummaryFile ()
{
  nsresult err = NS_OK;
  nsCOMPtr <nsIDBFolderInfo>  folderInfo;
  nsCOMPtr <nsIMsgFolder> scopeFolder;
  err = m_scope->GetFolder(getter_AddRefs(scopeFolder));
  nsCOMPtr <nsIFileSpec> pathSpec;
  err= scopeFolder->GetPath(getter_AddRefs(pathSpec));
  PRBool exists=PR_FALSE;
  err = pathSpec->Exists(&exists);
  if (!exists) return NS_ERROR_FILE_NOT_FOUND;
  if (NS_SUCCEEDED(err) && scopeFolder)
    err = scopeFolder->GetMsgDatabase(nsnull, &m_db);
  return err;
}

nsresult nsMsgSearchOfflineNews::ValidateTerms ()
{
  return nsMsgSearchOfflineMail::ValidateTerms ();
}


//-----------------------------------------------------------------------------
nsresult nsMsgSearchValidityManager::InitLocalNewsTable()
{
	NS_ASSERTION (nsnull == m_localNewsTable, "already have local news validty table");
	nsresult err = NewTable (getter_AddRefs(m_localNewsTable));

	if (NS_SUCCEEDED(err))
	{
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);

		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);

		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Contains, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Contains, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::DoesntContain, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::DoesntContain, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Is, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Is, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Isnt, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Isnt, 1);


		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);

		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan,  1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is,  1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is, 1);

		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);

    m_localNewsTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::BeginsWith, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::BeginsWith, 1);
		m_localNewsTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::EndsWith, 1);
		m_localNewsTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::EndsWith, 1);


	}

	return err;
}


