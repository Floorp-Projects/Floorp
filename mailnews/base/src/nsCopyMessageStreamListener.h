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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef NSCOPYMESSAGESTREAMLISTENER_H
#define NSCOPYMESSAGESTREAMLISTENER_H

#include "nsIStreamListener.h"
#include "nsIMsgFolder.h"
#include "nsICopyMessageListener.h"

class nsCopyMessageStreamListener : public nsIStreamListener {

public:
	nsCopyMessageStreamListener(nsIMsgFolder *srcFolder, nsICopyMessageListener *destination,
								nsISupports *listenerData);
	~nsCopyMessageStreamListener();

	NS_DECL_ISUPPORTS

	//nsIStreamListener implementation
	NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo);
	NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, 
                               PRUint32 aLength);
	NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);

	NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax);

	NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);

	NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);

protected:
	nsICopyMessageListener *mDestination;
	nsISupports *mListenerData;
	nsIMsgFolder *mSrcFolder;

};

#endif