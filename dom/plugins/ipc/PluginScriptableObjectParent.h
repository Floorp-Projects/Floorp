/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_PluginScriptableObjectParent_h
#define dom_plugins_PluginScriptableObjectParent_h 1

#include "mozilla/plugins/PPluginScriptableObjectParent.h"

#include "npfunctions.h"
#include "npruntime.h"

namespace mozilla {
namespace plugins {

class PluginInstanceParent;
class PluginScriptableObjectParent;
class PPluginIdentifierParent;

struct ParentNPObject : NPObject
{
  ParentNPObject()
    : NPObject(), parent(NULL), invalidated(false) { }

  // |parent| is always valid as long as the actor is alive. Once the actor is
  // destroyed this will be set to null.
  PluginScriptableObjectParent* parent;
  bool invalidated;
};

class PluginScriptableObjectParent : public PPluginScriptableObjectParent
{
  friend class PluginInstanceParent;

public:
  PluginScriptableObjectParent(ScriptableObjectType aType);
  virtual ~PluginScriptableObjectParent();

  void
  InitializeProxy();

  void
  InitializeLocal(NPObject* aObject);

  virtual bool
  AnswerHasMethod(PPluginIdentifierParent* aId,
                  bool* aHasMethod);

  virtual bool
  AnswerInvoke(PPluginIdentifierParent* aId,
               const InfallibleTArray<Variant>& aArgs,
               Variant* aResult,
               bool* aSuccess);

  virtual bool
  AnswerInvokeDefault(const InfallibleTArray<Variant>& aArgs,
                      Variant* aResult,
                      bool* aSuccess);

  virtual bool
  AnswerHasProperty(PPluginIdentifierParent* aId,
                    bool* aHasProperty);

  virtual bool
  AnswerGetParentProperty(PPluginIdentifierParent* aId,
                          Variant* aResult,
                          bool* aSuccess);

  virtual bool
  AnswerSetProperty(PPluginIdentifierParent* aId,
                    const Variant& aValue,
                    bool* aSuccess);

  virtual bool
  AnswerRemoveProperty(PPluginIdentifierParent* aId,
                       bool* aSuccess);

  virtual bool
  AnswerEnumerate(InfallibleTArray<PPluginIdentifierParent*>* aProperties,
                  bool* aSuccess);

  virtual bool
  AnswerConstruct(const InfallibleTArray<Variant>& aArgs,
                  Variant* aResult,
                  bool* aSuccess);

  virtual bool
  AnswerNPN_Evaluate(const nsCString& aScript,
                     Variant* aResult,
                     bool* aSuccess);

  virtual bool
  RecvProtect();

  virtual bool
  RecvUnprotect();

  static const NPClass*
  GetClass()
  {
    return &sNPClass;
  }

  PluginInstanceParent*
  GetInstance() const
  {
    return mInstance;
  }

  NPObject*
  GetObject(bool aCanResurrect);

  // Protect only affects LocalObject actors. It is called by the
  // ProtectedVariant/Actor helper classes before the actor is used as an
  // argument to an IPC call and when the child process resurrects a
  // proxy object to the NPObject associated with this actor.
  void Protect();

  // Unprotect only affects LocalObject actors. It is called by the
  // ProtectedVariant/Actor helper classes after the actor is used as an
  // argument to an IPC call and when the child process is no longer using this
  // actor.
  void Unprotect();

  // DropNPObject is only used for Proxy actors and is called when the parent
  // process is no longer using the NPObject associated with this actor. The
  // child process may subsequently use this actor again in which case a new
  // NPObject will be created and associated with this actor (see
  // ResurrectProxyObject).
  void DropNPObject();

  ScriptableObjectType
  Type() const {
    return mType;
  }

  bool GetPropertyHelper(NPIdentifier aName,
                         bool* aHasProperty,
                         bool* aHasMethod,
                         NPVariant* aResult);

private:
  static NPObject*
  ScriptableAllocate(NPP aInstance,
                     NPClass* aClass);

  static void
  ScriptableInvalidate(NPObject* aObject);

  static void
  ScriptableDeallocate(NPObject* aObject);

  static bool
  ScriptableHasMethod(NPObject* aObject,
                      NPIdentifier aName);

  static bool
  ScriptableInvoke(NPObject* aObject,
                   NPIdentifier aName,
                   const NPVariant* aArgs,
                   uint32_t aArgCount,
                   NPVariant* aResult);

  static bool
  ScriptableInvokeDefault(NPObject* aObject,
                          const NPVariant* aArgs,
                          uint32_t aArgCount,
                          NPVariant* aResult);

  static bool
  ScriptableHasProperty(NPObject* aObject,
                        NPIdentifier aName);

  static bool
  ScriptableGetProperty(NPObject* aObject,
                        NPIdentifier aName,
                        NPVariant* aResult);

  static bool
  ScriptableSetProperty(NPObject* aObject,
                        NPIdentifier aName,
                        const NPVariant* aValue);

  static bool
  ScriptableRemoveProperty(NPObject* aObject,
                           NPIdentifier aName);

  static bool
  ScriptableEnumerate(NPObject* aObject,
                      NPIdentifier** aIdentifiers,
                      uint32_t* aCount);

  static bool
  ScriptableConstruct(NPObject* aObject,
                      const NPVariant* aArgs,
                      uint32_t aArgCount,
                      NPVariant* aResult);

  NPObject*
  CreateProxyObject();

  // ResurrectProxyObject is only used with Proxy actors. It is called when the
  // child process uses an actor whose NPObject was deleted by the parent
  // process.
  bool ResurrectProxyObject();

private:
  PluginInstanceParent* mInstance;

  // This may be a ParentNPObject or some other kind depending on who created
  // it. Have to check its class to find out.
  NPObject* mObject;
  int mProtectCount;

  ScriptableObjectType mType;

  static const NPClass sNPClass;
};

} /* namespace plugins */
} /* namespace mozilla */

#endif /* dom_plugins_PluginScriptableObjectParent_h */
