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

// Implementation of search for newsgroups

#include "msg.h"
#include "pmsgsrch.h"
#include "msgfinfo.h"
#include "newsdb.h"
#include "newshdr.h"
#include "newshost.h"
#include "hosttbl.h"
#include "libi18n.h"

//-----------------------------------------------------------------------------
//----------- Adapter class for searching XPAT-capable news servers -----------
//-----------------------------------------------------------------------------


const char *msg_SearchNews::m_kNntpFrom = "FROM ";
const char *msg_SearchNews::m_kNntpSubject = "SUBJECT ";
const char *msg_SearchNews::m_kTermSeparator = "/";


msg_SearchNews::msg_SearchNews (MSG_ScopeTerm *scope, MSG_SearchTermArray &termList) : msg_SearchAdapter (scope, termList)
{
	m_encoding = NULL;
}


msg_SearchNews::~msg_SearchNews ()
{
	delete [] m_encoding;
}


MSG_SearchError msg_SearchNews::ValidateTerms ()
{
	MSG_SearchError err = msg_SearchAdapter::ValidateTerms ();
	if (SearchError_Success == err)
	{
		err = Encode (&m_encoding);
		if (SearchError_Success == err)
		{
			// hack
			URL_Struct *url = NET_CreateURLStruct (m_encoding, NET_DONT_RELOAD);
			if (url)
			{
				url->pre_exit_fn = PreExitFunction;
				m_scope->m_frame->m_urlStruct = url;
			}
			else
				err = SearchError_OutOfMemory;
		}
	}

	return err;
}


MSG_SearchError msg_SearchNews::Search ()
{
	// the state machine runs in the news: handler
	MSG_SearchError err = SearchError_NotImplemented;
	return err;
}


char *msg_SearchNews::EncodeTerm (MSG_SearchTerm *term)
{
	// Develop an XPAT-style encoding for the search term

	XP_ASSERT(term);
	if (!term)
		return NULL;

	// Find a string to represent the attribute
	const char *attribEncoding = NULL;
	switch (term->m_attribute)
	{
	case attribSender:
		attribEncoding = m_kNntpFrom;
		break;
	case attribSubject:
		attribEncoding = m_kNntpSubject;
		break;
	default:
		XP_ASSERT(FALSE); // malformed search term?
		return NULL;
	}

	// Build a string to represent the string pattern
	XP_Bool leadingStar = FALSE;
	XP_Bool trailingStar = FALSE;
	int overhead = 1; // null terminator
	switch (term->m_operator)
	{
	case opContains:
		leadingStar = TRUE;
		trailingStar = TRUE;
		overhead += 2;
		break;
	case opIs:
		break;
	case opBeginsWith:
		trailingStar = TRUE;
		overhead++;
		break;
	case opEndsWith:
		leadingStar = TRUE;
		overhead++;
		break;
	default:
		XP_ASSERT(FALSE); // malformed search term?
		return NULL;
	}
	
	int16 wincsid = INTL_DefaultWinCharSetID(0);	// *** FIX ME: Should not get default csid, should get csid from FE or folder

	// Do INTL_FormatNNTPXPATInRFC1522Format trick for non-ASCII string
	unsigned char *intlNonRFC1522Value = 
		INTL_FormatNNTPXPATInNonRFC1522Format (wincsid, (unsigned char*)term->m_value.u.string);
	if (!intlNonRFC1522Value)
		return NULL;
		
	// TO DO: Do INTL_FormatNNTPXPATInRFC1522Format trick for non-ASCII string
	// Unfortunately, we currently do not handle xxx or xxx search in XPAT
	// Need to add the INTL_FormatNNTPXPATInRFC1522Format call after we can do that
	// so we should search a string in either RFC1522 format and non-RFC1522 format
		
	char *escapedValue = MSG_EscapeSearchUrl ((char*)intlNonRFC1522Value);
	XP_FREE(intlNonRFC1522Value);

	if (!escapedValue)
		return NULL;

	// We also need to apply NET_Escape to it since we have to pass 8-bits data
	// And sometimes % in the 7-bit doulbe byte JIS
	// 
	char * urlEncoded = NET_Escape((char*)escapedValue, URL_PATH);
	XP_FREE(escapedValue);

	if (! urlEncoded)
		return NULL;

	char *pattern = pattern = new char [XP_STRLEN(urlEncoded) + overhead];
	if (!pattern)
		return NULL;
	else 
		pattern[0] = '\0';

	if (leadingStar)
		XP_STRCAT (pattern, "*");
	XP_STRCAT (pattern, urlEncoded);
	if (trailingStar)
		XP_STRCAT (pattern, "*");

	// Combine the XPAT command syntax with the attribute and the pattern to
	// form the term encoding
	char *xpatTemplate = "XPAT %s 1- %s";
	int termLength = XP_STRLEN(xpatTemplate) + XP_STRLEN(attribEncoding) + XP_STRLEN(pattern) + 1;
	char *termEncoding = new char [termLength];
	if (termEncoding)
		PR_snprintf (termEncoding, termLength, xpatTemplate, attribEncoding, pattern);

	XP_FREE(urlEncoded);
	delete [] pattern;

	return termEncoding;
}


