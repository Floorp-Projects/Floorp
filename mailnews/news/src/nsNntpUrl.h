/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef nsNntpUrl_h__
#define nsNntpUrl_h__

#include "nsINntpUrl.h"
#include "nsMsgMailNewsUrl.h"
#include "nsINNTPNewsgroupPost.h"
#include "nsFileSpec.h"
#include "nsIFileSpec.h"

class nsNntpUrl : public nsINntpUrl, public nsMsgMailNewsUrl, public nsIMsgMessageUrl, public nsIMsgI18NUrl
{
public:
  NS_DECL_NSINNTPURL
  NS_DECL_NSIMSGMESSAGEURL
  NS_DECL_NSIMSGI18NURL

  // nsIURI over-ride...
	NS_IMETHOD SetSpec(const nsACString &aSpec);

	NS_IMETHOD IsUrlType(PRUint32 type, PRBool *isType);

  // nsIMsgFolder override
  NS_IMETHOD GetFolder(nsIMsgFolder **msgFolder);

  // nsNntpUrl
  nsNntpUrl();
  virtual ~nsNntpUrl();

  NS_DECL_ISUPPORTS_INHERITED

private:
	virtual const char * GetUserName() { return nsnull; }
  nsresult DetermineNewsAction();

  nsINNTPNewsgroupPost *m_newsgroupPost;
	nsNewsAction m_newsAction; // the action this url represents...parse mailbox, display messages, etc.
 
  nsCString mURI; // the RDF URI associated with this url.
  nsCString mCharsetOverride; // used by nsIMsgI18NUrl...

  nsCString mOriginalSpec;
  nsFileSpec	*m_filePath; 
    
  // used by save message to disk
	nsCOMPtr<nsIFileSpec> m_messageFileSpec;

  PRPackedBool  m_addDummyEnvelope;
  PRPackedBool  m_canonicalLineEnding;
	PRPackedBool	m_getOldMessages;
};

#endif // nsNntpUrl_h__
