/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/ipc/NodeController.h"

#if defined(OS_WIN)
#  include <objbase.h>
#endif

namespace mozilla {
namespace ipc {

//
// BrowserProcessSubThread
//

// Friendly names for the well-known threads.
static const char* kBrowserThreadNames[BrowserProcessSubThread::ID_COUNT] = {
    "IPC I/O Parent",  // IO
};

/* static */
StaticMutex BrowserProcessSubThread::sLock;
BrowserProcessSubThread* BrowserProcessSubThread::sBrowserThreads[ID_COUNT] = {
    nullptr,  // IO
};

BrowserProcessSubThread::BrowserProcessSubThread(ID aId)
    : base::Thread(kBrowserThreadNames[aId]), mIdentifier(aId) {
  StaticMutexAutoLock lock(sLock);
  DCHECK(aId >= 0 && aId < ID_COUNT);
  DCHECK(sBrowserThreads[aId] == nullptr);
  sBrowserThreads[aId] = this;
}

BrowserProcessSubThread::~BrowserProcessSubThread() {
  Stop();
  {
    StaticMutexAutoLock lock(sLock);
    sBrowserThreads[mIdentifier] = nullptr;
  }
}

void BrowserProcessSubThread::Init() {
#if defined(OS_WIN)
  // Initializes the COM library on the current thread.
  CoInitialize(nullptr);
#endif

  // Initialize the ports library in the current thread.
  if (mIdentifier == IO) {
    NodeController::InitBrokerProcess();
  }
}

void BrowserProcessSubThread::CleanUp() {
  if (mIdentifier == IO) {
    NodeController::CleanUp();
  }

#if defined(OS_WIN)
  // Closes the COM library on the current thread. CoInitialize must
  // be balanced by a corresponding call to CoUninitialize.
  CoUninitialize();
#endif
}

// static
MessageLoop* BrowserProcessSubThread::GetMessageLoop(ID aId) {
  StaticMutexAutoLock lock(sLock);
  DCHECK(aId >= 0 && aId < ID_COUNT);

  if (sBrowserThreads[aId]) return sBrowserThreads[aId]->message_loop();

  return nullptr;
}

}  // namespace ipc
}  // namespace mozilla
