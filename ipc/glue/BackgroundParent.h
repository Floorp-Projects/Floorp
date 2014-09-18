/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_backgroundparent_h__
#define mozilla_ipc_backgroundparent_h__

#include "base/process.h"
#include "mozilla/Attributes.h"
#include "mozilla/ipc/Transport.h"

template <class> struct already_AddRefed;

namespace mozilla {
namespace dom {

class ContentParent;

} // namespace dom

namespace ipc {

class PBackgroundParent;

// This class is not designed for public consumption beyond the few static
// member functions.
class BackgroundParent MOZ_FINAL
{
  friend class mozilla::dom::ContentParent;

  typedef base::ProcessId ProcessId;
  typedef mozilla::dom::ContentParent ContentParent;
  typedef mozilla::ipc::Transport Transport;

public:
  // This function allows the caller to determine if the given parent actor
  // corresponds to a child actor from another process or a child actor from a
  // different thread in the same process.
  // This function may only be called on the background thread.
  static bool
  IsOtherProcessActor(PBackgroundParent* aBackgroundActor);

  // This function returns the ContentParent associated with the parent actor if
  // the parent actor corresponds to a child actor from another process. If the
  // parent actor corresponds to a child actor from a different thread in the
  // same process then this function returns null.
  // This function may only be called on the background thread. However,
  // ContentParent is not threadsafe and the returned pointer may not be used on
  // any thread other than the main thread. Callers must take care to use (and
  // release) the returned pointer appropriately.
  static already_AddRefed<ContentParent>
  GetContentParent(PBackgroundParent* aBackgroundActor);

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
