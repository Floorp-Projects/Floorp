/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebBrowserPersistResourcesParent_h__
#define WebBrowserPersistResourcesParent_h__

#include "mozilla/PWebBrowserPersistResourcesParent.h"

#include "WebBrowserPersistDocumentParent.h"
#include "nsCOMPtr.h"
#include "nsIWebBrowserPersistDocument.h"

namespace mozilla {

class WebBrowserPersistResourcesParent final
    : public PWebBrowserPersistResourcesParent,
      public nsIWebBrowserPersistDocumentReceiver {
 public:
  WebBrowserPersistResourcesParent(
      nsIWebBrowserPersistDocument* aDocument,
      nsIWebBrowserPersistResourceVisitor* aVisitor);

  virtual mozilla::ipc::IPCResult RecvVisitResource(
      const nsACString& aURI,
      const nsContentPolicyType& aContentPolicyType) override;

  virtual mozilla::ipc::IPCResult RecvVisitDocument(
      PWebBrowserPersistDocumentParent* aSubDocument) override;

  virtual mozilla::ipc::IPCResult RecvVisitBrowsingContext(
      const dom::MaybeDiscarded<dom::BrowsingContext>& aContext) override;

  virtual mozilla::ipc::IPCResult Recv__delete__(
      const nsresult& aStatus) override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  NS_DECL_NSIWEBBROWSERPERSISTDOCUMENTRECEIVER
  NS_DECL_ISUPPORTS

 private:
  // Note: even if the XPIDL didn't need mDocument for visitor
  // callbacks, this object still needs to hold a strong reference
  // to it to defer actor subtree deletion until after the
  // visitation is finished.
  nsCOMPtr<nsIWebBrowserPersistDocument> mDocument;
  nsCOMPtr<nsIWebBrowserPersistResourceVisitor> mVisitor;

  virtual ~WebBrowserPersistResourcesParent();
};

}  // namespace mozilla

#endif  // WebBrowserPersistResourcesParent_h__
