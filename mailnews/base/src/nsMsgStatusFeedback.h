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

#ifndef _nsMsgStatusFeedback_h
#define _nsMsgStatusFeedback_h

#include "nsIDocumentLoaderObserver.h"
#include "nsIDOMWindow.h"
#include "nsIWebShell.h"
#include "nsCOMPtr.h"
#include "nsIMsgStatusFeedback.h"

class nsMsgStatusFeedback : public nsIMsgStatusFeedback, public nsIDocumentLoaderObserver
{
public:
	nsMsgStatusFeedback();
	virtual ~nsMsgStatusFeedback();

	NS_DECL_ISUPPORTS
  
	// nsIMsgStatusFeedback
	NS_IMETHOD ShowStatusString(const PRUnichar *status);
	NS_IMETHOD StartMeteors();
	NS_IMETHOD StopMeteors();
	NS_IMETHOD ShowProgress(PRInt32 percent);

	// nsIDocumntLoaderObserver
    NS_IMETHOD OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aCommand);
    NS_IMETHOD OnEndDocumentLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus, nsIDocumentLoaderObserver* aObserver);
    NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsIContentViewer* aViewer);
    NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, PRUint32 aProgress, PRUint32 aProgressMax);
    NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsString& aMsg);
    NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus);
    NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader, nsIChannel* channel, const char *aContentType,const char *aCommand );		

	void SetWebShell(nsIWebShell *shell,  nsIDOMWindow  *mWindow);

	nsresult setAttribute( nsIWebShell *shell,
                              const char *id,
                              const char *name,
                              const nsString &value );
protected:
	nsCOMPtr <nsIWebShell>  mWebShell;
	nsCOMPtr <nsIDOMWindow> mWindow;
	PRBool					m_meteorsSpinning;
};


#endif // _nsMsgStatusFeedback_h

