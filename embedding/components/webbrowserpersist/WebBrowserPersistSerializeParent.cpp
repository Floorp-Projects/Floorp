/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebBrowserPersistSerializeParent.h"

#include "nsReadableUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {

WebBrowserPersistSerializeParent::WebBrowserPersistSerializeParent(
        nsIWebBrowserPersistDocument* aDocument,
        nsIOutputStream* aStream,
        nsIWebBrowserPersistWriteCompletion* aFinish)
: mDocument(aDocument)
, mStream(aStream)
, mFinish(aFinish)
, mOutputError(NS_OK)
{
    MOZ_ASSERT(aDocument);
    MOZ_ASSERT(aStream);
    MOZ_ASSERT(aFinish);
}

WebBrowserPersistSerializeParent::~WebBrowserPersistSerializeParent()
{
}

bool
WebBrowserPersistSerializeParent::RecvWriteData(nsTArray<uint8_t>&& aData)
{
    if (NS_FAILED(mOutputError)) {
        return true;
    }

    uint32_t written = 0;
    static_assert(sizeof(char) == sizeof(uint8_t),
                  "char must be (at least?) 8 bits");
    const char* data = reinterpret_cast<const char*>(aData.Elements());
    // nsIOutputStream::Write is allowed to return short writes.
    while (written < aData.Length()) {
        uint32_t writeReturn;
        nsresult rv = mStream->Write(data + written,
                                     aData.Length() - written,
                                     &writeReturn);
        if (NS_FAILED(rv)) {
            mOutputError = rv;
            return true;
        }
        written += writeReturn;
    }
    return true;
}

bool
WebBrowserPersistSerializeParent::Recv__delete__(const nsCString& aContentType,
                                                 const nsresult& aStatus)
{
    if (NS_SUCCEEDED(mOutputError)) {
        mOutputError = aStatus;
    }
    mFinish->OnFinish(mDocument,
                      mStream,
                      aContentType,
                      mOutputError);
    mFinish = nullptr;
    return true;
}

void
WebBrowserPersistSerializeParent::ActorDestroy(ActorDestroyReason aWhy)
{
    if (mFinish) {
        MOZ_ASSERT(aWhy != Deletion);
        // See comment in WebBrowserPersistDocumentParent::ActorDestroy
        // (or bug 1202887) for why this is deferred.
        nsCOMPtr<nsIRunnable> errorLater = NS_NewRunnableMethodWithArgs
            <nsCOMPtr<nsIWebBrowserPersistDocument>, nsCOMPtr<nsIOutputStream>,
             nsCString, nsresult>
            (mFinish, &nsIWebBrowserPersistWriteCompletion::OnFinish,
             mDocument, mStream, EmptyCString(), NS_ERROR_FAILURE);
        NS_DispatchToCurrentThread(errorLater);
        mFinish = nullptr;
    }
}

} // namespace mozilla
