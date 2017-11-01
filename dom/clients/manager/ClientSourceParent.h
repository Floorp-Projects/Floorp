/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientSourceParent_h
#define _mozilla_dom_ClientSourceParent_h

#include "mozilla/dom/PClientSourceParent.h"

namespace mozilla {
namespace dom {

class ClientSourceParent final : public PClientSourceParent
{
  // PClientSourceParent
  IPCResult
  RecvTeardown() override;

  void
  ActorDestroy(ActorDestroyReason aReason) override;

  PClientSourceOpParent*
  AllocPClientSourceOpParent(const ClientOpConstructorArgs& aArgs) override;

  bool
  DeallocPClientSourceOpParent(PClientSourceOpParent* aActor) override;

public:
  explicit ClientSourceParent(const ClientSourceConstructorArgs& aArgs);
  ~ClientSourceParent();
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientSourceParent_h
