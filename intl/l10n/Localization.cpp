/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Localization.h"
#include "nsImportModule.h"
#include "nsContentUtils.h"
#include "mozilla/BasePrincipal.h"

#define INTL_APP_LOCALES_CHANGED "intl:app-locales-changed"
#define L10N_PSEUDO_PREF "intl.l10n.pseudo"
#define INTL_UI_DIRECTION_PREF "intl.uidirection"

static const char* kObservedPrefs[] = {L10N_PSEUDO_PREF, INTL_UI_DIRECTION_PREF,
                                       nullptr};

using namespace mozilla::intl;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(Localization)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Localization)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocalization)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Localization)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocalization)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(Localization)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Localization)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Localization)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Localization)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

Localization::Localization(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal), mIsSync(false) {
  nsCOMPtr<mozILocalizationJSM> jsm =
      do_ImportModule("resource://gre/modules/Localization.jsm");
  MOZ_RELEASE_ASSERT(jsm);

  Unused << jsm->GetLocalization(getter_AddRefs(mLocalization));
  MOZ_RELEASE_ASSERT(mLocalization);
}

void Localization::Activate(const bool aSync, const bool aEager,
                            const BundleGenerator& aBundleGenerator) {
  AutoJSContext cx;
  JS::Rooted<JS::Value> generateBundlesJS(cx);
  JS::Rooted<JS::Value> generateBundlesSyncJS(cx);

  if (aBundleGenerator.mGenerateBundles.WasPassed()) {
    GenerateBundles& generateBundles =
        aBundleGenerator.mGenerateBundles.Value();
    generateBundlesJS.set(JS::ObjectValue(*generateBundles.CallbackOrNull()));
  }
  if (aBundleGenerator.mGenerateBundlesSync.WasPassed()) {
    GenerateBundlesSync& generateBundlesSync =
        aBundleGenerator.mGenerateBundlesSync.Value();
    generateBundlesSyncJS.set(
        JS::ObjectValue(*generateBundlesSync.CallbackOrNull()));
  }
  mIsSync = aSync;

  mLocalization->Activate(aSync, aEager, generateBundlesJS,
                          generateBundlesSyncJS);

  RegisterObservers();
}

already_AddRefed<Localization> Localization::Constructor(
    const GlobalObject& aGlobal, const Sequence<nsString>& aResourceIds,
    const bool aSync, const BundleGenerator& aBundleGenerator,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Localization> loc = new Localization(global);

  if (aResourceIds.Length()) {
    loc->AddResourceIds(aResourceIds);
  }

  loc->Activate(aSync, true, aBundleGenerator);

  return loc.forget();
}

nsIGlobalObject* Localization::GetParentObject() const { return mGlobal; }

JSObject* Localization::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return Localization_Binding::Wrap(aCx, this, aGivenProto);
}

Localization::~Localization() {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, INTL_APP_LOCALES_CHANGED);
  }

  Preferences::RemoveObservers(this, kObservedPrefs);
}

/* Protected */

void Localization::RegisterObservers() {
  DebugOnly<nsresult> rv = Preferences::AddWeakObservers(this, kObservedPrefs);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Adding observers failed.");

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, INTL_APP_LOCALES_CHANGED, true);
  }
}

NS_IMETHODIMP
Localization::Observe(nsISupports* aSubject, const char* aTopic,
                      const char16_t* aData) {
  if (!strcmp(aTopic, INTL_APP_LOCALES_CHANGED)) {
    OnChange();
  } else {
    MOZ_ASSERT(!strcmp("nsPref:changed", aTopic));
    nsDependentString pref(aData);
    if (pref.EqualsLiteral(L10N_PSEUDO_PREF) ||
        pref.EqualsLiteral(INTL_UI_DIRECTION_PREF)) {
      OnChange();
    }
  }

  return NS_OK;
}

void Localization::OnChange() {
  if (mLocalization) {
    mLocalization->OnChange();
  }
}

uint32_t Localization::AddResourceId(const nsAString& aResourceId) {
  uint32_t ret = 0;
  mLocalization->AddResourceId(aResourceId, &ret);
  return ret;
}

uint32_t Localization::RemoveResourceId(const nsAString& aResourceId) {
  // We need to guard against a scenario where the
  // mLocalization has been unlinked, but the elements
  // are only now removed from DOM.
  if (!mLocalization) {
    return 0;
  }
  uint32_t ret = 0;
  mLocalization->RemoveResourceId(aResourceId, &ret);
  return ret;
}

