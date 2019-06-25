#ifndef mozilla_intl_l10n_Localization_h
#define mozilla_intl_l10n_Localization_h

#include "nsWeakReference.h"
#include "nsIObserver.h"
#include "mozILocalization.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/LocalizationBinding.h"
#include "mozilla/dom/PromiseNativeHandler.h"

class nsIGlobalObject;

using namespace mozilla::dom;

namespace mozilla {
namespace intl {

class Localization : public nsIObserver,
                     public nsSupportsWeakReference,
                     public nsWrapperCache {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Localization,
                                                         nsIObserver)
  NS_DECL_NSIOBSERVER

  explicit Localization(nsIGlobalObject* aGlobal);
  void Init(nsTArray<nsString>& aResourceIds, ErrorResult& aRv);
  void Init(nsTArray<nsString>& aResourceIds,
            JS::Handle<JS::Value> aGenerateMessages, ErrorResult& aRv);

  static already_AddRefed<Localization> Constructor(
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
   * Methods documentation in Localization.webidl
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

 protected:
  virtual ~Localization();
  void RegisterObservers();
  void OnChange();
  already_AddRefed<Promise> MaybeWrapPromise(Promise* aInnerPromise);

  nsCOMPtr<nsIGlobalObject> mGlobal;
  nsCOMPtr<mozILocalization> mLocalization;
};

}  // namespace intl
}  // namespace mozilla

#endif