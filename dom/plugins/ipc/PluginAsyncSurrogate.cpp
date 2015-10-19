/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginAsyncSurrogate.h"

#include "base/message_loop.h"
#include "base/message_pump_default.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/plugins/PluginInstanceParent.h"
#include "mozilla/plugins/PluginModuleParent.h"
#include "mozilla/plugins/PluginScriptableObjectParent.h"
#include "mozilla/Telemetry.h"
#include "nsJSNPRuntime.h"
#include "nsNPAPIPlugin.h"
#include "nsNPAPIPluginInstance.h"
#include "nsNPAPIPluginStreamListener.h"
#include "nsPluginInstanceOwner.h"
#include "nsPluginStreamListenerPeer.h"
#include "npruntime.h"
#include "nsThreadUtils.h"
#include "PluginMessageUtils.h"

namespace mozilla {
namespace plugins {

AsyncNPObject::AsyncNPObject(PluginAsyncSurrogate* aSurrogate)
  : NPObject()
  , mSurrogate(aSurrogate)
  , mRealObject(nullptr)
{
}

AsyncNPObject::~AsyncNPObject()
{
  if (mRealObject) {
    --mRealObject->asyncWrapperCount;
    parent::_releaseobject(mRealObject);
    mRealObject = nullptr;
  }
}

NPObject*
AsyncNPObject::GetRealObject()
{
  if (mRealObject) {
    return mRealObject;
  }
  PluginInstanceParent* instance = PluginInstanceParent::Cast(mSurrogate->GetNPP());
  if (!instance) {
    return nullptr;
  }
  NPObject* realObject = nullptr;
  NPError err = instance->NPP_GetValue(NPPVpluginScriptableNPObject,
                                       &realObject);
  if (err != NPERR_NO_ERROR) {
    return nullptr;
  }
  if (realObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    parent::_releaseobject(realObject);
    return nullptr;
  }
  mRealObject = static_cast<ParentNPObject*>(realObject);
  ++mRealObject->asyncWrapperCount;
  return mRealObject;
}

class MOZ_STACK_CLASS RecursionGuard
{
public:
  RecursionGuard()
    : mIsRecursive(sHasEntered)
  {
    if (!mIsRecursive) {
      sHasEntered = true;
    }
  }

  ~RecursionGuard()
  {
    if (!mIsRecursive) {
      sHasEntered = false;
    }
  }

