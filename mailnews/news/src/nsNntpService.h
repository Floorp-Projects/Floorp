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

class nsIURI;
class nsIUrlListener;

class nsNntpService : public nsINntpService, public nsIMsgMessageService, public nsIProtocolHandler
{
public:
  ////////////////////////////////////////////////////////////////////////////////////////
  // we suppport the nsINntpService Interface 
  ////////////////////////////////////////////////////////////////////////////////////////
  NS_IMETHOD ConvertNewsgroupsString(const char *newsgroupsStr, char **_retval);

  NS_IMETHOD PostMessage(nsFilePath &pathToFile, const char *newsgroup, nsIUrlListener * aUrlListener, nsIURI **_retval);

  NS_IMETHOD RunNewsUrl (nsString& urlString, nsString& newsgroupName, nsMsgKey aKey, nsISupports * aConsumer, nsIUrlListener * aUrlListener, nsIURI **_retval);

  NS_IMETHOD GetNewNews(nsINntpIncomingServer *nntpServer, const char *uri, nsIUrlListener * aUrlListener, nsIURI **_retval);

  NS_IMETHOD CancelMessages(const char *hostname, const char *newsgroupname, nsISupportsArray *messages, nsISupports * aDisplayConsumer, nsIUrlListener * aUrlListener, nsIURI ** aURL);

  ////////////////////////////////////////////////////////////////////////////////////////
  // we suppport the nsIMsgMessageService Interface 
  ////////////////////////////////////////////////////////////////////////////////////////

  NS_IMETHOD SaveMessageToDisk(const char *aMessageURI, nsIFileSpec *aFile, PRBool aAppendToFile, nsIUrlListener *aUrlListener, nsIURI **aURL);

  NS_IMETHOD CopyMessage(const char * aSrcMailboxURI, nsIStreamListener * aMailboxCopy, 
						   PRBool moveMessage,nsIUrlListener * aUrlListener, nsIURI **aURL);
  
  NS_IMETHOD DisplayMessage(const char* aMessageURI, nsISupports * aDisplayConsumer, 
                            nsIUrlListener * aUrlListener, nsIURI ** aURL);


  ////////////////////////////////////////////////////////////////////////////////////////
  // we suppport the nsIProtocolHandler Interface 
  ////////////////////////////////////////////////////////////////////////////////////////
  NS_IMETHOD GetScheme(char * *aScheme);
  NS_IMETHOD GetDefaultPort(PRInt32 *aDefaultPort);
  NS_IMETHOD MakeAbsolute(const char *aRelativeSpec, nsIURI *aBaseURI, char **_retval);
  NS_IMETHOD NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval);
  NS_IMETHOD NewChannel(const char *verb, nsIURI *aURI, nsIEventSinkGetter *eventSinkGetter, nsIChannel **_retval);
  
  // nsNntpService
  nsNntpService();
  virtual ~nsNntpService();
  nsresult ConvertNewsMessageURI2NewsURI(const char *messageURI,
                                         nsString &newsURI,
                                         nsString &newsgroupName,
                                         nsMsgKey *aKey);
  
  nsresult DetermineHostForPosting(nsString &host, const char *newsgroupNames);
  nsresult FindHostFromGroup(nsString &host, nsString &groupName);
  
  NS_DECL_ISUPPORTS  
};

#endif /* nsNntpService_h___ */
