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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef NSCOPYMESSAGESTREAMLISTENER_H
#define NSCOPYMESSAGESTREAMLISTENER_H

#include "nsICopyMsgStreamListener.h"
#include "nsIStreamListener.h"
#include "nsIMsgFolder.h"
#include "nsICopyMessageListener.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"

class nsCopyMessageStreamListener : public nsIStreamListener, public nsICopyMessageStreamListener {

public:
	nsCopyMessageStreamListener();
	virtual ~nsCopyMessageStreamListener();

	NS_DECL_ISUPPORTS
    NS_DECL_NSICOPYMESSAGESTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

protected:
	nsCOMPtr<nsICopyMessageListener> mDestination;
	nsCOMPtr<nsISupports> mListenerData;
	nsCOMPtr<nsIMsgFolder> mSrcFolder;

};



#endif
