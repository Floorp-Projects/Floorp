/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_ModuleObject_h
#define builtin_ModuleObject_h

#include "mozilla/HashTable.h"  // mozilla::{HashMap, DefaultHasher}
#include "mozilla/Maybe.h"      // mozilla::Maybe

#include <stddef.h>  // size_t
#include <stdint.h>  // int32_t, uint32_t

#include "builtin/SelfHostingDefines.h"  // MODULE_OBJECT_*
#include "gc/Barrier.h"                  // HeapPtr
#include "gc/ZoneAllocator.h"            // CellAllocPolicy
#include "js/Class.h"                    // JSClass, ObjectOpResult
#include "js/GCVector.h"                 // GCVector
#include "js/Id.h"                       // jsid
#include "js/Modules.h"                  // JS::DynamicImportStatus
#include "js/PropertyDescriptor.h"       // PropertyDescriptor
#include "js/Proxy.h"                    // BaseProxyHandler
#include "js/RootingAPI.h"               // Rooted, Handle, MutableHandle
#include "js/TypeDecls.h"  // HandleValue, HandleId, HandleObject, HandleScript, MutableHandleValue, MutableHandleIdVector, MutableHandleObject
#include "js/UniquePtr.h"  // UniquePtr
#include "js/Value.h"      // JS::Value
#include "vm/JSAtom.h"     // JSAtom
#include "vm/JSObject.h"   // JSObject
#include "vm/List.h"       // ListObject
#include "vm/NativeObject.h"   // NativeObject
#include "vm/PromiseObject.h"  // js::PromiseObject
#include "vm/ProxyObject.h"    // ProxyObject

class JSScript;
class JSTracer;

namespace js {

class ArrayObject;
class Shape;
class Scope;
class ScriptSourceObject;

class ModuleEnvironmentObject;
class ModuleObject;

class ModuleRequestObject : public NativeObject {
 public:
  enum { SpecifierSlot = 0, AssertionSlot, SlotCount };

  static const JSClass class_;
  static bool isInstance(HandleValue value);
  [[nodiscard]] static ModuleRequestObject* create(
      JSContext* cx, Handle<JSAtom*> specifier,
      Handle<ArrayObject*> maybeAssertions);

  JSAtom* specifier() const;
};

class ImportEntryObject : public NativeObject {
 public:
  enum {
    ModuleRequestSlot = 0,
    ImportNameSlot,
    LocalNameSlot,
    LineNumberSlot,
    ColumnNumberSlot,
    SlotCount
  };

  static const JSClass class_;
  static bool isInstance(HandleValue value);
  static ImportEntryObject* create(JSContext* cx, HandleObject moduleRequest,
                                   Handle<JSAtom*> maybeImportName,
                                   Handle<JSAtom*> localName,
                                   uint32_t lineNumber, uint32_t columnNumber);
  ModuleRequestObject* moduleRequest() const;
  JSAtom* importName() const;
  JSAtom* localName() const;
  uint32_t lineNumber() const;
  uint32_t columnNumber() const;
};

class ExportEntryObject : public NativeObject {
 public:
  enum {
    ExportNameSlot = 0,
    ModuleRequestSlot,
    ImportNameSlot,
    LocalNameSlot,
    LineNumberSlot,
    ColumnNumberSlot,
    SlotCount
  };

  static const JSClass class_;
  static bool isInstance(HandleValue value);
  static ExportEntryObject* create(JSContext* cx,
                                   Handle<JSAtom*> maybeExportName,
                                   HandleObject maybeModuleRequest,
                                   Handle<JSAtom*> maybeImportName,
                                   Handle<JSAtom*> maybeLocalName,
                                   uint32_t lineNumber, uint32_t columnNumber);
  JSAtom* exportName() const;
  ModuleRequestObject* moduleRequest() const;
  JSAtom* importName() const;
  JSAtom* localName() const;
  uint32_t lineNumber() const;
  uint32_t columnNumber() const;
};

class RequestedModuleObject : public NativeObject {
 public:
  enum { ModuleRequestSlot = 0, LineNumberSlot, ColumnNumberSlot, SlotCount };

  static const JSClass class_;
  static bool isInstance(HandleValue value);
  static RequestedModuleObject* create(JSContext* cx,
                                       HandleObject moduleRequest,
                                       uint32_t lineNumber,
                                       uint32_t columnNumber);
  ModuleRequestObject* moduleRequest() const;
  uint32_t lineNumber() const;
  uint32_t columnNumber() const;
};

class IndirectBindingMap {
 public:
  void trace(JSTracer* trc);

