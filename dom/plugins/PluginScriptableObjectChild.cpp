/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=4 ts=4 et :
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

#include "PluginScriptableObjectChild.h"

#include "npapi.h"
#include "npruntime.h"
#include "nsDebug.h"

#include "PluginModuleChild.h"
#include "PluginInstanceChild.h"

using namespace mozilla::plugins;
using mozilla::ipc::NPRemoteIdentifier;

namespace {

inline NPObject*
NPObjectFromVariant(const Variant& aRemoteVariant)
{
  NS_ASSERTION(aRemoteVariant.type() ==
               Variant::TPPluginScriptableObjectChild,
               "Wrong variant type!");
  PluginScriptableObjectChild* actor =
    const_cast<PluginScriptableObjectChild*>(
      reinterpret_cast<const PluginScriptableObjectChild*>(
        aRemoteVariant.get_PPluginScriptableObjectChild()));
  return actor->GetObject();
}

inline NPObject*
NPObjectFromVariant(const NPVariant& aVariant)
{
  NS_ASSERTION(NPVARIANT_IS_OBJECT(aVariant), "Wrong variant type!");
  return NPVARIANT_TO_OBJECT(aVariant);
}

void
ConvertToVariant(const Variant& aRemoteVariant,
                 NPVariant& aVariant)
{
  switch (aRemoteVariant.type()) {
    case Variant::Tvoid_t: {
      VOID_TO_NPVARIANT(aVariant);
      break;
    }

    case Variant::Tnull_t: {
      NULL_TO_NPVARIANT(aVariant);
      break;
    }

    case Variant::Tbool: {
      BOOLEAN_TO_NPVARIANT(aRemoteVariant.get_bool(), aVariant);
      break;
    }

    case Variant::Tint: {
      INT32_TO_NPVARIANT(aRemoteVariant.get_int(), aVariant);
      break;
    }

    case Variant::Tdouble: {
      DOUBLE_TO_NPVARIANT(aRemoteVariant.get_double(), aVariant);
      break;
    }

    case Variant::TnsCString: {
      const nsCString& string = aRemoteVariant.get_nsCString();
      NPUTF8* buffer = reinterpret_cast<NPUTF8*>(strdup(string.get()));
      NS_ASSERTION(buffer, "Out of memory!");
      STRINGN_TO_NPVARIANT(buffer, string.Length(), aVariant);
      break;
    }

    case Variant::TPPluginScriptableObjectChild: {
      NPObject* object = NPObjectFromVariant(aRemoteVariant);
      NS_ASSERTION(object, "Null object?!");
      PluginModuleChild::sBrowserFuncs.retainobject(object);
      OBJECT_TO_NPVARIANT(object, aVariant);
      break;
    }

    default:
      NS_RUNTIMEABORT("Shouldn't get here!");
  }
}

bool
ConvertToRemoteVariant(const NPVariant& aVariant,
                       Variant& aRemoteVariant,
                       PluginInstanceChild* aInstance)
{
  if (NPVARIANT_IS_VOID(aVariant)) {
    aRemoteVariant = mozilla::void_t();
  }
  else if (NPVARIANT_IS_NULL(aVariant)) {
    aRemoteVariant = mozilla::null_t();
  }
  else if (NPVARIANT_IS_BOOLEAN(aVariant)) {
    aRemoteVariant = NPVARIANT_TO_BOOLEAN(aVariant);
  }
  else if (NPVARIANT_IS_INT32(aVariant)) {
    aRemoteVariant = NPVARIANT_TO_INT32(aVariant);
  }
  else if (NPVARIANT_IS_DOUBLE(aVariant)) {
    aRemoteVariant = NPVARIANT_TO_DOUBLE(aVariant);
  }
  else if (NPVARIANT_IS_STRING(aVariant)) {
    NPString str = NPVARIANT_TO_STRING(aVariant);
    nsCString string(str.UTF8Characters, str.UTF8Length);
    aRemoteVariant = string;
  }
  else if (NPVARIANT_IS_OBJECT(aVariant)) {
    NS_ASSERTION(aInstance, "Must have an instance to wrap!");

    NPObject* object = NPVARIANT_TO_OBJECT(aVariant);
    NS_ASSERTION(object, "Null object?!");

    PluginScriptableObjectChild* actor = aInstance->GetActorForNPObject(object);
    if (!actor) {
      NS_ERROR("Failed to create actor!");
      return false;
    }
    aRemoteVariant = actor;
  }
  else {
    NS_NOTREACHED("Shouldn't get here!");
    return false;
  }

  return true;
}

} // anonymous namespace

