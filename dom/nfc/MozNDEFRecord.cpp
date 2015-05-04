/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright © 2013 Deutsche Telekom, Inc. */

#include "MozNDEFRecord.h"
#include "js/StructuredClone.h"
#include "mozilla/dom/MozNDEFRecordBinding.h"
#include "mozilla/HoldDropJSObjects.h"
#include "nsContentUtils.h"
#include "nsIGlobalObject.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MozNDEFRecord)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MozNDEFRecord)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->DropData();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(MozNDEFRecord)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
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
/* static */ bool
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

/* static */ already_AddRefed<MozNDEFRecord>
MozNDEFRecord::Constructor(const GlobalObject& aGlobal,
                           const MozNDEFRecordOptions& aOptions,
                           ErrorResult& aRv)
{
  if (!ValidateTNF(aOptions, aRv)) {
    return nullptr;
  }

  nsCOMPtr<nsISupports> parent = do_QueryInterface(aGlobal.GetAsSupports());
  if (!parent) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  JSContext* context = aGlobal.Context();
  nsRefPtr<MozNDEFRecord> ndefRecord = new MozNDEFRecord(parent, aOptions.mTnf);
  ndefRecord->InitType(context, aOptions.mType);
  ndefRecord->InitId(context, aOptions.mId);
  ndefRecord->InitPayload(context, aOptions.mPayload);

  return ndefRecord.forget();
}

