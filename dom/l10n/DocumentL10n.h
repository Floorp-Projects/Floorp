/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_l10n_DocumentL10n_h
#define mozilla_dom_l10n_DocumentL10n_h

#include "mozilla/dom/Document.h"
#include "mozilla/dom/DOMLocalization.h"

namespace mozilla {
namespace dom {

enum class DocumentL10nState {
  // State set when the DocumentL10n gets constructed.
  Uninitialized = 0,

  // State set when the DocumentL10n is activated and ready to be used.
  Activated,

  // State set when the initial translation got triggered. This happens
  // if DocumentL10n was constructed during parsing of the document.
  //
  // If the DocumentL10n gets constructed later, we'll skip directly to
  // Ready state.
  InitialTranslationTriggered,

  // State set the DocumentL10n has been fully initialized, potentially
  // with initial translation being completed.
  Ready,
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
class DocumentL10n final : public DOMLocalization {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DocumentL10n, DOMLocalization)

  static RefPtr<DocumentL10n> Create(Document* aDocument);

  void Activate(const bool aLazy);

 protected:
  explicit DocumentL10n(Document* aDocument);
  bool Init();

  virtual ~DocumentL10n() = default;

  RefPtr<Document> mDocument;
  RefPtr<Promise> mReady;
  DocumentL10nState mState;
  nsCOMPtr<nsIContentSink> mContentSink;

 public:
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  Promise* Ready();

  void TriggerInitialTranslation();
  already_AddRefed<Promise> TranslateDocument(ErrorResult& aRv);

  void InitialTranslationCompleted();

  Document* GetDocument() { return mDocument; };
  void OnCreatePresShell();

  void ConnectRoot(nsINode& aNode, bool aTranslate, ErrorResult& aRv);

  DocumentL10nState GetState() { return mState; };

  bool mBlockingLayout = false;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_l10n_DocumentL10n_h
