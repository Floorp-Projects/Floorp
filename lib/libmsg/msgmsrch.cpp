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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// Implementation of search for POP and IMAP mail folders

#include "msg.h"
#include "pmsgsrch.h"

#include "prsembst.h"
#include "maildb.h"
#include "newsdb.h"
#include "grpinfo.h"
#include "msgfinfo.h"
#include "msgmpane.h"
#include "imap.h"
#include "msgprefs.h"
#include "xpgetstr.h"
#include "mailhdr.h"
#include "libi18n.h"
#include "msgimap.h"

extern "C"
{
	extern int MK_MSG_SEARCH_STATUS;
	extern int MK_MSG_CANT_SEARCH_IF_NO_SUMMARY;
	extern int MK_MSG_SEARCH_HITS_NOT_IN_DB;
}


//----------------------------------------------------------------------------
// Class definitions for the boolean expression structure....
//----------------------------------------------------------------------------
CBoolExpression::CBoolExpression()
{
	m_term = NULL;
	m_boolOp = MSG_SearchBooleanAND;
	m_encodingStr = NULL;
	m_evalValue = FALSE;
	m_leftChild = NULL;
	m_rightChild = NULL;
}

CBoolExpression::CBoolExpression (MSG_SearchTerm * newTerm, XP_Bool evalValue, char * encodingStr)
// we are creating an expression which contains a single search term (newTerm) 
// and the search term's IMAP or NNTP encoding value for online search expressions AND
// a boolean evaluation value which is used for offline search expressions.
{
	m_term = newTerm;
	m_encodingStr = encodingStr;
	m_evalValue = evalValue;

	// this expression does not contain sub expressions
	m_leftChild = NULL;
	m_rightChild = NULL;
}


CBoolExpression::CBoolExpression (CBoolExpression * expr1, CBoolExpression * expr2, MSG_SearchBooleanOp boolOp)
// we are creating an expression which contains two sub expressions and a boolean operator used to combine
// them.
{
	m_leftChild = expr1;
	m_rightChild = expr2;
	m_boolOp = boolOp;

	m_term = NULL;
	m_evalValue = FALSE;
}

CBoolExpression::~CBoolExpression()
{
	// we must recursively destroy all sub expressions before we destroy ourself.....We leave search terms alone!
	if (m_leftChild)
		delete m_leftChild;
	if (m_rightChild)
		delete m_rightChild;
}

CBoolExpression * CBoolExpression::AddSearchTerm(MSG_SearchTerm * newTerm, char * encodingStr)
// appropriately add the search term to the current expression and return a pointer to the
// new expression. The encodingStr is the IMAP/NNTP encoding string for newTerm.
{
	return leftToRightAddTerm(newTerm,FALSE,encodingStr);
}

CBoolExpression * CBoolExpression::AddSearchTerm(MSG_SearchTerm * newTerm, XP_Bool evalValue)
// appropriately add the search term to the current expression
// Returns: a pointer to the new expression which includes this new search term
{
	return leftToRightAddTerm(newTerm, evalValue,NULL);	  // currently we build our expressions to
													      // evaluate left to right.
}

CBoolExpression * CBoolExpression::leftToRightAddTerm(MSG_SearchTerm * newTerm, XP_Bool evalValue, char * encodingStr)
{
	// we have a base case where this is the first term being added to the expression:
	if (!m_term && !m_leftChild && !m_rightChild)
	{
		m_term = newTerm;
		m_evalValue = evalValue;
		m_encodingStr = encodingStr;
		return this;
	}

	CBoolExpression * tempExpr = new CBoolExpression (newTerm,evalValue,encodingStr);
	if (tempExpr)  // make sure creation succeeded
	{
		CBoolExpression * newExpr = new CBoolExpression (this, tempExpr, newTerm->GetBooleanOp());  
		if (newExpr)
			return newExpr;
		else
			delete tempExpr;    // clean up memory allocation in case of failure
	}
	return this;   // in case we failed to create a new expression, return self
}