  inline bool
  IsRecursive()
  {
    return mIsRecursive;
  }

private:
  bool        mIsRecursive;
  static bool sHasEntered;
};

bool RecursionGuard::sHasEntered = false;

PluginAsyncSurrogate::PluginAsyncSurrogate(PluginModuleParent* aParent)
  : mParent(aParent)
  , mMode(0)
  , mWindow(nullptr)
  , mAcceptCalls(false)
  , mInstantiated(false)
  , mAsyncSetWindow(false)
  , mInitCancelled(false)
  , mDestroyPending(false)
  , mAsyncCallsInFlight(0)
{
  MOZ_ASSERT(aParent);
}

PluginAsyncSurrogate::~PluginAsyncSurrogate()
{
}

bool
PluginAsyncSurrogate::Init(NPMIMEType aPluginType, NPP aInstance, uint16_t aMode,
                           int16_t aArgc, char* aArgn[], char* aArgv[])
{
  mMimeType = aPluginType;
  nsNPAPIPluginInstance* instance =
    static_cast<nsNPAPIPluginInstance*>(aInstance->ndata);
  MOZ_ASSERT(instance);
  mInstance = instance;
  mMode = aMode;
  for (int i = 0; i < aArgc; ++i) {
    mNames.AppendElement(NullableString(aArgn[i]));
    mValues.AppendElement(NullableString(aArgv[i]));
  }
  return true;
}

/* static */ bool
PluginAsyncSurrogate::Create(PluginModuleParent* aParent, NPMIMEType aPluginType,
                             NPP aInstance, uint16_t aMode, int16_t aArgc,
                             char* aArgn[], char* aArgv[])
{
  nsRefPtr<PluginAsyncSurrogate> surrogate(new PluginAsyncSurrogate(aParent));
  if (!surrogate->Init(aPluginType, aInstance, aMode, aArgc, aArgn, aArgv)) {
    return false;
  }
  PluginAsyncSurrogate* rawSurrogate = nullptr;
  surrogate.forget(&rawSurrogate);
  aInstance->pdata = static_cast<PluginDataResolver*>(rawSurrogate);
  return true;
}

/* static */ PluginAsyncSurrogate*
PluginAsyncSurrogate::Cast(NPP aInstance)
{
  MOZ_ASSERT(aInstance);
  PluginDataResolver* resolver =
    reinterpret_cast<PluginDataResolver*>(aInstance->pdata);
  if (!resolver) {
    return nullptr;
  }
  return resolver->GetAsyncSurrogate();
}

nsresult
PluginAsyncSurrogate::NPP_New(NPError* aError)
{
  if (!mInstance) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = mParent->NPP_NewInternal(mMimeType.BeginWriting(), GetNPP(),
                                         mMode, mNames, mValues, nullptr,
                                         aError);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

void
PluginAsyncSurrogate::NP_GetEntryPoints(NPPluginFuncs* aFuncs)
{
  aFuncs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  aFuncs->destroy = &NPP_Destroy;
  aFuncs->getvalue = &NPP_GetValue;
  aFuncs->setvalue = &NPP_SetValue;
  aFuncs->newstream = &NPP_NewStream;
  aFuncs->setwindow = &NPP_SetWindow;
  aFuncs->writeready = &NPP_WriteReady;
  aFuncs->event = &NPP_HandleEvent;
  aFuncs->destroystream = &NPP_DestroyStream;
  // We need to set these so that content code doesn't make assumptions
  // about these operations not being supported
  aFuncs->write = &PluginModuleParent::NPP_Write;
  aFuncs->asfile = &PluginModuleParent::NPP_StreamAsFile;
}

/* static */ void
PluginAsyncSurrogate::NotifyDestroyPending(NPP aInstance)
{
  PluginAsyncSurrogate* surrogate = Cast(aInstance);
  if (!surrogate) {
    return;
  }
  surrogate->NotifyDestroyPending();
}

NPP
PluginAsyncSurrogate::GetNPP()
{
  MOZ_ASSERT(mInstance);
  NPP npp;
  DebugOnly<nsresult> rv = mInstance->GetNPP(&npp);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return npp;
}

void
PluginAsyncSurrogate::NotifyDestroyPending()
{
  mDestroyPending = true;
  nsJSNPRuntime::OnPluginDestroyPending(GetNPP());
}

NPError
PluginAsyncSurrogate::NPP_Destroy(NPSavedData** aSave)
{
  NotifyDestroyPending();
  if (!WaitForInit()) {
    return NPERR_GENERIC_ERROR;
  }
  return PluginModuleParent::NPP_Destroy(GetNPP(), aSave);
}

NPError
PluginAsyncSurrogate::NPP_GetValue(NPPVariable aVariable, void* aRetval)
{
  if (aVariable != NPPVpluginScriptableNPObject) {
    if (!WaitForInit()) {
      return NPERR_GENERIC_ERROR;
    }

    PluginInstanceParent* instance = PluginInstanceParent::Cast(GetNPP());
    MOZ_ASSERT(instance);
    return instance->NPP_GetValue(aVariable, aRetval);
  }

  NPObject* npobject = parent::_createobject(GetNPP(),
                                             const_cast<NPClass*>(GetClass()));
  MOZ_ASSERT(npobject);
  MOZ_ASSERT(npobject->_class == GetClass());
  MOZ_ASSERT(npobject->referenceCount == 1);
  *(NPObject**)aRetval = npobject;
  return npobject ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
}

NPError
PluginAsyncSurrogate::NPP_SetValue(NPNVariable aVariable, void* aValue)
{
  if (!WaitForInit()) {
    return NPERR_GENERIC_ERROR;
  }
  return PluginModuleParent::NPP_SetValue(GetNPP(), aVariable, aValue);
}

NPError
PluginAsyncSurrogate::NPP_NewStream(NPMIMEType aType, NPStream* aStream,
                                    NPBool aSeekable, uint16_t* aStype)
{
  mPendingNewStreamCalls.AppendElement(PendingNewStreamCall(aType, aStream,
                                                            aSeekable));
  if (aStype) {
    *aStype = nsPluginStreamListenerPeer::STREAM_TYPE_UNKNOWN;
  }
  return NPERR_NO_ERROR;
}

NPError
PluginAsyncSurrogate::NPP_SetWindow(NPWindow* aWindow)
{
  mWindow = aWindow;
  mAsyncSetWindow = false;
  return NPERR_NO_ERROR;
}

nsresult
PluginAsyncSurrogate::AsyncSetWindow(NPWindow* aWindow)
{
  mWindow = aWindow;
  mAsyncSetWindow = true;
  return NS_OK;
}

void
PluginAsyncSurrogate::NPP_Print(NPPrint* aPrintInfo)
{
  // Do nothing, we've got nothing to print right now
}

int16_t
PluginAsyncSurrogate::NPP_HandleEvent(void* event)
{
  // Drop the event -- the plugin isn't around to handle it
  return false;
}

int32_t
PluginAsyncSurrogate::NPP_WriteReady(NPStream* aStream)
{
  // We'll tell the browser to retry in a bit. Eventually NPP_WriteReady
  // will resolve to the plugin's NPP_WriteReady and this should all just work.
  return 0;
}

NPError
PluginAsyncSurrogate::NPP_DestroyStream(NPStream* aStream, NPReason aReason)
{
  for (uint32_t idx = 0, len = mPendingNewStreamCalls.Length(); idx < len; ++idx) {
    PendingNewStreamCall& curPendingCall = mPendingNewStreamCalls[idx];
    if (curPendingCall.mStream == aStream) {
      mPendingNewStreamCalls.RemoveElementAt(idx);
      break;
    }
  }
  return NPERR_NO_ERROR;
}

/* static */ NPError
PluginAsyncSurrogate::NPP_Destroy(NPP aInstance, NPSavedData** aSave)
{
  PluginAsyncSurrogate* rawSurrogate = Cast(aInstance);
  MOZ_ASSERT(rawSurrogate);
  PluginModuleParent* module = rawSurrogate->GetParent();
  if (module && !module->IsInitialized()) {
    // Take ownership of pdata's surrogate since we're going to release it
    nsRefPtr<PluginAsyncSurrogate> surrogate(dont_AddRef(rawSurrogate));
    aInstance->pdata = nullptr;
    // We haven't actually called NPP_New yet, so we should remove the
    // surrogate for this instance.
    bool removeOk = module->RemovePendingSurrogate(surrogate);
    MOZ_ASSERT(removeOk);
    if (!removeOk) {
      return NPERR_GENERIC_ERROR;
    }
    surrogate->mInitCancelled = true;
    return NPERR_NO_ERROR;
  }
  return rawSurrogate->NPP_Destroy(aSave);
}

/* static */ NPError
PluginAsyncSurrogate::NPP_GetValue(NPP aInstance, NPPVariable aVariable,
                                   void* aRetval)
{
  PluginAsyncSurrogate* surrogate = Cast(aInstance);
  MOZ_ASSERT(surrogate);
  return surrogate->NPP_GetValue(aVariable, aRetval);
}

/* static */ NPError
PluginAsyncSurrogate::NPP_SetValue(NPP aInstance, NPNVariable aVariable,
                                   void* aValue)
{
  PluginAsyncSurrogate* surrogate = Cast(aInstance);
  MOZ_ASSERT(surrogate);
  return surrogate->NPP_SetValue(aVariable, aValue);
}

/* static */ NPError
PluginAsyncSurrogate::NPP_NewStream(NPP aInstance, NPMIMEType aType,
                                    NPStream* aStream, NPBool aSeekable,
                                    uint16_t* aStype)
{
  PluginAsyncSurrogate* surrogate = Cast(aInstance);
  MOZ_ASSERT(surrogate);
  return surrogate->NPP_NewStream(aType, aStream, aSeekable, aStype);
}

/* static */ NPError
PluginAsyncSurrogate::NPP_SetWindow(NPP aInstance, NPWindow* aWindow)
{
  PluginAsyncSurrogate* surrogate = Cast(aInstance);
  MOZ_ASSERT(surrogate);
  return surrogate->NPP_SetWindow(aWindow);
}

/* static */ void
PluginAsyncSurrogate::NPP_Print(NPP aInstance, NPPrint* aPrintInfo)
{
  PluginAsyncSurrogate* surrogate = Cast(aInstance);
  MOZ_ASSERT(surrogate);
  surrogate->NPP_Print(aPrintInfo);
}

/* static */ int16_t
PluginAsyncSurrogate::NPP_HandleEvent(NPP aInstance, void* aEvent)
{
  PluginAsyncSurrogate* surrogate = Cast(aInstance);
  MOZ_ASSERT(surrogate);
  return surrogate->NPP_HandleEvent(aEvent);
}

/* static */ int32_t
PluginAsyncSurrogate::NPP_WriteReady(NPP aInstance, NPStream* aStream)
{
  PluginAsyncSurrogate* surrogate = Cast(aInstance);
  MOZ_ASSERT(surrogate);
  return surrogate->NPP_WriteReady(aStream);
}

/* static */ NPError
PluginAsyncSurrogate::NPP_DestroyStream(NPP aInstance,
                                        NPStream* aStream,
                                        NPReason aReason)
{
  PluginAsyncSurrogate* surrogate = Cast(aInstance);
  MOZ_ASSERT(surrogate);
  return surrogate->NPP_DestroyStream(aStream, aReason);
}

PluginAsyncSurrogate::PendingNewStreamCall::PendingNewStreamCall(
    NPMIMEType aType, NPStream* aStream, NPBool aSeekable)
  : mType(NullableString(aType))
  , mStream(aStream)
  , mSeekable(aSeekable)
{
}

/* static */ nsNPAPIPluginStreamListener*
PluginAsyncSurrogate::GetStreamListener(NPStream* aStream)
{
  nsNPAPIStreamWrapper* wrapper =
    reinterpret_cast<nsNPAPIStreamWrapper*>(aStream->ndata);
  if (!wrapper) {
    return nullptr;
  }
  return wrapper->GetStreamListener();
}

void
PluginAsyncSurrogate::DestroyAsyncStream(NPStream* aStream)
{
  MOZ_ASSERT(aStream);
  nsNPAPIPluginStreamListener* streamListener = GetStreamListener(aStream);
  MOZ_ASSERT(streamListener);
  // streamListener was suspended during async init. We must resume the stream
  // request prior to calling _destroystream for cleanup to work correctly.
  streamListener->ResumeRequest();
  if (!mInstance) {
    return;
  }
  parent::_destroystream(GetNPP(), aStream, NPRES_DONE);
}

/* static */ bool
PluginAsyncSurrogate::SetStreamType(NPStream* aStream, uint16_t aStreamType)
{
  nsNPAPIPluginStreamListener* streamListener = GetStreamListener(aStream);
  if (!streamListener) {
    return false;
  }
  return streamListener->SetStreamType(aStreamType);
}

void
PluginAsyncSurrogate::OnInstanceCreated(PluginInstanceParent* aInstance)
{
  if (!mDestroyPending) {
    // If NPP_Destroy has already been called then these streams have already
    // been cleaned up on the browser side and are no longer valid.
    for (uint32_t i = 0, len = mPendingNewStreamCalls.Length(); i < len; ++i) {
      PendingNewStreamCall& curPendingCall = mPendingNewStreamCalls[i];
      uint16_t streamType = NP_NORMAL;
      NPError curError = aInstance->NPP_NewStream(
                      const_cast<char*>(NullableStringGet(curPendingCall.mType)),
                      curPendingCall.mStream, curPendingCall.mSeekable,
                      &streamType);
      if (curError != NPERR_NO_ERROR) {
        // If we failed here then the send failed and we need to clean up
        DestroyAsyncStream(curPendingCall.mStream);
      }
    }
  }
  mPendingNewStreamCalls.Clear();
  mInstantiated = true;
}

/**
 * During asynchronous initialization it might be necessary to wait for the
 * plugin to complete its initialization. This typically occurs when the result
 * of a plugin call depends on the plugin being fully instantiated. For example,
 * if some JS calls into the plugin, the call must be executed synchronously to
 * preserve correctness.
 *
 * This function works by pumping the plugin's IPC channel for events until
 * initialization has completed.
 */
bool
PluginAsyncSurrogate::WaitForInit()
{
  if (mInitCancelled) {
    return false;
  }
  if (mAcceptCalls) {
    return true;
  }
  Telemetry::AutoTimer<Telemetry::BLOCKED_ON_PLUGINASYNCSURROGATE_WAITFORINIT_MS>
    timer(mParent->GetHistogramKey());
  bool result = false;
  MOZ_ASSERT(mParent);
  if (mParent->IsChrome()) {
    PluginProcessParent* process = static_cast<PluginModuleChromeParent*>(mParent)->Process();
    MOZ_ASSERT(process);
    process->SetCallRunnableImmediately(true);
    if (!process->WaitUntilConnected()) {
      return false;
    }
  }
  if (!mParent->WaitForIPCConnection()) {
    return false;
  }
  if (!mParent->IsChrome()) {
    // For e10s content processes, we need to spin the content channel until the
    // protocol bridging has occurred.
    dom::ContentChild* cp = dom::ContentChild::GetSingleton();
    mozilla::ipc::MessageChannel* contentChannel = cp->GetIPCChannel();
    MOZ_ASSERT(contentChannel);
    while (!mParent->mNPInitialized) {
      if (mParent->mShutdown) {
        // Since we are pumping the message channel for events, it may be
        // possible for module initialization to fail during this loop. We must
        // return false if this happens or else we'll be permanently stuck.
        return false;
      }
      result = contentChannel->WaitForIncomingMessage();
      if (!result) {
        return result;
      }
    }
  }
  mozilla::ipc::MessageChannel* channel = mParent->GetIPCChannel();
  MOZ_ASSERT(channel);
  while (!mAcceptCalls) {
    if (mInitCancelled) {
      // Since we are pumping the message channel for events, it may be
      // possible for plugin instantiation to fail during this loop. We must
      // return false if this happens or else we'll be permanently stuck.
      return false;
    }
    result = channel->WaitForIncomingMessage();
    if (!result) {
      break;
    }
  }
  return result;
}

void
PluginAsyncSurrogate::AsyncCallDeparting()
{
  ++mAsyncCallsInFlight;
  if (!mPluginDestructionGuard) {
    mPluginDestructionGuard = MakeUnique<PluginDestructionGuard>(this);
  }
}

void
PluginAsyncSurrogate::AsyncCallArriving()
{
  MOZ_ASSERT(mAsyncCallsInFlight > 0);
  if (--mAsyncCallsInFlight == 0) {
    mPluginDestructionGuard.reset(nullptr);
  }
}

void
PluginAsyncSurrogate::NotifyAsyncInitFailed()
{
  if (!mDestroyPending) {
    // Clean up any pending NewStream requests
    for (uint32_t i = 0, len = mPendingNewStreamCalls.Length(); i < len; ++i) {
      PendingNewStreamCall& curPendingCall = mPendingNewStreamCalls[i];
      DestroyAsyncStream(curPendingCall.mStream);
    }
  }
  mPendingNewStreamCalls.Clear();

  // Make sure that any WaitForInit calls on this surrogate will fail, or else
  // we'll be perma-blocked
  mInitCancelled = true;

  if (!mInstance) {
      return;
  }
  nsPluginInstanceOwner* owner = mInstance->GetOwner();
  if (owner) {
    owner->NotifyHostAsyncInitFailed();
  }
}

// static
NPObject*
PluginAsyncSurrogate::ScriptableAllocate(NPP aInstance, NPClass* aClass)
{
  PLUGIN_LOG_DEBUG_FUNCTION;
  if (aClass != GetClass()) {
    NS_ERROR("Huh?! Wrong class!");
    return nullptr;
  }

  return new AsyncNPObject(Cast(aInstance));
}

// static
void
PluginAsyncSurrogate::ScriptableInvalidate(NPObject* aObject)
{
  PLUGIN_LOG_DEBUG_FUNCTION;
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return;
  }

  AsyncNPObject* object = static_cast<AsyncNPObject*>(aObject);
  if (!object->mSurrogate->WaitForInit()) {
    return;
  }
  NPObject* realObject = object->GetRealObject();
  if (!realObject) {
    return;
  }
  realObject->_class->invalidate(realObject);
}

// static
void
PluginAsyncSurrogate::ScriptableDeallocate(NPObject* aObject)
{
  PLUGIN_LOG_DEBUG_FUNCTION;
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return;
  }

