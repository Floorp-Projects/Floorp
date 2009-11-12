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

#include "PluginScriptableObjectParent.h"
#include "PluginInstanceParent.h"
#include "PluginModuleParent.h"

#include "npapi.h"
#include "nsDebug.h"

using namespace mozilla::plugins;

using mozilla::ipc::NPRemoteIdentifier;

namespace {

inline PluginInstanceParent*
GetInstance(NPObject* aObject)
{
  NS_ASSERTION(aObject->_class == PluginScriptableObjectParent::GetClass(),
               "Bad class!");

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return nsnull;
  }
  return object->parent->GetInstance();
}

inline const NPNetscapeFuncs*
GetNetscapeFuncs(PluginInstanceParent* aInstance)
{
  PluginModuleParent* module = aInstance->Module();
  if (!module) {
    NS_WARNING("Null module?!");
    return nsnull;
  }
  return module->GetNetscapeFuncs();
}

inline const NPNetscapeFuncs*
GetNetscapeFuncs(NPObject* aObject)
{
  NS_ASSERTION(aObject->_class == PluginScriptableObjectParent::GetClass(),
               "Bad class!");

  PluginInstanceParent* instance = GetInstance(aObject);
  if (!instance) {
    return nsnull;
  }

  return GetNetscapeFuncs(instance);
}

inline NPObject*
NPObjectFromVariant(const Variant& aRemoteVariant) {
  NS_ASSERTION(aRemoteVariant.type() ==
               Variant::TPPluginScriptableObjectParent,
               "Wrong variant type!");
  PluginScriptableObjectParent* actor =
    const_cast<PluginScriptableObjectParent*>(
      reinterpret_cast<const PluginScriptableObjectParent*>(
        aRemoteVariant.get_PPluginScriptableObjectParent()));
  return actor->GetObject();
}

inline NPObject*
NPObjectFromVariant(const NPVariant& aVariant) {
  NS_ASSERTION(NPVARIANT_IS_OBJECT(aVariant), "Wrong variant type!");
  return NPVARIANT_TO_OBJECT(aVariant);
}

inline void
ReleaseVariant(NPVariant& aVariant,
               PluginInstanceParent* aInstance)
{
  const NPNetscapeFuncs* npn = GetNetscapeFuncs(aInstance);
  if (npn) {
    npn->releasevariantvalue(&aVariant);
  }
}

inline bool
EnsureValidIdentifier(PluginInstanceParent* aInstance,
                      NPIdentifier aIdentifier)
{
  PluginModuleParent* module = aInstance->Module();
  if (!module) {
    NS_WARNING("Huh?!");
    return false;
  }

  return module->EnsureValidNPIdentifier(aIdentifier);
}

inline bool
EnsureValidIdentifier(NPObject* aObject,
                      NPIdentifier aIdentifier)
{
  PluginInstanceParent* instance = GetInstance(aObject);
  if (!instance) {
    NS_WARNING("Huh?!");
    return false;
  }

  return EnsureValidIdentifier(instance, aIdentifier);
}

bool
ConvertToVariant(const Variant& aRemoteVariant,
                 NPVariant& aVariant,
                 PluginInstanceParent* aInstance)
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
      if (!buffer) {
        NS_ERROR("Out of memory!");
        return false;
      }
      STRINGN_TO_NPVARIANT(buffer, string.Length(), aVariant);
      break;
    }

    case Variant::TPPluginScriptableObjectParent: {
      NPObject* object = NPObjectFromVariant(aRemoteVariant);
      if (!object) {
        NS_ERROR("Er, this shouldn't fail!");
        return false;
      }

      const NPNetscapeFuncs* npn = GetNetscapeFuncs(aInstance);
      if (!npn) {
        NS_ERROR("Null netscape funcs!");
        return false;
      }
      npn->retainobject(object);
      OBJECT_TO_NPVARIANT(object, aVariant);
      break;
    }

    default:
      NS_NOTREACHED("Shouldn't get here!");
      return false;
  }

  return true;
}

