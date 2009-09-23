/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "npfunctions.h"
#include "nsDebug.h"

using namespace mozilla::plugins;

using mozilla::ipc::NPRemoteIdentifier;
using mozilla::ipc::NPRemoteVariant;

namespace {

inline PluginInstanceParent*
GetInstance(NPObject* aObject)
{
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
  PluginModuleParent* module = aInstance->GetModule();
  if (!module) {
    NS_WARNING("Null module?!");
    return nsnull;
  }
  return module->GetNetscapeFuncs();
}

inline const NPNetscapeFuncs*
GetNetscapeFuncs(NPObject* aObject)
{
  PluginInstanceParent* instance = GetInstance(aObject);
  if (!instance) {
    return nsnull;
  }
  return GetNetscapeFuncs(instance);
}

inline NPObject*
NPObjectFromVariant(const NPRemoteVariant& aRemoteVariant) {
  NS_ASSERTION(aRemoteVariant.type() ==
               NPRemoteVariant::TPPluginScriptableObjectParent,
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

inline bool
EnsureValidIdentifier(NPObject* aObject,
                      NPIdentifier aIdentifier)
{
  PluginInstanceParent* instance = GetInstance(aObject);
  if (!instance) {
    NS_WARNING("Huh?!");
    return false;
  }

  PluginModuleParent* module = instance->GetModule();
  if (!module) {
    NS_WARNING("Huh?!");
    return false;
  }

  return module->EnsureValidNPIdentifier(aIdentifier);
}

bool
ConvertToVariant(const NPRemoteVariant& aRemoteVariant,
                 NPVariant& aVariant)
{
  switch (aRemoteVariant.type()) {
    case NPRemoteVariant::Tvoid_t: {
      VOID_TO_NPVARIANT(aVariant);
      break;
    }

    case NPRemoteVariant::Tnull_t: {
      NULL_TO_NPVARIANT(aVariant);
      break;
    }

    case NPRemoteVariant::Tbool: {
      BOOLEAN_TO_NPVARIANT(aRemoteVariant.get_bool(), aVariant);
      break;
    }

    case NPRemoteVariant::Tint: {
      INT32_TO_NPVARIANT(aRemoteVariant.get_int(), aVariant);
      break;
    }

    case NPRemoteVariant::Tdouble: {
      DOUBLE_TO_NPVARIANT(aRemoteVariant.get_double(), aVariant);
      break;
    }

    case NPRemoteVariant::TnsCString: {
      const nsCString& string = aRemoteVariant.get_nsCString();
      NPUTF8* buffer = reinterpret_cast<NPUTF8*>(strdup(string.get()));
      if (!buffer) {
        NS_ERROR("Out of memory!");
        return false;
      }
      STRINGN_TO_NPVARIANT(buffer, string.Length(), aVariant);
      break;
    }

    case NPRemoteVariant::TPPluginScriptableObjectParent: {
      NPObject* object = NPObjectFromVariant(aRemoteVariant);
      if (!object) {
        NS_ERROR("Er, this shouldn't fail!");
        return false;
      }
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
                       NPRemoteVariant& aRemoteVariant)
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
    NS_ASSERTION(object->_class == PluginScriptableObjectParent::GetClass(),
                 "Don't know anything about this object!");

    PluginScriptableObjectParent* actor =
      static_cast<ParentNPObject*>(object)->parent;
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

NPObject*
ScriptableAllocate(NPP aInstance,
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

void
ScriptableInvalidate(NPObject* aObject)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_WARNING("Don't know what kind of object this is!");
    return;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling invalidate more than once!");
    return;
  }

  PluginInstanceParent* instance = GetInstance(aObject);
  if (instance) {
    if (!instance->CallPPluginScriptableObjectDestructor(object->parent)) {
      NS_WARNING("Failed to send message!");
    }
  }

  object->invalidated = true;
}

void
ScriptableDeallocate(NPObject* aObject)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_WARNING("Don't know what kind of object this is!");
    return;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (!object->invalidated) {
    NS_WARNING("Object wasn't previously invalidated!");
    ScriptableInvalidate(aObject);
  }

  NS_ASSERTION(object->invalidated, "Should be invalidated!");

  const NPNetscapeFuncs* npn = GetNetscapeFuncs(aObject);
  if (npn) {
    npn->memfree(aObject);
  }
}

bool
ScriptableHasMethod(NPObject* aObject,
                    NPIdentifier aName)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_WARNING("Don't know what kind of object this is!");
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

bool
ScriptableInvoke(NPObject* aObject,
                 NPIdentifier aName,
                 const NPVariant* aArgs,
                 uint32_t aArgCount,
                 NPVariant* aResult)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_WARNING("Don't know what kind of object this is!");
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

  nsAutoTArray<NPRemoteVariant, 10> args;
  if (!args.SetLength(aArgCount)) {
    NS_ERROR("Out of memory?!");
    return false;
  }

  for (PRUint32 index = 0; index < aArgCount; index++) {
    NPRemoteVariant& arg = args[index];
    if (!ConvertToRemoteVariant(aArgs[index], arg)) {
      NS_WARNING("Failed to convert argument!");
      return false;
    }
  }

  NPRemoteVariant remoteResult;
  bool success;
  if (!actor->CallInvoke((NPRemoteIdentifier)aName, args, &remoteResult,
       &success)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  if (!success) {
    return false;
  }

  NPVariant result;
  if (!ConvertToVariant(remoteResult, result)) {
    NS_WARNING("Failed to convert result!");
    return false;
  }

  *aResult = result;
  return true;
}

bool
ScriptableInvokeDefault(NPObject* aObject,
                        const NPVariant* aArgs,
                        uint32_t aArgCount,
                        NPVariant* aResult)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_WARNING("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  PluginScriptableObjectParent* actor = object->parent;
  NS_ASSERTION(actor, "This shouldn't ever be null!");

  nsAutoTArray<NPRemoteVariant, 10> args;
  if (!args.SetLength(aArgCount)) {
    NS_ERROR("Out of memory?!");
    return false;
  }

  for (PRUint32 index = 0; index < aArgCount; index++) {
    NPRemoteVariant& arg = args[index];
    if (!ConvertToRemoteVariant(aArgs[index], arg)) {
      NS_WARNING("Failed to convert argument!");
      return false;
    }
  }

  NPRemoteVariant remoteResult;
  bool success;
  if (!actor->CallInvokeDefault(args, &remoteResult, &success)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  if (!success) {
    return false;
  }

  NPVariant result;
  if (!ConvertToVariant(remoteResult, result)) {
    NS_WARNING("Failed to convert result!");
    return false;
  }

  *aResult = result;
  return true;
}

bool
ScriptableHasProperty(NPObject* aObject,
                      NPIdentifier aName)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_WARNING("Don't know what kind of object this is!");
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

bool
ScriptableGetProperty(NPObject* aObject,
                      NPIdentifier aName,
                      NPVariant* aResult)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_WARNING("Don't know what kind of object this is!");
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

  NPRemoteVariant result;
  bool success;
  if (!actor->CallGetProperty((NPRemoteIdentifier)aName, &result, &success)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  if (!success) {
    return false;
  }

  if (!ConvertToVariant(result, *aResult)) {
    NS_WARNING("Failed to convert result!");
    return false;
  }

  return true;
}

bool
ScriptableSetProperty(NPObject* aObject,
                      NPIdentifier aName,
                      const NPVariant* aValue)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_WARNING("Don't know what kind of object this is!");
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

  NPRemoteVariant value;
  if (!ConvertToRemoteVariant(*aValue, value)) {
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

bool
ScriptableRemoveProperty(NPObject* aObject,
                         NPIdentifier aName)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_WARNING("Don't know what kind of object this is!");
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

bool
ScriptableEnumerate(NPObject* aObject,
                    NPIdentifier** aIdentifiers,
                    uint32_t* aCount)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_WARNING("Don't know what kind of object this is!");
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

bool
ScriptableConstruct(NPObject* aObject,
                    const NPVariant* aArgs,
                    uint32_t aArgCount,
                    NPVariant* aResult)
{
  if (aObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_WARNING("Don't know what kind of object this is!");
    return false;
  }

  ParentNPObject* object = reinterpret_cast<ParentNPObject*>(aObject);
  if (object->invalidated) {
    NS_WARNING("Calling method on an invalidated object!");
    return false;
  }

  PluginScriptableObjectParent* actor = object->parent;
  NS_ASSERTION(actor, "This shouldn't ever be null!");

  nsAutoTArray<NPRemoteVariant, 10> args;
  if (!args.SetLength(aArgCount)) {
    NS_ERROR("Out of memory?!");
    return false;
  }

  for (PRUint32 index = 0; index < aArgCount; index++) {
    NPRemoteVariant& arg = args[index];
    if (!ConvertToRemoteVariant(aArgs[index], arg)) {
      NS_WARNING("Failed to convert argument!");
      return false;
    }
  }

  NPRemoteVariant remoteResult;
  bool success;
  if (!actor->CallConstruct(args, &remoteResult, &success)) {
    NS_WARNING("Failed to send message!");
    return false;
  }

  if (!success) {
    return false;
  }

  NPVariant result;
  if (!ConvertToVariant(remoteResult, result)) {
    NS_WARNING("Failed to convert result!");
    return false;
  }

  *aResult = result;
  return true;
}

} // anonymous namespace

NPClass PluginScriptableObjectParent::sNPClass = {
  NP_CLASS_STRUCT_VERSION,
  ScriptableAllocate,
  ScriptableDeallocate,
  ScriptableInvalidate,
  ScriptableHasMethod,
  ScriptableInvoke,
  ScriptableInvokeDefault,
  ScriptableHasProperty,
  ScriptableGetProperty,
  ScriptableSetProperty,
  ScriptableRemoveProperty,
  ScriptableEnumerate,
  ScriptableConstruct
};

PluginScriptableObjectParent::PluginScriptableObjectParent()
: mInstance(nsnull),
  mObject(nsnull)
{
}

PluginScriptableObjectParent::~PluginScriptableObjectParent()
{
  if (mObject && !mObject->invalidated) {
    ScriptableInvalidate(mObject);
  }
}

void
PluginScriptableObjectParent::Initialize(PluginInstanceParent* aInstance,
                                         ParentNPObject* aObject)
{
  NS_ASSERTION(aInstance && aObject, "Null pointers!");
  NS_ASSERTION(!(mInstance && mObject), "Calling Initialize more than once!");
  NS_ASSERTION(aObject->_class == &sNPClass && !aObject->parent, "Bad object!");

  mInstance = aInstance;

  mObject = aObject;
  mObject->parent = const_cast<PluginScriptableObjectParent*>(this);
  // XXX UGLY HACK, we let NPP_GetValue mess with the refcount, start at 0 here
  mObject->referenceCount = 0;
}