  AsyncNPObject* object = static_cast<AsyncNPObject*>(aObject);
  delete object;
}

// static
bool
PluginAsyncSurrogate::ScriptableHasMethod(NPObject* aObject,
                                                  NPIdentifier aName)
{
  PLUGIN_LOG_DEBUG_FUNCTION;
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  RecursionGuard guard;
  if (guard.IsRecursive()) {
    return false;
  }

  AsyncNPObject* object = static_cast<AsyncNPObject*>(aObject);
  MOZ_ASSERT(object);
  bool checkPluginObject = !object->mSurrogate->mInstantiated &&
                           !object->mSurrogate->mAcceptCalls;

  if (!object->mSurrogate->WaitForInit()) {
    return false;
  }
  NPObject* realObject = object->GetRealObject();
  if (!realObject) {
    return false;
  }
  bool result = realObject->_class->hasMethod(realObject, aName);
  if (!result && checkPluginObject) {
    // We may be calling into this object because properties in the WebIDL
    // object hadn't been set yet. Now that we're further along in
    // initialization, we should try again.
    const NPNetscapeFuncs* npn = object->mSurrogate->mParent->GetNetscapeFuncs();
    NPObject* pluginObject = nullptr;
    NPError nperror = npn->getvalue(object->mSurrogate->GetNPP(),
                                    NPNVPluginElementNPObject,
                                    (void*)&pluginObject);
    if (nperror == NPERR_NO_ERROR) {
      NPPAutoPusher nppPusher(object->mSurrogate->GetNPP());
      result = pluginObject->_class->hasMethod(pluginObject, aName);
      npn->releaseobject(pluginObject);
      NPUTF8* idstr = npn->utf8fromidentifier(aName);
      npn->memfree(idstr);
    }
  }
  return result;
}

bool
PluginAsyncSurrogate::GetPropertyHelper(NPObject* aObject, NPIdentifier aName,
                                        bool* aHasProperty, bool* aHasMethod,
                                        NPVariant* aResult)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  if (!aObject) {
    return false;
  }