char *msg_SearchNews::BuildUrlPrefix ()
{
	char *result = NULL;
	switch (m_scope->m_folder->GetType())
	{
	case FOLDER_CONTAINERONLY: 
	{
		// Would be better to do this in the folder info, but we need to get
		// back to the NewsHost to find out if it's secure.
		msg_HostTable *table = m_scope->m_frame->m_pane->GetMaster()->GetHostTable();
		for (int i = 0; i < table->getNumHosts() && !result; i++)
		{
			MSG_NewsHost *host = table->getHost(i);
			if (m_scope->m_folder == host->GetHostInfo())
				result = PR_smprintf("%s/unused", host->GetURLBase());
		}
		break;
	}
	case FOLDER_NEWSGROUP:
	case FOLDER_CATEGORYCONTAINER:
		result = m_scope->m_folder->BuildUrl(NULL, MSG_MESSAGEKEYNONE);
		break;
	default:
		XP_ASSERT(FALSE);
	}
	return result;
}


MSG_SearchError msg_SearchNews::Encode (char **outEncoding)
{
	XP_ASSERT(outEncoding);
	if (!outEncoding)
		return SearchError_NullPointer;

	*outEncoding = NULL;
	MSG_SearchError err = SearchError_Success;

	char **intermediateEncodings = new char * [m_searchTerms.GetSize()];
	if (intermediateEncodings)
	{
		char *urlPrefix = BuildUrlPrefix();
		if (urlPrefix)
		{
			// Build an XPAT command for each term
			int encodingLength = XP_STRLEN(urlPrefix);
			int i;
			for (i = 0; i < m_searchTerms.GetSize(); i++)
			{
				MSG_SearchTerm * term = m_searchTerms.GetAt(i);
				// set boolean OR term if any of the search terms are an OR...this only works if we are using
				// homogeneous boolean operators.
				m_ORSearch = !(term->IsBooleanOpAND());
			
				intermediateEncodings[i] = EncodeTerm (m_searchTerms.GetAt(i));	
				if (intermediateEncodings[i])
					encodingLength += XP_STRLEN(intermediateEncodings[i]) + XP_STRLEN(m_kTermSeparator);
			}
			encodingLength += XP_STRLEN("?search");
			// Combine all the term encodings into one big encoding
			char *encoding = new char [encodingLength + 1];
			if (encoding)
			{
				XP_STRCPY (encoding, urlPrefix);
				XP_STRCAT (encoding, "?search");
				for (i = 0; i < m_searchTerms.GetSize(); i++)
				{
					if (intermediateEncodings[i])
					{
						XP_STRCAT (encoding, m_kTermSeparator);
						XP_STRCAT (encoding, intermediateEncodings[i]);
						delete [] intermediateEncodings[i];
					}
				}
				*outEncoding = encoding;
			}
			else
				err = SearchError_OutOfMemory;
			XP_FREE(urlPrefix);
		}
		else
			err = SearchError_OutOfMemory;
		delete [] intermediateEncodings;
	}
	else
		err = SearchError_OutOfMemory;

	return err;
}


// Callback from libnet
SEARCH_API void MSG_AddNewsXpatHit (MWContext *context, uint32 artNum)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromContext(context);
	msg_SearchNews *adapter = (msg_SearchNews*) frame->GetRunningAdapter();
	adapter->AddHit (artNum);
}


void msg_SearchNews::PreExitFunction (URL_Struct * /*url*/, int status, MWContext *context)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromContext (context);
	msg_SearchNews *adapter = (msg_SearchNews*) frame->GetRunningAdapter();
	adapter->CollateHits();
	adapter->ReportHits();

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

XP_Bool msg_SearchNews::DuplicateHit(uint32 artNum)  
// ASSUMES m_hits is sorted!!
{
	int index;
	for (index = 0; index < m_hits.GetSize(); index++)
		if (artNum == m_hits.GetAt(index))
			return TRUE;
	return FALSE;
}


