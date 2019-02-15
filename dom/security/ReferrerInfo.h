/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReferrerInfo_h
#define mozilla_dom_ReferrerInfo_h

#include "nsCOMPtr.h"
#include "nsIReferrerInfo.h"
#include "nsISerializable.h"
#include "mozilla/net/ReferrerPolicy.h"

#define REFERRERINFOF_CONTRACTID "@mozilla.org/referrer-info;1"
// 041a129f-10ce-4bda-a60d-e027a26d5ed0
#define REFERRERINFO_CID                             \
  {                                                  \
    0x041a129f, 0x10ce, 0x4bda, {                    \
      0xa6, 0x0d, 0xe0, 0x27, 0xa2, 0x6d, 0x5e, 0xd0 \
    }                                                \
  }

class nsIURI;
class nsIChannel;

namespace mozilla {
namespace dom {

/**
 * ReferrerInfo class holds a original referrer URL, as well as the referrer
 * policy to be applied to this referrer.
 *
 **/
class ReferrerInfo : public nsIReferrerInfo {
 public:
  ReferrerInfo() = default;
  explicit ReferrerInfo(nsIURI* aOriginalReferrer,
                        uint32_t aPolicy = mozilla::net::RP_Unset,
                        bool aSendReferrer = true);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREFERRERINFO
  NS_DECL_NSISERIALIZABLE

 private:
  virtual ~ReferrerInfo() {}

  nsCOMPtr<nsIURI> mOriginalReferrer;
  uint32_t mPolicy;

  // Indicates if the referrer should be sent or not even when it's available
  // (default is true).
  bool mSendReferrer;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ReferrerInfo_h
