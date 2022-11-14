/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CHROME_COMMON_IPC_CHANNEL_CAPABILITY_H_
#define CHROME_COMMON_IPC_CHANNEL_CAPABILITY_H_

#include "mozilla/ThreadSafety.h"
#include "mozilla/Mutex.h"
#include "mozilla/EventTargetCapability.h"
#include "nsISerialEventTarget.h"

namespace IPC {

// A thread-safety capability used in IPC channel implementations. Combines an
// EventTargetCapability and a Mutex to allow using each independently, as well
// as combined together.
//
// The ChannelCapability grants shared access if on the IOThread or if the send
// mutex is held, and only allows exclusive access if both on the IO thread and
// holding the send mutex. This is similar to a `MutexSingleWriter`, but more
// flexible due to providing access to each sub-capability.
class MOZ_CAPABILITY("channel cap") ChannelCapability {
 public:
  using Mutex = mozilla::Mutex;
  using Thread = mozilla::EventTargetCapability<nsISerialEventTarget>;

  ChannelCapability(const char* mutex_name, nsISerialEventTarget* io_thread)
      : send_mutex_(mutex_name), io_thread_(io_thread) {}

  const Thread& IOThread() const MOZ_RETURN_CAPABILITY(io_thread_) {
    return io_thread_;
  }
  Mutex& SendMutex() MOZ_RETURN_CAPABILITY(send_mutex_) { return send_mutex_; }

  // Note that we're on the IO thread, and thus have shared access to values
  // guarded by the channel capability for the thread-safety analysis.
  void NoteOnIOThread() const MOZ_REQUIRES(io_thread_)
      MOZ_ASSERT_SHARED_CAPABILITY(this) {}

  // Note that we're holding the send mutex, and thus have shared access to
  // values guarded by the channel capability for the thread-safety analysis.
  void NoteSendMutex() const MOZ_REQUIRES(send_mutex_)
      MOZ_ASSERT_SHARED_CAPABILITY(this) {}

  // Note that we're holding the send mutex while on the IO thread, and thus
  // have exclusive access to values guarded by the channel capability for the
  // thread-safety analysis.
  void NoteExclusiveAccess() const MOZ_REQUIRES(io_thread_, send_mutex_)
      MOZ_ASSERT_CAPABILITY(this) {}

 private:
  mozilla::Mutex send_mutex_;
  mozilla::EventTargetCapability<nsISerialEventTarget> io_thread_;
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_CHANNEL_CAPABILITY_H_