/* static */ already_AddRefed<MozNDEFRecord>
MozNDEFRecord::Constructor(const GlobalObject& aGlobal,
                           const nsAString& aUri,
                           ErrorResult& aRv)
{
  if (aUri.IsVoid()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsISupports> parent = do_QueryInterface(aGlobal.GetAsSupports());
  if (!parent) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<MozNDEFRecord> ndefRecord = new MozNDEFRecord(parent, TNF::Well_known);
  ndefRecord->InitType(aGlobal.Context(), RTD::U);
  ndefRecord->InitPayload(aGlobal.Context(), aUri);
  return ndefRecord.forget();
}

MozNDEFRecord::MozNDEFRecord(nsISupports* aParent, TNF aTnf)
  : mParent(aParent) // For GetParentObject()
  , mTnf(aTnf)
  , mSize(3) // 1(flags) + 1(type_length) + 1(payload_length)
{
  HoldData();
}

void
MozNDEFRecord::GetAsURI(nsAString& aRetVal)
{
  aRetVal.SetIsVoid(true);
  if (mTnf != TNF::Well_known) {
    return;
  }

  JS::AutoCheckCannotGC nogc;
  uint8_t* typeData = JS_GetUint8ArrayData(mType, nogc);
  const char* uVal = RTDValues::strings[static_cast<uint32_t>(RTD::U)].value;
  if (typeData[0] != uVal[0]) {
    return;
  }

  uint32_t payloadLen;
  uint8_t* payloadData;
  js::GetUint8ArrayLengthAndData(mPayload, &payloadLen, &payloadData);
  uint8_t id = payloadData[0];
  if (id >= static_cast<uint8_t>(WellKnownURIPrefix::EndGuard_)) {
    return;
  }

  using namespace mozilla::dom::WellKnownURIPrefixValues;
  aRetVal.AssignASCII(strings[id].value);
  aRetVal.Append(NS_ConvertUTF8toUTF16(
    nsDependentCSubstring(reinterpret_cast<char*>(&payloadData[1]), payloadLen - 1)));
}

bool
MozNDEFRecord::WriteStructuredClone(JSContext* aCx, JSStructuredCloneWriter* aWriter) const
{
  uint8_t* dummy;
  uint32_t typeLen = 0, idLen = 0, payloadLen = 0;
  if (mType) {
    js::GetUint8ArrayLengthAndData(mType, &typeLen, &dummy);
  }

  if (mId) {
    js::GetUint8ArrayLengthAndData(mId, &idLen, &dummy);
  }

  if (mPayload) {
    js::GetUint8ArrayLengthAndData(mPayload, &payloadLen, &dummy);
  }

  return JS_WriteUint32Pair(aWriter, static_cast<uint32_t>(mTnf), typeLen) &&
         JS_WriteUint32Pair(aWriter, idLen, payloadLen) &&
         WriteUint8Array(aCx, aWriter, mType, typeLen) &&
         WriteUint8Array(aCx, aWriter, mId, idLen) &&
         WriteUint8Array(aCx, aWriter, mPayload, payloadLen);
}

bool
MozNDEFRecord::ReadStructuredClone(JSContext* aCx, JSStructuredCloneReader* aReader)
{
  uint32_t tnf, typeLen, idLen, payloadLen;

  if (!JS_ReadUint32Pair(aReader, &tnf, &typeLen) ||
      !JS_ReadUint32Pair(aReader, &idLen, &payloadLen)) {
    return false;
  }

  mTnf = static_cast<TNF>(tnf);

  if (typeLen) {
    JS::Rooted<JS::Value> value(aCx);
    if (!JS_ReadTypedArray(aReader, &value)) {
      return false;
    }
    MOZ_ASSERT(value.isObject());
    InitType(aCx, value.toObject(), typeLen);
  }

  if (idLen) {
    JS::Rooted<JS::Value> value(aCx);
    if (!JS_ReadTypedArray(aReader, &value)) {
      return false;
    }
    MOZ_ASSERT(value.isObject());
    InitId(aCx, value.toObject(), idLen);
  }

  if (payloadLen) {
    JS::Rooted<JS::Value> value(aCx);
    if (!JS_ReadTypedArray(aReader, &value)) {
      return false;
    }
    MOZ_ASSERT(value.isObject());
    InitPayload(aCx, value.toObject(), payloadLen);
  }

  return true;
}

void
MozNDEFRecord::InitType(JSContext* aCx, const Optional<Nullable<Uint8Array>>& aType)
{
  if (!aType.WasPassed() || aType.Value().IsNull()) {
    return;
  }

  const Uint8Array& type = aType.Value().Value();
  type.ComputeLengthAndData();
  mType = Uint8Array::Create(aCx, this, type.Length(), type.Data());
  IncSize(type.Length());
}

void
MozNDEFRecord::InitType(JSContext* aCx, RTD rtd)
{
  EnumEntry rtdType = RTDValues::strings[static_cast<uint32_t>(rtd)];
  mType = Uint8Array::Create(aCx, rtdType.length,
                             reinterpret_cast<const uint8_t*>(rtdType.value));
  IncSize(rtdType.length);
}

void
MozNDEFRecord::InitType(JSContext* aCx, JSObject& aType, uint32_t aLen)
{
  mType = &aType;
  IncSize(aLen);
}

void
MozNDEFRecord::InitId(JSContext* aCx, const Optional<Nullable<Uint8Array>>& aId)
{
  if (!aId.WasPassed() || aId.Value().IsNull()) {
    return;
  }

  const Uint8Array& id = aId.Value().Value();
  id.ComputeLengthAndData();
  mId = Uint8Array::Create(aCx, this, id.Length(), id.Data());
  IncSize(1 /* id_length */ + id.Length());
}

void
MozNDEFRecord::InitId(JSContext* aCx, JSObject& aId, uint32_t aLen)
{
  mId = &aId;
  IncSize(1 /* id_length */ + aLen);
}

void
MozNDEFRecord::InitPayload(JSContext* aCx, const Optional<Nullable<Uint8Array>>& aPayload)
{
  if (!aPayload.WasPassed() || aPayload.Value().IsNull()) {
    return;
  }

  const Uint8Array& payload = aPayload.Value().Value();
  payload.ComputeLengthAndData();
  mPayload = Uint8Array::Create(aCx, this, payload.Length(), payload.Data());
  IncSizeForPayload(payload.Length());
}

void
MozNDEFRecord::InitPayload(JSContext* aCx, const nsAString& aUri)
{
  using namespace mozilla::dom::WellKnownURIPrefixValues;

  nsCString uri = NS_ConvertUTF16toUTF8(aUri);
  uint8_t id = GetURIIdentifier(uri);
  uri = Substring(uri, strings[id].length);
  mPayload = Uint8Array::Create(aCx, this, uri.Length() + 1);

  JS::AutoCheckCannotGC nogc;
  uint8_t* data = JS_GetUint8ArrayData(mPayload, nogc);
  data[0] = id;
  memcpy(&data[1], reinterpret_cast<const uint8_t*>(uri.Data()), uri.Length());
  IncSizeForPayload(uri.Length() + 1);
}

void
MozNDEFRecord::InitPayload(JSContext* aCx, JSObject& aPayload, uint32_t aLen)
{
  mPayload = &aPayload;
  IncSizeForPayload(aLen);
}

void
MozNDEFRecord::IncSize(uint32_t aCount)
{
  mSize += aCount;
}

void
MozNDEFRecord::IncSizeForPayload(uint32_t aLen)
{
  if (aLen > 0xff) {
    IncSize(3);
  }

  IncSize(aLen);
}

bool
MozNDEFRecord::WriteUint8Array(JSContext* aCx, JSStructuredCloneWriter* aWriter, JSObject* aObj, uint32_t aLen) const
{
  if (!aLen) {
    return true;
  }

  JS::Rooted<JSObject*> obj(aCx, aObj);
  JSAutoCompartment ac(aCx, obj);
  JS::Rooted<JS::Value> value(aCx, JS::ObjectValue(*obj));
  return JS_WriteTypedArray(aWriter, value);
}

/* static */ uint32_t
MozNDEFRecord::GetURIIdentifier(const nsCString& aUri)
{
  using namespace mozilla::dom::WellKnownURIPrefixValues;

  // strings[0] is "", so we start from 1.
  for (uint32_t i = 1; i < static_cast<uint32_t>(WellKnownURIPrefix::EndGuard_); i++) {
    if (StringBeginsWith(aUri, nsDependentCString(strings[i].value))) {
      return i;
    }
  }

  return 0;
}

MozNDEFRecord::~MozNDEFRecord()
{
  DropData();
}

JSObject*
MozNDEFRecord::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozNDEFRecordBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
