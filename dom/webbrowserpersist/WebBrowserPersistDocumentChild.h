/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebBrowserPersistDocumentChild_h__
#define WebBrowserPersistDocumentChild_h__

#include "mozilla/PWebBrowserPersistDocumentChild.h"
#include "nsCOMPtr.h"
#include "nsIWebBrowserPersistDocument.h"

namespace mozilla {

class WebBrowserPersistDocumentChild final
    : public PWebBrowserPersistDocumentChild {
 public:
  WebBrowserPersistDocumentChild();
  ~WebBrowserPersistDocumentChild();

  // This sends either Attributes or InitFailure and thereby causes
  // the actor to leave the START state.
  void Start(nsIWebBrowserPersistDocument* aDocument);
  void Start(dom::Document* aDocument);

  mozilla::ipc::IPCResult RecvSetPersistFlags(const uint32_t& aNewFlags);

  PWebBrowserPersistResourcesChild* AllocPWebBrowserPersistResourcesChild();
  virtual mozilla::ipc::IPCResult RecvPWebBrowserPersistResourcesConstructor(
      PWebBrowserPersistResourcesChild* aActor) override;
  bool DeallocPWebBrowserPersistResourcesChild(
      PWebBrowserPersistResourcesChild* aActor);

  PWebBrowserPersistSerializeChild* AllocPWebBrowserPersistSerializeChild(
      const WebBrowserPersistURIMap& aMap,
      const nsACString& aRequestedContentType, const uint32_t& aEncoderFlags,
      const uint32_t& aWrapColumn);
  virtual mozilla::ipc::IPCResult RecvPWebBrowserPersistSerializeConstructor(
      PWebBrowserPersistSerializeChild* aActor,
      const WebBrowserPersistURIMap& aMap,
      const nsACString& aRequestedContentType, const uint32_t& aEncoderFlags,
      const uint32_t& aWrapColumn) override;
  bool DeallocPWebBrowserPersistSerializeChild(
      PWebBrowserPersistSerializeChild* aActor);

 private:
  nsCOMPtr<nsIWebBrowserPersistDocument> mDocument;
};

}  // namespace mozilla

#endif  // WebBrowserPersistDocumentChild_h__
