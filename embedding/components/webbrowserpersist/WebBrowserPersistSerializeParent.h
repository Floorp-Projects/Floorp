/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebBrowserPersistSerializeParent_h__
#define WebBrowserPersistSerializeParent_h__

#include "mozilla/PWebBrowserPersistSerializeParent.h"

#include "nsCOMPtr.h"
#include "nsIOutputStream.h"
#include "nsIWebBrowserPersistDocument.h"

namespace mozilla {

class WebBrowserPersistSerializeParent
    : public PWebBrowserPersistSerializeParent
{
public:
    WebBrowserPersistSerializeParent(
        nsIWebBrowserPersistDocument* aDocument,
        nsIOutputStream* aStream,
        nsIWebBrowserPersistWriteCompletion* aFinish);
    virtual ~WebBrowserPersistSerializeParent();

    virtual mozilla::ipc::IPCResult
    RecvWriteData(nsTArray<uint8_t>&& aData) override;

    virtual mozilla::ipc::IPCResult
    Recv__delete__(const nsCString& aContentType,
                   const nsresult& aStatus) override;

    virtual void
    ActorDestroy(ActorDestroyReason aWhy) override;

private:
    // See also ...ReadParent::mDocument for the other reason this
    // strong reference needs to be here.
    nsCOMPtr<nsIWebBrowserPersistDocument> mDocument;
    nsCOMPtr<nsIOutputStream> mStream;
    nsCOMPtr<nsIWebBrowserPersistWriteCompletion> mFinish;
    nsresult mOutputError;
};

} // namespace mozilla

#endif // WebBrowserPersistSerializeParent_h__
