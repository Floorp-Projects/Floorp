/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSOracleChild
#define mozilla_dom_JSOracleChild

#include "mozilla/dom/PJSOracleChild.h"

namespace mozilla::ipc {
class UtilityProcessParent;
}
namespace mozilla::dom {

class PJSValidatorChild;

class JSOracleChild final : public PJSOracleChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(JSOracleChild, override);

  already_AddRefed<PJSValidatorChild> AllocPJSValidatorChild();

  void Start(Endpoint<PJSOracleChild>&& aEndpoint);

 private:
  ~JSOracleChild() = default;

  static JSOracleChild* GetSingleton();
};
}  // namespace mozilla::dom

#endif  // defined(mozilla_dom_JSOracleChild)
