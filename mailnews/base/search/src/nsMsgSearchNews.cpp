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
#include "nsMsgSearchNews.h"
#include "nsINNTPHost.h"

// Implementation of search for IMAP mail folders


// Implementation of search for newsgroups


//-----------------------------------------------------------------------------
//----------- Adapter class for searching XPAT-capable news servers -----------
//-----------------------------------------------------------------------------


const char *nsMsgSearchNews::m_kNntpFrom = "FROM ";
const char *nsMsgSearchNews::m_kNntpSubject = "SUBJECT ";
const char *nsMsgSearchNews::m_kTermSeparator = "/";


nsMsgSearchNews::nsMsgSearchNews (nsMsgSearchScopeTerm *scope, nsMsgSearchTermArray &termList) : nsMsgSearchAdapter (scope, termList)
{
}


nsMsgSearchNews::~nsMsgSearchNews ()
{
}


nsresult nsMsgSearchNews::ValidateTerms ()
{
	nsresult err = nsMsgSearchAdapter::ValidateTerms ();
	if (NS_OK == err)
	{
		err = Encode (&m_encoding);
	}

	return err;
}


nsresult nsMsgSearchNews::Search ()
{
	// the state machine runs in the news: handler
	nsresult err = NS_ERROR_NOT_IMPLEMENTED;
	return err;
}


char *nsMsgSearchNews::EncodeValue (const char *value)
{
	// Here we take advantage of XPAT's use of the wildmat format, which allows
	// a case-insensitive match by specifying each case possibility for each character
	// So, "FooBar" is encoded as "[Ff][Oo][Bb][Aa][Rr]"

    char *caseInsensitiveValue = (char*) PR_Malloc ((4 * nsCRT::strlen(value)) + 1);
	if (caseInsensitiveValue)
	{
		char *walkValue = caseInsensitiveValue;
		while (*value)
		{
			if (nsCRT::IsAsciiAlpha(*value))
			{
				*walkValue++ = '[';
				*walkValue++ = (char)nsCRT::ToUpper((PRUnichar)*value);
				*walkValue++ = (char)nsCRT::ToLower((PRUnichar)*value);
				*walkValue++ = ']';
			}
			else
				*walkValue++ = *value;
			value++;
		}
		*walkValue = '\0';
	}
	return caseInsensitiveValue;
}


