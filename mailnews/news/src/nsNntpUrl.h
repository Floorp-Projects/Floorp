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
	NS_IMETHOD SetSpec(const char * aSpec);

	NS_IMETHOD IsUrlType(PRUint32 type, PRBool *isType);

  // nsNntpUrl
  nsNntpUrl();
  virtual ~nsNntpUrl();

  NS_DECL_ISUPPORTS_INHERITED

protected:  
  virtual nsresult ParseUrl(const char * aSpec);
	virtual const char * GetUserName() { return nsnull; }
  nsINNTPNewsgroupPost *m_newsgroupPost;
	nsNewsAction m_newsAction; // the action this url represents...parse mailbox, display messages, etc.
    
  nsFileSpec	*m_filePath; 
    
    // used by save message to disk
	nsCOMPtr<nsIFileSpec> m_messageFileSpec;
  PRBool                m_addDummyEnvelope;
  PRBool                m_canonicalLineEnding;

  nsresult GetMsgFolder(nsIMsgFolder **msgFolder);

  nsCString mURI; // the RDF URI associated with this url.
  nsString mCharsetOverride; // used by nsIMsgI18NUrl...
	PRBool				m_getOldMessages;
    nsCString mOriginalSpec;
};

#endif // nsNntpUrl_h__
