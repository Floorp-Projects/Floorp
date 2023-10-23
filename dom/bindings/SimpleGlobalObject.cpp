/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SimpleGlobalObject.h"

#include "jsapi.h"
#include "js/Class.h"
#include "js/Object.h"  // JS::GetClass, JS::GetObjectISupports, JS::SetObjectISupports

#include "nsJSPrincipals.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"

#include "xpcprivate.h"

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/NullPrincipal.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(SimpleGlobalObject)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SimpleGlobalObject)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->UnlinkObjectsInGlobal();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(SimpleGlobalObject)
  tmp->TraverseObjectsInGlobal(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(SimpleGlobalObject)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SimpleGlobalObject)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SimpleGlobalObject)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
NS_INTERFACE_MAP_END

static SimpleGlobalObject* GetSimpleGlobal(JSObject* global);

static void SimpleGlobal_finalize(JS::GCContext* gcx, JSObject* obj) {
  SimpleGlobalObject* globalObject = GetSimpleGlobal(obj);
  if (globalObject) {
    globalObject->ClearWrapper(obj);
    NS_RELEASE(globalObject);
  }
}

static size_t SimpleGlobal_moved(JSObject* obj, JSObject* old) {
  SimpleGlobalObject* globalObject = GetSimpleGlobal(obj);
  if (globalObject) {
    globalObject->UpdateWrapper(obj, old);
  }
  return 0;
}

static const JSClassOps SimpleGlobalClassOps = {
    nullptr,
    nullptr,
    nullptr,
    JS_NewEnumerateStandardClasses,
    JS_ResolveStandardClass,
    JS_MayResolveStandardClass,
    SimpleGlobal_finalize,
    nullptr,
    nullptr,
    JS_GlobalObjectTraceHook,
};

static const js::ClassExtension SimpleGlobalClassExtension = {
    SimpleGlobal_moved};

static_assert(JSCLASS_GLOBAL_APPLICATION_SLOTS > 0,
              "Need at least one slot for JSCLASS_SLOT0_IS_NSISUPPORTS");

const JSClass SimpleGlobalClass = {"",
                                   JSCLASS_GLOBAL_FLAGS |
                                       JSCLASS_SLOT0_IS_NSISUPPORTS |
                                       JSCLASS_FOREGROUND_FINALIZE,
                                   &SimpleGlobalClassOps,
                                   JS_NULL_CLASS_SPEC,
                                   &SimpleGlobalClassExtension,
                                   JS_NULL_OBJECT_OPS};

static SimpleGlobalObject* GetSimpleGlobal(JSObject* global) {
  MOZ_ASSERT(JS::GetClass(global) == &SimpleGlobalClass);

  return JS::GetObjectISupports<SimpleGlobalObject>(global);
}

// static
JSObject* SimpleGlobalObject::Create(GlobalType globalType,
                                     JS::Handle<JS::Value> proto) {
  // We can't root our return value with our AutoJSAPI because the rooting
  // analysis thinks ~AutoJSAPI can GC.  So we need to root in a scope outside
  // the lifetime of the AutoJSAPI.
  JS::Rooted<JSObject*> global(RootingCx());

  {  // Scope to ensure the AutoJSAPI destructor runs before we end up returning
    AutoJSAPI jsapi;
    jsapi.Init();
    JSContext* cx = jsapi.cx();

    JS::RealmOptions options;
    options.creationOptions()
        .setInvisibleToDebugger(true)
        // Put our SimpleGlobalObjects in the system zone, so we won't create
        // lots of zones for what are probably very short-lived
        // compartments.  This should help them be GCed quicker and take up
        // less memory before they're GCed.
        .setNewCompartmentInSystemZone();

    if (NS_IsMainThread()) {
      nsCOMPtr<nsIPrincipal> principal =
          NullPrincipal::CreateWithoutOriginAttributes();
      options.creationOptions().setTrace(xpc::TraceXPCGlobal);
      global = xpc::CreateGlobalObject(cx, &SimpleGlobalClass,
                                       nsJSPrincipals::get(principal), options);
    } else {
      global = JS_NewGlobalObject(cx, &SimpleGlobalClass, nullptr,
                                  JS::DontFireOnNewGlobalHook, options);
    }

    if (!global) {
      jsapi.ClearException();
      return nullptr;
    }

    JSAutoRealm ar(cx, global);

    // It's important to create the nsIGlobalObject for our new global before we
    // start trying to wrap things like the prototype into its compartment,
    // because the wrap operation relies on the global having its
    // nsIGlobalObject already.
    RefPtr<SimpleGlobalObject> globalObject =
        new SimpleGlobalObject(global, globalType);

    // Pass on ownership of globalObject to |global|.
    JS::SetObjectISupports(global, globalObject.forget().take());

    if (proto.isObjectOrNull()) {
      JS::Rooted<JSObject*> protoObj(cx, proto.toObjectOrNull());
      if (!JS_WrapObject(cx, &protoObj)) {
        jsapi.ClearException();
        return nullptr;
      }

      if (!JS_SetPrototype(cx, global, protoObj)) {
        jsapi.ClearException();
        return nullptr;
      }
    } else if (!proto.isUndefined()) {
      // Bogus proto.
      return nullptr;
    }

    JS_FireOnNewGlobalObject(cx, global);
  }

  return global;
}

// static
SimpleGlobalObject::GlobalType SimpleGlobalObject::SimpleGlobalType(
    JSObject* obj) {
  if (JS::GetClass(obj) != &SimpleGlobalClass) {
    return SimpleGlobalObject::GlobalType::NotSimpleGlobal;
  }

  SimpleGlobalObject* globalObject = GetSimpleGlobal(obj);
  return globalObject->Type();
}

}  // namespace mozilla::dom
