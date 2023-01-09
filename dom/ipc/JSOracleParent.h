/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSOracleParent
#define mozilla_dom_JSOracleParent

#include "mozilla/MozPromise.h"
#include "mozilla/ProcInfo.h"
#include "mozilla/dom/PJSOracleParent.h"

namespace mozilla::ipc {
class UtilityProcessParent;
}

namespace mozilla::dom {

class JSOracleParent final : public PJSOracleParent {
 public:
  JSOracleParent() = default;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(JSOracleParent, override);

  static void WithJSOracle(
      const std::function<void(JSOracleParent* aParent)>& aCallback);

  UtilityActorName GetActorName() { return UtilityActorName::JSOracle; }

  void ActorDestroy(ActorDestroyReason aReason) override;

  nsresult BindToUtilityProcess(
      const RefPtr<mozilla::ipc::UtilityProcessParent>& aUtilityParent);

  void Bind(Endpoint<PJSOracleParent>&& aEndpoint);

 private:
  ~JSOracleParent() = default;

  static JSOracleParent* GetSingleton();

  using JSOraclePromise = GenericNonExclusivePromise;
  RefPtr<JSOraclePromise> StartJSOracle();
};
}  // namespace mozilla::dom

#endif  // defined(mozilla_dom_JSOracleParent)
