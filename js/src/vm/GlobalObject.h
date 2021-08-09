/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_GlobalObject_h
#define vm_GlobalObject_h

#include "mozilla/Assertions.h"
#include "mozilla/EnumeratedArray.h"

#include <stdint.h>
#include <type_traits>

#include "jsapi.h"
#include "jsexn.h"
#include "jsfriendapi.h"
#include "jspubtd.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "gc/AllocKind.h"
#include "gc/Rooting.h"
#include "js/CallArgs.h"
#include "js/Class.h"
#include "js/ErrorReport.h"
#include "js/PropertyDescriptor.h"
#include "js/RootingAPI.h"
#include "js/ScalarType.h"  // js::Scalar::Type
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/JSContext.h"
#include "vm/JSFunction.h"
#include "vm/JSObject.h"
#include "vm/NativeObject.h"
#include "vm/Realm.h"
#include "vm/Runtime.h"
#include "vm/Shape.h"
#include "vm/StringType.h"

struct JSFunctionSpec;
class JSJitInfo;
struct JSPrincipals;
struct JSPropertySpec;

namespace JS {
class JS_PUBLIC_API RealmOptions;
};

namespace js {

class GlobalScope;
class GlobalLexicalEnvironmentObject;
class PlainObject;
class RegExpStatics;
class RegExpStaticsObject;

// Data attached to a GlobalObject. This is freed when clearing the Realm's
// global_ only because this way we don't need to add a finalizer to all
// GlobalObject JSClasses.
class GlobalObjectData {
  friend class js::GlobalObject;

  GlobalObjectData(const GlobalObjectData&) = delete;
  void operator=(const GlobalObjectData&) = delete;

 public:
  GlobalObjectData() = default;

  // The original values for built-in constructors (with their prototype
  // objects) based on JSProtoKey.
  //
  // This is necessary to implement spec language speaking in terms of "the
  // original Array prototype object", or "as if by the expression new Array()"
  // referring to the original Array constructor. The actual (writable and even
  // deletable) Object, Array, &c. properties are not stored here.
  struct ConstructorWithProto {
    HeapPtr<JSObject*> constructor;
    HeapPtr<JSObject*> prototype;
  };
  using CtorArray =
      mozilla::EnumeratedArray<JSProtoKey, JSProto_LIMIT, ConstructorWithProto>;
  CtorArray builtinConstructors;

  // Built-in prototypes for this global. Note that this is different from the
  // set of built-in constructors/prototypes based on JSProtoKey.
  enum class ProtoKind {
    IteratorProto,
    ArrayIteratorProto,
    StringIteratorProto,
    RegExpStringIteratorProto,
    GeneratorObjectProto,
    AsyncIteratorProto,
    AsyncFromSyncIteratorProto,
    AsyncGeneratorProto,
    MapIteratorProto,
    SetIteratorProto,
    WrapForValidIteratorProto,
    IteratorHelperProto,
    AsyncIteratorHelperProto,
    ModuleProto,
    ImportEntryProto,
    ExportEntryProto,
    RequestedModuleProto,
    ModuleRequestProto,

    Limit
  };
  using ProtoArray =
      mozilla::EnumeratedArray<ProtoKind, ProtoKind::Limit, HeapPtr<JSObject*>>;
  ProtoArray builtinProtos;

  HeapPtr<GlobalScope*> emptyGlobalScope;

  // Global state for regular expressions.
  HeapPtr<RegExpStaticsObject*> regExpStatics;

  // Functions and other top-level values for self-hosted code.
  HeapPtr<NativeObject*> intrinsicsHolder;

  // Cache used to optimize certain for-of operations.
  HeapPtr<NativeObject*> forOfPICChain;

  // List of source URLs for this realm. This is used by the debugger.
  HeapPtr<ArrayObject*> sourceURLsHolder;

  // Realm-specific object that can be used as key in WeakMaps.
  HeapPtr<PlainObject*> realmKeyObject;

  // The unique %ThrowTypeError% function for this global.
  HeapPtr<JSFunction*> throwTypeError;

  // The unique %eval% function (for indirect eval) for this global.
  HeapPtr<JSFunction*> eval;

  // Cached shape for new arrays with Array.prototype as prototype.
  HeapPtr<Shape*> arrayShape;

  // Whether the |globalThis| property has been resolved on the global object.
  bool globalThisResolved = false;