  bool put(JSContext* cx, HandleId name,
           Handle<ModuleEnvironmentObject*> environment, HandleId targetName);

  size_t count() const { return map_ ? map_->count() : 0; }

  bool has(jsid name) const { return map_ ? map_->has(name) : false; }

  bool lookup(jsid name, ModuleEnvironmentObject** envOut,
              mozilla::Maybe<PropertyInfo>* propOut) const;

  template <typename Func>
  void forEachExportedName(Func func) const {
    if (!map_) {
      return;
    }

    for (auto r = map_->all(); !r.empty(); r.popFront()) {
      func(r.front().key());
    }
  }

 private:
  struct Binding {
    Binding(ModuleEnvironmentObject* environment, jsid targetName,
            PropertyInfo prop);
    HeapPtr<ModuleEnvironmentObject*> environment;
#ifdef DEBUG
    HeapPtr<jsid> targetName;
#endif
    PropertyInfo prop;
  };

  using Map = mozilla::HashMap<PreBarriered<jsid>, Binding,
                               mozilla::DefaultHasher<PreBarriered<jsid>>,
                               CellAllocPolicy>;

  mozilla::Maybe<Map> map_;
};

class ModuleNamespaceObject : public ProxyObject {
 public:
  enum ModuleNamespaceSlot { ExportsSlot = 0, BindingsSlot };

  static bool isInstance(HandleValue value);
  static ModuleNamespaceObject* create(JSContext* cx,
                                       Handle<ModuleObject*> module,
                                       Handle<ArrayObject*> exports,
                                       UniquePtr<IndirectBindingMap> bindings);

  ModuleObject& module();
  ArrayObject& exports();
  IndirectBindingMap& bindings();

  bool addBinding(JSContext* cx, Handle<JSAtom*> exportedName,
                  Handle<ModuleObject*> targetModule,
                  Handle<JSAtom*> targetName);

 private:
  struct ProxyHandler : public BaseProxyHandler {
    ProxyHandler();

    bool getOwnPropertyDescriptor(
        JSContext* cx, HandleObject proxy, HandleId id,
        MutableHandle<mozilla::Maybe<PropertyDescriptor>> desc) const override;
    bool defineProperty(JSContext* cx, HandleObject proxy, HandleId id,
                        Handle<PropertyDescriptor> desc,
                        ObjectOpResult& result) const override;
    bool ownPropertyKeys(JSContext* cx, HandleObject proxy,
                         MutableHandleIdVector props) const override;
    bool delete_(JSContext* cx, HandleObject proxy, HandleId id,
                 ObjectOpResult& result) const override;
    bool getPrototype(JSContext* cx, HandleObject proxy,
                      MutableHandleObject protop) const override;
    bool setPrototype(JSContext* cx, HandleObject proxy, HandleObject proto,
                      ObjectOpResult& result) const override;
    bool getPrototypeIfOrdinary(JSContext* cx, HandleObject proxy,
                                bool* isOrdinary,
                                MutableHandleObject protop) const override;
    bool setImmutablePrototype(JSContext* cx, HandleObject proxy,
                               bool* succeeded) const override;

    bool preventExtensions(JSContext* cx, HandleObject proxy,
                           ObjectOpResult& result) const override;
    bool isExtensible(JSContext* cx, HandleObject proxy,
                      bool* extensible) const override;
    bool has(JSContext* cx, HandleObject proxy, HandleId id,
             bool* bp) const override;
    bool get(JSContext* cx, HandleObject proxy, HandleValue receiver,
             HandleId id, MutableHandleValue vp) const override;
    bool set(JSContext* cx, HandleObject proxy, HandleId id, HandleValue v,
             HandleValue receiver, ObjectOpResult& result) const override;

    void trace(JSTracer* trc, JSObject* proxy) const override;
    void finalize(JS::GCContext* gcx, JSObject* proxy) const override;

    static const char family;
  };

  bool hasBindings() const;

