/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentBridgeChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/ipc/InputStreamUtils.h"

using namespace mozilla::ipc;
using namespace mozilla::jsipc;

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(ContentBridgeChild,
                  nsIContentChild)

ContentBridgeChild::ContentBridgeChild(Transport* aTransport)
  : mTransport(aTransport)
{}

ContentBridgeChild::~ContentBridgeChild()
{
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new DeleteTask<Transport>(mTransport));
}

void
ContentBridgeChild::ActorDestroy(ActorDestroyReason aWhy)
{
  MessageLoop::current()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this, &ContentBridgeChild::DeferredDestroy));
}

/*static*/ ContentBridgeChild*
ContentBridgeChild::Create(Transport* aTransport, ProcessId aOtherPid)
{
  nsRefPtr<ContentBridgeChild> bridge =
    new ContentBridgeChild(aTransport);
  bridge->mSelfRef = bridge;

  DebugOnly<bool> ok = bridge->Open(aTransport, aOtherPid, XRE_GetIOMessageLoop());
  MOZ_ASSERT(ok);

  return bridge;
}

void
ContentBridgeChild::DeferredDestroy()
{
  mSelfRef = nullptr;
  // |this| was just destroyed, hands off
}

bool
ContentBridgeChild::RecvAsyncMessage(const nsString& aMsg,
                                     const ClonedMessageData& aData,
                                     InfallibleTArray<jsipc::CpowEntry>&& aCpows,
                                     const IPC::Principal& aPrincipal)
{
  return nsIContentChild::RecvAsyncMessage(aMsg, aData, Move(aCpows), aPrincipal);
}

PBlobChild*
ContentBridgeChild::SendPBlobConstructor(PBlobChild* actor,
                                         const BlobConstructorParams& params)
{
  return PContentBridgeChild::SendPBlobConstructor(actor, params);
}

bool
ContentBridgeChild::SendPBrowserConstructor(PBrowserChild* aActor,
                                            const TabId& aTabId,
                                            const IPCTabContext& aContext,
                                            const uint32_t& aChromeFlags,
                                            const ContentParentId& aCpID,
                                            const bool& aIsForApp,
                                            const bool& aIsForBrowser)
{
  return PContentBridgeChild::SendPBrowserConstructor(aActor,
                                                      aTabId,
                                                      aContext,
                                                      aChromeFlags,
                                                      aCpID,
                                                      aIsForApp,
                                                      aIsForBrowser);
}

// This implementation is identical to ContentChild::GetCPOWManager but we can't
// move it to nsIContentChild because it calls ManagedPJavaScriptChild() which
// only exists in PContentChild and PContentBridgeChild.
jsipc::CPOWManager*
ContentBridgeChild::GetCPOWManager()
{
  if (ManagedPJavaScriptChild().Length()) {
    return CPOWManagerFor(ManagedPJavaScriptChild()[0]);
  }
  return CPOWManagerFor(SendPJavaScriptConstructor());
}

mozilla::jsipc::PJavaScriptChild *
ContentBridgeChild::AllocPJavaScriptChild()
{
  return nsIContentChild::AllocPJavaScriptChild();
}

bool
ContentBridgeChild::DeallocPJavaScriptChild(PJavaScriptChild *child)
{
  return nsIContentChild::DeallocPJavaScriptChild(child);
}

PBrowserChild*
ContentBridgeChild::AllocPBrowserChild(const TabId& aTabId,
                                       const IPCTabContext &aContext,
                                       const uint32_t& aChromeFlags,
                                       const ContentParentId& aCpID,
                                       const bool& aIsForApp,
                                       const bool& aIsForBrowser)
{
  return nsIContentChild::AllocPBrowserChild(aTabId,
                                             aContext,
                                             aChromeFlags,
                                             aCpID,
                                             aIsForApp,
                                             aIsForBrowser);
}

bool
ContentBridgeChild::DeallocPBrowserChild(PBrowserChild* aChild)
{
  return nsIContentChild::DeallocPBrowserChild(aChild);
}

bool
ContentBridgeChild::RecvPBrowserConstructor(PBrowserChild* aActor,
                                            const TabId& aTabId,
                                            const IPCTabContext& aContext,
                                            const uint32_t& aChromeFlags,
                                            const ContentParentId& aCpID,
                                            const bool& aIsForApp,
                                            const bool& aIsForBrowser)
{
  return ContentChild::GetSingleton()->RecvPBrowserConstructor(aActor,
                                                               aTabId,
                                                               aContext,
                                                               aChromeFlags,
                                                               aCpID,
                                                               aIsForApp,
                                                               aIsForBrowser);
}

PBlobChild*
ContentBridgeChild::AllocPBlobChild(const BlobConstructorParams& aParams)
{
  return nsIContentChild::AllocPBlobChild(aParams);
}

bool
ContentBridgeChild::DeallocPBlobChild(PBlobChild* aActor)
{
  return nsIContentChild::DeallocPBlobChild(aActor);
}

} // namespace dom
} // namespace mozilla
