/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFormData_h__
#define nsFormData_h__

#include "nsIDOMFormData.h"
#include "nsIXMLHttpRequest.h"
#include "nsFormSubmission.h"
#include "nsIJSNativeInitializer.h"
#include "nsTArray.h"

class nsIDOMFile;

class nsFormData : public nsIDOMFormData,
                   public nsIXHRSendable,
                   public nsIJSNativeInitializer,
                   public nsFormSubmission
{
public:
  nsFormData();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMFORMDATA
  NS_DECL_NSIXHRSENDABLE

  // nsFormSubmission
  virtual nsresult GetEncodedSubmission(nsIURI* aURI,
                                        nsIInputStream** aPostDataStream);
  virtual nsresult AddNameValuePair(const nsAString& aName,
                                    const nsAString& aValue);
  virtual nsresult AddNameFilePair(const nsAString& aName,
                                   nsIDOMBlob* aBlob);

  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* aCx, JSObject* aObj,
                        PRUint32 aArgc, jsval* aArgv);
private:
  struct FormDataTuple
  {
    nsString name;
    nsString stringValue;
    nsCOMPtr<nsIDOMBlob> fileValue;
    bool valueIsFile;
  };
  
  nsTArray<FormDataTuple> mFormData;
};

#endif // nsFormData_h__