void msg_SearchNews::CollateHits ()
{
	// Since the XPAT commands are processed one at a time, the result set for the
	// entire query is the intersection of results for each XPAT command if an AND Search
	// otherwise we want the union of all the search hits (minus the duplicates of course)

	if (m_candidateHits.GetSize() == 0)
		return;

	// Sort the article numbers first, so it's easy to tell how many hits
	// on a given article we got
	m_candidateHits.QuickSort(CompareArticleNumbers);
	int size = m_candidateHits.GetSize();
	int index = 0;
	uint32 candidate = m_candidateHits.GetAt(index);

	if (m_ORSearch)
	{
		for (index = 0; index < size; index++)
		{
			candidate = m_candidateHits.GetAt(index);
			if (!DuplicateHit(candidate)) // if not a dup, add it to the hit list
				m_hits.Add (candidate);
		}
		return;
	}


	// otherwise we have a traditional and search which must be collated

	// In order to get promoted into the hits list, a candidate article number
	// must appear in the results of each XPAT command. So if we fire 3 XPAT
	// commands (one per search term), the article number must appear 3 times.
	// If it appears less than 3 times, it matched some search terms, but not all

	int termCount = m_searchTerms.GetSize();
	int candidateCount = 0;
	while (index < size)
	{
		if (candidate == m_candidateHits.GetAt(index))
			candidateCount++;
		else
			candidateCount = 1;
		if (candidateCount == termCount)
			m_hits.Add (m_candidateHits.GetAt(index));
		candidate = m_candidateHits.GetAt(index++);
	}
}


void msg_SearchNews::ReportHits ()
{
	XP_ASSERT (m_scope->m_folder->IsNews());
	if (!m_scope->m_folder->IsNews())
		return;
	
	MSG_FolderInfoNews *folder = (MSG_FolderInfoNews*) m_scope->m_folder;

	// Construct a URL for the newsgroup, since thats what newDB::open wants
	char *url = folder->BuildUrl(NULL, MSG_MESSAGEKEYNONE);
	if (!url)
		return;

	NewsGroupDB	*newsDB = NULL;
	MsgERR status = NewsGroupDB::Open(url, m_scope->m_frame->m_pane->GetMaster(), &newsDB);
	if (status == eSUCCESS)
	{
		XP_ASSERT(newsDB);
		if (!newsDB)
			return;

		XP_FREEIF(url);

		for (uint32 i = 0; i < m_hits.GetSize(); i++)
		{
			NewsMessageHdr *header = newsDB->GetNewsHdrForKey(m_hits.GetAt(i));
			if (header)
			{
				ReportHit(header);
				delete header;
			}
		}
		newsDB->Close();
	}
}


void msg_SearchNews::ReportHit (MessageHdrStruct *pHeaders, const char *location)
{
	// this is taken from msg_SearchOfflineMail until I decide whether the 
	// right thing is to get them from the db or from NNTP

	MSG_SearchError err = SearchError_Success;
	MSG_ResultElement *newResult = new MSG_ResultElement (this);

	if (newResult)
	{
		XP_ASSERT (newResult);

		// This isn't very general. Just add the headers we think we'll be interested in
		// to the list of attributes per result element.
		MSG_SearchValue *pValue = new MSG_SearchValue;
		if (pValue)
		{
			pValue->attribute = attribSubject;
			char *reString = (pHeaders->m_flags & MSG_FLAG_HAS_RE) ? "Re:" : "";
			pValue->u.string = PR_smprintf ("%s%s", reString, pHeaders->m_subject);
			newResult->AddValue (pValue);
		}
		pValue = new MSG_SearchValue;
		if (pValue)
		{
			pValue->attribute = attribSender;
			pValue->u.string = (char*) XP_ALLOC(64);
			if (pValue->u.string)
			{
				XP_STRNCPY_SAFE(pValue->u.string, pHeaders->m_author, 64);
				newResult->AddValue (pValue);
			}
			else
				err = SearchError_OutOfMemory;
		}
		pValue = new MSG_SearchValue;
		if (pValue)
		{
			pValue->attribute = attribDate;
			pValue->u.date = pHeaders->m_date;
			newResult->AddValue (pValue);
		}
		pValue = new MSG_SearchValue;
		if (pValue)
		{
			pValue->attribute = attribMsgStatus;
			pValue->u.msgStatus = pHeaders->m_flags;
			newResult->AddValue (pValue);
		}
		pValue = new MSG_SearchValue;
		if (pValue)
		{
			pValue->attribute = attribPriority;
			pValue->u.priority = pHeaders->m_priority;
			newResult->AddValue (pValue);
		}
		pValue = new MSG_SearchValue;
		if (pValue)
		{
				pValue->attribute = attribLocation;
				pValue->u.string = XP_STRDUP(location); 
				newResult->AddValue (pValue);
		}
		pValue = new MSG_SearchValue;
		if (pValue)
		{
			pValue->attribute = attribMessageKey;
			pValue->u.key = pHeaders->m_messageKey;
			newResult->AddValue (pValue);
		}
		if (pHeaders->m_messageId)
		{
			pValue = new MSG_SearchValue;
			if (pValue)
			{
				pValue->attribute = attribMessageId;
				pValue->u.string = XP_STRDUP(pHeaders->m_messageId);
				newResult->AddValue (pValue);
			}
		}
		if (!pValue)
			err = SearchError_OutOfMemory;
		m_scope->m_frame->AddResultElement (newResult);
	}
}


