/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "msgCore.h"
#include "nsMsgSearchCore.h"
#include "nsMsgSearchAdapter.h"
#include "nsMsgSearchScopeTerm.h"
#include "nsMsgI18N.h"
#include "nsIPref.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

NS_IMETHODIMP nsMsgSearchAdapter::FindTargetFolder(const nsMsgResultElement *,nsIMsgFolder * *)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgSearchAdapter::ModifyResultElement(nsMsgResultElement *, nsMsgSearchValue *)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgSearchAdapter::OpenResultElement(nsMsgResultElement *)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

nsMsgSearchAdapter::nsMsgSearchAdapter(nsMsgSearchScopeTerm *scope, nsMsgSearchTermArray &searchTerms) 
	: m_searchTerms(searchTerms)
{
}

nsMsgSearchAdapter::~nsMsgSearchAdapter()
{
}

NS_IMETHODIMP nsMsgSearchAdapter::ValidateTerms ()
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgSearchAdapter::Abort ()
{
	return NS_ERROR_NOT_IMPLEMENTED;

}

void
nsMsgSearchAdapter::GetSearchCharsets(nsString &srcCharset, nsString& dstCharset)
{
//  char *defaultCharset =   nsMsgI18NGetDefaultMailCharset();
  nsAutoString defaultCharset = nsMsgI18NGetDefaultMailCharset();
	srcCharset = defaultCharset;
	dstCharset = defaultCharset;

	if (m_scope)
	{
    // ### DMB is there a way to get the charset for the "window"?

		// Ask the newsgroup/folder for its csid.
		if (m_scope->m_folder)
    {
      nsXPIDLString folderCharset;
			m_scope->m_folder->GetCharset(getter_Copies(folderCharset));
      dstCharset = folderCharset;
    }
	}


	// If 
	// the destination is still CS_DEFAULT, make the destination match
	// the source. (CS_DEFAULT is an indication that the charset
	// was undefined or unavailable.)
  // ### well, it's not really anymore. Is there an equivalent?
  if (!nsCRT::strcmp(dstCharset.GetUnicode(), defaultCharset.GetUnicode()))
		dstCharset = srcCharset;

	PRBool forceAscii = PR_FALSE;
  nsresult rv;

	NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);

	if (NS_SUCCEEDED(rv))
	{
		rv = prefs->GetBoolPref("mailnews.force_ascii_search", &forceAscii);
	}

	if (forceAscii)
	{
		// Special cases to use in order to force US-ASCII searching with Latin1
		// or MacRoman text. Eurgh. This only has to happen because IMAP
		// and Dredd servers currently (4/23/97) only support US-ASCII.
		// 
		// If the dest csid is ISO Latin 1 or MacRoman, attempt to convert the 
		// source text to US-ASCII. (Not for now.)
		// if ((dst_csid == CS_LATIN1) || (dst_csid == CS_MAC_ROMAN))
			dstCharset = "us-ascii";
	}
}

