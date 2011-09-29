/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Ben Turner <bent.mozilla@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "PluginScriptableObjectParent.h"
#include "PluginScriptableObjectUtils.h"

#include "nsNPAPIPlugin.h"
#include "mozilla/unused.h"
#include "mozilla/Util.h"

using namespace mozilla::plugins;
using namespace mozilla::plugins::parent;

namespace {

typedef PluginIdentifierParent::StackIdentifier StackIdentifier;

inline void
ReleaseVariant(NPVariant& aVariant,
               PluginInstanceParent* aInstance)
{
  const NPNetscapeFuncs* npn = GetNetscapeFuncs(aInstance);
  if (npn) {
    npn->releasevariantvalue(&aVariant);
  }
}

} // anonymous namespace

// static
NPObject*
PluginScriptableObjectParent::ScriptableAllocate(NPP aInstance,
                                                 NPClass* aClass)
{
  if (aClass != GetClass()) {
    NS_ERROR("Huh?! Wrong class!");
    return nsnull;
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

  StackIdentifier identifier(aObject, aName);
  if (!identifier) {
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

  StackIdentifier identifier(aObject, aName);
  if (!identifier) {
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

  StackIdentifier identifier(aObject, aName);
  if (!identifier) {
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

  StackIdentifier identifier(aObject, aName);
  if (!identifier) {
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

  StackIdentifier identifier(aObject, aName);
  if (!identifier) {
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

  AutoInfallibleTArray<PPluginIdentifierParent*, 10> identifiers;
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
    *aIdentifiers = nsnull;
    return true;
  }

  *aIdentifiers = (NPIdentifier*)npn->memalloc(*aCount * sizeof(NPIdentifier));
  if (!*aIdentifiers) {
    NS_ERROR("Out of memory!");
    return false;
  }

  for (PRUint32 index = 0; index < *aCount; index++) {
    PluginIdentifierParent* id =
      static_cast<PluginIdentifierParent*>(identifiers[index]);
    (*aIdentifiers)[index] = id->ToNPIdentifier();
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
: mInstance(nsnull),
  mObject(nsnull),
  mProtectCount(0),
  mType(aType)
{
}

PluginScriptableObjectParent::~PluginScriptableObjectParent()
{
  if (mObject) {
    if (mObject->_class == GetClass()) {
      NS_ASSERTION(mType == Proxy, "Wrong type!");
      static_cast<ParentNPObject*>(mObject)->parent = nsnull;
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
    return nsnull;
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
      unused << PluginScriptableObjectParent::Send__delete__(this);
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
  mObject = nsnull;

  unused << SendUnprotect();
}

bool
PluginScriptableObjectParent::AnswerHasMethod(PPluginIdentifierParent* aId,
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

  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aHasMethod = false;
    return true;
  }

  PluginIdentifierParent* id = static_cast<PluginIdentifierParent*>(aId);
  *aHasMethod = npn->hasmethod(instance->GetNPP(), mObject, id->ToNPIdentifier());
  return true;
}

bool
PluginScriptableObjectParent::AnswerInvoke(PPluginIdentifierParent* aId,
                                           const InfallibleTArray<Variant>& aArgs,
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

  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  nsAutoTArray<NPVariant, 10> convertedArgs;
  PRUint32 argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  for (PRUint32 index = 0; index < argCount; index++) {
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

  PluginIdentifierParent* id = static_cast<PluginIdentifierParent*>(aId);
  NPVariant result;
  bool success = npn->invoke(instance->GetNPP(), mObject, id->ToNPIdentifier(),
                             convertedArgs.Elements(), argCount, &result);

  for (PRUint32 index = 0; index < argCount; index++) {
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
PluginScriptableObjectParent::AnswerInvokeDefault(const InfallibleTArray<Variant>& aArgs,
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

  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  nsAutoTArray<NPVariant, 10> convertedArgs;
  PRUint32 argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  for (PRUint32 index = 0; index < argCount; index++) {
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

  for (PRUint32 index = 0; index < argCount; index++) {
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
PluginScriptableObjectParent::AnswerHasProperty(PPluginIdentifierParent* aId,
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

  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aHasProperty = false;
    return true;
  }

  PluginIdentifierParent* id = static_cast<PluginIdentifierParent*>(aId);
  *aHasProperty = npn->hasproperty(instance->GetNPP(), mObject,
                                   id->ToNPIdentifier());
  return true;
}

bool
PluginScriptableObjectParent::AnswerGetParentProperty(
                                                   PPluginIdentifierParent* aId,
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

  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  PluginIdentifierParent* id = static_cast<PluginIdentifierParent*>(aId);
  NPVariant result;
  if (!npn->getproperty(instance->GetNPP(), mObject, id->ToNPIdentifier(),
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
PluginScriptableObjectParent::AnswerSetProperty(PPluginIdentifierParent* aId,
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

  PluginIdentifierParent* id = static_cast<PluginIdentifierParent*>(aId);
  if ((*aSuccess = npn->setproperty(instance->GetNPP(), mObject,
                                    id->ToNPIdentifier(), &converted))) {
    ReleaseVariant(converted, instance);
  }
  return true;
}

bool
PluginScriptableObjectParent::AnswerRemoveProperty(PPluginIdentifierParent* aId,
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

  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aSuccess = false;
    return true;
  }

  PluginIdentifierParent* id = static_cast<PluginIdentifierParent*>(aId);
  *aSuccess = npn->removeproperty(instance->GetNPP(), mObject,
                                  id->ToNPIdentifier());
  return true;
}

bool
PluginScriptableObjectParent::AnswerEnumerate(InfallibleTArray<PPluginIdentifierParent*>* aProperties,
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

  if (!aProperties->SetCapacity(idCount)) {
    npn->memfree(ids);
    *aSuccess = false;
    return true;
  }

  JSContext* cx = GetJSContext(instance->GetNPP());
  JSAutoRequest ar(cx);

  for (uint32_t index = 0; index < idCount; index++) {
    // Because of GC hazards, all identifiers returned from enumerate
    // must be made permanent.
    if (_identifierisstring(ids[index])) {
      JSString* str = NPIdentifierToString(ids[index]);
      if (!JS_StringHasBeenInterned(cx, str)) {
        DebugOnly<JSString*> str2 = JS_InternJSString(cx, str);
        NS_ASSERTION(str2 == str, "Interning a JS string which is currently an ID should return itself.");
      }
    }
    PluginIdentifierParent* id =
      instance->Module()->GetIdentifierForNPIdentifier(instance->GetNPP(), ids[index]);
    aProperties->AppendElement(id);
    NS_ASSERTION(!id->IsTemporary(), "Should only have permanent identifiers!");
  }

  npn->memfree(ids);
  *aSuccess = true;
  return true;
}

bool
PluginScriptableObjectParent::AnswerConstruct(const InfallibleTArray<Variant>& aArgs,
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

  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_ERROR("No netscape funcs?!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  nsAutoTArray<NPVariant, 10> convertedArgs;
  PRUint32 argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  for (PRUint32 index = 0; index < argCount; index++) {
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

  for (PRUint32 index = 0; index < argCount; index++) {
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

JSBool
PluginScriptableObjectParent::GetPropertyHelper(NPIdentifier aName,
                                                bool* aHasProperty,
                                                bool* aHasMethod,
                                                NPVariant* aResult)
{
  NS_ASSERTION(Type() == Proxy, "Bad type!");

  ParentNPObject* object = static_cast<ParentNPObject*>(mObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return JS_FALSE;
  }

  StackIdentifier identifier(GetInstance(), aName);
  if (!identifier) {
    return JS_FALSE;
  }

  bool hasProperty, hasMethod, success;
  Variant result;
  if (!CallGetChildProperty(identifier, &hasProperty, &hasMethod, &result,
                            &success)) {
    return JS_FALSE;
  }

  if (!success) {
    return JS_FALSE;
  }

  if (!ConvertToVariant(result, *aResult, GetInstance())) {
    NS_WARNING("Failed to convert result!");
    return JS_FALSE;
  }

  *aHasProperty = hasProperty;
  *aHasMethod = hasMethod;
  return JS_TRUE;
}
