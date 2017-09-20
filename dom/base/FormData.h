/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FormData_h
#define mozilla_dom_FormData_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/HTMLFormSubmission.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FormDataBinding.h"
#include "nsIDOMFormData.h"
#include "nsIXMLHttpRequest.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class HTMLFormElement;
class GlobalObject;

class FormData final : public nsIDOMFormData,
                       public HTMLFormSubmission,
                       public nsWrapperCache
{
private:
  ~FormData() {}

  struct FormDataTuple
  {
    nsString name;
    bool wasNullBlob;
    OwningBlobOrDirectoryOrUSVString value;
  };

  // Returns the FormDataTuple to modify. This may be null, in which case
  // no element with aName was found.
  FormDataTuple*
  RemoveAllOthersAndGetFirstFormDataTuple(const nsAString& aName);

  void SetNameValuePair(FormDataTuple* aData,
                        const nsAString& aName,
                        const nsAString& aValue,
                        bool aWasNullBlob = false);

  void SetNameFilePair(FormDataTuple* aData,
                       const nsAString& aName,
                       File* aFile);

  void SetNameDirectoryPair(FormDataTuple* aData,
                            const nsAString& aName,
                            Directory* aDirectory);

public:
  explicit FormData(nsISupports* aOwner = nullptr);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(FormData,
                                                         nsIDOMFormData)

  NS_DECL_NSIDOMFORMDATA

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  nsISupports*
  GetParentObject() const
  {
    return mOwner;
  }

  static already_AddRefed<FormData>
  Constructor(const GlobalObject& aGlobal,
              const Optional<NonNull<HTMLFormElement> >& aFormElement,
              ErrorResult& aRv);

  void Append(const nsAString& aName, const nsAString& aValue,
              ErrorResult& aRv);

  void Append(const nsAString& aName, Blob& aBlob,
              const Optional<nsAString>& aFilename,
              ErrorResult& aRv);

  void Append(const nsAString& aName, Directory* aDirectory);

  void Delete(const nsAString& aName);

  void Get(const nsAString& aName, Nullable<OwningBlobOrDirectoryOrUSVString>& aOutValue);

  void GetAll(const nsAString& aName, nsTArray<OwningBlobOrDirectoryOrUSVString>& aValues);

  bool Has(const nsAString& aName);

  void Set(const nsAString& aName, Blob& aBlob,
           const Optional<nsAString>& aFilename,
           ErrorResult& aRv);
  void Set(const nsAString& aName, const nsAString& aValue,
           ErrorResult& aRv);

  uint32_t GetIterableLength() const;

  const nsAString& GetKeyAtIndex(uint32_t aIndex) const;

  const OwningBlobOrDirectoryOrUSVString& GetValueAtIndex(uint32_t aIndex) const;

  // HTMLFormSubmission
  virtual nsresult
  GetEncodedSubmission(nsIURI* aURI, nsIInputStream** aPostDataStream,
                       int64_t* aPostDataStreamLength) override;

  virtual nsresult AddNameValuePair(const nsAString& aName,
                                    const nsAString& aValue) override
  {
    FormDataTuple* data = mFormData.AppendElement();
    SetNameValuePair(data, aName, aValue);
    return NS_OK;
  }

  virtual nsresult AddNameBlobOrNullPair(const nsAString& aName,
                                         Blob* aBlob) override;

  virtual nsresult AddNameDirectoryPair(const nsAString& aName,
                                        Directory* aDirectory) override;

  typedef bool (*FormDataEntryCallback)(const nsString& aName,
                                        const OwningBlobOrDirectoryOrUSVString& aValue,
                                        void* aClosure);

  uint32_t
  Length() const
  {
    return mFormData.Length();
  }

  // Stops iteration and returns false if any invocation of callback returns
  // false. Returns true otherwise.
  bool
  ForEach(FormDataEntryCallback aFunc, void* aClosure)
  {
    for (uint32_t i = 0; i < mFormData.Length(); ++i) {
      FormDataTuple& tuple = mFormData[i];
      if (!aFunc(tuple.name, tuple.value, aClosure)) {
        return false;
      }
    }

    return true;
  }

  nsresult
  GetSendInfo(nsIInputStream** aBody, uint64_t* aContentLength,
              nsACString& aContentTypeWithCharset, nsACString& aCharset) const;

private:
  nsCOMPtr<nsISupports> mOwner;

  nsTArray<FormDataTuple> mFormData;
};

} // dom namespace
} // mozilla namepsace

#endif // mozilla_dom_FormData_h
