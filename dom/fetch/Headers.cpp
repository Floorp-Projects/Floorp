/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Headers.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/Preferences.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(Headers)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Headers)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Headers, mOwner)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Headers)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// static
already_AddRefed<Headers> Headers::Constructor(
    const GlobalObject& aGlobal,
    const Optional<ByteStringSequenceSequenceOrByteStringByteStringRecord>&
        aInit,
    ErrorResult& aRv) {
  RefPtr<InternalHeaders> ih = new InternalHeaders();
  RefPtr<Headers> headers = new Headers(aGlobal.GetAsSupports(), ih);

  if (!aInit.WasPassed()) {
    return headers.forget();
  }

  if (aInit.Value().IsByteStringSequenceSequence()) {
    ih->Fill(aInit.Value().GetAsByteStringSequenceSequence(), aRv);
  } else if (aInit.Value().IsByteStringByteStringRecord()) {
    ih->Fill(aInit.Value().GetAsByteStringByteStringRecord(), aRv);
  }

  if (aRv.Failed()) {
    return nullptr;
  }

  return headers.forget();
}

// static
already_AddRefed<Headers> Headers::Constructor(
    const GlobalObject& aGlobal,
    const OwningByteStringSequenceSequenceOrByteStringByteStringRecord& aInit,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  return Create(global, aInit, aRv);
}

/* static */
already_AddRefed<Headers> Headers::Create(
    nsIGlobalObject* aGlobal,
    const OwningByteStringSequenceSequenceOrByteStringByteStringRecord& aInit,
    ErrorResult& aRv) {
  RefPtr<InternalHeaders> ih = new InternalHeaders();
  RefPtr<Headers> headers = new Headers(aGlobal, ih);

  if (aInit.IsByteStringSequenceSequence()) {
    ih->Fill(aInit.GetAsByteStringSequenceSequence(), aRv);
  } else if (aInit.IsByteStringByteStringRecord()) {
    ih->Fill(aInit.GetAsByteStringByteStringRecord(), aRv);
  }

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return headers.forget();
}

JSObject* Headers::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::Headers_Binding::Wrap(aCx, this, aGivenProto);
}

Headers::~Headers() = default;

}  // namespace mozilla::dom
