/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJSNPRuntime_h_
#define nsJSNPRuntime_h_

#include "nscore.h"
#include "npapi.h"
#include "npruntime.h"
#include "PLDHashTable.h"
#include "js/RootingAPI.h"

class nsJSNPRuntime
{
public:
  static void OnPluginDestroy(NPP npp);
  static void OnPluginDestroyPending(NPP npp);
};

class nsJSObjWrapperKey
{
public:
  nsJSObjWrapperKey(JSObject *obj, NPP npp)
    : mJSObj(obj), mNpp(npp)
  {
  }

  bool operator==(const nsJSObjWrapperKey& other) const {
    return mJSObj == other.mJSObj && mNpp == other.mNpp;
  }
  bool operator!=(const nsJSObjWrapperKey& other) const {
    return !(*this == other);
  }

  void trace(JSTracer* trc) {
      JS::TraceEdge(trc, &mJSObj, "nsJSObjWrapperKey");
  }

  nsJSObjWrapperKey(const nsJSObjWrapperKey& other)
    : mJSObj(other.mJSObj),
      mNpp(other.mNpp)
  {}
  void operator=(const nsJSObjWrapperKey& other) {
    mJSObj = other.mJSObj;
    mNpp = other.mNpp;
  }

  JS::Heap<JSObject*> mJSObj;
  NPP mNpp;
};

class nsJSObjWrapper : public NPObject
{
public:
  JS::Heap<JSObject *> mJSObj;
  const NPP mNpp;
  bool mDestroyPending;

  static NPObject* GetNewOrUsed(NPP npp, JS::Handle<JSObject*> obj);
  static bool HasOwnProperty(NPObject* npobj, NPIdentifier npid);

  void trace(JSTracer* trc) {
      JS::TraceEdge(trc, &mJSObj, "nsJSObjWrapper");
  }

protected:
  explicit nsJSObjWrapper(NPP npp);
  ~nsJSObjWrapper();

  static NPObject * NP_Allocate(NPP npp, NPClass *aClass);
  static void NP_Deallocate(NPObject *obj);
  static void NP_Invalidate(NPObject *obj);
  static bool NP_HasMethod(NPObject *, NPIdentifier identifier);
  static bool NP_Invoke(NPObject *obj, NPIdentifier method,
                        const NPVariant *args, uint32_t argCount,
                        NPVariant *result);
  static bool NP_InvokeDefault(NPObject *obj, const NPVariant *args,
                               uint32_t argCount, NPVariant *result);
  static bool NP_HasProperty(NPObject * obj, NPIdentifier property);
  static bool NP_GetProperty(NPObject *obj, NPIdentifier property,
                             NPVariant *result);
  static bool NP_SetProperty(NPObject *obj, NPIdentifier property,
                             const NPVariant *value);
  static bool NP_RemoveProperty(NPObject *obj, NPIdentifier property);
  static bool NP_Enumerate(NPObject *npobj, NPIdentifier **identifier,
                           uint32_t *count);
  static bool NP_Construct(NPObject *obj, const NPVariant *args,
                           uint32_t argCount, NPVariant *result);

public:
  static NPClass sJSObjWrapperNPClass;
};

class nsNPObjWrapper
{
public:
  static bool IsWrapper(JSObject *obj);
  static void OnDestroy(NPObject *npobj);
  static JSObject *GetNewOrUsed(NPP npp, JSContext *cx, NPObject *npobj);
};

bool
JSValToNPVariant(NPP npp, JSContext *cx, JS::Value val, NPVariant *variant);


#endif // nsJSNPRuntime_h_