void msg_SearchNews::ReportHit (DBMessageHdr *pHeaders)
{
	MessageHdrStruct hdr;
	pHeaders->CopyToMessageHdr(&hdr);
	ReportHit (&hdr, m_scope->m_folder->GetName());
}


int msg_SearchNews::CompareArticleNumbers (const void *v1, const void *v2)
{
	// QuickSort callback to compare article numbers

	uint32 i1 = *(uint32*) v1;
	uint32 i2 = *(uint32*) v2;
	return i1 - i2;
}


//-----------------------------------------------------------------------------
//-------- Adapter class for searching SEARCH-capable news servers ------------
//-----------------------------------------------------------------------------


const char *msg_SearchNewsEx::m_kSearchTemplate = "?search/SEARCH HEADER NEWSGROUPS %s %s";
const char *msg_SearchNewsEx::m_kProfileTemplate = "%s/dummy?profile/PROFILE NEW %s HEADER NEWSGROUPS %s %s";

msg_SearchNewsEx::msg_SearchNewsEx (MSG_ScopeTerm *scope, MSG_SearchTermArray &termList) : msg_SearchNews (scope, termList) 
{
}


msg_SearchNewsEx::~msg_SearchNewsEx () 
{
}


MSG_SearchError msg_SearchNewsEx::ValidateTerms ()
{
	MSG_SearchError err = msg_SearchAdapter::ValidateTerms ();
	if (SearchError_Success == err)
	{
		err = Encode (&m_encoding);
		if (SearchError_Success == err)
		{
			// hack.
			URL_Struct *url = NET_CreateURLStruct (m_encoding, NET_DONT_RELOAD);
			if (url)
			{				
				url->pre_exit_fn = PreExitFunctionEx;
				m_scope->m_frame->m_urlStruct = url;
			}
			else
				err = SearchError_OutOfMemory;
		}
	}
	return err;
}


MSG_SearchError msg_SearchNewsEx::Search ()
{
	// State machine runs in mknews.c?
	return SearchError_NotImplemented;
}

MSG_SearchError msg_SearchNewsEx::Encode (char **ppOutEncoding)
{
	*ppOutEncoding = NULL;
	char *imapTerms = NULL;

	// Figure out the charsets to use for the search terms and targets.
	int16 src_csid, dst_csid;
	GetSearchCSIDs(src_csid, dst_csid);

	MSG_SearchError err = EncodeImap (&imapTerms, m_searchTerms, src_csid, dst_csid, TRUE );
	if (SearchError_Success == err)
	{
		char *scopeString = NULL; 
		err = m_scope->m_frame->EncodeRFC977bisScopes (&scopeString);
		if (SearchError_Success == err)
		{
			// Wrap the pattern with the RFC-977bis specified SEARCH syntax
			char *RFC977bisEncoding = PR_smprintf (m_kSearchTemplate, scopeString, imapTerms);
			if (RFC977bisEncoding)
			{
				// Build the host/group specification
				char *urlPrefix = BuildUrlPrefix ();
				if (urlPrefix)
				{
					// Build the whole URL e.g. new://host/local.index/search?SEARCH FROM "John Smith"
					*ppOutEncoding = new char [XP_STRLEN(urlPrefix) + XP_STRLEN(RFC977bisEncoding) + 1];
					if (*ppOutEncoding)
					{
						XP_STRCPY (*ppOutEncoding, urlPrefix);
						XP_STRCAT (*ppOutEncoding, RFC977bisEncoding);
					}
					else
						err = SearchError_OutOfMemory;
					XP_FREE(urlPrefix);
				}
				else
					err = SearchError_OutOfMemory;
				XP_FREE(RFC977bisEncoding);
			}
			else
				err = SearchError_OutOfMemory;
			XP_FREE(scopeString);
		}
	}

	return err;
}


