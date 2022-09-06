/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FormData.h"
#include "nsIInputStream.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozilla/Encoding.h"

#include "MultipartBlobImpl.h"

using namespace mozilla;
using namespace mozilla::dom;

FormData::FormData(nsISupports* aOwner, NotNull<const Encoding*> aEncoding,
                   Element* aSubmitter)
    : HTMLFormSubmission(nullptr, u""_ns, aEncoding),
      mOwner(aOwner),
      mSubmitter(aSubmitter) {}

FormData::FormData(const FormData& aFormData)
    : HTMLFormSubmission(aFormData.mActionURL, aFormData.mTarget,
                         aFormData.mEncoding) {
  mOwner = aFormData.mOwner;
  mSubmitter = aFormData.mSubmitter;
  mFormData = aFormData.mFormData.Clone();
}

namespace {

already_AddRefed<File> GetOrCreateFileCalledBlob(Blob& aBlob,
                                                 ErrorResult& aRv) {
  // If this is file, we can just use it
  RefPtr<File> file = aBlob.ToFile();
  if (file) {
    return file.forget();
  }

  // Forcing 'blob' as filename
  file = aBlob.ToFile(u"blob"_ns, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return file.forget();
}

already_AddRefed<File> GetBlobForFormDataStorage(
    Blob& aBlob, const Optional<nsAString>& aFilename, ErrorResult& aRv) {
  // Forcing a filename
  if (aFilename.WasPassed()) {
    RefPtr<File> file = aBlob.ToFile(aFilename.Value(), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    return file.forget();
  }

  return GetOrCreateFileCalledBlob(aBlob, aRv);
}

}  // namespace

// -------------------------------------------------------------------------
// nsISupports

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FormData)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(FormData)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSubmitter)

  for (uint32_t i = 0, len = tmp->mFormData.Length(); i < len; ++i) {
    ImplCycleCollectionUnlink(tmp->mFormData[i].value);
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(FormData)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSubmitter)

  for (uint32_t i = 0, len = tmp->mFormData.Length(); i < len; ++i) {
    ImplCycleCollectionTraverse(cb, tmp->mFormData[i].value,
                                "mFormData[i].GetAsBlob()", 0);
  }

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FormData)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FormData)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FormData)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// -------------------------------------------------------------------------
// HTMLFormSubmission
nsresult FormData::GetEncodedSubmission(nsIURI* aURI,
                                        nsIInputStream** aPostDataStream,
                                        nsCOMPtr<nsIURI>& aOutURI) {
  MOZ_ASSERT_UNREACHABLE("Shouldn't call FormData::GetEncodedSubmission");
  return NS_OK;
}

void FormData::Append(const nsAString& aName, const nsAString& aValue,
                      ErrorResult& aRv) {
  AddNameValuePair(aName, aValue);
}

