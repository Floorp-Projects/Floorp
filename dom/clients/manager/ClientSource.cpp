/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientSource.h"

#include "ClientManager.h"
#include "ClientManagerChild.h"
#include "ClientSourceChild.h"
#include "ClientValidation.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

using mozilla::dom::workers::WorkerPrivate;
using mozilla::ipc::PrincipalInfo;

void
ClientSource::Shutdown()
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (IsShutdown()) {
    return;
  }

  ShutdownThing();

  mManager = nullptr;
}

void
ClientSource::ExecutionReady(const ClientSourceExecutionReadyArgs& aArgs)
{
  // Fast fail if we don't understand this particular principal/URL combination.
  // This can happen since we use MozURL for validation which does not handle
  // some of the more obscure internal principal/url combinations.  Normal
  // content pages will pass this check.
  if (NS_WARN_IF(!ClientIsValidCreationURL(mClientInfo.PrincipalInfo(),
                                           aArgs.url()))) {
    Shutdown();
    return;
  }

  mClientInfo.SetURL(aArgs.url());
  mClientInfo.SetFrameType(aArgs.frameType());
  MaybeExecute([aArgs](PClientSourceChild* aActor) {
    aActor->SendExecutionReady(aArgs);
  });
}

WorkerPrivate*
ClientSource::GetWorkerPrivate() const
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (!mOwner.is<WorkerPrivate*>()) {
    return nullptr;
  }
  return mOwner.as<WorkerPrivate*>();
}

nsIDocShell*
ClientSource::GetDocShell() const
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (!mOwner.is<nsCOMPtr<nsIDocShell>>()) {
    return nullptr;
  }
  return mOwner.as<nsCOMPtr<nsIDocShell>>();
}

ClientSource::ClientSource(ClientManager* aManager,
                           nsISerialEventTarget* aEventTarget,
                           const ClientSourceConstructorArgs& aArgs)
  : mManager(aManager)
  , mEventTarget(aEventTarget)
  , mOwner(AsVariant(Nothing()))
  , mClientInfo(aArgs.id(), aArgs.type(), aArgs.principalInfo(), aArgs.creationTime())
{
  MOZ_ASSERT(mManager);
  MOZ_ASSERT(mEventTarget);
}

void
ClientSource::Activate(PClientManagerChild* aActor)
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  MOZ_ASSERT(!GetActor());

  if (IsShutdown()) {
    return;
  }

  // Fast fail if we don't understand this particular kind of PrincipalInfo.
  // This can happen since we use MozURL for validation which does not handle
  // some of the more obscure internal principal/url combinations.  Normal
  // content pages will pass this check.
  if (NS_WARN_IF(!ClientIsValidPrincipalInfo(mClientInfo.PrincipalInfo()))) {
    Shutdown();
    return;
  }

  ClientSourceConstructorArgs args(mClientInfo.Id(), mClientInfo.Type(),
                                   mClientInfo.PrincipalInfo(),
                                   mClientInfo.CreationTime());
  PClientSourceChild* actor = aActor->SendPClientSourceConstructor(args);
  if (!actor) {
    Shutdown();
    return;
  }

  ActivateThing(static_cast<ClientSourceChild*>(actor));
}

ClientSource::~ClientSource()
{
  Shutdown();
}

nsPIDOMWindowInner*
ClientSource::GetInnerWindow() const
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (!mOwner.is<RefPtr<nsPIDOMWindowInner>>()) {
    return nullptr;
  }
  return mOwner.as<RefPtr<nsPIDOMWindowInner>>();
}

void
ClientSource::WorkerExecutionReady(WorkerPrivate* aWorkerPrivate)
{
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  // Its safe to store the WorkerPrivate* here because the ClientSource
  // is explicitly destroyed by WorkerPrivate before exiting its run loop.
  MOZ_DIAGNOSTIC_ASSERT(mOwner.is<Nothing>());
  mOwner = AsVariant(aWorkerPrivate);

  ClientSourceExecutionReadyArgs args(
    aWorkerPrivate->GetLocationInfo().mHref,
    FrameType::None);

  ExecutionReady(args);
}

