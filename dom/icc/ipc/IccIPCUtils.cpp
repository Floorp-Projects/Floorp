/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/icc/IccIPCUtils.h"
#include "mozilla/dom/icc/PIccTypes.h"
#include "nsIIccContact.h"
#include "nsIIccInfo.h"

namespace mozilla {
namespace dom {
namespace icc {

/*static*/ void
IccIPCUtils::GetIccInfoDataFromIccInfo(nsIIccInfo* aInInfo,
                                       IccInfoData& aOutData)
{
  aInInfo->GetIccType(aOutData.iccType());
  aInInfo->GetIccid(aOutData.iccid());
  aInInfo->GetMcc(aOutData.mcc());
  aInInfo->GetMnc(aOutData.mnc());
  aInInfo->GetSpn(aOutData.spn());
  aInInfo->GetIsDisplayNetworkNameRequired(
    &aOutData.isDisplayNetworkNameRequired());
  aInInfo->GetIsDisplaySpnRequired(
    &aOutData.isDisplaySpnRequired());

  nsCOMPtr<nsIGsmIccInfo> gsmIccInfo(do_QueryInterface(aInInfo));
  if (gsmIccInfo) {
    gsmIccInfo->GetMsisdn(aOutData.phoneNumber());
  }

  nsCOMPtr<nsICdmaIccInfo> cdmaIccInfo(do_QueryInterface(aInInfo));
  if (cdmaIccInfo) {
    cdmaIccInfo->GetMdn(aOutData.phoneNumber());
    cdmaIccInfo->GetPrlVersion(&aOutData.prlVersion());
  }
}

/*static*/ void
IccIPCUtils::GetIccContactDataFromIccContact(nsIIccContact* aContact,
                                             IccContactData& aOutData){
  // Id
  nsresult rv = aContact->GetId(aOutData.id());
  NS_ENSURE_SUCCESS_VOID(rv);

  // Names
  char16_t** rawStringArray = nullptr;
  uint32_t count = 0;
  rv = aContact->GetNames(&count, &rawStringArray);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (count > 0) {
    for (uint32_t i = 0; i < count; i++) {
      aOutData.names().AppendElement(
        rawStringArray[i] ? nsDependentString(rawStringArray[i])
                          : NullString());
    }
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, rawStringArray);
  }

  // Numbers
  rawStringArray = nullptr;
  count = 0;
  rv = aContact->GetNumbers(&count, &rawStringArray);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (count > 0) {
    for (uint32_t i = 0; i < count; i++) {
      aOutData.numbers().AppendElement(
        rawStringArray[i] ? nsDependentString(rawStringArray[i])
                          : NullString());
    }
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, rawStringArray);
  }

  // Emails
  rawStringArray = nullptr;
  count = 0;
  rv = aContact->GetEmails(&count, &rawStringArray);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (count > 0) {
    for (uint32_t i = 0; i < count; i++) {
      aOutData.emails().AppendElement(
        rawStringArray[i] ? nsDependentString(rawStringArray[i])
                          : NullString());
    }
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, rawStringArray);
  }
}

} // namespace icc
} // namespace dom
} // namespace mozilla