MSG_SearchError msg_SearchNewsEx::SaveProfile (const char *profileName)
{
	MSG_SearchError err = SearchError_Success;
	MSG_FolderInfo *folder = m_scope->m_folder;

	// Figure out which news host to fire the URL at. Maybe we should have a virtual function in MSG_FolderInfo for this?
	MSG_NewsHost *host = NULL;
	MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();
	if (newsFolder)
		host = newsFolder->GetHost();
	else if (FOLDER_CONTAINERONLY == folder->GetType())
		host = ((MSG_NewsFolderInfoContainer*) folder)->GetHost();

	XP_ASSERT(NULL != host && NULL != profileName);
	if (NULL != host && NULL != profileName)
	{
		char *scopeString = NULL;
		m_scope->m_frame->EncodeRFC977bisScopes (&scopeString);

		// Figure out the charsets to use for the search terms and targets.
		int16 src_csid, dst_csid;
		GetSearchCSIDs(src_csid, dst_csid);

		char *termsString = NULL;
		EncodeImap (&termsString, m_searchTerms, 
					src_csid, dst_csid,
					TRUE );

		char *legalProfileName = XP_STRDUP(profileName);

		if (termsString && scopeString && legalProfileName)
		{
			msg_MakeLegalNewsgroupComponent (legalProfileName);
			char *url = PR_smprintf (m_kProfileTemplate, host->GetURLBase(),
									 legalProfileName, scopeString,
									 termsString);
			if (url)
			{
				URL_Struct *urlStruct = NET_CreateURLStruct (url, NET_DONT_RELOAD);
				if (urlStruct)
				{
					// Set the internal_url flag so just in case someone else happens to have
					// a search-libmsg URL, it won't fire my code, and surely crash.
					urlStruct->internal_url = TRUE;

					// Set the pre_exit_fn to we can turn off cylon mode when we're done
					urlStruct->pre_exit_fn = PreExitFunctionEx;

					int getUrlErr = m_scope->m_frame->m_pane->GetURL (urlStruct, FALSE);
					if (getUrlErr != 0)
						err = SearchError_ScopeAgreement; // ### not really. impedance mismatch
					else
						m_scope->m_frame->BeginCylonMode();
				}
				else
					err = SearchError_OutOfMemory;
				XP_FREE(url);
			}
			else
				err = SearchError_OutOfMemory;
		}

		XP_FREEIF(scopeString);
		delete [] termsString;
		XP_FREEIF(legalProfileName);
	}
	return err;
}


// Callback from libnet
SEARCH_API void MSG_AddNewsSearchHit (MWContext *context, const char *resultLine)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromContext (context);
	msg_SearchNewsEx *adapter = (msg_SearchNewsEx *) frame->GetRunningAdapter();
	if (adapter)
	{
		MessageHdrStruct hdr;
		XP_BZERO(&hdr, sizeof(hdr));

		// Here we make the SEARCH result compatible with xover conventions. In SEARCH, the
		// group name and a ':' precede the article number, so try to skip over this stuff
		// before asking DBMessageHdr to parse it
		char *xoverCompatLine = XP_STRCHR(resultLine, ':');
		if (xoverCompatLine)
			xoverCompatLine++;
		else
			xoverCompatLine = (char*) resultLine; //### casting away const

		if (DBMessageHdr::ParseLine ((char*) xoverCompatLine, &hdr)) //### casting away const
		{
			if (hdr.m_flags & kHasRe) // hack around which kind of flag we actually got
			{
				hdr.m_flags &= !kHasRe;
				hdr.m_flags |= MSG_FLAG_HAS_RE;
			}
			adapter->ReportHit (&hdr, XP_STRTOK((char*) resultLine, ":")); //### casting away const
		}
	}
}


SEARCH_API MSG_SearchError MSG_SaveProfileStatus (MSG_Pane *searchPane, XP_Bool *cmdEnabled)
{
	MSG_SearchError err = SearchError_Success;

	XP_ASSERT(cmdEnabled);
	if (cmdEnabled)
	{
		*cmdEnabled = FALSE;
		MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (searchPane);
		if (frame)
			*cmdEnabled = frame->GetSaveProfileStatus();
	}
	else
		err = SearchError_NullPointer;

	return err;
}


