/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_l10n_DOMLocalization_h
#define mozilla_dom_l10n_DOMLocalization_h

#include "nsTHashSet.h"
#include "nsXULPrototypeDocument.h"
#include "mozilla/intl/Localization.h"
#include "mozilla/dom/DOMLocalizationBinding.h"
#include "mozilla/dom/L10nMutations.h"
#include "mozilla/dom/L10nOverlaysBinding.h"
#include "mozilla/dom/LocalizationBinding.h"

// XXX Avoid including this here by moving function bodies to the cpp file
#include "nsINode.h"

namespace mozilla {
namespace dom {

class Element;
class L10nMutations;

class DOMLocalization : public intl::Localization {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DOMLocalization, Localization)

  static already_AddRefed<DOMLocalization> Create(
      nsIGlobalObject* aGlobal, const bool aSync,
      const BundleGenerator& aBundleGenerator);

  void Destroy();

  static already_AddRefed<DOMLocalization> Constructor(
      const GlobalObject& aGlobal, const Sequence<nsString>& aResourceIds,
      const bool aSync, const BundleGenerator& aBundleGenerator,
      ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  /**
   * DOMLocalization API
   *
   * Methods documentation in DOMLocalization.webidl
   */

  void ConnectRoot(nsINode& aNode, ErrorResult& aRv);
  void DisconnectRoot(nsINode& aNode, ErrorResult& aRv);

  void PauseObserving(ErrorResult& aRv);
  void ResumeObserving(ErrorResult& aRv);

  void SetAttributes(JSContext* aCx, Element& aElement, const nsAString& aId,
                     const Optional<JS::Handle<JSObject*>>& aArgs,
                     ErrorResult& aRv);
  void GetAttributes(Element& aElement, L10nIdArgs& aResult, ErrorResult& aRv);

  already_AddRefed<Promise> TranslateFragment(nsINode& aNode, ErrorResult& aRv);

  already_AddRefed<Promise> TranslateElements(
      const Sequence<OwningNonNull<Element>>& aElements, ErrorResult& aRv);
  already_AddRefed<Promise> TranslateElements(
      const Sequence<OwningNonNull<Element>>& aElements,
      nsXULPrototypeDocument* aProto, ErrorResult& aRv);

  already_AddRefed<Promise> TranslateRoots(ErrorResult& aRv);

  /**
   * Helper methods
   */

  /**
   * Accumulates all translatable elements (ones containing
   * a `data-l10n-id` attribute) from under a node into
   * a list of elements.
   */
  static void GetTranslatables(nsINode& aNode,
                               Sequence<OwningNonNull<Element>>& aElements,
                               ErrorResult& aRv);

  /**
   * Sets the root information such as locale and direction.
   */
  static void SetRootInfo(Element* aElement);

  /**
   * Applies l10n translations on translatable elements.
   *
   * If `aProto` gets passed, it'll be used to cache
   * the localized elements.
   *
   * Result is `true` if all translations were applied
   * successfully, and `false` otherwise.
   */
  bool ApplyTranslations(nsTArray<nsCOMPtr<Element>>& aElements,
                         nsTArray<Nullable<L10nMessage>>& aTranslations,
                         nsXULPrototypeDocument* aProto, ErrorResult& aRv);

  bool SubtreeRootInRoots(nsINode* aSubtreeRoot) {
    for (const auto* key : mRoots) {
      nsINode* subtreeRoot = key->SubtreeRoot();
      if (subtreeRoot == aSubtreeRoot) {
        return true;
      }
    }
    return false;
  }

 protected:
  explicit DOMLocalization(nsIGlobalObject* aGlobal, const bool aSync,
                           const BundleGenerator& aBundleGenerator);
  virtual ~DOMLocalization();
  void OnChange() override;
  void DisconnectMutations();
  void DisconnectRoots();
  void ReportL10nOverlaysErrors(nsTArray<L10nOverlaysError>& aErrors);
  void ConvertStringToL10nArgs(const nsString& aInput, intl::L10nArgs& aRetVal,
                               ErrorResult& aRv);

  RefPtr<L10nMutations> mMutations;
  nsTHashSet<RefPtr<nsINode>> mRoots;
};

}  // namespace dom
}  // namespace mozilla

#endif
