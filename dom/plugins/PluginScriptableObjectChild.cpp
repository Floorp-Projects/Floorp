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
#include "nsThreadUtils.h"

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
      return true;
    }

    case NPRemoteVariant::Tnull_t: {
      NULL_TO_NPVARIANT(aVariant);
      return true;
    }

    case NPRemoteVariant::Tbool: {
      BOOLEAN_TO_NPVARIANT(aRemoteVariant.get_bool(), aVariant);
      return true;
    }

    case NPRemoteVariant::Tint: {
      INT32_TO_NPVARIANT(aRemoteVariant.get_int(), aVariant);
      return true;
    }

    case NPRemoteVariant::Tdouble: {
      DOUBLE_TO_NPVARIANT(aRemoteVariant.get_double(), aVariant);
      return true;
    }

    case NPRemoteVariant::TnsCString: {
      const nsCString& string = aRemoteVariant.get_nsCString();
      NPUTF8* buffer = reinterpret_cast<NPUTF8*>(strdup(string.get()));
      if (!buffer) {
        NS_ERROR("Out of memory!");
        return false;
      }
      STRINGN_TO_NPVARIANT(buffer, string.Length(), aVariant);
      return true;
    }

    case NPRemoteVariant::TPPluginScriptableObjectChild: {
      NPObject* object = NPObjectFromVariant(aRemoteVariant);
      if (!object) {
        return false;
      }
      OBJECT_TO_NPVARIANT(object, aVariant);
      return true;
    }
  }

  NS_NOTREACHED("Shouldn't get here!");
  return false;
}

bool
ConvertToRemoteVariant(const NPVariant& aVariant,
                       NPRemoteVariant& aRemoteVariant,
                       PluginInstanceChild* aInstance)
{
  if (NPVARIANT_IS_VOID(aVariant)) {
    aRemoteVariant = mozilla::void_t();
    return true;
  }

  if (NPVARIANT_IS_NULL(aVariant)) {
    aRemoteVariant = mozilla::null_t();
    return true;
  }

  if (NPVARIANT_IS_BOOLEAN(aVariant)) {
    aRemoteVariant = NPVARIANT_TO_BOOLEAN(aVariant);
    return true;
  }

  if (NPVARIANT_IS_INT32(aVariant)) {
    aRemoteVariant = NPVARIANT_TO_INT32(aVariant);
    return true;
  }

  if (NPVARIANT_IS_DOUBLE(aVariant)) {
    aRemoteVariant = NPVARIANT_TO_DOUBLE(aVariant);
    return true;
  }

  if (NPVARIANT_IS_STRING(aVariant)) {
    NPString str = NPVARIANT_TO_STRING(aVariant);
    nsCString string(str.UTF8Characters, str.UTF8Length);
    aRemoteVariant = string;
    return true;
  }

  if (NPVARIANT_IS_OBJECT(aVariant)) {
    NS_ASSERTION(aInstance, "Must have an instance to wrap!");
    PluginScriptableObjectChild* actor =
      CreateActorForNPObject(aInstance, NPVARIANT_TO_OBJECT(aVariant));
    if (!actor) {
      return false;
    }
    aRemoteVariant = actor;
    return true;
  }

  NS_NOTREACHED("Shouldn't get here!");
  return false;
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
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!(mInstance && mObject), "Calling Initialize class twice!");
  mInstance = aInstance;
  mObject = PluginModuleChild::sBrowserFuncs.retainobject(aObject);
}

nsresult
PluginScriptableObjectChild::AnswerInvalidate()
{
  NS_ENSURE_STATE(NS_IsMainThread());
  if (mObject) {
    PluginModuleChild::sBrowserFuncs.releaseobject(mObject);
    mObject = nsnull;
    return NS_OK;
  }
  return NS_ERROR_UNEXPECTED;
}