nsresult
ClientSource::WindowExecutionReady(nsPIDOMWindowInner* aInnerWindow)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aInnerWindow);
  MOZ_DIAGNOSTIC_ASSERT(aInnerWindow->IsCurrentInnerWindow());
  MOZ_DIAGNOSTIC_ASSERT(aInnerWindow->HasActiveDocument());

  nsIDocument* doc = aInnerWindow->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_UNEXPECTED;
  }

  // Don't use nsAutoCString here since IPC requires a full nsCString anyway.
  nsCString spec;

  nsIURI* uri = doc->GetOriginalURI();
  if (uri) {
    nsresult rv = uri->GetSpec(spec);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsPIDOMWindowOuter* outer = aInnerWindow->GetOuterWindow();
  if (NS_WARN_IF(!outer)) {
    return NS_ERROR_UNEXPECTED;
  }

  FrameType frameType = FrameType::Top_level;
  if (!outer->IsTopLevelWindow()) {
    frameType = FrameType::Nested;
  } else if(outer->HadOriginalOpener()) {
    frameType = FrameType::Auxiliary;
  }

  // We should either be setting a window execution ready for the
  // first time or setting the same window execution ready again.
  // The secondary calls are due to initial about:blank replacement.
  MOZ_DIAGNOSTIC_ASSERT(mOwner.is<Nothing>() ||
                        mOwner.is<nsCOMPtr<nsIDocShell>>() ||
                        GetInnerWindow() == aInnerWindow);

  // This creates a cycle with the window.  It is broken when
  // nsGlobalWindow::FreeInnerObjects() deletes the ClientSource.
  mOwner = AsVariant(RefPtr<nsPIDOMWindowInner>(aInnerWindow));

  ClientSourceExecutionReadyArgs args(spec, frameType);
  ExecutionReady(args);

  return NS_OK;
}

nsresult
ClientSource::DocShellExecutionReady(nsIDocShell* aDocShell)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aDocShell);

  nsPIDOMWindowOuter* outer = aDocShell->GetWindow();
  if (NS_WARN_IF(!outer)) {
    return NS_ERROR_UNEXPECTED;
  }

  // TODO: dedupe this with WindowExecutionReady
  FrameType frameType = FrameType::Top_level;
  if (!outer->IsTopLevelWindow()) {
    frameType = FrameType::Nested;
  } else if(outer->HadOriginalOpener()) {
    frameType = FrameType::Auxiliary;
  }

  MOZ_DIAGNOSTIC_ASSERT(mOwner.is<Nothing>());

  // This creates a cycle with the docshell.  It is broken when
  // nsDocShell::Destroy() deletes the ClientSource.
  mOwner = AsVariant(nsCOMPtr<nsIDocShell>(aDocShell));

  ClientSourceExecutionReadyArgs args(NS_LITERAL_CSTRING("about:blank"),
                                      frameType);
  ExecutionReady(args);

  return NS_OK;
}

void
ClientSource::Freeze()
{
  MaybeExecute([](PClientSourceChild* aActor) {
    aActor->SendFreeze();
  });
}

void
ClientSource::Thaw()
{
  MaybeExecute([](PClientSourceChild* aActor) {
    aActor->SendThaw();
  });
}

const ClientInfo&
ClientSource::Info() const
{
  return mClientInfo;
}

nsISerialEventTarget*
ClientSource::EventTarget() const
{
  return mEventTarget;
}

void
ClientSource::Traverse(nsCycleCollectionTraversalCallback& aCallback,
                       const char* aName,
                       uint32_t aFlags)
{
  if (mOwner.is<RefPtr<nsPIDOMWindowInner>>()) {
    ImplCycleCollectionTraverse(aCallback,
                                mOwner.as<RefPtr<nsPIDOMWindowInner>>(),
                                aName, aFlags);
  } else if (mOwner.is<nsCOMPtr<nsIDocShell>>()) {
    ImplCycleCollectionTraverse(aCallback,
                                mOwner.as<nsCOMPtr<nsIDocShell>>(),
                                aName, aFlags);
  }
}

} // namespace dom
} // namespace mozilla
