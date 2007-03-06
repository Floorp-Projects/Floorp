/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Howard Chu <hyc@symas.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Implementation of db search for POP and offline IMAP mail folders

#include "msgCore.h"
#include "nsIMsgDatabase.h"
#include "nsMsgSearchCore.h"
#include "nsMsgLocalSearch.h"
#include "nsIStreamListener.h"
#include "nsMsgSearchBoolExpression.h"
#include "nsMsgSearchTerm.h"
#include "nsMsgResultElement.h"
#include "nsIDBFolderInfo.h"

#include "nsMsgBaseCID.h"
#include "nsMsgSearchValue.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsIMsgWindow.h"
#include "nsIFileSpec.h"
#include "nsIMsgHdr.h"

extern "C"
{
    extern int MK_MSG_SEARCH_STATUS;
    extern int MK_MSG_CANT_SEARCH_IF_NO_SUMMARY;
    extern int MK_MSG_SEARCH_HITS_NOT_IN_DB;
}


//----------------------------------------------------------------------------
// Class definitions for the boolean expression structure....
//----------------------------------------------------------------------------


nsMsgSearchBoolExpression * nsMsgSearchBoolExpression::AddSearchTerm(nsMsgSearchBoolExpression * aOrigExpr, nsIMsgSearchTerm * aNewTerm, char * aEncodingStr)
// appropriately add the search term to the current expression and return a pointer to the
// new expression. The encodingStr is the IMAP/NNTP encoding string for newTerm.
{
    return aOrigExpr->leftToRightAddTerm(aNewTerm, aEncodingStr);
}

nsMsgSearchBoolExpression * nsMsgSearchBoolExpression::AddExpressionTree(nsMsgSearchBoolExpression * aOrigExpr, nsMsgSearchBoolExpression * aExpression, PRBool aBoolOp)
{
  if (!aOrigExpr->m_term && !aOrigExpr->m_leftChild && !aOrigExpr->m_rightChild)
  {
      // just use the original expression tree...
      // delete the original since we have a new original to use
      delete aOrigExpr;
      return aExpression;
  }

  nsMsgSearchBoolExpression * newExpr = new nsMsgSearchBoolExpression (aOrigExpr, aExpression, aBoolOp);  
  return (newExpr) ? newExpr : aOrigExpr;
}

nsMsgSearchBoolExpression::nsMsgSearchBoolExpression() 
{
    m_term = nsnull;
    m_boolOp = nsMsgSearchBooleanOp::BooleanAND;
    m_leftChild = nsnull;
    m_rightChild = nsnull;
}

nsMsgSearchBoolExpression::nsMsgSearchBoolExpression (nsIMsgSearchTerm * newTerm, char * encodingStr) 
// we are creating an expression which contains a single search term (newTerm) 
// and the search term's IMAP or NNTP encoding value for online search expressions AND
// a boolean evaluation value which is used for offline search expressions.
{
    m_term = newTerm;
    m_encodingStr = encodingStr;
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
}

nsMsgSearchBoolExpression::~nsMsgSearchBoolExpression()
{
  // we must recursively destroy all sub expressions before we destroy ourself.....We leave search terms alone!
  delete m_leftChild;
  delete m_rightChild;
}

