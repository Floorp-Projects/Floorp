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
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
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

#ifndef dom_plugins_PluginScriptableObjectUtils_h
#define dom_plugins_PluginScriptableObjectUtils_h

#include "PluginModuleParent.h"
#include "PluginModuleChild.h"
#include "PluginInstanceParent.h"
#include "PluginInstanceChild.h"
#include "PluginScriptableObjectParent.h"
#include "PluginScriptableObjectChild.h"

#include "npapi.h"
#include "npfunctions.h"
#include "npruntime.h"

#include "nsDebug.h"

namespace mozilla {
namespace plugins {

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
  if (!object->parent) {
    return nsnull;
  }
  return object->parent->GetInstance();
}

inline NPObject*
NPObjectFromVariant(const Variant& aRemoteVariant)
{
  switch (aRemoteVariant.type()) {
    case Variant::TPPluginScriptableObjectParent: {
      PluginScriptableObjectParent* actor =
        const_cast<PluginScriptableObjectParent*>(
          reinterpret_cast<const PluginScriptableObjectParent*>(
            aRemoteVariant.get_PPluginScriptableObjectParent()));
      return actor->GetObject(true);
    }

    case Variant::TPPluginScriptableObjectChild: {
      PluginScriptableObjectChild* actor =
        const_cast<PluginScriptableObjectChild*>(
          reinterpret_cast<const PluginScriptableObjectChild*>(
            aRemoteVariant.get_PPluginScriptableObjectChild()));
      return actor->GetObject(true);
    }

    default:
      NS_NOTREACHED("Shouldn't get here!");
      return nsnull;
  }
}

inline NPObject*
NPObjectFromVariant(const NPVariant& aVariant)
{
  NS_ASSERTION(NPVARIANT_IS_OBJECT(aVariant), "Wrong variant type!");
  return NPVARIANT_TO_OBJECT(aVariant);
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

inline void
ReleaseRemoteVariant(Variant& aVariant)
{
  switch (aVariant.type()) {
    case Variant::TPPluginScriptableObjectParent: {
      PluginScriptableObjectParent* actor =
        const_cast<PluginScriptableObjectParent*>(
          reinterpret_cast<const PluginScriptableObjectParent*>(
            aVariant.get_PPluginScriptableObjectParent()));
      actor->Unprotect();
      break;
    }

    case Variant::TPPluginScriptableObjectChild: {
      NS_ASSERTION(PluginModuleChild::current(),
                   "Should only be running in the child!");
      PluginScriptableObjectChild* actor =
        const_cast<PluginScriptableObjectChild*>(
          reinterpret_cast<const PluginScriptableObjectChild*>(
            aVariant.get_PPluginScriptableObjectChild()));
      actor->Unprotect();
      break;
    }

  default:
    break; // Intentional fall-through for other variant types.
  }

  aVariant = mozilla::void_t();
}

bool
ConvertToVariant(const Variant& aRemoteVariant,
                 NPVariant& aVariant,
                 PluginInstanceParent* aInstance = nsnull);

template <class InstanceType>
bool
ConvertToRemoteVariant(const NPVariant& aVariant,
                       Variant& aRemoteVariant,
                       InstanceType* aInstance,
                       bool aProtectActors = false);

class ProtectedVariant
{
public:
  ProtectedVariant(const NPVariant& aVariant,
                   PluginInstanceParent* aInstance)
  {
    mOk = ConvertToRemoteVariant(aVariant, mVariant, aInstance, true);
  }

  ProtectedVariant(const NPVariant& aVariant,
                   PluginInstanceChild* aInstance)
  {
    mOk = ConvertToRemoteVariant(aVariant, mVariant, aInstance, true);
  }

  ~ProtectedVariant() {
    ReleaseRemoteVariant(mVariant);
  }

  bool IsOk() {
    return mOk;
  }

  operator const Variant&() {
    return mVariant;
  }

private:
  Variant mVariant;
  bool mOk;
};

class ProtectedVariantArray
{
public:
  ProtectedVariantArray(const NPVariant* aArgs,
                        PRUint32 aCount,
                        PluginInstanceParent* aInstance)
    : mUsingShadowArray(false)
  {
    for (PRUint32 index = 0; index < aCount; index++) {
      Variant* remoteVariant = mArray.AppendElement();
      if (!(remoteVariant && 
            ConvertToRemoteVariant(aArgs[index], *remoteVariant, aInstance,
                                   true))) {
        mOk = false;
        return;
      }
    }
    mOk = true;
  }

  ProtectedVariantArray(const NPVariant* aArgs,
                        PRUint32 aCount,
                        PluginInstanceChild* aInstance)
    : mUsingShadowArray(false)
  {
    for (PRUint32 index = 0; index < aCount; index++) {
      Variant* remoteVariant = mArray.AppendElement();
      if (!(remoteVariant && 
            ConvertToRemoteVariant(aArgs[index], *remoteVariant, aInstance,
                                   true))) {
        mOk = false;
        return;
      }
    }
    mOk = true;
  }

  ~ProtectedVariantArray()
  {
    InfallibleTArray<Variant>& vars = EnsureAndGetShadowArray();
    PRUint32 count = vars.Length();
    for (PRUint32 index = 0; index < count; index++) {
      ReleaseRemoteVariant(vars[index]);
    }
  }

  operator const InfallibleTArray<Variant>&()
  {
    return EnsureAndGetShadowArray();
  }

  bool IsOk()
  {
    return mOk;
  }

private:
  InfallibleTArray<Variant>&
  EnsureAndGetShadowArray()
  {
    if (!mUsingShadowArray) {
      mShadowArray.SwapElements(mArray);
      mUsingShadowArray = true;
    }
    return mShadowArray;
  }

  // We convert the variants fallibly, but pass them to Call*()
  // methods as an infallible array
  nsTArray<Variant> mArray;
  InfallibleTArray<Variant> mShadowArray;
  bool mOk;
  bool mUsingShadowArray;
};

template<class ActorType>
struct ProtectedActorTraits
{
  static bool Nullable();
};

template<class ActorType, class Traits=ProtectedActorTraits<ActorType> >
class ProtectedActor
{
public:
  ProtectedActor(ActorType* aActor) : mActor(aActor)
  {
    if (!Traits::Nullable()) {
      NS_ASSERTION(mActor, "This should never be null!");
    }
  }

  ~ProtectedActor()
  {
    if (Traits::Nullable() && !mActor)
      return;
    mActor->Unprotect();
  }

  ActorType* operator->()
  {
    return mActor;
  }

  operator bool()
  {
    return !!mActor;
  }

private:
  ActorType* mActor;
};

template<>
struct ProtectedActorTraits<PluginScriptableObjectParent>
{
  static bool Nullable() { return true; }
};

template<>
struct ProtectedActorTraits<PluginScriptableObjectChild>
{
  static bool Nullable() { return false; }
};

} /* namespace plugins */
} /* namespace mozilla */

#include "PluginScriptableObjectUtils-inl.h"

#endif /* dom_plugins_PluginScriptableObjectUtils_h */
