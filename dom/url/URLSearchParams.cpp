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
    const UTF8StringSequenceSequenceOrUTF8StringUTF8StringRecordOrUTF8String&
        aInit,
    ErrorResult& aRv) {
  RefPtr<URLSearchParams> sp =
      new URLSearchParams(aGlobal.GetAsSupports(), nullptr);

  if (aInit.IsUTF8String()) {
    const auto& input = aInit.GetAsUTF8String();
    if (StringBeginsWith(input, "?"_ns)) {
      sp->ParseInput(Substring(input, 1, input.Length() - 1));
    } else {
      sp->ParseInput(input);
    }
  } else if (aInit.IsUTF8StringSequenceSequence()) {
    const Sequence<Sequence<nsCString>>& list =
        aInit.GetAsUTF8StringSequenceSequence();
    for (uint32_t i = 0; i < list.Length(); ++i) {
      const Sequence<nsCString>& item = list[i];
      if (item.Length() != 2) {
        nsPrintfCString err("Expected 2 items in pair but got %zu",
                            item.Length());
        aRv.ThrowTypeError(err);
        return nullptr;
      }
      sp->Append(item[0], item[1]);
    }
  } else if (aInit.IsUTF8StringUTF8StringRecord()) {
    const Record<nsCString, nsCString>& record =
        aInit.GetAsUTF8StringUTF8StringRecord();
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

void URLSearchParams::Get(const nsACString& aName, nsACString& aRetval) {
  return mParams->Get(aName, aRetval);
}

void URLSearchParams::GetAll(const nsACString& aName,
                             nsTArray<nsCString>& aRetval) {
  return mParams->GetAll(aName, aRetval);
}

void URLSearchParams::Set(const nsACString& aName, const nsACString& aValue) {
  mParams->Set(aName, aValue);
  NotifyObserver();
}

void URLSearchParams::Append(const nsACString& aName,
                             const nsACString& aValue) {
  mParams->Append(aName, aValue);
  NotifyObserver();
}

bool URLSearchParams::Has(const nsACString& aName,
                          const Optional<nsACString>& aValue) {
  if (!aValue.WasPassed()) {
    return mParams->Has(aName);
  }
  return mParams->Has(aName, aValue.Value());
}

void URLSearchParams::Delete(const nsACString& aName,
                             const Optional<nsACString>& aValue) {
  if (!aValue.WasPassed()) {
    mParams->Delete(aName);
    NotifyObserver();
    return;
  }
  mParams->Delete(aName, aValue.Value());
  NotifyObserver();
}

void URLSearchParams::DeleteAll() { mParams->DeleteAll(); }

void URLSearchParams::Serialize(nsACString& aValue) const {
  mParams->Serialize(aValue, true);
}

// TODO(emilio): Allow stringifier attributes with CString return values.
void URLSearchParams::Stringify(nsAString& aValue) const {
  nsAutoCString serialized;
  mParams->Serialize(serialized, true);
  CopyUTF8toUTF16(serialized, aValue);
}

void URLSearchParams::NotifyObserver() {
  if (mObserver) {
    mObserver->URLSearchParamsUpdated(this);
  }
}

uint32_t URLSearchParams::GetIterableLength() const {
  return mParams->Length();
}

const nsACString& URLSearchParams::GetKeyAtIndex(uint32_t aIndex) const {
  return mParams->GetKeyAtIndex(aIndex);
}

const nsACString& URLSearchParams::GetValueAtIndex(uint32_t aIndex) const {
  return mParams->GetValueAtIndex(aIndex);
}

void URLSearchParams::Sort(ErrorResult& aRv) {
  mParams->Sort();
  NotifyObserver();
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

  nsAutoCString serialized;
  Serialize(serialized);
  *aContentLength = serialized.Length();
  return NS_NewCStringInputStream(aBody, std::move(serialized));
}

}  // namespace mozilla::dom
