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

#ifndef nsNntpService_h___
#define nsNntpService_h___

#include "nsINntpService.h"
#include "nsIProtocolHandler.h"
#include "nsIMsgMessageService.h"
#include "nsINntpIncomingServer.h"
#include "nsIFileSpec.h"
#include "MailNewsTypes.h"
#include "nsIMsgProtocolInfo.h"

class nsIURI;
class nsIUrlListener;

class nsNntpService : public nsINntpService,
                      public nsIMsgMessageService,
                      public nsIProtocolHandler,
                      public nsIMsgProtocolInfo
{
public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSINNTPSERVICE
  NS_DECL_NSIMSGMESSAGESERVICE
  NS_DECL_NSIPROTOCOLHANDLER
  NS_DECL_NSIMSGPROTOCOLINFO
  
  // nsNntpService
  nsNntpService();
  virtual ~nsNntpService();
protected:
  nsresult ConvertNewsMessageURI2NewsURI(const char *messageURI,
                                         nsCString &newsURI,
                                         nsCString &newsgroupName,
                                         nsMsgKey *aKey);
  
  nsresult DetermineHostForPosting(nsCString &host, const char *newsgroupNames);
  nsresult FindHostFromGroup(nsCString &host, nsCString &groupName);
  
  // a convience routine used to put together news urls.
  nsresult ConstructNntpUrl(const char * urlString, const char * newsgroupName, nsMsgKey key, nsISupports * aConsumer, nsIUrlListener *aUrlListener,  nsIURI ** aUrl);
  // a convience routine to run news urls
  nsresult RunNewsUrl (nsIURI * aUrl, nsISupports * aConsumer);
};

#endif /* nsNntpService_h___ */
