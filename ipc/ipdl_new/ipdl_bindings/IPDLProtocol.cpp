/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "IPDLProtocol.h"

#include <cfloat>

#include "ipdl_ffi_generated.h"
#include "mozilla/dom/DOMMozPromiseRequestHolder.h"
#include "mozilla/dom/Promise.h"
#include "nsJSUtils.h"
#include "wrapper.h"
#include "mozilla/dom/IPDL.h"
#include "IPCInterface.h"
#include "IPDLProtocolInstance.h"

namespace mozilla {
namespace ipdl {

NS_IMPL_CYCLE_COLLECTION_CLASS(IPDLProtocol)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IPDLProtocol)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInstances)
  tmp->mProtoObj = nullptr;
  tmp->mConstructorObj = nullptr;
  mozilla::DropJSObjects(tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IPDLProtocol)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInstances)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IPDLProtocol)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mProtoObj)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mConstructorObj)
NS_IMPL_CYCLE_COLLECTION_TRACE_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(IPDLProtocol)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IPDLProtocol)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IPDLProtocol)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

void
IPDLProtocol::ASTDeletePolicy::operator()(const mozilla::ipdl::ffi::AST* aASTPtr)
{
  if (!aASTPtr) {
    return;
  }
  wrapper::FreeAST(aASTPtr);
}

