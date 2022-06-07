/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_CLIENTUSAGEARAY_H_
#define DOM_QUOTA_CLIENTUSAGEARAY_H_

#include <cstdint>
#include "mozilla/Maybe.h"
#include "mozilla/dom/quota/Client.h"

namespace mozilla::dom::quota {

// XXX Change this not to derive from AutoTArray.
class ClientUsageArray final
    : public AutoTArray<Maybe<uint64_t>, Client::TYPE_MAX> {
 public:
  ClientUsageArray() { SetLength(Client::TypeMax()); }

  void Serialize(nsACString& aText) const;

  nsresult Deserialize(const nsACString& aText);

  ClientUsageArray Clone() const {
    ClientUsageArray res;
    res.Assign(*this);
    return res;
  }
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_CLIENTUSAGEARAY_H_
