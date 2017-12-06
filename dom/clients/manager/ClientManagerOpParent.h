/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientManagerOpParent_h
#define _mozilla_dom_ClientManagerOpParent_h

#include "mozilla/dom/PClientManagerOpParent.h"
#include "ClientOpPromise.h"

namespace mozilla {
namespace dom {

class ClientManagerService;

class ClientManagerOpParent final : public PClientManagerOpParent
{
  RefPtr<ClientManagerService> mService;
  MozPromiseRequestHolder<ClientOpPromise> mPromiseRequestHolder;

  template <typename Method, typename... Args>
  void
  DoServiceOp(Method aMethod, Args&&... aArgs);

  // PClientManagerOpParent interface
  void
  ActorDestroy(ActorDestroyReason aReason) override;

public:
  explicit ClientManagerOpParent(ClientManagerService* aService);
  ~ClientManagerOpParent() = default;

  void
  Init(const ClientOpConstructorArgs& aArgs);
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientManagerOpParent_h
