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
#include "nsIMsgSendListener.h"

class QuotingOutputStreamImpl;
class nsMsgComposeSendListener;

class nsMsgCompose : public nsIMsgCompose
{
 public: 

	nsMsgCompose();
	virtual ~nsMsgCompose();

	/* this macro defines QueryInterface, AddRef and Release for this class */
	NS_DECL_ISUPPORTS

/*** nsIMsgCompose pure virtual functions */

	/* void Initialize (in nsIDOMWindow aWindow, in wstring originalMsgURI, in MSG_ComposeType type, in MSG_ComposeFormat format, in nsIMsgCompFields compFields, in nsISupports object); */
	NS_IMETHOD Initialize(nsIDOMWindow *aWindow, const PRUnichar *originalMsgURI, MSG_ComposeType type, MSG_ComposeFormat format, nsIMsgCompFields *compFields, nsISupports *object);

	/* void LoadFields (); */
	NS_IMETHOD LoadFields();

	/* void SetDocumentCharset (in wstring charset); */
	NS_IMETHOD SetDocumentCharset(const PRUnichar *charset);

	/* void SendMsg (in MSG_DeliverMode deliverMode, in wstring callback); */
	NS_IMETHOD SendMsg(MSG_DeliverMode deliverMode,
                     nsIMsgIdentity *identity,
                     const PRUnichar *callback);

	/* void SendMsgEx (in MSG_DeliverMode deliverModer, in wstring addrTo, in wstring addrCc, in wstring addrBcc,
		in wstring newsgroup, in wstring subject, in wstring body, in wstring callback); */
	NS_IMETHOD SendMsgEx(MSG_DeliverMode deliverMode,
                       nsIMsgIdentity *identity,
                       const PRUnichar *addrTo, const PRUnichar *addrCc,
                       const PRUnichar *addrBcc, const PRUnichar *newsgroup,
                       const PRUnichar *subject, const PRUnichar *body,
                       const PRUnichar *callback);

	/* void CloseWindow (); */
	NS_IMETHOD CloseWindow();

	/* attribute nsIEditorShell editor; */ 
	NS_IMETHOD GetEditor(nsIEditorShell * *aEditor); 
	NS_IMETHOD SetEditor(nsIEditorShell * aEditor); 
  
	/* readonly attribute nsIDOMWindow domWindow; */
	NS_IMETHOD GetDomWindow(nsIDOMWindow * *aDomWindow);

	/* readonly attribute nsIMsgCompFields compFields; */
	NS_IMETHOD GetCompFields(nsIMsgCompFields * *aCompFields); //GetCompFields will addref, you need to release when your are done with it
	
	/* readonly attribute boolean composeHTML; */
	NS_IMETHOD GetComposeHTML(PRBool *aComposeHTML);

	/* readonly attribute long wrapLength; */
	NS_IMETHOD GetWrapLength(PRInt32 *aWrapLength);
/******/

	nsresult LoadBody();
	nsresult SetQuotingToFollow(PRBool aVal);
	nsresult ShowWindow(PRBool show);
 private:

	nsresult _SendMsg(MSG_DeliverMode deliverMode, nsIMsgIdentity *identity, const PRUnichar *callback);
	nsresult CreateMessage(const PRUnichar * originalMsgURI, MSG_ComposeType type, MSG_ComposeFormat format, nsIMsgCompFields* compFields, nsISupports* object);
	void HackToGetBody(PRInt32 what); //Temporary
	void CleanUpRecipients(nsString& recipients);

	nsresult QuoteOriginalMessage(const PRUnichar * originalMsgURI, PRInt32 what); // New template

	nsMsgComposeSendListener*	m_sendListener;
	nsIEditorShell*				m_editor;
	nsIDOMWindow*				m_window;
	nsIWebShell*				m_webShell;
	nsIWebShellWindow*			m_webShellWin;
	nsMsgCompFields* 			m_compFields;
	PRBool						m_composeHTML;
	QuotingOutputStreamImpl *	mOutStream;
	nsCOMPtr<nsIOutputStream>   mBaseStream;
	nsCOMPtr<nsIMsgQuote>       mQuote;

	// For only making a single LoadUrl call on the editor
	PRBool						mBodyLoaded;
	PRBool						mQuotingToFollow;

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

    NS_IMETHOD  ConvertToPlainText();

private:
    nsMsgCompose    *mComposeObj;
    nsString        mMsgBody;
};

////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the send operation. We have to create this class 
// to listen for message send completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////
class nsMsgComposeSendListener : public nsIMsgSendListener
{
public:
	nsMsgComposeSendListener(void);
	virtual ~nsMsgComposeSendListener(void);

	// nsISupports interface
	NS_DECL_ISUPPORTS

	/* void OnStartSending (in string aMsgID, in PRUint32 aMsgSize); */
	NS_IMETHOD OnStartSending(const char *aMsgID, PRUint32 aMsgSize);

	/* void OnProgress (in string aMsgID, in PRUint32 aProgress, in PRUint32 aProgressMax); */
	NS_IMETHOD OnProgress(const char *aMsgID, PRUint32 aProgress, PRUint32 aProgressMax);

	/* void OnStatus (in string aMsgID, in wstring aMsg); */
	NS_IMETHOD OnStatus(const char *aMsgID, const PRUnichar *aMsg);

	/* void OnStopSending (in string aMsgID, in nsresult aStatus, in wstring aMsg, in nsIFileSpec returnFileSpec); */
	NS_IMETHOD OnStopSending(const char *aMsgID, nsresult aStatus, const PRUnichar *aMsg, 
                           nsIFileSpec *returnFileSpec);

    NS_IMETHOD SetComposeObj(nsMsgCompose *obj);
	NS_IMETHOD SetDeliverMode(MSG_DeliverMode deliverMode);
	nsIMsgSendListener ** CreateListenerArray();

private:
    nsMsgCompose    *mComposeObj;
	MSG_DeliverMode mDeliverMode;
};
