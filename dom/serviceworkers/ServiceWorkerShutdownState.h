/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SERVICEWORKERS_SERVICEWORKERSHUTDOWNSTATE_H_
#define DOM_SERVICEWORKERS_SERVICEWORKERSHUTDOWNSTATE_H_

#include <cstdint>

#include "ipc/EnumSerializer.h"
#include "mozilla/dom/ServiceWorkerOpArgs.h"

namespace mozilla::dom {

class ServiceWorkerShutdownState {
 public:
  // Represents the "location" of the shutdown message or completion of
  // shutdown.
  enum class Progress {
    ParentProcessMainThread,
    ParentProcessIpdlBackgroundThread,
    ContentProcessWorkerLauncherThread,
    ContentProcessMainThread,
    ShutdownCompleted,
    EndGuard_,
  };

  ServiceWorkerShutdownState();

  ~ServiceWorkerShutdownState();

  const char* GetProgressString() const;

  void SetProgress(Progress aProgress);

 private:
  Progress mProgress;
};

// Asynchronously reports that shutdown has progressed to the calling thread
// if aArgs is for shutdown. If aShutdownCompleted is true, aArgs must be for
// shutdown.
void MaybeReportServiceWorkerShutdownProgress(const ServiceWorkerOpArgs& aArgs,
                                              bool aShutdownCompleted = false);

}  // namespace mozilla::dom

namespace IPC {

using Progress = mozilla::dom::ServiceWorkerShutdownState::Progress;

template <>
struct ParamTraits<Progress>
    : public ContiguousEnumSerializer<
          Progress, Progress::ParentProcessMainThread, Progress::EndGuard_> {};

}  // namespace IPC

#endif  // DOM_SERVICEWORKERS_SERVICEWORKERSHUTDOWNSTATE_H_
