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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsMsgMailNewsUrl_h___
#define nsMsgMailNewsUrl_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsIUrlListener.h"
#include "nsIUrlListenerManager.h"
#include "nsIMsgWindow.h"
#include "nsIMsgStatusFeedback.h"
#include "nsCOMPtr.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIURL.h"
#include "nsILoadGroup.h"
#include "nsIMsgSearchSession.h"
#include "nsICachedNetData.h"
///////////////////////////////////////////////////////////////////////////////////
// Okay, I found that all of the mail and news url interfaces needed to support
// several common interfaces (in addition to those provided through nsIURI). 
// So I decided to group them all in this implementation so we don't have to
// duplicate the code.
//
//////////////////////////////////////////////////////////////////////////////////

class NS_MSG_BASE nsMsgMailNewsUrl : public nsIMsgMailNewsUrl
{
public:
	nsMsgMailNewsUrl();

	NS_DECL_ISUPPORTS
    NS_DECL_NSIMSGMAILNEWSURL
    NS_DECL_NSIURI
    NS_DECL_NSIURL

protected:
	virtual ~nsMsgMailNewsUrl();

	// a helper function I needed from derived urls...
	virtual const char * GetUserName() = 0;

	nsCOMPtr<nsIURL> m_baseURL;
	nsCOMPtr<nsIMsgStatusFeedback> m_statusFeedback;
	nsCOMPtr<nsIMsgWindow> m_msgWindow;
	nsCOMPtr<nsILoadGroup> m_loadGroup;
  nsCOMPtr<nsIMsgSearchSession> m_searchSession;
  nsCOMPtr<nsICachedNetData> m_memCacheEntry;
	char		*m_errorMessage;
	PRBool	m_runningUrl;
	PRBool	m_updatingFolder;
  PRBool  m_addContentToCache;
  PRBool  m_msgIsInLocalCache;

  // the following field is really a bit of a hack to make 
  // open attachments work. The external applications code sometimes trys to figure out the right
  // handler to use by looking at the file extension of the url we are trying to load. Unfortunately,
  // the attachment file name really isn't part of the url string....so we'll store it here...and if 
  // the url we are running is an attachment url, we'll set it here. Then when the helper apps code
  // asks us for it, we'll return the right value.
  nsCString mAttachmentFileName;

	// manager of all of current url listeners....
	nsCOMPtr<nsIUrlListenerManager> m_urlListeners;
};

#endif /* nsMsgMailNewsUrl_h___ */
