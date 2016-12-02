/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginScriptableObjectChild.h"
#include "PluginScriptableObjectUtils.h"
#include "mozilla/plugins/PluginTypes.h"

using namespace mozilla::plugins;

/**
 * NPIdentifiers in the plugin process use a tagged representation. The low bit
 * stores the tag. If it's zero, the identifier is a string, and the value is a
 * pointer to a StoredIdentifier. If the tag bit is 1, then the rest of the
 * NPIdentifier value is the integer itself. Like the JSAPI, we require that all
 * integers stored in NPIdentifier be non-negative.
 *
 * String identifiers are stored in the sIdentifiers hashtable to ensure
 * uniqueness. The lifetime of these identifiers is only as long as the incoming
 * IPC call from the chrome process. If the plugin wants to retain an
 * identifier, it needs to call NPN_GetStringIdentifier, which causes the
 * mPermanent flag to be set on the identifier. When this flag is set, the
 * identifier is saved until the plugin process exits.
 *
 * The StackIdentifier RAII class is used to manage ownership of
 * identifiers. Any identifier obtained from this class should not be used
 * outside its scope, except when the MakePermanent() method has been called on
 * it.
 *
 * The lifetime of an NPIdentifier in the plugin process is totally divorced
 * from the lifetime of an NPIdentifier in the chrome process (where an
 * NPIdentifier is stored as a jsid). The JS GC in the chrome process is able to
 * trace through the entire heap, unlike in the plugin process, so there is no
 * reason to retain identifiers there.
 */

PluginScriptableObjectChild::IdentifierTable PluginScriptableObjectChild::sIdentifiers;

/* static */ PluginScriptableObjectChild::StoredIdentifier*
PluginScriptableObjectChild::HashIdentifier(const nsCString& aIdentifier)
{
  StoredIdentifier* stored = sIdentifiers.Get(aIdentifier).get();
  if (stored) {
    return stored;
  }

  stored = new StoredIdentifier(aIdentifier);
  sIdentifiers.Put(aIdentifier, stored);
  return stored;
}

/* static */ void
PluginScriptableObjectChild::UnhashIdentifier(StoredIdentifier* aStored)
{
  MOZ_ASSERT(sIdentifiers.Get(aStored->mIdentifier));
  sIdentifiers.Remove(aStored->mIdentifier);
}

/* static */ void
PluginScriptableObjectChild::ClearIdentifiers()
{
  sIdentifiers.Clear();
}

PluginScriptableObjectChild::StackIdentifier::StackIdentifier(const PluginIdentifier& aIdentifier)
: mIdentifier(aIdentifier),
  mStored(nullptr)
{
  if (aIdentifier.type() == PluginIdentifier::TnsCString) {
    mStored = PluginScriptableObjectChild::HashIdentifier(mIdentifier.get_nsCString());
  }
}

PluginScriptableObjectChild::StackIdentifier::StackIdentifier(NPIdentifier aIdentifier)
: mStored(nullptr)
{
  uintptr_t bits = reinterpret_cast<uintptr_t>(aIdentifier);
  if (bits & 1) {
    int32_t num = int32_t(bits >> 1);
    mIdentifier = PluginIdentifier(num);
  } else {
    mStored = static_cast<StoredIdentifier*>(aIdentifier);
    mIdentifier = mStored->mIdentifier;
  }
}

PluginScriptableObjectChild::StackIdentifier::~StackIdentifier()
{
  if (!mStored) {
    return;
  }

  // Each StackIdentifier owns one reference to its StoredIdentifier. In
  // addition, the sIdentifiers table owns a reference. If mPermanent is false
  // and sIdentifiers has the last reference, then we want to remove the
  // StoredIdentifier from the table (and destroy it).
  StoredIdentifier *stored = mStored;
  mStored = nullptr;
  if (stored->mRefCnt == 1 && !stored->mPermanent) {
    PluginScriptableObjectChild::UnhashIdentifier(stored);
  }
}

NPIdentifier
PluginScriptableObjectChild::StackIdentifier::ToNPIdentifier() const
{
  if (mStored) {
    MOZ_ASSERT(mIdentifier.type() == PluginIdentifier::TnsCString);
    MOZ_ASSERT((reinterpret_cast<uintptr_t>(mStored.get()) & 1) == 0);
    return mStored;
  }

  int32_t num = mIdentifier.get_int32_t();
  // The JS engine imposes this condition on int32s in jsids, so we assume it.
  MOZ_ASSERT(num >= 0);
  return reinterpret_cast<NPIdentifier>((num << 1) | 1);
}

