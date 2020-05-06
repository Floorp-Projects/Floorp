/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Feature_h
#define mozilla_dom_Feature_h

#include "nsString.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"

class nsIPrincipal;

namespace mozilla {
namespace dom {

class Feature final {
 public:
  explicit Feature(const nsAString& aFeatureName);

  ~Feature();

  const nsAString& Name() const;

  void SetAllowsNone();

  bool AllowsNone() const;

  void SetAllowsAll();

  bool AllowsAll() const;

  void AppendToAllowList(nsIPrincipal* aPrincipal);

  void GetAllowList(nsTArray<nsCOMPtr<nsIPrincipal>>& aList) const;

  bool AllowListContains(nsIPrincipal* aPrincipal) const;

  bool HasAllowList() const;

  bool Allows(nsIPrincipal* aPrincipal) const;

 private:
  nsString mFeatureName;

  enum Policy {
    // denotes a policy of "feature 'none'"
    eNone,

    // denotes a policy of "feature *"
    eAll,

    // denotes a policy of "feature bar.com foo.com"
    eAllowList,
  };

  Policy mPolicy;

  CopyableTArray<nsCOMPtr<nsIPrincipal>> mAllowList;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_Feature_h