IPDLProtocol::IPDLProtocol(dom::IPDL* aIPDL,
                           IPDLSide aSide,
                           const nsACString& aIPDLFile,
                           nsIGlobalObject* aGlobal,
                           JS::HandleObject aParent,
                           JSContext* aCx)
  : mSide(aSide)
  , mGlobal(aGlobal)
  , mIPDL(aIPDL)
  , mNextProtocolInstanceChannelId(0)
{
  mozilla::HoldJSObjects(this);

  nsCString errorString;
  mAST.reset(wrapper::Parse(aIPDLFile, errorString));

  if (!mAST) {
    JS_ReportErrorUTF8(aCx, "IPDL: %s", errorString.get());
    return;
  }

  // Get the main translation unit.
  auto mainTU = GetMainTU();

  MOZ_ASSERT(mainTU->protocol, "should have a protocol in protocol file");

  mProtocolName = JoinNamespace(mainTU->protocol->ns);

  auto& manages = mainTU->protocol->protocol.manages;
  auto& messages = mainTU->protocol->protocol.messages;

  NS_NAMED_LITERAL_CSTRING(sendPrefix, "send");
  NS_NAMED_LITERAL_CSTRING(recvPrefix, "recv");
  NS_NAMED_LITERAL_CSTRING(allocPrefix, "alloc");
  NS_NAMED_LITERAL_CSTRING(constructorSuffix, "Constructor");

  // Store the managed protocols in a set for fast retrieval and access.
  nsTHashtable<nsCStringHashKey> managedProtocols;
  for (auto& managedProtocol : manages) {
    managedProtocols.PutEntry(managedProtocol.id);
  }

  nsTArray<JSFunctionSpec> funcs;
  nsTArray<nsCString> functionNames;
  // We loop through every message of the protocol and add them to our message
  // table, taking into account our protocol side and the message direction.
  for (auto& message : messages) {
    // The message is gonna be sendXXX.
    if ((message.direction == ffi::Direction::ToChild &&
         mSide == IPDLSide::Parent) ||
        (message.direction == ffi::Direction::ToParent &&
         mSide == IPDLSide::Child) ||
        message.direction == ffi::Direction::ToParentOrChild) {

      // If this is a managed protocol name...
      if (managedProtocols.Contains(message.name.id)) {
        // Append the sendXXXConstructor message.
        const nsAutoCString& constructorName =
          sendPrefix + message.name.id + constructorSuffix;

        functionNames.AppendElement(constructorName);
        funcs.AppendElement<JSFunctionSpec>(JS_FN(
          functionNames.LastElement().get(),
          SendConstructorDispatch,
          static_cast<uint16_t>(message.in_params.Length()),
          0));
        mMessageTable.Put(constructorName, &message);

        // And the allocXXX message.
        const nsAutoCString& allocName = allocPrefix + message.name.id;
        functionNames.AppendElement(allocName);
        funcs.AppendElement<JSFunctionSpec>(JS_FN(
          functionNames.LastElement().get(),
          AbstractAlloc,
          static_cast<uint16_t>(message.in_params.Length()),
          0));
        mMessageTable.Put(allocName, &message);
      } else {
        // Otherwise for the regular sendXXX messages, we define the associated JS function.
        const nsAutoCString& funcName = sendPrefix + message.name.id;
        functionNames.AppendElement(funcName);
        funcs.AppendElement<JSFunctionSpec>(JS_FN(
          functionNames.LastElement().get(),
          message.name.id.Equals("__delete__") ? SendDeleteDispatch
                                               : SendMessageDispatch,
          static_cast<uint16_t>(message.in_params.Length()),
          0));
        mMessageTable.Put(funcName, &message);
      }
    }

    // The message is gonna be recvXXX;
    if ((message.direction == ffi::Direction::ToChild &&
         mSide == IPDLSide::Child) ||
        (message.direction == ffi::Direction::ToParent &&
         mSide == IPDLSide::Parent) ||
        message.direction == ffi::Direction::ToParentOrChild) {

      // If this is a managed protocol...
      if (managedProtocols.Contains(message.name.id)) {
        // Append the recvXXXConstructor message.
        const nsAutoCString& constructorName =
          recvPrefix + message.name.id + constructorSuffix;
        functionNames.AppendElement(constructorName);
        funcs.AppendElement<JSFunctionSpec>(JS_FN(
          functionNames.LastElement().get(),
          RecvConstructor,
          static_cast<uint16_t>(message.in_params.Length()),
          0));
        mMessageTable.Put(constructorName, &message);

        // And the allocXXX message.
        const nsAutoCString& allocName = allocPrefix + message.name.id;
        functionNames.AppendElement(allocName);
        funcs.AppendElement<JSFunctionSpec>(JS_FN(
          functionNames.LastElement().get(),
          AbstractAlloc,
          static_cast<uint16_t>(message.in_params.Length()),
          0));
        mMessageTable.Put(allocName, &message);
      } else {
        // For the regular recvXXX callbacks, we define the associated "abstract"
        // functions or the delete default function.
        const nsAutoCString& funcName = recvPrefix + message.name.id;
        functionNames.AppendElement(funcName);
        funcs.AppendElement<JSFunctionSpec>(JS_FN(
          functionNames.LastElement().get(),
          message.name.id.Equals("__delete__") ? RecvDelete
                                               : AbstractRecvMessage,
          static_cast<uint16_t>(message.in_params.Length()),
          0));

        mMessageTable.Put(funcName, &message);
      }
    }
  }

  funcs.AppendElement<JSFunctionSpec>(JS_FS_END);

  // Create our new protocol JSClass.
  mSidedProtocolName = GetSidedProtocolName(mProtocolName, mSide);
  mProtocolClass =
    { mSidedProtocolName.get(),
               JSCLASS_HAS_PRIVATE,
               &sIPDLJSClassOps };

  JS::RootedObject parentProto(aCx);
  JS_GetClassPrototype(aCx, JSProto_Object, &parentProto);

  // Initialize it with the constructor and the functions.
  mProtoObj = JS_InitClass(aCx,
                           aParent,
                           parentProto,
                           &mProtocolClass,
                           Constructor,
                           0,
                           nullptr,
                           funcs.Elements(),
                           nullptr,
                           nullptr);

  JS::RootedObject protoObj(aCx, mProtoObj);

  // Add this object to the private field.
  JS_SetPrivate(protoObj, this);
  mConstructorObj = JS_GetConstructor(aCx, protoObj);

  // We build the name->AST lookup tables for later typechecking.
  BuildNameLookupTables();
}

nsCString
IPDLProtocol::GetProtocolName()
{
  return mProtocolName;
}

JSObject*
IPDLProtocol::GetProtocolClassConstructor()
{
  return mConstructorObj.get();
}

uint32_t
IPDLProtocol::RegisterExternalInstance()
{
  auto childID = mNextProtocolInstanceChannelId++;
  return childID;
}

JSClass&
IPDLProtocol::GetProtocolClass()
{
  return mProtocolClass;
}

const ffi::TranslationUnit*
IPDLProtocol::GetMainTU()
{
  return wrapper::GetTU(mAST.get(), wrapper::GetMainTUId(mAST.get()));
}

