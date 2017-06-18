/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebBrowserPersistLocalDocument_h__
#define WebBrowserPersistLocalDocument_h__

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocument.h"
#include "nsIURI.h"
#include "nsIWebBrowserPersistDocument.h"

class nsIDocumentEncoder;
class nsISHEntry;

namespace mozilla {

class WebBrowserPersistLocalDocument final
    : public nsIWebBrowserPersistDocument
{
public:
    explicit WebBrowserPersistLocalDocument(nsIDocument* aDocument);

    NotNull<const Encoding*> GetCharacterSet() const;
    uint32_t GetPersistFlags() const;
    already_AddRefed<nsIURI> GetBaseURI() const;

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIWEBBROWSERPERSISTDOCUMENT

    NS_DECL_CYCLE_COLLECTION_CLASS(WebBrowserPersistLocalDocument)

private:
    nsCOMPtr<nsIDocument> mDocument;
    uint32_t mPersistFlags;

    void DecideContentType(nsACString& aContentType);
    nsresult GetDocEncoder(const nsACString& aContentType,
                           uint32_t aEncoderFlags,
                           nsIDocumentEncoder** aEncoder);
    already_AddRefed<nsISHEntry> GetHistory();

    virtual ~WebBrowserPersistLocalDocument();
};

} // namespace mozilla

#endif // WebBrowserPersistLocalDocument_h__