XP_Bool CBoolExpression::OfflineEvaluate()
// returns TRUE or FALSE depending on what the current expression evaluates to. Since this is
// offline, when we created the expression we stored an evaluation value for each search term in 
// the expression. These are the values we use to determine if the expression is TRUE or FALSE.
{
	if (m_term) // do we contain just a search term?
		return m_evalValue;
	
	// otherwise we must recursively determine the value of our sub expressions
	XP_Bool result1 = TRUE;    // always default to false positives
	XP_Bool result2 = TRUE;
	
	if (m_leftChild)
		result1 = m_leftChild->OfflineEvaluate();
	if (m_rightChild)
		result2 = m_rightChild->OfflineEvaluate();

	if (m_boolOp == MSG_SearchBooleanOR)
	{
		if (result1 || result2)
			return TRUE;
	}
	
	if (m_boolOp == MSG_SearchBooleanAND)
	{
		if (result1 && result2)
			return TRUE;
	}

	return FALSE;
}

int32 CBoolExpression::CalcEncodeStrSize()
// recursively examine each sub expression and calculate a final size for the entire IMAP/NNTP encoding 
{
	if (!m_term && (!m_leftChild || !m_rightChild))   // is the expression empty?
		return 0;    
	if (m_term)  // are we a leaf node?
		return XP_STRLEN(m_encodingStr);
	if (m_boolOp == MSG_SearchBooleanOR)
		return sizeOfORTerm + m_leftChild->CalcEncodeStrSize() + m_rightChild->CalcEncodeStrSize();
	if (m_boolOp == MSG_SearchBooleanAND)
		return sizeOfANDTerm + m_leftChild->CalcEncodeStrSize() + m_rightChild->CalcEncodeStrSize();
	return 0;
}


int32 CBoolExpression::GenerateEncodeStr(char * buffer, int32 bufSize)
// recurively combine sub expressions to form a single IMAP/NNTP encoded string 
// assumes buffer[0] == '\0'
// RETURNS: number of bytes copied into the buffer
{
	if (bufSize < CalcEncodeStrSize() || (!m_term && (!m_leftChild || !m_rightChild))) // is expression empty?
		return 0;
	
	if (m_term) // are we a leaf expression?
	{
		XP_STRCAT(buffer, m_encodingStr);
		return XP_STRLEN(m_encodingStr);
	}
	
	// add encode strings of each sub expression
	int32 numBytesAdded = 0;
	if (m_boolOp == MSG_SearchBooleanOR) 
	{
		char * marker = buffer;
		XP_STRCAT(buffer, " (OR");
		buffer[4] = '\0'; // terminate the string
		buffer += 4;      // advance buffer
		bufSize -= 4;     // adjust remaining buffer size

		numBytesAdded = m_leftChild->GenerateEncodeStr(buffer, bufSize);  // insert left expression into the buffer
		buffer += numBytesAdded;
		buffer[0] = '\0';
		bufSize -= numBytesAdded;
		numBytesAdded = m_rightChild->GenerateEncodeStr(buffer, bufSize);  // insert right expression into the buffer
		
		// hack.  If last returned character in the buffer is now a ' ' then we need to remove it because we don't want
		// a ' ' to preceded the closing paren in the OR encoding.
		if (buffer[numBytesAdded-1] == ' ')
			numBytesAdded--;
		
		buffer[numBytesAdded++] = ')';
		buffer[numBytesAdded] = '\0';
		return XP_STRLEN(marker);   // return # bytes we have added to the beginning of the buffer
	}
	
	if (m_boolOp == MSG_SearchBooleanAND)
	{
		char * marker = buffer;
		buffer[0] = '\0';
		numBytesAdded = m_leftChild->GenerateEncodeStr(buffer, bufSize); // insert left expression
		buffer += numBytesAdded;
		bufSize -= numBytesAdded;
		buffer[0] = '\0';

		numBytesAdded = m_rightChild->GenerateEncodeStr(buffer, bufSize);
		buffer += numBytesAdded;
		buffer[numBytesAdded] = '\0';
		return XP_STRLEN(marker);
	}
	return 0;
}



//---------------- Adapter class for searching offline IMAP folders -----------
//-----------------------------------------------------------------------------
msg_SearchIMAPOfflineMail::msg_SearchIMAPOfflineMail (MSG_ScopeTerm *scope, MSG_SearchTermArray &termList) : msg_SearchOfflineMail(scope, termList)
{ 

}                                                                                                                                                                                                                                                                                                                                                                                                                                   


