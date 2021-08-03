/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Localization.h"
#include "nsContentUtils.h"

#define INTL_APP_LOCALES_CHANGED "intl:app-locales-changed"
#define L10N_PSEUDO_PREF "intl.l10n.pseudo"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::intl;

static const char* kObservedPrefs[] = {L10N_PSEUDO_PREF, nullptr};

static nsTArray<ffi::L10nKey> ConvertFromL10nKeys(
    const Sequence<OwningUTF8StringOrL10nIdArgs>& aKeys) {
  nsTArray<ffi::L10nKey> l10nKeys(aKeys.Length());

  for (const auto& entry : aKeys) {
    if (entry.IsUTF8String()) {
      const auto& id = entry.GetAsUTF8String();
      ffi::L10nKey* key = l10nKeys.AppendElement();
      key->id = &id;
    } else {
      const auto& e = entry.GetAsL10nIdArgs();
      ffi::L10nKey* key = l10nKeys.AppendElement();
      key->id = &e.mId;
      if (!e.mArgs.IsNull()) {
        FluentBundle::ConvertArgs(e.mArgs.Value(), key->args);
      }
    }
  }

  return l10nKeys;
}

static FallibleTArray<AttributeNameValue> ConvertToAttributeNameValue(
    const nsTArray<ffi::L10nAttribute>& aAttributes, OOMReporter& aError) {
  FallibleTArray<AttributeNameValue> result(aAttributes.Length());
  for (const auto& attr : aAttributes) {
    auto cvtAttr = AttributeNameValue();
    cvtAttr.mName = attr.name;
    cvtAttr.mValue = attr.value;
    if (!result.AppendElement(std::move(cvtAttr), fallible)) {
      result.Clear();
      aError.ReportOOM();
      return result;
    }
  }

  return result;
}

static FallibleTArray<Nullable<L10nMessage>> ConvertToL10nMessages(
    const nsTArray<ffi::OptionalL10nMessage>& aMessages, ErrorResult& aError) {
  FallibleTArray<Nullable<L10nMessage>> l10nMessages(aMessages.Length());

  for (const auto& entry : aMessages) {
    Nullable<L10nMessage> msg;
    if (entry.is_present) {
      L10nMessage& m = msg.SetValue();
      if (!entry.message.value.IsVoid()) {
        m.mValue = entry.message.value;
      }
      if (!entry.message.attributes.IsEmpty()) {
        m.mAttributes.SetValue(
            ConvertToAttributeNameValue(entry.message.attributes, aError));
      }
    }

    if (!l10nMessages.AppendElement(std::move(msg), fallible)) {
      l10nMessages.Clear();
      aError.Throw(NS_ERROR_OUT_OF_MEMORY);
      return l10nMessages;
    }
  }

  return l10nMessages;
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(Localization)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Localization)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Localization)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WEAK(Localization, mGlobal)

/* static */
already_AddRefed<Localization> Localization::Create(
    const nsTArray<nsCString>& aResourceIds, bool aIsSync) {
  return MakeAndAddRef<Localization>(aResourceIds, aIsSync);
}

Localization::Localization(const nsTArray<nsCString>& aResIds, bool aIsSync)
    : mIsSync(aIsSync) {
  ffi::localization_new(&aResIds, mIsSync, getter_AddRefs(mRaw));
  RegisterObservers();
}

Localization::Localization(nsIGlobalObject* aGlobal,
                           const nsTArray<nsCString>& aResIds, bool aIsSync)
    : mGlobal(aGlobal), mIsSync(aIsSync) {
  ffi::localization_new(&aResIds, mIsSync, getter_AddRefs(mRaw));
  RegisterObservers();
}

Localization::Localization(nsIGlobalObject* aGlobal,
                           const nsTArray<nsCString>& aResIds, bool aIsSync,
                           const L10nRegistry& aRegistry)
    : mGlobal(aGlobal), mIsSync(aIsSync) {
  ffi::localization_new_with_reg(&aResIds, mIsSync, aRegistry.Raw(),
                                 getter_AddRefs(mRaw));
}

Localization::Localization(nsIGlobalObject* aGlobal, bool aIsSync)
    : mGlobal(aGlobal), mIsSync(aIsSync) {
  nsTArray<nsCString> resIds;
  ffi::localization_new(&resIds, mIsSync, getter_AddRefs(mRaw));
  RegisterObservers();
}

Localization::Localization(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal), mIsSync(false) {
  nsTArray<nsCString> resIds;
  ffi::localization_new(&resIds, mIsSync, getter_AddRefs(mRaw));
  RegisterObservers();
}

