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

#include "nsIMsgComposeService.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"

class nsMsgComposeService : public nsIMsgComposeService
{
public: 
	nsMsgComposeService();
	virtual ~nsMsgComposeService();

	/* this macro defines QueryInterface, AddRef and Release for this class */
	NS_DECL_ISUPPORTS

	/* void OpenComposeWindow (in wstring msgComposeWindowURL, in wstring originalMsgURI, in long type, in long format); */
	NS_IMETHOD OpenComposeWindow(const PRUnichar *msgComposeWindowURL, const PRUnichar *originalMsgURI, PRInt32 type, PRInt32 format, nsISupports *object);

	/* void OpenComposeWindowWithValues (in wstring msgComposeWindowURL, in MSG_ComposeFormat format, in wstring to, in wstring cc, in wstring bcc, in wstring newsgroups, in wstring subject, in wstring body); */
	NS_IMETHOD OpenComposeWindowWithValues(const PRUnichar *msgComposeWindowURL, MSG_ComposeFormat format, const PRUnichar *to, const PRUnichar *cc, const PRUnichar *bcc, const PRUnichar *newsgroups,
											const PRUnichar *subject, const PRUnichar *body);

	/* void OpenComposeWindowWithCompFields (in wstring msgComposeWindowURL, in MSG_ComposeFormat format, in nsIMsgCompFields compFields); */
	NS_IMETHOD OpenComposeWindowWithCompFields(const PRUnichar *msgComposeWindowURL, MSG_ComposeFormat format, nsIMsgCompFields *compFields);

	/* nsIMsgCompose InitCompose (in nsIDOMWindow aWindow, in wstring originalMsgURI, in long type, in long format, in long compFieldsAddr); */
	NS_IMETHOD InitCompose(nsIDOMWindow *aWindow, const PRUnichar *originalMsgURI, PRInt32 type, PRInt32 format, PRInt32 compFieldsAddr, nsIMsgCompose **_retval);

	/* void DisposeCompose (in nsIMsgCompose compose, in boolean closeWindow); */
	NS_IMETHOD DisposeCompose(nsIMsgCompose *compose, PRBool closeWindow);

private:
	nsCOMPtr<nsISupportsArray> m_msgQueue;
	
	//tempory hack
	nsString		hack_uri[16];
	nsISupports*	hack_object[16];
};
