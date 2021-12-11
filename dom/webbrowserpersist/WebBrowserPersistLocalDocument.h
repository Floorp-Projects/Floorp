/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebBrowserPersistLocalDocument_h__
#define WebBrowserPersistLocalDocument_h__

#include "mozilla/NotNull.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIURI.h"
#include "nsIWebBrowserPersistDocument.h"

class nsIDocumentEncoder;
class nsISHEntry;

namespace mozilla {

namespace dom {
class Document;
}

class WebBrowserPersistLocalDocument final
    : public nsIWebBrowserPersistDocument {
 public:
  explicit WebBrowserPersistLocalDocument(dom::Document* aDocument);

  NotNull<const Encoding*> GetCharacterSet() const;
  uint32_t GetPersistFlags() const;
  nsIURI* GetBaseURI() const;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIWEBBROWSERPERSISTDOCUMENT

  NS_DECL_CYCLE_COLLECTION_CLASS(WebBrowserPersistLocalDocument)

 private:
  RefPtr<dom::Document> mDocument;
  uint32_t mPersistFlags;

  void DecideContentType(nsACString& aContentType);
  nsresult GetDocEncoder(const nsACString& aContentType, uint32_t aEncoderFlags,
                         nsIDocumentEncoder** aEncoder);

  virtual ~WebBrowserPersistLocalDocument();
};

}  // namespace mozilla

#endif  // WebBrowserPersistLocalDocument_h__
