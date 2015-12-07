/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "mozilla/dom/File.h"
#include "mozilla/dom/FormDataBinding.h"

namespace mozilla {
class ErrorResult;

namespace dom {
class HTMLFormElement;
class GlobalObject;
} // namespace dom
} // namespace mozilla

class nsFormData final : public nsIDOMFormData,
                         public nsIXHRSendable,
                         public nsFormSubmission,
                         public nsWrapperCache
{
private:
  ~nsFormData() {}

  typedef mozilla::dom::Blob Blob;
  typedef mozilla::dom::File File;
  typedef mozilla::dom::OwningFileOrUSVString OwningFileOrUSVString;

  struct FormDataTuple
  {
    nsString name;
    OwningFileOrUSVString value;
  };

  // Returns the FormDataTuple to modify. This may be null, in which case
  // no element with aName was found.
  FormDataTuple*
  RemoveAllOthersAndGetFirstFormDataTuple(const nsAString& aName);

  void SetNameValuePair(FormDataTuple* aData,
                        const nsAString& aName,
                        const nsAString& aValue)
  {
    MOZ_ASSERT(aData);
    aData->name = aName;
    aData->value.SetAsUSVString() = aValue;
  }

  void SetNameFilePair(FormDataTuple* aData,
                       const nsAString& aName,
                       File* aFile)
  {
    MOZ_ASSERT(aData);
    aData->name = aName;
    if (aFile) {
      aData->value.SetAsFile() = aFile;
    }
  }

public:
  explicit nsFormData(nsISupports* aOwner = nullptr);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsFormData,
                                                         nsIDOMFormData)

  NS_DECL_NSIDOMFORMDATA
  NS_DECL_NSIXHRSENDABLE

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

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
  void Append(const nsAString& aName, const nsAString& aValue,
              mozilla::ErrorResult& aRv);
  void Append(const nsAString& aName, Blob& aBlob,
              const mozilla::dom::Optional<nsAString>& aFilename,
              mozilla::ErrorResult& aRv);
  void Delete(const nsAString& aName);
  void Get(const nsAString& aName, mozilla::dom::Nullable<OwningFileOrUSVString>& aOutValue);
  void GetAll(const nsAString& aName, nsTArray<OwningFileOrUSVString>& aValues);
  bool Has(const nsAString& aName);
  void Set(const nsAString& aName, Blob& aBlob,
           const mozilla::dom::Optional<nsAString>& aFilename,
           mozilla::ErrorResult& aRv);
  void Set(const nsAString& aName, const nsAString& aValue,
           mozilla::ErrorResult& aRv);

  uint32_t GetIterableLength() const;
  const nsAString& GetKeyAtIndex(uint32_t aIndex) const;
  const OwningFileOrUSVString& GetValueAtIndex(uint32_t aIndex) const;

  // nsFormSubmission
  virtual nsresult GetEncodedSubmission(nsIURI* aURI,
                                        nsIInputStream** aPostDataStream) override;
  virtual nsresult AddNameValuePair(const nsAString& aName,
                                    const nsAString& aValue) override
  {
    FormDataTuple* data = mFormData.AppendElement();
    SetNameValuePair(data, aName, aValue);
    return NS_OK;
  }
  virtual nsresult AddNameFilePair(const nsAString& aName,
                                   File* aFile) override;

  typedef bool (*FormDataEntryCallback)(const nsString& aName,
                                        const OwningFileOrUSVString& aValue,
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

private:
  nsCOMPtr<nsISupports> mOwner;

  nsTArray<FormDataTuple> mFormData;
};

#endif // nsFormData_h__
