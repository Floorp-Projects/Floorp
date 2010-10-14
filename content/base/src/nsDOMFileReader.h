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
 * The Original Code is mozila.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsDOMFileReader_h__
#define nsDOMFileReader_h__

#include "nsISupportsUtils.h"      
#include "nsString.h"              
#include "nsIStreamListener.h"     
#include "nsIScriptContext.h"      
#include "nsIInterfaceRequestor.h" 
#include "nsJSUtils.h"             
#include "nsTArray.h"              
#include "nsIJSNativeInitializer.h"
#include "prtime.h"                
#include "nsITimer.h"              
#include "nsDOMEventTargetHelper.h"
#include "nsICharsetDetector.h"
#include "nsICharsetDetectionObserver.h"

#include "nsIDOMFile.h"
#include "nsIDOMFileReader.h"
#include "nsIDOMFileList.h"
#include "nsIDOMFileError.h"
#include "nsIInputStream.h"
#include "nsCOMPtr.h"
#include "nsIStreamLoader.h"
#include "nsIChannel.h"
#include "prmem.h"

#include "nsXMLHttpRequest.h"

class nsDOMFileReader : public nsXHREventTarget,
                        public nsIDOMFileReader,
                        public nsIStreamListener,
                        public nsIInterfaceRequestor,
                        public nsSupportsWeakReference,
                        public nsIJSNativeInitializer,
                        public nsITimerCallback,
                        public nsICharsetDetectionObserver
{
public:
  nsDOMFileReader();
  virtual ~nsDOMFileReader();

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMFILEREADER
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMFileReader, nsXHREventTarget)

  NS_FORWARD_NSIXMLHTTPREQUESTEVENTTARGET(nsXHREventTarget::);
        
  // nsIStreamListener
  NS_DECL_NSISTREAMLISTENER
                               
  // nsIRequestObserver
  NS_DECL_NSIREQUESTOBSERVER
                               
  // nsIInterfaceRequestor 
  NS_DECL_NSIINTERFACEREQUESTOR

  // nsITimerCallback
  NS_DECL_NSITIMERCALLBACK
                               
  // nsIJSNativeInitializer                                                
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* cx, JSObject* obj, 
                        PRUint32 argc, jsval* argv);

  NS_FORWARD_NSIDOMEVENTTARGET(nsXHREventTarget::)
  NS_FORWARD_NSIDOMNSEVENTTARGET(nsXHREventTarget::)

  // nsICharsetDetectionObserver
  NS_IMETHOD Notify(const char *aCharset, nsDetectionConfident aConf);

  void DispatchProgressEvent(const nsAString& aType);

  nsresult Init();

protected:
  enum eDataFormat {
    FILE_AS_BINARY,
    FILE_AS_TEXT,
    FILE_AS_DATAURL
  };

  nsresult ReadFileContent(nsIDOMBlob *aFile, const nsAString &aCharset, eDataFormat aDataFormat); 
  nsresult GetAsText(const nsAString &aCharset,
                     const char *aFileData, PRUint32 aDataLen, nsAString &aResult);
  nsresult GetAsDataURL(nsIDOMBlob *aFile, const char *aFileData, PRUint32 aDataLen, nsAString &aResult); 
  nsresult GuessCharset(const char *aFileData, PRUint32 aDataLen, nsACString &aCharset); 
  nsresult ConvertStream(const char *aFileData, PRUint32 aDataLen, const char *aCharset, nsAString &aResult); 
  void DispatchError(nsresult rv);
  void StartProgressEventTimer();

  void FreeFileData() {
    PR_Free(mFileData);
    mFileData = nsnull;
    mDataLen = 0;
  }

  char *mFileData;
  nsCOMPtr<nsIDOMBlob> mFile;
  nsString mCharset;
  PRUint32 mDataLen;

  eDataFormat mDataFormat;

  nsString mResult;
  PRUint16 mReadyState;

  PRBool mProgressEventWasDelayed;
  PRBool mTimerIsActive;
  nsCOMPtr<nsIDOMFileError> mError;

  nsCOMPtr<nsITimer> mProgressNotifier;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIChannel> mChannel;

  PRUint64 mReadTotal;
  PRUint64 mReadTransferred;

  nsRefPtr<nsDOMEventListenerWrapper> mOnLoadEndListener;
};

#endif
