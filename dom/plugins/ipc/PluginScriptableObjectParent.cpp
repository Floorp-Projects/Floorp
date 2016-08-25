/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginScriptableObjectParent.h"

#include "jsapi.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/plugins/PluginTypes.h"
#include "mozilla/Unused.h"
#include "nsNPAPIPlugin.h"
#include "PluginAsyncSurrogate.h"
#include "PluginScriptableObjectUtils.h"

using namespace mozilla;
using namespace mozilla::plugins;
using namespace mozilla::plugins::parent;

/**
 * NPIdentifiers in the chrome process are stored as jsids. The difficulty is in
 * ensuring that string identifiers are rooted without pinning them all. We
 * assume that all NPIdentifiers passed into nsJSNPRuntime will not be used
 * outside the scope of the NPAPI call (i.e., they won't be stored in the
 * heap). Rooting is done using the StackIdentifier class, which roots the
 * identifier via RootedId.
 *
 * This system does not allow jsids to be moved, as would be needed for
 * generational or compacting GC. When Firefox implements a moving GC for
 * strings, we will need to ensure that no movement happens while NPAPI code is
 * on the stack: although StackIdentifier roots all identifiers used, the GC has
 * no way to know that a jsid cast to an NPIdentifier needs to be fixed up if it
 * is moved.
 */

class MOZ_STACK_CLASS StackIdentifier
{
public:
  explicit StackIdentifier(const PluginIdentifier& aIdentifier,
                           bool aAtomizeAndPin = false);

  bool Failed() const { return mFailed; }
  NPIdentifier ToNPIdentifier() const { return mIdentifier; }

private:
  bool mFailed;
  NPIdentifier mIdentifier;
  AutoSafeJSContext mCx;
  JS::RootedId mId;
};

StackIdentifier::StackIdentifier(const PluginIdentifier& aIdentifier, bool aAtomizeAndPin)
: mFailed(false),
  mId(mCx)
{
  if (aIdentifier.type() == PluginIdentifier::TnsCString) {
    // We don't call _getstringidentifier because we may not want to intern the string.
    NS_ConvertUTF8toUTF16 utf16name(aIdentifier.get_nsCString());
    JS::RootedString str(mCx, JS_NewUCStringCopyN(mCx, utf16name.get(), utf16name.Length()));
    if (!str) {
      NS_ERROR("Id can't be allocated");
      mFailed = true;
      return;
    }
    if (aAtomizeAndPin) {
      str = JS_AtomizeAndPinJSString(mCx, str);
      if (!str) {
        NS_ERROR("Id can't be allocated");
        mFailed = true;
        return;
      }
    }
    if (!JS_StringToId(mCx, str, &mId)) {
      NS_ERROR("Id can't be allocated");
      mFailed = true;
      return;
    }
    mIdentifier = JSIdToNPIdentifier(mId);
    return;
  }

  mIdentifier = mozilla::plugins::parent::_getintidentifier(aIdentifier.get_int32_t());
}

static bool
FromNPIdentifier(NPIdentifier aIdentifier, PluginIdentifier* aResult)
{
  if (mozilla::plugins::parent::_identifierisstring(aIdentifier)) {
    nsCString string;
    NPUTF8* chars =
      mozilla::plugins::parent::_utf8fromidentifier(aIdentifier);
    if (!chars) {
      return false;
    }
    string.Adopt(chars);
    *aResult = PluginIdentifier(string);
    return true;
  }
  else {
    int32_t intval = mozilla::plugins::parent::_intfromidentifier(aIdentifier);
    *aResult = PluginIdentifier(intval);
    return true;
  }
}

namespace {

inline void
ReleaseVariant(NPVariant& aVariant,
               PluginInstanceParent* aInstance)
{
  PushSurrogateAcceptCalls acceptCalls(aInstance);
  const NPNetscapeFuncs* npn = GetNetscapeFuncs(aInstance);
  if (npn) {
    npn->releasevariantvalue(&aVariant);
  }
}

} // namespace

// static
NPObject*
PluginScriptableObjectParent::ScriptableAllocate(NPP aInstance,
                                                 NPClass* aClass)
{
  if (aClass != GetClass()) {
    NS_ERROR("Huh?! Wrong class!");
    return nullptr;
  }

  return new ParentNPObject();
}

