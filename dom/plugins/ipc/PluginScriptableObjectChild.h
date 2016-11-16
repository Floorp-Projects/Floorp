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


  virtual mozilla::ipc::IPCResult
  AnswerInvalidate() override;

  virtual mozilla::ipc::IPCResult
  AnswerHasMethod(const PluginIdentifier& aId,
                  bool* aHasMethod) override;

  virtual mozilla::ipc::IPCResult
  AnswerInvoke(const PluginIdentifier& aId,
               InfallibleTArray<Variant>&& aArgs,
               Variant* aResult,
               bool* aSuccess) override;

  virtual mozilla::ipc::IPCResult
  AnswerInvokeDefault(InfallibleTArray<Variant>&& aArgs,
                      Variant* aResult,
                      bool* aSuccess) override;

  virtual mozilla::ipc::IPCResult
  AnswerHasProperty(const PluginIdentifier& aId,
                    bool* aHasProperty) override;

  virtual mozilla::ipc::IPCResult
  AnswerGetChildProperty(const PluginIdentifier& aId,
                         bool* aHasProperty,
                         bool* aHasMethod,
                         Variant* aResult,
                         bool* aSuccess) override;

  virtual mozilla::ipc::IPCResult
  AnswerSetProperty(const PluginIdentifier& aId,
                    const Variant& aValue,
                    bool* aSuccess) override;

  virtual mozilla::ipc::IPCResult
  AnswerRemoveProperty(const PluginIdentifier& aId,
                       bool* aSuccess) override;

  virtual mozilla::ipc::IPCResult
  AnswerEnumerate(InfallibleTArray<PluginIdentifier>* aProperties,
                  bool* aSuccess) override;

  virtual mozilla::ipc::IPCResult
  AnswerConstruct(InfallibleTArray<Variant>&& aArgs,
                  Variant* aResult,
                  bool* aSuccess) override;

  virtual mozilla::ipc::IPCResult
  RecvProtect() override;

  virtual mozilla::ipc::IPCResult
  RecvUnprotect() override;

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
    explicit StackIdentifier(const PluginIdentifier& aIdentifier);
    explicit StackIdentifier(NPIdentifier aIdentifier);
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
    RefPtr<StoredIdentifier> mStored;
  };

  static void ClearIdentifiers();

  bool RegisterActor(NPObject* aObject);
  void UnregisterActor(NPObject* aObject);

  static PluginScriptableObjectChild* GetActorForNPObject(NPObject* aObject);

  static void RegisterObject(NPObject* aObject, PluginInstanceChild* aInstance);
  static void UnregisterObject(NPObject* aObject);

  static PluginInstanceChild* GetInstanceForNPObject(NPObject* aObject);

  /**
   * Fill PluginInstanceChild.mDeletingHash with all the remaining NPObjects
   * associated with that instance.
   */
  static void NotifyOfInstanceShutdown(PluginInstanceChild* aInstance);

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

  typedef nsDataHashtable<nsCStringHashKey, RefPtr<StoredIdentifier>> IdentifierTable;
  static IdentifierTable sIdentifiers;

  struct NPObjectData : public nsPtrHashKey<NPObject>
  {
    explicit NPObjectData(const NPObject* key)
    : nsPtrHashKey<NPObject>(key),
      instance(nullptr),
      actor(nullptr)
    { }

    // never nullptr
    PluginInstanceChild* instance;

    // sometimes nullptr (no actor associated with an NPObject)
    PluginScriptableObjectChild* actor;
  };

  /**
   * mObjectMap contains all the currently active NPObjects (from NPN_CreateObject until the
   * final release/dealloc, whether or not an actor is currently associated with the object.
   */
  static nsTHashtable<NPObjectData>* sObjectMap;
};

} /* namespace plugins */
} /* namespace mozilla */

#endif /* dom_plugins_PluginScriptableObjectChild_h */
