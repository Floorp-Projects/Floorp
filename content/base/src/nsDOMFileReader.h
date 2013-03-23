/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

#include "nsIDOMFile.h"
#include "nsIDOMFileReader.h"
#include "nsIDOMFileList.h"
#include "nsIInputStream.h"
#include "nsCOMPtr.h"
#include "nsIStreamLoader.h"
#include "nsIChannel.h"

#include "FileIOObject.h"

class nsDOMFileReader : public mozilla::dom::FileIOObject,
                        public nsIDOMFileReader,
                        public nsIInterfaceRequestor,
                        public nsSupportsWeakReference,
                        public nsIJSNativeInitializer
{
public:
  nsDOMFileReader();
  virtual ~nsDOMFileReader();

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMFILEREADER

  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  // nsIInterfaceRequestor 
  NS_DECL_NSIINTERFACEREQUESTOR

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* cx, JSObject* obj,
                        uint32_t argc, JS::Value* argv);

  // FileIOObject overrides
  NS_IMETHOD DoAbort(nsAString& aEvent);
  NS_IMETHOD DoOnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                             nsresult aStatus, nsAString& aSuccessEvent,
                             nsAString& aTerminationEvent);
  NS_IMETHOD DoOnDataAvailable(nsIRequest* aRequest, nsISupports* aContext,
                               nsIInputStream* aInputStream, uint64_t aOffset,
                               uint32_t aCount);

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
                     const char *aFileData, uint32_t aDataLen, nsAString &aResult);
  nsresult GetAsDataURL(nsIDOMBlob *aFile, const char *aFileData, uint32_t aDataLen, nsAString &aResult); 
  nsresult ConvertStream(const char *aFileData, uint32_t aDataLen, const char *aCharset, nsAString &aResult); 

  void FreeFileData() {
    moz_free(mFileData);
    mFileData = nullptr;
    mDataLen = 0;
  }

  char *mFileData;
  nsCOMPtr<nsIDOMBlob> mFile;
  nsCString mCharset;
  uint32_t mDataLen;

  eDataFormat mDataFormat;

  nsString mResult;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  
  JSObject* mResultArrayBuffer;
};

#endif