void
IPDLProtocol::BuildNameLookupTables()
{
  // We don't have an Int32 hashkey, but it's ok since we can just perform
  // conversions in and out.
  nsTHashtable<nsUint32HashKey> visitedTUId;
  nsTArray<ffi::TUId> workList;

  // Our current loop element.
  const ffi::TranslationUnit* currentTU = nullptr;
  // Our current loop index.
  ffi::TUId currentTUId = -1;

  // We start with only the main tu in the worklist, and then unroll all the
  // includes.
  ffi::TUId mainTUId = wrapper::GetMainTUId(mAST.get());
  workList.AppendElement(mainTUId);
  while (!workList.IsEmpty()) {
    // Get an index and anelement from the work list.
    currentTUId = workList.PopLastElement();
    currentTU = wrapper::GetTU(mAST.get(), currentTUId);

    // If we are in a protocol file, just add the protocol and stop here (we)
    // shouldn't keep going through the includes recursively, and we should
    // ignore the local structs and unions.
    if (currentTU->file_type == ffi::FileType::Protocol) {
      // If there is a protocol, add it.
      if (currentTU->protocol) {
        mProtocolTable.Put(JoinNamespace(currentTU->protocol->ns),
                           currentTU->protocol.ptr());
      }
    }

    // If we are in a header file, add the structs and the unions it contains,
    // and then recurse through all of its includes.
    if (currentTU->file_type == ffi::FileType::Header ||
        currentTUId == mainTUId) {
      for (size_t i = 0; i < currentTU->structs.Length(); i++) {
        // Dirty, but this is the only way to have a pointer to an element
        const auto* s = currentTU->structs.Elements() + i;
        mStructTable.Put(JoinNamespace(s->ns), s);
      }

      for (size_t i = 0; i < currentTU->unions.Length(); i++) {
        // Dirty, but this is the only way to have a pointer to an element
        const auto* u = currentTU->unions.Elements() + i;
        mUnionTable.Put(JoinNamespace(u->ns), u);
      }

      workList.AppendElements(currentTU->includes);
    }
  }
}

/* static */ bool
IPDLProtocol::SendMessageDispatch(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  // Unwrap the JS args.
  auto args = JS::CallArgsFromVp(aArgc, aVp);

  // Get the IPDLProtocol object from the private field of the method `this`.
  JS::RootedObject thisObj(aCx);
  args.computeThis(aCx, &thisObj);

  auto* instance = static_cast<IPDLProtocolInstance*>(JS_GetPrivate(thisObj));

  if (!instance) {
    JS_ReportErrorUTF8(aCx, "Cannot use deleted protocol");
    return false;
  }

  // Get the method name that we are calling.
  auto function = JS_GetObjectFunction(&args.callee());
  nsAutoJSString functionName;
  if (!functionName.init(aCx, JS_GetFunctionId(function))) {
    return false;
  }

  // Call our internal SendMessage function.
  return instance->SendMessage(aCx, NS_ConvertUTF16toUTF8(functionName), args);
}

/* static */ bool
IPDLProtocol::SendConstructorDispatch(JSContext* aCx,
                                      unsigned aArgc,
                                      JS::Value* aVp)
{
  // Unwrap the JS args.
  auto args = JS::CallArgsFromVp(aArgc, aVp);

  // Get the IPDLProtocol object from the private field of the method `this`.
  JS::RootedObject thisObj(aCx);
  args.computeThis(aCx, &thisObj);

  auto* instance = static_cast<IPDLProtocolInstance*>(JS_GetPrivate(thisObj));

  if (!instance) {
    JS_ReportErrorUTF8(aCx, "Cannot use deleted protocol");
    return false;
  }

  // Get the method name that we are calling.
  auto function = JS_GetObjectFunction(&args.callee());
  nsAutoJSString functionName;
  if (!functionName.init(aCx, JS_GetFunctionId(function))) {
    return false;
  }

  // Call our internal SendConstructor function.
  return instance->SendConstructor(aCx, thisObj, NS_ConvertUTF16toUTF8(functionName), args);
}

/* static */ bool
IPDLProtocol::SendDeleteDispatch(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  // Unwrap the JS args.
  auto args = JS::CallArgsFromVp(aArgc, aVp);

  // Get the IPDLProtocol object from the private field of the method `this`.
  JS::RootedObject thisObj(aCx);
  args.computeThis(aCx, &thisObj);

  auto* instance = static_cast<IPDLProtocolInstance*>(JS_GetPrivate(thisObj));

  if (!instance) {
    JS_ReportErrorUTF8(aCx, "Cannot use deleted protocol");
    return false;
  }

  // Get the method name that we are calling.
  auto function = JS_GetObjectFunction(&args.callee());
  nsAutoJSString functionName;
  if (!functionName.init(aCx, JS_GetFunctionId(function))) {
    return false;
  }

  // Call our internal SendConstructor function.
  return instance->SendDelete(aCx, NS_ConvertUTF16toUTF8(functionName), args);
}