nsresult
PluginScriptableObjectChild::AnswerHasMethod(const NPRemoteIdentifier& aId,
                                             bool* aHasMethod)
{
  NS_ENSURE_STATE(NS_IsMainThread());
  NS_ENSURE_STATE(mObject);

  if (!(mObject->_class && mObject->_class->hasMethod)) {
    *aHasMethod = false;
    return NS_OK;
  }

  *aHasMethod = mObject->_class->hasMethod(mObject, (NPIdentifier)aId);
  return NS_OK;
}

nsresult
PluginScriptableObjectChild::AnswerInvoke(const NPRemoteIdentifier& aId,
                                          const nsTArray<NPRemoteVariant>& aArgs,
                                          NPRemoteVariant* aResult,
                                          bool* aSuccess)
{
  NS_ENSURE_STATE(NS_IsMainThread());
  NS_ENSURE_STATE(mObject);

  if (!(mObject->_class && mObject->_class->invoke)) {
    *aSuccess = false;
    return NS_OK;
  }

  nsAutoTArray<NPVariant, 10> convertedArgs;
  PRUint32 argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount)) {
    *aSuccess = false;
    return NS_OK;
  }

  for (PRUint32 index = 0; index < argCount; index++) {
    if (!ConvertToVariant(aArgs[index], convertedArgs[index])) {
      *aSuccess = false;
      return NS_OK;
    }
  }

  NPVariant result;
  bool success = mObject->_class->invoke(mObject, (NPIdentifier)aId,
                                         convertedArgs.Elements(), argCount,
                                         &result);
  if (!success) {
    *aSuccess = false;
    return NS_OK;
  }

  NPRemoteVariant convertedResult;
  if (!ConvertToRemoteVariant(result, convertedResult, mInstance)) {
    *aSuccess = false;
    return NS_OK;
  }

  *aSuccess = true;
  *aResult = convertedResult;
  return NS_OK;
}

nsresult
PluginScriptableObjectChild::AnswerInvokeDefault(const nsTArray<NPRemoteVariant>& aArgs,
                                                 NPRemoteVariant* aResult,
                                                 bool* aSuccess)
{
  NS_ENSURE_STATE(NS_IsMainThread());
  NS_ENSURE_STATE(mObject);

  if (!(mObject->_class && mObject->_class->invokeDefault)) {
    *aSuccess = false;
    return NS_OK;
  }

  nsAutoTArray<NPVariant, 10> convertedArgs;
  PRUint32 argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount)) {
    *aSuccess = false;
    return NS_OK;
  }

  for (PRUint32 index = 0; index < argCount; index++) {
    if (!ConvertToVariant(aArgs[index], convertedArgs[index])) {
      *aSuccess = false;
      return NS_OK;
    }
  }

  NPVariant result;
  bool success = mObject->_class->invokeDefault(mObject,
                                                convertedArgs.Elements(),
                                                argCount, &result);
  if (!success) {
    *aSuccess = false;
    return NS_OK;
  }

  NPRemoteVariant convertedResult;
  if (!ConvertToRemoteVariant(result, convertedResult, mInstance)) {
    *aSuccess = false;
    return NS_OK;
  }

  *aSuccess = true;
  *aResult = convertedResult;
  return NS_OK;
}

nsresult
PluginScriptableObjectChild::AnswerHasProperty(const NPRemoteIdentifier& aId,
                                               bool* aHasProperty)
{
  NS_ENSURE_STATE(NS_IsMainThread());
  NS_ENSURE_STATE(mObject);

  if (!(mObject->_class && mObject->_class->hasProperty)) {
    *aHasProperty = false;
    return NS_OK;
  }

  *aHasProperty = mObject->_class->hasProperty(mObject, (NPIdentifier)aId);
  return NS_OK;
}

