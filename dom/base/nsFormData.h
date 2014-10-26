/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFormData_h__
#define nsFormData_h__

#include "mozilla/Attributes.h"
#include "nsIDOMFormData.h"
#include "nsIXMLHttpRequest.h"
#include "nsFormSubmission.h"
#include "nsWrapperCache.h"
#include "nsTArray.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"

namespace mozilla {
class ErrorResult;

namespace dom {
class File;
class HTMLFormElement;
class GlobalObject;
} // namespace dom
} // namespace mozilla

class nsFormData MOZ_FINAL : public nsIDOMFormData,
                             public nsIXHRSendable,
                             public nsFormSubmission,
                             public nsWrapperCache
{
  ~nsFormData() {}

public:
  explicit nsFormData(nsISupports* aOwner = nullptr);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsFormData,
                                                         nsIDOMFormData)

  NS_DECL_NSIDOMFORMDATA
  NS_DECL_NSIXHRSENDABLE

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL
  nsISupports*
  GetParentObject() const
  {
    return mOwner;
  }
  static already_AddRefed<nsFormData>
  Constructor(const mozilla::dom::GlobalObject& aGlobal,
              const mozilla::dom::Optional<mozilla::dom::NonNull<mozilla::dom::HTMLFormElement> >& aFormElement,
              mozilla::ErrorResult& aRv);
  void Append(const nsAString& aName, const nsAString& aValue);
  void Append(const nsAString& aName, mozilla::dom::File& aBlob,
              const mozilla::dom::Optional<nsAString>& aFilename);

  // nsFormSubmission
  virtual nsresult GetEncodedSubmission(nsIURI* aURI,
                                        nsIInputStream** aPostDataStream) MOZ_OVERRIDE;
  virtual nsresult AddNameValuePair(const nsAString& aName,
                                    const nsAString& aValue) MOZ_OVERRIDE
  {
    FormDataTuple* data = mFormData.AppendElement();
    data->name = aName;
    data->stringValue = aValue;
    data->valueIsFile = false;
    return NS_OK;
  }
  virtual nsresult AddNameFilePair(const nsAString& aName,
                                   nsIDOMBlob* aBlob,
                                   const nsString& aFilename) MOZ_OVERRIDE
  {
    FormDataTuple* data = mFormData.AppendElement();
    data->name = aName;
    data->fileValue = aBlob;
    data->filename = aFilename;
    data->valueIsFile = true;
    return NS_OK;
  }

private:
  nsCOMPtr<nsISupports> mOwner;

  struct FormDataTuple
  {
    nsString name;
    nsString stringValue;
    nsCOMPtr<nsIDOMBlob> fileValue;
    nsString filename;
    bool valueIsFile;
  };

  nsTArray<FormDataTuple> mFormData;
};

#endif // nsFormData_h__
