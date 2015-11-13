/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nfc_TagUtils_h__
#define mozilla_dom_nfc_TagUtils_h__

#include "mozilla/dom/MozNFCTagBinding.h"

namespace mozilla {
namespace dom {
namespace nfc {

/**
 * Class of static helper functions for nfc tag.
 */
class TagUtils
{
public:
  /**
   * Check if specified technogy is supported in this tag.
   */
  static bool
  IsTechSupported(const MozNFCTag& aNFCTag,
                  const NFCTechType& aTechnology);

  /**
   * Send raw command to tag and receive the response.
   */
  static already_AddRefed<Promise>
  Transceive(MozNFCTag* aTag,
             const NFCTechType& aTechnology,
             const Uint8Array& aCommand,
             ErrorResult& aRv);
};

} // namespace nfc
} // namespace dom
} // namespace mozilla

#endif  // mozilla_dom_nfc_TagUtils_h__
