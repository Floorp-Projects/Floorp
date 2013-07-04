/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFormData.h"
#include "nsIVariant.h"
#include "nsIInputStream.h"
#include "nsIDOMFile.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozilla/dom/FormDataBinding.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

nsFormData::nsFormData(nsISupports* aOwner)
  : nsFormSubmission(NS_LITERAL_CSTRING("UTF-8"), nullptr)
  , mOwner(aOwner)
{
  SetIsDOMBinding();
}

// -------------------------------------------------------------------------
// nsISupports

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsFormData, mOwner)
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFormData)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFormData)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFormData)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMFormData)
  NS_INTERFACE_MAP_ENTRY(nsIXHRSendable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFormData)
NS_INTERFACE_MAP_END

// -------------------------------------------------------------------------
// nsFormSubmission
nsresult
nsFormData::GetEncodedSubmission(nsIURI* aURI,
                                 nsIInputStream** aPostDataStream)
{
  NS_NOTREACHED("Shouldn't call nsFormData::GetEncodedSubmission");
  return NS_OK;
}

void
nsFormData::Append(const nsAString& aName, const nsAString& aValue)
{
  AddNameValuePair(aName, aValue);
}

void
nsFormData::Append(const nsAString& aName, nsIDOMBlob* aBlob,
                   const Optional<nsAString>& aFilename)
{
  nsString filename;
  if (aFilename.WasPassed()) {
    filename = aFilename.Value();
  } else {
    filename.SetIsVoid(true);
  }
  AddNameFilePair(aName, aBlob, filename);
}

// -------------------------------------------------------------------------
// nsIDOMFormData

NS_IMETHODIMP
nsFormData::Append(const nsAString& aName, nsIVariant* aValue)
{
  uint16_t dataType;
  nsresult rv = aValue->GetDataType(&dataType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (dataType == nsIDataType::VTYPE_INTERFACE ||
      dataType == nsIDataType::VTYPE_INTERFACE_IS) {
    nsCOMPtr<nsISupports> supports;
    nsID *iid;
    rv = aValue->GetAsInterface(&iid, getter_AddRefs(supports));
    NS_ENSURE_SUCCESS(rv, rv);

    nsMemory::Free(iid);

    nsCOMPtr<nsIDOMBlob> domBlob = do_QueryInterface(supports);
    if (domBlob) {
      Optional<nsAString> temp;
      Append(aName, domBlob, temp);
      return NS_OK;
    }
  }

  PRUnichar* stringData = nullptr;
  uint32_t stringLen = 0;
  rv = aValue->GetAsWStringWithSize(&stringLen, &stringData);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString valAsString;
  valAsString.Adopt(stringData, stringLen);

  Append(aName, valAsString);
  return NS_OK;
}

/* virtual */ JSObject*
nsFormData::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return FormDataBinding::Wrap(aCx, aScope, this);
}

/* static */ already_AddRefed<nsFormData>
nsFormData::Constructor(const GlobalObject& aGlobal,
                        const Optional<NonNull<HTMLFormElement> >& aFormElement,
                        ErrorResult& aRv)
{
  nsRefPtr<nsFormData> formData = new nsFormData(aGlobal.Get());
  if (aFormElement.WasPassed()) {
    aRv = aFormElement.Value().WalkFormElements(formData);
  }
  return formData.forget();
}

// -------------------------------------------------------------------------
// nsIXHRSendable

NS_IMETHODIMP
nsFormData::GetSendInfo(nsIInputStream** aBody, uint64_t* aContentLength,
                        nsACString& aContentType, nsACString& aCharset)
{
  nsFSMultipartFormData fs(NS_LITERAL_CSTRING("UTF-8"), nullptr);

  for (uint32_t i = 0; i < mFormData.Length(); ++i) {
    if (mFormData[i].valueIsFile) {
      fs.AddNameFilePair(mFormData[i].name, mFormData[i].fileValue,
                         mFormData[i].filename);
    }
    else {
      fs.AddNameValuePair(mFormData[i].name, mFormData[i].stringValue);
    }
  }

  fs.GetContentType(aContentType);
  aCharset.Truncate();
  *aContentLength = 0;
  NS_ADDREF(*aBody = fs.GetSubmissionBody(aContentLength));

  return NS_OK;
}
