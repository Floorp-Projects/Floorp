/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebBrowserPersistDocumentParent_h__
#define WebBrowserPersistDocumentParent_h__

#include "mozilla/Maybe.h"
#include "mozilla/PWebBrowserPersistDocumentParent.h"
#include "nsCOMPtr.h"
#include "nsIWebBrowserPersistDocument.h"

// This class is the IPC half of the glue between the
// nsIWebBrowserPersistDocument interface and a remote document.  When
// (and if) it receives the Attributes message it constructs an
// WebBrowserPersistRemoteDocument and releases it into the XPCOM
// universe; otherwise, it invokes the document receiver's error
// callback.
//
// This object's lifetime is the normal IPC lifetime; on destruction,
// it calls its XPCOM reflection (if it exists yet) to remove that
// reference.  Normal deletion occurs when the XPCOM object is being
// destroyed or after an InitFailure is received and handled.
//
// See also: TabParent::StartPersistence.

namespace mozilla {

class WebBrowserPersistRemoteDocument;

class WebBrowserPersistDocumentParent final
    : public PWebBrowserPersistDocumentParent
{
public:
    WebBrowserPersistDocumentParent();
    virtual ~WebBrowserPersistDocumentParent();

    // Set a callback to be invoked when the actor leaves the START
    // state.  This method must be called exactly once while the actor
    // is still in the START state (or is unconstructed).
    void SetOnReady(nsIWebBrowserPersistDocumentReceiver* aOnReady);

    using Attrs = WebBrowserPersistDocumentAttrs;

    // IPDL methods:
    virtual bool
    RecvAttributes(const Attrs& aAttrs,
                   const OptionalInputStreamParams& aPostData,
                   nsTArray<FileDescriptor>&& aPostFiles) override;
    virtual bool
    RecvInitFailure(const nsresult& aFailure) override;

    virtual PWebBrowserPersistResourcesParent*
    AllocPWebBrowserPersistResourcesParent() override;
    virtual bool
    DeallocPWebBrowserPersistResourcesParent(PWebBrowserPersistResourcesParent* aActor) override;

    virtual PWebBrowserPersistSerializeParent*
    AllocPWebBrowserPersistSerializeParent(
            const WebBrowserPersistURIMap& aMap,
            const nsCString& aRequestedContentType,
            const uint32_t& aEncoderFlags,
            const uint32_t& aWrapColumn) override;
    virtual bool
    DeallocPWebBrowserPersistSerializeParent(PWebBrowserPersistSerializeParent* aActor) override;

    virtual void
    ActorDestroy(ActorDestroyReason aWhy) override;
private:
    // This is reset to nullptr when the callback is invoked.
    nsCOMPtr<nsIWebBrowserPersistDocumentReceiver> mOnReady;
    WebBrowserPersistRemoteDocument* mReflection;
};

} // namespace mozilla

#endif // WebBrowserPersistDocumentParent_h__