  void trace(JSTracer* trc);
};

class GlobalObject : public NativeObject {
  enum : unsigned {
    GLOBAL_DATA_SLOT = JSCLASS_GLOBAL_APPLICATION_SLOTS,
    WINDOW_PROXY,

    // Total reserved-slot count for global objects.
    RESERVED_SLOTS
  };

  // The slot count must be in the public API for JSCLASS_GLOBAL_FLAGS, and
  // we won't expose GlobalObject, so just assert that the two values are
  // synchronized.
  static_assert(JSCLASS_GLOBAL_SLOT_COUNT == RESERVED_SLOTS,
                "global object slot counts are inconsistent");

  // Ensure GlobalObjectData is only one dereference away.
  static_assert(GLOBAL_DATA_SLOT < MAX_FIXED_SLOTS,
                "GlobalObjectData should be stored in a fixed slot for "
                "performance reasons");

  using ProtoKind = GlobalObjectData::ProtoKind;

  GlobalObjectData* maybeData() {
    Value v = getReservedSlot(GLOBAL_DATA_SLOT);
    return static_cast<GlobalObjectData*>(v.toPrivate());
  }
  const GlobalObjectData* maybeData() const {
    Value v = getReservedSlot(GLOBAL_DATA_SLOT);
    return static_cast<const GlobalObjectData*>(v.toPrivate());
  }

  GlobalObjectData& data() { return *maybeData(); }
  const GlobalObjectData& data() const { return *maybeData(); }

  void initBuiltinProto(ProtoKind kind, JSObject* proto) {
    MOZ_ASSERT(proto);
    data().builtinProtos[kind].init(proto);
  }
  bool hasBuiltinProto(ProtoKind kind) const {
    return bool(data().builtinProtos[kind]);
  }
  JSObject* maybeBuiltinProto(ProtoKind kind) const {
    return data().builtinProtos[kind];
  }
  JSObject& getBuiltinProto(ProtoKind kind) const {
    MOZ_ASSERT(hasBuiltinProto(kind));
    return *data().builtinProtos[kind];
  }

 public:
  GlobalLexicalEnvironmentObject& lexicalEnvironment() const;
  GlobalScope& emptyGlobalScope() const;

  void traceData(JSTracer* trc) { data().trace(trc); }
  void releaseData(JSFreeOp* fop);

  size_t sizeOfData(mozilla::MallocSizeOf mallocSizeOf) const {
    return mallocSizeOf(maybeData());
  }

  void setOriginalEval(JSFunction* evalFun) {
    MOZ_ASSERT(!data().eval);
    data().eval.init(evalFun);
  }

  bool hasConstructor(JSProtoKey key) const {
    return bool(data().builtinConstructors[key].constructor);
  }
  JSObject& getConstructor(JSProtoKey key) const {
    MOZ_ASSERT(hasConstructor(key));
    return *maybeGetConstructor(key);
  }

  static bool skipDeselectedConstructor(JSContext* cx, JSProtoKey key);
  static bool initBuiltinConstructor(JSContext* cx,
                                     Handle<GlobalObject*> global,
                                     JSProtoKey key, HandleObject ctor,
                                     HandleObject proto);

 private:
  enum class IfClassIsDisabled { DoNothing, Throw };

  static bool resolveConstructor(JSContext* cx, Handle<GlobalObject*> global,
                                 JSProtoKey key, IfClassIsDisabled mode);

 public:
  static bool ensureConstructor(JSContext* cx, Handle<GlobalObject*> global,
                                JSProtoKey key) {
    if (global->isStandardClassResolved(key)) {
      return true;
    }
    return resolveConstructor(cx, global, key, IfClassIsDisabled::Throw);
  }

  static JSObject* getOrCreateConstructor(JSContext* cx, JSProtoKey key) {
    MOZ_ASSERT(key != JSProto_Null);
    Handle<GlobalObject*> global = cx->global();
    if (!GlobalObject::ensureConstructor(cx, global, key)) {
      return nullptr;
    }
    return &global->getConstructor(key);
  }

  static JSObject* getOrCreatePrototype(JSContext* cx, JSProtoKey key) {
    MOZ_ASSERT(key != JSProto_Null);
    Handle<GlobalObject*> global = cx->global();
    if (!GlobalObject::ensureConstructor(cx, global, key)) {
      return nullptr;
    }
    return &global->getPrototype(key);
  }

