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

#include "nsIMsgCompose.h"
#include "nsCOMPtr.h"
#include "nsMsgCompFields.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"


class nsMsgCompose : public nsIMsgCompose
{
 public: 

	nsMsgCompose();
	virtual ~nsMsgCompose();

	/* this macro defines QueryInterface, AddRef and Release for this class */
	NS_DECL_ISUPPORTS

/*** nsIMsgCompose pure virtual functions */

	/* void Initialize (in nsIDOMWindow aWindow, in wstring originalMsgURI, in long type, in long format); */
	NS_IMETHOD Initialize(nsIDOMWindow *aWindow, const PRUnichar *originalMsgURI, MSG_ComposeType type, MSG_ComposeFormat format, nsISupports *object);

	/* void LoadFields (); */
	NS_IMETHOD LoadFields();

	/* void SetDocumentCharset (in wstring charset); */
	NS_IMETHOD SetDocumentCharset(const PRUnichar *charset);

	/* void SendMsg (in MSG_DeliverMode deliverMode, in wstring callback); */
	NS_IMETHOD SendMsg(MSG_DeliverMode deliverMode, const PRUnichar *callback);

	/* void SendMsgEx (in MSG_DeliverMode deliverModer, in wstring addrTo, in wstring addrCc, in wstring addrBcc,
		in wstring newsgroup, in wstring subject, in wstring body, in wstring callback); */
	NS_IMETHOD SendMsgEx(MSG_DeliverMode deliverMode, const PRUnichar *addrTo, const PRUnichar *addrCc, const PRUnichar *addrBcc,
		const PRUnichar *newsgroup, const PRUnichar *subject, const PRUnichar *body, const PRUnichar *callback);

	/* void CloseWindow (); */
	NS_IMETHOD CloseWindow();

	/* attribute nsIDOMEditorAppCore editor; */
	NS_IMETHOD GetEditor(/*nsIDOMEditorAppCore*/nsISupports * *aEditor);
	NS_IMETHOD SetEditor(/*nsIDOMEditorAppCore*/nsISupports * aEditor);

	/* readonly attribute nsIDOMWindow domWindow; */
	NS_IMETHOD GetDomWindow(nsIDOMWindow * *aDomWindow);

	/* readonly attribute nsIMsgCompFields compFields; */
	NS_IMETHOD GetCompFields(nsIMsgCompFields * *aCompFields);
	
	/* readonly attribute boolean composeHTML; */
	NS_IMETHOD GetComposeHTML(PRBool *aComposeHTML);

	/* readonly attribute long wrapLength; */
	NS_IMETHOD GetWrapLength(PRInt32 *aWrapLength);
/******/
	

private:

	nsresult CreateMessage(const PRUnichar * originalMsgURI, MSG_ComposeType type, MSG_ComposeFormat format, nsISupports* object);
	void HackToGetBody(PRInt32 what); //Temporary
	
	nsIDOMWindow*				m_window;
	nsIWebShell*				m_webShell;
	nsIWebShellWindow*			m_webShellWin;
	nsIDOMEditorAppCore*		m_editor;
	nsCOMPtr<nsMsgCompFields>	m_compFields;
	PRBool						m_composeHTML;
};


