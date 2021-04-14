/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* nsIVariant implementation for xpconnect. */

#include "mozilla/Range.h"

#include "xpcprivate.h"

#include "jsfriendapi.h"
#include "js/Array.h"  // JS::GetArrayLength, JS::IsArrayObject, JS::NewArrayObject
#include "js/friend/StackLimits.h"  // js::AutoCheckRecursionLimit
#include "js/friend/WindowProxy.h"  // js::ToWindowIfWindowProxy
#include "js/Wrapper.h"

using namespace JS;
using namespace mozilla;

NS_IMPL_CLASSINFO(XPCVariant, nullptr, 0, XPCVARIANT_CID)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XPCVariant)
  NS_INTERFACE_MAP_ENTRY(XPCVariant)
  NS_INTERFACE_MAP_ENTRY(nsIVariant)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_IMPL_QUERY_CLASSINFO(XPCVariant)
NS_INTERFACE_MAP_END
NS_IMPL_CI_INTERFACE_GETTER(XPCVariant, XPCVariant, nsIVariant)

NS_IMPL_CYCLE_COLLECTING_ADDREF(XPCVariant)
NS_IMPL_CYCLE_COLLECTING_RELEASE(XPCVariant)

XPCVariant::XPCVariant(JSContext* cx, const Value& aJSVal) : mJSVal(aJSVal) {
  if (!mJSVal.isPrimitive()) {
    // XXXbholley - The innerization here was from bug 638026. Blake says
    // the basic problem was that we were storing the C++ inner but the JS
    // outer, which meant that, after navigation, the JS inner could be
    // collected, which would cause us to try to recreate the JS inner at
    // some later point after teardown, which would crash. This is shouldn't
    // be a problem anymore because SetParentToWindow will do the right
    // thing, but I'm saving the cleanup here for another day. Blake thinks
    // that we should just not store the WN if we're creating a variant for
    // an outer window.
    JSObject* obj = js::ToWindowIfWindowProxy(&mJSVal.toObject());
    mJSVal = JS::ObjectValue(*obj);

    JSObject* unwrapped =
        js::CheckedUnwrapDynamic(obj, cx, /* stopAtWindowProxy = */ false);
    mReturnRawObject = !(unwrapped && IS_WN_REFLECTOR(unwrapped));
  } else {
    mReturnRawObject = false;
  }
}

XPCTraceableVariant::~XPCTraceableVariant() {
  Value val = GetJSValPreserveColor();

  MOZ_ASSERT(val.isGCThing() || val.isNull(), "Must be traceable or unlinked");
  bool unroot = val.isGCThing();

  mData.Cleanup();

  if (unroot) {
    RemoveFromRootSet();
  }
}

void XPCTraceableVariant::TraceJS(JSTracer* trc) {
  MOZ_ASSERT(GetJSValPreserveColor().isGCThing());
  JS::TraceEdge(trc, &mJSVal, "XPCTraceableVariant::mJSVal");
}

NS_IMPL_CYCLE_COLLECTION_CLASS(XPCVariant)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(XPCVariant)
  JS::Value val = tmp->GetJSValPreserveColor();
  if (val.isObject()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mJSVal");
    cb.NoteJSChild(val.toGCCellPtr());
  }

  tmp->mData.Traverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(XPCVariant)
  JS::Value val = tmp->GetJSValPreserveColor();
  bool unroot = val.isGCThing();

  tmp->mData.Cleanup();

  if (unroot) {
    XPCTraceableVariant* v = static_cast<XPCTraceableVariant*>(tmp);
    v->RemoveFromRootSet();
  }
  tmp->mJSVal = JS::NullValue();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

// static
already_AddRefed<XPCVariant> XPCVariant::newVariant(JSContext* cx,
                                                    const Value& aJSVal) {
  RefPtr<XPCVariant> variant;

  if (!aJSVal.isGCThing()) {
    variant = new XPCVariant(cx, aJSVal);
  } else {
    variant = new XPCTraceableVariant(cx, aJSVal);
  }

  if (!variant->InitializeData(cx)) {
    return nullptr;
  }

  return variant.forget();
}

// Helper class to give us a namespace for the table based code below.
class XPCArrayHomogenizer {
 private:
  enum Type {
    tNull = 0,  // null value
    tInt,       // Integer
    tDbl,       // Double
    tBool,      // Boolean
    tStr,       // String
    tID,        // ID
    tArr,       // Array
    tISup,      // nsISupports (really just a plain JSObject)
    tUnk,       // Unknown. Used only for initial state.

