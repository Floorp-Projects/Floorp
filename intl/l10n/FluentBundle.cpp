/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FluentBundle.h"
#include "nsContentUtils.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/intl/NumberFormat.h"
#include "nsIInputStream.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "unicode/datefmt.h"

using namespace mozilla::dom;

namespace mozilla {
namespace intl {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FluentPattern, mParent)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(FluentPattern, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(FluentPattern, Release)

FluentPattern::FluentPattern(nsISupports* aParent, const nsACString& aId)
    : mId(aId), mParent(aParent) {
  MOZ_COUNT_CTOR(FluentPattern);
}
FluentPattern::FluentPattern(nsISupports* aParent, const nsACString& aId,
                             const nsACString& aAttrName)
    : mId(aId), mAttrName(aAttrName), mParent(aParent) {
  MOZ_COUNT_CTOR(FluentPattern);
}

JSObject* FluentPattern::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return FluentPattern_Binding::Wrap(aCx, this, aGivenProto);
}

FluentPattern::~FluentPattern() { MOZ_COUNT_DTOR(FluentPattern); };

/* FluentBundle */

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FluentBundle, mParent)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(FluentBundle, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(FluentBundle, Release)

FluentBundle::FluentBundle(nsISupports* aParent,
                           UniquePtr<ffi::FluentBundleRc> aRaw)
    : mParent(aParent), mRaw(std::move(aRaw)) {
  MOZ_COUNT_CTOR(FluentBundle);
}

already_AddRefed<FluentBundle> FluentBundle::Constructor(
    const dom::GlobalObject& aGlobal,
    const UTF8StringOrUTF8StringSequence& aLocales,
    const dom::FluentBundleOptions& aOptions, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  bool useIsolating = aOptions.mUseIsolating;

  nsAutoCString pseudoStrategy;
  if (aOptions.mPseudoStrategy.WasPassed()) {
    pseudoStrategy = aOptions.mPseudoStrategy.Value();
  }

  UniquePtr<ffi::FluentBundleRc> raw;

  if (aLocales.IsUTF8String()) {
    const nsACString& locale = aLocales.GetAsUTF8String();
    raw.reset(
        ffi::fluent_bundle_new_single(&locale, useIsolating, &pseudoStrategy));
  } else {
    const auto& locales = aLocales.GetAsUTF8StringSequence();
    raw.reset(ffi::fluent_bundle_new(locales.Elements(), locales.Length(),
                                     useIsolating, &pseudoStrategy));
  }

  if (!raw) {
    aRv.ThrowInvalidStateError(
        "Failed to create the FluentBundle. Check the "
        "locales and pseudo strategy arguments.");
    return nullptr;
  }

  return do_AddRef(new FluentBundle(global, std::move(raw)));
}

JSObject* FluentBundle::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return FluentBundle_Binding::Wrap(aCx, this, aGivenProto);
}

FluentBundle::~FluentBundle() { MOZ_COUNT_DTOR(FluentBundle); };

void FluentBundle::GetLocales(nsTArray<nsCString>& aLocales) {
  fluent_bundle_get_locales(mRaw.get(), &aLocales);
}

void FluentBundle::AddResource(
    FluentResource& aResource,
    const dom::FluentBundleAddResourceOptions& aOptions) {
  bool allowOverrides = aOptions.mAllowOverrides;
  nsTArray<nsCString> errors;

  fluent_bundle_add_resource(mRaw.get(), aResource.Raw(), allowOverrides,
                             &errors);

  for (auto& err : errors) {
    nsContentUtils::LogSimpleConsoleError(NS_ConvertUTF8toUTF16(err), "L10n",
                                          false, true,
                                          nsIScriptError::warningFlag);
  }
}

bool FluentBundle::HasMessage(const nsACString& aId) {
  return fluent_bundle_has_message(mRaw.get(), &aId);
}

void FluentBundle::GetMessage(const nsACString& aId,
                              Nullable<FluentMessage>& aRetVal) {
  bool hasValue = false;
  nsTArray<nsCString> attributes;
  bool exists =
      fluent_bundle_get_message(mRaw.get(), &aId, &hasValue, &attributes);
  if (exists) {
    FluentMessage& msg = aRetVal.SetValue();
    if (hasValue) {
      msg.mValue = new FluentPattern(mParent, aId);
    }
    for (auto& name : attributes) {
      auto newEntry = msg.mAttributes.Entries().AppendElement(fallible);
      newEntry->mKey = name;
      newEntry->mValue = new FluentPattern(mParent, aId, name);
    }
  }
}

