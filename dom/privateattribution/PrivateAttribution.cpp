/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrivateAttribution.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PrivateAttributionBinding.h"
#include "mozilla/Components.h"
#include "nsIGlobalObject.h"
#include "nsIPrivateAttributionService.h"
#include "nsXULAppAPI.h"
#include "nsURLHelper.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PrivateAttribution, mOwner)

PrivateAttribution::PrivateAttribution(nsIGlobalObject* aGlobal)
    : mOwner(aGlobal) {
  MOZ_ASSERT(aGlobal);
}

JSObject* PrivateAttribution::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return PrivateAttribution_Binding::Wrap(aCx, this, aGivenProto);
}

PrivateAttribution::~PrivateAttribution() = default;

bool PrivateAttribution::GetSourceHostIfNonPrivate(nsACString& aSourceHost,
                                                   ErrorResult& aRv) {
  MOZ_ASSERT(mOwner);
  nsIPrincipal* prin = mOwner->PrincipalOrNull();
  if (!prin || NS_FAILED(prin->GetHost(aSourceHost))) {
    aRv.ThrowInvalidStateError("Couldn't get source host");
    return false;
  }
  if (prin->GetPrivateBrowsingId() > 0) {
    return false;  // Do not throw.
  }
  return true;
}

[[nodiscard]] static bool ValidateHost(const nsACString& aHost,
                                       ErrorResult& aRv) {
  if (!net_IsValidHostName(aHost)) {
    aRv.ThrowSyntaxError(aHost + " is not a valid host name"_ns);
    return false;
  }
  return true;
}

void PrivateAttribution::SaveImpression(
    const PrivateAttributionImpressionOptions& aOptions, ErrorResult& aRv) {
  if (!StaticPrefs::dom_private_attribution_submission_enabled()) {
    return;
  }

  nsAutoCString source;
  if (!GetSourceHostIfNonPrivate(source, aRv)) {
    return;
  }

  if (!ValidateHost(aOptions.mTarget, aRv)) {
    return;
  }

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIPrivateAttributionService> pa =
        components::PrivateAttribution::Service();
    if (NS_WARN_IF(!pa)) {
      return;
    }
    pa->OnAttributionEvent(source, GetEnumString(aOptions.mType),
                           aOptions.mIndex, aOptions.mAd, aOptions.mTarget);
    return;
  }

  auto* content = ContentChild::GetSingleton();
  if (NS_WARN_IF(!content)) {
    return;
  }
  content->SendAttributionEvent(source, aOptions.mType, aOptions.mIndex,
                                aOptions.mAd, aOptions.mTarget);
}

void PrivateAttribution::MeasureConversion(
    const PrivateAttributionConversionOptions& aOptions, ErrorResult& aRv) {
  if (!StaticPrefs::dom_private_attribution_submission_enabled()) {
    return;
  }

  nsAutoCString source;
  if (!GetSourceHostIfNonPrivate(source, aRv)) {
    return;
  }
  for (const nsACString& host : aOptions.mSources) {
    if (!ValidateHost(host, aRv)) {
      return;
    }
  }
  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIPrivateAttributionService> pa =
        components::PrivateAttribution::Service();
    if (NS_WARN_IF(!pa)) {
      return;
    }
    pa->OnAttributionConversion(
        source, aOptions.mTask, aOptions.mHistogramSize,
        aOptions.mLookbackDays.WasPassed() ? aOptions.mLookbackDays.Value() : 0,
        aOptions.mImpression.WasPassed()
            ? GetEnumString(aOptions.mImpression.Value())
            : EmptyCString(),
        aOptions.mAds, aOptions.mSources);
    return;
  }

  auto* content = ContentChild::GetSingleton();
  if (NS_WARN_IF(!content)) {
    return;
  }
  content->SendAttributionConversion(
      source, aOptions.mTask, aOptions.mHistogramSize,
      aOptions.mLookbackDays.WasPassed() ? Some(aOptions.mLookbackDays.Value())
                                         : Nothing(),
      aOptions.mImpression.WasPassed() ? Some(aOptions.mImpression.Value())
                                       : Nothing(),
      aOptions.mAds, aOptions.mSources);
}

}  // namespace mozilla::dom
