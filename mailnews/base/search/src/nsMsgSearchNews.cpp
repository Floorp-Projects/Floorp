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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "msgCore.h"
#include "nsMsgSearchAdapter.h"
#include "nsXPIDLString.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsMsgSearchScopeTerm.h"
#include "nsMsgResultElement.h"
#include "nsMsgSearchTerm.h"
#include "nsIMsgHdr.h"
#include "nsMsgSearchNews.h"
#include "nsIDBFolderInfo.h"
#include "prprf.h"

// Implementation of search for IMAP mail folders


// Implementation of search for newsgroups


//-----------------------------------------------------------------------------
//----------- Adapter class for searching XPAT-capable news servers -----------
//-----------------------------------------------------------------------------


const char *nsMsgSearchNews::m_kNntpFrom = "FROM ";
const char *nsMsgSearchNews::m_kNntpSubject = "SUBJECT ";
const char *nsMsgSearchNews::m_kTermSeparator = "/";


nsMsgSearchNews::nsMsgSearchNews (nsMsgSearchScopeTerm *scope, nsISupportsArray *termList) : nsMsgSearchAdapter (scope, termList)
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


nsresult nsMsgSearchNews::Search (PRBool *aDone)
{
	// the state machine runs in the news: handler
	nsresult err = NS_ERROR_NOT_IMPLEMENTED;
	return err;
}

PRUnichar *nsMsgSearchNews::EncodeToWildmat (const PRUnichar *value)
{
	// Here we take advantage of XPAT's use of the wildmat format, which allows
	// a case-insensitive match by specifying each case possibility for each character
	// So, "FooBar" is encoded as "[Ff][Oo][Bb][Aa][Rr]"

  PRUnichar *caseInsensitiveValue = (PRUnichar*) nsMemory::Alloc(sizeof(PRUnichar) * ((4 * nsCRT::strlen(value)) + 1));
	if (caseInsensitiveValue)
	{
		PRUnichar *walkValue = caseInsensitiveValue;
		while (*value)
		{
			if (nsCRT::IsAsciiAlpha(*value))
			{
				*walkValue++ = (PRUnichar)'[';
				*walkValue++ = ToUpperCase((PRUnichar)*value);
				*walkValue++ = ToLowerCase((PRUnichar)*value);
				*walkValue++ = (PRUnichar)']';
			}
			else
				*walkValue++ = *value;
			value++;
		}
		*walkValue = 0;
	}
	return caseInsensitiveValue;
}


char *nsMsgSearchNews::EncodeTerm (nsIMsgSearchTerm *term)
{
	// Develop an XPAT-style encoding for the search term

	NS_ASSERTION(term, "null term");
	if (!term)
		return nsnull;

	// Find a string to represent the attribute
	const char *attribEncoding = nsnull;
  nsMsgSearchAttribValue attrib;

  term->GetAttrib(&attrib);

	switch (attrib)
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
  nsMsgSearchOpValue op;
  term->GetOp(&op);

	switch (op)
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
  nsCOMPtr <nsIMsgSearchValue> searchValue;

  nsresult rv = term->GetValue(getter_AddRefs(searchValue));
  if (NS_FAILED(rv) || !searchValue)
    return nsnull;


  nsXPIDLString intlNonRFC1522Value;
  rv = searchValue->GetStr(getter_Copies(intlNonRFC1522Value));
	if (NS_FAILED(rv) || !intlNonRFC1522Value)
		return nsnull;
		
	PRUnichar *caseInsensitiveValue = EncodeToWildmat (intlNonRFC1522Value);
	if (!caseInsensitiveValue)
		return nsnull;

	// TO DO: Do INTL_FormatNNTPXPATInRFC1522Format trick for non-ASCII string
	// Unfortunately, we currently do not handle xxx or xxx search in XPAT
	// Need to add the INTL_FormatNNTPXPATInRFC1522Format call after we can do that
	// so we should search a string in either RFC1522 format and non-RFC1522 format
		
	PRUnichar *escapedValue = EscapeSearchUrl (caseInsensitiveValue);
	nsMemory::Free(caseInsensitiveValue);
	if (!escapedValue)
		return nsnull;

#if 0
	// We also need to apply NET_Escape to it since we have to pass 8-bits data
	// And sometimes % in the 7-bit doulbe byte JIS
	// 
	PRUnichar * urlEncoded = nsEscape(escapedValue, url_Path);
	nsCRT::free(escapedValue);

	if (! urlEncoded)
		return nsnull;

	char *pattern = new char [nsCRT::strlen(urlEncoded) + overhead];
	if (!pattern)
		return nsnull;
	else 
		pattern[0] = '\0';
#else
    nsCAutoString pattern;
#endif

    
	if (leadingStar)
      pattern.Append('*');
    AppendUTF16toUTF8(escapedValue, pattern);
	if (trailingStar)
      pattern.Append('*');

	// Combine the XPAT command syntax with the attribute and the pattern to
	// form the term encoding
	const char xpatTemplate[] = "XPAT %s 1- %s";
	int termLength = (sizeof(xpatTemplate) - 1) + strlen(attribEncoding) + pattern.Length() + 1;
	char *termEncoding = new char [termLength];
	if (termEncoding)
		PR_snprintf (termEncoding, termLength, xpatTemplate, attribEncoding, pattern.get());

	return termEncoding;
}

