/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsMsgSendLater_H_
#define _nsMsgSendLater_H_

#include "nsIMsgIdentity.h"
#include "nsIMsgSendLater.h"
#include "nsIEnumerator.h"
#include "nsIFileSpec.h"

class nsMsgSendLater: public nsIMsgSendLater
{
public:
	nsMsgSendLater();
	virtual     ~nsMsgSendLater();

	NS_DECL_ISUPPORTS

	// nsIMsgSendLater support
  NS_IMETHOD                SendUnsentMessages(nsIMsgIdentity *identity);

  // Methods needed for implementing interface...
  nsIMsgFolder              *GetUnsentMessagesFolder(nsIMsgIdentity *userIdentity);
  nsresult                  StartNextMailFileSend();
  nsresult                  CompleteMailFileSend();

private:
  // Private Information
  nsIMsgIdentity            *mIdentity;
  nsCOMPtr<nsIMsgFolder>    mMessageFolder;
	nsCOMPtr<nsIMessage>      mMessage;
  nsIFileSpec               *mTempFileSpec;
  nsIEnumerator             *mEnumerator;
  PRBool                    mFirstTime;
};


#endif /* _nsMsgSendLater_H_ */