    tTypeCount,  // Just a count for table dimensioning.

    tVar,  // nsVariant - last ditch if no other common type found.
    tErr   // No valid state or type has this value.
  };

  // Table has tUnk as a state (column) but not as a type (row).
  static const Type StateTable[tTypeCount][tTypeCount - 1];

 public:
  static bool GetTypeForArray(JSContext* cx, HandleObject array,
                              uint32_t length, nsXPTType* resultType,
                              nsID* resultID);
};

// Current state is the column down the side.
// Current type is the row along the top.
// New state is in the box at the intersection.

const XPCArrayHomogenizer::Type
    XPCArrayHomogenizer::StateTable[tTypeCount][tTypeCount - 1] = {
        /*          tNull,tInt ,tDbl ,tBool,tStr ,tID  ,tArr ,tISup */
        /* tNull */ {tNull, tVar, tVar, tVar, tStr, tID, tVar, tISup},
        /* tInt  */ {tVar, tInt, tDbl, tVar, tVar, tVar, tVar, tVar},
        /* tDbl  */ {tVar, tDbl, tDbl, tVar, tVar, tVar, tVar, tVar},
        /* tBool */ {tVar, tVar, tVar, tBool, tVar, tVar, tVar, tVar},
        /* tStr  */ {tStr, tVar, tVar, tVar, tStr, tVar, tVar, tVar},
        /* tID   */ {tID, tVar, tVar, tVar, tVar, tID, tVar, tVar},
        /* tArr  */ {tErr, tErr, tErr, tErr, tErr, tErr, tErr, tErr},
        /* tISup */ {tISup, tVar, tVar, tVar, tVar, tVar, tVar, tISup},
        /* tUnk  */ {tNull, tInt, tDbl, tBool, tStr, tID, tVar, tISup}};

// static
bool XPCArrayHomogenizer::GetTypeForArray(JSContext* cx, HandleObject array,
                                          uint32_t length,
                                          nsXPTType* resultType,
                                          nsID* resultID) {
  Type state = tUnk;
  Type type;

  RootedValue val(cx);
  RootedObject jsobj(cx);
  for (uint32_t i = 0; i < length; i++) {
    if (!JS_GetElement(cx, array, i, &val)) {
      return false;
    }

    if (val.isInt32()) {
      type = tInt;
    } else if (val.isDouble()) {
      type = tDbl;
    } else if (val.isBoolean()) {
      type = tBool;
    } else if (val.isUndefined() || val.isSymbol() || val.isBigInt()) {
      state = tVar;
      break;
    } else if (val.isNull()) {
      type = tNull;
    } else if (val.isString()) {
      type = tStr;
    } else {
      MOZ_RELEASE_ASSERT(val.isObject(), "invalid type of jsval!");
      jsobj = &val.toObject();

      bool isArray;
      if (!JS::IsArrayObject(cx, jsobj, &isArray)) {
        return false;
      }

      if (isArray) {
        type = tArr;
      } else if (xpc::JSValue2ID(cx, val)) {
        type = tID;
      } else {
        type = tISup;
      }
    }

    MOZ_ASSERT(state != tErr, "bad state table!");
    MOZ_ASSERT(type != tErr, "bad type!");
    MOZ_ASSERT(type != tVar, "bad type!");
    MOZ_ASSERT(type != tUnk, "bad type!");

    state = StateTable[state][type];

    MOZ_ASSERT(state != tErr, "bad state table!");
    MOZ_ASSERT(state != tUnk, "bad state table!");

    if (state == tVar) {
      break;
    }
  }

  switch (state) {
    case tInt:
      *resultType = nsXPTType::MkArrayType(nsXPTType::Idx::INT32);
      break;
    case tDbl:
      *resultType = nsXPTType::MkArrayType(nsXPTType::Idx::DOUBLE);
      break;
    case tBool:
      *resultType = nsXPTType::MkArrayType(nsXPTType::Idx::BOOL);
      break;
    case tStr:
      *resultType = nsXPTType::MkArrayType(nsXPTType::Idx::PWSTRING);
      break;
    case tID:
      *resultType = nsXPTType::MkArrayType(nsXPTType::Idx::NSIDPTR);
      break;
    case tISup:
      *resultType = nsXPTType::MkArrayType(nsXPTType::Idx::INTERFACE_IS_TYPE);
      *resultID = NS_GET_IID(nsISupports);
      break;
    case tNull:
      // FALL THROUGH
    case tVar:
      *resultType = nsXPTType::MkArrayType(nsXPTType::Idx::INTERFACE_IS_TYPE);
      *resultID = NS_GET_IID(nsIVariant);
      break;
    case tArr:
      // FALL THROUGH
    case tUnk:
      // FALL THROUGH
    case tErr:
      // FALL THROUGH
    default:
      NS_ERROR("bad state");
      return false;
  }
  return true;
}