void FormData::Append(const nsAString& aName, Blob& aBlob,
                      const Optional<nsAString>& aFilename, ErrorResult& aRv) {
  RefPtr<File> file = GetBlobForFormDataStorage(aBlob, aFilename, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  AddNameBlobPair(aName, file);
}

void FormData::Append(const nsAString& aName, Directory* aDirectory) {
  AddNameDirectoryPair(aName, aDirectory);
}

void FormData::Append(const FormData& aFormData) {
  for (uint32_t i = 0; i < aFormData.mFormData.Length(); ++i) {
    mFormData.AppendElement(aFormData.mFormData[i]);
  }
}

void FormData::Delete(const nsAString& aName) {
  mFormData.RemoveElementsBy([&aName](const auto& formDataItem) {
    return aName.Equals(formDataItem.name);
  });
}

void FormData::Get(const nsAString& aName,
                   Nullable<OwningBlobOrDirectoryOrUSVString>& aOutValue) {
  for (uint32_t i = 0; i < mFormData.Length(); ++i) {
    if (aName.Equals(mFormData[i].name)) {
      aOutValue.SetValue() = mFormData[i].value;
      return;
    }
  }

  aOutValue.SetNull();
}

void FormData::GetAll(const nsAString& aName,
                      nsTArray<OwningBlobOrDirectoryOrUSVString>& aValues) {
  for (uint32_t i = 0; i < mFormData.Length(); ++i) {
    if (aName.Equals(mFormData[i].name)) {
      OwningBlobOrDirectoryOrUSVString* element = aValues.AppendElement();
      *element = mFormData[i].value;
    }
  }
}

bool FormData::Has(const nsAString& aName) {
  for (uint32_t i = 0; i < mFormData.Length(); ++i) {
    if (aName.Equals(mFormData[i].name)) {
      return true;
    }
  }

  return false;
}

nsresult FormData::AddNameBlobPair(const nsAString& aName, Blob* aBlob) {
  MOZ_ASSERT(aBlob);

  nsAutoString usvName(aName);
  if (!NormalizeUSVString(usvName)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  RefPtr<File> file;
  ErrorResult rv;
  file = GetOrCreateFileCalledBlob(*aBlob, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  FormDataTuple* data = mFormData.AppendElement();
  SetNameFilePair(data, usvName, file);
  return NS_OK;
}

nsresult FormData::AddNameDirectoryPair(const nsAString& aName,
                                        Directory* aDirectory) {
  MOZ_ASSERT(aDirectory);

  nsAutoString usvName(aName);
  if (!NormalizeUSVString(usvName)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  FormDataTuple* data = mFormData.AppendElement();
  SetNameDirectoryPair(data, usvName, aDirectory);
  return NS_OK;
}

FormData::FormDataTuple* FormData::RemoveAllOthersAndGetFirstFormDataTuple(
    const nsAString& aName) {
  FormDataTuple* lastFoundTuple = nullptr;
  uint32_t lastFoundIndex = mFormData.Length();
  // We have to use this slightly awkward for loop since uint32_t >= 0 is an
  // error for being always true.
  for (uint32_t i = mFormData.Length(); i-- > 0;) {
    if (aName.Equals(mFormData[i].name)) {
      if (lastFoundTuple) {
        // The one we found earlier was not the first one, we can remove it.
        mFormData.RemoveElementAt(lastFoundIndex);
      }

      lastFoundTuple = &mFormData[i];
      lastFoundIndex = i;
    }
  }

  return lastFoundTuple;
}

void FormData::Set(const nsAString& aName, Blob& aBlob,
                   const Optional<nsAString>& aFilename, ErrorResult& aRv) {
  FormDataTuple* tuple = RemoveAllOthersAndGetFirstFormDataTuple(aName);
  if (tuple) {
    RefPtr<File> file = GetBlobForFormDataStorage(aBlob, aFilename, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    SetNameFilePair(tuple, aName, file);
  } else {
    Append(aName, aBlob, aFilename, aRv);
  }
}

void FormData::Set(const nsAString& aName, const nsAString& aValue,
                   ErrorResult& aRv) {
  FormDataTuple* tuple = RemoveAllOthersAndGetFirstFormDataTuple(aName);
  if (tuple) {
    SetNameValuePair(tuple, aName, aValue);
  } else {
    Append(aName, aValue, aRv);
  }
}

uint32_t FormData::GetIterableLength() const { return mFormData.Length(); }

const nsAString& FormData::GetKeyAtIndex(uint32_t aIndex) const {
  MOZ_ASSERT(aIndex < mFormData.Length());
  return mFormData[aIndex].name;
}

const OwningBlobOrDirectoryOrUSVString& FormData::GetValueAtIndex(
    uint32_t aIndex) const {
  MOZ_ASSERT(aIndex < mFormData.Length());
  return mFormData[aIndex].value;
}

void FormData::SetNameValuePair(FormDataTuple* aData, const nsAString& aName,
                                const nsAString& aValue) {
  MOZ_ASSERT(aData);
  aData->name = aName;
  aData->value.SetAsUSVString() = aValue;
}

void FormData::SetNameFilePair(FormDataTuple* aData, const nsAString& aName,
                               File* aFile) {
  MOZ_ASSERT(aData);
  MOZ_ASSERT(aFile);

  aData->name = aName;
  aData->value.SetAsBlob() = aFile;
}

void FormData::SetNameDirectoryPair(FormDataTuple* aData,
                                    const nsAString& aName,
                                    Directory* aDirectory) {
  MOZ_ASSERT(aData);
  MOZ_ASSERT(aDirectory);

  aData->name = aName;
  aData->value.SetAsDirectory() = aDirectory;
}

/* virtual */
JSObject* FormData::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) {
  return FormData_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<FormData> FormData::Constructor(
    const GlobalObject& aGlobal,
    const Optional<NonNull<HTMLFormElement> >& aFormElement, ErrorResult& aRv) {
  RefPtr<FormData> formData = new FormData(aGlobal.GetAsSupports());
  if (aFormElement.WasPassed()) {
    aRv = aFormElement.Value().ConstructEntryList(formData);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    // Step 9. Return a shallow clone of entry list.
    // https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#constructing-form-data-set
    formData = formData->Clone();
  }

  return formData.forget();
}

// contentTypeWithCharset can be set to the contentType or
// contentType+charset based on what the spec says.
// See: https://fetch.spec.whatwg.org/#concept-bodyinit-extract
nsresult FormData::GetSendInfo(nsIInputStream** aBody, uint64_t* aContentLength,
                               nsACString& aContentTypeWithCharset,
                               nsACString& aCharset) const {
  FSMultipartFormData fs(nullptr, u""_ns, UTF_8_ENCODING, nullptr);
  nsresult rv = CopySubmissionDataTo(&fs);
  NS_ENSURE_SUCCESS(rv, rv);

  fs.GetContentType(aContentTypeWithCharset);
  aCharset.Truncate();
  *aContentLength = 0;
  NS_ADDREF(*aBody = fs.GetSubmissionBody(aContentLength));

  return NS_OK;
}

already_AddRefed<FormData> FormData::Clone() {
  RefPtr<FormData> formData = new FormData(*this);
  return formData.forget();
}

nsresult FormData::CopySubmissionDataTo(
    HTMLFormSubmission* aFormSubmission) const {
  MOZ_ASSERT(aFormSubmission, "Must have FormSubmission!");
  for (size_t i = 0; i < mFormData.Length(); ++i) {
    if (mFormData[i].value.IsUSVString()) {
      aFormSubmission->AddNameValuePair(mFormData[i].name,
                                        mFormData[i].value.GetAsUSVString());
    } else if (mFormData[i].value.IsBlob()) {
      aFormSubmission->AddNameBlobPair(mFormData[i].name,
                                       mFormData[i].value.GetAsBlob());
    } else {
      MOZ_ASSERT(mFormData[i].value.IsDirectory());
      aFormSubmission->AddNameDirectoryPair(
          mFormData[i].name, mFormData[i].value.GetAsDirectory());
    }
  }

  return NS_OK;
}
