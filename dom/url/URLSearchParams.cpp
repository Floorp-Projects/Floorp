/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/URLSearchParams.h"

// XXX encoding_rs.h is not self-contained, this order is required
#include "mozilla/Encoding.h"
#include "encoding_rs.h"

#include <new>
#include <type_traits>
#include <utility>
#include "js/StructuredClone.h"
#include "mozilla/ArrayIterator.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/NotNull.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Record.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/dom/URLSearchParamsBinding.h"
#include "mozilla/fallible.h"
#include "nsDOMString.h"
#include "nsError.h"
#include "nsIGlobalObject.h"
#include "nsLiteralString.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsStringFlags.h"
#include "nsStringIterator.h"
#include "nsStringStream.h"
#include "nsURLHelper.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(URLSearchParams, mParent, mObserver)
NS_IMPL_CYCLE_COLLECTING_ADDREF(URLSearchParams)
NS_IMPL_CYCLE_COLLECTING_RELEASE(URLSearchParams)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(URLSearchParams)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

URLSearchParams::URLSearchParams(nsISupports* aParent,
                                 URLSearchParamsObserver* aObserver)
    : mParams(new URLParams()), mParent(aParent), mObserver(aObserver) {}

URLSearchParams::~URLSearchParams() { DeleteAll(); }

JSObject* URLSearchParams::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return URLSearchParams_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<URLSearchParams> URLSearchParams::Constructor(
    const GlobalObject& aGlobal,
    const USVStringSequenceSequenceOrUSVStringUSVStringRecordOrUSVString& aInit,
    ErrorResult& aRv) {
  RefPtr<URLSearchParams> sp =
      new URLSearchParams(aGlobal.GetAsSupports(), nullptr);

  if (aInit.IsUSVString()) {
    NS_ConvertUTF16toUTF8 input(aInit.GetAsUSVString());
    if (StringBeginsWith(input, "?"_ns)) {
      sp->ParseInput(Substring(input, 1, input.Length() - 1));
    } else {
      sp->ParseInput(input);
    }
  } else if (aInit.IsUSVStringSequenceSequence()) {
    const Sequence<Sequence<nsString>>& list =
        aInit.GetAsUSVStringSequenceSequence();
    for (uint32_t i = 0; i < list.Length(); ++i) {
      const Sequence<nsString>& item = list[i];
      if (item.Length() != 2) {
        nsPrintfCString err("Expected 2 items in pair but got %zu",
                            item.Length());
        aRv.ThrowTypeError(err);
        return nullptr;
      }
      sp->Append(item[0], item[1]);
    }
  } else if (aInit.IsUSVStringUSVStringRecord()) {
    const Record<nsString, nsString>& record =
        aInit.GetAsUSVStringUSVStringRecord();
    for (auto& entry : record.Entries()) {
      sp->Append(entry.mKey, entry.mValue);
    }
  } else {
    MOZ_CRASH("This should not happen.");
  }

  return sp.forget();
}

void URLSearchParams::ParseInput(const nsACString& aInput) {
  mParams->ParseInput(aInput);
}

uint32_t URLSearchParams::Size() const { return mParams->Length(); }

void URLSearchParams::Get(const nsAString& aName, nsString& aRetval) {
  return mParams->Get(aName, aRetval);
}

void URLSearchParams::GetAll(const nsAString& aName,
                             nsTArray<nsString>& aRetval) {
  return mParams->GetAll(aName, aRetval);
}

void URLSearchParams::Set(const nsAString& aName, const nsAString& aValue) {
  mParams->Set(aName, aValue);
  NotifyObserver();
}

void URLSearchParams::Append(const nsAString& aName, const nsAString& aValue) {
  mParams->Append(aName, aValue);
  NotifyObserver();
}

bool URLSearchParams::Has(const nsAString& aName,
                          const Optional<nsAString>& aValue) {
  if (!aValue.WasPassed()) {
    return mParams->Has(aName);
  }
  return mParams->Has(aName, aValue.Value());
}

void URLSearchParams::Delete(const nsAString& aName,
                             const Optional<nsAString>& aValue) {
  if (!aValue.WasPassed()) {
    mParams->Delete(aName);
    NotifyObserver();
    return;
  }
  mParams->Delete(aName, aValue.Value());
  NotifyObserver();
}

void URLSearchParams::DeleteAll() { mParams->DeleteAll(); }

void URLSearchParams::Serialize(nsAString& aValue) const {
  mParams->Serialize(aValue, true);
}

void URLSearchParams::NotifyObserver() {
  if (mObserver) {
    mObserver->URLSearchParamsUpdated(this);
  }
}

uint32_t URLSearchParams::GetIterableLength() const {
  return mParams->Length();
}

const nsAString& URLSearchParams::GetKeyAtIndex(uint32_t aIndex) const {
  return mParams->GetKeyAtIndex(aIndex);
}

const nsAString& URLSearchParams::GetValueAtIndex(uint32_t aIndex) const {
  return mParams->GetValueAtIndex(aIndex);
}

void URLSearchParams::Sort(ErrorResult& aRv) {
  mParams->Sort();
  NotifyObserver();
}

bool URLSearchParams::WriteStructuredClone(
    JSStructuredCloneWriter* aWriter) const {
  const uint32_t& nParams = mParams->Length();
  if (!JS_WriteUint32Pair(aWriter, nParams, 0)) {
    return false;
  }
  for (uint32_t i = 0; i < nParams; ++i) {
    if (!StructuredCloneHolder::WriteString(aWriter,
                                            mParams->GetKeyAtIndex(i)) ||
        !StructuredCloneHolder::WriteString(aWriter,
                                            mParams->GetValueAtIndex(i))) {
      return false;
    }
  }
  return true;
}

bool URLSearchParams::ReadStructuredClone(JSStructuredCloneReader* aReader) {
  MOZ_ASSERT(aReader);

  DeleteAll();

  uint32_t nParams, zero;
  nsAutoString key, value;
  if (!JS_ReadUint32Pair(aReader, &nParams, &zero) || zero != 0) {
    return false;
  }

  for (uint32_t i = 0; i < nParams; ++i) {
    if (!StructuredCloneHolder::ReadString(aReader, key) ||
        !StructuredCloneHolder::ReadString(aReader, value)) {
      return false;
    }
    Append(key, value);
  }
  return true;
}

bool URLSearchParams::WriteStructuredClone(
    JSContext* aCx, JSStructuredCloneWriter* aWriter) const {
  return WriteStructuredClone(aWriter);
}

// static
already_AddRefed<URLSearchParams> URLSearchParams::ReadStructuredClone(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    JSStructuredCloneReader* aReader) {
  RefPtr<URLSearchParams> params = new URLSearchParams(aGlobal);
  if (!params->ReadStructuredClone(aReader)) {
    return nullptr;
  }
  return params.forget();
}

// contentTypeWithCharset can be set to the contentType or
// contentType+charset based on what the spec says.
// See: https://fetch.spec.whatwg.org/#concept-bodyinit-extract
nsresult URLSearchParams::GetSendInfo(nsIInputStream** aBody,
                                      uint64_t* aContentLength,
                                      nsACString& aContentTypeWithCharset,
                                      nsACString& aCharset) const {
  aContentTypeWithCharset.AssignLiteral(
      "application/x-www-form-urlencoded;charset=UTF-8");
  aCharset.AssignLiteral("UTF-8");

  nsAutoString serialized;
  Serialize(serialized);
  NS_ConvertUTF16toUTF8 converted(serialized);
  *aContentLength = converted.Length();
  return NS_NewCStringInputStream(aBody, std::move(converted));
}

}  // namespace mozilla::dom