bool XPCVariant::InitializeData(JSContext* cx) {
  js::AutoCheckRecursionLimit recursion(cx);
  if (!recursion.check(cx)) {
    return false;
  }

  RootedValue val(cx, GetJSVal());

  if (val.isInt32()) {
    mData.SetFromInt32(val.toInt32());
    return true;
  }
  if (val.isDouble()) {
    mData.SetFromDouble(val.toDouble());
    return true;
  }
  if (val.isBoolean()) {
    mData.SetFromBool(val.toBoolean());
    return true;
  }
  // We can't represent symbol or BigInt on C++ side, so pretend it is void.
  if (val.isUndefined() || val.isSymbol() || val.isBigInt()) {
    mData.SetToVoid();
    return true;
  }
  if (val.isNull()) {
    mData.SetToEmpty();
    return true;
  }
  if (val.isString()) {
    RootedString str(cx, val.toString());
    if (!str) {
      return false;
    }

    MOZ_ASSERT(mData.GetType() == nsIDataType::VTYPE_EMPTY,
               "Why do we already have data?");

    size_t length = JS_GetStringLength(str);
    mData.AllocateWStringWithSize(length);

    mozilla::Range<char16_t> destChars(mData.u.wstr.mWStringValue, length);
    if (!JS_CopyStringChars(cx, destChars, str)) {
      return false;
    }

    MOZ_ASSERT(mData.u.wstr.mWStringValue[length] == '\0');
    return true;
  }
  if (Maybe<nsID> id = xpc::JSValue2ID(cx, val)) {
    mData.SetFromID(id.ref());
    return true;
  }

  // leaving only JSObject...
  MOZ_RELEASE_ASSERT(val.isObject(), "invalid type of jsval!");

  RootedObject jsobj(cx, &val.toObject());

  // Let's see if it is a js array object.

  uint32_t len;

  bool isArray;
  if (!JS::IsArrayObject(cx, jsobj, &isArray) ||
      (isArray && !JS::GetArrayLength(cx, jsobj, &len))) {
    return false;
  }

  if (isArray) {
    if (!len) {
      // Zero length array
      mData.SetToEmptyArray();
      return true;
    }

    nsXPTType type;
    nsID id;

    if (!XPCArrayHomogenizer::GetTypeForArray(cx, jsobj, len, &type, &id)) {
      return false;
    }

    if (!XPCConvert::JSData2Native(cx, &mData.u.array.mArrayValue, val, type,
                                   &id, len, nullptr))
      return false;

    const nsXPTType& elty = type.ArrayElementType();
    mData.mType = nsIDataType::VTYPE_ARRAY;
    if (elty.IsInterfacePointer()) {
      mData.u.array.mArrayInterfaceID = id;
    }
    mData.u.array.mArrayCount = len;
    mData.u.array.mArrayType = elty.Tag();

    return true;
  }

  // XXX This could be smarter and pick some more interesting iface.

  nsIXPConnect* xpc = nsIXPConnect::XPConnect();
  nsCOMPtr<nsISupports> wrapper;
  const nsIID& iid = NS_GET_IID(nsISupports);

  if (NS_FAILED(xpc->WrapJS(cx, jsobj, iid, getter_AddRefs(wrapper)))) {
    return false;
  }

  mData.SetFromInterface(iid, wrapper);
  return true;
}

NS_IMETHODIMP
XPCVariant::GetAsJSVal(MutableHandleValue result) {
  result.set(GetJSVal());
  return NS_OK;
}