char *nsMsgSearchNews::EncodeTerm (nsMsgSearchTerm *term)
{
	// Develop an XPAT-style encoding for the search term

	NS_ASSERTION(term, "null term");
	if (!term)
		return nsnull;

	// Find a string to represent the attribute
	const char *attribEncoding = nsnull;
	switch (term->m_attribute)
	{
	case nsMsgSearchAttrib::Sender:
		attribEncoding = m_kNntpFrom;
		break;
	case nsMsgSearchAttrib::Subject:
		attribEncoding = m_kNntpSubject;
		break;
	default:
		NS_ASSERTION(PR_FALSE,"malformed search"); // malformed search term?
		return nsnull;
	}

	// Build a string to represent the string pattern
	PRBool leadingStar = PR_FALSE;
	PRBool trailingStar = PR_FALSE;
	int overhead = 1; // null terminator
	switch (term->m_operator)
	{
	case nsMsgSearchOp::Contains:
		leadingStar = PR_TRUE;
		trailingStar = PR_TRUE;
		overhead += 2;
		break;
	case nsMsgSearchOp::Is:
		break;
	case nsMsgSearchOp::BeginsWith:
		trailingStar = PR_TRUE;
		overhead++;
		break;
	case nsMsgSearchOp::EndsWith:
		leadingStar = PR_TRUE;
		overhead++;
		break;
	default:
		NS_ASSERTION(PR_FALSE,"malformed search"); // malformed search term?
		return nsnull;
	}
	
    // ### i18N problem Get the csid from FE, which is the correct csid for term
//	int16 wincsid = INTL_GetCharSetID(INTL_DefaultTextWidgetCsidSel);

	// Do INTL_FormatNNTPXPATInRFC1522Format trick for non-ASCII string
//	unsigned char *intlNonRFC1522Value = INTL_FormatNNTPXPATInNonRFC1522Format (wincsid, (unsigned char*)term->m_value.u.string);
    char *intlNonRFC1522Value = nsCRT::strdup((const char*)term->m_value.u.string);
	if (!intlNonRFC1522Value)
		return nsnull;
		
	char *caseInsensitiveValue = EncodeValue ((char*)intlNonRFC1522Value);
	nsCRT::free(intlNonRFC1522Value);
	if (!caseInsensitiveValue)
		return nsnull;

	// TO DO: Do INTL_FormatNNTPXPATInRFC1522Format trick for non-ASCII string
	// Unfortunately, we currently do not handle xxx or xxx search in XPAT
	// Need to add the INTL_FormatNNTPXPATInRFC1522Format call after we can do that
	// so we should search a string in either RFC1522 format and non-RFC1522 format
		
	char *escapedValue = EscapeSearchUrl (caseInsensitiveValue);
	nsCRT::free(caseInsensitiveValue);
	if (!escapedValue)
		return nsnull;

	// We also need to apply NET_Escape to it since we have to pass 8-bits data
	// And sometimes % in the 7-bit doulbe byte JIS
	// 
	char * urlEncoded = nsEscape((char*)escapedValue, url_Path);
	nsCRT::free(escapedValue);

	if (! urlEncoded)
		return nsnull;

	char *pattern = pattern = new char [nsCRT::strlen(urlEncoded) + overhead];
	if (!pattern)
		return nsnull;
	else 
		pattern[0] = '\0';

	if (leadingStar)
		PL_strcat (pattern, "*");
	PL_strcat (pattern, urlEncoded);
	if (trailingStar)
		PL_strcat (pattern, "*");

	// Combine the XPAT command syntax with the attribute and the pattern to
	// form the term encoding
	char *xpatTemplate = "XPAT %s 1- %s";
	int termLength = nsCRT::strlen(xpatTemplate) + nsCRT::strlen(attribEncoding) + nsCRT::strlen(pattern) + 1;
	char *termEncoding = new char [termLength];
	if (termEncoding)
		PR_snprintf (termEncoding, termLength, xpatTemplate, attribEncoding, pattern);

	nsCRT::free(urlEncoded);
	delete [] pattern;

	return termEncoding;
}

nsresult nsMsgSearchNews::GetEncoding(char **result)
{
  NS_ENSURE_ARG(result);
  *result = m_encoding.ToNewCString();
  return (*result) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult nsMsgSearchNews::Encode (nsCString *outEncoding)
{
	NS_ASSERTION(outEncoding, "no out encoding");
	if (!outEncoding)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;

	char **intermediateEncodings = new char * [m_searchTerms.Count()];
	if (intermediateEncodings)
	{
		// Build an XPAT command for each term
		int encodingLength = 0;
		int i;
		for (i = 0; i < m_searchTerms.Count(); i++)
		{
			nsMsgSearchTerm * term = m_searchTerms.ElementAt(i);
			// set boolean OR term if any of the search terms are an OR...this only works if we are using
			// homogeneous boolean operators.
			m_ORSearch = !(term->IsBooleanOpAND());
		
			intermediateEncodings[i] = EncodeTerm (m_searchTerms.ElementAt(i));	
			if (intermediateEncodings[i])
				encodingLength += nsCRT::strlen(intermediateEncodings[i]) + nsCRT::strlen(m_kTermSeparator);
		}
		encodingLength += nsCRT::strlen("?search");
		// Combine all the term encodings into one big encoding
		char *encoding = new char [encodingLength + 1];
		if (encoding)
		{
			PL_strcpy (encoding, "?search");
			for (i = 0; i < m_searchTerms.Count(); i++)
			{
				if (intermediateEncodings[i])
				{
					PL_strcat (encoding, m_kTermSeparator);
					PL_strcat (encoding, intermediateEncodings[i]);
					delete [] intermediateEncodings[i];
				}
			}
			*outEncoding = encoding;
		}
		else
			err = NS_ERROR_OUT_OF_MEMORY;
	}
	else
		err = NS_ERROR_OUT_OF_MEMORY;
	delete [] intermediateEncodings;

	return err;
}

NS_IMETHODIMP nsMsgSearchNews::AddHit(nsMsgKey key)
{
  m_candidateHits.Add (key); 
  return NS_OK;
}

#if 0
// Callback from libnet
void MSG_AddNewsXpatHit (MWContext *context, uint32 artNum)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromContext(context);
	nsMsgSearchNews *adapter = (nsMsgSearchNews*) frame->GetRunningAdapter();
	adapter->AddHit (artNum);
}


