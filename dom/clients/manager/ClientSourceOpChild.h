/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientSourceOpChild_h
#define _mozilla_dom_ClientSourceOpChild_h

#include "mozilla/dom/PClientSourceOpChild.h"
#include "ClientOpPromise.h"

namespace mozilla {
namespace dom {

class ClientSource;

class ClientSourceOpChild final : public PClientSourceOpChild
{
  MozPromiseRequestHolder<ClientOpPromise> mPromiseRequestHolder;

  ClientSource*
  GetSource() const;

  template <typename Method, typename Args>
  void
  DoSourceOp(Method aMethod, const Args& aArgs);

  // PClientSourceOpChild interface
  void
  ActorDestroy(ActorDestroyReason aReason) override;

public:
  ClientSourceOpChild() = default;
  ~ClientSourceOpChild() = default;

  void
  Init(const ClientOpConstructorArgs& aArgs);
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientSourceOpChild_h