static PluginIdentifier
FromNPIdentifier(NPIdentifier aIdentifier)
{
  PluginScriptableObjectChild::StackIdentifier stack(aIdentifier);
  return stack.GetIdentifier();
}

// static
NPObject*
PluginScriptableObjectChild::ScriptableAllocate(NPP aInstance,
                                                NPClass* aClass)
{
  AssertPluginThread();

  if (aClass != GetClass()) {
    MOZ_CRASH("Huh?! Wrong class!");
  }

  return new ChildNPObject();
}

// static
void
PluginScriptableObjectChild::ScriptableInvalidate(NPObject* aObject)
{
  AssertPluginThread();

  if (aObject->_class != GetClass()) {
    MOZ_CRASH("Don't know what kind of object this is!");
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
    MOZ_CRASH("Don't know what kind of object this is!");
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
    MOZ_CRASH("Don't know what kind of object this is!");
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
  actor->CallHasMethod(FromNPIdentifier(aName), &result);

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
    MOZ_CRASH("Don't know what kind of object this is!");
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
  actor->CallInvoke(FromNPIdentifier(aName), args,
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
    MOZ_CRASH("Don't know what kind of object this is!");
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
    MOZ_CRASH("Don't know what kind of object this is!");
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
  actor->CallHasProperty(FromNPIdentifier(aName), &result);

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
    MOZ_CRASH("Don't know what kind of object this is!");
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectChild> actor(object->parent);
  NS_ASSERTION(actor, "This shouldn't ever be null!");
  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  PluginInstanceChild::AutoStackHelper guard(actor->mInstance);

  Variant result;
  bool success;
  actor->CallGetParentProperty(FromNPIdentifier(aName),
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
    MOZ_CRASH("Don't know what kind of object this is!");
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
  actor->CallSetProperty(FromNPIdentifier(aName), value,
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
    MOZ_CRASH("Don't know what kind of object this is!");
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
  actor->CallRemoveProperty(FromNPIdentifier(aName),
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
    MOZ_CRASH("Don't know what kind of object this is!");
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectChild> actor(object->parent);
  NS_ASSERTION(actor, "This shouldn't ever be null!");
  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  AutoTArray<PluginIdentifier, 10> identifiers;
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
    StackIdentifier id(identifiers[index]);
    // Make the id permanent in case the plugin retains it.
    id.MakePermanent();
    (*aIdentifiers)[index] = id.ToNPIdentifier();
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
    MOZ_CRASH("Don't know what kind of object this is!");
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
    UnregisterActor(mObject);

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

bool
PluginScriptableObjectChild::InitializeProxy()
{
  AssertPluginThread();
  NS_ASSERTION(mType == Proxy, "Bad type!");
  NS_ASSERTION(!mObject, "Calling Initialize more than once!");
  NS_ASSERTION(!mInvalidated, "Already invalidated?!");

  mInstance = static_cast<PluginInstanceChild*>(Manager());
  NS_ASSERTION(mInstance, "Null manager?!");

  NPObject* object = CreateProxyObject();
  if (!object) {
    NS_ERROR("Failed to create object!");
    return false;
  }

  if (!RegisterActor(object)) {
    NS_ERROR("RegisterActor failed");
    return false;
  }

  mObject = object;
  return true;
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

  if (!RegisterActor(aObject)) {
    NS_ERROR("RegisterActor failed");
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

  if (!InitializeProxy()) {
    NS_ERROR("Initialize failed!");
    return false;
  }

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
  UnregisterActor(mObject);
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

mozilla::ipc::IPCResult
PluginScriptableObjectChild::AnswerInvalidate()
{
  AssertPluginThread();
  PluginInstanceChild::AutoStackHelper guard(mInstance);

  if (mInvalidated) {
    return IPC_OK();
  }

  mInvalidated = true;

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (mObject->_class && mObject->_class->invalidate) {
    mObject->_class->invalidate(mObject);
  }

  Unprotect();

  return IPC_OK();
}

mozilla::ipc::IPCResult
PluginScriptableObjectChild::AnswerHasMethod(const PluginIdentifier& aId,
                                             bool* aHasMethod)
{
  AssertPluginThread();
  PluginInstanceChild::AutoStackHelper guard(mInstance);

  if (mInvalidated) {
    NS_WARNING("Calling AnswerHasMethod with an invalidated object!");
    *aHasMethod = false;
    return IPC_OK();
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->hasMethod)) {
    *aHasMethod = false;
    return IPC_OK();
  }

  StackIdentifier id(aId);
  *aHasMethod = mObject->_class->hasMethod(mObject, id.ToNPIdentifier());
  return IPC_OK();
}

mozilla::ipc::IPCResult
PluginScriptableObjectChild::AnswerInvoke(const PluginIdentifier& aId,
                                          InfallibleTArray<Variant>&& aArgs,
                                          Variant* aResult,
                                          bool* aSuccess)
{
  AssertPluginThread();
  PluginInstanceChild::AutoStackHelper guard(mInstance);

  if (mInvalidated) {
    NS_WARNING("Calling AnswerInvoke with an invalidated object!");
    *aResult = void_t();
    *aSuccess = false;
    return IPC_OK();
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->invoke)) {
    *aResult = void_t();
    *aSuccess = false;
    return IPC_OK();
  }

  AutoTArray<NPVariant, 10> convertedArgs;
  uint32_t argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount, mozilla::fallible)) {
    *aResult = void_t();
    *aSuccess = false;
    return IPC_OK();
  }

  for (uint32_t index = 0; index < argCount; index++) {
    ConvertToVariant(aArgs[index], convertedArgs[index]);
  }

  NPVariant result;
  VOID_TO_NPVARIANT(result);
  StackIdentifier id(aId);
  bool success = mObject->_class->invoke(mObject, id.ToNPIdentifier(),
                                         convertedArgs.Elements(), argCount,
                                         &result);

  for (uint32_t index = 0; index < argCount; index++) {
    PluginModuleChild::sBrowserFuncs.releasevariantvalue(&convertedArgs[index]);
  }

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return IPC_OK();
  }

  Variant convertedResult;
  success = ConvertToRemoteVariant(result, convertedResult, GetInstance(),
                                   false);

  DeferNPVariantLastRelease(&PluginModuleChild::sBrowserFuncs, &result);

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return IPC_OK();
  }

  *aSuccess = true;
  *aResult = convertedResult;
  return IPC_OK();
}

mozilla::ipc::IPCResult
PluginScriptableObjectChild::AnswerInvokeDefault(InfallibleTArray<Variant>&& aArgs,
                                                 Variant* aResult,
                                                 bool* aSuccess)
{
  AssertPluginThread();
  PluginInstanceChild::AutoStackHelper guard(mInstance);

  if (mInvalidated) {
    NS_WARNING("Calling AnswerInvokeDefault with an invalidated object!");
    *aResult = void_t();
    *aSuccess = false;
    return IPC_OK();
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->invokeDefault)) {
    *aResult = void_t();
    *aSuccess = false;
    return IPC_OK();
  }

  AutoTArray<NPVariant, 10> convertedArgs;
  uint32_t argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount, mozilla::fallible)) {
    *aResult = void_t();
    *aSuccess = false;
    return IPC_OK();
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
    return IPC_OK();
  }

  Variant convertedResult;
  success = ConvertToRemoteVariant(result, convertedResult, GetInstance(),
                                   false);

  DeferNPVariantLastRelease(&PluginModuleChild::sBrowserFuncs, &result);

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return IPC_OK();
  }

  *aResult = convertedResult;
  *aSuccess = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult
