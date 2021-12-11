/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FormData_h
#define mozilla_dom_FormData_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/HTMLFormSubmission.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FormDataBinding.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class HTMLFormElement;
class GlobalObject;

class FormData final : public nsISupports,
                       public HTMLFormSubmission,
                       public nsWrapperCache {
 private:
  FormData(const FormData& aFormData);
  ~FormData() = default;

  struct FormDataTuple {
    nsString name;
    OwningBlobOrDirectoryOrUSVString value;
  };

  // Returns the FormDataTuple to modify. This may be null, in which case
  // no element with aName was found.
  FormDataTuple* RemoveAllOthersAndGetFirstFormDataTuple(
      const nsAString& aName);

  void SetNameValuePair(FormDataTuple* aData, const nsAString& aName,
                        const nsAString& aValue);

  void SetNameFilePair(FormDataTuple* aData, const nsAString& aName,
                       File* aFile);

  void SetNameDirectoryPair(FormDataTuple* aData, const nsAString& aName,
                            Directory* aDirectory);

 public:
  explicit FormData(nsISupports* aOwner = nullptr,
                    NotNull<const Encoding*> aEncoding = UTF_8_ENCODING,
                    Element* aSubmitter = nullptr);

  already_AddRefed<FormData> Clone();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FormData)

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  nsISupports* GetParentObject() const { return mOwner; }

  static already_AddRefed<FormData> Constructor(
      const GlobalObject& aGlobal,
      const Optional<NonNull<HTMLFormElement> >& aFormElement,
      ErrorResult& aRv);

  void Append(const nsAString& aName, const nsAString& aValue,
              ErrorResult& aRv);

  void Append(const nsAString& aName, Blob& aBlob,
              const Optional<nsAString>& aFilename, ErrorResult& aRv);

  void Append(const nsAString& aName, Directory* aDirectory);

  void Append(const FormData& aFormData);

  void Delete(const nsAString& aName);

  void Get(const nsAString& aName,
           Nullable<OwningBlobOrDirectoryOrUSVString>& aOutValue);

  void GetAll(const nsAString& aName,
              nsTArray<OwningBlobOrDirectoryOrUSVString>& aValues);

  bool Has(const nsAString& aName);

  void Set(const nsAString& aName, Blob& aBlob,
           const Optional<nsAString>& aFilename, ErrorResult& aRv);
  void Set(const nsAString& aName, const nsAString& aValue, ErrorResult& aRv);

  uint32_t GetIterableLength() const;

  const nsAString& GetKeyAtIndex(uint32_t aIndex) const;

  const OwningBlobOrDirectoryOrUSVString& GetValueAtIndex(
      uint32_t aIndex) const;

  // HTMLFormSubmission
  virtual nsresult GetEncodedSubmission(nsIURI* aURI,
                                        nsIInputStream** aPostDataStream,
                                        nsCOMPtr<nsIURI>& aOutURI) override;

  virtual nsresult AddNameValuePair(const nsAString& aName,
                                    const nsAString& aValue) override {
    nsAutoString usvName(aName);
    nsAutoString usvValue(aValue);
    if (!NormalizeUSVString(usvName) || !NormalizeUSVString(usvValue)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    FormDataTuple* data = mFormData.AppendElement();
    SetNameValuePair(data, usvName, usvValue);
    return NS_OK;
  }

  virtual nsresult AddNameBlobPair(const nsAString& aName,
                                   Blob* aBlob) override;

  virtual nsresult AddNameDirectoryPair(const nsAString& aName,
                                        Directory* aDirectory) override;

  using FormDataEntryCallback =
      bool (*)(const nsString& aName,
               const OwningBlobOrDirectoryOrUSVString& aValue, void* aClosure);

  uint32_t Length() const { return mFormData.Length(); }

  // Stops iteration and returns false if any invocation of callback returns
  // false. Returns true otherwise.
  bool ForEach(FormDataEntryCallback aFunc, void* aClosure) {
    for (uint32_t i = 0; i < mFormData.Length(); ++i) {
      FormDataTuple& tuple = mFormData[i];
      if (!aFunc(tuple.name, tuple.value, aClosure)) {
        return false;
      }
    }

    return true;
  }

  nsresult GetSendInfo(nsIInputStream** aBody, uint64_t* aContentLength,
                       nsACString& aContentTypeWithCharset,
                       nsACString& aCharset) const;

  nsresult CopySubmissionDataTo(HTMLFormSubmission* aFormSubmission) const;

  Element* GetSubmitterElement() const { return mSubmitter.get(); }

 private:
  nsCOMPtr<nsISupports> mOwner;

  // Submitter element.
  RefPtr<Element> mSubmitter;

  nsTArray<FormDataTuple> mFormData;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FormData_h
