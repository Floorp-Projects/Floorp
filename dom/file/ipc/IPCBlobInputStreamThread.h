/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IPCBlobInputStreamThread_h
#define mozilla_dom_IPCBlobInputStreamThread_h

#include "nsIObserverService.h"

class nsIThread;

namespace mozilla {
namespace dom {

class IPCBlobInputStreamChild;

class IPCBlobInputStreamThread final : public nsIObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static bool
  IsOnFileThread(nsIThread* aThread);

  static IPCBlobInputStreamThread*
  GetOrCreate();

  void
  MigrateActor(IPCBlobInputStreamChild* aActor);

  void
  Initialize();

private:
  ~IPCBlobInputStreamThread() = default;

  void
  MigrateActorInternal(IPCBlobInputStreamChild* aActor);

  nsCOMPtr<nsIThread> mThread;

  // This is populated if MigrateActor() is called before the initialization of
  // the thread.
  nsTArray<RefPtr<IPCBlobInputStreamChild>> mPendingActors;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_IPCBlobInputStreamThread_h