already_AddRefed<Localization> Localization::Constructor(
    const GlobalObject& aGlobal, const Sequence<nsCString>& aResourceIds,
    bool aIsSync, const Optional<NonNull<L10nRegistry>>& aRegistry,
    ErrorResult& aRv) {
  nsTArray<nsCString> resIds = ToTArray<nsTArray<nsCString>>(aResourceIds);

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  if (aRegistry.WasPassed()) {
    return do_AddRef(
        new Localization(global, resIds, aIsSync, aRegistry.Value()));
  } else {
    return do_AddRef(new Localization(global, resIds, aIsSync));
  }
}

JSObject* Localization::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return Localization_Binding::Wrap(aCx, this, aGivenProto);
}

Localization::~Localization() = default;

NS_IMETHODIMP
Localization::Observe(nsISupports* aSubject, const char* aTopic,
                      const char16_t* aData) {
  if (!strcmp(aTopic, INTL_APP_LOCALES_CHANGED)) {
    OnChange();
  } else {
    MOZ_ASSERT(!strcmp("nsPref:changed", aTopic));
    nsDependentString pref(aData);
    if (pref.EqualsLiteral(L10N_PSEUDO_PREF)) {
      OnChange();
    }
  }

  return NS_OK;
}

void Localization::RegisterObservers() {
  DebugOnly<nsresult> rv = Preferences::AddWeakObservers(this, kObservedPrefs);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Adding observers failed.");

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();

  if (obs) {
    obs->AddObserver(this, INTL_APP_LOCALES_CHANGED, true);
  }
}

void Localization::OnChange() { ffi::localization_on_change(mRaw.get()); }

void Localization::SetIsSync(bool aIsSync) {
  MOZ_ASSERT(!aIsSync, "We should only move from sync to async!");
  mIsSync = aIsSync;
  Upgrade();
}

void Localization::AddResourceId(const nsAString& aResourceId) {
  NS_ConvertUTF16toUTF8 resId(aResourceId);
  ffi::localization_add_res_id(mRaw.get(), &resId);
}

uint32_t Localization::RemoveResourceId(const nsAString& aResourceId) {
  NS_ConvertUTF16toUTF8 resId(aResourceId);
  return ffi::localization_remove_res_id(mRaw.get(), &resId);
}

void Localization::AddResourceIds(const nsTArray<nsString>& aResourceIds) {
  nsTArray<nsCString> resIds(aResourceIds.Length());
  for (const auto& resId : aResourceIds) {
    resIds.AppendElement(NS_ConvertUTF16toUTF8(resId));
  }
  ffi::localization_add_res_ids(mRaw.get(), &resIds);
}

uint32_t Localization::RemoveResourceIds(
    const nsTArray<nsString>& aResourceIds) {
  nsTArray<nsCString> resIds(aResourceIds.Length());
  for (const auto& resId : aResourceIds) {
    resIds.AppendElement(NS_ConvertUTF16toUTF8(resId));
  }
  return ffi::localization_remove_res_ids(mRaw.get(), &resIds);
}

already_AddRefed<Promise> Localization::FormatValue(
    const nsACString& aId, const Optional<L10nArgs>& aArgs, ErrorResult& aRv) {
  nsTArray<ffi::L10nArg> l10nArgs;
  nsTArray<nsCString> errors;

  if (aArgs.WasPassed()) {
    const L10nArgs& args = aArgs.Value();
    FluentBundle::ConvertArgs(args, l10nArgs);
  }
  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);

  ffi::localization_format_value(
      mRaw.get(), &aId, &l10nArgs, promise,
      [](const Promise* aPromise, const nsACString* aValue,
         const nsTArray<nsCString>* aErrors) {
        Promise* promise = const_cast<Promise*>(aPromise);

        if (!aErrors->IsEmpty()) {
          ErrorResult rv;
          rv.ThrowInvalidStateError(aErrors->ElementAt(0));
          promise->MaybeReject(std::move(rv));
        } else {
          promise->MaybeResolve(aValue);
        }
      });

  return MaybeWrapPromise(promise);
}

already_AddRefed<Promise> Localization::FormatValues(
    const Sequence<OwningUTF8StringOrL10nIdArgs>& aKeys, ErrorResult& aRv) {
  nsTArray<ffi::L10nKey> l10nKeys = ConvertFromL10nKeys(aKeys);

  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  ffi::localization_format_values(
      mRaw.get(), &l10nKeys, promise,
      // callback function which will be invoked by the rust code, passing the
      // promise back in.
      [](const Promise* aPromise, const nsTArray<nsCString>* aValues,
         const nsTArray<nsCString>* aErrors) {
        Promise* promise = const_cast<Promise*>(aPromise);

        if (!aErrors->IsEmpty()) {
          promise->MaybeRejectWithInvalidStateError(aErrors->ElementAt(0));
        } else {
          promise->MaybeResolve(*aValues);
        }
      });

  return MaybeWrapPromise(promise);
}

