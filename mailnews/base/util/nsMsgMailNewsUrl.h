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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
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

#ifndef nsMsgMailNewsUrl_h___
#define nsMsgMailNewsUrl_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsIUrlListener.h"
#include "nsIUrlListenerManager.h"
#include "nsIMsgWindow.h"
#include "nsIMsgStatusFeedback.h"
#include "nsCOMPtr.h"
#include "nsIMimeHeaders.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIURL.h"
#include "nsILoadGroup.h"
#include "nsIMsgSearchSession.h"
#include "nsICacheEntryDescriptor.h"
#include "nsICacheSession.h"
#include "nsISupportsArray.h"
#include "nsIMimeMiscStatus.h"

///////////////////////////////////////////////////////////////////////////////////
// Okay, I found that all of the mail and news url interfaces needed to support
// several common interfaces (in addition to those provided through nsIURI). 
// So I decided to group them all in this implementation so we don't have to
// duplicate the code.
//
//////////////////////////////////////////////////////////////////////////////////

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_DEFAULT

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
  nsCOMPtr<nsIMimeHeaders> mMimeHeaders;
  nsCOMPtr<nsIMsgSearchSession> m_searchSession;
  nsCOMPtr<nsICacheEntryDescriptor> m_memCacheEntry;
  nsCOMPtr<nsICacheSession> m_imageCacheSession;
  nsCOMPtr<nsISupportsArray> m_cachedMemCacheEntries;
  nsCOMPtr<nsIMsgHeaderSink> mMsgHeaderSink;
  char *m_errorMessage;
  PRBool m_runningUrl;
  PRBool m_updatingFolder;
  PRBool m_addContentToCache;
  PRBool m_msgIsInLocalCache;
  PRBool m_suppressErrorMsgs;

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

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_HIDDEN

#endif /* nsMsgMailNewsUrl_h___ */