// static
void
PluginScriptableObjectParent::ScriptableInvalidate(NPObject* aObject)
{
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    // This can happen more than once, and is just fine.
    return;
  }

  object->invalidated = true;

  // |object->parent| may be null already if the instance has gone away.
  if (object->parent && !object->parent->CallInvalidate()) {
    NS_ERROR("Failed to send message!");
  }
}

// static
void
PluginScriptableObjectParent::ScriptableDeallocate(NPObject* aObject)
{
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);

  if (object->asyncWrapperCount > 0) {
    // In this case we should just drop the refcount to the asyncWrapperCount
    // instead of deallocating because there are still some async wrappers
    // out there that are referencing this object.
    object->referenceCount = object->asyncWrapperCount;
    return;
  }

  PluginScriptableObjectParent* actor = object->parent;
  if (actor) {
    NS_ASSERTION(actor->Type() == Proxy, "Bad type!");
    actor->DropNPObject();
  }

  delete object;
}

// static
bool
PluginScriptableObjectParent::ScriptableHasMethod(NPObject* aObject,
                                                  NPIdentifier aName)
{
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectParent> actor(object->parent);
  if (!actor) {
    return false;
  }

  PluginIdentifier identifier;
  if (!FromNPIdentifier(aName, &identifier)) {
    return false;
  }

  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  bool result;
  if (!actor->CallHasMethod(identifier, &result)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  return result;
}

// static
bool
PluginScriptableObjectParent::ScriptableInvoke(NPObject* aObject,
                                               NPIdentifier aName,
                                               const NPVariant* aArgs,
                                               uint32_t aArgCount,
                                               NPVariant* aResult)
{
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectParent> actor(object->parent);
  if (!actor) {
    return false;
  }

  PluginIdentifier identifier;
  if (!FromNPIdentifier(aName, &identifier)) {
    return false;
  }

  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  ProtectedVariantArray args(aArgs, aArgCount, actor->GetInstance());
  if (!args.IsOk()) {
    NS_ERROR("Failed to convert arguments!");
    return false;
  }

  Variant remoteResult;
  bool success;
  if (!actor->CallInvoke(identifier, args, &remoteResult,
                         &success)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  if (!success) {
    return false;
  }

  if (!ConvertToVariant(remoteResult, *aResult, actor->GetInstance())) {
    NS_WARNING("Failed to convert result!");
    return false;
  }
  return true;
}

// static
bool
PluginScriptableObjectParent::ScriptableInvokeDefault(NPObject* aObject,
                                                      const NPVariant* aArgs,
                                                      uint32_t aArgCount,
                                                      NPVariant* aResult)
{
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectParent> actor(object->parent);
  if (!actor) {
    return false;
  }

  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  ProtectedVariantArray args(aArgs, aArgCount, actor->GetInstance());
  if (!args.IsOk()) {
    NS_ERROR("Failed to convert arguments!");
    return false;
  }

  Variant remoteResult;
  bool success;
  if (!actor->CallInvokeDefault(args, &remoteResult, &success)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  if (!success) {
    return false;
  }

  if (!ConvertToVariant(remoteResult, *aResult, actor->GetInstance())) {
    NS_WARNING("Failed to convert result!");
    return false;
  }
  return true;
}

// static
bool
PluginScriptableObjectParent::ScriptableHasProperty(NPObject* aObject,
                                                    NPIdentifier aName)
{
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectParent> actor(object->parent);
  if (!actor) {
    return false;
  }

  PluginIdentifier identifier;
  if (!FromNPIdentifier(aName, &identifier)) {
    return false;
  }

  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  bool result;
  if (!actor->CallHasProperty(identifier, &result)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  return result;
}

// static
bool
PluginScriptableObjectParent::ScriptableGetProperty(NPObject* aObject,
                                                    NPIdentifier aName,
                                                    NPVariant* aResult)
{
  // See GetPropertyHelper below.
  NS_NOTREACHED("Shouldn't ever call this directly!");
  return false;
}

// static
bool
PluginScriptableObjectParent::ScriptableSetProperty(NPObject* aObject,
                                                    NPIdentifier aName,
                                                    const NPVariant* aValue)
{
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectParent> actor(object->parent);
  if (!actor) {
    return false;
  }

  PluginIdentifier identifier;
  if (!FromNPIdentifier(aName, &identifier)) {
    return false;
  }

  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  ProtectedVariant value(*aValue, actor->GetInstance());
  if (!value.IsOk()) {
    NS_WARNING("Failed to convert variant!");
    return false;
  }

  bool success;
  if (!actor->CallSetProperty(identifier, value, &success)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  return success;
}

// static
bool
PluginScriptableObjectParent::ScriptableRemoveProperty(NPObject* aObject,
                                                       NPIdentifier aName)
{
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectParent> actor(object->parent);
  if (!actor) {
    return false;
  }

  PluginIdentifier identifier;
  if (!FromNPIdentifier(aName, &identifier)) {
    return false;
  }

  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  bool success;
  if (!actor->CallRemoveProperty(identifier, &success)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  return success;
}

// static
bool
PluginScriptableObjectParent::ScriptableEnumerate(NPObject* aObject,
                                                  NPIdentifier** aIdentifiers,
                                                  uint32_t* aCount)
{
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectParent> actor(object->parent);
  if (!actor) {
    return false;
  }

  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  const NPNetscapeFuncs* npn = GetNetscapeFuncs(aObject);
  if (!npn) {
    NS_ERROR("No netscape funcs!");
    return false;
  }

  AutoTArray<PluginIdentifier, 10> identifiers;
  bool success;
  if (!actor->CallEnumerate(&identifiers, &success)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  if (!success) {
    return false;
  }

  *aCount = identifiers.Length();
  if (!*aCount) {
    *aIdentifiers = nullptr;
    return true;
  }

  *aIdentifiers = (NPIdentifier*)npn->memalloc(*aCount * sizeof(NPIdentifier));
  if (!*aIdentifiers) {
    NS_ERROR("Out of memory!");
    return false;
  }

  for (uint32_t index = 0; index < *aCount; index++) {
    // We pin the ID to avoid a GC hazard here. This could probably be fixed
    // if the interface with nsJSNPRuntime were smarter.
    StackIdentifier stackID(identifiers[index], true /* aAtomizeAndPin */);
    if (stackID.Failed()) {
      return false;
    }
    (*aIdentifiers)[index] = stackID.ToNPIdentifier();
  }
  return true;
}

// static
bool
PluginScriptableObjectParent::ScriptableConstruct(NPObject* aObject,
                                                  const NPVariant* aArgs,
                                                  uint32_t aArgCount,
                                                  NPVariant* aResult)
{
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  ProtectedActor<PluginScriptableObjectParent> actor(object->parent);
  if (!actor) {
    return false;
  }

  NS_ASSERTION(actor->Type() == Proxy, "Bad type!");

  ProtectedVariantArray args(aArgs, aArgCount, actor->GetInstance());
  if (!args.IsOk()) {
    NS_ERROR("Failed to convert arguments!");
    return false;
  }

  Variant remoteResult;
  bool success;
  if (!actor->CallConstruct(args, &remoteResult, &success)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  if (!success) {
    return false;
  }

  if (!ConvertToVariant(remoteResult, *aResult, actor->GetInstance())) {
    NS_WARNING("Failed to convert result!");
    return false;
  }
  return true;
}

const NPClass PluginScriptableObjectParent::sNPClass = {
  NP_CLASS_STRUCT_VERSION,
  PluginScriptableObjectParent::ScriptableAllocate,
  PluginScriptableObjectParent::ScriptableDeallocate,
  PluginScriptableObjectParent::ScriptableInvalidate,
  PluginScriptableObjectParent::ScriptableHasMethod,
  PluginScriptableObjectParent::ScriptableInvoke,
  PluginScriptableObjectParent::ScriptableInvokeDefault,
  PluginScriptableObjectParent::ScriptableHasProperty,
  PluginScriptableObjectParent::ScriptableGetProperty,
  PluginScriptableObjectParent::ScriptableSetProperty,
  PluginScriptableObjectParent::ScriptableRemoveProperty,
  PluginScriptableObjectParent::ScriptableEnumerate,
  PluginScriptableObjectParent::ScriptableConstruct
};

PluginScriptableObjectParent::PluginScriptableObjectParent(
                                                     ScriptableObjectType aType)
: mInstance(nullptr),
  mObject(nullptr),
  mProtectCount(0),
  mType(aType)
{
}

PluginScriptableObjectParent::~PluginScriptableObjectParent()
{
  if (mObject) {
    if (mObject->_class == GetClass()) {
      NS_ASSERTION(mType == Proxy, "Wrong type!");
      static_cast<ParentNPObject*>(mObject)->parent = nullptr;
    }
    else {
      NS_ASSERTION(mType == LocalObject, "Wrong type!");
      GetInstance()->GetNPNIface()->releaseobject(mObject);
    }
  }
}

void
PluginScriptableObjectParent::InitializeProxy()
{
  NS_ASSERTION(mType == Proxy, "Bad type!");
  NS_ASSERTION(!mObject, "Calling Initialize more than once!");

  mInstance = static_cast<PluginInstanceParent*>(Manager());
  NS_ASSERTION(mInstance, "Null manager?!");

  NPObject* object = CreateProxyObject();
  NS_ASSERTION(object, "Failed to create object!");

  if (!mInstance->RegisterNPObjectForActor(object, this)) {
    NS_ERROR("Out of memory?");
  }

  mObject = object;
}

void
PluginScriptableObjectParent::InitializeLocal(NPObject* aObject)
{
  NS_ASSERTION(mType == LocalObject, "Bad type!");
  NS_ASSERTION(!(mInstance && mObject), "Calling Initialize more than once!");

  mInstance = static_cast<PluginInstanceParent*>(Manager());
  NS_ASSERTION(mInstance, "Null manager?!");

  mInstance->GetNPNIface()->retainobject(aObject);

  NS_ASSERTION(!mProtectCount, "Should be zero!");
  mProtectCount++;

  if (!mInstance->RegisterNPObjectForActor(aObject, this)) {
    NS_ERROR("Out of memory?");
  }

  mObject = aObject;
}

NPObject*
PluginScriptableObjectParent::CreateProxyObject()
{
  NS_ASSERTION(mInstance, "Must have an instance!");
  NS_ASSERTION(mType == Proxy, "Shouldn't call this for non-proxy object!");

  PushSurrogateAcceptCalls acceptCalls(mInstance);
  const NPNetscapeFuncs* npn = GetNetscapeFuncs(mInstance);

  NPObject* npobject = npn->createobject(mInstance->GetNPP(),
                                         const_cast<NPClass*>(GetClass()));
  NS_ASSERTION(npobject, "Failed to create object?!");
  NS_ASSERTION(npobject->_class == GetClass(), "Wrong kind of object!");
  NS_ASSERTION(npobject->referenceCount == 1, "Some kind of live object!");

  ParentNPObject* object = static_cast<ParentNPObject*>(npobject);
  NS_ASSERTION(!object->invalidated, "Bad object!");
  NS_ASSERTION(!object->parent, "Bad object!");

  // We don't want to have the actor own this object but rather let the object
  // own this actor. Set the reference count to 0 here so that when the object
  // dies we will send the destructor message to the child.
  object->referenceCount = 0;
  NS_LOG_RELEASE(object, 0, "BrowserNPObject");

  object->parent = const_cast<PluginScriptableObjectParent*>(this);
  return object;
}

bool
PluginScriptableObjectParent::ResurrectProxyObject()
{
  NS_ASSERTION(mInstance, "Must have an instance already!");
  NS_ASSERTION(!mObject, "Should not have an object already!");
  NS_ASSERTION(mType == Proxy, "Shouldn't call this for non-proxy object!");

  InitializeProxy();
  NS_ASSERTION(mObject, "Initialize failed!");

  if (!SendProtect()) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  return true;
}

NPObject*
PluginScriptableObjectParent::GetObject(bool aCanResurrect)
{
  if (!mObject && aCanResurrect && !ResurrectProxyObject()) {
    NS_ERROR("Null object!");
    return nullptr;
  }
  return mObject;
}

void
PluginScriptableObjectParent::Protect()
{
  NS_ASSERTION(mObject, "No object!");
  NS_ASSERTION(mProtectCount >= 0, "Negative protect count?!");

  if (mType == LocalObject) {
    ++mProtectCount;
  }
}

void
PluginScriptableObjectParent::Unprotect()
{
  NS_ASSERTION(mObject, "No object!");
  NS_ASSERTION(mProtectCount >= 0, "Negative protect count?!");

  if (mType == LocalObject) {
    if (--mProtectCount == 0) {
      Unused << PluginScriptableObjectParent::Send__delete__(this);
    }
  }
}

void
PluginScriptableObjectParent::DropNPObject()
{
  NS_ASSERTION(mObject, "Invalidated object!");
  NS_ASSERTION(mObject->_class == GetClass(), "Wrong type of object!");
  NS_ASSERTION(mType == Proxy, "Shouldn't call this for non-proxy object!");

  // We think we're about to be deleted, but we could be racing with the other
  // process.
  PluginInstanceParent* instance = GetInstance();
  NS_ASSERTION(instance, "Must have an instance!");

  instance->UnregisterNPObject(mObject);
  mObject = nullptr;

  Unused << SendUnprotect();
}

void
PluginScriptableObjectParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Implement me! Bug 1005163
}

bool
PluginScriptableObjectParent::AnswerHasMethod(const PluginIdentifier& aId,
                                              bool* aHasMethod)
{
  if (!mObject) {
    NS_WARNING("Calling AnswerHasMethod with an invalidated object!");
    *aHasMethod = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  PluginInstanceParent* instance = GetInstance();
  if (!instance) {
    NS_ERROR("No instance?!");
    *aHasMethod = false;
    return true;
  }

  PushSurrogateAcceptCalls acceptCalls(instance);
  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aHasMethod = false;
    return true;
  }

  StackIdentifier stackID(aId);
  if (stackID.Failed()) {
    *aHasMethod = false;
    return true;
  }
  *aHasMethod = npn->hasmethod(instance->GetNPP(), mObject, stackID.ToNPIdentifier());
  return true;
}

bool
PluginScriptableObjectParent::AnswerInvoke(const PluginIdentifier& aId,
                                           InfallibleTArray<Variant>&& aArgs,
                                           Variant* aResult,
                                           bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling AnswerInvoke with an invalidated object!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  PluginInstanceParent* instance = GetInstance();
  if (!instance) {
    NS_ERROR("No instance?!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  PushSurrogateAcceptCalls acceptCalls(instance);
  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  StackIdentifier stackID(aId);
  if (stackID.Failed()) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  AutoTArray<NPVariant, 10> convertedArgs;
  uint32_t argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount, fallible)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  for (uint32_t index = 0; index < argCount; index++) {
    if (!ConvertToVariant(aArgs[index], convertedArgs[index], instance)) {
      // Don't leak things we've already converted!
      while (index-- > 0) {
        ReleaseVariant(convertedArgs[index], instance);
      }
      *aResult = void_t();
      *aSuccess = false;
      return true;
    }
  }

  NPVariant result;
  bool success = npn->invoke(instance->GetNPP(), mObject, stackID.ToNPIdentifier(),
                             convertedArgs.Elements(), argCount, &result);

  for (uint32_t index = 0; index < argCount; index++) {
    ReleaseVariant(convertedArgs[index], instance);
  }

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  Variant convertedResult;
  success = ConvertToRemoteVariant(result, convertedResult, GetInstance());

  DeferNPVariantLastRelease(npn, &result);

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
PluginScriptableObjectParent::AnswerInvokeDefault(InfallibleTArray<Variant>&& aArgs,
                                                  Variant* aResult,
                                                  bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling AnswerInvoke with an invalidated object!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  PluginInstanceParent* instance = GetInstance();
  if (!instance) {
    NS_ERROR("No instance?!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  PushSurrogateAcceptCalls acceptCalls(instance);
  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  AutoTArray<NPVariant, 10> convertedArgs;
  uint32_t argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount, fallible)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  for (uint32_t index = 0; index < argCount; index++) {
    if (!ConvertToVariant(aArgs[index], convertedArgs[index], instance)) {
      // Don't leak things we've already converted!
      while (index-- > 0) {
        ReleaseVariant(convertedArgs[index], instance);
      }
      *aResult = void_t();
      *aSuccess = false;
      return true;
    }
  }

  NPVariant result;
  bool success = npn->invokeDefault(instance->GetNPP(), mObject,
                                    convertedArgs.Elements(), argCount,
                                    &result);

  for (uint32_t index = 0; index < argCount; index++) {
    ReleaseVariant(convertedArgs[index], instance);
  }

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  Variant convertedResult;
  success = ConvertToRemoteVariant(result, convertedResult, GetInstance());

  DeferNPVariantLastRelease(npn, &result);

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
PluginScriptableObjectParent::AnswerHasProperty(const PluginIdentifier& aId,
                                                bool* aHasProperty)
{
  if (!mObject) {
    NS_WARNING("Calling AnswerHasProperty with an invalidated object!");
    *aHasProperty = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  PluginInstanceParent* instance = GetInstance();
  if (!instance) {
    NS_ERROR("No instance?!");
    *aHasProperty = false;
    return true;
  }

  PushSurrogateAcceptCalls acceptCalls(instance);
  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aHasProperty = false;
    return true;
  }

  StackIdentifier stackID(aId);
  if (stackID.Failed()) {
    *aHasProperty = false;
    return true;
  }

  *aHasProperty = npn->hasproperty(instance->GetNPP(), mObject,
                                   stackID.ToNPIdentifier());
  return true;
}

bool
PluginScriptableObjectParent::AnswerGetParentProperty(
                                                   const PluginIdentifier& aId,
                                                   Variant* aResult,
                                                   bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling AnswerGetProperty with an invalidated object!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  PluginInstanceParent* instance = GetInstance();
  if (!instance) {
    NS_ERROR("No instance?!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  PushSurrogateAcceptCalls acceptCalls(instance);
  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  StackIdentifier stackID(aId);
  if (stackID.Failed()) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  NPVariant result;
  if (!npn->getproperty(instance->GetNPP(), mObject, stackID.ToNPIdentifier(),
                        &result)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  Variant converted;
  if ((*aSuccess = ConvertToRemoteVariant(result, converted, instance))) {
    DeferNPVariantLastRelease(npn, &result);
    *aResult = converted;
  }
  else {
    *aResult = void_t();
  }

  return true;
}

bool
PluginScriptableObjectParent::AnswerSetProperty(const PluginIdentifier& aId,
                                                const Variant& aValue,
                                                bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling AnswerSetProperty with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  PluginInstanceParent* instance = GetInstance();
  if (!instance) {
    NS_ERROR("No instance?!");
    *aSuccess = false;
    return true;
  }

  PushSurrogateAcceptCalls acceptCalls(instance);
  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aSuccess = false;
    return true;
  }

  NPVariant converted;
  if (!ConvertToVariant(aValue, converted, instance)) {
    *aSuccess = false;
    return true;
  }

  StackIdentifier stackID(aId);
  if (stackID.Failed()) {
    *aSuccess = false;
    return true;
  }

  if ((*aSuccess = npn->setproperty(instance->GetNPP(), mObject,
                                    stackID.ToNPIdentifier(), &converted))) {
    ReleaseVariant(converted, instance);
  }
  return true;
}

bool
PluginScriptableObjectParent::AnswerRemoveProperty(const PluginIdentifier& aId,
                                                   bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling AnswerRemoveProperty with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  PluginInstanceParent* instance = GetInstance();
  if (!instance) {
    NS_ERROR("No instance?!");
    *aSuccess = false;
    return true;
  }

  PushSurrogateAcceptCalls acceptCalls(instance);
  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aSuccess = false;
    return true;
  }

  StackIdentifier stackID(aId);
  if (stackID.Failed()) {
    *aSuccess = false;
    return true;
  }

  *aSuccess = npn->removeproperty(instance->GetNPP(), mObject,
                                  stackID.ToNPIdentifier());
  return true;
}

bool
PluginScriptableObjectParent::AnswerEnumerate(InfallibleTArray<PluginIdentifier>* aProperties,
                                              bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling AnswerEnumerate with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  PluginInstanceParent* instance = GetInstance();
  if (!instance) {
    NS_ERROR("No instance?!");
    *aSuccess = false;
    return true;
  }

  PushSurrogateAcceptCalls acceptCalls(instance);
  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_WARNING("No netscape funcs?!");
    *aSuccess = false;
    return true;
  }

  NPIdentifier* ids;
  uint32_t idCount;
  if (!npn->enumerate(instance->GetNPP(), mObject, &ids, &idCount)) {
    *aSuccess = false;
    return true;
  }

  aProperties->SetCapacity(idCount);

  for (uint32_t index = 0; index < idCount; index++) {
    PluginIdentifier id;
    if (!FromNPIdentifier(ids[index], &id)) {
      return false;
    }
    aProperties->AppendElement(id);
  }

  npn->memfree(ids);
  *aSuccess = true;
  return true;
}

bool
PluginScriptableObjectParent::AnswerConstruct(InfallibleTArray<Variant>&& aArgs,
                                              Variant* aResult,
                                              bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling AnswerConstruct with an invalidated object!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  PluginInstanceParent* instance = GetInstance();
  if (!instance) {
    NS_ERROR("No instance?!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  PushSurrogateAcceptCalls acceptCalls(instance);
  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  AutoTArray<NPVariant, 10> convertedArgs;
  uint32_t argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount, fallible)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  for (uint32_t index = 0; index < argCount; index++) {
    if (!ConvertToVariant(aArgs[index], convertedArgs[index], instance)) {
      // Don't leak things we've already converted!
      while (index-- > 0) {
        ReleaseVariant(convertedArgs[index], instance);
      }
      *aResult = void_t();
      *aSuccess = false;
      return true;
    }
  }

  NPVariant result;
  bool success = npn->construct(instance->GetNPP(), mObject,
                                convertedArgs.Elements(), argCount, &result);

  for (uint32_t index = 0; index < argCount; index++) {
    ReleaseVariant(convertedArgs[index], instance);
  }

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  Variant convertedResult;
  success = ConvertToRemoteVariant(result, convertedResult, instance);

  DeferNPVariantLastRelease(npn, &result);

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
PluginScriptableObjectParent::RecvProtect()
{
  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  Protect();
  return true;
}

bool
PluginScriptableObjectParent::RecvUnprotect()
{
  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
  NS_ASSERTION(mType == LocalObject, "Bad type!");

  Unprotect();
  return true;
}

bool
PluginScriptableObjectParent::AnswerNPN_Evaluate(const nsCString& aScript,
                                                 Variant* aResult,
                                                 bool* aSuccess)
{
  PluginInstanceParent* instance = GetInstance();
  if (!instance) {
    NS_ERROR("No instance?!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  PushSurrogateAcceptCalls acceptCalls(instance);
  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  NPString script = { aScript.get(), aScript.Length() };

  NPVariant result;
  bool success = npn->evaluate(instance->GetNPP(), mObject, &script, &result);
  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  Variant convertedResult;
  success = ConvertToRemoteVariant(result, convertedResult, instance);

  DeferNPVariantLastRelease(npn, &result);

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
PluginScriptableObjectParent::GetPropertyHelper(NPIdentifier aName,
                                                bool* aHasProperty,
                                                bool* aHasMethod,
                                                NPVariant* aResult)
{
  NS_ASSERTION(Type() == Proxy, "Bad type!");

  ParentNPObject* object = static_cast<ParentNPObject*>(mObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  PluginIdentifier identifier;
  if (!FromNPIdentifier(aName, &identifier)) {
    return false;
  }

  bool hasProperty, hasMethod, success;
  Variant result;
  if (!CallGetChildProperty(identifier, &hasProperty, &hasMethod, &result,
                            &success)) {
    return false;
  }

  if (!success) {
    return false;
  }

  if (!ConvertToVariant(result, *aResult, GetInstance())) {
    NS_WARNING("Failed to convert result!");
    return false;
  }

  *aHasProperty = hasProperty;
  *aHasMethod = hasMethod;
  return true;
}
