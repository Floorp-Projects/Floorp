/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ReferrerInfo.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIClassInfoImpl.h"

namespace mozilla {
namespace dom {

// Implementation of ClassInfo is required to serialize/deserialize
NS_IMPL_CLASSINFO(ReferrerInfo, nullptr, nsIClassInfo::MAIN_THREAD_ONLY,
                  REFERRERINFO_CID)

NS_IMPL_ISUPPORTS_CI(ReferrerInfo, nsIReferrerInfo, nsISerializable)

ReferrerInfo::ReferrerInfo(nsIURI* aOriginalReferrer, uint32_t aPolicy,
                           bool aSendReferrer)
    : mOriginalReferrer(aOriginalReferrer),
      mPolicy(aPolicy),
      mSendReferrer(aSendReferrer) {}

NS_IMETHODIMP
ReferrerInfo::GetOriginalReferrer(nsIURI** aOriginalReferrer) {
  *aOriginalReferrer = mOriginalReferrer;
  NS_IF_ADDREF(*aOriginalReferrer);
  return NS_OK;
}

NS_IMETHODIMP
ReferrerInfo::GetReferrerPolicy(uint32_t* aReferrerPolicy) {
  *aReferrerPolicy = mPolicy;
  return NS_OK;
}

NS_IMETHODIMP
ReferrerInfo::SetReferrerPolicy(uint32_t aReferrerPolicy) {
  mPolicy = aReferrerPolicy;
  return NS_OK;
}

NS_IMETHODIMP
ReferrerInfo::GetSendReferrer(bool* aSendReferrer) {
  *aSendReferrer = mSendReferrer;
  return NS_OK;
}

NS_IMETHODIMP
ReferrerInfo::SetSendReferrer(bool aSendReferrer) {
  mSendReferrer = aSendReferrer;
  return NS_OK;
}

NS_IMETHODIMP
ReferrerInfo::Init(uint32_t aReferrerPolicy, bool aSendReferrer,
                   nsIURI* aOriginalReferrer) {
  mPolicy = aReferrerPolicy;
  mSendReferrer = aSendReferrer;
  mOriginalReferrer = aOriginalReferrer;
  return NS_OK;
}

/* ===== nsISerializable implementation ====== */

NS_IMETHODIMP
ReferrerInfo::Read(nsIObjectInputStream* aStream) {
  nsresult rv;
  nsCOMPtr<nsISupports> supports;

  rv = NS_ReadOptionalObject(aStream, true, getter_AddRefs(supports));
  if (NS_FAILED(rv)) {
    return rv;
  }

  mOriginalReferrer = do_QueryInterface(supports);

  rv = aStream->Read32(&mPolicy);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aStream->ReadBoolean(&mSendReferrer);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
ReferrerInfo::Write(nsIObjectOutputStream* aStream) {
  nsresult rv = NS_WriteOptionalCompoundObject(aStream, mOriginalReferrer,
                                               NS_GET_IID(nsIURI), true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aStream->Write32(mPolicy);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aStream->WriteBoolean(mSendReferrer);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}
}  // namespace dom
}  // namespace mozilla
