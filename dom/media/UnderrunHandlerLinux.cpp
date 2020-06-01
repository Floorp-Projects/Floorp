/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <csignal>
#include <cerrno>
#include <pthread.h>

#include <mozilla/Sprintf.h>
#include <mozilla/Atomics.h>
#include "audio_thread_priority.h"
#include "nsDebug.h"

namespace mozilla {

Atomic<bool, MemoryOrdering::ReleaseAcquire> gRealtimeLimitReached;

void UnderrunHandler(int signum) { gRealtimeLimitReached = true; }

bool SoftRealTimeLimitReached() { return gRealtimeLimitReached; }

void InstallSoftRealTimeLimitHandler() {
  struct sigaction action, previous;

  action.sa_handler = UnderrunHandler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  int rv = sigaction(SIGXCPU, &action, &previous);
  if (rv != 0) {
    char buf[256];
    SprintfLiteral(buf, "sigaction(SIGXCPU, ...): %s", strerror(errno));
    NS_WARNING(buf);
    return;
  }

  void* previous_handler = previous.sa_flags == SA_SIGINFO
                               ? reinterpret_cast<void*>(previous.sa_sigaction)
                               : reinterpret_cast<void*>(previous.sa_handler);

  MOZ_ASSERT(previous_handler != UnderrunHandler,
             "Only install the SIGXCPU handler once per process.");

  if (previous_handler != SIG_DFL && previous_handler != UnderrunHandler) {
    NS_WARNING(
        "SIGXCPU handler was already set by something else, dropping real-time "
        "priority.");
    rv = sigaction(SIGXCPU, &previous, nullptr);
    if (rv != 0) {
      NS_WARNING("Could not restore previous handler for SIGXCPU.");
    }
    gRealtimeLimitReached = true;
    return;
  }

  gRealtimeLimitReached = false;
}

void DemoteThreadFromRealTime() {
  atp_thread_info* info = atp_get_current_thread_info();
  if (!info) {
    NS_WARNING("Could not get current thread info when demoting thread.");
    return;
  }
  int rv = atp_demote_thread_from_real_time(info);
  if (rv) {
    NS_WARNING("Could not demote thread from real-time.");
    return;
  }
  rv = atp_free_thread_info(info);
  if (rv) {
    NS_WARNING("Could not free atp_thread_info struct");
  }
  gRealtimeLimitReached = false;
}

}  // namespace mozilla
