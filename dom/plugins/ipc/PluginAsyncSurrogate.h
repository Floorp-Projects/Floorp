/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_ipc_PluginAsyncSurrogate_h
#define dom_plugins_ipc_PluginAsyncSurrogate_h

#include "mozilla/UniquePtr.h"
#include "npapi.h"
#include "npfunctions.h"
#include "npruntime.h"
#include "nsAutoPtr.h"
#include "nsISupportsImpl.h"
#include "nsPluginHost.h"
#include "nsString.h"
#include "nsTArray.h"
#include "PluginDataResolver.h"

namespace mozilla {
namespace plugins {

class PluginInstanceParent;
class PluginModuleParent;

class PluginAsyncSurrogate : public PluginDataResolver
{
public:
  NS_INLINE_DECL_REFCOUNTING(PluginAsyncSurrogate)

  bool Init(NPMIMEType aPluginType, NPP aInstance, uint16_t aMode,
            int16_t aArgc, char* aArgn[], char* aArgv[]);
  nsresult NPP_New(NPError* aError);
  NPError NPP_Destroy(NPSavedData** aSave);
  NPError NPP_GetValue(NPPVariable aVariable, void* aRetval);
  NPError NPP_SetValue(NPNVariable aVariable, void* aValue);
  NPError NPP_NewStream(NPMIMEType aType, NPStream* aStream, NPBool aSeekable,
                        uint16_t* aStype);
  NPError NPP_SetWindow(NPWindow* aWindow);
  nsresult AsyncSetWindow(NPWindow* aWindow);
  void NPP_Print(NPPrint* aPrintInfo);
  int16_t NPP_HandleEvent(void* aEvent);
  int32_t NPP_WriteReady(NPStream* aStream);
  void OnInstanceCreated(PluginInstanceParent* aInstance);
  static bool Create(PluginModuleParent* aParent, NPMIMEType aPluginType,
                     NPP aInstance, uint16_t aMode, int16_t aArgc,
                     char* aArgn[], char* aArgv[]);
  static const NPClass* GetClass() { return &sNPClass; }
  static void NP_GetEntryPoints(NPPluginFuncs* aFuncs);
  static PluginAsyncSurrogate* Cast(NPP aInstance);

  virtual PluginAsyncSurrogate*
  GetAsyncSurrogate() { return this; }

  virtual PluginInstanceParent*
  GetInstance() { return nullptr; }

  NPP GetNPP() { return mInstance; }

  bool GetPropertyHelper(NPObject* aObject, NPIdentifier aName,
                         bool* aHasProperty, bool* aHasMethod,
                         NPVariant* aResult);

  PluginModuleParent*
  GetParent() { return mParent; }

  bool SetAcceptingCalls(bool aAccept)
  {
    bool prevState = mAcceptCalls;
    if (mInstantiated) {
      aAccept = true;
    }
    mAcceptCalls = aAccept;
    return prevState;
  }

  void AsyncCallDeparting();
  void AsyncCallArriving();

  void NotifyAsyncInitFailed();
  void DestroyAsyncStream(NPStream* aStream);

private:
  explicit PluginAsyncSurrogate(PluginModuleParent* aParent);
  virtual ~PluginAsyncSurrogate();

  bool WaitForInit();

  static bool SetStreamType(NPStream* aStream, uint16_t aStreamType);

  static NPError NPP_Destroy(NPP aInstance, NPSavedData** aSave);
  static NPError NPP_GetValue(NPP aInstance, NPPVariable aVariable, void* aRetval);
  static NPError NPP_SetValue(NPP aInstance, NPNVariable aVariable, void* aValue);
  static NPError NPP_NewStream(NPP aInstance, NPMIMEType aType, NPStream* aStream,
                               NPBool aSeekable, uint16_t* aStype);
  static NPError NPP_SetWindow(NPP aInstance, NPWindow* aWindow);
  static void NPP_Print(NPP aInstance, NPPrint* aPrintInfo);
  static int16_t NPP_HandleEvent(NPP aInstance, void* aEvent);
  static int32_t NPP_WriteReady(NPP aInstance, NPStream* aStream);

  static NPObject* ScriptableAllocate(NPP aInstance, NPClass* aClass);
  static void ScriptableInvalidate(NPObject* aObject);
  static void ScriptableDeallocate(NPObject* aObject);
  static bool ScriptableHasMethod(NPObject* aObject, NPIdentifier aName);
  static bool ScriptableInvoke(NPObject* aObject, NPIdentifier aName,
                               const NPVariant* aArgs, uint32_t aArgCount,
                               NPVariant* aResult);
  static bool ScriptableInvokeDefault(NPObject* aObject, const NPVariant* aArgs,
                                      uint32_t aArgCount, NPVariant* aResult);
  static bool ScriptableHasProperty(NPObject* aObject, NPIdentifier aName);
  static bool ScriptableGetProperty(NPObject* aObject, NPIdentifier aName,
                                    NPVariant* aResult);
  static bool ScriptableSetProperty(NPObject* aObject, NPIdentifier aName,
                                    const NPVariant* aValue);
  static bool ScriptableRemoveProperty(NPObject* aObject, NPIdentifier aName);
  static bool ScriptableEnumerate(NPObject* aObject, NPIdentifier** aIdentifiers,
                                  uint32_t* aCount);
  static bool ScriptableConstruct(NPObject* aObject, const NPVariant* aArgs,
                                  uint32_t aArgCount, NPVariant* aResult);
  static nsNPAPIPluginStreamListener* GetStreamListener(NPStream* aStream);

private:
  struct PendingNewStreamCall
  {
    PendingNewStreamCall(NPMIMEType aType, NPStream* aStream, NPBool aSeekable);
    ~PendingNewStreamCall() {}
    nsCString   mType;
    NPStream*   mStream;
    NPBool      mSeekable;
  };

private:
  PluginModuleParent*             mParent;
  // These values are used to construct the plugin instance
  nsCString                       mMimeType;
  NPP                             mInstance;
  uint16_t                        mMode;
  InfallibleTArray<nsCString>     mNames;
  InfallibleTArray<nsCString>     mValues;
  // This is safe to store as a pointer because the spec says it will remain
  // valid until destruction or a subsequent NPP_SetWindow call.
  NPWindow*                       mWindow;
  nsTArray<PendingNewStreamCall>  mPendingNewStreamCalls;
  UniquePtr<PluginDestructionGuard> mPluginDestructionGuard;

  bool      mAcceptCalls;
  bool      mInstantiated;
  bool      mAsyncSetWindow;
  bool      mInitCancelled;
  int32_t   mAsyncCallsInFlight;

  static const NPClass sNPClass;
};

struct AsyncNPObject : NPObject
{
  explicit AsyncNPObject(PluginAsyncSurrogate* aSurrogate);
  ~AsyncNPObject();

  NPObject* GetRealObject();

  nsRefPtr<PluginAsyncSurrogate>  mSurrogate;
  NPObject*                       mRealObject;
};

class MOZ_STACK_CLASS PushSurrogateAcceptCalls
{
public:
  explicit PushSurrogateAcceptCalls(PluginInstanceParent* aInstance);
  ~PushSurrogateAcceptCalls();

private:
  PluginAsyncSurrogate* mSurrogate;
  bool                  mPrevAcceptCallsState;
};

} // namespace plugins
} // namespace mozilla

#endif // dom_plugins_ipc_PluginAsyncSurrogate_h
