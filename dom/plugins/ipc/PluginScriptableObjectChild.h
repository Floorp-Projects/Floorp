/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_PluginScriptableObjectChild_h
#define dom_plugins_PluginScriptableObjectChild_h 1

#include "mozilla/plugins/PPluginScriptableObjectChild.h"
#include "mozilla/plugins/PluginMessageUtils.h"
#include "mozilla/plugins/PluginTypes.h"

#include "npruntime.h"
#include "nsDataHashtable.h"

namespace mozilla {
namespace plugins {

class PluginInstanceChild;
class PluginScriptableObjectChild;

struct ChildNPObject : NPObject
{
  ChildNPObject()
    : NPObject(), parent(nullptr), invalidated(false)
  {
    MOZ_COUNT_CTOR(ChildNPObject);
  }

  ~ChildNPObject()
  {
    MOZ_COUNT_DTOR(ChildNPObject);
  }

  // |parent| is always valid as long as the actor is alive. Once the actor is
  // destroyed this will be set to null.
  PluginScriptableObjectChild* parent;
  bool invalidated;
};

class PluginScriptableObjectChild : public PPluginScriptableObjectChild
{
  friend class PluginInstanceChild;

public:
  explicit PluginScriptableObjectChild(ScriptableObjectType aType);
  virtual ~PluginScriptableObjectChild();

  bool
  InitializeProxy();

  void
  InitializeLocal(NPObject* aObject);


  virtual bool
  AnswerInvalidate() MOZ_OVERRIDE;

  virtual bool
  AnswerHasMethod(const PluginIdentifier& aId,
                  bool* aHasMethod) MOZ_OVERRIDE;

  virtual bool
  AnswerInvoke(const PluginIdentifier& aId,
               const InfallibleTArray<Variant>& aArgs,
               Variant* aResult,
               bool* aSuccess) MOZ_OVERRIDE;

  virtual bool
  AnswerInvokeDefault(const InfallibleTArray<Variant>& aArgs,
                      Variant* aResult,
                      bool* aSuccess) MOZ_OVERRIDE;

  virtual bool
  AnswerHasProperty(const PluginIdentifier& aId,
                    bool* aHasProperty) MOZ_OVERRIDE;

  virtual bool
  AnswerGetChildProperty(const PluginIdentifier& aId,
                         bool* aHasProperty,
                         bool* aHasMethod,
                         Variant* aResult,
                         bool* aSuccess) MOZ_OVERRIDE;

  virtual bool
  AnswerSetProperty(const PluginIdentifier& aId,
                    const Variant& aValue,
                    bool* aSuccess) MOZ_OVERRIDE;

  virtual bool
  AnswerRemoveProperty(const PluginIdentifier& aId,
                       bool* aSuccess) MOZ_OVERRIDE;

  virtual bool
  AnswerEnumerate(InfallibleTArray<PluginIdentifier>* aProperties,
                  bool* aSuccess) MOZ_OVERRIDE;

  virtual bool
  AnswerConstruct(const InfallibleTArray<Variant>& aArgs,
                  Variant* aResult,
                  bool* aSuccess) MOZ_OVERRIDE;

  virtual bool
  RecvProtect() MOZ_OVERRIDE;

  virtual bool
  RecvUnprotect() MOZ_OVERRIDE;

  NPObject*
  GetObject(bool aCanResurrect);

  static const NPClass*
  GetClass()
  {
    return &sNPClass;
  }

  PluginInstanceChild*
  GetInstance() const
  {
    return mInstance;
  }

  // Protect only affects LocalObject actors. It is called by the
  // ProtectedVariant/Actor helper classes before the actor is used as an
  // argument to an IPC call and when the parent process resurrects a
  // proxy object to the NPObject associated with this actor.
  void Protect();

  // Unprotect only affects LocalObject actors. It is called by the
  // ProtectedVariant/Actor helper classes after the actor is used as an
  // argument to an IPC call and when the parent process is no longer using
  // this actor.
  void Unprotect();

  // DropNPObject is only used for Proxy actors and is called when the child
  // process is no longer using the NPObject associated with this actor. The
  // parent process may subsequently use this actor again in which case a new
  // NPObject will be created and associated with this actor (see
  // ResurrectProxyObject).
  void DropNPObject();

  /**
   * After NPP_Destroy, all NPObjects associated with an instance are
   * destroyed. We are informed of this destruction. This should only be called
   * on Local actors.
   */
  void NPObjectDestroyed();

  bool
  Evaluate(NPString* aScript,
           NPVariant* aResult);

  ScriptableObjectType
  Type() const {
    return mType;
  }

private:
  struct StoredIdentifier
  {
    nsCString mIdentifier;
    nsAutoRefCnt mRefCnt;
    bool mPermanent;

    nsrefcnt AddRef() {
      ++mRefCnt;
      return mRefCnt;
    }

    nsrefcnt Release() {
      --mRefCnt;
      if (mRefCnt == 0) {
        delete this;
        return 0;
      }
      return mRefCnt;
    }

    explicit StoredIdentifier(const nsCString& aIdentifier)
      : mIdentifier(aIdentifier), mRefCnt(), mPermanent(false)
    { MOZ_COUNT_CTOR(StoredIdentifier); }

    ~StoredIdentifier() { MOZ_COUNT_DTOR(StoredIdentifier); }
  };

public:
  class MOZ_STACK_CLASS StackIdentifier
  {
  public:
    StackIdentifier(const PluginIdentifier& aIdentifier);
    StackIdentifier(NPIdentifier aIdentifier);
    ~StackIdentifier();

    void MakePermanent()
    {
      if (mStored) {
        mStored->mPermanent = true;
      }
    }
    NPIdentifier ToNPIdentifier() const;

    bool IsString() const { return mIdentifier.type() == PluginIdentifier::TnsCString; }
    const nsCString& GetString() const { return mIdentifier.get_nsCString(); }

    int32_t GetInt() const { return mIdentifier.get_int32_t(); }

    PluginIdentifier GetIdentifier() const { return mIdentifier; }

   private:
    DISALLOW_COPY_AND_ASSIGN(StackIdentifier);

    PluginIdentifier mIdentifier;
    nsRefPtr<StoredIdentifier> mStored;
  };

  static void ClearIdentifiers();

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
  // parent process uses an actor whose NPObject was deleted by the child
  // process.
  bool ResurrectProxyObject();

private:
  PluginInstanceChild* mInstance;
  NPObject* mObject;
  bool mInvalidated;
  int mProtectCount;

  ScriptableObjectType mType;

  static const NPClass sNPClass;

  static StoredIdentifier* HashIdentifier(const nsCString& aIdentifier);
  static void UnhashIdentifier(StoredIdentifier* aIdentifier);

  typedef nsDataHashtable<nsCStringHashKey, nsRefPtr<StoredIdentifier>> IdentifierTable;
  static IdentifierTable sIdentifiers;
};

} /* namespace plugins */
} /* namespace mozilla */

#endif /* dom_plugins_PluginScriptableObjectChild_h */
