/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientUsageArray.h"

#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"

namespace mozilla::dom::quota {

void ClientUsageArray::Serialize(nsACString& aText) const {
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  bool first = true;

  for (Client::Type type : quotaManager->AllClientTypes()) {
    const Maybe<uint64_t>& clientUsage = ElementAt(type);
    if (clientUsage.isSome()) {
      if (first) {
        first = false;
      } else {
        aText.Append(" ");
      }

      aText.Append(Client::TypeToPrefix(type));
      aText.AppendInt(clientUsage.value());
    }
  }
}

nsresult ClientUsageArray::Deserialize(const nsACString& aText) {
  for (const auto& token :
       nsCCharSeparatedTokenizerTemplate<NS_TokenizerIgnoreNothing>(aText, ' ')
           .ToRange()) {
    QM_TRY(OkIf(token.Length() >= 2), NS_ERROR_FAILURE);

    Client::Type clientType;
    QM_TRY(OkIf(Client::TypeFromPrefix(token.First(), clientType, fallible)),
           NS_ERROR_FAILURE);

    nsresult rv;
    const uint64_t usage = Substring(token, 1).ToInteger64(&rv);
    QM_TRY(MOZ_TO_RESULT(rv));

    ElementAt(clientType) = Some(usage);
  }

  return NS_OK;
}

}  // namespace mozilla::dom::quota
