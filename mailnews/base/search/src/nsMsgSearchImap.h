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

#ifndef _nsMsgSearchImap_h__
#include "nsMsgSearchAdapter.h"

//-----------------------------------------------------------------------------
//---------- Adapter class for searching online (IMAP) folders ----------------
//-----------------------------------------------------------------------------

class nsMsgSearchOnlineMail : public nsMsgSearchAdapter
{
public:
	nsMsgSearchOnlineMail (nsMsgSearchScopeTerm *scope, nsMsgSearchTermArray &termList);
	virtual ~nsMsgSearchOnlineMail ();

	NS_IMETHOD ValidateTerms ();
	NS_IMETHOD Search ();
	NS_IMETHOD GetEncoding (char **result);

	static nsresult Encode (nsCString *ppEncoding, nsMsgSearchTermArray &searchTerms, const PRUnichar *srcCharset, const PRUnichar *destCharset);
	
	nsresult AddResultElement (nsIMsgDBHdr *);

protected:
	nsCString m_encoding;
};



#endif

