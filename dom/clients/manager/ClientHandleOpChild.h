/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientHandleOpChild_h
#define _mozilla_dom_ClientHandleOpChild_h

#include "mozilla/dom/ClientOpPromise.h"
#include "mozilla/dom/PClientHandleOpChild.h"
#include "mozilla/MozPromise.h"

namespace mozilla {
namespace dom {

class ClientHandleOpChild final : public PClientHandleOpChild
{
  RefPtr<ClientOpPromise::Private> mPromise;

  // PClientHandleOpChild interface
  void
  ActorDestroy(ActorDestroyReason aReason) override;

  mozilla::ipc::IPCResult
  Recv__delete__(const ClientOpResult& aResult) override;

public:
  ClientHandleOpChild(const ClientOpConstructorArgs& aArgs,
                      ClientOpPromise::Private* aPromise);

  ~ClientHandleOpChild();
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientHandleOpChild_h
