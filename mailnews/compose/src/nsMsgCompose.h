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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsMsgCompose_H_
#define _nsMsgCompose_H_

#include "nsIMsgCompose.h"
#include "nsCOMPtr.h"
#include "nsMsgCompFields.h"
#include "nsIOutputStream.h"
#include "nsIMsgQuote.h"
#include "nsIMsgSendListener.h"
#include "nsIMsgCopyServiceListener.h"
#include "nsIMsgSend.h"
#include "nsIStreamListener.h"
#include "nsIMimeHeaders.h"
#include "nsIBaseWindow.h"
#include "nsIAbDirectory.h"

// Forward declares
class QuotingOutputStreamListener;
class nsMsgComposeSendListener;
class nsMsgDocumentStateListener;

class nsMsgCompose : public nsIMsgCompose
{
 public: 

	nsMsgCompose();
	virtual ~nsMsgCompose();

	/* this macro defines QueryInterface, AddRef and Release for this class */
	NS_DECL_ISUPPORTS

	/*** nsIMsgCompose pure virtual functions */
	NS_DECL_NSIMSGCOMPOSE

  MSG_ComposeType				        GetMessageType();
  nsresult                      ConvertAndLoadComposeWindow(nsIEditorShell *aEditorShell, nsString& aPrefix, nsString& aBuf, 
                                                            nsString& aSignature, PRBool aQuoted, PRBool aHTMLEditor);

 // Deal with quoting issues...
	nsresult                      QuoteOriginalMessage(const PRUnichar * originalMsgURI, PRInt32 what); // New template
  PRBool                        QuotingToFollow(void);
  nsresult                      SetQuotingToFollow(PRBool aVal);
  nsresult                      ConvertHTMLToText(nsFileSpec& aSigFile, nsString &aSigData);
  nsresult                      ConvertTextToHTML(nsFileSpec& aSigFile, nsString &aSigData);
  nsresult                      BuildBodyMessageAndSignature();

  nsString                      mQuoteURI;

  PRInt32                       mWhatHolder;

  nsresult                      ProcessSignature(nsIMsgIdentity *identity,        // for setting up the users compose window environment
                                                  nsString      *aMsgBody);       // the body to append the sig to
  nsresult                      BuildQuotedMessageAndSignature(void);             // for setting up the users compose window environment
	nsresult                      ShowWindow(PRBool show);
  nsresult                      LoadDataFromFile(nsFileSpec& fSpec, nsString &sigData);

  nsresult                      GetCompFields(nsMsgCompFields **aCompFields) 
                                {
                                  if (aCompFields)
                                    *aCompFields = m_compFields;
                                  return NS_OK;
                                }
  nsresult                      GetIdentity(nsIMsgIdentity **aIdentity)
                                {
                                  *aIdentity = m_identity;
                                  return NS_OK;
                                }

 private:
	nsresult _SendMsg(MSG_DeliverMode deliverMode, nsIMsgIdentity *identity);
	nsresult CreateMessage(const PRUnichar * originalMsgURI, MSG_ComposeType type, MSG_ComposeFormat format, nsIMsgCompFields* compFields);
	void CleanUpRecipients(nsString& recipients);
  nsresult GetABDirectories(char * dirUri, nsISupportsArray* directoriesArray, PRBool searchSubDirectory);
  nsresult BuildMailListArray(nsIAddrDatabase* database, nsIAbDirectory* parentDir, nsISupportsArray* array);
  nsresult GetMailListAddresses(nsString& name, nsISupportsArray* mailListArray, nsISupportsArray** addresses);
  nsresult TagConvertible(nsIDOMNode *node,  PRInt32 *_retval);
       // Helper function. Parameters are not checked.
  PRBool mConvertStructs;  // for TagConvertible

	typedef enum {
    	eComposeFieldsReady,
    	eSaveAndSendProcessDone
	} TStateListenerNotification;
  
	// tell the doc state listeners that the doc state has changed
	nsresult NotifyStateListeners(TStateListenerNotification aNotificationType);

	nsMsgComposeSendListener      *m_sendListener;
	nsIEditorShell                *m_editor;
	nsIDOMWindowInternal                  *m_window;
  nsCOMPtr<nsIBaseWindow>        m_baseWindow;
	nsMsgCompFields               *m_compFields;
	nsCOMPtr<nsIMsgIdentity>      m_identity;
	PRBool						  m_composeHTML;
	QuotingOutputStreamListener   *mQuoteStreamListener;
	nsCOMPtr<nsIOutputStream>     mBaseStream;

	nsCOMPtr<nsIMsgSend>          mMsgSend;   // for composition back end

  PRBool                        mEntityConversionDone;

