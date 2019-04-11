/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XULDocument_h
#define mozilla_dom_XULDocument_h

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsXULPrototypeDocument.h"
#include "nsTArray.h"

#include "mozilla/dom/XMLDocument.h"
#include "mozilla/StyleSheet.h"
#include "nsIContent.h"
#include "nsCOMArray.h"
#include "nsIURI.h"
#include "nsIStreamListener.h"
#include "nsIStreamLoader.h"
#include "nsICSSLoaderObserver.h"

#include "mozilla/Attributes.h"
#include "mozilla/dom/ScriptLoader.h"

#include "js/TracingAPI.h"
#include "js/TypeDecls.h"

class nsPIWindowRoot;
class nsXULPrototypeElement;
#if 0  // XXXbe save me, scc (need NSCAP_FORWARD_DECL(nsXULPrototypeScript))
class nsIObjectInputStream;
class nsIObjectOutputStream;
#else
#  include "nsIObjectInputStream.h"
#  include "nsIObjectOutputStream.h"
#  include "nsXULElement.h"
#endif
#include "nsURIHashKey.h"
#include "nsInterfaceHashtable.h"

/**
 * The XUL document class
 */

// Factory function.
nsresult NS_NewXULDocument(mozilla::dom::Document** result);

namespace mozilla {
namespace dom {

class XULDocument final : public XMLDocument {
 public:
  XULDocument();

  // nsISupports interface
  NS_DECL_ISUPPORTS_INHERITED

  // Document interface
  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) override;
  virtual void ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup,
                          nsIPrincipal* aPrincipal,
                          nsIPrincipal* aStoragePrincipal) override;

  virtual nsresult StartDocumentLoad(const char* aCommand, nsIChannel* channel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener** aDocListener,
                                     bool aReset = true,
                                     nsIContentSink* aSink = nullptr) override;
  virtual void EndLoad() override;

  virtual void SetContentType(const nsAString& aContentType) override;

  // nsINode interface overrides
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULDocument, XMLDocument)

 protected:
  virtual ~XULDocument();

  // Implementation methods
  friend nsresult(::NS_NewXULDocument(Document** aResult));

  nsresult Init(void) override;

  // pseudo constants
  static int32_t gRefCnt;

  static LazyLogModule gXULLog;

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 private:
  // helpers
};

inline XULDocument* Document::AsXULDocument() {
  MOZ_ASSERT(IsXULDocument());
  return static_cast<mozilla::dom::XULDocument*>(this);
}

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XULDocument_h
