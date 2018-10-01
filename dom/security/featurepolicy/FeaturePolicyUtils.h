/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FeaturePolicyUtils_h
#define mozilla_dom_FeaturePolicyUtils_h

#include "nsString.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {

class FeaturePolicyUtils final
{
public:
  static bool
  IsSupportedFeature(const nsAString& aFeatureName);
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_FeaturePolicyUtils_h
