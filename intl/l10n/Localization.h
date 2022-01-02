/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_intl_l10n_Localization_h
#define mozilla_intl_l10n_Localization_h

#include "nsCycleCollectionParticipant.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"
#include "nsWeakReference.h"
#include "nsIScriptError.h"
#include "nsContentUtils.h"
#include "nsPIDOMWindow.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/LocalizationBinding.h"
#include "mozilla/intl/LocalizationBindings.h"
#include "mozilla/intl/L10nRegistry.h"

namespace mozilla {
namespace intl {

// The state where the application contains incomplete localization resources
// is much more common than for other types of core resources.
//
// In result, our localization is designed to handle missing resources
// gracefully, and we need a more fine-tuned way to communicate those problems
// to developers.
//
// In particular, we want developers and early adopters to be able to reason
// about missing translations, without bothering end user in production, where
// the user cannot react to that.
//
// We currently differentiate between nightly/dev-edition builds or automation
// where we report the errors, and beta/release, where we silence them.
//
// A side effect of the conditional model of strict vs loose error handling is
// that we don't have a way to write integration tests for behavior we expect
// out of production environment. See bug 1741430.
[[maybe_unused]] static bool MaybeReportErrorsToGecko(
    const nsTArray<nsCString>& aErrors, ErrorResult& aRv,
    nsIGlobalObject* aGlobal) {
  if (!aErrors.IsEmpty()) {
    if (xpc::IsInAutomation()) {
      aRv.ThrowInvalidStateError(aErrors.ElementAt(0));
      return true;
    }

#if defined(NIGHTLY_BUILD) || defined(MOZ_DEV_EDITION) || defined(DEBUG)
    dom::Document* doc = nullptr;
    if (aGlobal) {
      nsPIDOMWindowInner* innerWindow = aGlobal->AsInnerWindow();
      if (innerWindow) {
        doc = innerWindow->GetExtantDoc();
      }
    }

    for (const auto& error : aErrors) {
      nsContentUtils::ReportToConsoleNonLocalized(NS_ConvertUTF8toUTF16(error),
                                                  nsIScriptError::warningFlag,
                                                  "l10n"_ns, doc);
      printf_stderr("%s\n", error.get());
    }
#endif
  }

  return false;
}

class Localization : public nsIObserver,
                     public nsWrapperCache,
                     public nsSupportsWeakReference {
  template <typename T, typename... Args>
  friend already_AddRefed<T> mozilla::MakeAndAddRef(Args&&... aArgs);

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Localization,
                                                         nsIObserver)
  NS_DECL_NSIOBSERVER

  static already_AddRefed<Localization> Constructor(
      const dom::GlobalObject& aGlobal,
      const dom::Sequence<nsCString>& aResourceIds, bool aIsSync,
      const dom::Optional<dom::NonNull<L10nRegistry>>& aRegistry,
      const dom::Optional<dom::Sequence<nsCString>>& aLocales,
      ErrorResult& aRv);
  static already_AddRefed<Localization> Create(
      const nsTArray<nsCString>& aResourceIds, bool aIsSync);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  void SetIsSync(bool aIsSync);

  already_AddRefed<dom::Promise> FormatValue(
      const nsACString& aId, const dom::Optional<L10nArgs>& aArgs,
      ErrorResult& aRv);

  already_AddRefed<dom::Promise> FormatValues(
      const dom::Sequence<dom::OwningUTF8StringOrL10nIdArgs>& aKeys,
      ErrorResult& aRv);

  already_AddRefed<dom::Promise> FormatMessages(
      const dom::Sequence<dom::OwningUTF8StringOrL10nIdArgs>& aKeys,
      ErrorResult& aRv);

  void FormatValueSync(const nsACString& aId,
                       const dom::Optional<L10nArgs>& aArgs,
                       nsACString& aRetVal, ErrorResult& aRv);
  void FormatValuesSync(
      const dom::Sequence<dom::OwningUTF8StringOrL10nIdArgs>& aKeys,
      nsTArray<nsCString>& aRetVal, ErrorResult& aRv);
  void FormatMessagesSync(
      const dom::Sequence<dom::OwningUTF8StringOrL10nIdArgs>& aKeys,
      nsTArray<dom::Nullable<dom::L10nMessage>>& aRetVal, ErrorResult& aRv);

  void AddResourceId(const nsACString& aResourceId);
  uint32_t RemoveResourceId(const nsACString& aResourceId);
  void AddResourceIds(const nsTArray<nsCString>& aResourceIds);
  uint32_t RemoveResourceIds(const nsTArray<nsCString>& aResourceIds);

  void SetAsync();
  bool IsSync();

 protected:
  Localization(const nsTArray<nsCString>& aResIds, bool aIsSync);
  Localization(nsIGlobalObject* aGlobal, bool aIsSync);

  Localization(nsIGlobalObject* aGlobal, const nsTArray<nsCString>& aResIds,
               bool aIsSync);

  Localization(nsIGlobalObject* aGlobal, bool aIsSync,
               const ffi::LocalizationRc* aRaw);

  virtual ~Localization();

  void RegisterObservers();
  virtual void OnChange();
  already_AddRefed<dom::Promise> MaybeWrapPromise(dom::Promise* aInnerPromise);

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<const ffi::LocalizationRc> mRaw;
};

}  // namespace intl
}  // namespace mozilla

#endif