void nsMsgSearchNews::PreExitFunction (URL_Struct * /*url*/, int status, MWContext *context)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromContext (context);
	nsMsgSearchNews *adapter = (nsMsgSearchNews*) frame->GetRunningAdapter();
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
		if (frame->m_idxRunningScope >= frame->m_scopeList.Count())
			frame->EndCylonMode();
	}
}
#endif // 0

PRBool nsMsgSearchNews::DuplicateHit(PRUint32 artNum)  
// ASSUMES m_hits is sorted!!
{
	PRUint32 index;
	for (index = 0; index < m_hits.GetSize(); index++)
		if (artNum == m_hits.ElementAt(index))
			return PR_TRUE;
	return PR_FALSE;
}


void nsMsgSearchNews::CollateHits ()
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
	PRUint32 candidate = m_candidateHits.ElementAt(index);

	if (m_ORSearch)
	{
		for (index = 0; index < size; index++)
		{
			candidate = m_candidateHits.ElementAt(index);
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

	int termCount = m_searchTerms.Count();
	int candidateCount = 0;
	while (index < size)
	{
		if (candidate == m_candidateHits.ElementAt(index))
			candidateCount++;
		else
			candidateCount = 1;
		if (candidateCount == termCount)
			m_hits.Add (m_candidateHits.ElementAt(index));
		candidate = m_candidateHits.ElementAt(index++);
	}
}

#ifdef OLD_WAY
void nsMsgSearchNews::ReportHits ()
{
	XP_ASSERT (m_scope->m_folder->IsNews());
	if (!m_scope->m_folder->IsNews())
		return;
	
	MSG_FolderInfoNews *folder = (MSG_FolderInfoNews*) m_scope->m_folder;

	// Construct a URL for the newsgroup, since thats what newDB::open wants
	char *url = folder->BuildUrl(nsnull, MSG_MESSAGEKEYNONE);
	if (!url)
		return;

	NewsGroupDB	*newsDB = nsnull;
	MsgERR status = NewsGroupDB::Open(url, m_scope->m_frame->m_pane->GetMaster(), &newsDB);
	if (status == eSUCCESS)
	{
		XP_ASSERT(newsDB);
		if (!newsDB)
			return;

		CRTFREEIF(url);

		for (uint32 i = 0; i < m_hits.Count(); i++)
		{
			NewsMessageHdr *header = newsDB->GetNewsHdrForKey(m_hits.ElementAt(i));
			if (header)
			{
				ReportHit(header);
				header->unrefer();
			}
		}
		newsDB->Close();
	}
}
#endif // OLDWAY

void nsMsgSearchNews::ReportHit (nsIMsgDBHdr *pHeaders, const char *location)
{
    // this is totally filched from msg_SearchOfflineMail until I decide whether the 
    // right thing is to get them from the db or from NNTP

    nsresult err = NS_OK;
    nsMsgResultElement *newResult = new nsMsgResultElement (this);

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
		    pValue->attribute = nsMsgSearchAttrib::Sender;
        nsXPIDLCString author;
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
			    pValue->attribute = nsMsgSearchAttrib::Location;
			    pValue->u.string = PL_strdup(location); 
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
        nsXPIDLCString messageId;
        pValue->u.string = (char*) PR_Malloc(64);
        if (pValue->u.string)
        {
  			  pValue->attribute = nsMsgSearchAttrib::MessageId;
            pHeaders->GetMessageId(getter_Copies(messageId));
            PL_strncpy(pValue->u.string, (const char *) messageId, 64);
            newResult->AddValue (pValue);
        }
	    }
	    if (!pValue)
		    err = NS_ERROR_OUT_OF_MEMORY;
        nsCOMPtr<nsIMsgSearchSession> session;
        m_scope->GetSearchSession(getter_AddRefs(session));
        session->AddResultElement (newResult);
    }
}



