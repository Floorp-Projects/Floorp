/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "prtime.h"
#include "nsITimer.h"
#include "nsIAsyncInputStream.h"

#include "nsIDOMFileReader.h"
#include "nsIDOMFileList.h"
#include "nsCOMPtr.h"

#include "FileIOObject.h"

namespace mozilla {
namespace dom {
class Blob;
}
}

class nsDOMFileReader final : public mozilla::dom::FileIOObject,
                              public nsIDOMFileReader,
                              public nsIInterfaceRequestor,
                              public nsSupportsWeakReference
{
  typedef mozilla::ErrorResult ErrorResult;
  typedef mozilla::dom::GlobalObject GlobalObject;
  typedef mozilla::dom::Blob Blob;

public:
  nsDOMFileReader();

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMFILEREADER

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(mozilla::DOMEventTargetHelper)

  // nsIInterfaceRequestor 
  NS_DECL_NSIINTERFACEREQUESTOR

  // FileIOObject overrides
  virtual void DoAbort(nsAString& aEvent) override;

  virtual nsresult DoReadData(nsIAsyncInputStream* aStream, uint64_t aCount) override;

  virtual nsresult DoOnLoadEnd(nsresult aStatus, nsAString& aSuccessEvent,
                               nsAString& aTerminationEvent) override;

  nsPIDOMWindow* GetParentObject() const
  {
    return GetOwner();
  }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  static already_AddRefed<nsDOMFileReader>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);
  void ReadAsArrayBuffer(JSContext* aCx, Blob& aBlob, ErrorResult& aRv)
  {
    ReadFileContent(aBlob, EmptyString(), FILE_AS_ARRAYBUFFER, aRv);
  }

  void ReadAsText(Blob& aBlob, const nsAString& aLabel, ErrorResult& aRv)
  {
    ReadFileContent(aBlob, aLabel, FILE_AS_TEXT, aRv);
  }

  void ReadAsDataURL(Blob& aBlob, ErrorResult& aRv)
  {
    ReadFileContent(aBlob, EmptyString(), FILE_AS_DATAURL, aRv);
  }

  using FileIOObject::Abort;

  // Inherited ReadyState().

  void GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                 ErrorResult& aRv);

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

  void ReadAsBinaryString(Blob& aBlob, ErrorResult& aRv)
  {
    ReadFileContent(aBlob, EmptyString(), FILE_AS_BINARY, aRv);
  }


  nsresult Init();

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(nsDOMFileReader,
                                                         FileIOObject)
  void RootResultArrayBuffer();

protected:
  virtual ~nsDOMFileReader();

  enum eDataFormat {
    FILE_AS_ARRAYBUFFER,
    FILE_AS_BINARY,
    FILE_AS_TEXT,
    FILE_AS_DATAURL
  };

  void ReadFileContent(Blob& aBlob,
                       const nsAString &aCharset, eDataFormat aDataFormat,
                       ErrorResult& aRv);
  nsresult GetAsText(nsIDOMBlob *aBlob, const nsACString &aCharset,
                     const char *aFileData, uint32_t aDataLen,
                     nsAString &aResult);
  nsresult GetAsDataURL(nsIDOMBlob *aBlob, const char *aFileData,
                        uint32_t aDataLen, nsAString &aResult);

  void FreeFileData() {
    free(mFileData);
    mFileData = nullptr;
    mDataLen = 0;
  }

  char *mFileData;
  nsCOMPtr<nsIDOMBlob> mBlob;
  nsCString mCharset;
  uint32_t mDataLen;

  eDataFormat mDataFormat;

  nsString mResult;

  JS::Heap<JSObject*> mResultArrayBuffer;
};

#endif