void
IPDLProtocol::RemoveInstance(IPDLProtocolInstance *instance)
{
  mInstances.RemoveEntry(instance);
}

bool
IPDLProtocol::CheckParamTypeSpec(JSContext* aCx,
                                 JS::HandleValue aJSVal,
                                 ffi::Param aParam)
{
  // Just unwrap the type spec.
  return CheckTypeSpec(aCx, aJSVal, aParam.type_spec);
}

bool
IPDLProtocol::CheckTypeSpec(JSContext* aCx,
                            JS::HandleValue aJSVal,
                            ffi::TypeSpec aTypeSpec)
{
  // If the type is nullable and we get a null, we return now
  if (aTypeSpec.nullable && aJSVal.isNull()) {
    return true;
  }

  // If the type is an array, we check the type for all the array elements
  if (aTypeSpec.array) {
    // Check if jsVal is an array.
    bool isArray = false;
    JS_IsArrayObject(aCx, aJSVal, &isArray);
    if (!isArray) {
      return false;
    }

    // Get the JS array.
    JS::RootedObject jsArray(aCx, &aJSVal.toObject());

    uint32_t arrayLength = 0;
    JS_GetArrayLength(aCx, jsArray, &arrayLength);

    // For each element of the array, we check its type.
    for (size_t i = 0; i < arrayLength; i++) {
      JS::RootedValue arrayVal(aCx);
      JS_GetElement(aCx, jsArray, i, &arrayVal);

      if (!CheckType(aCx, arrayVal, aTypeSpec.spec)) {
        return false;
      }
    }

    return true;
  }

  // We check the actual type if not an array.
  return CheckType(aCx, aJSVal, aTypeSpec.spec);
}

bool
IPDLProtocol::CheckType(JSContext* aCx,
                        JS::HandleValue aJSVal,
                        ffi::QualifiedId aType)
{
  // First check the builtin types
  if (CheckBuiltinType(aCx, aJSVal, aType)) {
    return true;
  }

  // Then the protocol types
  if (CheckProtocolType(aCx, aJSVal, aType)) {
    return true;
  }

  // Then the struct types
  if (CheckStructType(aCx, aJSVal, aType)) {
    return true;
  }

  // Then the union types.
  if (CheckUnionType(aCx, aJSVal, aType)) {
    return true;
  }

  // Otherwise we have a type mismatch.
  return false;
}

bool
IPDLProtocol::CheckProtocolType(JSContext* aCx,
                                JS::HandleValue aJSVal,
                                ffi::QualifiedId aType)
{
  // If we don't know that protocol name, return false.
  auto typeString = JoinQualifiedId(aType);
  if (!mProtocolTable.Contains(typeString)) {
    return false;
  }

  // If we don't have an object, return false.
  if (!aJSVal.isObject()) {
    return false;
  }

  // Get our JS object.
  JS::RootedObject jsObj(aCx, &aJSVal.toObject());

  // If the object is not an IPDL protocol class instance, return false.
  if (typeString.Equals(JS_GetClass(jsObj)->name) != 0) { // not equal
    return false;
  }

  auto* protocolObj = static_cast<IPDLProtocol*>(JS_GetPrivate(jsObj));

  if (!protocolObj) {
    JS_ReportErrorUTF8(aCx, "Couldn't get protocol object from private date field");
    return false;
  }

  // Check the protocol name against the one we require.
  return protocolObj->GetProtocolName().Equals(typeString);
}

