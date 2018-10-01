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

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FeaturePolicy, mParentNode)
NS_IMPL_CYCLE_COLLECTING_ADDREF(FeaturePolicy)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FeaturePolicy)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FeaturePolicy)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

FeaturePolicy::FeaturePolicy(nsINode* aNode)
  : mParentNode(aNode)
{}

void
FeaturePolicy::InheritPolicy(FeaturePolicy* aParentPolicy)
{
  MOZ_ASSERT(aParentPolicy);

  mInheritedDeniedFeatureNames.Clear();

  RefPtr<FeaturePolicy> dest = this;
  RefPtr<FeaturePolicy> src = aParentPolicy;
  nsString origin = mDefaultOrigin;
  FeaturePolicyUtils::ForEachFeature([dest, src, origin](const char* aFeatureName) {
    nsString featureName;
    featureName.AppendASCII(aFeatureName);

    // If the destination has a declared feature (via the HTTP header or 'allow'
    // attribute) we allow the feature only if both parent FeaturePolicy and this
    // one allow the current origin.
    if (dest->HasDeclaredFeature(featureName)) {
      if (!dest->AllowsFeatureInternal(featureName, origin) ||
          !src->AllowsFeatureInternal(featureName, origin)) {
        dest->SetInheritedDeniedFeature(featureName);
      }
      return;
    }

    // If there was not a declared feature, we allow the feature if the parent
    // FeaturePolicy allows the current origin.
    if (!src->AllowsFeatureInternal(featureName, origin)) {
      dest->SetInheritedDeniedFeature(featureName);
    }
  });
}

void
FeaturePolicy::SetInheritedDeniedFeature(const nsAString& aFeatureName)
{
  MOZ_ASSERT(!HasInheritedDeniedFeature(aFeatureName));
  mInheritedDeniedFeatureNames.AppendElement(aFeatureName);
}

bool
FeaturePolicy::HasInheritedDeniedFeature(const nsAString& aFeatureName) const
{
  return mInheritedDeniedFeatureNames.Contains(aFeatureName);
}

bool
FeaturePolicy::HasDeclaredFeature(const nsAString& aFeatureName) const
{
  for (const Feature& feature : mFeatures) {
    if (feature.Name().Equals(aFeatureName)) {
      return true;
    }
  }

  return false;
}

void
FeaturePolicy::SetDeclaredPolicy(nsIDocument* aDocument,
                                 const nsAString& aPolicyString,
                                 const nsAString& aSelfOrigin,
                                 const nsAString& aSrcOrigin,
                                 bool aSrcEnabled)
{
  ResetDeclaredPolicy();

  Unused << NS_WARN_IF(!FeaturePolicyParser::ParseString(aPolicyString,
                                                         aDocument,
                                                         aSelfOrigin,
                                                         aSrcOrigin,
                                                         aSrcEnabled,
                                                         mFeatures));
}

void
FeaturePolicy::ResetDeclaredPolicy()
{
  mFeatures.Clear();
}

JSObject*
FeaturePolicy::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return Policy_Binding::Wrap(aCx, this, aGivenProto);
}

bool
FeaturePolicy::AllowsFeature(const nsAString& aFeatureName,
                             const Optional<nsAString>& aOrigin) const
{
  return AllowsFeatureInternal(aFeatureName,
                               aOrigin.WasPassed()
                                 ? aOrigin.Value()
                                 : mDefaultOrigin);
}

bool
FeaturePolicy::AllowsFeatureInternal(const nsAString& aFeatureName,
                                     const nsAString& aOrigin) const
{
  // Let's see if have to disable this feature because inherited policy.
  if (HasInheritedDeniedFeature(aFeatureName)) {
    return false;
  }

  for (const Feature& feature : mFeatures) {
    if (feature.Name().Equals(aFeatureName)) {
      return feature.Allows(aOrigin);
    }
  }

  return FeaturePolicyUtils::AllowDefaultFeature(aFeatureName, mDefaultOrigin,
                                                 aOrigin);
}

void
FeaturePolicy::AllowedFeatures(nsTArray<nsString>& aAllowedFeatures)
{
  RefPtr<FeaturePolicy> self = this;
  FeaturePolicyUtils::ForEachFeature([self, &aAllowedFeatures](const char* aFeatureName) {
    nsString featureName;
    featureName.AppendASCII(aFeatureName);

    if (self->AllowsFeatureInternal(featureName, self->mDefaultOrigin)) {
      aAllowedFeatures.AppendElement(featureName);
    }
  });
}

void
FeaturePolicy::GetAllowlistForFeature(const nsAString& aFeatureName,
                                      nsTArray<nsString>& aList) const
{
  if (!AllowsFeatureInternal(aFeatureName, mDefaultOrigin)) {
    return;
  }

  for (const Feature& feature : mFeatures) {
    if (feature.Name().Equals(aFeatureName)) {
      if (feature.AllowsAll()) {
        aList.AppendElement(NS_LITERAL_STRING("*"));
        return;
      }

      feature.GetWhiteListedOrigins(aList);
      return;
    }
  }

  nsString defaultAllowList;
  FeaturePolicyUtils::DefaultAllowListFeature(aFeatureName, mDefaultOrigin,
                                              defaultAllowList);
   if (!defaultAllowList.IsEmpty()) {
    aList.AppendElement(defaultAllowList);
  }
}