already_AddRefed<Promise> Localization::FormatMessages(
    const Sequence<OwningUTF8StringOrL10nIdArgs>& aKeys, ErrorResult& aRv) {
  auto l10nKeys = ConvertFromL10nKeys(aKeys);

  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  ffi::localization_format_messages(
      mRaw.get(), &l10nKeys, promise,
      // callback function which will be invoked by the rust code, passing the
      // promise back in.
      [](const Promise* aPromise,
         const nsTArray<ffi::OptionalL10nMessage>* aRaw,
         const nsTArray<nsCString>* aErrors) {
        Promise* promise = const_cast<Promise*>(aPromise);

        if (!aErrors->IsEmpty()) {
          // XXX: We should consider throwing an array of errors here.
          promise->MaybeRejectWithInvalidStateError(aErrors->ElementAt(0));
        } else {
          ErrorResult rv;
          FallibleTArray<Nullable<L10nMessage>> messages;
          if (aRaw) {
            messages = ConvertToL10nMessages(*aRaw, rv);
          }
          if (rv.Failed()) {
            promise->MaybeReject(std::move(rv));
          } else {
            promise->MaybeResolve(messages);
          }
        }
      });

  return MaybeWrapPromise(promise);
}

void Localization::FormatValueSync(const nsACString& aId,
                                   const Optional<L10nArgs>& aArgs,
                                   nsACString& aRetVal, ErrorResult& aRv) {
  nsTArray<ffi::L10nArg> l10nArgs;
  nsTArray<nsCString> errors;

  if (aArgs.WasPassed()) {
    const L10nArgs& args = aArgs.Value();
    FluentBundle::ConvertArgs(args, l10nArgs);
  }

  ffi::localization_format_value_sync(mRaw.get(), &aId, &l10nArgs, &aRetVal,
                                      &errors);

  if (!errors.IsEmpty()) {
    aRv.ThrowInvalidStateError(errors.ElementAt(0));
  }
}

void Localization::FormatValuesSync(
    const Sequence<OwningUTF8StringOrL10nIdArgs>& aKeys,
    nsTArray<nsCString>& aRetVal, ErrorResult& aRv) {
  nsTArray<ffi::L10nKey> l10nKeys(aKeys.Length());
  nsTArray<nsCString> errors;

  for (const auto& entry : aKeys) {
    if (entry.IsUTF8String()) {
      const auto& id = entry.GetAsUTF8String();
      nsTArray<ffi::L10nArg> l10nArgs;
      ffi::L10nKey* key = l10nKeys.AppendElement();
      key->id = &id;
    } else {
      const auto& e = entry.GetAsL10nIdArgs();
      nsTArray<ffi::L10nArg> l10nArgs;
      ffi::L10nKey* key = l10nKeys.AppendElement();
      key->id = &e.mId;
      if (!e.mArgs.IsNull()) {
        FluentBundle::ConvertArgs(e.mArgs.Value(), key->args);
      }
    }
  }

  ffi::localization_format_values_sync(mRaw.get(), &l10nKeys, &aRetVal,
                                       &errors);

  if (!errors.IsEmpty()) {
    aRv.ThrowInvalidStateError(errors.ElementAt(0));
  }
}

void Localization::FormatMessagesSync(
    const Sequence<OwningUTF8StringOrL10nIdArgs>& aKeys,
    nsTArray<Nullable<L10nMessage>>& aRetVal, ErrorResult& aRv) {
  nsTArray<ffi::L10nKey> l10nKeys(aKeys.Length());
  nsTArray<nsCString> errors;

  for (const auto& entry : aKeys) {
    if (entry.IsUTF8String()) {
      const auto& id = entry.GetAsUTF8String();
      nsTArray<ffi::L10nArg> l10nArgs;
      ffi::L10nKey* key = l10nKeys.AppendElement();
      key->id = &id;
    } else {
      const auto& e = entry.GetAsL10nIdArgs();
      nsTArray<ffi::L10nArg> l10nArgs;
      ffi::L10nKey* key = l10nKeys.AppendElement();
      key->id = &e.mId;
      if (!e.mArgs.IsNull()) {
        FluentBundle::ConvertArgs(e.mArgs.Value(), key->args);
      }
    }
  }

  nsTArray<ffi::OptionalL10nMessage> result(l10nKeys.Length());

  ffi::localization_format_messages_sync(mRaw.get(), &l10nKeys, &result,
                                         &errors);

  if (!errors.IsEmpty()) {
    aRv.ThrowInvalidStateError(errors.ElementAt(0));
    return;
  }

  aRetVal = ConvertToL10nMessages(result, aRv);
}

void Localization::Upgrade() { ffi::localization_upgrade(mRaw.get()); }

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