  JSObject* maybeGetConstructor(JSProtoKey protoKey) const {
    MOZ_ASSERT(JSProto_Null < protoKey);
    MOZ_ASSERT(protoKey < JSProto_LIMIT);
    return data().builtinConstructors[protoKey].constructor;
  }

  JSObject* maybeGetPrototype(JSProtoKey protoKey) const {
    MOZ_ASSERT(JSProto_Null < protoKey);
    MOZ_ASSERT(protoKey < JSProto_LIMIT);
    return data().builtinConstructors[protoKey].prototype;
  }

  static bool maybeResolveGlobalThis(JSContext* cx,
                                     Handle<GlobalObject*> global,
                                     bool* resolved);

  void setConstructor(JSProtoKey key, JSObject* obj) {
    MOZ_ASSERT(obj);
    data().builtinConstructors[key].constructor = obj;
  }

  bool hasPrototype(JSProtoKey key) const {
    return bool(data().builtinConstructors[key].prototype);
  }
  JSObject& getPrototype(JSProtoKey key) const {
    MOZ_ASSERT(hasPrototype(key));
    return *maybeGetPrototype(key);
  }

  void setPrototype(JSProtoKey key, JSObject* obj) {
    MOZ_ASSERT(obj);
    data().builtinConstructors[key].prototype = obj;
  }

  /*
   * Lazy standard classes need a way to indicate they have been initialized.
   * Otherwise, when we delete them, we might accidentally recreate them via
   * a lazy initialization. We use the presence of an object in the constructor
   * array to indicate that they've been initialized.
   *
   * Note: A few builtin objects, like JSON and Math, are not constructors,
   * so getConstructor is a bit of a misnomer.
   */
  bool isStandardClassResolved(JSProtoKey key) const {
    return hasConstructor(key);
  }

 private:
  bool classIsInitialized(JSProtoKey key) const {
    bool inited = hasConstructor(key);
    MOZ_ASSERT(inited == hasPrototype(key));
    return inited;
  }

  bool functionObjectClassesInitialized() const {
    bool inited = classIsInitialized(JSProto_Function);
    MOZ_ASSERT(inited == classIsInitialized(JSProto_Object));
    return inited;
  }

  // Disallow use of unqualified JSObject::create in GlobalObject.
  static GlobalObject* create(...) = delete;

  friend struct ::JSRuntime;
  static GlobalObject* createInternal(JSContext* cx, const JSClass* clasp);

 public:
  static GlobalObject* new_(JSContext* cx, const JSClass* clasp,
                            JSPrincipals* principals,
                            JS::OnNewGlobalHookOption hookOption,
                            const JS::RealmOptions& options);

  /*
   * Create a constructor function with the specified name and length using
   * ctor, a method which creates objects with the given class.
   */
  static JSFunction* createConstructor(
      JSContext* cx, JSNative ctor, JSAtom* name, unsigned length,
      gc::AllocKind kind = gc::AllocKind::FUNCTION,
      const JSJitInfo* jitInfo = nullptr);

  /*
   * Create an object to serve as [[Prototype]] for instances of the given
   * class, using |Object.prototype| as its [[Prototype]].  Users creating
   * prototype objects with particular internal structure (e.g. reserved
   * slots guaranteed to contain values of particular types) must immediately
   * complete the minimal initialization to make the returned object safe to
   * touch.
   */
  static NativeObject* createBlankPrototype(JSContext* cx,
                                            Handle<GlobalObject*> global,
                                            const JSClass* clasp);

  /*
   * Identical to createBlankPrototype, but uses proto as the [[Prototype]]
   * of the returned blank prototype.
   */
  static NativeObject* createBlankPrototypeInheriting(JSContext* cx,
                                                      const JSClass* clasp,
                                                      HandleObject proto);

  template <typename T>
  static T* createBlankPrototypeInheriting(JSContext* cx, HandleObject proto) {
    NativeObject* res = createBlankPrototypeInheriting(cx, &T::class_, proto);
    return res ? &res->template as<T>() : nullptr;
  }

  template <typename T>
  static T* createBlankPrototype(JSContext* cx, Handle<GlobalObject*> global) {
    NativeObject* res = createBlankPrototype(cx, global, &T::class_);
    return res ? &res->template as<T>() : nullptr;
  }

