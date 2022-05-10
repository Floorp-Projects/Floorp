/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FeaturePolicyParser_h
#define mozilla_dom_FeaturePolicyParser_h

#include "nsString.h"

class nsIPrincipal;

namespace mozilla::dom {

class Document;
class Feature;

class FeaturePolicyParser final {
 public:
  // aSelfOrigin must not be null. if aSrcOrigin is null, the parsing will not
  // support 'src' as valid allow directive value.
  static bool ParseString(const nsAString& aPolicy, Document* aDocument,
                          nsIPrincipal* aSelfOrigin, nsIPrincipal* aSrcOrigin,
                          nsTArray<Feature>& aParsedFeatures);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_FeaturePolicyParser_h
