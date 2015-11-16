/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TagUtils.h"
#include "mozilla/dom/Promise.h"

namespace mozilla {
namespace dom {
namespace nfc {

bool
TagUtils::IsTechSupported(const MozNFCTag& aTag,
                          const NFCTechType& aTechnology)
{
  ErrorResult rv;

  Nullable<nsTArray<NFCTechType>> techList;
  aTag.GetTechList(techList, rv);
  ENSURE_SUCCESS(rv, false);
  return !techList.IsNull() && techList.Value().Contains(aTechnology);
}

already_AddRefed<Promise>
TagUtils::Transceive(MozNFCTag* aTag,
                     const NFCTechType& aTechnology,
                     const Uint8Array& aCommand,
                     ErrorResult& aRv)
{
  ErrorResult rv;

  aCommand.ComputeLengthAndData();
  RefPtr<Promise> promise = aTag->Transceive(aTechnology, aCommand, rv);
  if (rv.Failed()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return promise.forget();
}

} // namespace nfc
} // namespace dom
} // namespace mozilla
