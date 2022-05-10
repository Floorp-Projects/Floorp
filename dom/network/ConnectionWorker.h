/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_network_ConnectionWorker_h
#define mozilla_dom_network_ConnectionWorker_h

#include "Connection.h"

namespace mozilla::dom::network {

class ConnectionProxy;

class ConnectionWorker final : public Connection {
  friend class ConnectionProxy;

 public:
  static already_AddRefed<ConnectionWorker> Create(
      WorkerPrivate* aWorkerPrivate, ErrorResult& aRv);

 private:
  explicit ConnectionWorker(bool aShouldResistFingerprinting);
  ~ConnectionWorker();

  virtual void ShutdownInternal() override;

  RefPtr<ConnectionProxy> mProxy;
};

}  // namespace mozilla::dom::network

#endif  // mozilla_dom_network_ConnectionWorker_h
