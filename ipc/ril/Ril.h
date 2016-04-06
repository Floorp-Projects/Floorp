/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_Ril_h
#define mozilla_ipc_Ril_h 1

#include "nsError.h"
#include "nsTArray.h"

namespace mozilla {

namespace dom {
namespace workers {

class WorkerCrossThreadDispatcher;

} // namespace workers
} // namespace dom

namespace ipc {

class RilConsumer;

class RilWorker final
{
public:
  static nsresult Register(
    unsigned int aClientId,
    mozilla::dom::workers::WorkerCrossThreadDispatcher* aDispatcher);

  static void Shutdown();

  // Public for |MakeUnique| Call |Register| instead.
  RilWorker(mozilla::dom::workers::WorkerCrossThreadDispatcher* aDispatcher);

private:
  class RegisterConsumerTask;
  class UnregisterConsumerTask;

  nsresult RegisterConsumer(unsigned int aClientId);
  void     UnregisterConsumer(unsigned int aClientId);

  static nsTArray<UniquePtr<RilWorker>> sRilWorkers;

  RefPtr<mozilla::dom::workers::WorkerCrossThreadDispatcher> mDispatcher;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_Ril_h
