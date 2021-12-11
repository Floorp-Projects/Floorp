/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RemoteLazyInputStreamThread_h
#define mozilla_RemoteLazyInputStreamThread_h

#include "mozilla/RemoteLazyInputStreamChild.h"
#include "nsIEventTarget.h"
#include "nsIObserver.h"
#include "nsTArray.h"

class nsIThread;

namespace mozilla {

class RemoteLazyInputStreamChild;

class RemoteLazyInputStreamThread final : public nsIObserver,
                                          public nsIEventTarget {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIEVENTTARGET

  static bool IsOnFileEventTarget(nsIEventTarget* aEventTarget);

  static RemoteLazyInputStreamThread* Get();

  static RemoteLazyInputStreamThread* GetOrCreate();

  void MigrateActor(RemoteLazyInputStreamChild* aActor);

  bool Initialize();

  void InitializeOnMainThread();

 private:
  ~RemoteLazyInputStreamThread() = default;

  void MigrateActorInternal(RemoteLazyInputStreamChild* aActor);

  nsCOMPtr<nsIThread> mThread;

  // This is populated if MigrateActor() is called before the initialization of
  // the thread.
  nsTArray<RefPtr<RemoteLazyInputStreamChild>> mPendingActors;
};

bool IsOnDOMFileThread();

void AssertIsOnDOMFileThread();

}  // namespace mozilla

#endif  // mozilla_RemoteLazyInputStreamThread_h