  // Deal with quoting issues...
  nsString                      mCiteReference;
	nsCOMPtr<nsIMsgQuote>         mQuote;
	PRBool						            mQuotingToFollow; // Quoting indicator
	nsMsgDocumentStateListener    *mDocumentListener;
	MSG_ComposeType				  mType;		//Message type
  nsCOMPtr<nsISupportsArray> 	  mStateListeners;		// contents are nsISupports
    
  friend class QuotingOutputStreamListener;
	friend class nsMsgDocumentStateListener;
	friend class nsMsgComposeSendListener;
};

////////////////////////////////////////////////////////////////////////////////////
// THIS IS THE CLASS THAT IS THE STREAM Listener OF THE HTML OUPUT
// FROM LIBMIME. THIS IS FOR QUOTING
////////////////////////////////////////////////////////////////////////////////////
class QuotingOutputStreamListener : public nsIStreamListener
{
public:
    QuotingOutputStreamListener(const PRUnichar *originalMsgURI,
                                PRBool quoteHeaders,
                                nsIMsgIdentity *identity);
    virtual ~QuotingOutputStreamListener(void);

    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMLISTENER

    NS_IMETHOD  SetComposeObj(nsMsgCompose *obj);
	  NS_IMETHOD  ConvertToPlainText(PRBool formatflowed = PR_FALSE);
	  NS_IMETHOD	SetMimeHeaders(nsIMimeHeaders * headers);

private:
    nsCOMPtr<nsMsgCompose>    mComposeObj;
    nsString       				    mMsgBody;
    nsString       				    mCitePrefix;
    nsString       				    mSignature;
    PRBool						        mQuoteHeaders;
    nsCOMPtr<nsIMimeHeaders>	mHeaders;
    nsCOMPtr<nsIMsgIdentity>  mIdentity;
    nsString                  mCiteReference;
};

////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the send operation. We have to create this class 
// to listen for message send completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////
class nsMsgComposeSendListener : public nsIMsgSendListener, public nsIMsgCopyServiceListener
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

  // For the nsIMsgCopySerivceListener!
  NS_IMETHOD OnStartCopy();
  
  NS_IMETHOD OnProgress(PRUint32 aProgress, PRUint32 aProgressMax);
  
  NS_IMETHOD OnStopCopy(nsresult aStatus);
  NS_IMETHOD SetMessageKey(PRUint32 aMessageKey);
  NS_IMETHOD GetMessageId(nsCString* aMessageId);

  NS_IMETHOD SetComposeObj(nsMsgCompose *obj);
	NS_IMETHOD SetDeliverMode(MSG_DeliverMode deliverMode);
	nsIMsgSendListener **CreateListenerArray(PRUint32 *aLength);

private:

  nsCOMPtr<nsMsgCompose> mComposeObj;
	MSG_DeliverMode mDeliverMode;
};

////////////////////////////////////////////////////////////////////////////////////
// This is a class that will allow us to listen to state changes in the Ender 
// compose window. This is important since we must wait until the have this 
////////////////////////////////////////////////////////////////////////////////////

class nsMsgDocumentStateListener : public nsIDocumentStateListener {
public:
  nsMsgDocumentStateListener(void);
  virtual ~nsMsgDocumentStateListener(void);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  NS_IMETHOD  NotifyDocumentCreated(void);
  NS_IMETHOD  NotifyDocumentWillBeDestroyed(void);
  NS_IMETHOD  NotifyDocumentStateChanged(PRBool nowDirty);

  void        SetComposeObj(nsMsgCompose *obj);

  // class vars.
  nsMsgCompose    *mComposeObj;
};

/******************************************************************************
 * nsMsgRecipient
 ******************************************************************************/
class nsMsgRecipient : public nsISupports
{
public:
  nsMsgRecipient();
  nsMsgRecipient(nsString fullAddress, nsString email, PRBool acceptHtml = PR_FALSE, PRBool processed = PR_FALSE);
	virtual ~nsMsgRecipient();

  NS_DECL_ISUPPORTS
  
public:
  nsString mAddress;  /* full email address (name + email) */
  nsString mEmail;    /* email address only */
  PRBool mAcceptHtml;
  PRBool mProcessed;
};

/******************************************************************************
 * nsMsgMailList
 ******************************************************************************/
class nsMsgMailList : public nsISupports
{
public:
  nsMsgMailList();
  nsMsgMailList(nsString listName, nsString listDescription, nsIAbDirectory* directory);
	virtual ~nsMsgMailList();

  NS_DECL_ISUPPORTS
  
public:
  nsString mFullName;  /* full email address (name + email) */
  nsCOMPtr<nsIAbDirectory> mDirectory;
};

#endif /* _nsMsgCompose_H_ */
