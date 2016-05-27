/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_nsiipcbackgroundchildcreatecallback_h
#define mozilla_ipc_nsiipcbackgroundchildcreatecallback_h

#include "mozilla/Attributes.h"
#include "nsISupports.h"

namespace mozilla {
namespace ipc {

class PBackgroundChild;

} // namespace ipc
} // namespace mozilla

#define NS_IIPCBACKGROUNDCHILDCREATECALLBACK_IID                               \
  {0x4de01707, 0x70e3, 0x4181, {0xbc, 0x9f, 0xa3, 0xec, 0xfe, 0x74, 0x1a, 0xe3}}

class NS_NO_VTABLE nsIIPCBackgroundChildCreateCallback : public nsISupports
{
public:
  typedef mozilla::ipc::PBackgroundChild PBackgroundChild;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IIPCBACKGROUNDCHILDCREATECALLBACK_IID)

  // This will be called upon successful creation of a PBackgroundChild actor.
  // The actor is unique per-thread and must not be shared across threads. It
  // may be saved and reused on the same thread for as long as the thread lives.
  // After this callback BackgroundChild::GetForCurrentThread() will return the
  // same actor.
  virtual void
  ActorCreated(PBackgroundChild*) = 0;

  // This will be called if for some reason the PBackgroundChild actor cannot be
  // created. This should never be called in child processes as the failure to
  // create the actor should result in the termination of the child process
  // first. This may be called for cross-thread actors in the main process.
  virtual void
  ActorFailed() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIIPCBackgroundChildCreateCallback,
                              NS_IIPCBACKGROUNDCHILDCREATECALLBACK_IID)

#define NS_DECL_NSIIPCBACKGROUNDCHILDCREATECALLBACK                            \
  virtual void                                                                 \
  ActorCreated(mozilla::ipc::PBackgroundChild*) override;                      \
  virtual void                                                                 \
  ActorFailed() override;

#define NS_FORWARD_NSIIPCBACKGROUNDCHILDCREATECALLBACK(_to)                    \
  virtual void                                                                 \
  ActorCreated(mozilla::ipc::PBackgroundChild* aActor) override                \
  { _to ActorCreated(aActor); }                                                \
  virtual void                                                                 \
  ActorFailed() override                                                       \
  { _to ActorFailed(); }

#define NS_FORWARD_SAFE_NSIIPCBACKGROUNDCHILDCREATECALLBACK(_to)               \
  virtual void                                                                 \
  ActorCreated(mozilla::ipc::PBackgroundChild* aActor) override                \
  { if (_to) { _to->ActorCreated(aActor); } }                                  \
  virtual void                                                                 \
  ActorFailed() override                                                       \
  { if (_to) { _to->ActorFailed(); } }

#endif // mozilla_ipc_nsiipcbackgroundchildcreatecallback_h
