/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIDocumentTransformer_h__
#define nsIDocumentTransformer_h__

#include "nsISupports.h"
#include "nsStringFwd.h"

template <class>
class nsCOMPtr;
class nsIContent;
class nsINode;
class nsIURI;
template <class>
class nsTArray;

namespace mozilla {
namespace dom {
class Document;
}
}  // namespace mozilla

#define NS_ITRANSFORMOBSERVER_IID                    \
  {                                                  \
    0x04b2d17c, 0xe98d, 0x45f5, {                    \
      0x9a, 0x67, 0xb7, 0x01, 0x19, 0x59, 0x7d, 0xe7 \
    }                                                \
  }

class nsITransformObserver : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ITRANSFORMOBSERVER_IID)

  virtual nsresult OnDocumentCreated(
      mozilla::dom::Document* aSourceDocument,
      mozilla::dom::Document* aResultDocument) = 0;

  virtual nsresult OnTransformDone(mozilla::dom::Document* aSourceDocument,
                                   nsresult aResult,
                                   mozilla::dom::Document* aResultDocument) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsITransformObserver, NS_ITRANSFORMOBSERVER_IID)

#define NS_IDOCUMENTTRANSFORMER_IID                  \
  {                                                  \
    0xf45e1ff8, 0x50f3, 0x4496, {                    \
      0xb3, 0xa2, 0x0e, 0x03, 0xe8, 0x4a, 0x57, 0x11 \
    }                                                \
  }

class nsIDocumentTransformer : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOCUMENTTRANSFORMER_IID)

  NS_IMETHOD SetTransformObserver(nsITransformObserver* aObserver) = 0;
  NS_IMETHOD LoadStyleSheet(nsIURI* aUri,
                            mozilla::dom::Document* aLoaderDocument) = 0;
  NS_IMETHOD SetSourceContentModel(nsINode* aSource) = 0;
  NS_IMETHOD CancelLoads() = 0;

  NS_IMETHOD AddXSLTParamNamespace(const nsString& aPrefix,
                                   const nsString& aNamespace) = 0;
  NS_IMETHOD AddXSLTParam(const nsString& aName, const nsString& aNamespace,
                          const nsString& aValue, const nsString& aSelect,
                          nsINode* aContextNode) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDocumentTransformer,
                              NS_IDOCUMENTTRANSFORMER_IID)

#endif  // nsIDocumentTransformer_h__
