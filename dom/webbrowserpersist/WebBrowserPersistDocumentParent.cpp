/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebBrowserPersistDocumentParent.h"

#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/dom/PContentParent.h"
#include "nsIInputStream.h"
#include "nsThreadUtils.h"
#include "WebBrowserPersistResourcesParent.h"
#include "WebBrowserPersistSerializeParent.h"
#include "WebBrowserPersistRemoteDocument.h"

namespace mozilla {

WebBrowserPersistDocumentParent::WebBrowserPersistDocumentParent()
: mReflection(nullptr)
{
}

void
WebBrowserPersistDocumentParent::SetOnReady(nsIWebBrowserPersistDocumentReceiver* aOnReady)
{
    MOZ_ASSERT(aOnReady);
    MOZ_ASSERT(!mOnReady);
    MOZ_ASSERT(!mReflection);
    mOnReady = aOnReady;
}

void
WebBrowserPersistDocumentParent::ActorDestroy(ActorDestroyReason aWhy)
{
    if (mReflection) {
        mReflection->ActorDestroy();
        mReflection = nullptr;
    }
    if (mOnReady) {
        // Bug 1202887: If this is part of a subtree destruction, then
        // anything which could cause another actor in that subtree to
        // be Send__delete__()ed will cause use-after-free -- such as
        // dropping the last reference to another document's
        // WebBrowserPersistRemoteDocument.  To avoid that, defer the
        // callback until after the entire subtree is destroyed.
        nsCOMPtr<nsIRunnable> errorLater = NewRunnableMethod
            <nsresult>(mOnReady, &nsIWebBrowserPersistDocumentReceiver::OnError,
                       NS_ERROR_FAILURE);
        NS_DispatchToCurrentThread(errorLater);
        mOnReady = nullptr;
    }
}

WebBrowserPersistDocumentParent::~WebBrowserPersistDocumentParent()
{
    MOZ_RELEASE_ASSERT(!mReflection);
    MOZ_ASSERT(!mOnReady);
}

mozilla::ipc::IPCResult
WebBrowserPersistDocumentParent::RecvAttributes(const Attrs& aAttrs,
                                                const OptionalIPCStream& aPostStream)
{
    // Deserialize the postData unconditionally so that fds aren't leaked.
    nsCOMPtr<nsIInputStream> postData = mozilla::ipc::DeserializeIPCStream(aPostStream);
    if (!mOnReady || mReflection) {
        return IPC_FAIL_NO_REASON(this);
    }
    mReflection = new WebBrowserPersistRemoteDocument(this, aAttrs, postData);
    RefPtr<WebBrowserPersistRemoteDocument> reflection = mReflection;
    mOnReady->OnDocumentReady(reflection);
    mOnReady = nullptr;
    return IPC_OK();
}

mozilla::ipc::IPCResult
WebBrowserPersistDocumentParent::RecvInitFailure(const nsresult& aFailure)
{
    if (!mOnReady || mReflection) {
        return IPC_FAIL_NO_REASON(this);
    }
    mOnReady->OnError(aFailure);
    mOnReady = nullptr;
    // Warning: Send__delete__ deallocates this object.
    IProtocol* mgr = Manager();
    if (!Send__delete__(this)) {
        return IPC_FAIL_NO_REASON(mgr);
    }
    return IPC_OK();
}

PWebBrowserPersistResourcesParent*
WebBrowserPersistDocumentParent::AllocPWebBrowserPersistResourcesParent()
{
    MOZ_CRASH("Don't use this; construct the actor directly and AddRef.");
    return nullptr;
}

bool
WebBrowserPersistDocumentParent::DeallocPWebBrowserPersistResourcesParent(PWebBrowserPersistResourcesParent* aActor)
{
    // Turn the ref held by IPC back into an nsRefPtr.
    RefPtr<WebBrowserPersistResourcesParent> actor =
        already_AddRefed<WebBrowserPersistResourcesParent>(
            static_cast<WebBrowserPersistResourcesParent*>(aActor));
    return true;
}

PWebBrowserPersistSerializeParent*
WebBrowserPersistDocumentParent::AllocPWebBrowserPersistSerializeParent(
        const WebBrowserPersistURIMap& aMap,
        const nsCString& aRequestedContentType,
        const uint32_t& aEncoderFlags,
        const uint32_t& aWrapColumn)
{
    MOZ_CRASH("Don't use this; construct the actor directly.");
    return nullptr;
}

bool
WebBrowserPersistDocumentParent::DeallocPWebBrowserPersistSerializeParent(PWebBrowserPersistSerializeParent* aActor)
{
    delete aActor;
    return true;
}

} // namespace mozilla