PluginScriptableObjectChild::AnswerHasProperty(const PluginIdentifier& aId,
                                               bool* aHasProperty)
{
  AssertPluginThread();
  PluginInstanceChild::AutoStackHelper guard(mInstance);

  if (mInvalidated) {
    NS_WARNING("Calling AnswerHasProperty with an invalidated object!");
    *aHasProperty = false;
    return IPC_OK();
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->hasProperty)) {
    *aHasProperty = false;
    return IPC_OK();
  }

  StackIdentifier id(aId);
  *aHasProperty = mObject->_class->hasProperty(mObject, id.ToNPIdentifier());
  return IPC_OK();
}

mozilla::ipc::IPCResult
PluginScriptableObjectChild::AnswerGetChildProperty(const PluginIdentifier& aId,
                                                    bool* aHasProperty,
                                                    bool* aHasMethod,
                                                    Variant* aResult,
                                                    bool* aSuccess)
{
  AssertPluginThread();
  PluginInstanceChild::AutoStackHelper guard(mInstance);

  *aHasProperty = *aHasMethod = *aSuccess = false;
  *aResult = void_t();

  if (mInvalidated) {
    NS_WARNING("Calling AnswerGetProperty with an invalidated object!");
    return IPC_OK();
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->hasProperty &&
        mObject->_class->hasMethod && mObject->_class->getProperty)) {
    return IPC_OK();
  }

  StackIdentifier stackID(aId);
  NPIdentifier id = stackID.ToNPIdentifier();

  *aHasProperty = mObject->_class->hasProperty(mObject, id);
  *aHasMethod = mObject->_class->hasMethod(mObject, id);

  if (*aHasProperty) {
    NPVariant result;
    VOID_TO_NPVARIANT(result);

    if (!mObject->_class->getProperty(mObject, id, &result)) {
      return IPC_OK();
    }

    Variant converted;
    if ((*aSuccess = ConvertToRemoteVariant(result, converted, GetInstance(),
                                            false))) {
      DeferNPVariantLastRelease(&PluginModuleChild::sBrowserFuncs, &result);
      *aResult = converted;
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
PluginScriptableObjectChild::AnswerSetProperty(const PluginIdentifier& aId,
                                               const Variant& aValue,
                                               bool* aSuccess)
{
  AssertPluginThread();
  PluginInstanceChild::AutoStackHelper guard(mInstance);

  if (mInvalidated) {
    NS_WARNING("Calling AnswerSetProperty with an invalidated object!");
    *aSuccess = false;
    return IPC_OK();
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->hasProperty &&
        mObject->_class->setProperty)) {
    *aSuccess = false;
    return IPC_OK();
  }

  StackIdentifier stackID(aId);
  NPIdentifier id = stackID.ToNPIdentifier();

  if (!mObject->_class->hasProperty(mObject, id)) {
    *aSuccess = false;
    return IPC_OK();
  }

  NPVariant converted;
  ConvertToVariant(aValue, converted);

  if ((*aSuccess = mObject->_class->setProperty(mObject, id, &converted))) {
    PluginModuleChild::sBrowserFuncs.releasevariantvalue(&converted);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
PluginScriptableObjectChild::AnswerRemoveProperty(const PluginIdentifier& aId,
                                                  bool* aSuccess)
{
  AssertPluginThread();
  PluginInstanceChild::AutoStackHelper guard(mInstance);

  if (mInvalidated) {
    NS_WARNING("Calling AnswerRemoveProperty with an invalidated object!");
    *aSuccess = false;
    return IPC_OK();
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->hasProperty &&
        mObject->_class->removeProperty)) {
    *aSuccess = false;
    return IPC_OK();
  }

  StackIdentifier stackID(aId);
  NPIdentifier id = stackID.ToNPIdentifier();
  *aSuccess = mObject->_class->hasProperty(mObject, id) ?
              mObject->_class->removeProperty(mObject, id) :
              true;

  return IPC_OK();
}

mozilla::ipc::IPCResult
PluginScriptableObjectChild::AnswerEnumerate(InfallibleTArray<PluginIdentifier>* aProperties,
                                             bool* aSuccess)
{
  AssertPluginThread();
  PluginInstanceChild::AutoStackHelper guard(mInstance);

  if (mInvalidated) {
    NS_WARNING("Calling AnswerEnumerate with an invalidated object!");
    *aSuccess = false;
    return IPC_OK();
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->enumerate)) {
    *aSuccess = false;
    return IPC_OK();
  }

  NPIdentifier* ids;
  uint32_t idCount;
  if (!mObject->_class->enumerate(mObject, &ids, &idCount)) {
    *aSuccess = false;
    return IPC_OK();
  }

  aProperties->SetCapacity(idCount);

  for (uint32_t index = 0; index < idCount; index++) {
    aProperties->AppendElement(FromNPIdentifier(ids[index]));
  }

  PluginModuleChild::sBrowserFuncs.memfree(ids);
  *aSuccess = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult
PluginScriptableObjectChild::AnswerConstruct(InfallibleTArray<Variant>&& aArgs,
                                             Variant* aResult,
                                             bool* aSuccess)
{
  AssertPluginThread();
  PluginInstanceChild::AutoStackHelper guard(mInstance);

  if (mInvalidated) {
    NS_WARNING("Calling AnswerConstruct with an invalidated object!");
    *aResult = void_t();
    *aSuccess = false;
    return IPC_OK();
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  if (!(mObject->_class && mObject->_class->construct)) {
    *aResult = void_t();
    *aSuccess = false;
    return IPC_OK();
  }

  AutoTArray<NPVariant, 10> convertedArgs;
  uint32_t argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount, mozilla::fallible)) {
    *aResult = void_t();
    *aSuccess = false;
    return IPC_OK();
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
    return IPC_OK();
  }

  Variant convertedResult;
  success = ConvertToRemoteVariant(result, convertedResult, GetInstance(),
                                   false);

  DeferNPVariantLastRelease(&PluginModuleChild::sBrowserFuncs, &result);

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return IPC_OK();
  }

  *aResult = convertedResult;
  *aSuccess = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult
PluginScriptableObjectChild::RecvProtect()
{
  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  Protect();
  return IPC_OK();
}

mozilla::ipc::IPCResult
PluginScriptableObjectChild::RecvUnprotect()
{
  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  Unprotect();
  return IPC_OK();
}

bool
PluginScriptableObjectChild::Evaluate(NPString* aScript,
                                      NPVariant* aResult)
{
  PluginInstanceChild::AutoStackHelper guard(mInstance);

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

nsTHashtable<PluginScriptableObjectChild::NPObjectData>* PluginScriptableObjectChild::sObjectMap;

bool
PluginScriptableObjectChild::RegisterActor(NPObject* aObject)
{
  AssertPluginThread();
  MOZ_ASSERT(aObject, "Null pointer!");

  NPObjectData* d = sObjectMap->GetEntry(aObject);
  if (!d) {
    NS_ERROR("NPObject not in object table");
    return false;
  }

  d->actor = this;
  return true;
}

void
PluginScriptableObjectChild::UnregisterActor(NPObject* aObject)
{
  AssertPluginThread();
  MOZ_ASSERT(aObject, "Null pointer!");

  NPObjectData* d = sObjectMap->GetEntry(aObject);
  MOZ_ASSERT(d, "NPObject not in object table");
  if (d) {
    d->actor = nullptr;
  }
}

/* static */ PluginScriptableObjectChild*
PluginScriptableObjectChild::GetActorForNPObject(NPObject* aObject)
{
  AssertPluginThread();
  MOZ_ASSERT(aObject, "Null pointer!");

  NPObjectData* d = sObjectMap->GetEntry(aObject);
  if (!d) {
    NS_ERROR("Plugin using object not created with NPN_CreateObject?");
    return nullptr;
  }

  return d->actor;
}

/* static */ void
PluginScriptableObjectChild::RegisterObject(NPObject* aObject, PluginInstanceChild* aInstance)
{
  AssertPluginThread();

  if (!sObjectMap) {
    sObjectMap = new nsTHashtable<PluginScriptableObjectChild::NPObjectData>();
  }

  NPObjectData* d = sObjectMap->PutEntry(aObject);
  MOZ_ASSERT(!d->instance, "New NPObject already mapped?");
  d->instance = aInstance;
}

/* static */ void
PluginScriptableObjectChild::UnregisterObject(NPObject* aObject)
{
  AssertPluginThread();

  sObjectMap->RemoveEntry(aObject);

  if (!sObjectMap->Count()) {
    delete sObjectMap;
    sObjectMap = nullptr;
  }
}

/* static */ PluginInstanceChild*
PluginScriptableObjectChild::GetInstanceForNPObject(NPObject* aObject)
{
  AssertPluginThread();
  NPObjectData* d = sObjectMap->GetEntry(aObject);
  if (!d) {
    return nullptr;
  }
  return d->instance;
}

/* static */ void
PluginScriptableObjectChild::NotifyOfInstanceShutdown(PluginInstanceChild* aInstance)
{
  AssertPluginThread();
  if (!sObjectMap) {
    return;
  }

  for (auto iter = sObjectMap->Iter(); !iter.Done(); iter.Next()) {
    NPObjectData* d = iter.Get();
    if (d->instance == aInstance) {
        NPObject* o = d->GetKey();
        aInstance->mDeletingHash->PutEntry(o);
    }
  }
}