bool
ConvertToRemoteVariant(const NPVariant& aVariant,
                       Variant& aRemoteVariant,
                       PluginInstanceParent* aInstance)
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
    NPObject* object = NPVARIANT_TO_OBJECT(aVariant);
    PluginScriptableObjectParent* actor = aInstance->GetActorForNPObject(object);
    if (!actor) {
      NS_ERROR("Null actor!");
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
PluginScriptableObjectParent::ScriptableAllocate(NPP aInstance,
                                                 NPClass* aClass)
{
  NS_ASSERTION(aClass == PluginScriptableObjectParent::GetClass(),
               "Huh?! Wrong class!");

  PluginInstanceParent* instance = PluginModuleParent::InstCast(aInstance);
  NS_ASSERTION(instance, "This should never be null!");

  const NPNetscapeFuncs* npn = GetNetscapeFuncs(instance);
  if (!npn) {
    NS_WARNING("Can't allocate!");
    return nsnull;
  }

  ParentNPObject* object =
    reinterpret_cast<ParentNPObject*>(npn->memalloc(sizeof(ParentNPObject)));
  if (object) {
    memset(object, 0, sizeof(ParentNPObject));
  }
  return object;
}

// static
void
PluginScriptableObjectParent::ScriptableInvalidate(NPObject* aObject)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    // This can happen more than once, and is just fine.
    return;
  }

  PluginScriptableObjectParent* actor = object->parent;
  NS_ASSERTION(actor, "Null actor?!");

  PluginInstanceParent* instance = actor->GetInstance();
  NS_WARN_IF_FALSE(instance, "No instance?!");

  if (!actor->CallInvalidate()) {
    NS_WARNING("Failed to send message!");
  }

  object->invalidated = true;

  if (instance &&
      !instance->CallPPluginScriptableObjectDestructor(actor)) {
    NS_WARNING("Failed to send message!");
  }
}

// static
void
PluginScriptableObjectParent::ScriptableDeallocate(NPObject* aObject)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (!object->invalidated) {
    ScriptableInvalidate(aObject);
  }

  NS_ASSERTION(object->invalidated, "Should be invalidated!");

  NS_Free(aObject);
}

// static
bool
PluginScriptableObjectParent::ScriptableHasMethod(NPObject* aObject,
                                                  NPIdentifier aName)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  if (!EnsureValidIdentifier(aObject, aName)) {
    return false;
  }

  PluginScriptableObjectParent* actor = object->parent;
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
PluginScriptableObjectParent::ScriptableInvoke(NPObject* aObject,
                                               NPIdentifier aName,
                                               const NPVariant* aArgs,
                                               uint32_t aArgCount,
                                               NPVariant* aResult)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  if (!EnsureValidIdentifier(aObject, aName)) {
    return false;
  }

  PluginScriptableObjectParent* actor = object->parent;
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
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  PluginScriptableObjectParent* actor = object->parent;
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
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  if (!EnsureValidIdentifier(aObject, aName)) {
    return false;
  }

  PluginScriptableObjectParent* actor = object->parent;
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
PluginScriptableObjectParent::ScriptableGetProperty(NPObject* aObject,
                                                    NPIdentifier aName,
                                                    NPVariant* aResult)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  if (!EnsureValidIdentifier(aObject, aName)) {
    return false;
  }

  PluginScriptableObjectParent* actor = object->parent;
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

  if (!ConvertToVariant(result, *aResult, actor->GetInstance())) {
    NS_WARNING("Failed to convert result!");
    return false;
  }

  return true;
}

