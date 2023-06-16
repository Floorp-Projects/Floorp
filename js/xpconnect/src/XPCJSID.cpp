/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* An xpcom implementation of the JavaScript nsIID and nsCID objects. */

#include "xpcprivate.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/Attributes.h"
#include "js/Object.h"              // JS::GetClass, JS::GetReservedSlot
#include "js/PropertyAndElement.h"  // JS_DefineFunction, JS_DefineFunctionById, JS_DefineProperty, JS_DefinePropertyById
#include "js/Symbol.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace JS;

namespace xpc {

/******************************************************************************
 * # Generic IDs #
 *
 * Generic IDs are the type of JS object created by most code which passes nsID
 * objects to JavaScript code. They provide no special methods, and only store
 * the raw nsID value.
 *
 * The nsID value is stored in 4 reserved slots, with 32 bits of the 128-bit
 * value stored in each slot. Getter code extracts this data, and combines them
 * back into the nsID value.
 */
static bool ID_Equals(JSContext* aCx, unsigned aArgc, Value* aVp);
static bool ID_GetNumber(JSContext* aCx, unsigned aArgc, Value* aVp);

// Generic ID objects contain 4 reserved slots, each containing a uint32_t with
// 1/4 of the representation of the nsID value. This allows us to avoid an extra
// allocation for the nsID object, and eliminates the need for a finalizer.
enum { kID_Slot0, kID_Slot1, kID_Slot2, kID_Slot3, kID_SlotCount };
static const JSClass sID_Class = {
    "nsJSID", JSCLASS_HAS_RESERVED_SLOTS(kID_SlotCount), JS_NULL_CLASS_OPS};

/******************************************************************************
 * # Interface IDs #
 *
 * In addition to the properties exposed by Generic ID objects, IID supports
 * 'instanceof', exposes constant properties defined on the class, and exposes
 * the interface name as the 'name' and 'toString()' values.
 */
static bool IID_HasInstance(JSContext* aCx, unsigned aArgc, Value* aVp);
static bool IID_GetName(JSContext* aCx, unsigned aArgc, Value* aVp);

static bool IID_NewEnumerate(JSContext* cx, HandleObject obj,
                             MutableHandleIdVector properties,
                             bool enumerableOnly);
static bool IID_Resolve(JSContext* cx, HandleObject obj, HandleId id,
                        bool* resolvedp);
static bool IID_MayResolve(const JSAtomState& names, jsid id,
                           JSObject* maybeObj);

static const JSClassOps sIID_ClassOps = {
    nullptr,           // addProperty
    nullptr,           // delProperty
    nullptr,           // enumerate
    IID_NewEnumerate,  // newEnumerate
    IID_Resolve,       // resolve
    IID_MayResolve,    // mayResolve
    nullptr,           // finalize
    nullptr,           // call
    nullptr,           // construct
    nullptr,           // trace
};

// Interface ID objects use a single reserved slot containing a pointer to the
// nsXPTInterfaceInfo object for the interface in question.
enum { kIID_InfoSlot, kIID_SlotCount };
static const JSClass sIID_Class = {
    "nsJSIID", JSCLASS_HAS_RESERVED_SLOTS(kIID_SlotCount), &sIID_ClassOps};

/******************************************************************************
 * # Contract IDs #
 *
 * In addition to the properties exposed by Generic ID objects, Contract IDs
 * expose 'getService' and 'createInstance' methods, and expose the contractID
 * string as '.name' and '.toString()'.
 */
static bool CID_CreateInstance(JSContext* aCx, unsigned aArgc, Value* aVp);
static bool CID_GetService(JSContext* aCx, unsigned aArgc, Value* aVp);
static bool CID_GetName(JSContext* aCx, unsigned aArgc, Value* aVp);

// ContractID objects use a single reserved slot, containing the ContractID. The
// nsCID value for this object is looked up when the object is being unwrapped.
enum { kCID_ContractSlot, kCID_SlotCount };
static const JSClass sCID_Class = {
    "nsJSCID", JSCLASS_HAS_RESERVED_SLOTS(kCID_SlotCount), JS_NULL_CLASS_OPS};

/**
 * Ensure that the nsID prototype objects have been created for the current
 * global, and extract the prototype values.
 */
static JSObject* GetIDPrototype(JSContext* aCx, const JSClass* aClass) {
  XPCWrappedNativeScope* scope = ObjectScope(CurrentGlobalOrNull(aCx));
  if (NS_WARN_IF(!scope)) {
    return nullptr;
  }

  // Create prototype objects for the JSID objects if they haven't been
  // created for this scope yet.
  if (!scope->mIDProto) {
    MOZ_ASSERT(!scope->mIIDProto && !scope->mCIDProto);

    RootedObject idProto(aCx, JS_NewPlainObject(aCx));
    RootedObject iidProto(aCx,
                          JS_NewObjectWithGivenProto(aCx, nullptr, idProto));
    RootedObject cidProto(aCx,
                          JS_NewObjectWithGivenProto(aCx, nullptr, idProto));
    RootedId hasInstance(aCx,
                         GetWellKnownSymbolKey(aCx, SymbolCode::hasInstance));

    const uint32_t kFlags =
        JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT;
    const uint32_t kNoEnum = JSPROP_READONLY | JSPROP_PERMANENT;

    bool ok =
        idProto && iidProto && cidProto &&
        // Methods and properties on all ID Objects:
        JS_DefineFunction(aCx, idProto, "equals", ID_Equals, 1, kFlags) &&
        JS_DefineProperty(aCx, idProto, "number", ID_GetNumber, nullptr,
                          kFlags) &&

        // Methods for IfaceID objects, which also inherit ID properties:
        JS_DefineFunctionById(aCx, iidProto, hasInstance, IID_HasInstance, 1,
                              kNoEnum) &&
        JS_DefineProperty(aCx, iidProto, "name", IID_GetName, nullptr,
                          kFlags) &&

        // Methods for ContractID objects, which also inherit ID properties:
        JS_DefineFunction(aCx, cidProto, "createInstance", CID_CreateInstance,
                          1, kFlags) &&
        JS_DefineFunction(aCx, cidProto, "getService", CID_GetService, 1,
                          kFlags) &&
        JS_DefineProperty(aCx, cidProto, "name", CID_GetName, nullptr,
                          kFlags) &&

        // ToString returns '.number' on generic IDs, while returning
        // '.name' on other ID types.
        JS_DefineFunction(aCx, idProto, "toString", ID_GetNumber, 0, kFlags) &&
        JS_DefineFunction(aCx, iidProto, "toString", IID_GetName, 0, kFlags) &&
        JS_DefineFunction(aCx, cidProto, "toString", CID_GetName, 0, kFlags);
    if (!ok) {
      return nullptr;
    }

    scope->mIDProto = idProto;
    scope->mIIDProto = iidProto;
    scope->mCIDProto = cidProto;
  }

  if (aClass == &sID_Class) {
    return scope->mIDProto;
  } else if (aClass == &sIID_Class) {
    return scope->mIIDProto;
  } else if (aClass == &sCID_Class) {
    return scope->mCIDProto;
  }

  MOZ_CRASH("Unrecognized ID Object Class");
}

// Unwrap the given value to an object with the correct class, or nullptr.
static JSObject* GetIDObject(HandleValue aVal, const JSClass* aClass) {
  if (aVal.isObject()) {
    // We care only about IID/CID objects here, so CheckedUnwrapStatic is fine.
    JSObject* obj = js::CheckedUnwrapStatic(&aVal.toObject());
    if (obj && JS::GetClass(obj) == aClass) {
      return obj;
    }
  }
  return nullptr;
}

static const nsXPTInterfaceInfo* GetInterfaceInfo(JSObject* obj) {
  MOZ_ASSERT(JS::GetClass(obj) == &sIID_Class);
  return static_cast<const nsXPTInterfaceInfo*>(
      JS::GetReservedSlot(obj, kIID_InfoSlot).toPrivate());
}

/**
 * Unwrap an nsID object from a JSValue.
 *
 * For Generic ID objects, this function will extract the nsID from reserved
 * slots. For IfaceID objects, it will be extracted from the nsXPTInterfaceInfo,
 * and for ContractID objects, the ContractID's corresponding CID will be looked
 * up.
 */
Maybe<nsID> JSValue2ID(JSContext* aCx, HandleValue aVal) {
  if (!aVal.isObject()) {
    return Nothing();
  }

  // We only care about ID objects here, so CheckedUnwrapStatic is fine.
  RootedObject obj(aCx, js::CheckedUnwrapStatic(&aVal.toObject()));
  if (!obj) {
    return Nothing();
  }

  mozilla::Maybe<nsID> id;
  if (JS::GetClass(obj) == &sID_Class) {
    // Extract the raw bytes of the nsID from reserved slots.
    uint32_t rawid[] = {JS::GetReservedSlot(obj, kID_Slot0).toPrivateUint32(),
                        JS::GetReservedSlot(obj, kID_Slot1).toPrivateUint32(),
                        JS::GetReservedSlot(obj, kID_Slot2).toPrivateUint32(),
                        JS::GetReservedSlot(obj, kID_Slot3).toPrivateUint32()};

    // Construct a nsID inside the Maybe, and copy the rawid into it.
    id.emplace();
    memcpy(id.ptr(), &rawid, sizeof(nsID));
  } else if (JS::GetClass(obj) == &sIID_Class) {
    // IfaceID objects store a nsXPTInterfaceInfo* pointer.
    const nsXPTInterfaceInfo* info = GetInterfaceInfo(obj);
    id.emplace(info->IID());
  } else if (JS::GetClass(obj) == &sCID_Class) {
    // ContractID objects store a ContractID string.
    JS::UniqueChars contractId = JS_EncodeStringToLatin1(
        aCx, JS::GetReservedSlot(obj, kCID_ContractSlot).toString());

    // NOTE(nika): If we directly access the nsComponentManager, we can do
    // this with a more-basic pointer lookup:
    //     nsFactoryEntry* entry = nsComponentManagerImpl::gComponentManager->
    //         GetFactoryEntry(contractId.ptr(), contractId.length());
    //     if (entry) id.emplace(entry->mCIDEntry->cid);

    nsCOMPtr<nsIComponentRegistrar> registrar;
    nsresult rv = NS_GetComponentRegistrar(getter_AddRefs(registrar));
    if (NS_FAILED(rv) || !registrar) {
      return Nothing();
    }

    nsCID* cid = nullptr;
    if (NS_SUCCEEDED(registrar->ContractIDToCID(contractId.get(), &cid))) {
      id.emplace(*cid);
      free(cid);
    }
  }
  return id;
}

/**
 * Public ID Object Constructor Methods
 */
static JSObject* NewIDObjectHelper(JSContext* aCx, const JSClass* aClass) {
  RootedObject proto(aCx, GetIDPrototype(aCx, aClass));
  if (proto) {
    return JS_NewObjectWithGivenProto(aCx, aClass, proto);
  }
  return nullptr;
}

bool ID2JSValue(JSContext* aCx, const nsID& aId, MutableHandleValue aVal) {
  RootedObject obj(aCx, NewIDObjectHelper(aCx, &sID_Class));
  if (!obj) {
    return false;
  }

  // Get the data in nsID as 4 uint32_ts, and store them in slots.
  uint32_t rawid[4];
  memcpy(&rawid, &aId, sizeof(nsID));
  static_assert(sizeof(nsID) == sizeof(rawid), "Wrong size of nsID");
  JS::SetReservedSlot(obj, kID_Slot0, PrivateUint32Value(rawid[0]));
  JS::SetReservedSlot(obj, kID_Slot1, PrivateUint32Value(rawid[1]));
  JS::SetReservedSlot(obj, kID_Slot2, PrivateUint32Value(rawid[2]));
  JS::SetReservedSlot(obj, kID_Slot3, PrivateUint32Value(rawid[3]));

  aVal.setObject(*obj);
  return true;
}

bool IfaceID2JSValue(JSContext* aCx, const nsXPTInterfaceInfo& aInfo,
                     MutableHandleValue aVal) {
  RootedObject obj(aCx, NewIDObjectHelper(aCx, &sIID_Class));
  if (!obj) {
    return false;
  }

  // The InterfaceInfo is stored in a reserved slot.
  JS::SetReservedSlot(obj, kIID_InfoSlot, PrivateValue((void*)&aInfo));
  aVal.setObject(*obj);
  return true;
}

bool ContractID2JSValue(JSContext* aCx, JSString* aContract,
                        MutableHandleValue aVal) {
  RootedString jsContract(aCx, aContract);

  {
    // It is perfectly safe to have a ContractID object with an invalid
    // ContractID, but is usually a bug.
    nsCOMPtr<nsIComponentRegistrar> registrar;
    NS_GetComponentRegistrar(getter_AddRefs(registrar));
    if (!registrar) {
      return false;
    }

    bool registered = false;
    JS::UniqueChars contract = JS_EncodeStringToLatin1(aCx, jsContract);
    registrar->IsContractIDRegistered(contract.get(), &registered);
    if (!registered) {
      return false;
    }
  }

  RootedObject obj(aCx, NewIDObjectHelper(aCx, &sCID_Class));
  if (!obj) {
    return false;
  }

  // The Contract is stored in a reserved slot.
  JS::SetReservedSlot(obj, kCID_ContractSlot, StringValue(jsContract));
  aVal.setObject(*obj);
  return true;
}

/******************************************************************************
 * # Method & Property Getter Implementations #
 */

// NOTE: This method is used both for 'get ID.prototype.number' and
// 'ID.prototype.toString'.
static bool ID_GetNumber(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  Maybe<nsID> id = JSValue2ID(aCx, args.thisv());
  if (!id) {
    return Throw(aCx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }

  char buf[NSID_LENGTH];
  id->ToProvidedString(buf);
  JSString* jsnum = JS_NewStringCopyZ(aCx, buf);
  if (!jsnum) {
    return Throw(aCx, NS_ERROR_OUT_OF_MEMORY);
  }

  args.rval().setString(jsnum);
  return true;
}

static bool ID_Equals(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  if (!args.requireAtLeast(aCx, "nsID.equals", 1)) {
    return false;
  }

  Maybe<nsID> id = JSValue2ID(aCx, args.thisv());
  Maybe<nsID> id2 = JSValue2ID(aCx, args[0]);
  if (!id || !id2) {
    return Throw(aCx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }

  args.rval().setBoolean(id->Equals(*id2));
  return true;
}

/*
 * HasInstance hooks need to find an appropriate reflector in order to function
 * properly. There are two complexities that we need to handle:
 *
 * 1 - Cross-compartment wrappers. Chrome uses over 100 compartments, all with
 *     system principal. The success of an instanceof check should not depend
 *     on which compartment an object comes from. At the same time, we want to
 *     make sure we don't unwrap important security wrappers.
 *     CheckedUnwrap does the right thing here.
 *
 * 2 - Prototype chains. Suppose someone creates a vanilla JS object |a| and
 *     sets its __proto__ to some WN |b|. If |b instanceof nsIFoo| returns true,
 *     one would expect |a instanceof nsIFoo| to return true as well, since
 *     instanceof is transitive up the prototype chain in ECMAScript. Moreover,
 *     there's chrome code that relies on this.
 *
 * This static method handles both complexities, returning either an XPCWN, a
 * DOM object, or null. The object may well be cross-compartment from |cx|.
 */
static nsresult FindObjectForHasInstance(JSContext* cx, HandleObject objArg,
                                         MutableHandleObject target) {
  RootedObject obj(cx, objArg), proto(cx);
  while (true) {
    // Try the object, or the wrappee if allowed.  We want CheckedUnwrapDynamic
    // here, because we might in fact be looking for a Window.  "cx" represents
    // our current global.
    JSObject* o =
        js::IsWrapper(obj) ? js::CheckedUnwrapDynamic(obj, cx, false) : obj;
    if (o && (IsWrappedNativeReflector(o) || IsDOMObject(o))) {
      target.set(o);
      return NS_OK;
    }

    // Walk the prototype chain from the perspective of the callee (i.e.
    // respecting Xrays if they exist).
    if (!js::GetObjectProto(cx, obj, &proto)) {
      return NS_ERROR_FAILURE;
    }
    if (!proto) {
      target.set(nullptr);
      return NS_OK;
    }
    obj = proto;
  }
}

nsresult HasInstance(JSContext* cx, HandleObject objArg, const nsID* iid,
                     bool* bp) {
  *bp = false;

  RootedObject obj(cx);
  nsresult rv = FindObjectForHasInstance(cx, objArg, &obj);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!obj) {
    return NS_OK;
  }

  // Need to unwrap Window correctly here, so use ReflectorToISupportsDynamic.
  nsCOMPtr<nsISupports> identity = ReflectorToISupportsDynamic(obj, cx);
  if (!identity) {
    return NS_OK;
  }

  nsCOMPtr<nsISupports> supp;
  identity->QueryInterface(*iid, getter_AddRefs(supp));
  *bp = supp;

  // Our old HasInstance implementation operated by invoking FindTearOff on
  // XPCWrappedNatives, and various bits of chrome JS came to depend on
  // |instanceof| doing an implicit QI if it succeeds. Do a drive-by QI to
  // preserve that behavior. This is just a compatibility hack, so we don't
  // really care if it fails.
  if (IsWrappedNativeReflector(obj)) {
    (void)XPCWrappedNative::Get(obj)->FindTearOff(cx, *iid);
  }

  return NS_OK;
}

static bool IID_HasInstance(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  if (!args.requireAtLeast(aCx, "nsIID[Symbol.hasInstance]", 1)) {
    return false;
  }

  Maybe<nsID> id = JSValue2ID(aCx, args.thisv());
  if (!id) {
    return Throw(aCx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }

  bool hasInstance = false;
  if (args[0].isObject()) {
    RootedObject target(aCx, &args[0].toObject());
    nsresult rv = HasInstance(aCx, target, id.ptr(), &hasInstance);
    if (NS_FAILED(rv)) {
      return Throw(aCx, rv);
    }
  }
  args.rval().setBoolean(hasInstance);
  return true;
}

// NOTE: This method is used both for 'get IID.prototype.name' and
// 'IID.prototype.toString'.
static bool IID_GetName(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  RootedObject obj(aCx, GetIDObject(args.thisv(), &sIID_Class));
  if (!obj) {
    return Throw(aCx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }

  const nsXPTInterfaceInfo* info = GetInterfaceInfo(obj);

  // Name property is the name of the interface this nsIID was created from.
  JSString* name = JS_NewStringCopyZ(aCx, info->Name());
  if (!name) {
    return Throw(aCx, NS_ERROR_OUT_OF_MEMORY);
  }

  args.rval().setString(name);
  return true;
}

static bool IID_NewEnumerate(JSContext* cx, HandleObject obj,
                             MutableHandleIdVector properties,
                             bool enumerableOnly) {
  const nsXPTInterfaceInfo* info = GetInterfaceInfo(obj);

  if (!properties.reserve(info->ConstantCount())) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  RootedId id(cx);
  RootedString name(cx);
  for (uint16_t i = 0; i < info->ConstantCount(); ++i) {
    name = JS_AtomizeString(cx, info->Constant(i).Name());
    if (!name || !JS_StringToId(cx, name, &id)) {
      return false;
    }
    properties.infallibleAppend(id);
  }

  return true;
}

static bool IID_Resolve(JSContext* cx, HandleObject obj, HandleId id,
                        bool* resolvedp) {
  *resolvedp = false;
  if (!id.isString()) {
    return true;
  }

  JSLinearString* name = id.toLinearString();
  const nsXPTInterfaceInfo* info = GetInterfaceInfo(obj);
  for (uint16_t i = 0; i < info->ConstantCount(); ++i) {
    if (JS_LinearStringEqualsAscii(name, info->Constant(i).Name())) {
      *resolvedp = true;

      RootedValue constant(cx, info->Constant(i).JSValue());
      return JS_DefinePropertyById(
          cx, obj, id, constant,
          JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT);
    }
  }
  return true;
}

static bool IID_MayResolve(const JSAtomState& names, jsid id,
                           JSObject* maybeObj) {
  if (!id.isString()) {
    return false;
  }

  if (!maybeObj) {
    // Each interface object has its own set of constants, so if we don't know
    // the object, assume any string property may be resolved.
    return true;
  }

  JSLinearString* name = id.toLinearString();
  const nsXPTInterfaceInfo* info = GetInterfaceInfo(maybeObj);
  for (uint16_t i = 0; i < info->ConstantCount(); ++i) {
    if (JS_LinearStringEqualsAscii(name, info->Constant(i).Name())) {
      return true;
    }
  }
  return false;
}

// Common code for CID_CreateInstance and CID_GetService
static bool CIGSHelper(JSContext* aCx, unsigned aArgc, Value* aVp,
                       bool aGetService) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  // Extract the ContractID string from our reserved slot. Don't use
  // JSValue2ID as this method should only be defined on Contract ID objects,
  // and it allows us to avoid a duplicate hashtable lookup.
  RootedObject obj(aCx, GetIDObject(args.thisv(), &sCID_Class));
  if (!obj) {
    return Throw(aCx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }
  JS::UniqueChars contractID = JS_EncodeStringToLatin1(
      aCx, JS::GetReservedSlot(obj, kCID_ContractSlot).toString());

  // Extract the IID from the first argument, if passed. Default: nsISupports.
  Maybe<nsIID> iid = args.length() >= 1 ? JSValue2ID(aCx, args[0])
                                        : Some(NS_GET_IID(nsISupports));
  if (!iid) {
    return Throw(aCx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }

  // Invoke CreateInstance or GetService with our ContractID.
  nsresult rv;
  nsCOMPtr<nsISupports> result;
  if (aGetService) {
    rv = CallGetService(contractID.get(), *iid, getter_AddRefs(result));
    if (NS_FAILED(rv) || !result) {
      return Throw(aCx, NS_ERROR_XPC_GS_RETURNED_FAILURE);
    }
  } else {
    rv = CallCreateInstance(contractID.get(), *iid, getter_AddRefs(result));
    if (NS_FAILED(rv) || !result) {
      return Throw(aCx, NS_ERROR_XPC_CI_RETURNED_FAILURE);
    }
  }

  // Wrap the created object and return it.
  rv = nsContentUtils::WrapNative(aCx, result, iid.ptr(), args.rval());
  if (NS_FAILED(rv) || args.rval().isPrimitive()) {
    return Throw(aCx, NS_ERROR_XPC_CANT_CREATE_WN);
  }
  return true;
}

static bool CID_CreateInstance(JSContext* aCx, unsigned aArgc, Value* aVp) {
  return CIGSHelper(aCx, aArgc, aVp, /* aGetService = */ false);
}

static bool CID_GetService(JSContext* aCx, unsigned aArgc, Value* aVp) {
  return CIGSHelper(aCx, aArgc, aVp, /* aGetService = */ true);
}

// NOTE: This method is used both for 'get CID.prototype.name' and
// 'CID.prototype.toString'.
static bool CID_GetName(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  RootedObject obj(aCx, GetIDObject(args.thisv(), &sCID_Class));
  if (!obj) {
    return Throw(aCx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }

  // Return the string stored in our reserved ContractID slot.
  args.rval().set(JS::GetReservedSlot(obj, kCID_ContractSlot));
  return true;
}

}  // namespace xpc
