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
#include "nsWeakReference.h"
#include "nsIStreamListener.h"     
#include "nsIInterfaceRequestor.h" 
#include "nsJSUtils.h"             
#include "nsTArray.h"              
#include "nsIJSNativeInitializer.h"
#include "prtime.h"                
#include "nsITimer.h"              
#include "nsICharsetDetector.h"
#include "nsICharsetDetectionObserver.h"

#include "nsIDOMFile.h"
#include "nsIDOMFileReader.h"
#include "nsIDOMFileList.h"
#include "nsIInputStream.h"
#include "nsCOMPtr.h"
#include "nsIStreamLoader.h"
#include "nsIChannel.h"
#include "prmem.h"

#include "FileIOObject.h"

class nsDOMFileReader : public mozilla::dom::FileIOObject,
                        public nsIDOMFileReader,
                        public nsIInterfaceRequestor,
                        public nsSupportsWeakReference,
                        public nsIJSNativeInitializer,
                        public nsICharsetDetectionObserver
{
public:
  nsDOMFileReader();
  virtual ~nsDOMFileReader();

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMFILEREADER

  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  // nsIInterfaceRequestor 
  NS_DECL_NSIINTERFACEREQUESTOR

  NS_DECL_EVENT_HANDLER(load)
  NS_DECL_EVENT_HANDLER(loadend)
  NS_DECL_EVENT_HANDLER(loadstart)

  // nsIJSNativeInitializer                                                
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* cx, JSObject* obj, 
                        PRUint32 argc, jsval* argv);

  // nsICharsetDetectionObserver
  NS_IMETHOD Notify(const char *aCharset, nsDetectionConfident aConf);

  // FileIOObject overrides
  NS_IMETHOD DoAbort(nsAString& aEvent);
  NS_IMETHOD DoOnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                             nsresult aStatus, nsAString& aSuccessEvent,
                             nsAString& aTerminationEvent);
  NS_IMETHOD DoOnDataAvailable(nsIRequest* aRequest, nsISupports* aContext,
                               nsIInputStream* aInputStream, PRUint32 aOffset,
                               PRUint32 aCount);

  nsresult Init();

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(nsDOMFileReader,
                                                         FileIOObject)
  void RootResultArrayBuffer();

protected:
  enum eDataFormat {
    FILE_AS_ARRAYBUFFER,
    FILE_AS_BINARY,
    FILE_AS_TEXT,
    FILE_AS_DATAURL
  };

  nsresult ReadFileContent(JSContext* aCx, nsIDOMBlob *aFile, const nsAString &aCharset, eDataFormat aDataFormat); 
  nsresult GetAsText(const nsACString &aCharset,
                     const char *aFileData, PRUint32 aDataLen, nsAString &aResult);
  nsresult GetAsDataURL(nsIDOMBlob *aFile, const char *aFileData, PRUint32 aDataLen, nsAString &aResult); 
  nsresult GuessCharset(const char *aFileData, PRUint32 aDataLen, nsACString &aCharset); 
  nsresult ConvertStream(const char *aFileData, PRUint32 aDataLen, const char *aCharset, nsAString &aResult); 

  void FreeFileData() {
    PR_Free(mFileData);
    mFileData = nsnull;
    mDataLen = 0;
  }

  char *mFileData;
  nsCOMPtr<nsIDOMBlob> mFile;
  nsCString mCharset;
  PRUint32 mDataLen;

  eDataFormat mDataFormat;

  nsString mResult;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  
  JSObject* mResultArrayBuffer;
};

#endif
