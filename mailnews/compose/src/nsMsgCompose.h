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
#include "nsIOutputStream.h"
#include "nsIMsgQuote.h"

class QuotingOutputStreamImpl;

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

  /* attribute nsIEditorShell editor; */ 
  NS_IMETHOD GetEditor(nsIEditorShell * *aEditor); 
  NS_IMETHOD SetEditor(nsIEditorShell * aEditor); 
  
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
	void CleanUpRecipients(nsString& recipients);

  nsresult QuoteOriginalMessage(const PRUnichar * originalMsgURI, PRInt32 what); // New template

	nsIEditorShell*		m_editor;
	nsIDOMWindow*				m_window;
	nsIWebShell*				m_webShell;
	nsIWebShellWindow*			m_webShellWin;
	nsCOMPtr<nsIMsgCompFields> 	m_compFields;
	PRBool						m_composeHTML;
  QuotingOutputStreamImpl *mOutStream;
  nsCOMPtr<nsIOutputStream>          mBaseStream;
  nsCOMPtr<nsIMsgQuote>              mQuote;
};

////////////////////////////////////////////////////////////////////////////////////
// THIS IS THE CLASS THAT IS THE STREAM CONSUMER OF THE HTML OUPUT
// FROM LIBMIME. THIS IS FOR QUOTING
////////////////////////////////////////////////////////////////////////////////////
class QuotingOutputStreamImpl : public nsIOutputStream
{
public:
    QuotingOutputStreamImpl(void);
    virtual ~QuotingOutputStreamImpl(void);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIBaseStream interface
    NS_IMETHOD Close(void);

    // nsIOutputStream interface
    NS_IMETHOD Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount);

    NS_IMETHOD Flush(void);

    NS_IMETHOD  SetComposeObj(nsMsgCompose *obj);

private:
    nsMsgCompose    *mComposeObj;
    nsString        mMsgBody;
};