SEARCH_API MSG_SearchError MSG_SaveProfile (MSG_Pane *searchPane, const char * profileName)
{
	MSG_SearchError err = SearchError_Success;

#ifdef _DEBUG
	XP_Bool enabled = FALSE;
	MSG_SaveProfileStatus (searchPane, &enabled);
	XP_ASSERT(enabled);
	if (!enabled)
		return SearchError_ScopeAgreement;
#endif

	if (profileName)
	{
		MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (searchPane);
		XP_ASSERT(frame);
		if (frame)
		{
			msg_SearchNewsEx *adapter = frame->GetProfileAdapter();
			XP_ASSERT(adapter);
			if (adapter)
				err = adapter->SaveProfile (profileName);
		}
	}
	else
		err = SearchError_NullPointer;

	return err;
}


SEARCH_API int MSG_AddProfileGroup (MSG_Pane *pane, MSG_NewsHost* host,
									const char *groupName)
{
	MSG_FolderInfoNews *group =
		pane->GetMaster()->AddProfileNewsgroup(host, groupName);
	return group ? 0 : -1;
}


void msg_SearchNewsEx::PreExitFunctionEx (URL_Struct * /*url*/, int /*status*/, MWContext *context)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromContext (context);
	frame->EndCylonMode();
}


//-----------------------------------------------------------------------------
//------------ Adapter class for searching offline news groups ----------------
//-----------------------------------------------------------------------------


msg_SearchOfflineNews::msg_SearchOfflineNews (MSG_ScopeTerm *scopes, MSG_SearchTermArray &terms) : msg_SearchOfflineMail (scopes, terms)
{
}


msg_SearchOfflineNews::~msg_SearchOfflineNews ()
{
}


MSG_SearchError msg_SearchOfflineNews::OpenSummaryFile ()
{
	MSG_SearchError err = SearchError_DBOpenFailed;
	if (m_scope->m_folder->IsNews())
	{
		MSG_FolderInfoNews *newsFolder = (MSG_FolderInfoNews*) m_scope->m_folder;
		char *url = newsFolder->BuildUrl(NULL, MSG_MESSAGEKEYNONE);
		if (url)
		{
			NewsGroupDB *newsDb = NULL;
			MsgERR msgErr = NewsGroupDB::Open (url, m_scope->m_frame->m_pane->GetMaster(), &newsDb);
			if (eSUCCESS == msgErr)
			{
				m_db = newsDb;
				err = SearchError_Success;
			}
			XP_FREE(url);
		}
		else
			err = SearchError_OutOfMemory;
	}
	return err;
}

MSG_SearchError msg_SearchOfflineNews::ValidateTerms ()
{
	MSG_SearchError err = msg_SearchAdapter::ValidateTerms ();
	if (SearchError_Success == err)
	{
		// Make sure the terms themselves are valid
		msg_SearchValidityTable *table = NULL;
		err = gValidityMgr.GetTable (msg_SearchValidityManager::localNews, &table);
		if (SearchError_Success == err)
		{
			XP_ASSERT (table);
			err = table->ValidateTerms (m_searchTerms);
		}
	}
	return err;
}


