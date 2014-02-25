/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_backgroundparent_h__
#define mozilla_ipc_backgroundparent_h__

#include "base/process.h"
#include "mozilla/Attributes.h"
#include "mozilla/ipc/Transport.h"

namespace mozilla {
namespace dom {

class ContentParent;

} // namespace dom

namespace ipc {

class PBackgroundParent;

// This class is not designed for public consumption. It must only be used by
// ContentParent.
class BackgroundParent MOZ_FINAL
{
  friend class mozilla::dom::ContentParent;

  typedef base::ProcessId ProcessId;
  typedef mozilla::dom::ContentParent ContentParent;
  typedef mozilla::ipc::Transport Transport;

private:
  // Only called by ContentParent for cross-process actors.
  static PBackgroundParent*
  Alloc(ContentParent* aContent,
        Transport* aTransport,
        ProcessId aOtherProcess);
};

// Implemented in BackgroundImpl.cpp.
bool
IsOnBackgroundThread();

#ifdef DEBUG

// Implemented in BackgroundImpl.cpp.
void
AssertIsOnBackgroundThread();

#else

inline void
AssertIsOnBackgroundThread()
{ }

#endif // DEBUG

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_backgroundparent_h__
