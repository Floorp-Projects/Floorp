/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFormData.h"
#include "nsIVariant.h"
#include "nsIInputStream.h"
#include "nsIDOMFile.h"
#include "nsContentUtils.h"
#include "nsHTMLFormElement.h"

nsFormData::nsFormData()
  : nsFormSubmission(NS_LITERAL_CSTRING("UTF-8"), nullptr)
{
}

// -------------------------------------------------------------------------
// nsISupports

DOMCI_DATA(FormData, nsFormData)

NS_IMPL_ADDREF(nsFormData)
NS_IMPL_RELEASE(nsFormData)
NS_INTERFACE_MAP_BEGIN(nsFormData)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFormData)
  NS_INTERFACE_MAP_ENTRY(nsIXHRSendable)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(FormData)
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

nsresult
nsFormData::AddNameValuePair(const nsAString& aName,
                             const nsAString& aValue)
{
  FormDataTuple* data = mFormData.AppendElement();
  data->name = aName;
  data->stringValue = aValue;
  data->valueIsFile = false;

  return NS_OK;
}

nsresult
nsFormData::AddNameFilePair(const nsAString& aName,
                            nsIDOMBlob* aBlob)
{
  FormDataTuple* data = mFormData.AppendElement();
  data->name = aName;
  data->fileValue = aBlob;
  data->valueIsFile = true;

  return NS_OK;
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
      return AddNameFilePair(aName, domBlob);
    }
  }

  PRUnichar* stringData = nullptr;
  uint32_t stringLen = 0;
  rv = aValue->GetAsWStringWithSize(&stringLen, &stringData);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString valAsString;
  valAsString.Adopt(stringData, stringLen);

  return AddNameValuePair(aName, valAsString);
}

// -------------------------------------------------------------------------
// nsIXHRSendable

NS_IMETHODIMP
nsFormData::GetSendInfo(nsIInputStream** aBody, nsACString& aContentType,
                        nsACString& aCharset)
{
  nsFSMultipartFormData fs(NS_LITERAL_CSTRING("UTF-8"), nullptr);
  
  for (uint32_t i = 0; i < mFormData.Length(); ++i) {
    if (mFormData[i].valueIsFile) {
      fs.AddNameFilePair(mFormData[i].name, mFormData[i].fileValue);
    }
    else {
      fs.AddNameValuePair(mFormData[i].name, mFormData[i].stringValue);
    }
  }

  fs.GetContentType(aContentType);
  aCharset.Truncate();
  NS_ADDREF(*aBody = fs.GetSubmissionBody());

  return NS_OK;
}


// -------------------------------------------------------------------------
// nsIJSNativeInitializer

NS_IMETHODIMP
nsFormData::Initialize(nsISupports* aOwner,
                       JSContext* aCx,
                       JSObject* aObj,
                       uint32_t aArgc,
                       jsval* aArgv)
{
  if (aArgc > 0) {
    if (JSVAL_IS_PRIMITIVE(aArgv[0])) {
      return NS_ERROR_UNEXPECTED;
    }
    nsCOMPtr<nsIContent> formCont = do_QueryInterface(
      nsContentUtils::XPConnect()->
        GetNativeOfWrapper(aCx, JSVAL_TO_OBJECT(aArgv[0])));
    
    if (!formCont || !formCont->IsHTML(nsGkAtoms::form)) {
      return NS_ERROR_UNEXPECTED;
    }

    nsresult rv = static_cast<nsHTMLFormElement*>(formCont.get())->
      WalkFormElements(this);
    NS_ENSURE_SUCCESS(rv, rv);
  }


  return NS_OK;
}