int nsMsgSearchNews::CompareArticleNumbers (const void *v1, const void *v2, void *data)
{
	// QuickSort callback to compare article numbers

	uint32 i1 = *(uint32*) v1;
	uint32 i2 = *(uint32*) v2;
	return i1 - i2;
}


//-----------------------------------------------------------------------------
//-------- Adapter class for searching SEARCH-capable news servers ------------
//-----------------------------------------------------------------------------


const char *nsMsgSearchNewsEx::m_kSearchTemplate = "?search/SEARCH HEADER NEWSGROUPS %s %s";
const char *nsMsgSearchNewsEx::m_kProfileTemplate = "%s/dummy?profile/PROFILE NEW %s HEADER NEWSGROUPS %s %s";

nsMsgSearchNewsEx::nsMsgSearchNewsEx (nsMsgSearchScopeTerm *scope, nsMsgSearchTermArray &termList) : nsMsgSearchNews (scope, termList) 
{
}


nsMsgSearchNewsEx::~nsMsgSearchNewsEx () 
{
}


nsresult nsMsgSearchNewsEx::ValidateTerms ()
{
	nsresult err = nsMsgSearchAdapter::ValidateTerms ();
	if (NS_OK == err)
		err = Encode (&m_encoding);
  // this used to create a url struct for some reason.
	return err;
}

