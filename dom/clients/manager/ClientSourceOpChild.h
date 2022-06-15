/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientSourceOpChild_h
#define _mozilla_dom_ClientSourceOpChild_h

#include "mozilla/dom/FlippedOnce.h"
#include "mozilla/dom/PClientSourceOpChild.h"
#include "ClientOpPromise.h"

namespace mozilla::dom {

class ClientSource;

class ClientSourceOpChild final : public PClientSourceOpChild {
 public:
  void Init(const ClientOpConstructorArgs& aArgs);

  // Deletes "this" after initialization (or immediately if already
  // initialized.) It's UB to use "this" after calling ScheduleDeletion.
  void ScheduleDeletion();

 private:
  ~ClientSourceOpChild();

  void Cleanup();

  ClientSource* GetSource() const;

  template <typename Method, typename... Args>
  void DoSourceOp(Method aMethod, Args&&... aArgs);

  // PClientSourceOpChild interface
  void ActorDestroy(ActorDestroyReason aReason) override;

  MozPromiseRequestHolder<ClientOpPromise> mPromiseRequestHolder;
  FlippedOnce<false> mDeletionRequested;
  FlippedOnce<false> mInitialized;
};

}  // namespace mozilla::dom

#endif  // _mozilla_dom_ClientSourceOpChild_h
