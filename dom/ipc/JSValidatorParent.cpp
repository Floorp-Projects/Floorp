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
    const std::function<void(bool, Maybe<Shmem>)>& aCallback) {
  JSOracleParent::WithJSOracle([=, self = RefPtr{this}](const auto* aParent) {
    if (aParent) {
      MOZ_DIAGNOSTIC_ASSERT(self->CanSend());
      self->SendIsOpaqueResponseAllowed()->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [aCallback](
              const IsOpaqueResponseAllowedPromise::ResolveOrRejectValue&
                  aResult) {
            if (aResult.IsResolve()) {
              const Tuple<bool, Maybe<Shmem>>& result = aResult.ResolveValue();
              aCallback(Get<0>(result), Get<1>(aResult.ResolveValue()));
            } else {
              aCallback(false, Nothing());
            }
          });
    } else {
      aCallback(false, Nothing());
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

void JSValidatorParent::OnStopRequest(nsresult aResult) {
  JSOracleParent::WithJSOracle(
      [self = RefPtr{this}, aResult](const auto* aParent) {
        if (!aParent) {
          return;
        }
        if (self->CanSend()) {
          Unused << self->SendOnStopRequest(aResult);
        }
      });
}
}  // namespace mozilla::dom
