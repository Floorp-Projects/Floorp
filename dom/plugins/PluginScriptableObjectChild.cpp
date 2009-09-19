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

#include "PluginScriptableObjectChild.h"

#include "npapi.h"
#include "npruntime.h"
#include "nsDebug.h"

#include "PluginModuleChild.h"
#include "PluginInstanceChild.h"

using namespace mozilla::plugins;
using mozilla::ipc::NPRemoteVariant;

namespace {

inline NPObject*
NPObjectFromVariant(const NPRemoteVariant& aRemoteVariant) {
  NS_ASSERTION(aRemoteVariant.type() ==
               NPRemoteVariant::TPPluginScriptableObjectChild,
               "Wrong variant type!");
  PluginScriptableObjectChild* actor =
    const_cast<PluginScriptableObjectChild*>(
      reinterpret_cast<const PluginScriptableObjectChild*>(
        aRemoteVariant.get_PPluginScriptableObjectChild()));
  return actor->GetObject();
}

inline NPObject*
NPObjectFromVariant(const NPVariant& aVariant) {
  NS_ASSERTION(NPVARIANT_IS_OBJECT(aVariant), "Wrong variant type!");
  return NPVARIANT_TO_OBJECT(aVariant);
}

inline PluginScriptableObjectChild*
CreateActorForNPObject(PluginInstanceChild* aInstance,
                       NPObject* aObject)
{
  return aInstance->CreateActorForNPObject(aObject);
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

    case NPRemoteVariant::TPPluginScriptableObjectChild: {
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
                       NPRemoteVariant& aRemoteVariant,
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
    PluginScriptableObjectChild* actor =
      CreateActorForNPObject(aInstance, NPVARIANT_TO_OBJECT(aVariant));
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

PluginScriptableObjectChild::PluginScriptableObjectChild()
: mInstance(nsnull),
  mObject(nsnull)
{
}

PluginScriptableObjectChild::~PluginScriptableObjectChild()
{
  if (mObject) {
    PluginModuleChild::sBrowserFuncs.releaseobject(mObject);
  }
}

void
PluginScriptableObjectChild::Initialize(PluginInstanceChild* aInstance,
                                        NPObject* aObject)
{
  NS_ASSERTION(!(mInstance && mObject), "Calling Initialize class twice!");
  mInstance = aInstance;
  mObject = PluginModuleChild::sBrowserFuncs.retainobject(aObject);
}

bool
PluginScriptableObjectChild::AnswerInvalidate()
{
  if (mObject) {
    PluginModuleChild::sBrowserFuncs.releaseobject(mObject);
    mObject = nsnull;
  }
  return true;
}

bool
PluginScriptableObjectChild::AnswerHasMethod(const NPRemoteIdentifier& aId,
                                             bool* aHasMethod)
{
  if (!mObject) {
    NS_WARNING("Calling " __FUNCTION__ "with an invalidated object!");
    *aHasMethod = false;
    return true;
  }

  if (!(mObject->_class && mObject->_class->hasMethod)) {
    *aHasMethod = false;
    return true;
  }

  *aHasMethod = mObject->_class->hasMethod(mObject, (NPIdentifier)aId);
  return true;
}

bool
PluginScriptableObjectChild::AnswerInvoke(const NPRemoteIdentifier& aId,
                                          const nsTArray<NPRemoteVariant>& aArgs,
                                          NPRemoteVariant* aResult,
                                          bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling " __FUNCTION__ "with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  if (!(mObject->_class && mObject->_class->invoke)) {
    *aSuccess = false;
    return true;
  }

  nsAutoTArray<NPVariant, 10> convertedArgs;
  PRUint32 argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount)) {
    *aSuccess = false;
    return true;
  }

  for (PRUint32 index = 0; index < argCount; index++) {
    if (!ConvertToVariant(aArgs[index], convertedArgs[index])) {
      *aSuccess = false;
      return true;
    }
  }

  NPVariant result;
  bool success = mObject->_class->invoke(mObject, (NPIdentifier)aId,
                                         convertedArgs.Elements(), argCount,
                                         &result);
  if (!success) {
    *aSuccess = false;
    return true;
  }

  NPRemoteVariant convertedResult;
  if (!ConvertToRemoteVariant(result, convertedResult, mInstance)) {
    *aSuccess = false;
    return true;
  }

  *aSuccess = true;
  *aResult = convertedResult;
  return true;
}

bool
PluginScriptableObjectChild::AnswerInvokeDefault(const nsTArray<NPRemoteVariant>& aArgs,
                                                 NPRemoteVariant* aResult,
                                                 bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling " __FUNCTION__ "with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  if (!(mObject->_class && mObject->_class->invokeDefault)) {
    *aSuccess = false;
    return true;
  }

  nsAutoTArray<NPVariant, 10> convertedArgs;
  PRUint32 argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount)) {
    *aSuccess = false;
    return true;
  }

  for (PRUint32 index = 0; index < argCount; index++) {
    if (!ConvertToVariant(aArgs[index], convertedArgs[index])) {
      *aSuccess = false;
      return true;
    }
  }

  NPVariant result;
  bool success = mObject->_class->invokeDefault(mObject,
                                                convertedArgs.Elements(),
                                                argCount, &result);
  if (!success) {
    *aSuccess = false;
    return true;
  }

  NPRemoteVariant convertedResult;
  if (!ConvertToRemoteVariant(result, convertedResult, mInstance)) {
    *aSuccess = false;
    return true;
  }

  *aSuccess = true;
  *aResult = convertedResult;
  return true;
}