msg_SearchIMAPOfflineMail::~msg_SearchIMAPOfflineMail()
{

}

MSG_SearchError msg_SearchIMAPOfflineMail::ValidateTerms ()
{
	// most of this was copied from MSG_SearchOffline::ValidateTerms()....Difference: When using IMAP offline, we do not
	// have a mail folder to validate.
	
	MSG_SearchError err = msg_SearchAdapter::ValidateTerms ();
	if (SearchError_Success == err)
	{
		// Mail folder must exist. Don't worry about the summary file now since we may
		// have to regenerate the index later
//		XP_StatStruct fileStatus;
//		if (!XP_Stat (m_scope->GetMailPath(), &fileStatus, xpMailFolder))
//		{
			// Make sure the terms themselves are valid
			msg_SearchValidityTable *table = NULL;
			err = gValidityMgr.GetTable (msg_SearchValidityManager::offlineMail, &table);
			if (SearchError_Success == err)
			{
				XP_ASSERT (table);
				err = table->ValidateTerms (m_searchTerms);
			}
//		}
//		else
//			XP_ASSERT(0);
	}
	return err;
}



//-----------------------------------------------------------------------------
//---------------- Adapter class for searching offline folders ----------------
//-----------------------------------------------------------------------------


msg_SearchOfflineMail::msg_SearchOfflineMail (MSG_ScopeTerm *scope, MSG_SearchTermArray &termList) : msg_SearchAdapter (scope, termList)
{
	m_db = NULL;
	m_listContext = NULL;

	m_mailboxParser = NULL;
	m_parserState = kOpenFolderState;
}


msg_SearchOfflineMail::~msg_SearchOfflineMail ()
{
	// Database should have been closed when the scope term finished. 
	XP_ASSERT(!m_db);
}


MSG_SearchError msg_SearchOfflineMail::ValidateTerms ()
{
	MSG_SearchError err = msg_SearchAdapter::ValidateTerms ();
	if (SearchError_Success == err)
	{
		// Mail folder must exist. Don't worry about the summary file now since we may
		// have to regenerate the index later
		XP_StatStruct fileStatus;
		if (!XP_Stat (m_scope->GetMailPath(), &fileStatus, xpMailFolder))
		{
			// Make sure the terms themselves are valid
			msg_SearchValidityTable *table = NULL;
			err = gValidityMgr.GetTable (msg_SearchValidityManager::offlineMail, &table);
			if (SearchError_Success == err)
			{
				XP_ASSERT (table);
				err = table->ValidateTerms (m_searchTerms);
			}
		}
		else
			XP_ASSERT(0);
	}
	return err;
}