// static
NPObject*
PluginScriptableObjectChild::ScriptableAllocate(NPP aInstance,
                                                NPClass* aClass)
{
  AssertPluginThread();

  NS_ASSERTION(aClass == PluginScriptableObjectChild::GetClass(),
               "Huh?! Wrong class!");

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(
    PluginModuleChild::sBrowserFuncs.memalloc(sizeof(ChildNPObject)));
  if (object) {
    memset(object, 0, sizeof(ChildNPObject));
  }
  return object;
}

// static
void
PluginScriptableObjectChild::ScriptableInvalidate(NPObject* aObject)
{
  AssertPluginThread();

  if (aObject->_class != PluginScriptableObjectChild::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return;
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    // This can happen more than once, and is just fine.
    return;
  }

  PluginScriptableObjectChild* actor = object->parent;

  PluginInstanceChild* instance = actor ? actor->GetInstance() : nsnull;
  NS_WARN_IF_FALSE(instance, "No instance!");

  if (actor && !actor->CallInvalidate()) {
    NS_WARNING("Failed to send message!");
  }

  object->invalidated = true;

  if (instance &&
      !instance->CallPPluginScriptableObjectDestructor(object->parent)) {
    NS_WARNING("Failed to send message!");
  }
}

// static
void
PluginScriptableObjectChild::ScriptableDeallocate(NPObject* aObject)
{
  AssertPluginThread();

  if (aObject->_class != PluginScriptableObjectChild::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return;
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (!object->invalidated) {
    ScriptableInvalidate(aObject);
  }

  NS_ASSERTION(object->invalidated, "Should be invalidated!");

  NS_Free(aObject);
}

// static
bool
PluginScriptableObjectChild::ScriptableHasMethod(NPObject* aObject,
                                                 NPIdentifier aName)
{
  AssertPluginThread();

  if (aObject->_class != PluginScriptableObjectChild::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  PluginScriptableObjectChild* actor = object->parent;
  NS_ASSERTION(actor, "This shouldn't ever be null!");

  bool result;
  if (!actor->CallHasMethod((NPRemoteIdentifier)aName, &result)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

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

  if (aObject->_class != PluginScriptableObjectChild::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  PluginScriptableObjectChild* actor = object->parent;
  NS_ASSERTION(actor, "This shouldn't ever be null!");

  nsAutoTArray<Variant, 10> args;
  if (!args.SetLength(aArgCount)) {
    NS_ERROR("Out of memory?!");
    return false;
  }

  for (PRUint32 index = 0; index < aArgCount; index++) {
    Variant& arg = args[index];
    if (!ConvertToRemoteVariant(aArgs[index], arg, actor->GetInstance())) {
      NS_WARNING("Failed to convert argument!");
      return false;
    }
  }

  Variant remoteResult;
  bool success;
  if (!actor->CallInvoke((NPRemoteIdentifier)aName, args, &remoteResult,
                          &success)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

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

  if (aObject->_class != PluginScriptableObjectChild::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  PluginScriptableObjectChild* actor = object->parent;
  NS_ASSERTION(actor, "This shouldn't ever be null!");

  nsAutoTArray<Variant, 10> args;
  if (!args.SetLength(aArgCount)) {
    NS_ERROR("Out of memory?!");
    return false;
  }

  for (PRUint32 index = 0; index < aArgCount; index++) {
    Variant& arg = args[index];
    if (!ConvertToRemoteVariant(aArgs[index], arg, actor->GetInstance())) {
      NS_WARNING("Failed to convert argument!");
      return false;
    }
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

  ConvertToVariant(remoteResult, *aResult);
  return true;
}

// static
bool
PluginScriptableObjectChild::ScriptableHasProperty(NPObject* aObject,
                                                   NPIdentifier aName)
{
  AssertPluginThread();

  if (aObject->_class != PluginScriptableObjectChild::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  PluginScriptableObjectChild* actor = object->parent;
  NS_ASSERTION(actor, "This shouldn't ever be null!");

  bool result;
  if (!actor->CallHasProperty((NPRemoteIdentifier)aName, &result)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  return result;
}

// static
bool
PluginScriptableObjectChild::ScriptableGetProperty(NPObject* aObject,
                                                   NPIdentifier aName,
                                                   NPVariant* aResult)
{
  AssertPluginThread();

  if (aObject->_class != PluginScriptableObjectChild::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  PluginScriptableObjectChild* actor = object->parent;
  NS_ASSERTION(actor, "This shouldn't ever be null!");

  Variant result;
  bool success;
  if (!actor->CallGetProperty((NPRemoteIdentifier)aName, &result, &success)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

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

  if (aObject->_class != PluginScriptableObjectChild::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  PluginScriptableObjectChild* actor = object->parent;
  NS_ASSERTION(actor, "This shouldn't ever be null!");

  Variant value;
  if (!ConvertToRemoteVariant(*aValue, value, actor->GetInstance())) {
    NS_WARNING("Failed to convert variant!");
    return false;
  }

  bool success;
  if (!actor->CallSetProperty((NPRemoteIdentifier)aName, value, &success)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  return success;
}

// static
bool
PluginScriptableObjectChild::ScriptableRemoveProperty(NPObject* aObject,
                                                      NPIdentifier aName)
{
  AssertPluginThread();

  if (aObject->_class != PluginScriptableObjectChild::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  PluginScriptableObjectChild* actor = object->parent;
  NS_ASSERTION(actor, "This shouldn't ever be null!");

  bool success;
  if (!actor->CallRemoveProperty((NPRemoteIdentifier)aName, &success)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  return success;
}

// static
bool
PluginScriptableObjectChild::ScriptableEnumerate(NPObject* aObject,
                                                 NPIdentifier** aIdentifiers,
                                                 uint32_t* aCount)
{
  AssertPluginThread();

  if (aObject->_class != PluginScriptableObjectChild::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  PluginScriptableObjectChild* actor = object->parent;
  NS_ASSERTION(actor, "This shouldn't ever be null!");

  nsAutoTArray<NPRemoteIdentifier, 10> identifiers;
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

  *aIdentifiers = reinterpret_cast<NPIdentifier*>(
      PluginModuleChild::sBrowserFuncs.memalloc(*aCount * sizeof(NPIdentifier)));
  if (!*aIdentifiers) {
    NS_ERROR("Out of memory!");
    return false;
  }

  for (PRUint32 index = 0; index < *aCount; index++) {
    (*aIdentifiers)[index] = (NPIdentifier)identifiers[index];
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

  if (aObject->_class != PluginScriptableObjectChild::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ChildNPObject* object = reinterpret_cast<ChildNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  PluginScriptableObjectChild* actor = object->parent;
  NS_ASSERTION(actor, "This shouldn't ever be null!");

  nsAutoTArray<Variant, 10> args;
  if (!args.SetLength(aArgCount)) {
    NS_ERROR("Out of memory?!");
    return false;
  }

  for (PRUint32 index = 0; index < aArgCount; index++) {
    Variant& arg = args[index];
    if (!ConvertToRemoteVariant(aArgs[index], arg, actor->GetInstance())) {
      NS_WARNING("Failed to convert argument!");
      return false;
    }
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

PluginScriptableObjectChild::PluginScriptableObjectChild()
: mInstance(nsnull),
  mObject(nsnull)
{
  AssertPluginThread();
}

PluginScriptableObjectChild::~PluginScriptableObjectChild()
{
  AssertPluginThread();

  if (mObject) {
    if (mObject->_class == GetClass()) {
      if (!static_cast<ChildNPObject*>(mObject)->invalidated) {
        NS_WARNING("This should have happened already!");
        ScriptableInvalidate(mObject);
      }
    }
    else {
      PluginModuleChild::sBrowserFuncs.releaseobject(mObject);
    }
    NS_ASSERTION(!PluginModuleChild::current()->NPObjectIsRegistered(mObject),
                 "NPObject still registered!");
  }
}

void
PluginScriptableObjectChild::Initialize(PluginInstanceChild* aInstance,
                                        NPObject* aObject)
{
  AssertPluginThread();

  NS_ASSERTION(!(mInstance && mObject), "Calling Initialize class twice!");

  if (aObject->_class == GetClass()) {
    ChildNPObject* object = static_cast<ChildNPObject*>(aObject);

    NS_ASSERTION(!object->parent, "Bad object!");
    object->parent = const_cast<PluginScriptableObjectChild*>(this);

    // We don't want to have the actor own this object but rather let the object
    // own this actor. Set the reference count to 0 here so that when the object
    // dies we will send the destructor message to the parent.
    NS_ASSERTION(aObject->referenceCount == 1, "Some kind of live object!");
    aObject->referenceCount = 0;
  }
  else {
    // Plugin-provided object, retain here. This should be the only reference we
    // ever need.
    PluginModuleChild::sBrowserFuncs.retainobject(aObject);
  }

  mInstance = aInstance;
  mObject = aObject;
}

bool
PluginScriptableObjectChild::AnswerInvalidate()
{
  AssertPluginThread();

  if (mObject) {
    NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
    if (mObject->_class && mObject->_class->invalidate) {
      mObject->_class->invalidate(mObject);
    }
    PluginModuleChild::current()->UnregisterNPObject(mObject);
    PluginModuleChild::sBrowserFuncs.releaseobject(mObject);
    mObject = nsnull;
  }
  return true;
}

bool
PluginScriptableObjectChild::AnswerHasMethod(const NPRemoteIdentifier& aId,
                                             bool* aHasMethod)
{
  AssertPluginThread();

  if (!mObject) {
    NS_WARNING("Calling AnswerHasMethod with an invalidated object!");
    *aHasMethod = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");

  if (!(mObject->_class && mObject->_class->hasMethod)) {
    *aHasMethod = false;
    return true;
  }

  *aHasMethod = mObject->_class->hasMethod(mObject, (NPIdentifier)aId);
  return true;
}

bool
PluginScriptableObjectChild::AnswerInvoke(const NPRemoteIdentifier& aId,
                                          const nsTArray<Variant>& aArgs,
                                          Variant* aResult,
                                          bool* aSuccess)
{
  AssertPluginThread();

  if (!mObject) {
    NS_WARNING("Calling AnswerInvoke with an invalidated object!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");

  if (!(mObject->_class && mObject->_class->invoke)) {
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
    ConvertToVariant(aArgs[index], convertedArgs[index]);
  }

  NPVariant result;
  bool success = mObject->_class->invoke(mObject, (NPIdentifier)aId,
                                         convertedArgs.Elements(), argCount,
                                         &result);

  for (PRUint32 index = 0; index < argCount; index++) {
    PluginModuleChild::sBrowserFuncs.releasevariantvalue(&convertedArgs[index]);
  }

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  Variant convertedResult;
  success = ConvertToRemoteVariant(result, convertedResult, GetInstance());

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
PluginScriptableObjectChild::AnswerInvokeDefault(const nsTArray<Variant>& aArgs,
                                                 Variant* aResult,
                                                 bool* aSuccess)
{
  AssertPluginThread();

  if (!mObject) {
    NS_WARNING("Calling AnswerInvokeDefault with an invalidated object!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");

  if (!(mObject->_class && mObject->_class->invokeDefault)) {
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
    ConvertToVariant(aArgs[index], convertedArgs[index]);
  }

  NPVariant result;
  bool success = mObject->_class->invokeDefault(mObject,
                                                convertedArgs.Elements(),
                                                argCount, &result);

  for (PRUint32 index = 0; index < argCount; index++) {
    PluginModuleChild::sBrowserFuncs.releasevariantvalue(&convertedArgs[index]);
  }

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  Variant convertedResult;
  success = ConvertToRemoteVariant(result, convertedResult, GetInstance());

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
PluginScriptableObjectChild::AnswerHasProperty(const NPRemoteIdentifier& aId,
                                               bool* aHasProperty)
{
  AssertPluginThread();

  if (!mObject) {
    NS_WARNING("Calling AnswerHasProperty with an invalidated object!");
    *aHasProperty = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");

  if (!(mObject->_class && mObject->_class->hasProperty)) {
    *aHasProperty = false;
    return true;
  }

  *aHasProperty = mObject->_class->hasProperty(mObject, (NPIdentifier)aId);
  return true;
}

bool
PluginScriptableObjectChild::AnswerGetProperty(const NPRemoteIdentifier& aId,
                                               Variant* aResult,
                                               bool* aSuccess)
{
  AssertPluginThread();

  if (!mObject) {
    NS_WARNING("Calling AnswerGetProperty with an invalidated object!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");

  if (!(mObject->_class && mObject->_class->getProperty)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  NPVariant result;
  if (!mObject->_class->getProperty(mObject, (NPIdentifier)aId, &result)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  Variant converted;
  if ((*aSuccess = ConvertToRemoteVariant(result, converted, GetInstance()))) {
    DeferNPVariantLastRelease(&PluginModuleChild::sBrowserFuncs, &result);
    *aResult = converted;
  }
  else {
    *aResult = void_t();
  }

  return true;
}

bool
PluginScriptableObjectChild::AnswerSetProperty(const NPRemoteIdentifier& aId,
                                               const Variant& aValue,
                                               bool* aSuccess)
{
  AssertPluginThread();

  if (!mObject) {
    NS_WARNING("Calling AnswerSetProperty with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");

  if (!(mObject->_class && mObject->_class->setProperty)) {
    *aSuccess = false;
    return true;
  }

  NPVariant converted;
  ConvertToVariant(aValue, converted);

  if ((*aSuccess = mObject->_class->setProperty(mObject, (NPIdentifier)aId,
                                                &converted))) {
    PluginModuleChild::sBrowserFuncs.releasevariantvalue(&converted);
  }
  return true;
}

bool
PluginScriptableObjectChild::AnswerRemoveProperty(const NPRemoteIdentifier& aId,
                                                  bool* aSuccess)
{
  AssertPluginThread();

  if (!mObject) {
    NS_WARNING("Calling AnswerRemoveProperty with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");

  if (!(mObject->_class && mObject->_class->removeProperty)) {
    *aSuccess = false;
    return true;
  }

  *aSuccess = mObject->_class->removeProperty(mObject, (NPIdentifier)aId);
  return true;
}

bool
PluginScriptableObjectChild::AnswerEnumerate(nsTArray<NPRemoteIdentifier>* aProperties,
                                             bool* aSuccess)
{
  AssertPluginThread();

  if (!mObject) {
    NS_WARNING("Calling AnswerEnumerate with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");

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

  if (!aProperties->SetCapacity(idCount)) {
    PluginModuleChild::sBrowserFuncs.memfree(ids);
    *aSuccess = false;
    return true;
  }

  for (uint32_t index = 0; index < idCount; index++) {
#ifdef DEBUG
    NPRemoteIdentifier* remoteId =
#endif
    aProperties->AppendElement((NPRemoteIdentifier)ids[index]);
    NS_ASSERTION(remoteId, "Shouldn't fail if SetCapacity above succeeded!");
  }

  PluginModuleChild::sBrowserFuncs.memfree(ids);
  *aSuccess = true;
  return true;
}

bool
PluginScriptableObjectChild::AnswerConstruct(const nsTArray<Variant>& aArgs,
                                             Variant* aResult,
                                             bool* aSuccess)
{
  AssertPluginThread();

  if (!mObject) {
    NS_WARNING("Calling AnswerConstruct with an invalidated object!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");

  if (!(mObject->_class && mObject->_class->construct)) {
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
    ConvertToVariant(aArgs[index], convertedArgs[index]);
  }

  NPVariant result;
  bool success = mObject->_class->construct(mObject, convertedArgs.Elements(),
                                            argCount, &result);

  for (PRUint32 index = 0; index < argCount; index++) {
    PluginModuleChild::sBrowserFuncs.releasevariantvalue(&convertedArgs[index]);
  }

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  Variant convertedResult;
  success = ConvertToRemoteVariant(result, convertedResult, GetInstance());

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
PluginScriptableObjectChild::Evaluate(NPString* aScript,
                                      NPVariant* aResult)
{
  nsDependentCString script("");
  if (aScript->UTF8Characters && aScript->UTF8Length) {
    script.Rebind(aScript->UTF8Characters, aScript->UTF8Length);
  }

  bool success;
  Variant result;
  if (!(CallNPN_Evaluate(script, &result, &success) && success)) {
    return false;
  }

  ConvertToVariant(result, *aResult);
  return true;
}