  RecursionGuard guard;
  if (guard.IsRecursive()) {
    return false;
  }

  if (!WaitForInit()) {
    return false;
  }

  AsyncNPObject* object = static_cast<AsyncNPObject*>(aObject);
  NPObject* realObject = object->GetRealObject();
  if (!realObject) {
    return false;
  }
  if (realObject->_class != PluginScriptableObjectParent::GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  PluginScriptableObjectParent* actor =
    static_cast<ParentNPObject*>(realObject)->parent;
  if (!actor) {
    return false;
  }
  bool success = actor->GetPropertyHelper(aName, aHasProperty, aHasMethod, aResult);
  if (!success) {
    const NPNetscapeFuncs* npn = mParent->GetNetscapeFuncs();
    NPObject* pluginObject = nullptr;
    NPError nperror = npn->getvalue(GetNPP(), NPNVPluginElementNPObject,
                                    (void*)&pluginObject);
    if (nperror == NPERR_NO_ERROR) {
      NPPAutoPusher nppPusher(GetNPP());
      bool hasProperty = nsJSObjWrapper::HasOwnProperty(pluginObject, aName);
      NPUTF8* idstr = npn->utf8fromidentifier(aName);
      npn->memfree(idstr);
      bool hasMethod = false;
      if (hasProperty) {
        hasMethod = pluginObject->_class->hasMethod(pluginObject, aName);
        success = pluginObject->_class->getProperty(pluginObject, aName, aResult);
        idstr = npn->utf8fromidentifier(aName);
        npn->memfree(idstr);
      }
      *aHasProperty = hasProperty;
      *aHasMethod = hasMethod;
      npn->releaseobject(pluginObject);
    }
  }
  return success;
}

// static
bool
PluginAsyncSurrogate::ScriptableInvoke(NPObject* aObject,
                                               NPIdentifier aName,
                                               const NPVariant* aArgs,
                                               uint32_t aArgCount,
                                               NPVariant* aResult)
{
  PLUGIN_LOG_DEBUG_FUNCTION;
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  AsyncNPObject* object = static_cast<AsyncNPObject*>(aObject);
  if (!object->mSurrogate->WaitForInit()) {
    return false;
  }
  NPObject* realObject = object->GetRealObject();
  if (!realObject) {
    return false;
  }
  return realObject->_class->invoke(realObject, aName, aArgs, aArgCount, aResult);
}

// static
bool
PluginAsyncSurrogate::ScriptableInvokeDefault(NPObject* aObject,
                                                      const NPVariant* aArgs,
                                                      uint32_t aArgCount,
                                                      NPVariant* aResult)
{
  PLUGIN_LOG_DEBUG_FUNCTION;
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  AsyncNPObject* object = static_cast<AsyncNPObject*>(aObject);
  if (!object->mSurrogate->WaitForInit()) {
    return false;
  }
  NPObject* realObject = object->GetRealObject();
  if (!realObject) {
    return false;
  }
  return realObject->_class->invokeDefault(realObject, aArgs, aArgCount, aResult);
}

// static
bool
PluginAsyncSurrogate::ScriptableHasProperty(NPObject* aObject,
                                                    NPIdentifier aName)
{
  PLUGIN_LOG_DEBUG_FUNCTION;
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  RecursionGuard guard;
  if (guard.IsRecursive()) {
    return false;
  }

  AsyncNPObject* object = static_cast<AsyncNPObject*>(aObject);
  MOZ_ASSERT(object);
  bool checkPluginObject = !object->mSurrogate->mInstantiated &&
                           !object->mSurrogate->mAcceptCalls;

  if (!object->mSurrogate->WaitForInit()) {
    return false;
  }
  NPObject* realObject = object->GetRealObject();
  if (!realObject) {
    return false;
  }
  bool result = realObject->_class->hasProperty(realObject, aName);
  const NPNetscapeFuncs* npn = object->mSurrogate->mParent->GetNetscapeFuncs();
  NPUTF8* idstr = npn->utf8fromidentifier(aName);
  npn->memfree(idstr);
  if (!result && checkPluginObject) {
    // We may be calling into this object because properties in the WebIDL
    // object hadn't been set yet. Now that we're further along in
    // initialization, we should try again.
    NPObject* pluginObject = nullptr;
    NPError nperror = npn->getvalue(object->mSurrogate->GetNPP(),
                                    NPNVPluginElementNPObject,
                                    (void*)&pluginObject);
    if (nperror == NPERR_NO_ERROR) {
      NPPAutoPusher nppPusher(object->mSurrogate->GetNPP());
      result = nsJSObjWrapper::HasOwnProperty(pluginObject, aName);
      npn->releaseobject(pluginObject);
      idstr = npn->utf8fromidentifier(aName);
      npn->memfree(idstr);
    }
  }
  return result;
}

// static
bool
PluginAsyncSurrogate::ScriptableGetProperty(NPObject* aObject,
                                                    NPIdentifier aName,
                                                    NPVariant* aResult)
{
  PLUGIN_LOG_DEBUG_FUNCTION;
  // See GetPropertyHelper below.
  NS_NOTREACHED("Shouldn't ever call this directly!");
  return false;
}

// static
bool
PluginAsyncSurrogate::ScriptableSetProperty(NPObject* aObject,
                                                    NPIdentifier aName,
                                                    const NPVariant* aValue)
{
  PLUGIN_LOG_DEBUG_FUNCTION;
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  AsyncNPObject* object = static_cast<AsyncNPObject*>(aObject);
  if (!object->mSurrogate->WaitForInit()) {
    return false;
  }
  NPObject* realObject = object->GetRealObject();
  if (!realObject) {
    return false;
  }
  return realObject->_class->setProperty(realObject, aName, aValue);
}

// static
bool
PluginAsyncSurrogate::ScriptableRemoveProperty(NPObject* aObject,
                                                       NPIdentifier aName)
{
  PLUGIN_LOG_DEBUG_FUNCTION;
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  AsyncNPObject* object = static_cast<AsyncNPObject*>(aObject);
  if (!object->mSurrogate->WaitForInit()) {
    return false;
  }
  NPObject* realObject = object->GetRealObject();
  if (!realObject) {
    return false;
  }
  return realObject->_class->removeProperty(realObject, aName);
}

// static
bool
PluginAsyncSurrogate::ScriptableEnumerate(NPObject* aObject,
                                                  NPIdentifier** aIdentifiers,
                                                  uint32_t* aCount)
{
  PLUGIN_LOG_DEBUG_FUNCTION;
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  AsyncNPObject* object = static_cast<AsyncNPObject*>(aObject);
  if (!object->mSurrogate->WaitForInit()) {
    return false;
  }
  NPObject* realObject = object->GetRealObject();
  if (!realObject) {
    return false;
  }
  return realObject->_class->enumerate(realObject, aIdentifiers, aCount);
}

// static
bool
PluginAsyncSurrogate::ScriptableConstruct(NPObject* aObject,
                                                  const NPVariant* aArgs,
                                                  uint32_t aArgCount,
                                                  NPVariant* aResult)
{
  PLUGIN_LOG_DEBUG_FUNCTION;
  if (aObject->_class != GetClass()) {
    NS_ERROR("Don't know what kind of object this is!");
    return false;
  }

  AsyncNPObject* object = static_cast<AsyncNPObject*>(aObject);
  if (!object->mSurrogate->WaitForInit()) {
    return false;
  }
  NPObject* realObject = object->GetRealObject();
  if (!realObject) {
    return false;
  }
  return realObject->_class->construct(realObject, aArgs, aArgCount, aResult);
}

const NPClass PluginAsyncSurrogate::sNPClass = {
  NP_CLASS_STRUCT_VERSION,
  PluginAsyncSurrogate::ScriptableAllocate,
  PluginAsyncSurrogate::ScriptableDeallocate,
  PluginAsyncSurrogate::ScriptableInvalidate,
  PluginAsyncSurrogate::ScriptableHasMethod,
  PluginAsyncSurrogate::ScriptableInvoke,
  PluginAsyncSurrogate::ScriptableInvokeDefault,
  PluginAsyncSurrogate::ScriptableHasProperty,
  PluginAsyncSurrogate::ScriptableGetProperty,
  PluginAsyncSurrogate::ScriptableSetProperty,
  PluginAsyncSurrogate::ScriptableRemoveProperty,
  PluginAsyncSurrogate::ScriptableEnumerate,
  PluginAsyncSurrogate::ScriptableConstruct
};

PushSurrogateAcceptCalls::PushSurrogateAcceptCalls(PluginInstanceParent* aInstance)
  : mSurrogate(nullptr)
  , mPrevAcceptCallsState(false)
{
  MOZ_ASSERT(aInstance);
  mSurrogate = aInstance->GetAsyncSurrogate();
  if (mSurrogate) {
    mPrevAcceptCallsState = mSurrogate->SetAcceptingCalls(true);
  }
}

PushSurrogateAcceptCalls::~PushSurrogateAcceptCalls()
{
  if (mSurrogate) {
    mSurrogate->SetAcceptingCalls(mPrevAcceptCallsState);
  }
}

} // namespace plugins
} // namespace mozilla
