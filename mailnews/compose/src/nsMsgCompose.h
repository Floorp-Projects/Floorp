/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsMsgCompose_H_
#define _nsMsgCompose_H_

#include "nsCOMArray.h"
#include "nsIMsgCompose.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsMsgCompFields.h"
#include "nsIOutputStream.h"
#include "nsIDocumentStateListener.h"
#include "nsIMsgQuote.h"
#include "nsIMsgSendListener.h"
#include "nsIMsgCopyServiceListener.h"
#include "nsIMsgSend.h"
#include "nsIStreamListener.h"
#include "nsIMimeHeaders.h"
#include "nsIBaseWindow.h"
#include "nsIAbDirectory.h"
#include "nsIAbCard.h"
#include "nsIWebProgressListener.h"
#include "nsIAbDirectory.h"
#include "nsIMimeConverter.h"
#include "nsIUnicodeDecoder.h"
#include "nsIEditor.h"
#include "nsIMsgFolder.h"

// Forward declares
class QuotingOutputStreamListener;
class nsMsgComposeSendListener;
class nsIAddrDatabase;
class nsIEditorMailSupport;

class nsMsgCompose : public nsIMsgCompose, public nsSupportsWeakReference, public nsIMsgSendListener
{
 public: 

	nsMsgCompose();
	virtual ~nsMsgCompose();

	/* this macro defines QueryInterface, AddRef and Release for this class */
	NS_DECL_ISUPPORTS

	/*** nsIMsgCompose pure virtual functions */
	NS_DECL_NSIMSGCOMPOSE

  /* nsIMsgSendListener interface */
  NS_DECL_NSIMSGSENDLISTENER

private:

 // Deal with quoting issues...
	nsresult                      QuoteOriginalMessage(const char * originalMsgURI, PRInt32 what); // New template
  nsresult                      SetQuotingToFollow(PRBool aVal);
  nsresult                      ConvertHTMLToText(nsFileSpec& aSigFile, nsString &aSigData);
  nsresult                      ConvertTextToHTML(nsFileSpec& aSigFile, nsString &aSigData);
  PRBool                        IsEmbeddedObjectSafe(const char * originalScheme,
                                                     const char * originalHost,
                                                     const char * originalPath,
                                                     nsIDOMNode * object);
  nsresult                      ResetUrisForEmbeddedObjects();
  nsresult                      TagEmbeddedObjects(nsIEditorMailSupport *aMailEditor);

  nsCString                     mQuoteCharset;
  nsCString                     mOriginalMsgURI; // used so we can mark message disposition flags after we send the message

  PRInt32                       mWhatHolder;

  nsresult                      LoadDataFromFile(nsFileSpec& fSpec,
                                                 nsString &sigData,
                                                 PRBool aAllowUTF8 = PR_TRUE,
                                                 PRBool aAllowUTF16 = PR_TRUE);

/*
  nsresult                      GetCompFields(nsMsgCompFields **aCompFields) 
                                {
                                  if (aCompFields)
                                    *aCompFields = m_compFields;
                                  return NS_OK;
                                }
 */
  nsresult                      GetIdentity(nsIMsgIdentity **aIdentity)
                                {
                                  *aIdentity = m_identity;
                                  return NS_OK;
                                }
  //m_folderName to store the value of the saved drafts folder.
  nsCString                     m_folderName;

 private:
  nsresult _SendMsg(MSG_DeliverMode deliverMode, nsIMsgIdentity *identity, const char *accountKey, PRBool entityConversionDone);
  nsresult CreateMessage(const char * originalMsgURI, MSG_ComposeType type, nsIMsgCompFields* compFields);
  void CleanUpRecipients(nsString& recipients);
  nsresult GetABDirectories(const nsACString& dirUri, nsISupportsArray* directoriesArray, PRBool searchSubDirectory);
  nsresult BuildMailListArray(nsIAddrDatabase* database, nsIAbDirectory* parentDir, nsISupportsArray* array);
  nsresult GetMailListAddresses(nsString& name, nsISupportsArray* mailListArray, nsISupportsArray** addresses);
  nsresult TagConvertible(nsIDOMNode *node,  PRInt32 *_retval);
  nsresult _BodyConvertible(nsIDOMNode *node, PRInt32 *_retval);

  PRBool IsLastWindow();
 
       // Helper function. Parameters are not checked.
  PRBool                                    mConvertStructs;    // for TagConvertible
  
	nsCOMPtr<nsIEditor>                       m_editor;
	nsIDOMWindowInternal                      *m_window;
  nsCOMPtr<nsIBaseWindow>                   m_baseWindow;
	nsMsgCompFields                           *m_compFields;
	nsCOMPtr<nsIMsgIdentity>                  m_identity;
	PRBool						                        m_composeHTML;
	QuotingOutputStreamListener               *mQuoteStreamListener;
	nsCOMPtr<nsIOutputStream>                 mBaseStream;

