/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FeaturePolicy_h
#define mozilla_dom_FeaturePolicy_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Feature.h"
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

/**
 * FeaturePolicy
 * ~~~~~~~~~~~~~
 *
 * Each document and each HTMLIFrameElement have a FeaturePolicy object which is
 * used to allow or deny features in their contexts. FeaturePolicy is active
 * when pref dom.security.featurePolicy.enabled is set to true.
 *
 * FeaturePolicy is composed by a set of directives configured by the
 * 'Feature-Policy' HTTP Header and the 'allow' attribute in HTMLIFrameElements.
 * Both header and attribute are parsed by FeaturePolicyParser which returns an
 * array of Feature objects. Each Feature object has a feature name and one of
 * these policies:
 * - eNone - the feature is fully disabled.
 * - eAll - the feature is allowed.
 * - eWhitelist - the feature is allowed for a list of origins.
 *
 * An interesting element of FeaturePolicy is the inheritance: each context
 * inherits the feature-policy directives from the parent context, if it exists.
 * When a context inherits a policy for feature X, it only knows if that feature
 * is allowed or denied (it ignores the list of whitelist origins for instance).
 * This information is stored in an array of inherited feature strings because
 * we care only to know when they are denied.
 *
 * FeaturePolicy can be reset if the 'allow' or 'src' attributes change in
 * HTMLIFrameElements. 'src' attribute is important to compute correcly
 * the features via FeaturePolicy 'src' keyword.
 *
 * When FeaturePolicy must decide if feature X is allowed or denied for the
 * current origin, it checks if the parent context denied that feature.
 * If not, it checks if there is a Feature object for that
 * feature named X and if the origin is allowed or not.
 *
 * From a C++ point of view, use FeaturePolicyUtils to obtain the list of
 * features and to check if they are allowed in the current context.
 **/

class nsIDocument;
class nsIHttpChannel;
class nsINode;

namespace mozilla {
namespace dom {

class FeaturePolicyUtils;

class FeaturePolicy final : public nsISupports
                          , public nsWrapperCache
{
  friend class FeaturePolicyUtils;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FeaturePolicy)

  explicit FeaturePolicy(nsINode* aNode);

  // A FeaturePolicy must have a default origin, if not in a sandboxed context.
  // This method must be called before any other exposed WebIDL method or before
  // checking if a feature is allowed.
  void
  SetDefaultOrigin(const nsAString& aOrigin)
  {
    // aOrigin can be an empty string if this is a opaque origin.
    mDefaultOrigin = aOrigin;
  }

  const nsAString& DefaultOrigin() const
  {
    // Returns an empty string if this is an opaque origin.
    return mDefaultOrigin;
  }

  // Inherits the policy from the 'parent' context if it exists.
  void
  InheritPolicy(FeaturePolicy* aParentFeaturePolicy);

  // Sets the declarative part of the policy. This can be from the HTTP header
  // or for the 'allow' HTML attribute.
  void
  SetDeclaredPolicy(nsIDocument* aDocument,
                    const nsAString& aPolicyString,
                    const nsAString& aSelfOrigin,
                    const nsAString& aSrcOrigin,
                    bool aSrcEnabled);

  // Clears all the declarative policy directives. This is needed when the
  // 'allow' attribute or the 'src' attribute change for HTMLIFrameElement's
  // policy.
  void
  ResetDeclaredPolicy();

  // WebIDL internal methods.

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsINode*
  GetParentObject() const
  {
    return mParentNode;
  }

  // WebIDL explosed methods.

  bool
  AllowsFeature(const nsAString& aFeatureName,
                const Optional<nsAString>& aOrigin) const;

  void
  AllowedFeatures(nsTArray<nsString>& aAllowedFeatures);

  void
  GetAllowlistForFeature(const nsAString& aFeatureName,
                         nsTArray<nsString>& aList) const;

private:
  ~FeaturePolicy() = default;

  bool
  AllowsFeatureInternal(const nsAString& aFeatureName,
                        const nsAString& aOrigin) const;

  // Inherits a single denied feature from the parent context.
  void
  SetInheritedDeniedFeature(const nsAString& aFeatureName);

  bool
  HasInheritedDeniedFeature(const nsAString& aFeatureName) const;

  bool
  HasDeclaredFeature(const nsAString& aFeatureName) const;

  nsCOMPtr<nsINode> mParentNode;

  // This is set in sub-contexts when the parent blocks some feature for the
  // current context.
  nsTArray<nsString> mInheritedDeniedFeatureNames;

  // Feature policy for the current context.
  nsTArray<Feature> mFeatures;

  nsString mDefaultOrigin;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_FeaturePolicy_h