//-----------------------------------------------------------------------------
MSG_SearchError msg_SearchValidityManager::InitLocalNewsTable()
{
	XP_ASSERT (NULL == m_localNewsTable);
	MSG_SearchError err = NewTable (&m_localNewsTable);

	if (SearchError_Success == err)
	{
		m_localNewsTable->SetAvailable (attribSender, opContains, 1);
		m_localNewsTable->SetEnabled   (attribSender, opContains, 1);
		m_localNewsTable->SetAvailable (attribSender, opIs, 1);
		m_localNewsTable->SetEnabled   (attribSender, opIs, 1);
		m_localNewsTable->SetAvailable (attribSender, opBeginsWith, 1);
		m_localNewsTable->SetEnabled   (attribSender, opBeginsWith, 1);
		m_localNewsTable->SetAvailable (attribSender, opEndsWith, 1);
		m_localNewsTable->SetEnabled   (attribSender, opEndsWith, 1);

		m_localNewsTable->SetAvailable (attribSubject, opContains, 1);
		m_localNewsTable->SetEnabled   (attribSubject, opContains, 1);
		m_localNewsTable->SetAvailable (attribSubject, opIs, 1);
		m_localNewsTable->SetEnabled   (attribSubject, opIs, 1);
		m_localNewsTable->SetAvailable (attribSubject, opBeginsWith, 1);
		m_localNewsTable->SetEnabled   (attribSubject, opBeginsWith, 1);
		m_localNewsTable->SetAvailable (attribSubject, opEndsWith, 1);
		m_localNewsTable->SetEnabled   (attribSubject, opEndsWith, 1);

		m_localNewsTable->SetAvailable (attribBody, opContains, 1);
		m_localNewsTable->SetEnabled   (attribBody, opContains, 1);
		m_localNewsTable->SetAvailable (attribBody, opDoesntContain, 1);
		m_localNewsTable->SetEnabled   (attribBody, opDoesntContain, 1);
		m_localNewsTable->SetAvailable (attribBody, opIs, 1);
		m_localNewsTable->SetEnabled   (attribBody, opIs, 1);
		m_localNewsTable->SetAvailable (attribBody, opIsnt, 1);
		m_localNewsTable->SetEnabled   (attribBody, opIsnt, 1);


		m_localNewsTable->SetEnabled   (attribDate, opIsBefore, 1);
		m_localNewsTable->SetAvailable (attribDate, opIsAfter, 1);
		m_localNewsTable->SetEnabled   (attribDate, opIsAfter, 1);
		m_localNewsTable->SetAvailable (attribDate, opIs, 1);
		m_localNewsTable->SetEnabled   (attribDate, opIs, 1);
		m_localNewsTable->SetAvailable (attribDate, opIsnt, 1);
		m_localNewsTable->SetEnabled   (attribDate, opIsnt, 1);

		m_localNewsTable->SetAvailable (attribOtherHeader, opContains, 1);   // added for arbitrary headers
		m_localNewsTable->SetEnabled   (attribOtherHeader, opContains, 1);
		m_localNewsTable->SetAvailable (attribOtherHeader, opDoesntContain, 1);
		m_localNewsTable->SetEnabled   (attribOtherHeader, opDoesntContain, 1);
		m_localNewsTable->SetAvailable (attribOtherHeader, opIs, 1);
		m_localNewsTable->SetEnabled   (attribOtherHeader, opIs, 1);
		m_localNewsTable->SetAvailable (attribOtherHeader, opIsnt, 1);
		m_localNewsTable->SetEnabled   (attribOtherHeader, opIsnt, 1);

		m_localNewsTable->SetAvailable (attribAgeInDays, opIsGreaterThan, 1);
		m_localNewsTable->SetEnabled   (attribAgeInDays, opIsGreaterThan, 1);
		m_localNewsTable->SetAvailable (attribAgeInDays, opIsLessThan,  1);
		m_localNewsTable->SetEnabled   (attribAgeInDays, opIsLessThan, 1);
		m_localNewsTable->SetAvailable (attribAgeInDays, opIs,  1);
		m_localNewsTable->SetEnabled   (attribAgeInDays, opIs, 1);

		m_localNewsTable->SetAvailable (attribMsgStatus, opIs, 1);
		m_localNewsTable->SetEnabled   (attribMsgStatus, opIs, 1);
		m_localNewsTable->SetAvailable (attribMsgStatus, opIsnt, 1);
		m_localNewsTable->SetEnabled   (attribMsgStatus, opIsnt, 1);

	}

	return err;
}


MSG_SearchError msg_SearchValidityManager::InitNewsTable ()
{
	XP_ASSERT (NULL == m_newsTable);
	MSG_SearchError err = NewTable (&m_newsTable);

	if (SearchError_Success == err)
	{
		m_newsTable->SetAvailable (attribSender, opContains, 1);
		m_newsTable->SetEnabled   (attribSender, opContains, 1);
		m_newsTable->SetAvailable (attribSender, opIs, 1);
		m_newsTable->SetEnabled   (attribSender, opIs, 1);
		m_newsTable->SetAvailable (attribSender, opBeginsWith, 1);
		m_newsTable->SetEnabled   (attribSender, opBeginsWith, 1);
		m_newsTable->SetAvailable (attribSender, opEndsWith, 1);
		m_newsTable->SetEnabled   (attribSender, opEndsWith, 1);

		m_newsTable->SetAvailable (attribSubject, opContains, 1);
		m_newsTable->SetEnabled   (attribSubject, opContains, 1);
		m_newsTable->SetAvailable (attribSubject, opIs, 1);
		m_newsTable->SetEnabled   (attribSubject, opIs, 1);
		m_newsTable->SetAvailable (attribSubject, opBeginsWith, 1);
		m_newsTable->SetEnabled   (attribSubject, opBeginsWith, 1);
		m_newsTable->SetAvailable (attribSubject, opEndsWith, 1);
		m_newsTable->SetEnabled   (attribSubject, opEndsWith, 1);

	}

	return err;
}