bool
IPDLProtocol::CheckStructType(JSContext* aCx,
                              JS::HandleValue aJSVal,
                              ffi::QualifiedId aType)
{
  // If we don't know that struct name, return false.
  auto* ipdlStruct = mStructTable.Get(JoinQualifiedId(aType));
  if (!ipdlStruct) {
    return false;
  }

  // If we don't have an object, return false.
  if (!aJSVal.isObject()) {
    return false;
  }

  // Get our JS object.
  JS::RootedObject jsObj(aCx, &aJSVal.toObject());

  // Go through every field of the struct.
  for (auto& field : ipdlStruct->fields) {
    JS::RootedValue propertyValue(aCx);

    // Check that the field exists in the object we got.
    if (!JS_GetProperty(aCx, jsObj, field.name.id.get(), &propertyValue)) {
      return false;
    }
    if (propertyValue.isUndefined()) {
      return false;
    }

    // Check that the field type matches the element we got.
    if (!CheckTypeSpec(aCx, propertyValue, field.type_spec)) {
      return false;
    }
  }

  return true;
}

bool
IPDLProtocol::CheckUnionType(JSContext* aCx,
                             JS::HandleValue aJSVal,
                             ffi::QualifiedId aType)
{
  auto* ipdlUnion = mUnionTable.Get(JoinQualifiedId(aType));
  if (!ipdlUnion) {
    return false;
  }

  // Go through every type of the union
  for (auto& type : ipdlUnion->types) {
    // If our input value matches any of these types, return true.
    if (CheckTypeSpec(aCx, aJSVal, type)) {
      return true;
    }
  }

  // Otherwise return false.
  return false;
}

/* static */ bool
IPDLProtocol::CheckBuiltinType(JSContext* aCx, JS::HandleValue aJSVal, ffi::QualifiedId type)
{
  // Check all the builtin types using their type name, and custom constraints
  // such as bounds checking and length.
  auto typeString = JoinQualifiedId(type);

  if (typeString.Equals("bool")) {
    return aJSVal.isBoolean();
  }
  if (typeString.Equals("char")) {
    JS::AutoCheckCannotGC nogc;
    size_t length;
    if (!aJSVal.isString()) {
      return false;
    }

    JS_GetLatin1StringCharsAndLength(aCx, nogc, aJSVal.toString(), &length);
    return length == 1;
  }
  if (typeString.Equals("nsString") || typeString.Equals("nsCString")) {
    return aJSVal.isString();
  }
  if (typeString.Equals("short")) {
    return aJSVal.isNumber() && (trunc(aJSVal.toNumber()) == aJSVal.toNumber()) &&
           (aJSVal.toNumber() <= SHRT_MAX) && (aJSVal.toNumber() >= SHRT_MIN);
  }
  if (typeString.Equals("int")) {
    return aJSVal.isNumber() && (trunc(aJSVal.toNumber()) == aJSVal.toNumber()) &&
           (aJSVal.toNumber() <= INT_MAX) && (aJSVal.toNumber() >= INT_MIN);
  }
  if (typeString.Equals("long")) {
    return aJSVal.isNumber() && (trunc(aJSVal.toNumber()) == aJSVal.toNumber()) &&
           (aJSVal.toNumber() <= LONG_MAX) && (aJSVal.toNumber() >= LONG_MIN);
  }
  if (typeString.Equals("float")) {
    return aJSVal.isNumber() && (aJSVal.toNumber() <= FLT_MAX) &&
           (aJSVal.toNumber() >= FLT_MIN);
  }
  if (typeString.Equals("double")) {
    return aJSVal.isNumber();
  }
  if (typeString.Equals("int8_t")) {
    return aJSVal.isNumber() && (trunc(aJSVal.toNumber()) == aJSVal.toNumber()) &&
           (aJSVal.toNumber() <= INT8_MAX) && (aJSVal.toNumber() >= INT8_MIN);
  }
  if (typeString.Equals("uint8_t")) {
    return aJSVal.isNumber() && (trunc(aJSVal.toNumber()) == aJSVal.toNumber()) &&
           (aJSVal.toNumber() <= UINT8_MAX) && (aJSVal.toNumber() >= 0);
  }
  if (typeString.Equals("int16_t")) {
    return aJSVal.isNumber() && (trunc(aJSVal.toNumber()) == aJSVal.toNumber()) &&
           (aJSVal.toNumber() <= INT16_MAX) && (aJSVal.toNumber() >= INT16_MIN);
  }
  if (typeString.Equals("uint16_t")) {
    return aJSVal.isNumber() && (trunc(aJSVal.toNumber()) == aJSVal.toNumber()) &&
           (aJSVal.toNumber() <= UINT16_MAX) && (aJSVal.toNumber() >= 0);
  }
  if (typeString.Equals("int32_t")) {
    return aJSVal.isNumber() && (trunc(aJSVal.toNumber()) == aJSVal.toNumber()) &&
           (aJSVal.toNumber() <= INT32_MAX) && (aJSVal.toNumber() >= INT32_MIN);
  }
  if (typeString.Equals("uint32_t")) {
    return aJSVal.isNumber() && (trunc(aJSVal.toNumber()) == aJSVal.toNumber()) &&
           (aJSVal.toNumber() <= UINT32_MAX) && (aJSVal.toNumber() >= 0);
  }

  // These IPDL builtin types are not allowed from the JS API because they are
  // not easily representable.
  if (typeString.Equals("int64_t") || typeString.Equals("uint64_t") ||
      typeString.Equals("size_t") || typeString.Equals("ssize_t") ||
      typeString.Equals("nsresult") ||
      typeString.Equals("mozilla::ipc::Shmem") ||
      typeString.Equals("mozilla::ipc::ByteBuf") ||
      typeString.Equals("mozilla::ipc::FileDescriptor")) {
    JS_ReportErrorUTF8(
      aCx, "IPDL: cannot use type `%s` from JS", typeString.get());
    return false;
  }

  return false;
}

