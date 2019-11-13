/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGPUThreading.h"
#include "mtransport/runnable_utils.h"

namespace mozilla {
namespace webgpu {

static StaticRefPtr<WebGPUThreading> sWebGPUThread;

WebGPUThreading::WebGPUThreading(base::Thread* aThread) : mThread(aThread) {}

WebGPUThreading::~WebGPUThreading() { delete mThread; }

// static
void WebGPUThreading::Start() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sWebGPUThread);

  base::Thread* thread = new base::Thread("WebGPU");

  base::Thread::Options options;
  if (!thread->StartWithOptions(options)) {
    delete thread;
    return;
  }

  sWebGPUThread = new WebGPUThreading(thread);
  const auto fnInit = []() {};

  RefPtr<Runnable> runnable =
      NS_NewRunnableFunction("WebGPUThreading fnInit", fnInit);
  sWebGPUThread->GetLoop()->PostTask(runnable.forget());
}

// static
void WebGPUThreading::ShutDown() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sWebGPUThread);

  const auto fnExit = []() {};

  RefPtr<Runnable> runnable =
      NS_NewRunnableFunction("WebGPUThreading fnExit", fnExit);
  sWebGPUThread->GetLoop()->PostTask(runnable.forget());

  sWebGPUThread = nullptr;
}

// static
MessageLoop* WebGPUThreading::GetLoop() {
  MOZ_ASSERT(NS_IsMainThread());
  return sWebGPUThread ? sWebGPUThread->mThread->message_loop() : nullptr;
}

}  // namespace webgpu
}  // namespace mozilla
