/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMFileReader_h__
#define nsDOMFileReader_h__

#include "mozilla/Attributes.h"
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
                        public nsSupportsWeakReference
{
  typedef mozilla::ErrorResult ErrorResult;
public:
  nsDOMFileReader();
  virtual ~nsDOMFileReader();

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMFILEREADER

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper)

  // nsIInterfaceRequestor 
  NS_DECL_NSIINTERFACEREQUESTOR

  // FileIOObject overrides
  virtual void DoAbort(nsAString& aEvent) MOZ_OVERRIDE;
  NS_IMETHOD DoOnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                             nsresult aStatus, nsAString& aSuccessEvent,
                             nsAString& aTerminationEvent) MOZ_OVERRIDE;
  NS_IMETHOD DoOnDataAvailable(nsIRequest* aRequest, nsISupports* aContext,
                               nsIInputStream* aInputStream, uint64_t aOffset,
                               uint32_t aCount) MOZ_OVERRIDE;

  nsPIDOMWindow* GetParentObject() const
  {
    return GetOwner();
  }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // WebIDL
  static already_AddRefed<nsDOMFileReader>
  Constructor(const mozilla::dom::GlobalObject& aGlobal, ErrorResult& aRv);
  void ReadAsArrayBuffer(JSContext* aCx, nsIDOMBlob* aBlob, ErrorResult& aRv)
  {
    MOZ_ASSERT(aBlob);
    ReadFileContent(aCx, aBlob, EmptyString(), FILE_AS_ARRAYBUFFER, aRv);
  }
  void ReadAsText(nsIDOMBlob* aBlob, const nsAString& aLabel, ErrorResult& aRv)
  {
    MOZ_ASSERT(aBlob);
    ReadFileContent(nullptr, aBlob, aLabel, FILE_AS_TEXT, aRv);
  }
  void ReadAsDataURL(nsIDOMBlob* aBlob, ErrorResult& aRv)
  {
    MOZ_ASSERT(aBlob);
    ReadFileContent(nullptr, aBlob, EmptyString(), FILE_AS_DATAURL, aRv);
  }

  using FileIOObject::Abort;

  // Inherited ReadyState().

  JS::Value GetResult(JSContext* aCx, ErrorResult& aRv);

  using FileIOObject::GetError;

  IMPL_EVENT_HANDLER(loadstart)
  using FileIOObject::GetOnprogress;
  using FileIOObject::SetOnprogress;
  IMPL_EVENT_HANDLER(load)
  using FileIOObject::GetOnabort;
  using FileIOObject::SetOnabort;
  using FileIOObject::GetOnerror;
  using FileIOObject::SetOnerror;
  IMPL_EVENT_HANDLER(loadend)

  void ReadAsBinaryString(nsIDOMBlob* aBlob, ErrorResult& aRv)
  {
    MOZ_ASSERT(aBlob);
    ReadFileContent(nullptr, aBlob, EmptyString(), FILE_AS_BINARY, aRv);
  }


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

  void ReadFileContent(JSContext* aCx, nsIDOMBlob* aBlob,
                       const nsAString &aCharset, eDataFormat aDataFormat,
                       ErrorResult& aRv);
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
  
  JS::Heap<JSObject*> mResultArrayBuffer;
};

#endif