MSG_SearchError msg_SearchOfflineMail::OpenSummaryFile ()
{
	MailDB *mailDb = NULL;

	// do password protection of local cache thing.
	if (m_scope->m_folder && m_scope->m_folder->UserNeedsToAuthenticateForFolder(FALSE) && m_scope->m_folder->GetMaster()->PromptForHostPassword(m_scope->m_frame->GetContext(), m_scope->m_folder) != 0)
		return SearchError_DBOpenFailed;

	MsgERR dbErr = MailDB::Open (m_scope->GetMailPath(), FALSE /*create?*/, &mailDb);
	MSG_SearchError err = SearchError_Success;
	switch (dbErr)
	{
		case eSUCCESS:
			break;
		case eDBExistsNot:
		case eNoSummaryFile:
		case eOldSummaryFile:
			m_mailboxParser = new ParseMailboxState (m_scope->GetMailPath());
			if (!m_mailboxParser)
				err = SearchError_OutOfMemory;
			else
			{
				// Remove the old summary file so maildb::open can create a new one
				XP_FileRemove (m_scope->GetMailPath(), xpMailFolderSummary);
				dbErr = MailDB::Open (m_scope->GetMailPath(), TRUE /*create?*/, &mailDb, TRUE /*upgrading?*/);
				XP_ASSERT(mailDb);

				// Initialize the async parser to rebuild the summary file
				m_parserState = kOpenFolderState;
				m_mailboxParser->SetContext (m_scope->m_frame->GetContext());
				m_mailboxParser->SetDB (mailDb);
				m_mailboxParser->SetFolder(m_scope->m_folder);
				m_mailboxParser->SetIgnoreNonMailFolder(TRUE);
				err = SearchError_Success;
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

	if (mailDb && err == SearchError_Success)
		m_db = mailDb;

	return err;
}


MSG_SearchError msg_SearchOfflineMail::BuildSummaryFile ()
{
	// State machine for rebuilding the summary file asynchronously in the 
	// middle of the already-asynchronous search.

	MSG_SearchError err = SearchError_Success;
	int mkErr = 0;
	switch (m_parserState)
	{
	case kOpenFolderState:
		mkErr = m_mailboxParser->BeginOpenFolderSock (m_scope->GetMailPath(), NULL, 0, NULL);
		if (mkErr == MK_WAITING_FOR_CONNECTION)
			m_parserState++;
		else
			err = SummaryFileError();
		break;
	case kParseMoreState:
		mkErr = m_mailboxParser->ParseMoreFolderSock (m_scope->GetMailPath(), NULL, 0, NULL);
		if (mkErr == MK_CONNECTED)
			m_parserState++;
		else
			if (mkErr != MK_WAITING_FOR_CONNECTION)
				err = SummaryFileError();
		break;
	case kCloseFolderState:
		m_mailboxParser->CloseFolderSock (NULL, NULL, 0, NULL);
		if (!m_mailboxParser->GetIsRealMailFolder())
		{
			// mailbox parser has already closed the db (right?)
			XP_ASSERT(m_mailboxParser->GetDB() == 0);
			m_db = NULL;
			err = SearchError_ScopeDone;
		}
		delete m_mailboxParser;
		m_mailboxParser = NULL;
		// Put our regular "searching Inbox..." status text back up
		m_scope->m_frame->UpdateStatusBar(MK_MSG_SEARCH_STATUS);
		break;
	}
	return err;
}


MSG_SearchError msg_SearchOfflineMail::SummaryFileError ()
{
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
		m_mailboxParser->CloseFolderSock (NULL, NULL, 0, NULL);
		delete m_mailboxParser;
		m_mailboxParser = NULL;
		m_db = NULL;
	}

	return SearchError_ScopeDone;
}

MSG_SearchError msg_SearchOfflineMail::MatchTermsForFilter(DBMessageHdr *msgToMatch,
														   MSG_SearchTermArray & termList,
														   MSG_ScopeTerm * scope,
														   MessageDB * db, 
														   char * headers,
														   uint32 headerSize)
{
	return MatchTerms(msgToMatch, termList, scope, db, headers, headerSize, TRUE);
}

// static method which matches a header against a list of search terms.
MSG_SearchError msg_SearchOfflineMail::MatchTermsForSearch(DBMessageHdr *msgToMatch, 
												MSG_SearchTermArray &termList,
												MSG_ScopeTerm *scope,
												MessageDB *db)
{
	return MatchTerms(msgToMatch, termList, scope, db, NULL, 0, FALSE);
}

MSG_SearchError msg_SearchOfflineMail::MatchTerms(DBMessageHdr *msgToMatch,
														   MSG_SearchTermArray & termList,
														   MSG_ScopeTerm * scope,
														   MessageDB * db, 
														   char * headers,
														   uint32 headerSize,
														   XP_Bool Filtering) 
{
	MSG_SearchError err = SearchError_Success;
	XPStringObj	recipients;
	XPStringObj	ccList;
	XPStringObj	matchString;
	MSG_DBHandle dbHandle = (db) ? db->GetDB() : 0;

#ifdef _DEBUG
	// Use this to peek at the message since the atom IDs make the header strings opaque
	MessageHdrStruct debugHdr;
	msgToMatch->CopyToMessageHdr (&debugHdr);
#endif

	// Don't even bother to look at expunged messages awaiting compression
	if (msgToMatch->GetFlags() & kExpunged)
		err = SearchError_NotAMatch;

	// Loop over all terms, and match them all to this message. 

	int16 csid = scope->m_folder->GetFolderCSID() & ~CS_AUTO;
	if (CS_DEFAULT == csid)
		csid = INTL_DefaultWinCharSetID(0);


	CBoolExpression * expression = new CBoolExpression();  // create our expression
	if (!expression)
		return SearchError_OutOfMemory;
	for (int i = 0; i < termList.GetSize(); i++)
	{
		MSG_SearchTerm *pTerm = termList.GetAt(i);
		XP_ASSERT (pTerm->IsValid());
		XP_ASSERT (msgToMatch);

		switch (pTerm->m_attribute)
		{
		case attribSender:
			msgToMatch->GetAuthor(matchString, dbHandle);
			err = pTerm->MatchRfc822String (matchString, csid);
			break;
		case attribSubject:
			msgToMatch->GetSubject(matchString, TRUE, dbHandle);
			err = pTerm->MatchString (matchString, csid);
			break;
		case attribToOrCC:
		{
			MSG_SearchError errKeepGoing = pTerm->MatchAllBeforeDeciding() ? SearchError_Success : SearchError_NotAMatch;
			msgToMatch->GetRecipients(recipients, db->GetDB());
			err = pTerm->MatchRfc822String (recipients, csid);
			if (errKeepGoing == err)
			{
				msgToMatch->GetCCList(ccList, db->GetDB());
				err = pTerm->MatchRfc822String (ccList, csid);
			}
		}
			break;
		case attribBody:
			err = pTerm->MatchBody (scope, msgToMatch->GetArticleNum(), msgToMatch->GetLineCount(), csid, msgToMatch, db);
			break;
		case attribDate:
			err = pTerm->MatchDate (msgToMatch->GetDate());
			break;
		case attribMsgStatus:
			err = pTerm->MatchStatus (msgToMatch->GetFlags());
			break;
		case attribPriority:
			err = pTerm->MatchPriority (msgToMatch->GetPriority());
			break;
		case attribSize:
			err = pTerm->MatchSize (msgToMatch->GetByteLength());
			break;
		case attribTo:
			msgToMatch->GetRecipients(recipients, db->GetDB());
			err = pTerm->MatchRfc822String(recipients, csid);
			break;
		case attribCC:
			msgToMatch->GetCCList(ccList, db->GetDB());
			err = pTerm->MatchRfc822String (ccList, csid);
			break;
		case attribAgeInDays:
			err = pTerm->MatchAge (msgToMatch->GetDate());
			break;
		case attribOtherHeader:
			err = pTerm->MatchArbitraryHeader (scope, msgToMatch->GetArticleNum(), msgToMatch->GetLineCount(),csid, 
												msgToMatch, db, headers, headerSize, Filtering);
			break;

		default:
			err = SearchError_InvalidAttribute;
		}

		if (expression && (err == SearchError_Success || err == SearchError_NotAMatch))
			expression = expression->AddSearchTerm(pTerm, (err == SearchError_Success));    // added the term and its value to the expression tree
		else
			return SearchError_OutOfMemory;
	}
	XP_Bool result = expression->OfflineEvaluate();
	delete expression;
	return result ? SearchError_Success : SearchError_NotAMatch;
}


MSG_SearchError msg_SearchOfflineMail::Search ()
{
	MSG_SearchError err = SearchError_Success;
	DBMessageHdr *pHeaders = NULL;
	MsgERR dbErr = 0;

	// If we need to parse the mailbox before searching it, give another time
	// slice to the parser
	if (m_mailboxParser)
		err = BuildSummaryFile ();
	else
		// Try to open the DB lazily. This will set up a parser if one is required
		if (!m_db)
			err = OpenSummaryFile ();

	// Reparsing is unnecessary or completed
	if (m_mailboxParser == NULL && err == SearchError_Success)
	{
		XP_ASSERT (m_db);

		if (!m_listContext)
			dbErr = m_db->ListFirst (&m_listContext, &pHeaders);
		else
			dbErr = m_db->ListNext (m_listContext, &pHeaders);
		if (eSUCCESS != dbErr)
		{
			// Do clean up for end-of-scope processing
			err = SearchError_ScopeDone; //###phil dbErr is dropped on the floor
			m_db->ListDone (m_listContext);
			
			// Let go of the DB when we're done with it so we don't kill the db cache
			if (m_db)
				m_db->Close();
			m_db = NULL;

			// If we were searching the body of the message, close the folder
			if (m_scope->m_file)
				XP_FileClose (m_scope->m_file);
			m_scope->m_file = NULL;
				
			return err;
		}
		else
			// Is this message a hit?
			err = MatchTermsForSearch (pHeaders, m_searchTerms, m_scope, m_db);

		// Add search hits to the results list
		if (SearchError_Success == err)
			AddResultElement (pHeaders, m_db);

		m_scope->m_frame->IncrementOfflineProgress();

		delete pHeaders;

	}
	return err;
}


MSG_SearchError msg_SearchOfflineMail::AddResultElement (DBMessageHdr *pHeaders, MessageDB *db)
{
	MSG_SearchError err = SearchError_Success;
	MSG_DBHandle dbHandle = (db) ? db->GetDB() : 0;

	MSG_ResultElement *newResult = new MSG_ResultElement (this);

	if (newResult)
	{
		XP_ASSERT (newResult);

		// This isn't very general. Just add the headers we think we'll be interested in
		// to the list of attributes per result element.
		MSG_SearchValue *pValue = new MSG_SearchValue;
		if (pValue)
		{
			XPStringObj subject;
			pValue->attribute = attribSubject;
			char *reString = (pHeaders->GetFlags() & kHasRe) ? "Re: " : "";
			pHeaders->GetSubject(subject, FALSE, dbHandle);
			pValue->u.string = PR_smprintf ("%s%s", reString, (const char*) subject); // hack. invoke cast operator by force
			newResult->AddValue (pValue);
		}
		pValue = new MSG_SearchValue;
		if (pValue)
		{
			pValue->attribute = attribSender;
			pValue->u.string = (char*) XP_ALLOC(64);
			if (pValue->u.string)
			{
				pHeaders->GetAuthor(pValue->u.string, 64, dbHandle);
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
			pValue->u.string = XP_STRDUP(m_scope->m_folder->GetName());
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

int
msg_SearchOfflineMail::Abort ()
{
	// Let go of the DB when we're done with it so we don't kill the db cache
	if (m_db)
		m_db->Close();
	m_db = NULL;

	// If we got aborted in the middle of parsing a mail folder, we should
	// free the parser object (esp. so it releases the folderInfo's semaphore)
	if (m_mailboxParser)
		delete m_mailboxParser;
	m_mailboxParser = NULL;

	return msg_SearchAdapter::Abort ();
}


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
		int16 src_csid, dst_csid;
		GetSearchCSIDs(src_csid, dst_csid);

		// do IMAP specific validation
		char *tmpEncoding = NULL;
		err = Encode (&tmpEncoding, m_searchTerms, src_csid, dst_csid);
		if (SearchError_Success == err)
		{
			// we are searching an online folder, right?
			XP_ASSERT(m_scope->m_folder->GetType() == FOLDER_IMAPMAIL);
			MSG_IMAPFolderInfoMail *imapFolder = (MSG_IMAPFolderInfoMail *) m_scope->m_folder;
			m_encoding = CreateImapSearchUrl(imapFolder->GetHostName(),
													  imapFolder->GetOnlineName(),
													  imapFolder->GetOnlineHierarchySeparator(),
													  tmpEncoding,
													  TRUE);	// return UIDs
			delete [] tmpEncoding;
		}
		else 
			if (err == SearchError_ScopeAgreement)
				XP_ASSERT(FALSE);
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


// taken from offline mail, talk to phil
MSG_SearchError msg_SearchOnlineMail::AddResultElement (DBMessageHdr *pHeaders, MessageDB *db)
{
	MSG_SearchError err = SearchError_Success;
	MSG_DBHandle dbHandle = (db) ? db->GetDB() : 0;

	MSG_ResultElement *newResult = new MSG_ResultElement (this);

	if (newResult)
	{
		XP_ASSERT (newResult);

		// This isn't very general. Just add the headers we think we'll be interested in
		// to the list of attributes per result element.
		MSG_SearchValue *pValue = new MSG_SearchValue;
		if (pValue)
		{
			XPStringObj subject;
			pValue->attribute = attribSubject;
			char *reString = (pHeaders->GetFlags() & kHasRe) ? "Re:" : "";
			pHeaders->GetSubject(subject, FALSE, dbHandle);
			pValue->u.string = PR_smprintf ("%s%s", reString, (const char*) subject); // hack. invoke cast operator by force
			newResult->AddValue (pValue);
		}
		pValue = new MSG_SearchValue;
		if (pValue)
		{
			pValue->attribute = attribSender;
			pValue->u.string = (char*) XP_ALLOC(64);
			if (pValue->u.string)
			{
				pHeaders->GetAuthor(pValue->u.string, 64, dbHandle);
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
			pValue->u.string = XP_STRDUP(m_scope->m_folder->GetName());
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
	{	// this search happened because of a search dialog
		msg_SearchOnlineMail *adapter = (msg_SearchOnlineMail *) frame->GetRunningAdapter();
		if (adapter)
		{
			// open the relevant IMAP db
			MailDB *imapDB = NULL;
			XP_Bool wasCreated = FALSE;
			ImapMailDB::Open(adapter->m_scope->m_folder->GetMailFolderInfo()->GetPathname(),
							 FALSE,		// do not create if not found
							 &imapDB,
							 adapter->m_scope->m_folder->GetMaster(),
							 &wasCreated);
							 
			if (imapDB)
			{
				// expect search results in the form of "* SEARCH <hit> <hit> ..."
				char *tokenString = XP_STRDUP(resultLine);
				if (tokenString)
				{
					char *currentPosition = strcasestr(tokenString, "SEARCH");
					if (currentPosition)
					{
						currentPosition += XP_STRLEN("SEARCH");
						
						XP_Bool shownUpdateAlert = FALSE;
						char *hitUidToken = XP_STRTOK(currentPosition, WHITESPACE);
						while (hitUidToken)
						{
							long naturalLong; // %l is 64 bits on OSF1
							sscanf(hitUidToken, "%ld", &naturalLong);
							MessageKey hitUid = (MessageKey) naturalLong;
							
							MailMessageHdr *hitHeader = imapDB->GetMailHdrForKey(hitUid);
							if (hitHeader)
								adapter->AddResultElement(hitHeader, imapDB);
							else if (!shownUpdateAlert)
							{
								FE_Alert(context, XP_GetString(MK_MSG_SEARCH_HITS_NOT_IN_DB));
								shownUpdateAlert = TRUE;
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
		XP_ASSERT(FALSE); // apparently, this was left over from trying to do filtering on the server
	}
}

MSG_SearchError msg_SearchOnlineMail::Search ()
{
	// we should never end up here for a purely online
	// folder.  We might for an offline IMAP folder.
	MSG_SearchError err = SearchError_NotImplemented;

	return err;
}


MSG_SearchError msg_SearchOnlineMail::Encode (char **ppEncoding, MSG_SearchTermArray &searchTerms, int16 src_csid, int16 dest_csid)
{
	*ppEncoding = NULL;
	char *imapTerms = NULL;

	// Get the optional CHARSET parameter, in case we need it.
	char *csname = GetImapCharsetParam(dest_csid);

	MSG_SearchError err = msg_SearchAdapter::EncodeImap (&imapTerms,searchTerms, src_csid, dest_csid, FALSE);
	if (SearchError_Success == err)
	{
		int len = XP_STRLEN(m_kSearchTemplate) + XP_STRLEN(imapTerms) + (csname ? XP_STRLEN(csname) : 0) + 1;
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
	XP_ASSERT (NULL == m_offlineMailTable);
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

//		m_offlineMailTable->SetValidButNotShown (attribAgeInDays, opIsHigherThan, 1);
//		m_offlineMailTable->SetValidButNotShown (attribAgeInDays, opIsLowerThan,  1);
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
	XP_ASSERT (NULL == m_onlineMailTable);
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
	// IMAP filtering happens on the client, fundamentally using the same
	// capabilities as POP filtering. However, since we don't yet have the 
	// IMAP message body, we can't filter on body attributes. So this table
	// is supposed to be the same as offline mail, except that the body 
	// attribute is omitted

	XP_ASSERT (NULL == m_onlineMailFilterTable);
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