 public:
  static const ProxyHandler proxyHandler;
};

// Possible values for ModuleStatus are defined in SelfHostingDefines.h.
using ModuleStatus = int32_t;

class ModuleObject : public NativeObject {
 public:
  enum ModuleSlot {
    ScriptSlot = 0,
    EnvironmentSlot,
    NamespaceSlot,
    StatusSlot,
    EvaluationErrorSlot,
    MetaObjectSlot,
    ScriptSourceObjectSlot,
    RequestedModulesSlot,
    ImportEntriesSlot,
    LocalExportEntriesSlot,
    IndirectExportEntriesSlot,
    StarExportEntriesSlot,
    ImportBindingsSlot,
    FunctionDeclarationsSlot,
    DFSIndexSlot,
    DFSAncestorIndexSlot,
    AsyncSlot,
    AsyncEvaluatingPostOrderSlot,
    TopLevelCapabilitySlot,
    AsyncParentModulesSlot,
    PendingAsyncDependenciesSlot,
    CycleRootSlot,
    SlotCount
  };

  static_assert(EnvironmentSlot == MODULE_OBJECT_ENVIRONMENT_SLOT,
                "EnvironmentSlot must match self-hosting define");
  static_assert(StatusSlot == MODULE_OBJECT_STATUS_SLOT,
                "StatusSlot must match self-hosting define");
  static_assert(EvaluationErrorSlot == MODULE_OBJECT_EVALUATION_ERROR_SLOT,
                "EvaluationErrorSlot must match self-hosting define");
  static_assert(DFSIndexSlot == MODULE_OBJECT_DFS_INDEX_SLOT,
                "DFSIndexSlot must match self-hosting define");
  static_assert(DFSAncestorIndexSlot == MODULE_OBJECT_DFS_ANCESTOR_INDEX_SLOT,
                "DFSAncestorIndexSlot must match self-hosting define");
  static_assert(AsyncEvaluatingPostOrderSlot ==
                    MODULE_OBJECT_ASYNC_EVALUATING_POST_ORDER_SLOT,
                "AsyncEvaluatingSlot must match self-hosting define");
  static_assert(TopLevelCapabilitySlot ==
                    MODULE_OBJECT_TOP_LEVEL_CAPABILITY_SLOT,
                "topLevelCapabilitySlot must match self-hosting define");
  static_assert(PendingAsyncDependenciesSlot ==
                    MODULE_OBJECT_PENDING_ASYNC_DEPENDENCIES_SLOT,
                "PendingAsyncDependenciesSlot must match self-hosting define");

  static const JSClass class_;

  static bool isInstance(HandleValue value);

  static ModuleObject* create(JSContext* cx);

  // Initialize the slots on this object that are dependent on the script.
  void initScriptSlots(HandleScript script);

  void setInitialEnvironment(
      Handle<ModuleEnvironmentObject*> initialEnvironment);

  void initStatusSlot();
  void initImportExportData(Handle<ArrayObject*> requestedModules,
                            Handle<ArrayObject*> importEntries,
                            Handle<ArrayObject*> localExportEntries,
                            Handle<ArrayObject*> indirectExportEntries,
                            Handle<ArrayObject*> starExportEntries);
  static bool Freeze(JSContext* cx, Handle<ModuleObject*> self);
#ifdef DEBUG
  static bool AssertFrozen(JSContext* cx, Handle<ModuleObject*> self);
#endif

  JSScript* maybeScript() const;
  JSScript* script() const;
  ModuleEnvironmentObject& initialEnvironment() const;
  ModuleEnvironmentObject* environment() const;
  ModuleNamespaceObject* namespace_();
  ModuleStatus status() const;
  uint32_t dfsIndex() const;
  uint32_t dfsAncestorIndex() const;
  bool hadEvaluationError() const;
  Value evaluationError() const;
  JSObject* metaObject() const;
  ScriptSourceObject* scriptSourceObject() const;
  ArrayObject& requestedModules() const;
  ArrayObject& importEntries() const;
  ArrayObject& localExportEntries() const;
  ArrayObject& indirectExportEntries() const;
  ArrayObject& starExportEntries() const;
  IndirectBindingMap& importBindings();

  static PromiseObject* createTopLevelCapability(JSContext* cx,
                                                 Handle<ModuleObject*> module);
  bool isAsync() const;
  bool isAsyncEvaluating() const;
  void setAsyncEvaluatingFalse();
  void setEvaluationError(HandleValue newValue);
  void setPendingAsyncDependencies(uint32_t newValue);
  void setInitialTopLevelCapability(HandleObject promiseObj);
  bool hasTopLevelCapability() const;
  JSObject* topLevelCapability() const;
  ListObject* asyncParentModules() const;
  uint32_t pendingAsyncDependencies() const;
  uint32_t getAsyncEvaluatingPostOrder() const;
  void setCycleRoot(ModuleObject* cycleRoot);
  ModuleObject* getCycleRoot() const;