  static JSObject* getOrCreateObjectPrototype(JSContext* cx,
                                              Handle<GlobalObject*> global) {
    if (!global->functionObjectClassesInitialized()) {
      if (!ensureConstructor(cx, global, JSProto_Object)) {
        return nullptr;
      }
    }
    return &global->getPrototype(JSProto_Object);
  }

  static JSObject* getOrCreateFunctionConstructor(
      JSContext* cx, Handle<GlobalObject*> global) {
    if (!global->functionObjectClassesInitialized()) {
      if (!ensureConstructor(cx, global, JSProto_Object)) {
        return nullptr;
      }
    }
    return &global->getConstructor(JSProto_Function);
  }

  static JSObject* getOrCreateFunctionPrototype(JSContext* cx,
                                                Handle<GlobalObject*> global) {
    if (!global->functionObjectClassesInitialized()) {
      if (!ensureConstructor(cx, global, JSProto_Object)) {
        return nullptr;
      }
    }
    return &global->getPrototype(JSProto_Function);
  }

  static NativeObject* getOrCreateArrayPrototype(JSContext* cx,
                                                 Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_Array)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_Array).as<NativeObject>();
  }

  NativeObject* maybeGetArrayPrototype() {
    if (classIsInitialized(JSProto_Array)) {
      return &getPrototype(JSProto_Array).as<NativeObject>();
    }
    return nullptr;
  }

  static JSObject* getOrCreateBooleanPrototype(JSContext* cx,
                                               Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_Boolean)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_Boolean);
  }

  static JSObject* getOrCreateNumberPrototype(JSContext* cx,
                                              Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_Number)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_Number);
  }

  static JSObject* getOrCreateStringPrototype(JSContext* cx,
                                              Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_String)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_String);
  }

  static JSObject* getOrCreateSymbolPrototype(JSContext* cx,
                                              Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_Symbol)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_Symbol);
  }

  static JSObject* getOrCreateBigIntPrototype(JSContext* cx,
                                              Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_BigInt)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_BigInt);
  }

  static JSObject* getOrCreatePromisePrototype(JSContext* cx,
                                               Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_Promise)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_Promise);
  }

  static JSObject* getOrCreateRegExpPrototype(JSContext* cx,
                                              Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_RegExp)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_RegExp);
  }

  JSObject* maybeGetRegExpPrototype() {
    if (classIsInitialized(JSProto_RegExp)) {
      return &getPrototype(JSProto_RegExp);
    }
    return nullptr;
  }

  static JSObject* getOrCreateSavedFramePrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_SavedFrame)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_SavedFrame);
  }

  static JSObject* getOrCreateArrayBufferConstructor(
      JSContext* cx, Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_ArrayBuffer)) {
      return nullptr;
    }
    return &global->getConstructor(JSProto_ArrayBuffer);
  }

  static JSObject* getOrCreateArrayBufferPrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_ArrayBuffer)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_ArrayBuffer);
  }

  static JSObject* getOrCreateSharedArrayBufferPrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_SharedArrayBuffer)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_SharedArrayBuffer);
  }

  static JSObject* getOrCreateCustomErrorPrototype(JSContext* cx,
                                                   Handle<GlobalObject*> global,
                                                   JSExnType exnType) {
    JSProtoKey key = GetExceptionProtoKey(exnType);
    if (!ensureConstructor(cx, global, key)) {
      return nullptr;
    }
    return &global->getPrototype(key);
  }

  static JSFunction* getOrCreateErrorConstructor(JSContext* cx,
                                                 Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_Error)) {
      return nullptr;
    }
    return &global->getConstructor(JSProto_Error).as<JSFunction>();
  }

  static JSObject* getOrCreateErrorPrototype(JSContext* cx,
                                             Handle<GlobalObject*> global) {
    return getOrCreateCustomErrorPrototype(cx, global, JSEXN_ERR);
  }

  static NativeObject* getOrCreateSetPrototype(JSContext* cx,
                                               Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_Set)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_Set).as<NativeObject>();
  }

  static NativeObject* getOrCreateWeakSetPrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_WeakSet)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_WeakSet).as<NativeObject>();
  }

  static bool ensureModulePrototypesCreated(JSContext* cx,
                                            Handle<GlobalObject*> global,
                                            bool setUsedAsPrototype = false);

  static JSObject* getOrCreateModulePrototype(JSContext* cx,
                                              Handle<GlobalObject*> global) {
    return getOrCreateBuiltinProto(cx, global, ProtoKind::ModuleProto,
                                   initModuleProto);
  }

  static JSObject* getOrCreateImportEntryPrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    return getOrCreateBuiltinProto(cx, global, ProtoKind::ImportEntryProto,
                                   initImportEntryProto);
  }

  static JSObject* getOrCreateExportEntryPrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    return getOrCreateBuiltinProto(cx, global, ProtoKind::ExportEntryProto,
                                   initExportEntryProto);
  }

  static JSObject* getOrCreateRequestedModulePrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    return getOrCreateBuiltinProto(cx, global, ProtoKind::RequestedModuleProto,
                                   initRequestedModuleProto);
  }

  static JSObject* getOrCreateModuleRequestPrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    return getOrCreateBuiltinProto(cx, global, ProtoKind::ModuleRequestProto,
                                   initModuleRequestProto);
  }

  static JSFunction* getOrCreateTypedArrayConstructor(
      JSContext* cx, Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_TypedArray)) {
      return nullptr;
    }
    return &global->getConstructor(JSProto_TypedArray).as<JSFunction>();
  }

  static JSObject* getOrCreateTypedArrayPrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_TypedArray)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_TypedArray);
  }

 private:
  using ObjectInitOp = bool (*)(JSContext*, Handle<GlobalObject*>);
  using ObjectInitWithTagOp = bool (*)(JSContext*, Handle<GlobalObject*>,
                                       HandleAtom);

  static JSObject* getOrCreateBuiltinProto(JSContext* cx,
                                           Handle<GlobalObject*> global,
                                           ProtoKind kind, ObjectInitOp init) {
    if (JSObject* proto = global->maybeBuiltinProto(kind)) {
      return proto;
    }

    return createBuiltinProto(cx, global, kind, init);
  }

  static JSObject* getOrCreateBuiltinProto(JSContext* cx,
                                           Handle<GlobalObject*> global,
                                           ProtoKind kind, HandleAtom tag,
                                           ObjectInitWithTagOp init) {
    if (JSObject* proto = global->maybeBuiltinProto(kind)) {
      return proto;
    }

    return createBuiltinProto(cx, global, kind, tag, init);
  }

  static JSObject* createBuiltinProto(JSContext* cx,
                                      Handle<GlobalObject*> global,
                                      ProtoKind kind, ObjectInitOp init);
  static JSObject* createBuiltinProto(JSContext* cx,
                                      Handle<GlobalObject*> global,
                                      ProtoKind kind, HandleAtom tag,
                                      ObjectInitWithTagOp init);

  static JSObject* createIteratorPrototype(JSContext* cx,
                                           Handle<GlobalObject*> global);

 public:
  static JSObject* getOrCreateIteratorPrototype(JSContext* cx,
                                                Handle<GlobalObject*> global) {
    if (JSObject* proto = global->maybeBuiltinProto(ProtoKind::IteratorProto)) {
      return proto;
    }
    return createIteratorPrototype(cx, global);
  }

  static NativeObject* getOrCreateArrayIteratorPrototype(
      JSContext* cx, Handle<GlobalObject*> global);

  NativeObject* maybeGetArrayIteratorPrototype() {
    if (JSObject* obj = maybeBuiltinProto(ProtoKind::ArrayIteratorProto)) {
      return &obj->as<NativeObject>();
    }
    return nullptr;
  }

  static JSObject* getOrCreateStringIteratorPrototype(
      JSContext* cx, Handle<GlobalObject*> global);

  static JSObject* getOrCreateRegExpStringIteratorPrototype(
      JSContext* cx, Handle<GlobalObject*> global);

  void setGeneratorObjectPrototype(JSObject* obj) {
    initBuiltinProto(ProtoKind::GeneratorObjectProto, obj);
  }

  static JSObject* getOrCreateGeneratorObjectPrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_GeneratorFunction)) {
      return nullptr;
    }
    return &global->getBuiltinProto(ProtoKind::GeneratorObjectProto);
  }

  static JSObject* getOrCreateGeneratorFunctionPrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_GeneratorFunction)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_GeneratorFunction);
  }

  static JSObject* getOrCreateGeneratorFunction(JSContext* cx,
                                                Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_GeneratorFunction)) {
      return nullptr;
    }
    return &global->getConstructor(JSProto_GeneratorFunction);
  }

  static JSObject* getOrCreateAsyncFunctionPrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_AsyncFunction)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_AsyncFunction);
  }

  static JSObject* getOrCreateAsyncFunction(JSContext* cx,
                                            Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_AsyncFunction)) {
      return nullptr;
    }
    return &global->getConstructor(JSProto_AsyncFunction);
  }

  static JSObject* createAsyncIteratorPrototype(JSContext* cx,
                                                Handle<GlobalObject*> global);

  static JSObject* getOrCreateAsyncIteratorPrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    if (JSObject* proto =
            global->maybeBuiltinProto(ProtoKind::AsyncIteratorProto)) {
      return proto;
    }
    return createAsyncIteratorPrototype(cx, global);
  }

  static JSObject* getOrCreateAsyncFromSyncIteratorPrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    return getOrCreateBuiltinProto(cx, global,
                                   ProtoKind::AsyncFromSyncIteratorProto,
                                   initAsyncFromSyncIteratorProto);
  }

  static JSObject* getOrCreateAsyncGenerator(JSContext* cx,
                                             Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_AsyncGeneratorFunction)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_AsyncGeneratorFunction);
  }

  static JSObject* getOrCreateAsyncGeneratorFunction(
      JSContext* cx, Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_AsyncGeneratorFunction)) {
      return nullptr;
    }
    return &global->getConstructor(JSProto_AsyncGeneratorFunction);
  }

  void setAsyncGeneratorPrototype(JSObject* obj) {
    initBuiltinProto(ProtoKind::AsyncGeneratorProto, obj);
  }

  static JSObject* getOrCreateAsyncGeneratorPrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_AsyncGeneratorFunction)) {
      return nullptr;
    }
    return &global->getBuiltinProto(ProtoKind::AsyncGeneratorProto);
  }

  static JSObject* getOrCreateMapIteratorPrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    return getOrCreateBuiltinProto(cx, global, ProtoKind::MapIteratorProto,
                                   initMapIteratorProto);
  }

  static JSObject* getOrCreateSetIteratorPrototype(
      JSContext* cx, Handle<GlobalObject*> global) {
    return getOrCreateBuiltinProto(cx, global, ProtoKind::SetIteratorProto,
                                   initSetIteratorProto);
  }

  static JSObject* getOrCreateDataViewPrototype(JSContext* cx,
                                                Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_DataView)) {
      return nullptr;
    }
    return &global->getPrototype(JSProto_DataView);
  }

  static JSObject* getOrCreatePromiseConstructor(JSContext* cx,
                                                 Handle<GlobalObject*> global) {
    if (!ensureConstructor(cx, global, JSProto_Promise)) {
      return nullptr;
    }
    return &global->getConstructor(JSProto_Promise);
  }

  static NativeObject* getOrCreateWrapForValidIteratorPrototype(
      JSContext* cx, Handle<GlobalObject*> global);

  static NativeObject* getOrCreateIteratorHelperPrototype(
      JSContext* cx, Handle<GlobalObject*> global);

  static NativeObject* getOrCreateAsyncIteratorHelperPrototype(
      JSContext* cx, Handle<GlobalObject*> global);
  static bool initAsyncIteratorHelperProto(JSContext* cx,
                                           Handle<GlobalObject*> global);

  static NativeObject* getIntrinsicsHolder(JSContext* cx,
                                           Handle<GlobalObject*> global);

  bool maybeExistingIntrinsicValue(PropertyName* name, Value* vp) {
    NativeObject* holder = data().intrinsicsHolder;
    if (!holder) {
      return false;
    }

    mozilla::Maybe<PropertyInfo> prop = holder->lookupPure(name);
    if (prop.isNothing()) {
      *vp = UndefinedValue();
      return false;
    }

    *vp = holder->getSlot(prop->slot());
    return true;
  }

  static bool maybeGetIntrinsicValue(JSContext* cx,
                                     Handle<GlobalObject*> global,
                                     Handle<PropertyName*> name,
                                     MutableHandleValue vp, bool* exists) {
    NativeObject* holder = getIntrinsicsHolder(cx, global);
    if (!holder) {
      return false;
    }

    if (mozilla::Maybe<PropertyInfo> prop = holder->lookup(cx, name)) {
      vp.set(holder->getSlot(prop->slot()));
      *exists = true;
    } else {
      *exists = false;
    }

    return true;
  }

  static bool getIntrinsicValue(JSContext* cx, Handle<GlobalObject*> global,
                                HandlePropertyName name,
                                MutableHandleValue value) {
    bool exists = false;
    if (!GlobalObject::maybeGetIntrinsicValue(cx, global, name, value,
                                              &exists)) {
      return false;
    }
    if (exists) {
      return true;
    }
    return getIntrinsicValueSlow(cx, global, name, value);
  }

  static bool getIntrinsicValueSlow(JSContext* cx, Handle<GlobalObject*> global,
                                    HandlePropertyName name,
                                    MutableHandleValue value);

  static bool addIntrinsicValue(JSContext* cx, Handle<GlobalObject*> global,
                                HandlePropertyName name, HandleValue value);

  static inline bool setIntrinsicValue(JSContext* cx,
                                       Handle<GlobalObject*> global,
                                       HandlePropertyName name,
                                       HandleValue value);

  static bool getSelfHostedFunction(JSContext* cx, Handle<GlobalObject*> global,
                                    HandlePropertyName selfHostedName,
                                    HandleAtom name, unsigned nargs,
                                    MutableHandleValue funVal);

  static RegExpStatics* getRegExpStatics(JSContext* cx,
                                         Handle<GlobalObject*> global);

  static JSObject* getOrCreateThrowTypeError(JSContext* cx,
                                             Handle<GlobalObject*> global);

  static bool isRuntimeCodeGenEnabled(JSContext* cx, HandleString code,
                                      Handle<GlobalObject*> global);

  static bool getOrCreateEval(JSContext* cx, Handle<GlobalObject*> global,
                              MutableHandleObject eval);

  // Infallibly test whether the given value is the eval function for this
  // global.
  bool valueIsEval(const Value& val);

  // Implemented in vm/Iteration.cpp.
  static bool initIteratorProto(JSContext* cx, Handle<GlobalObject*> global);
  template <ProtoKind Kind, const JSClass* ProtoClass,
            const JSFunctionSpec* Methods>
  static bool initObjectIteratorProto(JSContext* cx,
                                      Handle<GlobalObject*> global,
                                      HandleAtom tag);

  // Implemented in vm/AsyncIteration.cpp.
  static bool initAsyncIteratorProto(JSContext* cx,
                                     Handle<GlobalObject*> global);
  static bool initAsyncFromSyncIteratorProto(JSContext* cx,
                                             Handle<GlobalObject*> global);

  // Implemented in builtin/MapObject.cpp.
  static bool initMapIteratorProto(JSContext* cx, Handle<GlobalObject*> global);
  static bool initSetIteratorProto(JSContext* cx, Handle<GlobalObject*> global);

  // Implemented in builtin/ModuleObject.cpp
  static bool initModuleProto(JSContext* cx, Handle<GlobalObject*> global);
  static bool initImportEntryProto(JSContext* cx, Handle<GlobalObject*> global);
  static bool initExportEntryProto(JSContext* cx, Handle<GlobalObject*> global);
  static bool initRequestedModuleProto(JSContext* cx,
                                       Handle<GlobalObject*> global);
  static bool initModuleRequestProto(JSContext* cx,
                                     Handle<GlobalObject*> global);

  static bool initStandardClasses(JSContext* cx, Handle<GlobalObject*> global);

  Realm::DebuggerVector& getDebuggers() const {
    return realm()->getDebuggers();
  }

  inline NativeObject* getForOfPICObject() { return data().forOfPICChain; }
  static NativeObject* getOrCreateForOfPICObject(JSContext* cx,
                                                 Handle<GlobalObject*> global);

  JSObject* windowProxy() const {
    return &getReservedSlot(WINDOW_PROXY).toObject();
  }
  JSObject* maybeWindowProxy() const {
    Value v = getReservedSlot(WINDOW_PROXY);
    MOZ_ASSERT(v.isObject() || v.isUndefined());
    return v.isObject() ? &v.toObject() : nullptr;
  }
  void setWindowProxy(JSObject* windowProxy) {
    setReservedSlot(WINDOW_PROXY, ObjectValue(*windowProxy));
  }

  ArrayObject* getSourceURLsHolder() const { return data().sourceURLsHolder; }

  void setSourceURLsHolder(ArrayObject* holder) {
    data().sourceURLsHolder = holder;
  }
  void clearSourceURLSHolder() {
    // This is called at the start of shrinking GCs, so avoids barriers.
    data().sourceURLsHolder.unbarrieredSet(nullptr);
  }

  void setArrayShape(Shape* shape) {
    MOZ_ASSERT(!data().arrayShape);
    data().arrayShape.init(shape);
  }
  Shape* maybeArrayShape() const { return data().arrayShape; }

  // Returns an object that represents the realm, used by embedder.
  static JSObject* getOrCreateRealmKeyObject(JSContext* cx,
                                             Handle<GlobalObject*> global);

  // A class used in place of a prototype during off-thread parsing.
  struct OffThreadPlaceholderObject : public NativeObject {
    // The slot either stores a JSProtoKey (Int32Value >= 0) or a ProtoKind
    // (Int32Value < 0).
    static const int32_t ProtoKeyOrProtoKindSlot = 0;
    static const JSClass class_;
    static OffThreadPlaceholderObject* New(JSContext* cx, JSProtoKey key);
    static OffThreadPlaceholderObject* New(JSContext* cx, ProtoKind kind);
    inline int32_t getProtoKeyOrProtoKind() const;
  };

  static bool isOffThreadPrototypePlaceholder(JSObject* obj) {
    return obj->is<OffThreadPlaceholderObject>();
  }

  JSObject* getPrototypeForOffThreadPlaceholder(JSObject* placeholder);

 private:
  static bool resolveOffThreadConstructor(JSContext* cx,
                                          Handle<GlobalObject*> global,
                                          JSProtoKey key);
  static JSObject* createOffThreadBuiltinProto(JSContext* cx,
                                               Handle<GlobalObject*> global,
                                               ProtoKind kind);
};