// static
bool
PluginScriptableObjectParent::ScriptableSetProperty(NPObject* aObject,
                                                    NPIdentifier aName,
                                                    const NPVariant* aValue)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  if (!EnsureValidIdentifier(aObject, aName)) {
    return false;
  }

  PluginScriptableObjectParent* actor = object->parent;
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
PluginScriptableObjectParent::ScriptableRemoveProperty(NPObject* aObject,
                                                       NPIdentifier aName)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  if (!EnsureValidIdentifier(aObject, aName)) {
    return false;
  }

  PluginScriptableObjectParent* actor = object->parent;
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
PluginScriptableObjectParent::ScriptableEnumerate(NPObject* aObject,
                                                  NPIdentifier** aIdentifiers,
                                                  uint32_t* aCount)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  PluginScriptableObjectParent* actor = object->parent;
  NS_ASSERTION(actor, "This shouldn't ever be null!");

  const NPNetscapeFuncs* npn = GetNetscapeFuncs(aObject);
  if (!npn) {
    NS_ERROR("No netscape funcs!");
    return false;
  }

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

  *aIdentifiers = (NPIdentifier*)npn->memalloc(*aCount * sizeof(NPIdentifier));
  if (!*aIdentifiers) {
    NS_ERROR("Out of memory!");
    return false;
  }

  for (PRUint32 index = 0; index < *aCount; index++) {
    NPIdentifier& id = *aIdentifiers[index];
    id = (NPIdentifier)identifiers[index];
    if (!EnsureValidIdentifier(aObject, id)) {
      return false;
    }
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
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  PluginScriptableObjectParent* actor = object->parent;
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

PluginScriptableObjectParent::PluginScriptableObjectParent()
: mInstance(nsnull),
  mObject(nsnull)
{
}

PluginScriptableObjectParent::~PluginScriptableObjectParent()
{
  if (mObject) {
    if (mObject->_class == GetClass()) {
      if (!static_cast<ParentNPObject*>(mObject)->invalidated) {
        ScriptableInvalidate(mObject);
      }
    }
    else {
      mInstance->GetNPNIface()->releaseobject(mObject);
    }
  }
}

void
PluginScriptableObjectParent::Initialize(PluginInstanceParent* aInstance,
                                         NPObject* aObject)
{
  NS_ASSERTION(aInstance && aObject, "Null pointers!");
  NS_ASSERTION(!(mInstance && mObject), "Calling Initialize more than once!");

  if (aObject->_class == GetClass()) {
    ParentNPObject* object = static_cast<ParentNPObject*>(aObject);

    NS_ASSERTION(!object->parent, "Bad object!");
    object->parent = const_cast<PluginScriptableObjectParent*>(this);

    // We don't want to have the actor own this object but rather let the object
    // own this actor. Set the reference count to 0 here so that when the object
    // dies we will send the destructor message to the child.
    NS_ASSERTION(aObject->referenceCount == 1, "Some kind of live object!");
    object->referenceCount = 0;
    NS_LOG_RELEASE(aObject, 0, "BrowserNPObject");
  }
  else {
    aInstance->GetNPNIface()->retainobject(aObject);
  }

  mInstance = aInstance;
  mObject = aObject;
}

bool
PluginScriptableObjectParent::AnswerInvalidate()
{
  if (mObject) {
    NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");
    const NPNetscapeFuncs* npn = GetNetscapeFuncs(GetInstance());
    if (npn) {
      npn->releaseobject(mObject);
    }
    mObject = nsnull;
  }
  return true;
}

bool
PluginScriptableObjectParent::AnswerHasMethod(const NPRemoteIdentifier& aId,
                                              bool* aHasMethod)
{
  if (!mObject) {
    NS_WARNING("Calling AnswerHasMethod with an invalidated object!");
    *aHasMethod = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");

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

  if (!EnsureValidIdentifier(instance, (NPIdentifier)aId)) {
    NS_WARNING("Invalid NPIdentifier!");
    *aHasMethod = false;
    return true;
  }

  *aHasMethod = npn->hasmethod(instance->GetNPP(), mObject, (NPIdentifier)aId);
  return true;
}

bool
PluginScriptableObjectParent::AnswerInvoke(const NPRemoteIdentifier& aId,
                                           const nsTArray<Variant>& aArgs,
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

  if (!EnsureValidIdentifier(instance, (NPIdentifier)aId)) {
    NS_WARNING("Invalid NPIdentifier!");
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
  bool success = npn->invoke(instance->GetNPP(), mObject, (NPIdentifier)aId,
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

  ReleaseVariant(result, instance);

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
PluginScriptableObjectParent::AnswerInvokeDefault(const nsTArray<Variant>& aArgs,
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

  ReleaseVariant(result, instance);

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
PluginScriptableObjectParent::AnswerHasProperty(const NPRemoteIdentifier& aId,
                                                bool* aHasProperty)
{
  if (!mObject) {
    NS_WARNING("Calling AnswerHasProperty with an invalidated object!");
    *aHasProperty = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");

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

  if (!EnsureValidIdentifier(instance, (NPIdentifier)aId)) {
    NS_WARNING("Invalid NPIdentifier!");
    *aHasProperty = false;
    return true;
  }

  *aHasProperty = npn->hasproperty(instance->GetNPP(), mObject,
                                   (NPIdentifier)aId);
  return true;
}

bool
PluginScriptableObjectParent::AnswerGetProperty(const NPRemoteIdentifier& aId,
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

  if (!EnsureValidIdentifier(instance, (NPIdentifier)aId)) {
    NS_WARNING("Invalid NPIdentifier!");
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  NPVariant result;
  if (!npn->getproperty(instance->GetNPP(), mObject, (NPIdentifier)aId,
                        &result)) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  Variant converted;
  if ((*aSuccess = ConvertToRemoteVariant(result, converted, instance))) {
    ReleaseVariant(result, instance);
    *aResult = converted;
  }
  else {
    *aResult = void_t();
  }

  return true;
}

bool
PluginScriptableObjectParent::AnswerSetProperty(const NPRemoteIdentifier& aId,
                                                const Variant& aValue,
                                                bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling AnswerSetProperty with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");

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

  if (!EnsureValidIdentifier(instance, (NPIdentifier)aId)) {
    NS_WARNING("Invalid NPIdentifier!");
    *aSuccess = false;
    return true;
  }

  NPVariant converted;
  if (!ConvertToVariant(aValue, converted, instance)) {
    *aSuccess = false;
    return true;
  }

  if ((*aSuccess = npn->setproperty(instance->GetNPP(), mObject,
                                    (NPIdentifier)aId, &converted))) {
    ReleaseVariant(converted, instance);
  }
  return true;
}

bool
PluginScriptableObjectParent::AnswerRemoveProperty(const NPRemoteIdentifier& aId,
                                                   bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling AnswerRemoveProperty with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");

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

  if (!EnsureValidIdentifier(instance, (NPIdentifier)aId)) {
    NS_WARNING("Invalid NPIdentifier!");
    *aSuccess = false;
    return true;
  }

  *aSuccess = npn->removeproperty(instance->GetNPP(), mObject,
                                  (NPIdentifier)aId);
  return true;
}

bool
PluginScriptableObjectParent::AnswerEnumerate(nsTArray<NPRemoteIdentifier>* aProperties,
                                              bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling AnswerEnumerate with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  NS_ASSERTION(mObject->_class != GetClass(), "Bad object type!");

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

  for (uint32_t index = 0; index < idCount; index++) {
    NS_ASSERTION(EnsureValidIdentifier(instance, ids[index]),
                 "Identifier not yet in hashset!");
#ifdef DEBUG
    NPRemoteIdentifier* remoteId =
#endif
    aProperties->AppendElement((NPRemoteIdentifier)ids[index]);
    NS_ASSERTION(remoteId, "Shouldn't fail if SetCapacity above succeeded!");
  }

  npn->memfree(ids);
  *aSuccess = true;
  return true;
}

bool
PluginScriptableObjectParent::AnswerConstruct(const nsTArray<Variant>& aArgs,
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

  ReleaseVariant(result, instance);

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

  ReleaseVariant(result, instance);

  if (!success) {
    *aResult = void_t();
    *aSuccess = false;
    return true;
  }

  *aSuccess = true;
  *aResult = convertedResult;
  return true;
}
