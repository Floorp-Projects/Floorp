/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IccIPCService.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/Preferences.h"

namespace mozilla {
namespace dom {
namespace icc {

NS_IMPL_ISUPPORTS(IccIPCService, nsIIccService)

IccIPCService::IccIPCService()
{
  int32_t numRil = Preferences::GetInt("ril.numRadioInterfaces", 1);
  mIccs.SetLength(numRil);
}

IccIPCService::~IccIPCService()
{
  uint32_t count = mIccs.Length();
  for (uint32_t i = 0; i < count; i++) {
    if (mIccs[i]) {
      mIccs[i]->Shutdown();
    }
  }
}

NS_IMETHODIMP
IccIPCService::GetIccByServiceId(uint32_t aServiceId, nsIIcc** aIcc)
{
  NS_ENSURE_TRUE(aServiceId < mIccs.Length(), NS_ERROR_INVALID_ARG);

  if (!mIccs[aServiceId]) {
    RefPtr<IccChild> child = new IccChild();

    // |SendPIccConstructor| adds another reference to the child
    // actor and removes in |DeallocPIccChild|.
    ContentChild::GetSingleton()->SendPIccConstructor(child, aServiceId);
    child->Init();

    mIccs[aServiceId] = child;
  }

  nsCOMPtr<nsIIcc> icc(mIccs[aServiceId]);
  icc.forget(aIcc);

  return NS_OK;
}

} // namespace icc
} // namespace dom
} // namespace mozilla