bool extendJSArrayWithErrors(JSContext* aCx, JS::Handle<JSObject*> aErrors,
                             nsTArray<nsCString>& aInput) {
  uint32_t length;
  if (NS_WARN_IF(!JS::GetArrayLength(aCx, aErrors, &length))) {
    return false;
  }

  for (auto& err : aInput) {
    JS::Rooted<JS::Value> jsval(aCx);
    if (!ToJSValue(aCx, NS_ConvertUTF8toUTF16(err), &jsval)) {
      return false;
    }
    if (!JS_DefineElement(aCx, aErrors, length++, jsval, JSPROP_ENUMERATE)) {
      return false;
    }
  }
  return true;
}

void FluentBundle::FormatPattern(JSContext* aCx, const FluentPattern& aPattern,
                                 const Nullable<L10nArgs>& aArgs,
                                 const Optional<JS::Handle<JSObject*>>& aErrors,
                                 nsACString& aRetVal, ErrorResult& aRv) {
  nsTArray<nsCString> argIds;
  nsTArray<ffi::FluentArgument> argValues;

  if (!aArgs.IsNull()) {
    const L10nArgs& args = aArgs.Value();
    for (auto& entry : args.Entries()) {
      if (!entry.mValue.IsNull()) {
        argIds.AppendElement(entry.mKey);

        auto& value = entry.mValue.Value();
        if (value.IsUTF8String()) {
          argValues.AppendElement(
              ffi::FluentArgument::String(&value.GetAsUTF8String()));
        } else {
          argValues.AppendElement(
              ffi::FluentArgument::Double_(value.GetAsDouble()));
        }
      }
    }
  }

  nsTArray<nsCString> errors;
  bool succeeded = fluent_bundle_format_pattern(mRaw.get(), &aPattern.mId,
                                                &aPattern.mAttrName, &argIds,
                                                &argValues, &aRetVal, &errors);

  if (!succeeded) {
    return aRv.ThrowInvalidStateError(
        "Failed to format the FluentPattern. Likely the "
        "pattern could not be retrieved from the bundle.");
  }

  if (aErrors.WasPassed()) {
    if (!extendJSArrayWithErrors(aCx, aErrors.Value(), errors)) {
      aRv.ThrowUnknownError("Failed to add errors to an error array.");
    }
  }
}

// FFI

