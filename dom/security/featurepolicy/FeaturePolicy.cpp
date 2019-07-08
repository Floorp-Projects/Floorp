/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FeaturePolicy.h"
#include "mozilla/dom/FeaturePolicyBinding.h"
#include "mozilla/dom/FeaturePolicyParser.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FeaturePolicy, mParentNode)
NS_IMPL_CYCLE_COLLECTING_ADDREF(FeaturePolicy)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FeaturePolicy)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FeaturePolicy)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

FeaturePolicy::FeaturePolicy(nsINode* aNode) : mParentNode(aNode) {}

void FeaturePolicy::InheritPolicy(FeaturePolicy* aParentPolicy) {
  MOZ_ASSERT(aParentPolicy);

  mInheritedDeniedFeatureNames.Clear();

  RefPtr<FeaturePolicy> dest = this;
  RefPtr<FeaturePolicy> src = aParentPolicy;
  FeaturePolicyUtils::ForEachFeature([dest, src](const char* aFeatureName) {
    nsString featureName;
    featureName.AppendASCII(aFeatureName);

    // If the destination has a declared feature (via the HTTP header or 'allow'
    // attribute) we allow the feature if the destination allows it and the
    // parent allows its origin or the destinations' one.
    if (dest->HasDeclaredFeature(featureName) &&
        dest->AllowsFeatureInternal(featureName, dest->mDefaultOrigin)) {
      if (!src->AllowsFeatureInternal(featureName, src->mDefaultOrigin) &&
          !src->AllowsFeatureInternal(featureName, dest->mDefaultOrigin)) {
        dest->SetInheritedDeniedFeature(featureName);
      }
      return;
    }

    // If there was not a declared feature, we allow the feature if the parent
    // FeaturePolicy allows the current origin.
    if (!src->AllowsFeatureInternal(featureName, dest->mDefaultOrigin)) {
      dest->SetInheritedDeniedFeature(featureName);
    }
  });
}

void FeaturePolicy::SetInheritedDeniedFeature(const nsAString& aFeatureName) {
  MOZ_ASSERT(!HasInheritedDeniedFeature(aFeatureName));
  mInheritedDeniedFeatureNames.AppendElement(aFeatureName);
}

bool FeaturePolicy::HasInheritedDeniedFeature(
    const nsAString& aFeatureName) const {
  return mInheritedDeniedFeatureNames.Contains(aFeatureName);
}

bool FeaturePolicy::HasDeclaredFeature(const nsAString& aFeatureName) const {
  for (const Feature& feature : mFeatures) {
    if (feature.Name().Equals(aFeatureName)) {
      return true;
    }
  }

  return false;
}

void FeaturePolicy::SetDeclaredPolicy(Document* aDocument,
                                      const nsAString& aPolicyString,
                                      nsIPrincipal* aSelfOrigin,
                                      nsIPrincipal* aSrcOrigin) {
  ResetDeclaredPolicy();

  Unused << NS_WARN_IF(!FeaturePolicyParser::ParseString(
      aPolicyString, aDocument, aSelfOrigin, aSrcOrigin, mFeatures));
}

void FeaturePolicy::ResetDeclaredPolicy() { mFeatures.Clear(); }

JSObject* FeaturePolicy::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return FeaturePolicy_Binding::Wrap(aCx, this, aGivenProto);
}

bool FeaturePolicy::AllowsFeature(const nsAString& aFeatureName,
                                  const Optional<nsAString>& aOrigin) const {
  nsCOMPtr<nsIPrincipal> origin;
  if (aOrigin.WasPassed()) {
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), aOrigin.Value());
    if (NS_FAILED(rv)) {
      return false;
    }
    origin = BasePrincipal::CreateContentPrincipal(
        uri, BasePrincipal::Cast(mDefaultOrigin)->OriginAttributesRef());
  } else {
    origin = mDefaultOrigin;
  }

  if (NS_WARN_IF(!origin)) {
    return false;
  }

  return AllowsFeatureInternal(aFeatureName, origin);
}

