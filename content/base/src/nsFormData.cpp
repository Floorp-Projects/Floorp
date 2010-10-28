/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsFormData.h"
#include "nsIVariant.h"
#include "nsIInputStream.h"
#include "nsIDOMFile.h"
#include "nsContentUtils.h"
#include "nsHTMLFormElement.h"

nsFormData::nsFormData()
  : nsFormSubmission(NS_LITERAL_CSTRING("UTF-8"), nsnull)
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
  data->valueIsFile = PR_FALSE;

  return NS_OK;
}

nsresult
nsFormData::AddNameFilePair(const nsAString& aName,
                            nsIDOMBlob* aBlob)
{
  FormDataTuple* data = mFormData.AppendElement();
  data->name = aName;
  data->fileValue = aBlob;
  data->valueIsFile = PR_TRUE;

  return NS_OK;
}

// -------------------------------------------------------------------------
// nsIDOMFormData

NS_IMETHODIMP
nsFormData::Append(const nsAString& aName, nsIVariant* aValue)
{
  PRUint16 dataType;
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

  PRUnichar* stringData = nsnull;
  PRUint32 stringLen = 0;
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
  nsFSMultipartFormData fs(NS_LITERAL_CSTRING("UTF-8"), nsnull);
  
  for (PRUint32 i = 0; i < mFormData.Length(); ++i) {
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
                       PRUint32 aArgc,
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