/**
 * Localization API
 */

uint32_t Localization::AddResourceIds(const nsTArray<nsString>& aResourceIds) {
  uint32_t ret = 0;
  mLocalization->AddResourceIds(aResourceIds, &ret);
  return ret;
}

uint32_t Localization::RemoveResourceIds(
    const nsTArray<nsString>& aResourceIds) {
  // We need to guard against a scenario where the
  // mLocalization has been unlinked, but the elements
  // are only now removed from DOM.
  if (!mLocalization) {
    return 0;
  }
  uint32_t ret = 0;
  mLocalization->RemoveResourceIds(aResourceIds, &ret);
  return ret;
}

already_AddRefed<Promise> Localization::FormatValue(
    JSContext* aCx, const nsACString& aId, const Optional<L10nArgs>& aArgs,
    ErrorResult& aRv) {
  JS::Rooted<JS::Value> args(aCx);

  if (aArgs.WasPassed()) {
    ConvertL10nArgsToJSValue(aCx, aArgs.Value(), &args, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  } else {
    args = JS::UndefinedValue();
  }

  RefPtr<Promise> promise;
  nsresult rv = mLocalization->FormatValue(aId, args, getter_AddRefs(promise));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }
  return MaybeWrapPromise(promise);
}

void Localization::SetIsSync(const bool aIsSync) {
  mIsSync = aIsSync;
  mLocalization->SetIsSync(aIsSync);
}

already_AddRefed<Promise> Localization::FormatValues(
    JSContext* aCx, const Sequence<L10nKey>& aKeys, ErrorResult& aRv) {
  nsTArray<JS::Value> jsKeys;
  SequenceRooter<JS::Value> rooter(aCx, &jsKeys);
  for (auto& key : aKeys) {
    JS::RootedValue jsKey(aCx);
    if (!ToJSValue(aCx, key, &jsKey)) {
      aRv.NoteJSContextException(aCx);
      return nullptr;
    }
    jsKeys.AppendElement(jsKey);
  }

  RefPtr<Promise> promise;
  aRv = mLocalization->FormatValues(jsKeys, getter_AddRefs(promise));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return MaybeWrapPromise(promise);
}

already_AddRefed<Promise> Localization::FormatMessages(
    JSContext* aCx, const Sequence<L10nKey>& aKeys, ErrorResult& aRv) {
  nsTArray<JS::Value> jsKeys;
  SequenceRooter<JS::Value> rooter(aCx, &jsKeys);
  for (auto& key : aKeys) {
    JS::RootedValue jsKey(aCx);
    if (!ToJSValue(aCx, key, &jsKey)) {
      aRv.NoteJSContextException(aCx);
      return nullptr;
    }
    jsKeys.AppendElement(jsKey);
  }

  RefPtr<Promise> promise;
  aRv = mLocalization->FormatMessages(jsKeys, getter_AddRefs(promise));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return MaybeWrapPromise(promise);
}