bool FeaturePolicy::AllowsFeatureInternal(const nsAString& aFeatureName,
                                          nsIPrincipal* aOrigin) const {
  MOZ_ASSERT(aOrigin);

  // Let's see if have to disable this feature because inherited policy.
  if (HasInheritedDeniedFeature(aFeatureName)) {
    return false;
  }

  for (const Feature& feature : mFeatures) {
    if (feature.Name().Equals(aFeatureName)) {
      return feature.Allows(aOrigin);
    }
  }

  switch (FeaturePolicyUtils::DefaultAllowListFeature(aFeatureName)) {
    case FeaturePolicyUtils::FeaturePolicyValue::eAll:
      return true;

    case FeaturePolicyUtils::FeaturePolicyValue::eSelf:
      return BasePrincipal::Cast(mDefaultOrigin)
          ->Subsumes(aOrigin, BasePrincipal::ConsiderDocumentDomain);

    case FeaturePolicyUtils::FeaturePolicyValue::eNone:
      return false;

    default:
      MOZ_CRASH("Unknown default value");
  }

  return false;
}

void FeaturePolicy::Features(nsTArray<nsString>& aFeatures) {
  RefPtr<FeaturePolicy> self = this;
  FeaturePolicyUtils::ForEachFeature(
      [self, &aFeatures](const char* aFeatureName) {
        nsString featureName;
        featureName.AppendASCII(aFeatureName);
        aFeatures.AppendElement(featureName);
      });
}

void FeaturePolicy::AllowedFeatures(nsTArray<nsString>& aAllowedFeatures) {
  RefPtr<FeaturePolicy> self = this;
  FeaturePolicyUtils::ForEachFeature(
      [self, &aAllowedFeatures](const char* aFeatureName) {
        nsString featureName;
        featureName.AppendASCII(aFeatureName);

        if (self->AllowsFeatureInternal(featureName, self->mDefaultOrigin)) {
          aAllowedFeatures.AppendElement(featureName);
        }
      });
}

void FeaturePolicy::GetAllowlistForFeature(const nsAString& aFeatureName,
                                           nsTArray<nsString>& aList) const {
  if (!AllowsFeatureInternal(aFeatureName, mDefaultOrigin)) {
    return;
  }

  for (const Feature& feature : mFeatures) {
    if (feature.Name().Equals(aFeatureName)) {
      if (feature.AllowsAll()) {
        aList.AppendElement(NS_LITERAL_STRING("*"));
        return;
      }

      nsTArray<nsCOMPtr<nsIPrincipal>> list;
      feature.GetAllowList(list);

      for (nsIPrincipal* principal : list) {
        nsAutoCString originNoSuffix;
        nsresult rv = principal->GetOriginNoSuffix(originNoSuffix);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return;
        }

        aList.AppendElement(NS_ConvertUTF8toUTF16(originNoSuffix));
      }
      return;
    }
  }

  switch (FeaturePolicyUtils::DefaultAllowListFeature(aFeatureName)) {
    case FeaturePolicyUtils::FeaturePolicyValue::eAll:
      aList.AppendElement(NS_LITERAL_STRING("*"));
      return;

    case FeaturePolicyUtils::FeaturePolicyValue::eSelf: {
      nsAutoCString originNoSuffix;
      nsresult rv = mDefaultOrigin->GetOriginNoSuffix(originNoSuffix);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return;
      }

      aList.AppendElement(NS_ConvertUTF8toUTF16(originNoSuffix));
      return;
    }

    case FeaturePolicyUtils::FeaturePolicyValue::eNone:
      return;

    default:
      MOZ_CRASH("Unknown default value");
  }
}

void FeaturePolicy::MaybeSetAllowedPolicy(const nsAString& aFeatureName) {
  MOZ_ASSERT(FeaturePolicyUtils::IsSupportedFeature(aFeatureName));

  if (HasDeclaredFeature(aFeatureName)) {
    return;
  }

  Feature feature(aFeatureName);
  feature.SetAllowsAll();

  mFeatures.AppendElement(feature);
}

}  // namespace dom
}  // namespace mozilla
