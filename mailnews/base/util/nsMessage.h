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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/********************************************************************************************************
 
   Interface for representing Messenger folders.
 
*********************************************************************************************************/

#ifndef nsMessage_h__
#define nsMessage_h__

#include "msgCore.h"
#include "nsIMessage.h" /* include the interface we are going to support */
#include "nsRDFResource.h"
#include "nsIMsgHdr.h"
#include "nsCOMPtr.h"
#include "nsWeakPtr.h"

class NS_MSG_BASE nsMessage: public nsRDFResource, public nsIDBMessage
{
public: 
	nsMessage(void);
	virtual ~nsMessage(void);

	NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIMESSAGE
  NS_DECL_NSIDBMESSAGE
  NS_DECL_NSIMSGHDR
  
	NS_IMETHOD Init(const char *aURI);

protected:
	nsCOMPtr<nsIMsgDBHdr> mMsgHdr;

private:
  nsWeakPtr mFolder;
  nsMsgKey mMsgKey;

  // could be combined into bitfield
  PRBool mMsgKeyValid;
  PRUint32 mMessageType;
};

#endif //nsMessage_h__