extern "C" {
ffi::RawNumberFormatter* FluentBuiltInNumberFormatterCreate(
    const nsCString* aLocale, const ffi::FluentNumberOptionsRaw* aOptions) {
  NumberFormatOptions options;
  switch (aOptions->style) {
    case ffi::FluentNumberStyleRaw::Decimal:
      break;
    case ffi::FluentNumberStyleRaw::Currency: {
      std::string currency = aOptions->currency.get();
      switch (aOptions->currency_display) {
        case ffi::FluentNumberCurrencyDisplayStyleRaw::Symbol:
          options.mCurrency = Some(std::make_pair(
              currency, NumberFormatOptions::CurrencyDisplayStyle::Symbol));
          break;
        case ffi::FluentNumberCurrencyDisplayStyleRaw::Code:
          options.mCurrency = Some(std::make_pair(
              currency, NumberFormatOptions::CurrencyDisplayStyle::Code));
          break;
        case ffi::FluentNumberCurrencyDisplayStyleRaw::Name:
          options.mCurrency = Some(std::make_pair(
              currency, NumberFormatOptions::CurrencyDisplayStyle::Name));
          break;
        default:
          MOZ_ASSERT_UNREACHABLE();
          break;
      }
    } break;
    case ffi::FluentNumberStyleRaw::Percent:
      options.mPercent = true;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE();
      break;
  }

  options.mUseGrouping = aOptions->use_grouping;
  options.mMinIntegerDigits = Some(aOptions->minimum_integer_digits);

  if (aOptions->minimum_significant_digits >= 0 ||
      aOptions->maximum_significant_digits >= 0) {
    options.mSignificantDigits =
        Some(std::make_pair(aOptions->minimum_significant_digits,
                            aOptions->maximum_significant_digits));
  } else {
    options.mFractionDigits = Some(std::make_pair(
        aOptions->minimum_fraction_digits, aOptions->maximum_fraction_digits));
  }

  return reinterpret_cast<ffi::RawNumberFormatter*>(
      new NumberFormat(aLocale->get(), options));
}

uint8_t* FluentBuiltInNumberFormatterFormat(
    const ffi::RawNumberFormatter* aFormatter, double input, size_t* aOutCount,
    size_t* aOutCapacity) {
  const NumberFormat* nf = reinterpret_cast<const NumberFormat*>(aFormatter);

  class Buffer {
   public:
    using CharType = uint8_t;

    bool allocate(size_t size) {
      mBuffer.reset(reinterpret_cast<CharType*>(malloc(size)));
      mCapacity = size;
      return true;
    }

    void* data() { return mBuffer.get(); }

    size_t size() const { return mCapacity; }

    void written(size_t amount) { mWritten = amount; }

    size_t mWritten = 0;
    size_t mCapacity = 0;
    CharType* mData = nullptr;

    struct FreePolicy {
      void operator()(const void* ptr) { free(const_cast<void*>(ptr)); }
    };

    UniquePtr<CharType[], FreePolicy> mBuffer;
  } buffer;

  nf->format(input, buffer);
  *aOutCount = buffer.mWritten;
  *aOutCapacity = buffer.mCapacity;
  return buffer.mBuffer.release();
}

void FluentBuiltInNumberFormatterDestroy(ffi::RawNumberFormatter* aFormatter) {
  delete reinterpret_cast<NumberFormat*>(aFormatter);
}

/* DateTime */

static icu::DateFormat::EStyle GetICUStyle(ffi::FluentDateTimeStyle aStyle) {
  switch (aStyle) {
    case ffi::FluentDateTimeStyle::Full:
      return icu::DateFormat::FULL;
    case ffi::FluentDateTimeStyle::Long:
      return icu::DateFormat::LONG;
    case ffi::FluentDateTimeStyle::Medium:
      return icu::DateFormat::MEDIUM;
    case ffi::FluentDateTimeStyle::Short:
      return icu::DateFormat::SHORT;
    case ffi::FluentDateTimeStyle::None:
      return icu::DateFormat::NONE;
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported date time style.");
      return icu::DateFormat::NONE;
  }
}

ffi::RawDateTimeFormatter* FluentBuiltInDateTimeFormatterCreate(
    const nsCString* aLocale, const ffi::FluentDateTimeOptionsRaw* aOptions) {
  icu::DateFormat* dtmf = nullptr;
  if (aOptions->date_style != ffi::FluentDateTimeStyle::None &&
      aOptions->time_style != ffi::FluentDateTimeStyle::None) {
    dtmf = icu::DateFormat::createDateTimeInstance(
        GetICUStyle(aOptions->date_style), GetICUStyle(aOptions->time_style),
        aLocale->get());
  } else if (aOptions->date_style != ffi::FluentDateTimeStyle::None) {
    dtmf = icu::DateFormat::createDateInstance(
        GetICUStyle(aOptions->date_style), aLocale->get());
  } else if (aOptions->time_style != ffi::FluentDateTimeStyle::None) {
    dtmf = icu::DateFormat::createTimeInstance(
        GetICUStyle(aOptions->time_style), aLocale->get());
  } else {
    if (aOptions->skeleton.IsEmpty()) {
      dtmf = icu::DateFormat::createDateTimeInstance(
          icu::DateFormat::DEFAULT, icu::DateFormat::DEFAULT, aLocale->get());
    } else {
      UErrorCode status = U_ZERO_ERROR;
      dtmf = icu::DateFormat::createInstanceForSkeleton(
          aOptions->skeleton.get(), aLocale->get(), status);
    }
  }
  MOZ_RELEASE_ASSERT(dtmf, "Failed to create a format for the skeleton.");

  return reinterpret_cast<ffi::RawDateTimeFormatter*>(dtmf);
}

uint8_t* FluentBuiltInDateTimeFormatterFormat(
    const ffi::RawDateTimeFormatter* aFormatter, double input,
    uint32_t* aOutCount) {
  auto formatter = reinterpret_cast<const icu::DateFormat*>(aFormatter);
  UDate myDate = input;

  icu::UnicodeString str;
  formatter->format(myDate, str);
  return reinterpret_cast<uint8_t*>(ToNewUTF8String(
      nsDependentSubstring(str.getBuffer(), str.length()), aOutCount));
}

void FluentBuiltInDateTimeFormatterDestroy(
    ffi::RawDateTimeFormatter* aFormatter) {
  delete reinterpret_cast<icu::DateFormat*>(aFormatter);
}
}

}  // namespace intl
}  // namespace mozilla