// static
bool XPCVariant::VariantDataToJS(JSContext* cx, nsIVariant* variant,
                                 nsresult* pErr, MutableHandleValue pJSVal) {
  // Get the type early because we might need to spoof it below.
  uint16_t type = variant->GetDataType();

  RootedValue realVal(cx);
  nsresult rv = variant->GetAsJSVal(&realVal);

  if (NS_SUCCEEDED(rv) &&
      (realVal.isPrimitive() || type == nsIDataType::VTYPE_ARRAY ||
       type == nsIDataType::VTYPE_EMPTY_ARRAY ||
       type == nsIDataType::VTYPE_ID)) {
    if (!JS_WrapValue(cx, &realVal)) {
      return false;
    }
    pJSVal.set(realVal);
    return true;
  }

  nsCOMPtr<XPCVariant> xpcvariant = do_QueryInterface(variant);
  if (xpcvariant && xpcvariant->mReturnRawObject) {
    MOZ_ASSERT(type == nsIDataType::VTYPE_INTERFACE ||
                   type == nsIDataType::VTYPE_INTERFACE_IS,
               "Weird variant");

    if (!JS_WrapValue(cx, &realVal)) {
      return false;
    }
    pJSVal.set(realVal);
    return true;
  }

  // else, it's an object and we really need to double wrap it if we've
  // already decided that its 'natural' type is as some sort of interface.

  // We just fall through to the code below and let it do what it does.

  // The nsIVariant is not a XPCVariant (or we act like it isn't).
  // So we extract the data and do the Right Thing.

  // We ASSUME that the variant implementation can do these conversions...

  nsID iid;

  switch (type) {
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_INT16:
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_INT64:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_UINT16:
    case nsIDataType::VTYPE_UINT32:
    case nsIDataType::VTYPE_UINT64:
    case nsIDataType::VTYPE_FLOAT:
    case nsIDataType::VTYPE_DOUBLE: {
      double d;
      if (NS_FAILED(variant->GetAsDouble(&d))) {
        return false;
      }
      pJSVal.setNumber(d);
      return true;
    }
    case nsIDataType::VTYPE_BOOL: {
      bool b;
      if (NS_FAILED(variant->GetAsBool(&b))) {
        return false;
      }
      pJSVal.setBoolean(b);
      return true;
    }
    case nsIDataType::VTYPE_CHAR: {
      char c;
      if (NS_FAILED(variant->GetAsChar(&c))) {
        return false;
      }
      return XPCConvert::NativeData2JS(cx, pJSVal, (const void*)&c, {TD_CHAR},
                                       &iid, 0, pErr);
    }
    case nsIDataType::VTYPE_WCHAR: {
      char16_t wc;
      if (NS_FAILED(variant->GetAsWChar(&wc))) {
        return false;
      }
      return XPCConvert::NativeData2JS(cx, pJSVal, (const void*)&wc, {TD_WCHAR},
                                       &iid, 0, pErr);
    }
    case nsIDataType::VTYPE_ID: {
      if (NS_FAILED(variant->GetAsID(&iid))) {
        return false;
      }
      nsID* v = &iid;
      return XPCConvert::NativeData2JS(cx, pJSVal, (const void*)&v,
                                       {TD_NSIDPTR}, &iid, 0, pErr);
    }
    case nsIDataType::VTYPE_ASTRING: {
      nsAutoString astring;
      if (NS_FAILED(variant->GetAsAString(astring))) {
        return false;
      }
      return XPCConvert::NativeData2JS(cx, pJSVal, &astring, {TD_ASTRING}, &iid,
                                       0, pErr);
    }
    case nsIDataType::VTYPE_CSTRING: {
      nsAutoCString cString;
      if (NS_FAILED(variant->GetAsACString(cString))) {
        return false;
      }
      return XPCConvert::NativeData2JS(cx, pJSVal, &cString, {TD_CSTRING}, &iid,
                                       0, pErr);
    }
    case nsIDataType::VTYPE_UTF8STRING: {
      nsUTF8String utf8String;
      if (NS_FAILED(variant->GetAsAUTF8String(utf8String))) {
        return false;
      }
      return XPCConvert::NativeData2JS(cx, pJSVal, &utf8String, {TD_UTF8STRING},
                                       &iid, 0, pErr);
    }
    case nsIDataType::VTYPE_CHAR_STR: {
      char* pc;
      if (NS_FAILED(variant->GetAsString(&pc))) {
        return false;
      }
      bool success = XPCConvert::NativeData2JS(cx, pJSVal, (const void*)&pc,
                                               {TD_PSTRING}, &iid, 0, pErr);
      free(pc);
      return success;
    }
    case nsIDataType::VTYPE_STRING_SIZE_IS: {
      char* pc;
      uint32_t size;
      if (NS_FAILED(variant->GetAsStringWithSize(&size, &pc))) {
        return false;
      }
      bool success = XPCConvert::NativeData2JS(
          cx, pJSVal, (const void*)&pc, {TD_PSTRING_SIZE_IS}, &iid, size, pErr);
      free(pc);
      return success;
    }
    case nsIDataType::VTYPE_WCHAR_STR: {
      char16_t* pwc;
      if (NS_FAILED(variant->GetAsWString(&pwc))) {
        return false;
      }
      bool success = XPCConvert::NativeData2JS(cx, pJSVal, (const void*)&pwc,
                                               {TD_PSTRING}, &iid, 0, pErr);
      free(pwc);
      return success;
    }
    case nsIDataType::VTYPE_WSTRING_SIZE_IS: {
      char16_t* pwc;
      uint32_t size;
      if (NS_FAILED(variant->GetAsWStringWithSize(&size, &pwc))) {
        return false;
      }
      bool success =
          XPCConvert::NativeData2JS(cx, pJSVal, (const void*)&pwc,
                                    {TD_PWSTRING_SIZE_IS}, &iid, size, pErr);
      free(pwc);
      return success;
    }
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS: {
      nsISupports* pi;
      nsID* piid;
      if (NS_FAILED(variant->GetAsInterface(&piid, (void**)&pi))) {
        return false;
      }

      iid = *piid;
      free((char*)piid);

      bool success = XPCConvert::NativeData2JS(
          cx, pJSVal, (const void*)&pi, {TD_INTERFACE_IS_TYPE}, &iid, 0, pErr);
      if (pi) {
        pi->Release();
      }
      return success;
    }
    case nsIDataType::VTYPE_ARRAY: {
      nsDiscriminatedUnion du;
      nsresult rv;

      rv = variant->GetAsArray(
          &du.u.array.mArrayType, &du.u.array.mArrayInterfaceID,
          &du.u.array.mArrayCount, &du.u.array.mArrayValue);
      if (NS_FAILED(rv)) {
        return false;
      }

      // must exit via VARIANT_DONE from here on...
      du.mType = nsIDataType::VTYPE_ARRAY;

      uint16_t elementType = du.u.array.mArrayType;
      const nsID* pid = nullptr;

      nsXPTType::Idx xptIndex;
      switch (elementType) {
        case nsIDataType::VTYPE_INT8:
          xptIndex = nsXPTType::Idx::INT8;
          break;
        case nsIDataType::VTYPE_INT16:
          xptIndex = nsXPTType::Idx::INT16;
          break;
        case nsIDataType::VTYPE_INT32:
          xptIndex = nsXPTType::Idx::INT32;
          break;
        case nsIDataType::VTYPE_INT64:
          xptIndex = nsXPTType::Idx::INT64;
          break;
        case nsIDataType::VTYPE_UINT8:
          xptIndex = nsXPTType::Idx::UINT8;
          break;
        case nsIDataType::VTYPE_UINT16:
          xptIndex = nsXPTType::Idx::UINT16;
          break;
        case nsIDataType::VTYPE_UINT32:
          xptIndex = nsXPTType::Idx::UINT32;
          break;
        case nsIDataType::VTYPE_UINT64:
          xptIndex = nsXPTType::Idx::UINT64;
          break;
        case nsIDataType::VTYPE_FLOAT:
          xptIndex = nsXPTType::Idx::FLOAT;
          break;
        case nsIDataType::VTYPE_DOUBLE:
          xptIndex = nsXPTType::Idx::DOUBLE;
          break;
        case nsIDataType::VTYPE_BOOL:
          xptIndex = nsXPTType::Idx::BOOL;
          break;
        case nsIDataType::VTYPE_CHAR:
          xptIndex = nsXPTType::Idx::CHAR;
          break;
        case nsIDataType::VTYPE_WCHAR:
          xptIndex = nsXPTType::Idx::WCHAR;
          break;
        case nsIDataType::VTYPE_ID:
          xptIndex = nsXPTType::Idx::NSIDPTR;
          break;
        case nsIDataType::VTYPE_CHAR_STR:
          xptIndex = nsXPTType::Idx::PSTRING;
          break;
        case nsIDataType::VTYPE_WCHAR_STR:
          xptIndex = nsXPTType::Idx::PWSTRING;
          break;
        case nsIDataType::VTYPE_INTERFACE:
          pid = &NS_GET_IID(nsISupports);
          xptIndex = nsXPTType::Idx::INTERFACE_IS_TYPE;
          break;
        case nsIDataType::VTYPE_INTERFACE_IS:
          pid = &du.u.array.mArrayInterfaceID;
          xptIndex = nsXPTType::Idx::INTERFACE_IS_TYPE;
          break;

        // The rest are illegal.
        case nsIDataType::VTYPE_VOID:
        case nsIDataType::VTYPE_ASTRING:
        case nsIDataType::VTYPE_CSTRING:
        case nsIDataType::VTYPE_UTF8STRING:
        case nsIDataType::VTYPE_WSTRING_SIZE_IS:
        case nsIDataType::VTYPE_STRING_SIZE_IS:
        case nsIDataType::VTYPE_ARRAY:
        case nsIDataType::VTYPE_EMPTY_ARRAY:
        case nsIDataType::VTYPE_EMPTY:
        default:
          NS_ERROR("bad type in array!");
          return false;
      }

      bool success = XPCConvert::NativeData2JS(
          cx, pJSVal, (const void*)&du.u.array.mArrayValue,
          nsXPTType::MkArrayType(xptIndex), pid, du.u.array.mArrayCount, pErr);

      return success;
    }
    case nsIDataType::VTYPE_EMPTY_ARRAY: {
      JSObject* array = JS::NewArrayObject(cx, 0);
      if (!array) {
        return false;
      }
      pJSVal.setObject(*array);
      return true;
    }
    case nsIDataType::VTYPE_VOID:
      pJSVal.setUndefined();
      return true;
    case nsIDataType::VTYPE_EMPTY:
      pJSVal.setNull();
      return true;
    default:
      NS_ERROR("bad type in variant!");
      return false;
  }
}