bool
PluginScriptableObjectChild::AnswerHasProperty(const NPRemoteIdentifier& aId,
                                               bool* aHasProperty)
{
  if (!mObject) {
    NS_WARNING("Calling " __FUNCTION__ "with an invalidated object!");
    *aHasProperty = false;
    return true;
  }

  if (!(mObject->_class && mObject->_class->hasProperty)) {
    *aHasProperty = false;
    return true;
  }

  *aHasProperty = mObject->_class->hasProperty(mObject, (NPIdentifier)aId);
  return true;
}

bool
PluginScriptableObjectChild::AnswerGetProperty(const NPRemoteIdentifier& aId,
                                               NPRemoteVariant* aResult,
                                               bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling " __FUNCTION__ "with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  if (!(mObject->_class && mObject->_class->getProperty)) {
    *aSuccess = false;
    return true;
  }

  NPVariant result;
  if (!mObject->_class->getProperty(mObject, (NPIdentifier)aId, &result)) {
    *aSuccess = false;
    return true;
  }

  NPRemoteVariant converted;
  if ((*aSuccess = ConvertToRemoteVariant(result, converted, mInstance))) {
    *aResult = converted;
  }
  return true;
}

bool
PluginScriptableObjectChild::AnswerSetProperty(const NPRemoteIdentifier& aId,
                                               const NPRemoteVariant& aValue,
                                               bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling " __FUNCTION__ "with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  if (!(mObject->_class && mObject->_class->setProperty)) {
    *aSuccess = false;
    return true;
  }

  NPVariant converted;
  if (!ConvertToVariant(aValue, converted)) {
    *aSuccess = false;
    return true;
  }

  *aSuccess = mObject->_class->setProperty(mObject, (NPIdentifier)aId,
                                           &converted);
  return true;
}

bool
PluginScriptableObjectChild::AnswerRemoveProperty(const NPRemoteIdentifier& aId,
                                                  bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling " __FUNCTION__ "with an invalidated object!");
    *aSuccess = false;
    return true;
  }

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
  if (!mObject) {
    NS_WARNING("Calling " __FUNCTION__ "with an invalidated object!");
    *aSuccess = false;
    return true;
  }

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
PluginScriptableObjectChild::AnswerConstruct(const nsTArray<NPRemoteVariant>& aArgs,
                                             NPRemoteVariant* aResult,
                                             bool* aSuccess)
{
  if (!mObject) {
    NS_WARNING("Calling " __FUNCTION__ "with an invalidated object!");
    *aSuccess = false;
    return true;
  }

  if (!(mObject->_class && mObject->_class->construct)) {
    *aSuccess = false;
    return true;
  }

  nsAutoTArray<NPVariant, 10> convertedArgs;
  PRUint32 argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount)) {
    *aSuccess = false;
    return true;
  }

  for (PRUint32 index = 0; index < argCount; index++) {
    if (!ConvertToVariant(aArgs[index], convertedArgs[index])) {
      *aSuccess = false;
      return true;
    }
  }

  NPVariant result;
  bool success = mObject->_class->construct(mObject, convertedArgs.Elements(),
                                            argCount, &result);
  if (!success) {
    *aSuccess = false;
    return true;
  }

  NPRemoteVariant convertedResult;
  if (!ConvertToRemoteVariant(result, convertedResult, mInstance)) {
    *aSuccess = false;
    return true;
  }

  *aSuccess = true;
  *aResult = convertedResult;
  return true;
}