nsresult
PluginScriptableObjectChild::AnswerGetProperty(const NPRemoteIdentifier& aId,
                                               NPRemoteVariant* aResult,
                                               bool* aSuccess)
{
  NS_ENSURE_STATE(NS_IsMainThread());
  NS_ENSURE_STATE(mObject);

  if (!(mObject->_class && mObject->_class->getProperty)) {
    *aSuccess = false;
    return NS_OK;
  }

  NPVariant result;
  if (!mObject->_class->getProperty(mObject, (NPIdentifier)aId, &result)) {
    *aSuccess = false;
    return NS_OK;
  }

  NPRemoteVariant converted;
  if ((*aSuccess = ConvertToRemoteVariant(result, converted, mInstance))) {
    *aResult = converted;
  }
  return NS_OK;
}

nsresult
PluginScriptableObjectChild::AnswerSetProperty(const NPRemoteIdentifier& aId,
                                               const NPRemoteVariant& aValue,
                                               bool* aSuccess)
{
  NS_ENSURE_STATE(NS_IsMainThread());
  NS_ENSURE_STATE(mObject);

  if (!(mObject->_class && mObject->_class->setProperty)) {
    *aSuccess = false;
    return NS_OK;
  }

  NPVariant converted;
  if (!ConvertToVariant(aValue, converted)) {
    *aSuccess = false;
    return NS_OK;
  }

  *aSuccess = mObject->_class->setProperty(mObject, (NPIdentifier)aId,
                                           &converted);
  return NS_OK;
}

nsresult
PluginScriptableObjectChild::AnswerRemoveProperty(const NPRemoteIdentifier& aId,
                                                  bool* aSuccess)
{
  NS_ENSURE_STATE(NS_IsMainThread());
  NS_ENSURE_STATE(mObject);

  if (!(mObject->_class && mObject->_class->removeProperty)) {
    *aSuccess = false;
    return NS_OK;
  }

  *aSuccess = mObject->_class->removeProperty(mObject, (NPIdentifier)aId);
  return NS_OK;
}

nsresult
PluginScriptableObjectChild::AnswerEnumerate(nsTArray<NPRemoteIdentifier>* aProperties,
                                             bool* aSuccess)
{
  NS_ENSURE_STATE(NS_IsMainThread());
  NS_ENSURE_STATE(mObject);

  if (!(mObject->_class && mObject->_class->enumerate)) {
    *aSuccess = false;
    return NS_OK;
  }

  NPIdentifier* ids;
  uint32_t idCount;
  if (!mObject->_class->enumerate(mObject, &ids, &idCount)) {
    *aSuccess = false;
    return NS_OK;
  }

  if (!aProperties->SetCapacity(idCount)) {
    PluginModuleChild::sBrowserFuncs.memfree(ids);
    *aSuccess = false;
    return NS_OK;
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
  return NS_OK;
}

nsresult
PluginScriptableObjectChild::AnswerConstruct(const nsTArray<NPRemoteVariant>& aArgs,
                                             NPRemoteVariant* aResult,
                                             bool* aSuccess)
{
  NS_ENSURE_STATE(NS_IsMainThread());
  NS_ENSURE_STATE(mObject);

  if (!(mObject->_class && mObject->_class->construct)) {
    *aSuccess = false;
    return NS_OK;
  }

  nsAutoTArray<NPVariant, 10> convertedArgs;
  PRUint32 argCount = aArgs.Length();

  if (!convertedArgs.SetLength(argCount)) {
    *aSuccess = false;
    return NS_OK;
  }

  for (PRUint32 index = 0; index < argCount; index++) {
    if (!ConvertToVariant(aArgs[index], convertedArgs[index])) {
      *aSuccess = false;
      return NS_OK;
    }
  }

  NPVariant result;
  bool success = mObject->_class->construct(mObject, convertedArgs.Elements(),
                                            argCount, &result);
  if (!success) {
    *aSuccess = false;
    return NS_OK;
  }

  NPRemoteVariant convertedResult;
  if (!ConvertToRemoteVariant(result, convertedResult, mInstance)) {
    *aSuccess = false;
    return NS_OK;
  }

  *aSuccess = true;
  *aResult = convertedResult;
  return NS_OK;
}
