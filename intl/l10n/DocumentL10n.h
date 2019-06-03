/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DocumentL10n_h
#define mozilla_dom_DocumentL10n_h

#include "mozILocalization.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIContentSink.h"
#include "nsINode.h"
#include "nsIObserver.h"
#include "nsWrapperCache.h"
#include "nsWeakReference.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/l10n/Mutations.h"
#include "mozilla/dom/DOMOverlaysBinding.h"

namespace mozilla {
namespace dom {

namespace l10n {
class Mutations;
}
struct L10nKey;

class PromiseResolver final : public PromiseNativeHandler {
 public:
  NS_DECL_ISUPPORTS

  explicit PromiseResolver(Promise* aPromise);
  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;
  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

 protected:
  virtual ~PromiseResolver();

  RefPtr<Promise> mPromise;
};

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
class DocumentL10n final : public nsIObserver,
                           public nsSupportsWeakReference,
                           public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(DocumentL10n,
                                                         nsIObserver)
  NS_DECL_NSIOBSERVER

 public:
  explicit DocumentL10n(Document* aDocument);
  bool Init(nsTArray<nsString>& aResourceIds);

 protected:
  virtual ~DocumentL10n();

  RefPtr<Document> mDocument;
  RefPtr<Promise> mReady;
  DocumentL10nState mState;
  nsCOMPtr<mozILocalization> mLocalization;
  nsCOMPtr<nsIContentSink> mContentSink;
  RefPtr<l10n::Mutations> mMutations;
  nsTHashtable<nsRefPtrHashKey<Element>> mRoots;

  already_AddRefed<Promise> MaybeWrapPromise(Promise* aPromise);
  void RegisterObservers();
  void DisconnectMutations();

 public:
  Document* GetParentObject() const { return mDocument; };

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  /**
   * A method for adding resources to the localization context.
   *
   * Returns a new count of resources used by the context.
   */
  uint32_t AddResourceIds(nsTArray<nsString>& aResourceIds);

  /**
   * A method for removing resources from the localization context.
   *
   * Returns a new count of resources used by the context.
   */
  uint32_t RemoveResourceIds(nsTArray<nsString>& aResourceIds);

  already_AddRefed<Promise> FormatMessages(JSContext* aCx,
                                           const Sequence<L10nKey>& aKeys,
                                           ErrorResult& aRv);
  already_AddRefed<Promise> FormatValues(JSContext* aCx,
                                         const Sequence<L10nKey>& aKeys,
                                         ErrorResult& aRv);
  already_AddRefed<Promise> FormatValue(
      JSContext* aCx, const nsAString& aId,
      const Optional<JS::Handle<JSObject*>>& aArgs, ErrorResult& aRv);

  void SetAttributes(JSContext* aCx, Element& aElement, const nsAString& aId,
                     const Optional<JS::Handle<JSObject*>>& aArgs,
                     ErrorResult& aRv);
  void GetAttributes(JSContext* aCx, Element& aElement, L10nKey& aResult,
                     ErrorResult& aRv);

  already_AddRefed<Promise> TranslateFragment(nsINode& aNode, ErrorResult& aRv);

  void GetTranslatables(nsINode& aNode,
                        Sequence<OwningNonNull<Element>>& aElements,
                        ErrorResult& aRv);

  already_AddRefed<Promise> TranslateElements(
      const Sequence<OwningNonNull<Element>>& aElements, ErrorResult& aRv);

  void PauseObserving(ErrorResult& aRv);
  void ResumeObserving(ErrorResult& aRv);
  static void ReportDOMOverlaysErrors(
      Document* aDocument, nsTArray<mozilla::dom::DOMOverlaysError>& aErrors);

  Promise* Ready();

  /**
   * Add node to nodes observed for localization
   * related changes.
   */
  void ConnectRoot(Element* aNode);

  /**
   * Remove node from nodes observed for localization
   * related changes.
   */
  void DisconnectRoot(Element* aNode);

  void TriggerInitialDocumentTranslation();

  void InitialDocumentTranslationCompleted();

  Document* GetDocument() { return mDocument; };

  void OnChange();
  static void SetRootInfo(Element* aElement);

  void OnCreatePresShell();

 protected:
  void DisconnectRoots();
  void TranslateRoots();
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_DocumentL10n_h
