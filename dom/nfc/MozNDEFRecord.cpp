/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright © 2013 Deutsche Telekom, Inc. */

#include "MozNDEFRecord.h"
#include "mozilla/dom/MozNDEFRecordBinding.h"
#include "mozilla/HoldDropJSObjects.h"
#include "nsContentUtils.h"


namespace mozilla {
namespace dom {


NS_IMPL_CYCLE_COLLECTION_CLASS(MozNDEFRecord)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MozNDEFRecord)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->DropData();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(MozNDEFRecord)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(MozNDEFRecord)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mType)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mId)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mPayload)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MozNDEFRecord)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MozNDEFRecord)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MozNDEFRecord)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

void
MozNDEFRecord::HoldData()
{
  mozilla::HoldJSObjects(this);
}

void
MozNDEFRecord::DropData()
{
  if (mType) {
    mType = nullptr;
  }
  if (mId) {
    mId = nullptr;
  }
  if (mPayload) {
    mPayload = nullptr;
  }
  mozilla::DropJSObjects(this);
}

/**
 * Validate TNF.
 * See section 3.3 THE NDEF Specification Test Requirements,
 * NDEF specification 1.0
 */
/* static */
bool
MozNDEFRecord::ValidateTNF(const MozNDEFRecordOptions& aOptions,
                           ErrorResult& aRv)
{
  // * The TNF field MUST have a value between 0x00 and 0x06.
  // * The TNF value MUST NOT be 0x07.
  // These two requirements are already handled by WebIDL bindings.

  // If the TNF value is 0x00 (Empty), the TYPE, ID, and PAYLOAD fields MUST be
  // omitted from the record.
  if ((aOptions.mTnf == TNF::Empty) &&
      (aOptions.mType.WasPassed() || aOptions.mId.WasPassed() ||
       aOptions.mPayload.WasPassed())) {
    NS_WARNING("tnf is empty but type/id/payload is not null.");
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  // If the TNF value is 0x05 (Unknown) or 0x06(Unchanged), the TYPE field MUST
  // be omitted from the NDEF record.
  if ((aOptions.mTnf == TNF::Unknown || aOptions.mTnf == TNF::Unchanged) &&
      aOptions.mType.WasPassed()) {
    NS_WARNING("tnf is unknown/unchanged but type  is not null.");
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  return true;
}

/* static */
already_AddRefed<MozNDEFRecord>
MozNDEFRecord::Constructor(const GlobalObject& aGlobal,
                           const MozNDEFRecordOptions& aOptions,
                           ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aGlobal.GetAsSupports());
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (!ValidateTNF(aOptions, aRv)) {
    return nullptr;
  }

  nsRefPtr<MozNDEFRecord> ndefrecord = new MozNDEFRecord(aGlobal.Context(),
                                                         win, aOptions);
  if (!ndefrecord) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  return ndefrecord.forget();
}

MozNDEFRecord::MozNDEFRecord(JSContext* aCx, nsPIDOMWindow* aWindow,
                             const MozNDEFRecordOptions& aOptions)
{
  mWindow = aWindow; // For GetParentObject()

  mTnf = aOptions.mTnf;

  if (aOptions.mType.WasPassed()) {
    const Uint8Array& type = aOptions.mType.Value();
    type.ComputeLengthAndData();
    mType = Uint8Array::Create(aCx, this, type.Length(), type.Data());
  }

  if (aOptions.mId.WasPassed()) {
    const Uint8Array& id = aOptions.mId.Value();
    id.ComputeLengthAndData();
    mId = Uint8Array::Create(aCx, this, id.Length(), id.Data());
  }

  if (aOptions.mPayload.WasPassed()) {
    const Uint8Array& payload = aOptions.mPayload.Value();
    payload.ComputeLengthAndData();
    mPayload = Uint8Array::Create(aCx, this, payload.Length(), payload.Data());
  }

  HoldData();
}

MozNDEFRecord::~MozNDEFRecord()
{
  DropData();
}

JSObject*
MozNDEFRecord::WrapObject(JSContext* aCx)
{
  return MozNDEFRecordBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
