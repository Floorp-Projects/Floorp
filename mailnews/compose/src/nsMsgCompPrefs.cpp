/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgIdentity.h"
#include "nsMsgCompPrefs.h"
#include "nsMsgBaseCID.h"
#include "nsIPref.h"

static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

nsMsgCompPrefs::nsMsgCompPrefs()
{
	nsresult res;

	m_wrapColumn = 72;

  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &res)); 
  if (NS_SUCCEEDED(res) && prefs) 
    {
      res = prefs->GetIntPref("mail.wraplength", &m_wrapColumn);
    }
}

nsMsgCompPrefs::~nsMsgCompPrefs()
{
}
