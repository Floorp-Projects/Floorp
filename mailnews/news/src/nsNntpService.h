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
#include "nsIMsgMessageService.h"
#include "nsINntpIncomingServer.h"

class nsIURL;
class nsIUrlListener;

class nsNntpService : public nsINntpService, public nsIMsgMessageService
{
public:
  ////////////////////////////////////////////////////////////////////////////////////////
  // we suppport the nsINntpService Interface 
  ////////////////////////////////////////////////////////////////////////////////////////
  NS_IMETHOD PostMessage(nsFilePath &pathToFile, const char *subject, const char *newsgroup, nsIUrlListener * aUrlListener, nsIURL **_retval);

  NS_IMETHOD RunNewsUrl (nsString& urlString, nsISupports * aConsumer, nsIUrlListener * aUrlListener, nsIURL **_retval);

  NS_IMETHOD GetNewNews(nsIUrlListener * aUrlListener, nsINntpIncomingServer *nntpServer, const char *uri, nsIURL **_retval);

  

  ////////////////////////////////////////////////////////////////////////////////////////
  // we suppport the nsIMsgMessageService Interface 
  ////////////////////////////////////////////////////////////////////////////////////////
	NS_IMETHOD CopyMessage(const char * aSrcMailboxURI, nsIStreamListener * aMailboxCopy, 
						   PRBool moveMessage,nsIUrlListener * aUrlListener, nsIURL **aURL);
  
  NS_IMETHOD DisplayMessage(const char* aMessageURI, nsISupports * aDisplayConsumer, 
                            nsIUrlListener * aUrlListener, nsIURL ** aURL);

  // nsNntpService
  nsNntpService();
  virtual ~nsNntpService();
  nsresult ConvertNewsMessageURI2NewsURI(const char *messageURI, nsString &newsURI);
  
  NS_DECL_ISUPPORTS  

};

#endif /* nsNntpService_h___ */
