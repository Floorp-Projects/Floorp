/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SharedWorkerManager_h
#define mozilla_dom_SharedWorkerManager_h

#include "mozilla/dom/SharedWorkerTypes.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

class nsIConsoleReportCollector;
class nsIPrincipal;

namespace mozilla {
namespace dom {

class ErrorValue;
class SharedWorkerLoadInfo;
class SharedWorkerParent;
class WorkerErrorReport;
class WorkerPrivate;

class SharedWorkerManager final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedWorkerManager);

  // Called on main-thread thread methods

  SharedWorkerManager(nsIEventTarget* aPBackgroundEventTarget,
                      const SharedWorkerLoadInfo& aInfo,
                      nsIPrincipal* aPrincipal,
                      nsIPrincipal* aLoadingPrincipal);

  nsresult
  CreateWorkerOnMainThread();

  nsresult
  ConnectPortOnMainThread(const MessagePortIdentifier& aPortIdentifier);

  bool
  MatchOnMainThread(const nsACString& aDomain,
                    const nsACString& aScriptURL,
                    const nsAString& aName,
                    nsIPrincipal* aLoadingPrincipal) const;

  void
  CloseOnMainThread();

  void
  FreezeOnMainThread();

  void
  ThawOnMainThread();

  void
  SuspendOnMainThread();

  void
  ResumeOnMainThread();

  void
  BroadcastErrorToActorsOnMainThread(const WorkerErrorReport* aReport,
                                     bool aIsErrorEvent);

  void
  CloseActorsOnMainThread();

  void
  FlushReportsToActorsOnMainThread(nsIConsoleReportCollector* aReporter);

  // Called on PBackground thread methods

  void
  AddActor(SharedWorkerParent* aParent);

  void
  RemoveActor(SharedWorkerParent* aParent);

  void
  UpdateSuspend();

  void
  UpdateFrozen();

  bool
  IsSecureContext() const;

  void
  CloseActors();

  void
  BroadcastErrorToActors(const ErrorValue& aValue);

private:
  ~SharedWorkerManager();

  nsCOMPtr<nsIEventTarget> mPBackgroundEventTarget;

  SharedWorkerLoadInfo mInfo;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIPrincipal> mLoadingPrincipal;

  // Raw pointers because SharedWorkerParent unregisters itself in ActorDestroy().
  nsTArray<SharedWorkerParent*> mActors;

  // With this patch, SharedWorker are executed on the parent process.
  // This is going to change in the following parts.
  RefPtr<WorkerPrivate> mWorkerPrivate;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_SharedWorkerManager_h
