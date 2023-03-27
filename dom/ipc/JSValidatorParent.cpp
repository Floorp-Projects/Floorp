/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/OpaqueResponseUtils.h"
#include "mozilla/dom/JSValidatorParent.h"
#include "mozilla/dom/JSValidatorUtils.h"
#include "mozilla/dom/JSOracleParent.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "HttpBaseChannel.h"

namespace mozilla::dom {
/* static */
already_AddRefed<JSValidatorParent> JSValidatorParent::Create() {
  RefPtr<JSValidatorParent> validator = new JSValidatorParent();
  JSOracleParent::WithJSOracle([validator](JSOracleParent* aParent) {
    MOZ_ASSERT_IF(aParent, aParent->CanSend());
    if (aParent) {
      MOZ_ALWAYS_TRUE(aParent->SendPJSValidatorConstructor(validator));
    }
  });
  return validator.forget();
}

void JSValidatorParent::IsOpaqueResponseAllowed(
    const std::function<void(Maybe<Shmem>, ValidatorResult)>& aCallback) {
  JSOracleParent::WithJSOracle([=, self = RefPtr{this}](const auto* aParent) {
    if (aParent) {
      MOZ_DIAGNOSTIC_ASSERT(self->CanSend());
      self->SendIsOpaqueResponseAllowed()->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [aCallback](
              const IsOpaqueResponseAllowedPromise::ResolveOrRejectValue&
                  aResult) {
            if (aResult.IsResolve()) {
              auto [data, result] = aResult.ResolveValue();
              aCallback(std::move(data), result);
            } else {
              // For cases like the Utility Process crashes, the promise will be
              // rejected due to sending failures, and we'll block the request
              // since we can't validate it.
              aCallback(Nothing(), ValidatorResult::Failure);
            }
          });
    } else {
      aCallback(Nothing(), ValidatorResult::Failure);
    }
  });
}

void JSValidatorParent::OnDataAvailable(const nsACString& aData) {
  JSOracleParent::WithJSOracle(
      [self = RefPtr{this}, data = nsCString{aData}](const auto* aParent) {
        if (!aParent) {
          return;
        }

        if (self->CanSend()) {
          Shmem sharedData;
          nsresult rv =
              JSValidatorUtils::CopyCStringToShmem(self, data, sharedData);
          if (NS_FAILED(rv)) {
            return;
          }
          Unused << self->SendOnDataAvailable(std::move(sharedData));
        }
      });
}

void JSValidatorParent::OnStopRequest(nsresult aResult, nsIRequest& aRequest) {
  JSOracleParent::WithJSOracle(
      [self = RefPtr{this}, aResult,
       request = nsCOMPtr{&aRequest}](const auto* aParent) {
        if (!aParent) {
          return;
        }
        if (self->CanSend() && request) {
          nsCOMPtr<net::HttpBaseChannel> httpBaseChannel =
              do_QueryInterface(request);
          MOZ_ASSERT(httpBaseChannel);

          nsAutoCString contentCharset;
          Unused << httpBaseChannel->GetContentCharset(contentCharset);

          nsAutoString hintCharset;
          Unused << httpBaseChannel->GetClassicScriptHintCharset(hintCharset);

          nsAutoString documentCharset;
          Unused << httpBaseChannel->GetDocumentCharacterSet(documentCharset);

          Unused << self->SendOnStopRequest(aResult, contentCharset,
                                            hintCharset, documentCharset);
        }
      });
}
}  // namespace mozilla::dom