/***************************************************************************/
/***************************************************************************/
// XXX These default implementations need to be improved to allow for
// some more interesting conversions.

uint16_t XPCVariant::GetDataType() { return mData.GetType(); }

NS_IMETHODIMP XPCVariant::GetAsInt8(uint8_t* _retval) {
  return mData.ConvertToInt8(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsInt16(int16_t* _retval) {
  return mData.ConvertToInt16(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsInt32(int32_t* _retval) {
  return mData.ConvertToInt32(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsInt64(int64_t* _retval) {
  return mData.ConvertToInt64(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsUint8(uint8_t* _retval) {
  return mData.ConvertToUint8(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsUint16(uint16_t* _retval) {
  return mData.ConvertToUint16(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsUint32(uint32_t* _retval) {
  return mData.ConvertToUint32(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsUint64(uint64_t* _retval) {
  return mData.ConvertToUint64(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsFloat(float* _retval) {
  return mData.ConvertToFloat(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsDouble(double* _retval) {
  return mData.ConvertToDouble(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsBool(bool* _retval) {
  return mData.ConvertToBool(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsChar(char* _retval) {
  return mData.ConvertToChar(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsWChar(char16_t* _retval) {
  return mData.ConvertToWChar(_retval);
}

NS_IMETHODIMP_(nsresult) XPCVariant::GetAsID(nsID* retval) {
  return mData.ConvertToID(retval);
}

NS_IMETHODIMP XPCVariant::GetAsAString(nsAString& _retval) {
  return mData.ConvertToAString(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsACString(nsACString& _retval) {
  return mData.ConvertToACString(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsAUTF8String(nsAUTF8String& _retval) {
  return mData.ConvertToAUTF8String(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsString(char** _retval) {
  return mData.ConvertToString(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsWString(char16_t** _retval) {
  return mData.ConvertToWString(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsISupports(nsISupports** _retval) {
  return mData.ConvertToISupports(_retval);
}

NS_IMETHODIMP XPCVariant::GetAsInterface(nsIID** iid, void** iface) {
  return mData.ConvertToInterface(iid, iface);
}

NS_IMETHODIMP_(nsresult)
XPCVariant::GetAsArray(uint16_t* type, nsIID* iid, uint32_t* count,
                       void** ptr) {
  return mData.ConvertToArray(type, iid, count, ptr);
}

NS_IMETHODIMP XPCVariant::GetAsStringWithSize(uint32_t* size, char** str) {
  return mData.ConvertToStringWithSize(size, str);
}

NS_IMETHODIMP XPCVariant::GetAsWStringWithSize(uint32_t* size, char16_t** str) {
  return mData.ConvertToWStringWithSize(size, str);
}
