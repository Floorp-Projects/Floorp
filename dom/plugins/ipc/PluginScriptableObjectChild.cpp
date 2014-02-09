/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginScriptableObjectChild.h"
#include "PluginScriptableObjectUtils.h"
#include "PluginIdentifierChild.h"

using namespace mozilla::plugins;

// static
NPObject*
PluginScriptableObjectChild::ScriptableAllocate(NPP aInstance,
                                                NPClass* aClass)
{
  AssertPluginThread();

  if (aClass != GetClass()) {
    NS_RUNTIMEABORT("Huh?! Wrong class!");
  }

  return new ChildNPObject();
}

// static
void
PluginScriptableObjectChild::ScriptableInvalidate(NPObject* aObject)
{
  AssertPluginThread();

  if (aObject->_class != GetClass()) {
    NS_RUNTIMEABORT("Don't know what kind of object this is!");
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    // This can happen more than once, and is just fine.
    return;
  }

  object->invalidated = true;
}

// static
void
PluginScriptableObjectChild::ScriptableDeallocate(NPObject* aObject)
{
  AssertPluginThread();

  if (aObject->_class != GetClass()) {
    NS_RUNTIMEABORT("Don't know what kind of object this is!");
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  PluginScriptableObjectChild* actor = object->parent;
  if (actor) {
    NS_ASSERTION(actor->Type() == Proxy, "Bad type!");
    actor->DropNPObject();
  }

  delete object;
}

// static
bool
PluginScriptableObjectChild::ScriptableHasMethod(NPObject* aObject,
                                                 NPIdentifier aName)
{
  AssertPluginThread();

  if (aObject->_class != GetClass()) {
    NS_RUNTIMEABORT("Don't know what kind of object this is!");
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectChild> actor(object->parent);
  NS_ASSERTION(actor, "This shouldn't ever be null!");
  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  bool result;
  actor->CallHasMethod(static_cast<PPluginIdentifierChild*>(aName), &result);

  return result;
}

// static
bool
PluginScriptableObjectChild::ScriptableInvoke(NPObject* aObject,
                                              NPIdentifier aName,
                                              const NPVariant* aArgs,
                                              uint32_t aArgCount,
                                              NPVariant* aResult)
{
  AssertPluginThread();

  if (aObject->_class != GetClass()) {
    NS_RUNTIMEABORT("Don't know what kind of object this is!");
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectChild> actor(object->parent);
  NS_ASSERTION(actor, "This shouldn't ever be null!");
  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  ProtectedVariantArray args(aArgs, aArgCount, actor->GetInstance());
  if (!args.IsOk()) {
    NS_ERROR("Failed to convert arguments!");
    return false;
  }

  Variant remoteResult;
  bool success;
  actor->CallInvoke(static_cast<PPluginIdentifierChild*>(aName), args,
                    &remoteResult, &success);

  if (!success) {
    return false;
  }

  ConvertToVariant(remoteResult, *aResult);
  return true;
}

// static
bool
PluginScriptableObjectChild::ScriptableInvokeDefault(NPObject* aObject,
                                                     const NPVariant* aArgs,
                                                     uint32_t aArgCount,
                                                     NPVariant* aResult)
{
  AssertPluginThread();

  if (aObject->_class != GetClass()) {
    NS_RUNTIMEABORT("Don't know what kind of object this is!");
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectChild> actor(object->parent);
  NS_ASSERTION(actor, "This shouldn't ever be null!");
  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  ProtectedVariantArray args(aArgs, aArgCount, actor->GetInstance());
  if (!args.IsOk()) {
    NS_ERROR("Failed to convert arguments!");
    return false;
  }

  Variant remoteResult;
  bool success;
  actor->CallInvokeDefault(args, &remoteResult, &success);

  if (!success) {
    return false;
  }

  ConvertToVariant(remoteResult, *aResult);
  return true;
}

// static
bool
PluginScriptableObjectChild::ScriptableHasProperty(NPObject* aObject,
                                                   NPIdentifier aName)
{
  AssertPluginThread();

  if (aObject->_class != GetClass()) {
    NS_RUNTIMEABORT("Don't know what kind of object this is!");
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectChild> actor(object->parent);
  NS_ASSERTION(actor, "This shouldn't ever be null!");
  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  bool result;
  actor->CallHasProperty(static_cast<PPluginIdentifierChild*>(aName), &result);

  return result;
}

// static
bool
PluginScriptableObjectChild::ScriptableGetProperty(NPObject* aObject,
                                                   NPIdentifier aName,
                                                   NPVariant* aResult)
{
  AssertPluginThread();

  if (aObject->_class != GetClass()) {
    NS_RUNTIMEABORT("Don't know what kind of object this is!");
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectChild> actor(object->parent);
  NS_ASSERTION(actor, "This shouldn't ever be null!");
  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  Variant result;
  bool success;
  actor->CallGetParentProperty(static_cast<PPluginIdentifierChild*>(aName),
                               &result, &success);

  if (!success) {
    return false;
  }

  ConvertToVariant(result, *aResult);
  return true;
}

// static
bool
PluginScriptableObjectChild::ScriptableSetProperty(NPObject* aObject,
                                                   NPIdentifier aName,
                                                   const NPVariant* aValue)
{
  AssertPluginThread();

  if (aObject->_class != GetClass()) {
    NS_RUNTIMEABORT("Don't know what kind of object this is!");
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectChild> actor(object->parent);
  NS_ASSERTION(actor, "This shouldn't ever be null!");
  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  ProtectedVariant value(*aValue, actor->GetInstance());
  if (!value.IsOk()) {
    NS_WARNING("Failed to convert variant!");
    return false;
  }

  bool success;
  actor->CallSetProperty(static_cast<PPluginIdentifierChild*>(aName), value,
                         &success);

  return success;
}

// static
bool
PluginScriptableObjectChild::ScriptableRemoveProperty(NPObject* aObject,
                                                      NPIdentifier aName)
{
  AssertPluginThread();

  if (aObject->_class != GetClass()) {
    NS_RUNTIMEABORT("Don't know what kind of object this is!");
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectChild> actor(object->parent);
  NS_ASSERTION(actor, "This shouldn't ever be null!");
  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  bool success;
  actor->CallRemoveProperty(static_cast<PPluginIdentifierChild*>(aName),
                            &success);

  return success;
}

// static
bool
PluginScriptableObjectChild::ScriptableEnumerate(NPObject* aObject,
                                                 NPIdentifier** aIdentifiers,
                                                 uint32_t* aCount)
{
  AssertPluginThread();

  if (aObject->_class != GetClass()) {
    NS_RUNTIMEABORT("Don't know what kind of object this is!");
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectChild> actor(object->parent);
  NS_ASSERTION(actor, "This shouldn't ever be null!");
  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  AutoInfallibleTArray<PPluginIdentifierChild*, 10> identifiers;
  bool success;
  actor->CallEnumerate(&identifiers, &success);

  if (!success) {
    return false;
  }

  *aCount = identifiers.Length();
  if (!*aCount) {
    *aIdentifiers = nullptr;
    return true;
  }

  *aIdentifiers = reinterpret_cast<NPIdentifier*>(
      PluginModuleChild::sBrowserFuncs.memalloc(*aCount * sizeof(NPIdentifier)));
  if (!*aIdentifiers) {
    NS_ERROR("Out of memory!");
    return false;
  }

  for (uint32_t index = 0; index < *aCount; index++) {
    (*aIdentifiers)[index] =
      static_cast<PPluginIdentifierChild*>(identifiers[index]);
  }
  return true;
}

// static
bool
PluginScriptableObjectChild::ScriptableConstruct(NPObject* aObject,
                                                 const NPVariant* aArgs,
                                                 uint32_t aArgCount,
                                                 NPVariant* aResult)
{
  AssertPluginThread();

  if (aObject->_class != GetClass()) {
    NS_RUNTIMEABORT("Don't know what kind of object this is!");
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectChild> actor(object->parent);
  NS_ASSERTION(actor, "This shouldn't ever be null!");
  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  ProtectedVariantArray args(aArgs, aArgCount, actor->GetInstance());
  if (!args.IsOk()) {
    NS_ERROR("Failed to convert arguments!");
    return false;
  }

  Variant remoteResult;
  bool success;
  actor->CallConstruct(args, &remoteResult, &success);

  if (!success) {
    return false;
  }

  ConvertToVariant(remoteResult, *aResult);
  return true;
}

const NPClass PluginScriptableObjectChild::sNPClass = {
  NP_CLASS_STRUCT_VERSION,
  PluginScriptableObjectChild::ScriptableAllocate,
  PluginScriptableObjectChild::ScriptableDeallocate,
  PluginScriptableObjectChild::ScriptableInvalidate,
  PluginScriptableObjectChild::ScriptableHasMethod,
  PluginScriptableObjectChild::ScriptableInvoke,
  PluginScriptableObjectChild::ScriptableInvokeDefault,
  PluginScriptableObjectChild::ScriptableHasProperty,
  PluginScriptableObjectChild::ScriptableGetProperty,
  PluginScriptableObjectChild::ScriptableSetProperty,
  PluginScriptableObjectChild::ScriptableRemoveProperty,
  PluginScriptableObjectChild::ScriptableEnumerate,
  PluginScriptableObjectChild::ScriptableConstruct
};

PluginScriptableObjectChild::PluginScriptableObjectChild(
                                                     ScriptableObjectType aType)
: mInstance(nullptr),
  mObject(nullptr),
  mInvalidated(false),
  mProtectCount(0),
  mType(aType)
{
  AssertPluginThread();
}

PluginScriptableObjectChild::~PluginScriptableObjectChild()
{
  AssertPluginThread();

  if (mObject) {
    PluginModuleChild::current()->UnregisterActorForNPObject(mObject);

    if (mObject->_class == GetClass()) {
      NS_ASSERTION(mType == Proxy, "Wrong type!");
      static_cast<ChildNPObject*>(mObject)->parent = nullptr;
    }
    else {
      NS_ASSERTION(mType == LocalObject, "Wrong type!");
      PluginModuleChild::sBrowserFuncs.releaseobject(mObject);
    }
  }
}

void
PluginScriptableObjectChild::InitializeProxy()
{
  AssertPluginThread();
  NS_ASSERTION(mType == Proxy, "Bad type!");
  NS_ASSERTION(!mObject, "Calling Initialize more than once!");
  NS_ASSERTION(!mInvalidated, "Already invalidated?!");

  mInstance = static_cast<PluginInstanceChild*>(Manager());
  NS_ASSERTION(mInstance, "Null manager?!");

  NPObject* object = CreateProxyObject();
  NS_ASSERTION(object, "Failed to create object!");

  if (!PluginModuleChild::current()->RegisterActorForNPObject(object, this)) {
    NS_ERROR("Out of memory?");
  }

  mObject = object;
}

void
PluginScriptableObjectChild::InitializeLocal(NPObject* aObject)
{
  AssertPluginThread();
  NS_ASSERTION(mType == LocalObject, "Bad type!");
  NS_ASSERTION(!mObject, "Calling Initialize more than once!");
  NS_ASSERTION(!mInvalidated, "Already invalidated?!");

  mInstance = static_cast<PluginInstanceChild*>(Manager());
  NS_ASSERTION(mInstance, "Null manager?!");

  PluginModuleChild::sBrowserFuncs.retainobject(aObject);

  NS_ASSERTION(!mProtectCount, "Should be zero!");
  mProtectCount++;

  if (!PluginModuleChild::current()->RegisterActorForNPObject(aObject, this)) {
      NS_ERROR("Out of memory?");
  }

  mObject = aObject;
}

NPObject*
PluginScriptableObjectChild::CreateProxyObject()
{
  NS_ASSERTION(mInstance, "Must have an instance!");
  NS_ASSERTION(mType == Proxy, "Shouldn't call this for non-proxy object!");

  NPClass* proxyClass = const_cast<NPClass*>(GetClass());
  NPObject* npobject =
    PluginModuleChild::sBrowserFuncs.createobject(mInstance->GetNPP(),
                                                  proxyClass);
  NS_ASSERTION(npobject, "Failed to create object?!");
  NS_ASSERTION(npobject->_class == GetClass(), "Wrong kind of object!");
  NS_ASSERTION(npobject->referenceCount == 1, "Some kind of live object!");

  ChildNPObject* object = static_cast<ChildNPObject*>(npobject);
  NS_ASSERTION(!object->invalidated, "Bad object!");
  NS_ASSERTION(!object->parent, "Bad object!");

  // We don't want to have the actor own this object but rather let the object
  // own this actor. Set the reference count to 0 here so that when the object
  // dies we will send the destructor message to the child.
  object->referenceCount = 0;
  NS_LOG_RELEASE(object, 0, "NPObject");

  object->parent = const_cast<PluginScriptableObjectChild*>(this);
  return object;
}

bool
PluginScriptableObjectChild::ResurrectProxyObject()
{
  NS_ASSERTION(mInstance, "Must have an instance already!");
  NS_ASSERTION(!mObject, "Should not have an object already!");
  NS_ASSERTION(mType == Proxy, "Shouldn't call this for non-proxy object!");

  NPObject* object = CreateProxyObject();
  if (!object) {
    NS_WARNING("Failed to create object!");
    return false;
  }

  InitializeProxy();
  NS_ASSERTION(mObject, "Initialize failed!");

  SendProtect();
  return true;
}

NPObject*
PluginScriptableObjectChild::GetObject(bool aCanResurrect)
{
  if (!mObject && aCanResurrect && !ResurrectProxyObject()) {
    NS_ERROR("Null object!");
    return nullptr;
  }
  return mObject;
}

void
PluginScriptableObjectChild::Protect()
{
  NS_ASSERTION(mObject, "No object!");
  NS_ASSERTION(mProtectCount >= 0, "Negative retain count?!");

  if (mType == LocalObject) {
    ++mProtectCount;
  }
}

void
PluginScriptableObjectChild::Unprotect()
{
  NS_ASSERTION(mObject, "Bad state!");
  NS_ASSERTION(mProtectCount >= 0, "Negative retain count?!");

  if (mType == LocalObject) {
    if (--mProtectCount == 0) {
      PluginScriptableObjectChild::Send__delete__(this);
    }
  }
}

void
PluginScriptableObjectChild::DropNPObject()
{
  NS_ASSERTION(mObject, "Invalidated object!");
  NS_ASSERTION(mObject->_class == GetClass(), "Wrong type of object!");
  NS_ASSERTION(mType == Proxy, "Shouldn't call this for non-proxy object!");

  // We think we're about to be deleted, but we could be racing with the other
  // process.
  PluginModuleChild::current()->UnregisterActorForNPObject(mObject);
  mObject = nullptr;

  SendUnprotect();
}

void
PluginScriptableObjectChild::NPObjectDestroyed()
{
  NS_ASSERTION(LocalObject == mType,
               "ScriptableDeallocate should have handled this for proxies");
  mInvalidated = true;
  mObject = nullptr;
}

bool
PluginScriptableObjectChild::AnswerInvalidate()
{
  AssertPluginThread();

  if (mInvalidated) {
    return true;
  }

  mInvalidated = true;

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (mObject->_class && mObject->_class->invalidate) {
    mObject->_class->invalidate(mObject);
  }

  Unprotect();

  return true;
}

bool
PluginScriptableObjectChild::AnswerHasMethod(PPluginIdentifierChild* aId,
                                             bool* aHasMethod)
{
  AssertPluginThread();

  if (mInvalidated) {
    NS_WARNING("Calling AnswerHasMethod with an invalidated object!");
    *aHasMethod = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->hasMethod)) {
    *aHasMethod = false;
    return true;
  }

  PluginIdentifierChild::StackIdentifier id(aId);
  *aHasMethod = mObject->_class->hasMethod(mObject, id->ToNPIdentifier());
  return true;
}

bool
PluginScriptableObjectChild::AnswerInvoke(PPluginIdentifierChild* aId,
                                          const InfallibleTArray<Variant>& aArgs,
                                          Variant* aResult,
                                          bool* aSuccess)
{
  AssertPluginThread();

  if (mInvalidated) {
    NS_WARNING("Calling AnswerInvoke with an invalidated object!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->invoke)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  AutoFallibleTArray<NPVariant, 10> convertedArgs;
  uint32_t argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  for (uint32_t index = 0; index < argCount; index++) {
    ConvertToVariant(aArgs[index], convertedArgs[index]);
  }

  NPVariant result;
  VOID_TO_NPVARIANT(result);
  PluginIdentifierChild::StackIdentifier id(aId);
  bool success = mObject->_class->invoke(mObject, id->ToNPIdentifier(),
                                         convertedArgs.Elements(), argCount,
                                         &result);

  for (uint32_t index = 0; index < argCount; index++) {
    PluginModuleChild::sBrowserFuncs.releasevariantvalue(&convertedArgs[index]);
  }

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  Variant convertedResult;
  success = ConvertToRemoteVariant(result, convertedResult, GetInstance(),
                                   false);

  DeferNPVariantLastRelease(&PluginModuleChild::sBrowserFuncs, &result);

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  *aSuccess = true;
  *aResult = convertedResult;
  return true;
}

bool
PluginScriptableObjectChild::AnswerInvokeDefault(const InfallibleTArray<Variant>& aArgs,
                                                 Variant* aResult,
                                                 bool* aSuccess)
{
  AssertPluginThread();

  if (mInvalidated) {
    NS_WARNING("Calling AnswerInvokeDefault with an invalidated object!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->invokeDefault)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  AutoFallibleTArray<NPVariant, 10> convertedArgs;
  uint32_t argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  for (uint32_t index = 0; index < argCount; index++) {
    ConvertToVariant(aArgs[index], convertedArgs[index]);
  }

  NPVariant result;
  VOID_TO_NPVARIANT(result);
  bool success = mObject->_class->invokeDefault(mObject,
                                                convertedArgs.Elements(),
                                                argCount, &result);

  for (uint32_t index = 0; index < argCount; index++) {
    PluginModuleChild::sBrowserFuncs.releasevariantvalue(&convertedArgs[index]);
  }

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  Variant convertedResult;
  success = ConvertToRemoteVariant(result, convertedResult, GetInstance(),
                                   false);

  DeferNPVariantLastRelease(&PluginModuleChild::sBrowserFuncs, &result);

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  *aResult = convertedResult;
  *aSuccess = true;
  return true;
}

bool
PluginScriptableObjectChild::AnswerHasProperty(PPluginIdentifierChild* aId,
                                               bool* aHasProperty)
{
  AssertPluginThread();

  if (mInvalidated) {
    NS_WARNING("Calling AnswerHasProperty with an invalidated object!");
    *aHasProperty = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->hasProperty)) {
    *aHasProperty = false;
    return true;
  }

  PluginIdentifierChild::StackIdentifier id(aId);
  *aHasProperty = mObject->_class->hasProperty(mObject, id->ToNPIdentifier());
  return true;
}

bool
PluginScriptableObjectChild::AnswerGetChildProperty(PPluginIdentifierChild* aId,
                                                    bool* aHasProperty,
                                                    bool* aHasMethod,
                                                    Variant* aResult,
                                                    bool* aSuccess)
{
  AssertPluginThread();

  *aHasProperty = *aHasMethod = *aSuccess = false;
  *aResult = void_t();

  if (mInvalidated) {
    NS_WARNING("Calling AnswerGetProperty with an invalidated object!");
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->hasProperty &&
        mObject->_class->hasMethod && mObject->_class->getProperty)) {
    return true;
  }

  PluginIdentifierChild::StackIdentifier stackID(aId);
  NPIdentifier id = stackID->ToNPIdentifier();

  *aHasProperty = mObject->_class->hasProperty(mObject, id);
  *aHasMethod = mObject->_class->hasMethod(mObject, id);

  if (*aHasProperty) {
    NPVariant result;
    VOID_TO_NPVARIANT(result);

    if (!mObject->_class->getProperty(mObject, id, &result)) {
      return true;
    }

    Variant converted;
    if ((*aSuccess = ConvertToRemoteVariant(result, converted, GetInstance(),
                                            false))) {
      DeferNPVariantLastRelease(&PluginModuleChild::sBrowserFuncs, &result);
      *aResult = converted;
    }
  }

  return true;
}

bool
PluginScriptableObjectChild::AnswerSetProperty(PPluginIdentifierChild* aId,
                                               const Variant& aValue,
                                               bool* aSuccess)
{
  AssertPluginThread();

  if (mInvalidated) {
    NS_WARNING("Calling AnswerSetProperty with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->hasProperty &&
        mObject->_class->setProperty)) {
    *aSuccess = false;
    return true;
  }

  PluginIdentifierChild::StackIdentifier stackID(aId);
  NPIdentifier id = stackID->ToNPIdentifier();

  if (!mObject->_class->hasProperty(mObject, id)) {
    *aSuccess = false;
    return true;
  }

  NPVariant converted;
  ConvertToVariant(aValue, converted);

  if ((*aSuccess = mObject->_class->setProperty(mObject, id, &converted))) {
    PluginModuleChild::sBrowserFuncs.releasevariantvalue(&converted);
  }
  return true;
}

bool
PluginScriptableObjectChild::AnswerRemoveProperty(PPluginIdentifierChild* aId,
                                                  bool* aSuccess)
{
  AssertPluginThread();

  if (mInvalidated) {
    NS_WARNING("Calling AnswerRemoveProperty with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->hasProperty &&
        mObject->_class->removeProperty)) {
    *aSuccess = false;
    return true;
  }

  PluginIdentifierChild::StackIdentifier stackID(aId);
  NPIdentifier id = stackID->ToNPIdentifier();
  *aSuccess = mObject->_class->hasProperty(mObject, id) ?
              mObject->_class->removeProperty(mObject, id) :
              true;

  return true;
}

bool
PluginScriptableObjectChild::AnswerEnumerate(InfallibleTArray<PPluginIdentifierChild*>* aProperties,
                                             bool* aSuccess)
{
  AssertPluginThread();

  if (mInvalidated) {
    NS_WARNING("Calling AnswerEnumerate with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->enumerate)) {
    *aSuccess = false;
    return true;
  }

  NPIdentifier* ids;
  uint32_t idCount;
  if (!mObject->_class->enumerate(mObject, &ids, &idCount)) {
    *aSuccess = false;
    return true;
  }

  aProperties->SetCapacity(idCount);

  for (uint32_t index = 0; index < idCount; index++) {
    PluginIdentifierChild* id = static_cast<PluginIdentifierChild*>(ids[index]);
    aProperties->AppendElement(id);
  }

  PluginModuleChild::sBrowserFuncs.memfree(ids);
  *aSuccess = true;
  return true;
}

bool
PluginScriptableObjectChild::AnswerConstruct(const InfallibleTArray<Variant>& aArgs,
                                             Variant* aResult,
                                             bool* aSuccess)
{
  AssertPluginThread();

  if (mInvalidated) {
    NS_WARNING("Calling AnswerConstruct with an invalidated object!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->construct)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  AutoFallibleTArray<NPVariant, 10> convertedArgs;
  uint32_t argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  for (uint32_t index = 0; index < argCount; index++) {
    ConvertToVariant(aArgs[index], convertedArgs[index]);
  }

  NPVariant result;
  VOID_TO_NPVARIANT(result);
  bool success = mObject->_class->construct(mObject, convertedArgs.Elements(),
                                            argCount, &result);

  for (uint32_t index = 0; index < argCount; index++) {
    PluginModuleChild::sBrowserFuncs.releasevariantvalue(&convertedArgs[index]);
  }

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  Variant convertedResult;
  success = ConvertToRemoteVariant(result, convertedResult, GetInstance(),
                                   false);

  DeferNPVariantLastRelease(&PluginModuleChild::sBrowserFuncs, &result);

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  *aResult = convertedResult;
  *aSuccess = true;
  return true;
}

bool
PluginScriptableObjectChild::RecvProtect()
{
  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  Protect();
  return true;
}

bool
PluginScriptableObjectChild::RecvUnprotect()
{
  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  Unprotect();
  return true;
}

bool
PluginScriptableObjectChild::Evaluate(NPString* aScript,
                                      NPVariant* aResult)
{
  nsDependentCString script("");
  if (aScript->UTF8Characters && aScript->UTF8Length) {
    script.Rebind(aScript->UTF8Characters, aScript->UTF8Length);
  }

  bool success;
  Variant result;
  CallNPN_Evaluate(script, &result, &success);

  if (!success) {
    return false;
  }

  ConvertToVariant(result, *aResult);
  return true;
}