nsresult nsMsgSearchNewsEx::Search ()
{
	// State machine runs in mknews.c?
	return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsMsgSearchNewsEx::Encode (nsCString *ppOutEncoding)
{
	*ppOutEncoding = "";
	char *imapTerms = nsnull;

	// Figure out the charsets to use for the search terms and targets.
	nsString srcCharset, dstCharset;
	GetSearchCharsets(srcCharset, dstCharset);

	nsresult err = EncodeImap (&imapTerms, m_searchTerms, srcCharset.GetUnicode(), dstCharset.GetUnicode(), PR_TRUE /*reallyDredd*/);
#ifdef DOING_DREDD
	if (NS_OK == err)
	{
		char *scopeString = nsnull; 
		err = m_scope->m_frame->EncodeDreddScopes (&scopeString);
		if (NS_OK == err)
		{
			// Wrap the pattern with the RFC-977bis (Dredd) specified SEARCH syntax
			char *dreddEncoding = PR_smprintf (m_kSearchTemplate, scopeString, imapTerms);
			if (dreddEncoding)
			{
				// Build the encoding part of the URL e.g. search?SEARCH FROM "John Smith"
				*ppOutEncoding = dreddEncoding;
				nsCRT::free(dreddEncoding);
			}
			else
				err = NS_ERROR_OUT_OF_MEMORY;
			nsCRT::free(scopeString);
		}
	}
#endif
	return err;
}

#ifdef DOING_PROFILES
nsresult nsMsgSearchNewsEx::SaveProfile (const char *profileName)
{
	nsresult err = NS_OK;
	MSG_FolderInfo *folder = m_scope->m_folder;

	// Figure out which news host to fire the URL at. Maybe we should have a virtual function in MSG_FolderInfo for this?
	MSG_NewsHost *host = nsnull;
	MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();
	if (newsFolder)
		host = newsFolder->GetHost();
	else if (FOLDER_CONTAINERONLY == folder->GetType())
		host = ((MSG_NewsFolderInfoContainer*) folder)->GetHost();

	XP_ASSERT(nsnull != host && nsnull != profileName);
	if (nsnull != host && nsnull != profileName)
	{
		char *scopeString = nsnull;
		m_scope->m_frame->EncodeDreddScopes (&scopeString);

		// Figure out the charsets to use for the search terms and targets.
		int16 src_csid, dst_csid;
		GetSearchCSIDs(src_csid, dst_csid);

		char *termsString = nsnull;
		EncodeImap (&termsString, m_searchTerms, 
					src_csid, dst_csid,
					PR_TRUE /*reallyDredd*/);

		char *legalProfileName = PL_strdup(profileName);

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
					urlStruct->internal_url = PR_TRUE;

					// Set the pre_exit_fn to we can turn off cylon mode when we're done
					urlStruct->pre_exit_fn = PreExitFunctionEx;

					int getUrlErr = m_scope->m_frame->m_pane->GetURL (urlStruct, PR_FALSE);
					if (getUrlErr != 0)
						err = SearchError_ScopeAgreement; // ### not really. impedance mismatch
					else
						m_scope->m_frame->BeginCylonMode();
				}
				else
					err = NS_ERROR_OUT_OF_MEMORY;
				nsCRT::free(url);
			}
			else
				err = NS_ERROR_OUT_OF_MEMORY;
		}

		CRTFREEIF(scopeString);
		delete [] termsString;
		CRTFREEIF(legalProfileName);
	}
	return err;
}
#endif // DOING_PROFILES
#if OLDWAY
// Callback from libnet
SEARCH_API void MSG_AddNewsSearchHit (MWContext *context, const char *resultLine)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromContext (context);
	nsMsgSearchNewsEx *adapter = (nsMsgSearchNewsEx *) frame->GetRunningAdapter();
	if (adapter)
	{
		MessageHdrStruct hdr;
		XP_BZERO(&hdr, sizeof(hdr));

		// Here we make the SEARCH result compatible with xover conventions. In SEARCH, the
		// group name and a ':' precede the article number, so try to skip over this stuff
		// before asking NeoMessageHdr to parse it
		char *xoverCompatLine = XP_STRCHR(resultLine, ':');
		if (xoverCompatLine)
			xoverCompatLine++;
		else
			xoverCompatLine = (char*) resultLine; //### casting away const

		if (NeoMessageHdr::ParseLine ((char*) xoverCompatLine, &hdr)) //### casting away const
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


SEARCH_API nsresult MSG_SaveProfileStatus (MSG_Pane *searchPane, PRBool *cmdEnabled)
{
	nsresult err = NS_OK;

	XP_ASSERT(cmdEnabled);
	if (cmdEnabled)
	{
		*cmdEnabled = PR_FALSE;
		MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (searchPane);
		if (frame)
			*cmdEnabled = frame->GetSaveProfileStatus();
	}
	else
		err = NS_ERROR_NULL_POINTER;

	return err;
}


SEARCH_API nsresult MSG_SaveProfile (MSG_Pane *searchPane, const char * profileName)
{
	nsresult err = NS_OK;

#ifdef _DEBUG
	PRBool enabled = PR_FALSE;
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
			nsMsgSearchNewsEx *adapter = frame->GetProfileAdapter();
			XP_ASSERT(adapter);
			if (adapter)
				err = adapter->SaveProfile (profileName);
		}
	}
	else
		err = NS_ERROR_NULL_POINTER;

	return err;
}


SEARCH_API int MSG_AddProfileGroup (MSG_Pane *pane, MSG_NewsHost* host,
									const char *groupName)
{
	MSG_FolderInfoNews *group =
		pane->GetMaster()->AddProfileNewsgroup(host, groupName);
	return group ? 0 : -1;
}

