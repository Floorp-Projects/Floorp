/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_Realm_h
#define js_Realm_h

#include "jspubtd.h"
#include "js/GCPolicyAPI.h"
#include "js/TypeDecls.h"  // forward-declaration of JS::Realm

namespace js {
namespace gc {
JS_PUBLIC_API void TraceRealm(JSTracer* trc, JS::Realm* realm,
                              const char* name);
JS_PUBLIC_API bool RealmNeedsSweep(JS::Realm* realm);
}  // namespace gc
}  // namespace js

namespace JS {

// Each Realm holds a strong reference to its GlobalObject, and vice versa.
template <>
struct GCPolicy<Realm*> : public NonGCPointerPolicy<Realm*> {
  static void trace(JSTracer* trc, Realm** vp, const char* name) {
    if (*vp) {
      ::js::gc::TraceRealm(trc, *vp, name);
    }
  }
  static bool needsSweep(Realm** vp) {
    return *vp && ::js::gc::RealmNeedsSweep(*vp);
  }
};

// Get the current realm, if any. The ECMAScript spec calls this "the current
// Realm Record".
extern JS_PUBLIC_API Realm* GetCurrentRealmOrNull(JSContext* cx);

namespace shadow {

class Realm {
 protected:
  JS::Compartment* compartment_;

  explicit Realm(JS::Compartment* comp) : compartment_(comp) {}

 public:
  JS::Compartment* compartment() { return compartment_; }
  static shadow::Realm* get(JS::Realm* realm) {
    return reinterpret_cast<shadow::Realm*>(realm);
  }
};

};  // namespace shadow

// Return the compartment that contains a given realm.
inline JS::Compartment* GetCompartmentForRealm(Realm* realm) {
  return shadow::Realm::get(realm)->compartment();
}

// Return an object's realm. All objects except cross-compartment wrappers are
// created in a particular realm, which never changes. Returns null if obj is
// a cross-compartment wrapper.
extern JS_PUBLIC_API Realm* GetObjectRealmOrNull(JSObject* obj);

// Get the value of the "private data" internal field of the given Realm.
// This field is initially null and is set using SetRealmPrivate.
// It's a pointer to embeddding-specific data that SpiderMonkey never uses.
extern JS_PUBLIC_API void* GetRealmPrivate(Realm* realm);

// Set the "private data" internal field of the given Realm.
extern JS_PUBLIC_API void SetRealmPrivate(Realm* realm, void* data);

typedef void (*DestroyRealmCallback)(JSFreeOp* fop, Realm* realm);

// Set the callback SpiderMonkey calls just before garbage-collecting a realm.
// Embeddings can use this callback to free private data associated with the
// realm via SetRealmPrivate.
//
// By the time this is called, the global object for the realm has already been
// collected.
extern JS_PUBLIC_API void SetDestroyRealmCallback(
    JSContext* cx, DestroyRealmCallback callback);

typedef void (*RealmNameCallback)(JSContext* cx, Handle<Realm*> realm,
                                  char* buf, size_t bufsize);

// Set the callback SpiderMonkey calls to get the name of a realm, for
// diagnostic output.
extern JS_PUBLIC_API void SetRealmNameCallback(JSContext* cx,
                                               RealmNameCallback callback);

// Get the global object for the given realm. This only returns nullptr during
// GC, between collecting the global object and destroying the Realm.
extern JS_PUBLIC_API JSObject* GetRealmGlobalOrNull(Handle<Realm*> realm);

// Initialize standard JS class constructors, prototypes, and any top-level
// functions and constants associated with the standard classes (e.g. isNaN
// for Number).
extern JS_PUBLIC_API bool InitRealmStandardClasses(JSContext* cx);

/*
 * Ways to get various per-Realm objects. All the getters declared below operate
 * on the JSContext's current Realm.
 */

extern JS_PUBLIC_API JSObject* GetRealmObjectPrototype(JSContext* cx);

extern JS_PUBLIC_API JSObject* GetRealmFunctionPrototype(JSContext* cx);

extern JS_PUBLIC_API JSObject* GetRealmArrayPrototype(JSContext* cx);

extern JS_PUBLIC_API JSObject* GetRealmErrorPrototype(JSContext* cx);

extern JS_PUBLIC_API JSObject* GetRealmIteratorPrototype(JSContext* cx);

// Implements https://tc39.github.io/ecma262/#sec-getfunctionrealm
// 7.3.22 GetFunctionRealm ( obj )
//
// WARNING: may return a realm in a different compartment!
//
// Will throw an exception and return nullptr when a security wrapper or revoked
// proxy is encountered.
extern JS_PUBLIC_API Realm* GetFunctionRealm(JSContext* cx,
                                             HandleObject objArg);

}  // namespace JS

#endif  // js_Realm_h
