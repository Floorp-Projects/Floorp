/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_l10n_DocumentL10n_h
#define mozilla_dom_l10n_DocumentL10n_h

#include "nsCycleCollectionParticipant.h"
#include "nsIContentSink.h"
#include "nsINode.h"
#include "nsWrapperCache.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/l10n/DOMLocalization.h"

namespace mozilla {
namespace dom {
namespace l10n {

enum class DocumentL10nState {
  Initialized = 0,
  InitialTranslationTriggered,
  InitialTranslationCompleted
};

/**
 * This class maintains localization status of the document.
 *
 * The document will initialize it lazily when a link with a localization
 * resource is added to the document.
 *
 * Once initialized, DocumentL10n relays all API methods to an
 * instance of mozILocalization and maintains a single promise
 * which gets resolved the first time the document gets translated.
 */
class DocumentL10n final : public l10n::DOMLocalization {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DocumentL10n, l10n::DOMLocalization)

 public:
  explicit DocumentL10n(Document* aDocument);
  void Init(nsTArray<nsString>& aResourceIds, ErrorResult& aRv);

   protected:
  virtual ~DocumentL10n() = default;

  RefPtr<Document> mDocument;
  RefPtr<Promise> mReady;
  DocumentL10nState mState;
  nsCOMPtr<nsIContentSink> mContentSink;

 public:
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  Promise* Ready();

  void TriggerInitialDocumentTranslation();

  void InitialDocumentTranslationCompleted();

  Document* GetDocument() { return mDocument; };
  void OnCreatePresShell();
};

}  // namespace l10n
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_l10n_DocumentL10n_h