MSG_SearchError msg_SearchValidityManager::InitNewsExTable (MSG_NewsHost *newsHost)
{
	MSG_SearchError err = SearchError_Success;

	if (!m_newsExTable)
		err = NewTable (&m_newsExTable);

	if (SearchError_Success == err)
	{
		XP_Bool hasAttrib = newsHost ? newsHost->QuerySearchableHeader("FROM") : TRUE;
		m_newsExTable->SetAvailable (attribSender, opContains, hasAttrib);
		m_newsExTable->SetEnabled   (attribSender, opContains, hasAttrib);
		m_newsExTable->SetAvailable (attribSender, opDoesntContain, hasAttrib);
		m_newsExTable->SetEnabled   (attribSender, opDoesntContain, hasAttrib);

		hasAttrib = newsHost ? newsHost->QuerySearchableHeader ("SUBJECT") : TRUE;
		m_newsExTable->SetAvailable (attribSubject, opContains, hasAttrib);
		m_newsExTable->SetEnabled   (attribSubject, opContains, hasAttrib);
		m_newsExTable->SetAvailable (attribSubject, opDoesntContain, hasAttrib);
		m_newsExTable->SetEnabled   (attribSubject, opDoesntContain, hasAttrib);

		hasAttrib = newsHost ? newsHost->QuerySearchableHeader ("DATE") : TRUE;
		m_newsExTable->SetAvailable (attribDate, opIsBefore, hasAttrib);
		m_newsExTable->SetEnabled   (attribDate, opIsBefore, hasAttrib);
		m_newsExTable->SetAvailable (attribDate, opIsAfter, hasAttrib);
		m_newsExTable->SetEnabled   (attribDate, opIsAfter, hasAttrib);

		hasAttrib = newsHost ? newsHost->QuerySearchableHeader (":TEXT") : TRUE;
		m_newsExTable->SetAvailable (attribAnyText, opContains, hasAttrib);
		m_newsExTable->SetEnabled   (attribAnyText, opContains, hasAttrib);
		m_newsExTable->SetAvailable (attribAnyText, opDoesntContain, hasAttrib);
		m_newsExTable->SetEnabled   (attribAnyText, opDoesntContain, hasAttrib);

		hasAttrib = newsHost ? newsHost->QuerySearchableHeader ("KEYWORDS") : TRUE;
		m_newsExTable->SetAvailable (attribKeywords, opContains, hasAttrib);
		m_newsExTable->SetEnabled   (attribKeywords, opContains, hasAttrib);
		m_newsExTable->SetAvailable (attribKeywords, opDoesntContain, hasAttrib);
		m_newsExTable->SetEnabled   (attribKeywords, opDoesntContain, hasAttrib);

#ifdef LATER
		// Not sure whether this would be useful or not. If so, can we specify more
		// than one NEWSGROUPS term to the server? If not, it would be tricky to merge
		// this with the NEWSGROUPS term we generate for the scope.
		hasAttrib = newsHost ? newsHost->QuerySearchableHeader("NEWSGROUPS") : TRUE;
		m_newsExTable->SetAvailable (attribNewsgroups, opIsBefore, hasAttrib);
		m_newsExTable->SetEnabled   (attribNewsgroups, opIsBefore, hasAttrib);
		m_newsExTable->SetAvailable (attribNewsgroups, opIsAfter, hasAttrib);
		m_newsExTable->SetEnabled   (attribNewsgroups, opIsAfter, hasAttrib);
#endif
		hasAttrib = newsHost ? newsHost->QuerySearchableHeader("DATE") : TRUE;
		m_newsExTable->SetAvailable (attribAgeInDays, opIsGreaterThan, hasAttrib);
		m_newsExTable->SetEnabled   (attribAgeInDays, opIsGreaterThan, hasAttrib);
		m_newsExTable->SetAvailable (attribAgeInDays, opIsLessThan,  hasAttrib);
		m_newsExTable->SetEnabled   (attribAgeInDays, opIsLessThan, hasAttrib);
		m_newsExTable->SetAvailable (attribAgeInDays, opIs,  hasAttrib);
		m_newsExTable->SetEnabled   (attribAgeInDays, opIs, hasAttrib);

		// it is possible that the user enters an arbitrary header that is not searchable using NNTP search extensions
		m_newsExTable->SetAvailable (attribOtherHeader, opContains, 1);   // added for arbitrary headers
		m_newsExTable->SetEnabled   (attribOtherHeader, opContains, 1);
		m_newsExTable->SetAvailable (attribOtherHeader, opDoesntContain, 1);
		m_newsExTable->SetEnabled   (attribOtherHeader, opDoesntContain, 1);
	}

	return err;
}


MSG_SearchError msg_SearchValidityManager::PostProcessValidityTable (MSG_NewsHost *host)
{
	return InitNewsExTable (host);
}