void Localization::FormatValueSync(JSContext* aCx, const nsACString& aId,
                                   const Optional<L10nArgs>& aArgs,
                                   nsACString& aRetVal, ErrorResult& aRv) {
  JS::Rooted<JS::Value> args(aCx);

  if (aArgs.WasPassed()) {
    ConvertL10nArgsToJSValue(aCx, aArgs.Value(), &args, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  } else {
    args = JS::UndefinedValue();
  }

  aRv = mLocalization->FormatValueSync(aId, args, aRetVal);
}

void Localization::FormatValuesSync(JSContext* aCx,
                                    const Sequence<L10nKey>& aKeys,
                                    nsTArray<nsCString>& aRetVal,
                                    ErrorResult& aRv) {
  nsTArray<JS::Value> jsKeys;
  SequenceRooter<JS::Value> rooter(aCx, &jsKeys);
  for (auto& key : aKeys) {
    JS::RootedValue jsKey(aCx);
    if (!ToJSValue(aCx, key, &jsKey)) {
      aRv.NoteJSContextException(aCx);
      return;
    }
    jsKeys.AppendElement(jsKey);
  }

  aRv = mLocalization->FormatValuesSync(jsKeys, aRetVal);
}

void Localization::FormatMessagesSync(JSContext* aCx,
                                      const Sequence<L10nKey>& aKeys,
                                      nsTArray<Nullable<L10nMessage>>& aRetVal,
                                      ErrorResult& aRv) {
  nsTArray<JS::Value> jsKeys;
  SequenceRooter<JS::Value> rooter(aCx, &jsKeys);
  for (auto& key : aKeys) {
    JS::RootedValue jsKey(aCx);
    if (!ToJSValue(aCx, key, &jsKey)) {
      aRv.NoteJSContextException(aCx);
      return;
    }
    jsKeys.AppendElement(jsKey);
  }

  nsTArray<JS::Value> messages;

  SequenceRooter<JS::Value> messagesRooter(aCx, &messages);
  aRv = mLocalization->FormatMessagesSync(jsKeys, messages);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  JS::Rooted<JS::Value> rootedMsg(aCx);
  for (auto& msg : messages) {
    rootedMsg.set(msg);
    Nullable<L10nMessage>* slotPtr = aRetVal.AppendElement(mozilla::fallible);
    if (!slotPtr) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    if (rootedMsg.isNull()) {
      slotPtr->SetNull();
    } else {
      JS_WrapValue(aCx, &rootedMsg);
      if (!slotPtr->SetValue().Init(aCx, rootedMsg)) {
        aRv.NoteJSContextException(aCx);
        return;
      }
    }
  }
}

/**
 * PromiseResolver is a PromiseNativeHandler used
 * by MaybeWrapPromise method.
 */
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

NS_INTERFACE_MAP_BEGIN(PromiseResolver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(PromiseResolver)
NS_IMPL_RELEASE(PromiseResolver)

PromiseResolver::PromiseResolver(Promise* aPromise) { mPromise = aPromise; }

void PromiseResolver::ResolvedCallback(JSContext* aCx,
                                       JS::Handle<JS::Value> aValue) {
  mPromise->MaybeResolveWithClone(aCx, aValue);
}

void PromiseResolver::RejectedCallback(JSContext* aCx,
                                       JS::Handle<JS::Value> aValue) {
  mPromise->MaybeRejectWithClone(aCx, aValue);
}

PromiseResolver::~PromiseResolver() { mPromise = nullptr; }

/**
 * MaybeWrapPromise is a helper method used by Localization
 * API methods to clone the value returned by a promise
 * into a new context.
 *
 * This allows for a promise from a privileged context
 * to be returned into an unprivileged document.
 *
 * This method is only used for promises that carry values.
 */
already_AddRefed<Promise> Localization::MaybeWrapPromise(
    Promise* aInnerPromise) {
  // For system principal we don't need to wrap the
  // result promise at all.
  nsIPrincipal* principal = mGlobal->PrincipalOrNull();
  if (principal && principal->IsSystemPrincipal()) {
    return RefPtr<Promise>(aInnerPromise).forget();
  }

  ErrorResult result;
  RefPtr<Promise> docPromise = Promise::Create(mGlobal, result);
  if (NS_WARN_IF(result.Failed())) {
    return nullptr;
  }

  RefPtr<PromiseResolver> resolver = new PromiseResolver(docPromise);
  aInnerPromise->AppendNativeHandler(resolver);
  return docPromise.forget();
}

void Localization::ConvertL10nArgsToJSValue(
    JSContext* aCx, const L10nArgs& aArgs, JS::MutableHandle<JS::Value> aRetVal,
    ErrorResult& aRv) {
  // This method uses a temporary dictionary to automate
  // converting an IDL Record to a JS Value via a dictionary.
  //
  // Once we get ToJSValue for Record, we'll switch to that.
  L10nArgsHelperDict helperDict;
  for (auto& entry : aArgs.Entries()) {
    L10nArgs::EntryType* newEntry =
        helperDict.mArgs.Entries().AppendElement(fallible);
    if (!newEntry) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    newEntry->mKey = entry.mKey;
    newEntry->mValue = entry.mValue;
  }
  JS::Rooted<JS::Value> jsVal(aCx);
  if (!ToJSValue(aCx, helperDict, &jsVal)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  JS::Rooted<JSObject*> jsObj(aCx, &jsVal.toObject());
  if (!JS_GetProperty(aCx, jsObj, "args", aRetVal)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
}