  static void onTopLevelEvaluationFinished(ModuleObject* module);

  static bool appendAsyncParentModule(JSContext* cx, Handle<ModuleObject*> self,
                                      Handle<ModuleObject*> parent);

  [[nodiscard]] static bool topLevelCapabilityResolve(
      JSContext* cx, Handle<ModuleObject*> module);
  [[nodiscard]] static bool topLevelCapabilityReject(
      JSContext* cx, Handle<ModuleObject*> module, HandleValue error);

  static bool Instantiate(JSContext* cx, Handle<ModuleObject*> self);

  // Start evaluating the module. If TLA is enabled, rval will be a promise
  static bool Evaluate(JSContext* cx, Handle<ModuleObject*> self,
                       MutableHandleValue rval);

  static ModuleNamespaceObject* GetOrCreateModuleNamespace(
      JSContext* cx, Handle<ModuleObject*> self);

  void setMetaObject(JSObject* obj);

  // For intrinsic_InstantiateModuleFunctionDeclarations.
  static bool instantiateFunctionDeclarations(JSContext* cx,
                                              Handle<ModuleObject*> self);

  // For intrinsic_ExecuteModule.
  static bool execute(JSContext* cx, Handle<ModuleObject*> self,
                      MutableHandleValue rval);

  // For intrinsic_NewModuleNamespace.
  static ModuleNamespaceObject* createNamespace(JSContext* cx,
                                                Handle<ModuleObject*> self,
                                                HandleObject exports);

  static bool createEnvironment(JSContext* cx, Handle<ModuleObject*> self);

  bool initAsyncSlots(JSContext* cx, bool isAsync,
                      HandleObject asyncParentModulesList);

  bool initAsyncEvaluatingSlot();

  static bool GatherAsyncParentCompletions(
      JSContext* cx, Handle<ModuleObject*> module,
      MutableHandle<ArrayObject*> execList);
  // NOTE: accessor for FunctionDeclarationsSlot is defined inside
  // ModuleObject.cpp as static function.

 private:
  static const JSClassOps classOps_;

  static void trace(JSTracer* trc, JSObject* obj);
  static void finalize(JS::GCContext* gcx, JSObject* obj);

  bool hasImportBindings() const;
};

JSObject* GetOrCreateModuleMetaObject(JSContext* cx, HandleObject module);

JSObject* CallModuleResolveHook(JSContext* cx, HandleValue referencingPrivate,
                                HandleObject moduleRequest);

// https://tc39.es/proposal-top-level-await/#sec-asyncmodulexecutionfulfilled
void AsyncModuleExecutionFulfilled(JSContext* cx, Handle<ModuleObject*> module);

// https://tc39.es/proposal-top-level-await/#sec-asyncmodulexecutionrejected
void AsyncModuleExecutionRejected(JSContext* cx, Handle<ModuleObject*> module,
                                  HandleValue error);

// https://tc39.es/proposal-top-level-await/#sec-asyncmodulexecutionfulfilled
bool AsyncModuleExecutionFulfilledHandler(JSContext* cx, unsigned argc,
                                          Value* vp);

// https://tc39.es/proposal-top-level-await/#sec-asyncmodulexecutionrejected
bool AsyncModuleExecutionRejectedHandler(JSContext* cx, unsigned argc,
                                         Value* vp);

JSObject* StartDynamicModuleImport(JSContext* cx, HandleScript script,
                                   HandleValue specifier, HandleValue options);

bool OnModuleEvaluationFailure(JSContext* cx, HandleObject evaluationPromise,
                               JS::ModuleErrorBehaviour errorBehaviour);

bool FinishDynamicModuleImport(JSContext* cx, HandleObject evaluationPromise,
                               HandleValue referencingPrivate,
                               HandleObject moduleRequest,
                               HandleObject promise);

// This is used so that Top Level Await functionality can be turned off
// entirely. It will be removed in bug#1676612.
bool FinishDynamicModuleImport_NoTLA(JSContext* cx,
                                     JS::DynamicImportStatus status,
                                     HandleValue referencingPrivate,
                                     HandleObject moduleRequest,
                                     HandleObject promise);

}  // namespace js

template <>
inline bool JSObject::is<js::ModuleNamespaceObject>() const {
  return js::IsDerivedProxyObject(this,
                                  &js::ModuleNamespaceObject::proxyHandler);
}

#endif /* builtin_ModuleObject_h */
