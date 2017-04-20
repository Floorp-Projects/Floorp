/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_IPCStreamDestination_h
#define mozilla_ipc_IPCStreamDestination_h

#include "mozilla/AlreadyAddRefed.h"

class nsIInputStream;
class nsIAsyncInputStream;
class nsIAsyncOutputStream;

namespace mozilla {
namespace ipc {

class PChildToParentStreamParent;
class PParentToChildStreamChild;

// On the destination side, you must simply call TakeReader() upon receiving a
// reference to the IPCStream{Child,Parent} actor.  You do not need to maintain
// a reference to the actor itself.
class IPCStreamDestination
{
public:
  static IPCStreamDestination*
  Cast(PChildToParentStreamParent* aActor);

  static IPCStreamDestination*
  Cast(PParentToChildStreamChild* aActor);

  virtual already_AddRefed<nsIInputStream>
  TakeReader();

protected:
  IPCStreamDestination();
  virtual ~IPCStreamDestination();

  nsresult Initialize();

  // The implementation of the actor should call these methods.

  void
  ActorDestroyed();

  void
  BufferReceived(const nsCString& aBuffer);

  void
  CloseReceived(nsresult aRv);

  // These methods will be implemented by the actor.

  virtual void
  RequestClose(nsresult aRv) = 0;

  virtual void
  TerminateDestination() = 0;

  nsCOMPtr<nsIAsyncInputStream> mReader;
  nsCOMPtr<nsIAsyncOutputStream> mWriter;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_IPCStreamDestination_h