nsresult nsMsgSearchNews::GetEncoding(char **result)
{
  NS_ENSURE_ARG(result);
  *result = ToNewCString(m_encoding);
  return (*result) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult nsMsgSearchNews::Encode (nsCString *outEncoding)
{
	NS_ASSERTION(outEncoding, "no out encoding");
	if (!outEncoding)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;

  PRUint32 numTerms;

  m_searchTerms->Count(&numTerms);
	char **intermediateEncodings = new char * [numTerms];
	if (intermediateEncodings)
	{
		// Build an XPAT command for each term
		int encodingLength = 0;
		PRUint32 i;
		for (i = 0; i < numTerms; i++)
		{
      nsCOMPtr<nsIMsgSearchTerm> pTerm;
      m_searchTerms->QueryElementAt(i, NS_GET_IID(nsIMsgSearchTerm),
                               (void **)getter_AddRefs(pTerm));
			// set boolean OR term if any of the search terms are an OR...this only works if we are using
			// homogeneous boolean operators.
      PRBool isBooleanOpAnd;
      pTerm->GetBooleanAnd(&isBooleanOpAnd);
			m_ORSearch = !isBooleanOpAnd;
		
			intermediateEncodings[i] = EncodeTerm (pTerm);	
			if (intermediateEncodings[i])
				encodingLength += strlen(intermediateEncodings[i]) + strlen(m_kTermSeparator);
		}
		encodingLength += strlen("?search");
		// Combine all the term encodings into one big encoding
		char *encoding = new char [encodingLength + 1];
		if (encoding)
		{
			PL_strcpy (encoding, "?search");

      m_searchTerms->Count(&numTerms);

			for (i = 0; i < numTerms; i++)
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

/* void CurrentUrlDone (in long exitCode); */
NS_IMETHODIMP nsMsgSearchNews::CurrentUrlDone(PRInt32 exitCode)
{
	CollateHits();
	ReportHits();
  return NS_OK;
}


#if 0 // need to switch this to a notify stop loading handler, I think.
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

	PRUint32 termCount;
  m_searchTerms->Count(&termCount);
	PRUint32 candidateCount = 0;
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

void nsMsgSearchNews::ReportHits ()
{
  nsCOMPtr <nsIMsgDatabase> db;
  nsCOMPtr <nsIDBFolderInfo>  folderInfo;
  nsCOMPtr <nsIMsgFolder> scopeFolder;

  nsresult err = m_scope->GetFolder(getter_AddRefs(scopeFolder));
  if (NS_SUCCEEDED(err) && scopeFolder)
  {
    err = scopeFolder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
  }

  if (db)
  {
	  for (PRUint32 i = 0; i < m_hits.GetSize(); i++)
	  {
      nsCOMPtr <nsIMsgDBHdr> header;

		  db->GetMsgHdrForKey(m_hits.ElementAt(i), getter_AddRefs(header));
		  if (header)
			  ReportHit(header, scopeFolder);
	  }
  }
}

// ### this should take an nsIMsgFolder instead of a string location.
void nsMsgSearchNews::ReportHit (nsIMsgDBHdr *pHeaders, nsIMsgFolder *folder)
{
    // this is totally filched from msg_SearchOfflineMail until I decide whether the 
    // right thing is to get them from the db or from NNTP

    nsresult err = NS_OK;
    nsCOMPtr<nsIMsgSearchSession> session;
    nsCOMPtr <nsIMsgFolder> scopeFolder;
    err = m_scope->GetFolder(getter_AddRefs(scopeFolder));
    m_scope->GetSearchSession(getter_AddRefs(session));
    if (session)
      session->AddSearchHit (pHeaders, scopeFolder);
}



int PR_CALLBACK nsMsgSearchNews::CompareArticleNumbers (const void *v1, const void *v2, void *data)
{
	// QuickSort callback to compare article numbers

	uint32 i1 = *(uint32*) v1;
	uint32 i2 = *(uint32*) v2;
	return i1 - i2;
}

nsresult nsMsgSearchValidityManager::InitNewsTable()
{
  NS_ASSERTION (nsnull == m_newsTable,"don't call this twice!");
  nsresult rv = NewTable (getter_AddRefs(m_newsTable));
  
  if (NS_SUCCEEDED(rv))
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
  
  return rv;
}

nsresult nsMsgSearchValidityManager::InitNewsFilterTable()
{
  NS_ASSERTION (nsnull == m_newsFilterTable, "news filter table already initted");
  nsresult rv = NewTable (getter_AddRefs(m_newsFilterTable));
  
  if (NS_SUCCEEDED(rv))
  {
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);
    
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsInAB, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsInAB, 1);
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsntInAB, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsntInAB, 1);
    
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);
    
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
    m_newsFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
    m_newsFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
  }
  
  return rv;
}
