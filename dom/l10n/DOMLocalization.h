#ifndef mozilla_dom_l10n_DOMLocalization_h__
#define mozilla_dom_l10n_DOMLocalization_h__

#include "nsWeakReference.h"
#include "nsIObserver.h"
#include "mozILocalization.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/DOMLocalizationBinding.h"
#include "mozilla/dom/DocumentL10nBinding.h"
#include "mozilla/dom/DOMOverlaysBinding.h"
#include "mozilla/dom/l10n/Mutations.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {
namespace l10n {

class DOMLocalization : public nsIObserver,
                        public nsSupportsWeakReference,
                        public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(DOMLocalization,
                                                         nsIObserver)
  NS_DECL_NSIOBSERVER

  explicit DOMLocalization(nsIGlobalObject* aGlobal);
  void Init(nsTArray<nsString>& aResourceIds, ErrorResult& aRv);
  void Init(nsTArray<nsString>& aResourceIds, JS::Handle<JS::Value> aGenerateMessages, ErrorResult& aRv);

  static already_AddRefed<DOMLocalization> Constructor(
      const GlobalObject& aGlobal,
      const Optional<Sequence<nsString>>& aResourceIds,
      const Optional<OwningNonNull<GenerateMessages>>& aGenerateMessages,
      ErrorResult& aRv);

  nsIGlobalObject* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  /**
   * Localization API
   *
   * Methods documentation in DOMLocalization.webidl
   */
  uint32_t AddResourceIds(const nsTArray<nsString>& aResourceIds, bool aEager);

  uint32_t RemoveResourceIds(const nsTArray<nsString>& aResourceIds);

  already_AddRefed<Promise> FormatValue(
      JSContext* aCx, const nsAString& aId,
      const Optional<JS::Handle<JSObject*>>& aArgs, ErrorResult& aRv);

  already_AddRefed<Promise> FormatValues(JSContext* aCx,
                                         const Sequence<L10nKey>& aKeys,
                                         ErrorResult& aRv);

  already_AddRefed<Promise> FormatMessages(JSContext* aCx,
                                           const Sequence<L10nKey>& aKeys,
                                           ErrorResult& aRv);

  /**
   * DOMLocalization API
   *
   * Methods documentation in DOMLocalization.webidl
   */

  void ConnectRoot(Element& aNode, ErrorResult& aRv);
  void DisconnectRoot(Element& aNode, ErrorResult& aRv);

  void PauseObserving(ErrorResult& aRv);
  void ResumeObserving(ErrorResult& aRv);

  void SetAttributes(JSContext* aCx, Element& aElement, const nsAString& aId,
                     const Optional<JS::Handle<JSObject*>>& aArgs,
                     ErrorResult& aRv);
  void GetAttributes(JSContext* aCx, Element& aElement, L10nKey& aResult,
                     ErrorResult& aRv);

  already_AddRefed<Promise> TranslateFragment(nsINode& aNode, ErrorResult& aRv);

  already_AddRefed<Promise> TranslateElements(
      const Sequence<OwningNonNull<Element>>& aElements, ErrorResult& aRv);

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
   */
  void ApplyTranslations(nsTArray<nsCOMPtr<Element>>& aElements,
                         nsTArray<L10nValue>& aTranslations, ErrorResult& aRv);

 protected:
  virtual ~DOMLocalization();
  void RegisterObservers();
  void OnChange();
  void DisconnectMutations();
  void DisconnectRoots();
  already_AddRefed<Promise> MaybeWrapPromise(Promise* aPromise);
  void ReportDOMOverlaysErrors(
      nsTArray<mozilla::dom::DOMOverlaysError>& aErrors);

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<Mutations> mMutations;
  nsCOMPtr<mozILocalization> mLocalization;
  nsTHashtable<nsRefPtrHashKey<Element>> mRoots;
};

}  // namespace l10n
}  // namespace dom
}  // namespace mozilla

#endif