nsMsgSearchBoolExpression *
nsMsgSearchBoolExpression::leftToRightAddTerm(nsIMsgSearchTerm * newTerm, char * encodingStr)
{
    // we have a base case where this is the first term being added to the expression:
    if (!m_term && !m_leftChild && !m_rightChild)
    {
        m_term = newTerm;
        m_encodingStr = encodingStr;
        return this;
    }

    nsMsgSearchBoolExpression * tempExpr = new nsMsgSearchBoolExpression (newTerm,encodingStr);
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


// returns PR_TRUE or PR_FALSE depending on what the current expression evaluates to. 
PRBool nsMsgSearchBoolExpression::OfflineEvaluate(nsIMsgDBHdr *msgToMatch, const char *defaultCharset,
  nsIMsgSearchScopeTerm *scope, nsIMsgDatabase *db, const char *headers,
  PRUint32 headerSize, PRBool Filtering)
{
    PRBool result = PR_TRUE;    // always default to false positives
    PRBool isAnd;

    if (m_term) // do we contain just a search term?
    {
      nsMsgSearchOfflineMail::ProcessSearchTerm(msgToMatch, m_term,
        defaultCharset, scope, db, headers, headerSize, Filtering, &result);
      return result;
    }
    
    // otherwise we must recursively determine the value of our sub expressions

    isAnd = (m_boolOp == nsMsgSearchBooleanOp::BooleanAND);

    if (m_leftChild)
    {
        result = m_leftChild->OfflineEvaluate(msgToMatch, defaultCharset,
          scope, db, headers, headerSize, Filtering);
        // If (TRUE and OR) or (FALSE and AND) return result
        if (result ^ isAnd)
          return result;
    }

    // If we got this far, either there was no leftChild (which is impossible)
    // or we got (FALSE and OR) or (TRUE and AND) from the first result. That
    // means the outcome depends entirely on the rightChild.
    if (m_rightChild)
        result = m_rightChild->OfflineEvaluate(msgToMatch, defaultCharset,
          scope, db, headers, headerSize, Filtering);

    return result;
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


//-----------------------------------------------------------------------------
//---------------- Adapter class for searching offline folders ----------------
//-----------------------------------------------------------------------------


NS_IMPL_ISUPPORTS_INHERITED1(nsMsgSearchOfflineMail, nsMsgSearchAdapter, nsIUrlListener)

nsMsgSearchOfflineMail::nsMsgSearchOfflineMail (nsIMsgSearchScopeTerm *scope, nsISupportsArray *termList) : nsMsgSearchAdapter (scope, termList)
{
}

nsMsgSearchOfflineMail::~nsMsgSearchOfflineMail ()
{
  // Database should have been closed when the scope term finished. 
  CleanUpScope();
  NS_ASSERTION(!m_db, "db not closed");
}


nsresult nsMsgSearchOfflineMail::ValidateTerms ()
{
  return nsMsgSearchAdapter::ValidateTerms ();
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
      err = scopeFolder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(m_db));
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


nsresult
nsMsgSearchOfflineMail::MatchTermsForFilter(nsIMsgDBHdr *msgToMatch,
                                            nsISupportsArray *termList,
                                            const char *defaultCharset,
                                            nsIMsgSearchScopeTerm * scope,
                                            nsIMsgDatabase * db, 
                                            const char * headers,
                                            PRUint32 headerSize,
                                            nsMsgSearchBoolExpression ** aExpressionTree,
                                            PRBool *pResult)
{
    return MatchTerms(msgToMatch, termList, defaultCharset, scope, db, headers, headerSize, PR_TRUE, aExpressionTree, pResult);
}

// static method which matches a header against a list of search terms.
nsresult
nsMsgSearchOfflineMail::MatchTermsForSearch(nsIMsgDBHdr *msgToMatch, 
                                            nsISupportsArray* termList,
                                            const char *defaultCharset,
                                            nsIMsgSearchScopeTerm *scope,
                                            nsIMsgDatabase *db,
                                            nsMsgSearchBoolExpression ** aExpressionTree,
                                            PRBool *pResult)
{

    return MatchTerms(msgToMatch, termList, defaultCharset, scope, db, nsnull, 0, PR_FALSE, aExpressionTree, pResult);
}

nsresult nsMsgSearchOfflineMail::ConstructExpressionTree(nsISupportsArray * termList,
                                            PRUint32 termCount,
                                            PRUint32 &aStartPosInList,
                                            nsMsgSearchBoolExpression ** aExpressionTree)
{
  nsMsgSearchBoolExpression * finalExpression = *aExpressionTree;

  if (!finalExpression)
    finalExpression = new nsMsgSearchBoolExpression(); 

  while (aStartPosInList < termCount)
  {
      nsCOMPtr<nsIMsgSearchTerm> pTerm;
      termList->QueryElementAt(aStartPosInList, NS_GET_IID(nsIMsgSearchTerm), (void **)getter_AddRefs(pTerm));
      NS_ASSERTION (pTerm, "couldn't get term to match");

      PRBool beginsGrouping;
      PRBool endsGrouping;
      pTerm->GetBeginsGrouping(&beginsGrouping);
      pTerm->GetEndsGrouping(&endsGrouping);

      if (beginsGrouping)
      {
          //temporarily turn off the grouping for our recursive call
          pTerm->SetBeginsGrouping(PR_FALSE); 
          nsMsgSearchBoolExpression * innerExpression = new nsMsgSearchBoolExpression(); 

          // the first search term in the grouping is the one that holds the operator for how this search term
          // should be joined with the expressions to it's left. 
          PRBool booleanAnd;
          pTerm->GetBooleanAnd(&booleanAnd);

          // now add this expression tree to our overall expression tree...
          finalExpression = nsMsgSearchBoolExpression::AddExpressionTree(finalExpression, innerExpression, booleanAnd);

          // recursively process this inner expression
          ConstructExpressionTree(termList, termCount, aStartPosInList, 
            &finalExpression->m_rightChild);

          // undo our damage
          pTerm->SetBeginsGrouping(PR_TRUE); 

      }
      else
      {
        finalExpression = nsMsgSearchBoolExpression::AddSearchTerm(finalExpression, pTerm, nsnull);    // add the term to the expression tree

        if (endsGrouping)
          break;
      }

      aStartPosInList++;
  } // while we still have terms to process in this group

  *aExpressionTree = finalExpression;

  return NS_OK; 
}

nsresult nsMsgSearchOfflineMail::ProcessSearchTerm(nsIMsgDBHdr *msgToMatch,
                                            nsIMsgSearchTerm * aTerm,
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
    PRBool matchAll;

    NS_ENSURE_ARG_POINTER(pResult);

    aTerm->GetMatchAll(&matchAll);
    if (matchAll)
    {
      *pResult = PR_TRUE;
      return NS_OK;
    }
    *pResult = PR_FALSE;

    nsMsgSearchAttribValue attrib;
    aTerm->GetAttrib(&attrib);
    msgToMatch->GetCharset(getter_Copies(msgCharset));
    charset = (const char*)msgCharset;
    if (!charset || !*charset)
      charset = (const char*)defaultCharset;
    msgToMatch->GetFlags(&msgFlags);

    switch (attrib)
    {
      case nsMsgSearchAttrib::Sender:
        msgToMatch->GetAuthor(getter_Copies(matchString));
        err = aTerm->MatchRfc822String (matchString, charset, charsetOverride, &result);
        break;
      case nsMsgSearchAttrib::Subject:
      {
        msgToMatch->GetSubject(getter_Copies(matchString) /* , PR_TRUE */);
        if (msgFlags & MSG_FLAG_HAS_RE)
        { 
          // Make sure we pass along the "Re: " part of the subject if this is a reply.
          nsXPIDLCString reString;
          reString.Assign("Re: ");
          reString.Append(matchString);
          err = aTerm->MatchRfc2047String(reString, charset, charsetOverride, &result);
        }
        else
          err = aTerm->MatchRfc2047String (matchString, charset, charsetOverride, &result);
        break;
      }
      case nsMsgSearchAttrib::ToOrCC:
      {
        PRBool boolKeepGoing;
        aTerm->GetMatchAllBeforeDeciding(&boolKeepGoing);
        msgToMatch->GetRecipients(getter_Copies(recipients));
        err = aTerm->MatchRfc822String (recipients, charset, charsetOverride, &result);
        if (boolKeepGoing == result)
        {
          msgToMatch->GetCcList(getter_Copies(ccList));
          err = aTerm->MatchRfc822String (ccList, charset, charsetOverride, &result);
        }
        break;
      }
      case nsMsgSearchAttrib::Body:
       {
         nsMsgKey messageOffset;
         PRUint32 lineCount;
         msgToMatch->GetMessageOffset(&messageOffset);
         msgToMatch->GetLineCount(&lineCount);
         err = aTerm->MatchBody (scope, messageOffset, lineCount, charset, msgToMatch, db, &result);
         break;
       }
      case nsMsgSearchAttrib::Date:
      {
        PRTime date;
        msgToMatch->GetDate(&date);
        err = aTerm->MatchDate (date, &result);

        break;
      }
      case nsMsgSearchAttrib::HasAttachmentStatus:
      case nsMsgSearchAttrib::MsgStatus:
        err = aTerm->MatchStatus (msgFlags, &result);
        break;
      case nsMsgSearchAttrib::Priority:
      {
        nsMsgPriorityValue msgPriority;
        msgToMatch->GetPriority(&msgPriority);
        err = aTerm->MatchPriority (msgPriority, &result);
        break;
      }      
      case nsMsgSearchAttrib::Size:
      {
        PRUint32 messageSize;
        msgToMatch->GetMessageSize(&messageSize);
        err = aTerm->MatchSize (messageSize, &result);
        break;
      }
      case nsMsgSearchAttrib::To:
        msgToMatch->GetRecipients(getter_Copies(recipients));
        err = aTerm->MatchRfc822String(recipients, charset, charsetOverride, &result);
        break;
      case nsMsgSearchAttrib::CC:
        msgToMatch->GetCcList(getter_Copies(ccList));
        err = aTerm->MatchRfc822String (ccList, charset, charsetOverride, &result);
        break;
      case nsMsgSearchAttrib::AgeInDays:
      {
        PRTime date;
        msgToMatch->GetDate(&date);
        err = aTerm->MatchAge (date, &result);
        break;
       }
      case nsMsgSearchAttrib::Label:
      {
         nsMsgLabelValue label;
         msgToMatch->GetLabel(&label);
         err = aTerm->MatchLabel(label, &result);
         break;
      }    
      case nsMsgSearchAttrib::Keywords:
      {
          nsXPIDLCString keywords;
          nsMsgLabelValue label;
          msgToMatch->GetStringProperty("keywords", getter_Copies(keywords));
          msgToMatch->GetLabel(&label);
          if (label >= 1)
          {
            if (!keywords.IsEmpty())
              keywords.Append(' ');
            keywords.Append("$label");
            keywords.Append(label + '0');
          }
          err = aTerm->MatchKeyword(keywords.get(), &result);
          break;
      }
      case nsMsgSearchAttrib::JunkStatus:
      {
         nsXPIDLCString junkScoreStr;
         msgToMatch->GetStringProperty("junkscore", getter_Copies(junkScoreStr));
         err = aTerm->MatchJunkStatus(junkScoreStr, &result);
         break; 
      }
      default:
          // XXX todo
          // for the temporary return receipts filters, we use a custom header for Content-Type
          // but unlike the other custom headers, this one doesn't show up in the search / filter
          // UI.  we set the attrib to be nsMsgSearchAttrib::OtherHeader, where as for user
          // defined custom headers start at nsMsgSearchAttrib::OtherHeader + 1
          // Not sure if there is a better way to do this yet.  Maybe reserve the last
          // custom header for ::Content-Type?  But if we do, make sure that change
          // doesn't cause nsMsgFilter::GetTerm() to change, and start making us
          // ask IMAP servers for the Content-Type header on all messages.
          if ( attrib >= nsMsgSearchAttrib::OtherHeader && attrib < nsMsgSearchAttrib::kNumMsgSearchAttributes)
          {
            PRUint32 lineCount;
            msgToMatch->GetLineCount(&lineCount);
            nsMsgKey messageKey;
            msgToMatch->GetMessageOffset(&messageKey);
            err = aTerm->MatchArbitraryHeader (scope, messageKey, lineCount,charset, charsetOverride,
                                                msgToMatch, db, headers, headerSize, Filtering, &result);
          }
          else
            err = NS_ERROR_INVALID_ARG; // ### was SearchError_InvalidAttribute
    }

    *pResult = result;
    return NS_OK;
}

nsresult nsMsgSearchOfflineMail::MatchTerms(nsIMsgDBHdr *msgToMatch,
                                            nsISupportsArray * termList,
                                            const char *defaultCharset,
                                            nsIMsgSearchScopeTerm * scope,
                                            nsIMsgDatabase * db, 
                                            const char * headers,
                                            PRUint32 headerSize,
                                            PRBool Filtering,
                                            nsMsgSearchBoolExpression ** aExpressionTree,
                                            PRBool *pResult) 
{
  NS_ENSURE_ARG(aExpressionTree);
  nsresult err;

  if (!*aExpressionTree)
  {
    PRUint32 initialPos = 0; 
    PRUint32 count;
    termList->Count(&count);
    err = ConstructExpressionTree(termList, count, initialPos, aExpressionTree);
    if (NS_FAILED(err))
      return err;
  }

  // evaluate the expression tree and return the result
  *pResult = (*aExpressionTree) 
    ?  (*aExpressionTree)->OfflineEvaluate(msgToMatch,
                 defaultCharset, scope, db, headers, headerSize, Filtering)
    :PR_TRUE;	// vacuously true...

  return NS_OK;
}


nsresult nsMsgSearchOfflineMail::Search (PRBool *aDone)
{
  nsresult err = NS_OK;
  
  NS_ENSURE_ARG(aDone);
  nsresult dbErr = NS_OK;
  nsCOMPtr<nsIMsgDBHdr> msgDBHdr;
  nsMsgSearchBoolExpression *expressionTree = nsnull;
  
  const PRUint32 kTimeSliceInMS = 200;

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
      PRIntervalTime startTime = PR_IntervalNow();
      while (!*aDone)  // we'll break out of the loop after kTimeSliceInMS milliseconds
      {
        nsCOMPtr<nsISupports> currentItem;
      
        dbErr = m_listContext->GetNext(getter_AddRefs(currentItem));
        if(NS_SUCCEEDED(dbErr))
        {
          msgDBHdr = do_QueryInterface(currentItem, &dbErr);
        }
        if (NS_FAILED(dbErr))      
          *aDone = PR_TRUE; //###phil dbErr is dropped on the floor. just note that we did have an error so we'll clean up later
        else
        {
          PRBool match = PR_FALSE;
          nsXPIDLString nullCharset, folderCharset;
          GetSearchCharsets(getter_Copies(nullCharset), getter_Copies(folderCharset));
          NS_ConvertUTF16toUTF8 charset(folderCharset);
          // Is this message a hit?
          err = MatchTermsForSearch (msgDBHdr, m_searchTerms, charset.get(), m_scope, m_db, &expressionTree, &match);
          // Add search hits to the results list
          if (NS_SUCCEEDED(err) && match)
          {
            AddResultElement (msgDBHdr);
          }
          PRIntervalTime elapsedTime;
          LL_SUB(elapsedTime, PR_IntervalNow(), startTime);
          // check if more than kTimeSliceInMS milliseconds have elapsed in this time slice started
          if (PR_IntervalToMilliseconds(elapsedTime) > kTimeSliceInMS)
            break;
        }
      }
    }    
  }
  else
    *aDone = PR_TRUE; // we couldn't open up the DB. This is an unrecoverable error so mark the scope as done.
  
  delete expressionTree;

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
  m_scope->SetInputStream(nsnull);
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
  if (m_scope)
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
  // code here used to check if offline store existed, which breaks offline news.
  if (NS_SUCCEEDED(err) && scopeFolder)
    err = scopeFolder->GetMsgDatabase(nsnull, getter_AddRefs(m_db));
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