IPDLProtocol::~IPDLProtocol()
{
  mProtoObj = nullptr;
  mConstructorObj = nullptr;
  mozilla::DropJSObjects(this);
}

/* static */ bool
IPDLProtocol::Constructor(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);

  // This function is a bit tricky. Basically we need to construct an object with
  // the protocol's C++ JSClass, but with the prototype of the inherited JS class.

  // First we get the prototype of the inherited class.
  JS::RootedValue newTargetObjProtov(aCx);
  JS::RootedObject newTargetObj(aCx, &args.newTarget().toObject());
  if (!JS_GetProperty(aCx, newTargetObj, "prototype", &newTargetObjProtov)) {
    return false;
  }
  JS::RootedObject newTargetObjProto(aCx, &newTargetObjProtov.toObject());

  // Now we get the prototype of abstract protocol class, and get the IPDLProtocol
  // object from it.
  JS::RootedObject callee(aCx, &args.callee());
  JS::RootedValue prototypev(aCx);
  if (!JS_GetProperty(aCx, callee, "prototype", &prototypev)) {
    return false;
  }
  JS::RootedObject prototype(aCx, &prototypev.toObject());
  auto protocol = static_cast<IPDLProtocol*>(JS_GetPrivate(prototype));

  if (!protocol) {
    JS_ReportErrorUTF8(aCx, "Couldn't get protocol object from private date field");
    return false;
  }

  // Now we construct our new object.
  JS::RootedObject newClassObject(
    aCx,
    JS_NewObjectWithGivenProto(
      aCx, &protocol->GetProtocolClass(), newTargetObjProto));

  // We add it to our list of protocol instances and set its private field.

  auto newInstance = MakeRefPtr<IPDLProtocolInstance>(
    nullptr, protocol->RegisterExternalInstance(), protocol, newClassObject);

  protocol->mInstances.PutEntry(newInstance);
  JS_SetPrivate(newClassObject, newInstance);

  args.rval().setObject(*newClassObject);
  return true;
}

/* static */ bool
IPDLProtocol::RecvDelete(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  return true;
}

/* static */ bool
IPDLProtocol::RecvConstructor(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  return true;
}

/* static */ bool
IPDLProtocol::AbstractRecvMessage(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  JS_ReportErrorUTF8(
    aCx, "Received message but recv method not overriden!");
  return false;
}

/* static */ bool
IPDLProtocol::AbstractAlloc(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  JS_ReportErrorUTF8(
    aCx, "Cannot alloc from abstract IPDL protocol class!");
  return false;
}

/* static */ nsCString
IPDLProtocol::JoinQualifiedId(const ffi::QualifiedId aQid)
{
  nsAutoCString ret;

  for (auto& qual : aQid.quals) {
    ret.Append(qual);
    ret.Append("::");
  }
  ret.Append(aQid.base_id.id);

  return std::move(ret);
}

/* static */ nsCString
IPDLProtocol::JoinNamespace(const ffi::Namespace aNs)
{
  nsAutoCString ret;

  for (auto& name : aNs.namespaces) {
    ret.Append(name);
    ret.Append("::");
  }
  ret.Append(aNs.name.id);

  return std::move(ret);
}

constexpr JSClassOps IPDLProtocol::sIPDLJSClassOps;

} // ipdl
} // mozilla