#endif

nsresult nsMsgSearchValidityManager::InitNewsTable ()
{
	NS_ASSERTION (nsnull == m_newsTable,"don't call this twice!");
	nsresult err = NewTable (getter_AddRefs(m_newsTable));

	if (NS_OK == err)
	{
		m_newsTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
		m_newsTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
		m_newsTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
		m_newsTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
		m_newsTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
		m_newsTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
		m_newsTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);
		m_newsTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);

		m_newsTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
		m_newsTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
		m_newsTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
		m_newsTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
		m_newsTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
		m_newsTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
		m_newsTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);
		m_newsTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);

	}

	return err;
}


nsresult nsMsgSearchValidityManager::InitNewsExTable (nsINNTPHost *newsHost)
{
	nsresult err = NS_OK;

	if (!m_newsExTable)
		err = NewTable (getter_AddRefs(m_newsExTable));

	if (NS_OK == err)
	{
		PRBool hasAttrib = PR_TRUE;
    if (newsHost)
      newsHost->QuerySearchableHeader("FROM", &hasAttrib);
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, hasAttrib);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, hasAttrib);
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, hasAttrib);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, hasAttrib);

    if (newsHost)
      newsHost->QuerySearchableHeader("SUBJECT", &hasAttrib);
    else
      hasAttrib = PR_TRUE;
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, hasAttrib);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, hasAttrib);
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, hasAttrib);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, hasAttrib);

    if (newsHost)
      newsHost->QuerySearchableHeader("DATE", &hasAttrib);
    else
      hasAttrib = PR_TRUE;
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, hasAttrib);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, hasAttrib);
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, hasAttrib);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, hasAttrib);

    if (newsHost)
      newsHost->QuerySearchableHeader(":TEXT", &hasAttrib);
    else
      hasAttrib = PR_TRUE;
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::AnyText, nsMsgSearchOp::Contains, hasAttrib);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::AnyText, nsMsgSearchOp::Contains, hasAttrib);
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::AnyText, nsMsgSearchOp::DoesntContain, hasAttrib);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::AnyText, nsMsgSearchOp::DoesntContain, hasAttrib);

    if (newsHost)
      newsHost->QuerySearchableHeader("KEYWORDS", &hasAttrib);
    else
      hasAttrib = PR_TRUE;
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Contains, hasAttrib);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Contains, hasAttrib);
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::DoesntContain, hasAttrib);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::DoesntContain, hasAttrib);

#ifdef LATER
		// Not sure whether this would be useful or not. If so, can we specify more
		// than one NEWSGROUPS term to the server? If not, it would be tricky to merge
		// this with the NEWSGROUPS term we generate for the scope.
		hasAttrib = newsHost ? newsHost->QuerySearchableHeader("NEWSGROUPS") : PR_TRUE;
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::Newsgroups, nsMsgSearchOp::IsBefore, hasAttrib);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::Newsgroups, nsMsgSearchOp::IsBefore, hasAttrib);
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::Newsgroups, nsMsgSearchOp::IsAfter, hasAttrib);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::Newsgroups, nsMsgSearchOp::IsAfter, hasAttrib);
#endif
    if (newsHost)
      newsHost->QuerySearchableHeader("DATE", &hasAttrib);
    else
      hasAttrib = PR_TRUE;
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, hasAttrib);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, hasAttrib);
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan,  hasAttrib);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan, hasAttrib);
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is,  hasAttrib);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is, hasAttrib);

		// it is possible that the user enters an arbitrary header that is not searchable using NNTP search extensions
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);   // added for arbitrary headers
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
		m_newsExTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
		m_newsExTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
	}

	return err;
}


nsresult nsMsgSearchValidityManager::PostProcessValidityTable (nsINNTPHost *host)
{
	return InitNewsExTable (host);
}