/*
 * Unless otherwise specified, define ctor.prototype = proto as non-enumerable,
 * non-configurable, and non-writable; and define proto.constructor = ctor as
 * non-enumerable but configurable and writable.
 */
extern bool LinkConstructorAndPrototype(
    JSContext* cx, JSObject* ctor, JSObject* proto,
    unsigned prototypeAttrs = JSPROP_PERMANENT | JSPROP_READONLY,
    unsigned constructorAttrs = 0);

/*
 * Define properties and/or functions on any object. Either ps or fs, or both,
 * may be null.
 */
extern bool DefinePropertiesAndFunctions(JSContext* cx, HandleObject obj,
                                         const JSPropertySpec* ps,
                                         const JSFunctionSpec* fs);

extern bool DefineToStringTag(JSContext* cx, HandleObject obj, JSAtom* tag);

/*
 * Convenience templates to generic constructor and prototype creation functions
 * for ClassSpecs.
 */

template <JSNative ctor, unsigned length, gc::AllocKind kind,
          const JSJitInfo* jitInfo = nullptr>
JSObject* GenericCreateConstructor(JSContext* cx, JSProtoKey key) {
  // Note - We duplicate the trick from ClassName() so that we don't need to
  // include vm/JSAtom-inl.h here.
  PropertyName* name = (&cx->names().Null)[key];
  return GlobalObject::createConstructor(cx, ctor, name, length, kind, jitInfo);
}

template <typename T>
JSObject* GenericCreatePrototype(JSContext* cx, JSProtoKey key) {
  static_assert(
      !std::is_same_v<T, PlainObject>,
      "creating Object.prototype is very special and isn't handled here");
  MOZ_ASSERT(&T::class_ == ProtoKeyToClass(key),
             "type mismatch--probably too much copy/paste in your ClassSpec");
  MOZ_ASSERT(
      InheritanceProtoKeyForStandardClass(key) == JSProto_Object,
      "subclasses (of anything but Object) can't use GenericCreatePrototype");
  return GlobalObject::createBlankPrototype(cx, cx->global(), &T::protoClass_);
}

inline JSProtoKey StandardProtoKeyOrNull(const JSObject* obj) {
  return JSCLASS_CACHED_PROTO_KEY(obj->getClass());
}

JSObject* NewTenuredObjectWithFunctionPrototype(JSContext* cx,
                                                Handle<GlobalObject*> global);

}  // namespace js

template <>
inline bool JSObject::is<js::GlobalObject>() const {
  return !!(getClass()->flags & JSCLASS_IS_GLOBAL);
}

#endif /* vm_GlobalObject_h */
