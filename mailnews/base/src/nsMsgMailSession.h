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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsMsgMailSession_h___
#define nsMsgMailSession_h___

#include "nsIMsgMailSession.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIMsgStatusFeedback.h"
#include "nsIMsgWindow.h"
#include "nsISupportsArray.h"

///////////////////////////////////////////////////////////////////////////////////
// The mail session is a replacement for the old 4.x MSG_Master object. It contains
// mail session generic information such as the user's current mail identity, ....
// I'm starting this off as an empty interface and as people feel they need to
// add more information to it, they can. I think this is a better approach than 
// trying to port over the old MSG_Master in its entirety as that had a lot of 
// cruft in it....
//////////////////////////////////////////////////////////////////////////////////

class nsMsgMailSession : public nsIMsgMailSession,
                         public nsIFolderListener
{
public:
	nsMsgMailSession();
	virtual ~nsMsgMailSession();

	NS_DECL_ISUPPORTS
	NS_DECL_NSIMSGMAILSESSION
    NS_DECL_NSIFOLDERLISTENER

	nsresult Init();
protected:
	nsCOMPtr<nsISupportsArray> mListeners; 
	nsCOMPtr<nsISupportsArray> mWindows;
	// stick this here temporarily
	nsCOMPtr <nsIMsgWindow> m_temporaryMsgWindow;

};


#endif /* nsMsgMailSession_h__ */
