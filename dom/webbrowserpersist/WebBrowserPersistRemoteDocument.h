/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebBrowserPersistRemoteDocument_h__
#define WebBrowserPersistRemoteDocument_h__

#include "mozilla/Maybe.h"
#include "mozilla/PWebBrowserPersistDocumentParent.h"
#include "nsCOMPtr.h"
#include "nsIWebBrowserPersistDocument.h"
#include "nsIInputStream.h"

class nsIPrincipal;

// This class is the XPCOM half of the glue between the
// nsIWebBrowserPersistDocument interface and a remote document; it is
// created by WebBrowserPersistDocumentParent when (and if) it
// receives the information needed to populate the interface's
// properties.
//
// This object has a normal refcounted lifetime.  The corresponding
// IPC actor holds a weak reference to this class; when the last
// strong reference is released, it sends an IPC delete message and
// thereby removes that reference.

namespace mozilla {

class WebBrowserPersistDocumentParent;

class WebBrowserPersistRemoteDocument final
    : public nsIWebBrowserPersistDocument
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBBROWSERPERSISTDOCUMENT

private:
    using Attrs = WebBrowserPersistDocumentAttrs;
    WebBrowserPersistDocumentParent* mActor;
    Attrs mAttrs;
    nsCOMPtr<nsIInputStream> mPostData;
    nsCOMPtr<nsIPrincipal> mPrincipal;

    friend class WebBrowserPersistDocumentParent;
    WebBrowserPersistRemoteDocument(WebBrowserPersistDocumentParent* aActor,
                                    const Attrs& aAttrs,
                                    nsIInputStream* aPostData);
    ~WebBrowserPersistRemoteDocument();

    void ActorDestroy(void);
};

} // namespace mozilla

#endif // WebBrowserPersistRemoteDocument_h__
