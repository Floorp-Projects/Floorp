/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidBridge.h"
#include "nsDOMFile.h"
#include "Navigator.h"

namespace mozilla {
namespace dom {

class GetUserMediaCallback: public nsFilePickerCallback {
public:
  GetUserMediaCallback(nsIDOMMediaCallback* aCallback, nsIDOMErrorCallback *aError) :
    mCallback(aCallback) , mError(aError){}

  virtual void handleResult(nsAString& filePath) {
    nsCOMPtr<nsILocalFile> file;
    NS_NewLocalFile(filePath, true, getter_AddRefs(file));
    bool exists;
    if (!file || NS_FAILED(file->Exists(&exists)) || !exists) {
      if (mError) {
        mError->Callback(NS_LITERAL_STRING("ERROR_NO_FILE"));
      }
    } else {
      mCallback->Callback(new nsDOMFileFile(file));
    }
  }

private:
  nsCOMPtr<nsIDOMMediaCallback> mCallback;
  nsCOMPtr<nsIDOMErrorCallback> mError;
};

class ErrorCallbackRunnable : public nsRunnable {
public:
  ErrorCallbackRunnable(nsIDOMErrorCallback* aErrorCallback, const nsString& aErrorMsg) :
    mErrorCallback(aErrorCallback), mErrorMsg(aErrorMsg) {}
  NS_IMETHOD Run() {
    mErrorCallback->Callback(mErrorMsg);
    return NS_OK;
  }
private:
  nsCOMPtr<nsIDOMErrorCallback> mErrorCallback;
  const nsString mErrorMsg;
};

NS_IMETHODIMP
Navigator::MozGetUserMedia(nsIMediaStreamOptions *params, nsIDOMMediaCallback *callback, nsIDOMErrorCallback *error)
{
  bool picture;
  if (!params || !callback || NS_FAILED(params->GetPicture(&picture))) {
    if (error) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(error, NS_LITERAL_STRING("BAD_ARGS")));
    }
    return NS_OK;
  }

  if (!picture) {
    if (error) {
      NS_DispatchToMainThread(new ErrorCallbackRunnable(error, NS_LITERAL_STRING("NOT_IMPLEMENTED")));
    }
    return NS_OK;
  }

  if (mozilla::AndroidBridge::Bridge()) {
    mozilla::AndroidBridge::Bridge()->ShowFilePickerAsync(NS_LITERAL_STRING("image/*"), new GetUserMediaCallback(callback, error));
  } else if (error) {
    NS_DispatchToMainThread(new ErrorCallbackRunnable(error, NS_LITERAL_STRING("INTERNAL_ERROR")));
  }
  return NS_OK;
}
}
}