  nsCOMPtr<nsIMsgComposeRecyclingListener>  mRecyclingListener;
  PRBool                                    mRecycledWindow;
	nsCOMPtr<nsIMsgSend>                      mMsgSend;           // for composition back end
	nsCOMPtr<nsIMsgProgress>                  mProgress;          // use by the back end to report progress to the front end

  // Deal with quoting issues...
  nsString                                  mCiteReference;
	nsCOMPtr<nsIMsgQuote>                     mQuote;
	PRBool						                        mQuotingToFollow;   // Quoting indicator
	MSG_ComposeType                           mType;		          // Message type
  nsCOMPtr<nsISupportsArray>                mStateListeners;		// contents are nsISupports
  PRBool                                    mCharsetOverride;
  PRBool                                    mDeleteDraft;
  nsMsgDispositionState                     mDraftDisposition;
  nsCOMPtr <nsIMsgDBHdr>                    mOrigMsgHdr;

  nsCString                                 mSmtpPassword;

  nsCOMArray<nsIMsgSendListener>            mExternalSendListeners;
    
  PRBool                                    mInsertingQuotedContent;
    
  friend class QuotingOutputStreamListener;
	friend class nsMsgComposeSendListener;
};

////////////////////////////////////////////////////////////////////////////////////
// THIS IS THE CLASS THAT IS THE STREAM Listener OF THE HTML OUPUT
// FROM LIBMIME. THIS IS FOR QUOTING
////////////////////////////////////////////////////////////////////////////////////
class QuotingOutputStreamListener : public nsIStreamListener
{
public:
    QuotingOutputStreamListener(const char *originalMsgURI,
                                nsIMsgDBHdr *origMsgHdr,
                                PRBool quoteHeaders,
                                PRBool headersOnly,
                                nsIMsgIdentity *identity,
                                const char *charset,
                                PRBool charetOverride, 
                                PRBool quoteOriginal);
    virtual ~QuotingOutputStreamListener(void);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    NS_IMETHOD  SetComposeObj(nsIMsgCompose *obj);
	  NS_IMETHOD  ConvertToPlainText(PRBool formatflowed = PR_FALSE);
	  NS_IMETHOD	SetMimeHeaders(nsIMimeHeaders * headers);
    NS_IMETHOD InsertToCompose(nsIEditor *aEditor, PRBool aHTMLEditor);

private:
    nsWeakPtr                 mWeakComposeObj;
    nsString       				    mMsgBody;
    nsString       				    mCitePrefix;
    nsString       				    mSignature;
    PRBool						        mQuoteHeaders;
    PRBool						        mHeadersOnly;
    nsCOMPtr<nsIMimeHeaders>	mHeaders;
    nsCOMPtr<nsIMsgIdentity>  mIdentity;
    nsString                  mCiteReference;
    nsCOMPtr<nsIMimeConverter> mMimeConverter;
    nsCOMPtr<nsIUnicodeDecoder> mUnicodeDecoder;
    PRInt32                   mUnicodeBufferCharacterLength;
    PRUnichar*                mUnicodeConversionBuffer;
    PRBool                    mQuoteOriginal;
};

////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the send operation. We have to create this class 
// to listen for message send completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////
class nsMsgComposeSendListener : public nsIMsgComposeSendListener, public nsIMsgSendListener, public nsIMsgCopyServiceListener, public nsIWebProgressListener
{
public:
  nsMsgComposeSendListener(void);
  virtual ~nsMsgComposeSendListener(void);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIMsgComposeSendListener interface
  NS_DECL_NSIMSGCOMPOSESENDLISTENER

  // nsIMsgSendListener interface
  NS_DECL_NSIMSGSENDLISTENER
  
  // nsIMsgCopyServiceListener interface
  NS_DECL_NSIMSGCOPYSERVICELISTENER
  
	// nsIWebProgressListener interface
	NS_DECL_NSIWEBPROGRESSLISTENER

  nsresult    RemoveCurrentDraftMessage(nsIMsgCompose *compObj, PRBool calledByCopy);
  nsresult    GetMsgFolder(nsIMsgCompose *compObj, nsIMsgFolder **msgFolder);

private:
  nsWeakPtr               mWeakComposeObj;
	MSG_DeliverMode         mDeliverMode;
};

/******************************************************************************
 * nsMsgRecipient
 ******************************************************************************/
class nsMsgRecipient : public nsISupports
{
public:
  nsMsgRecipient();
  nsMsgRecipient(nsString fullAddress, nsString email, PRUint32 preferFormat = nsIAbPreferMailFormat::unknown,
                 PRBool processed = PR_FALSE);
	virtual ~nsMsgRecipient();

  NS_DECL_ISUPPORTS
  
public:
  nsString mAddress;      /* full email address (name + email) */
  nsString mEmail;        /* email address only */
  PRUint32 mPreferFormat; /* nsIAbPreferMailFormat:: unknown, plaintext or html */
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
