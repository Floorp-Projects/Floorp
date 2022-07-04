/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JavaScript modules (as in, the syntactic construct) implementation. */

#include "vm/Modules.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Utf8.h"        // mozilla::Utf8Unit

#include <stdint.h>  // uint32_t

#include "jstypes.h"  // JS_PUBLIC_API

#include "builtin/ModuleObject.h"  // js::FinishDynamicModuleImport, js::{,Requested}ModuleObject
#include "frontend/BytecodeCompiler.h"  // js::frontend::CompileModule
#include "js/Context.h"                 // js::AssertHeapIsIdle
#include "js/PropertyAndElement.h"
#include "js/RootingAPI.h"         // JS::MutableHandle
#include "js/Value.h"              // JS::Value
#include "vm/EnvironmentObject.h"  // js::ModuleEnvironmentObject
#include "vm/JSContext.h"          // CHECK_THREAD, JSContext
#include "vm/JSObject.h"           // JSObject
#include "vm/Runtime.h"            // JSRuntime

#include "builtin/Array-inl.h"
#include "vm/JSContext-inl.h"  // JSContext::{c,releaseC}heck

using namespace js;

using mozilla::Utf8Unit;

////////////////////////////////////////////////////////////////////////////////
// Public API

JS_PUBLIC_API JS::SupportedAssertionsHook JS::GetSupportedAssertionsHook(
    JSRuntime* rt) {
  AssertHeapIsIdle();

  return rt->supportedAssertionsHook;
}

JS_PUBLIC_API void JS::SetSupportedAssertionsHook(
    JSRuntime* rt, SupportedAssertionsHook func) {
  AssertHeapIsIdle();

  rt->supportedAssertionsHook = func;
}

JS_PUBLIC_API JS::ModuleResolveHook JS::GetModuleResolveHook(JSRuntime* rt) {
  AssertHeapIsIdle();

  return rt->moduleResolveHook;
}

JS_PUBLIC_API void JS::SetModuleResolveHook(JSRuntime* rt,
                                            ModuleResolveHook func) {
  AssertHeapIsIdle();

  rt->moduleResolveHook = func;
}

JS_PUBLIC_API JS::ModuleMetadataHook JS::GetModuleMetadataHook(JSRuntime* rt) {
  AssertHeapIsIdle();

  return rt->moduleMetadataHook;
}

JS_PUBLIC_API void JS::SetModuleMetadataHook(JSRuntime* rt,
                                             ModuleMetadataHook func) {
  AssertHeapIsIdle();

  rt->moduleMetadataHook = func;
}

JS_PUBLIC_API JS::ModuleDynamicImportHook JS::GetModuleDynamicImportHook(
    JSRuntime* rt) {
  AssertHeapIsIdle();

  return rt->moduleDynamicImportHook;
}

JS_PUBLIC_API void JS::SetModuleDynamicImportHook(
    JSRuntime* rt, ModuleDynamicImportHook func) {
  AssertHeapIsIdle();

  rt->moduleDynamicImportHook = func;
}

JS_PUBLIC_API bool JS::FinishDynamicModuleImport(
    JSContext* cx, Handle<JSObject*> evaluationPromise,
    Handle<Value> referencingPrivate, Handle<JSObject*> moduleRequest,
    Handle<JSObject*> promise) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(referencingPrivate, promise);

  return js::FinishDynamicModuleImport(
      cx, evaluationPromise, referencingPrivate, moduleRequest, promise);
}

template <typename Unit>
static JSObject* CompileModuleHelper(JSContext* cx,
                                     const JS::ReadOnlyCompileOptions& options,
                                     JS::SourceText<Unit>& srcBuf) {
  MOZ_ASSERT(!cx->zone()->isAtomsZone());
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  return frontend::CompileModule(cx, options, srcBuf);
}

JS_PUBLIC_API JSObject* JS::CompileModule(JSContext* cx,
                                          const ReadOnlyCompileOptions& options,
                                          SourceText<char16_t>& srcBuf) {
  return CompileModuleHelper(cx, options, srcBuf);
}

JS_PUBLIC_API JSObject* JS::CompileModule(JSContext* cx,
                                          const ReadOnlyCompileOptions& options,
                                          SourceText<Utf8Unit>& srcBuf) {
  return CompileModuleHelper(cx, options, srcBuf);
}

JS_PUBLIC_API void JS::SetModulePrivate(JSObject* module, const Value& value) {
  JSRuntime* rt = module->zone()->runtimeFromMainThread();
  module->as<ModuleObject>().scriptSourceObject()->setPrivate(rt, value);
}

JS_PUBLIC_API JS::Value JS::GetModulePrivate(JSObject* module) {
  return module->as<ModuleObject>().scriptSourceObject()->getPrivate();
}

JS_PUBLIC_API bool JS::ModuleInstantiate(JSContext* cx,
                                         Handle<JSObject*> moduleArg) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->releaseCheck(moduleArg);

  return js::ModuleInstantiate(cx, moduleArg.as<ModuleObject>());
}

JS_PUBLIC_API bool JS::ModuleEvaluate(JSContext* cx,
                                      Handle<JSObject*> moduleRecord,
                                      MutableHandle<JS::Value> rval) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->releaseCheck(moduleRecord);

  return ModuleObject::Evaluate(cx, moduleRecord.as<ModuleObject>(), rval);
}

JS_PUBLIC_API bool JS::ThrowOnModuleEvaluationFailure(
    JSContext* cx, Handle<JSObject*> evaluationPromise,
    ModuleErrorBehaviour errorBehaviour) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->releaseCheck(evaluationPromise);

  return OnModuleEvaluationFailure(cx, evaluationPromise, errorBehaviour);
}

JS_PUBLIC_API JSObject* JS::GetRequestedModules(JSContext* cx,
                                                Handle<JSObject*> moduleArg) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(moduleArg);

  return &moduleArg->as<ModuleObject>().requestedModules();
}

JS_PUBLIC_API JSString* JS::GetRequestedModuleSpecifier(JSContext* cx,
                                                        Handle<Value> value) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(value);

  JSObject* obj = &value.toObject();
  return obj->as<RequestedModuleObject>().moduleRequest()->specifier();
}

JS_PUBLIC_API void JS::GetRequestedModuleSourcePos(JSContext* cx,
                                                   JS::HandleValue value,
                                                   uint32_t* lineNumber,
                                                   uint32_t* columnNumber) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(value);
  MOZ_ASSERT(lineNumber);
  MOZ_ASSERT(columnNumber);

  auto& requested = value.toObject().as<RequestedModuleObject>();
  *lineNumber = requested.lineNumber();
  *columnNumber = requested.columnNumber();
}

JS_PUBLIC_API JSScript* JS::GetModuleScript(JS::HandleObject moduleRecord) {
  AssertHeapIsIdle();

  return moduleRecord->as<ModuleObject>().script();
}

JS_PUBLIC_API JSObject* JS::GetModuleObject(HandleScript moduleScript) {
  AssertHeapIsIdle();
  MOZ_ASSERT(moduleScript->isModule());

  return moduleScript->module();
}

JS_PUBLIC_API JSObject* JS::GetModuleNamespace(JSContext* cx,
                                               HandleObject moduleRecord) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(moduleRecord);
  MOZ_ASSERT(moduleRecord->is<ModuleObject>());

  return GetOrCreateModuleNamespace(cx, moduleRecord.as<ModuleObject>());
}

JS_PUBLIC_API JSObject* JS::GetModuleForNamespace(
    JSContext* cx, HandleObject moduleNamespace) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(moduleNamespace);
  MOZ_ASSERT(moduleNamespace->is<ModuleNamespaceObject>());

  return &moduleNamespace->as<ModuleNamespaceObject>().module();
}

JS_PUBLIC_API JSObject* JS::GetModuleEnvironment(JSContext* cx,
                                                 Handle<JSObject*> moduleObj) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(moduleObj);
  MOZ_ASSERT(moduleObj->is<ModuleObject>());

  return moduleObj->as<ModuleObject>().environment();
}

JS_PUBLIC_API JSObject* JS::CreateModuleRequest(
    JSContext* cx, Handle<JSString*> specifierArg) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  Rooted<JSAtom*> specifierAtom(cx, AtomizeString(cx, specifierArg));
  if (!specifierAtom) {
    return nullptr;
  }

  return ModuleRequestObject::create(cx, specifierAtom, nullptr);
}

JS_PUBLIC_API JSString* JS::GetModuleRequestSpecifier(
    JSContext* cx, Handle<JSObject*> moduleRequestArg) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(moduleRequestArg);

  return moduleRequestArg->as<ModuleRequestObject>().specifier();
}

JS_PUBLIC_API void JS::ClearModuleEnvironment(JSObject* moduleObj) {
  MOZ_ASSERT(moduleObj);
  AssertHeapIsIdle();

  js::ModuleEnvironmentObject* env =
      moduleObj->as<js::ModuleObject>().environment();

  const JSClass* clasp = env->getClass();
  uint32_t numReserved = JSCLASS_RESERVED_SLOTS(clasp);
  uint32_t numSlots = env->slotSpan();
  for (uint32_t i = numReserved; i < numSlots; i++) {
    env->setSlot(i, UndefinedValue());
  }
}

////////////////////////////////////////////////////////////////////////////////
// Internal implementation

class ResolveSetEntry {
  ModuleObject* module_;
  JSAtom* exportName_;

 public:
  ResolveSetEntry(ModuleObject* module, JSAtom* exportName)
      : module_(module), exportName_(exportName) {}

  ModuleObject* module() const { return module_; }
  JSAtom* exportName() const { return exportName_; }

  void trace(JSTracer* trc) {
    TraceRoot(trc, &module_, "ResolveSetEntry::module_");
    TraceRoot(trc, &exportName_, "ResolveSetEntry::exportName_");
  }
};

using ResolveSet = GCVector<ResolveSetEntry, 0, SystemAllocPolicy>;

using ModuleSet =
    GCHashSet<ModuleObject*, DefaultHasher<ModuleObject*>, SystemAllocPolicy>;

using ModuleVector = GCVector<ModuleObject*, 0, SystemAllocPolicy>;

static ModuleObject* HostResolveImportedModule(
    JSContext* cx, Handle<ModuleObject*> module,
    Handle<ModuleRequestObject*> moduleRequest,
    ModuleStatus expectedMinimumStatus);
static bool ModuleResolveExport(JSContext* cx, Handle<ModuleObject*> module,
                                Handle<JSAtom*> exportName,
                                MutableHandle<ResolveSet> resolveSet,
                                MutableHandle<Value> result);
static ModuleNamespaceObject* ModuleNamespaceCreate(
    JSContext* cx, Handle<ModuleObject*> module, Handle<ArrayObject*> exports);
static bool InnerModuleLinking(JSContext* cx, Handle<ModuleObject*> module,
                               MutableHandle<ModuleVector> stack, size_t index,
                               size_t* indexOut);

static ArrayObject* NewList(JSContext* cx) {
  // Note that this creates an ArrayObject, not a ListObject (see vm/List.h).
  // Self hosted code currently depends on the length property being present.
  return NewArrayWithNullProto(cx);
}

static bool ArrayContainsName(Handle<ArrayObject*> array,
                              Handle<JSAtom*> name) {
  for (uint32_t i = 0; i < array->length(); i++) {
    if (array->getDenseElement(i) == StringValue(name)) {
      return true;
    }
  }

  return false;
}

#ifdef DEBUG

static bool ContainsElement(Handle<ModuleVector> stack, ModuleObject* module) {
  for (ModuleObject* m : stack) {
    if (m == module) {
      return true;
    }
  }

  return false;
}

static size_t CountElements(Handle<ModuleVector> stack, ModuleObject* module) {
  size_t count = 0;
  for (ModuleObject* m : stack) {
    if (m == module) {
      count++;
    }
  }

  return count;
}

#endif

// https://tc39.es/ecma262/#sec-getexportednames
// ES2023 16.2.1.6.2 GetExportedNames
static ArrayObject* ModuleGetExportedNames(
    JSContext* cx, Handle<ModuleObject*> module,
    MutableHandle<ModuleSet> exportStarSet) {
  // Step 4. Let exportedNames be a new empty List.
  Rooted<ArrayObject*> exportedNames(cx, NewList(cx));
  if (!exportedNames) {
    return nullptr;
  }

  // Step 2. If exportStarSet contains module, then:
  if (exportStarSet.has(module)) {
    // Step 2.a. We've reached the starting point of an export * circularity.
    // Step 2.b. Return a new empty List.
    return exportedNames;
  }

  // Step 3. Append module to exportStarSet.
  if (!exportStarSet.put(module)) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  // Step 5. For each ExportEntry Record e of module.[[LocalExportEntries]], do:
  Rooted<ArrayObject*> localExportEntries(cx, &module->localExportEntries());
  Rooted<ExportEntryObject*> e(cx);
  for (uint32_t i = 0; i != localExportEntries->length(); i++) {
    e = &localExportEntries->getDenseElement(i)
             .toObject()
             .as<ExportEntryObject>();
    // Step 5.a. Assert: module provides the direct binding for this export.
    // Step 5.b. Append e.[[ExportName]] to exportedNames.
    if (!NewbornArrayPush(cx, exportedNames, StringValue(e->exportName()))) {
      return nullptr;
    }
  }

  // Step 6. For each ExportEntry Record e of module.[[IndirectExportEntries]],
  //         do:
  Rooted<ArrayObject*> indirectExportEntries(cx,
                                             &module->indirectExportEntries());
  for (uint32_t i = 0; i != indirectExportEntries->length(); i++) {
    e = &indirectExportEntries->getDenseElement(i)
             .toObject()
             .as<ExportEntryObject>();
    // Step 6.a. Assert: module imports a specific binding for this export.
    // Step 6.b. Append e.[[ExportName]] to exportedNames.
    if (!NewbornArrayPush(cx, exportedNames, StringValue(e->exportName()))) {
      return nullptr;
    }
  }

  // Step 7. For each ExportEntry Record e of module.[[StarExportEntries]], do:
  Rooted<ArrayObject*> starExportEntries(cx, &module->starExportEntries());
  Rooted<ModuleRequestObject*> moduleRequest(cx);
  Rooted<ModuleObject*> requestedModule(cx);
  Rooted<ArrayObject*> starNames(cx);
  Rooted<JSAtom*> name(cx);
  for (uint32_t i = 0; i != starExportEntries->length(); i++) {
    e = &starExportEntries->getDenseElement(i)
             .toObject()
             .as<ExportEntryObject>();

    // Step 7.a. Let requestedModule be ? HostResolveImportedModule(module,
    //           e.[[ModuleRequest]]).
    moduleRequest = e->moduleRequest();
    requestedModule = HostResolveImportedModule(cx, module, moduleRequest,
                                                MODULE_STATUS_UNLINKED);
    if (!requestedModule) {
      return nullptr;
    }

    // Step 7.b. Let starNames be ?
    //           requestedModule.GetExportedNames(exportStarSet).
    starNames = ModuleGetExportedNames(cx, requestedModule, exportStarSet);
    if (!starNames) {
      return nullptr;
    }

    // Step 7.c. For each element n of starNames, do:
    for (uint32_t j = 0; j != starNames->length(); j++) {
      name = &starNames->getDenseElement(j).toString()->asAtom();

      // Step 7.c.i. If SameValue(n, "default") is false, then:
      if (name != cx->names().default_) {
        // Step 7.c.i.1. If n is not an element of exportedNames, then:
        if (!ArrayContainsName(exportedNames, name)) {
          // Step 7.c.i.1.a. Append n to exportedNames.
          if (!NewbornArrayPush(cx, exportedNames, StringValue(name))) {
            return nullptr;
          }
        }
      }
    }
  }

  // Step 8. Return exportedNames.
  return exportedNames;
}

static ModuleObject* HostResolveImportedModule(
    JSContext* cx, Handle<ModuleObject*> module,
    Handle<ModuleRequestObject*> moduleRequest,
    ModuleStatus expectedMinimumStatus) {
  Rooted<Value> referencingPrivate(cx, JS::GetModulePrivate(module));
  Rooted<ModuleObject*> requestedModule(cx);
  requestedModule =
      CallModuleResolveHook(cx, referencingPrivate, moduleRequest);
  if (!requestedModule) {
    return nullptr;
  }

  if (requestedModule->status() < expectedMinimumStatus) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BAD_MODULE_STATUS);
    return nullptr;
  }

  return requestedModule;
}

// https://tc39.es/ecma262/#sec-resolveexport
// ES2023 16.2.1.6.3 ResolveExport
//
// Returns an value describing the location of the resolved export or indicating
// a failure.
//
// On success this returns a resolved binding record: { module, bindingName }
//
// There are two failure cases:
//
//  - If no definition was found or the request is found to be circular, *null*
//    is returned.
//
//  - If the request is found to be ambiguous, the string `"ambiguous"` is
//    returned.
//
bool js::ModuleResolveExport(JSContext* cx, Handle<ModuleObject*> module,
                             Handle<JSAtom*> exportName,
                             MutableHandle<Value> result) {
  // Step 1. If resolveSet is not present, set resolveSet to a new empty List.
  Rooted<ResolveSet> resolveSet(cx);

  return ::ModuleResolveExport(cx, module, exportName, &resolveSet, result);
}

static bool CreateResolvedBindingObject(JSContext* cx,
                                        Handle<ModuleObject*> module,
                                        Handle<JSAtom*> bindingName,
                                        MutableHandle<Value> result) {
  Rooted<ResolvedBindingObject*> obj(
      cx, ResolvedBindingObject::create(cx, module, bindingName));
  if (!obj) {
    return false;
  }

  result.setObject(*obj);
  return true;
}

static bool ModuleResolveExport(JSContext* cx, Handle<ModuleObject*> module,
                                Handle<JSAtom*> exportName,
                                MutableHandle<ResolveSet> resolveSet,
                                MutableHandle<Value> result) {
  // Step 2. For each Record { [[Module]], [[ExportName]] } r of resolveSet, do:
  for (const auto& entry : resolveSet) {
    // Step 2.a. If module and r.[[Module]] are the same Module Record and
    //           SameValue(exportName, r.[[ExportName]]) is true, then:
    if (entry.module() == module && entry.exportName() == exportName) {
      // Step 2.a.i. Assert: This is a circular import request.
      // Step 2.a.ii. Return null.
      result.setNull();
      return true;
    }
  }

  // Step 3. Append the Record { [[Module]]: module, [[ExportName]]: exportName
  // } to resolveSet.
  if (!resolveSet.emplaceBack(module, exportName)) {
    ReportOutOfMemory(cx);
    return false;
  }

  // Step 4. For each ExportEntry Record e of module.[[LocalExportEntries]], do:
  Rooted<ArrayObject*> localExportEntries(cx, &module->localExportEntries());
  Rooted<ExportEntryObject*> e(cx);
  for (uint32_t i = 0; i != localExportEntries->length(); i++) {
    e = &localExportEntries->getDenseElement(i)
             .toObject()
             .as<ExportEntryObject>();

    // Step 4.a. If SameValue(exportName, e.[[ExportName]]) is true, then:
    if (exportName == e->exportName()) {
      // Step 4.a.i. Assert: module provides the direct binding for this export.
      // Step 4.a.ii. Return ResolvedBinding Record { [[Module]]: module,
      //              [[BindingName]]: e.[[LocalName]] }.
      Rooted<JSAtom*> localName(cx, e->localName());
      return CreateResolvedBindingObject(cx, module, localName, result);
    }
  }

  // Step 5. For each ExportEntry Record e of module.[[IndirectExportEntries]],
  //         do:
  Rooted<ArrayObject*> indirectExportEntries(cx,
                                             &module->indirectExportEntries());
  Rooted<ModuleRequestObject*> moduleRequest(cx);
  Rooted<ModuleObject*> importedModule(cx);
  Rooted<JSAtom*> name(cx);
  for (uint32_t i = 0; i != indirectExportEntries->length(); i++) {
    e = &indirectExportEntries->getDenseElement(i)
             .toObject()
             .as<ExportEntryObject>();
    // Step 5.a. If SameValue(exportName, e.[[ExportName]]) is true, then:
    if (exportName == e->exportName()) {
      // Step 5.a.i. Let importedModule be ? HostResolveImportedModule(module,
      //             e.[[ModuleRequest]]).
      moduleRequest = e->moduleRequest();
      importedModule = HostResolveImportedModule(cx, module, moduleRequest,
                                                 MODULE_STATUS_UNLINKED);
      if (!importedModule) {
        return false;
      }

      // Step 5.a.ii. If e.[[ImportName]] is all, then:
      if (!e->importName()) {
        // Step 5.a.ii.1. Assert: module does not provide the direct binding for
        //                this export.
        // Step 5.a.ii.2. Return ResolvedBinding Record { [[Module]]:
        //                importedModule, [[BindingName]]: namespace }.
        name = cx->names().starNamespaceStar;
        return CreateResolvedBindingObject(cx, importedModule, name, result);
      } else {
        // Step 5.a.iii.1. Assert: module imports a specific binding for this
        //                 export.
        // Step 5.a.iii.2. Return ?
        // importedModule.ResolveExport(e.[[ImportName]],
        //                 resolveSet).
        name = e->importName();
        return ModuleResolveExport(cx, importedModule, name, resolveSet,
                                   result);
      }
    }
  }

  // Step 6. If SameValue(exportName, "default") is true, then:
  if (exportName == cx->names().default_) {
    // Step 6.a. Assert: A default export was not explicitly defined by this
    //           module.
    // Step 6.b. Return null.
    // Step 6.c. NOTE: A default export cannot be provided by an export * from
    //           "mod" declaration.
    result.setNull();
    return true;
  }

  // Step 7. Let starResolution be null.
  Rooted<ResolvedBindingObject*> starResolution(cx);

  // Step 8. For each ExportEntry Record e of module.[[StarExportEntries]], do:
  Rooted<ArrayObject*> starExportEntries(cx, &module->starExportEntries());
  Rooted<Value> resolution(cx);
  Rooted<ResolvedBindingObject*> binding(cx);
  for (uint32_t i = 0; i != starExportEntries->length(); i++) {
    e = &starExportEntries->getDenseElement(i)
             .toObject()
             .as<ExportEntryObject>();
    // Step 8.a. Let importedModule be ? HostResolveImportedModule(module,
    //           e.[[ModuleRequest]]).
    moduleRequest = e->moduleRequest();
    importedModule = HostResolveImportedModule(cx, module, moduleRequest,
                                               MODULE_STATUS_UNLINKED);
    if (!importedModule) {
      return false;
    }

    // Step 8.b. Let resolution be ? importedModule.ResolveExport(exportName,
    //           resolveSet).
    if (!ModuleResolveExport(cx, importedModule, exportName, resolveSet,
                             &resolution)) {
      return false;
    }

    // Step 8.c. If resolution is ambiguous, return ambiguous.
    if (resolution == StringValue(cx->names().ambiguous)) {
      result.set(resolution);
      return true;
    }

    // Step 8.d. If resolution is not null, then:
    if (!resolution.isNull()) {
      // Step 8.d.i. Assert: resolution is a ResolvedBinding Record.
      binding = &resolution.toObject().as<ResolvedBindingObject>();

      // Step 8.d.ii. If starResolution is null, set starResolution to
      // resolution.
      if (!starResolution) {
        starResolution = binding;
      } else {
        // Step 8.d.iii. Else:
        // Step 8.d.iii.1. Assert: There is more than one * import that includes
        //                 the requested name.
        // Step 8.d.iii.2. If resolution.[[Module]] and
        //                 starResolution.[[Module]] are not the same Module
        //                 Record, return ambiguous.
        // Step 8.d.iii.3. If resolution.[[BindingName]] is namespace and
        //                 starResolution.[[BindingName]] is not namespace, or
        //                 if resolution.[[BindingName]] is not namespace and
        //                 starResolution.[[BindingName]] is namespace, return
        //                 ambiguous.
        // Step 8.d.iii.4. If resolution.[[BindingName]] is a String,
        //                 starResolution.[[BindingName]] is a String, and
        //                 SameValue(resolution.[[BindingName]],
        //                 starResolution.[[BindingName]]) is false, return
        //                 ambiguous.
        if (binding->module() != starResolution->module() ||
            binding->bindingName() != starResolution->bindingName()) {
          result.set(StringValue(cx->names().ambiguous));
          return true;
        }
      }
    }
  }

  // Step 9. Return starResolution.
  result.setObjectOrNull(starResolution);
  return true;
}

static void EnsureModuleEnvironmentNamespace(
    JSContext* cx, Handle<ModuleObject*> module,
    Handle<ModuleNamespaceObject*> ns) {
  Rooted<ModuleEnvironmentObject*> environment(cx,
                                               &module->initialEnvironment());
  // The property already exists in the evironment but is not writable, so set
  // the slot directly.
  mozilla::Maybe<PropertyInfo> prop =
      environment->lookup(cx, cx->names().starNamespaceStar);
  MOZ_ASSERT(prop.isSome());
  environment->setSlot(prop->slot(), ObjectValue(*ns));
}

// https://tc39.es/ecma262/#sec-getmodulenamespace
// ES2023 16.2.1.10 GetModuleNamespace
ModuleNamespaceObject* js::GetOrCreateModuleNamespace(
    JSContext* cx, Handle<ModuleObject*> module) {
  // Step 1. Assert: If module is a Cyclic Module Record, then module.[[Status]]
  //         is not unlinked.
  MOZ_ASSERT(module->status() != MODULE_STATUS_UNLINKED);

  // Step 2. Let namespace be module.[[Namespace]].
  Rooted<ModuleNamespaceObject*> ns(cx, module->namespace_());

  // Step 3. If namespace is empty, then:
  if (!ns) {
    // Step 3.a. Let exportedNames be ? module.GetExportedNames().
    Rooted<ModuleSet> exportStarSet(cx);
    Rooted<ArrayObject*> exportedNames(cx);
    exportedNames = ModuleGetExportedNames(cx, module, &exportStarSet);
    if (!exportedNames) {
      return nullptr;
    }

    // Step 3.b. Let unambiguousNames be a new empty List.
    Rooted<ArrayObject*> unambiguousNames(cx, NewList(cx));
    if (!unambiguousNames) {
      return nullptr;
    }

    // Step 3.c. For each element name of exportedNames, do:
    Rooted<JSAtom*> name(cx);
    Rooted<Value> resolution(cx);
    for (uint32_t i = 0; i != exportedNames->length(); i++) {
      name = &exportedNames->getDenseElement(i).toString()->asAtom();

      // Step 3.c.i. Let resolution be ? module.ResolveExport(name).
      if (!ModuleResolveExport(cx, module, name, &resolution)) {
        return nullptr;
      }

      // Step 3.c.ii. If resolution is a ResolvedBinding Record, append name to
      //              unambiguousNames.
      if (resolution.isObject() &&
          !NewbornArrayPush(cx, unambiguousNames, StringValue(name))) {
        return nullptr;
      }
    }

    // Step 3.d. Set namespace to ModuleNamespaceCreate(module,
    //           unambiguousNames).
    ns = ModuleNamespaceCreate(cx, module, unambiguousNames);
  }

  // Step 4. Return namespace.
  return ns;
}

static bool IsResolvedBinding(JSContext* cx, Handle<Value> resolution) {
  MOZ_ASSERT(resolution.isObjectOrNull() ||
             resolution.toString() == cx->names().ambiguous);
  return resolution.isObject();
}

// https://tc39.es/ecma262/#sec-modulenamespacecreate
// ES2023 10.4.6.12 ModuleNamespaceCreate
static ModuleNamespaceObject* ModuleNamespaceCreate(
    JSContext* cx, Handle<ModuleObject*> module, Handle<ArrayObject*> exports) {
  // Step 1. Assert: module.[[Namespace]] is empty.
  MOZ_ASSERT(!module->namespace_());

  // Steps 2 - 5.
  Rooted<ModuleNamespaceObject*> ns(
      cx, ModuleObject::createNamespace(cx, module, exports));
  if (!ns) {
    return nullptr;
  }

  // Step 6. Let sortedExports be a List whose elements are the elements of
  //         exports ordered as if an Array of the same values had been sorted
  //         using %Array.prototype.sort% using undefined as comparefn.
  if (!ArrayNativeSort(cx, exports)) {
    return nullptr;
  }

  // Pre-compute all binding mappings now instead of on each access.
  // See:
  // https://tc39.es/ecma262/#sec-module-namespace-exotic-objects-get-p-receiver
  // ES2023 10.4.6.8 Module Namespace Exotic Object [[Get]]
  Rooted<JSAtom*> name(cx);
  Rooted<Value> resolution(cx);
  Rooted<ResolvedBindingObject*> binding(cx);
  Rooted<ModuleObject*> importedModule(cx);
  Rooted<ModuleNamespaceObject*> importedNamespace(cx);
  Rooted<JSAtom*> bindingName(cx);
  for (uint32_t i = 0; i != exports->length(); i++) {
    name = &exports->getDenseElement(i).toString()->asAtom();

    if (!ModuleResolveExport(cx, module, name, &resolution)) {
      return nullptr;
    }

    MOZ_ASSERT(IsResolvedBinding(cx, resolution));
    binding = &resolution.toObject().as<ResolvedBindingObject>();
    importedModule = binding->module();
    if (binding->bindingName() == cx->names().starNamespaceStar) {
      importedNamespace = GetOrCreateModuleNamespace(cx, importedModule);
      if (!importedNamespace) {
        return nullptr;
      }

      // The spec uses an immutable binding here but we have already generated
      // bytecode for an indirect binding. Instead, use an indirect binding to
      // "*namespace*" slot of the target environment.
      EnsureModuleEnvironmentNamespace(cx, importedModule, importedNamespace);
    }

    bindingName = binding->bindingName();
    if (!ns->addBinding(cx, name, importedModule, bindingName)) {
      return nullptr;
    }
  }

  // Step 10. Return M.
  return ns;
}

static void ThrowResolutionError(JSContext* cx, Handle<ModuleObject*> module,
                                 Handle<Value> resolution, bool isDirectImport,
                                 Handle<JSAtom*> name, uint32_t line,
                                 uint32_t column) {
  MOZ_ASSERT(line != 0);

  bool isAmbiguous = resolution == StringValue(cx->names().ambiguous);

  static constexpr unsigned ErrorNumbers[2][2] = {
      {JSMSG_AMBIGUOUS_IMPORT, JSMSG_MISSING_IMPORT},
      {JSMSG_AMBIGUOUS_INDIRECT_EXPORT, JSMSG_MISSING_INDIRECT_EXPORT}};
  unsigned errorNumber = ErrorNumbers[isDirectImport][isAmbiguous];

  const JSErrorFormatString* errorString =
      GetErrorMessage(nullptr, errorNumber);
  MOZ_ASSERT(errorString);

  MOZ_ASSERT(errorString->argCount == 0);
  Rooted<JSString*> message(cx, JS_NewStringCopyZ(cx, errorString->format));
  if (!message) {
    return;
  }

  Rooted<JSString*> separator(cx, JS_NewStringCopyZ(cx, ": "));
  if (!separator) {
    return;
  }

  message = ConcatStrings<CanGC>(cx, message, separator);
  if (!message) {
    return;
  }

  message = ConcatStrings<CanGC>(cx, message, name);
  if (!message) {
    return;
  }

  RootedString filename(cx);
  filename = JS_NewStringCopyZ(cx, module->script()->filename());
  if (!filename) {
    return;
  }

  RootedValue error(cx);
  if (!JS::CreateError(cx, JSEXN_SYNTAXERR, nullptr, filename, line, column,
                       nullptr, message, JS::NothingHandleValue, &error)) {
    return;
  }

  cx->setPendingException(error, nullptr);
}

static void InitNamespaceBinding(JSContext* cx,
                                 Handle<ModuleEnvironmentObject*> env,
                                 Handle<JSAtom*> name,
                                 Handle<ModuleNamespaceObject*> ns) {
  // The property already exists in the evironment but is not writable, so set
  // the slot directly.
  RootedId id(cx, AtomToId(name));
  mozilla::Maybe<PropertyInfo> prop = env->lookup(cx, id);
  MOZ_ASSERT(prop.isSome());
  env->setSlot(prop->slot(), ObjectValue(*ns));
}

// https://tc39.es/ecma262/#sec-source-text-module-record-initialize-environment
// ES2023 16.2.1.6.4 InitializeEnvironment
bool js::ModuleInitializeEnvironment(JSContext* cx,
                                     Handle<ModuleObject*> module) {
  MOZ_ASSERT(module->status() == MODULE_STATUS_LINKING);

  // Step 1. For each ExportEntry Record e of module.[[IndirectExportEntries]],
  //         do:
  Rooted<ArrayObject*> indirectExportEntries(cx,
                                             &module->indirectExportEntries());
  Rooted<ExportEntryObject*> e(cx);
  Rooted<JSAtom*> exportName(cx);
  Rooted<Value> resolution(cx);
  for (uint32_t i = 0; i != indirectExportEntries->length(); i++) {
    e = &indirectExportEntries->getDenseElement(i)
             .toObject()
             .as<ExportEntryObject>();

    // Step 1.a. Let resolution be ? module.ResolveExport(e.[[ExportName]]).
    exportName = e->exportName();
    if (!ModuleResolveExport(cx, module, exportName, &resolution)) {
      return false;
    }

    // Step 1.b. If resolution is null or ambiguous, throw a SyntaxError
    //           exception.
    if (!IsResolvedBinding(cx, resolution)) {
      ThrowResolutionError(cx, module, resolution, false, exportName,
                           e->lineNumber(), e->columnNumber());
      return false;
    }
  }

  // Step 5. Let env be NewModuleEnvironment(realm.[[GlobalEnv]]).
  // Step 6. Set module.[[Environment]] to env.
  // Note that we have already created the environment by this point.
  Rooted<ModuleEnvironmentObject*> env(cx, &module->initialEnvironment());

  // Step 7. For each ImportEntry Record in of module.[[ImportEntries]], do:
  Rooted<ArrayObject*> importEntries(cx, &module->importEntries());
  Rooted<ImportEntryObject*> in(cx);
  Rooted<ModuleRequestObject*> moduleRequest(cx);
  Rooted<ModuleObject*> importedModule(cx);
  Rooted<JSAtom*> importName(cx);
  Rooted<JSAtom*> localName(cx);
  Rooted<ModuleObject*> sourceModule(cx);
  Rooted<JSAtom*> bindingName(cx);
  for (uint32_t i = 0; i != importEntries->length(); i++) {
    in = &importEntries->getDenseElement(i).toObject().as<ImportEntryObject>();

    // Step 7.a. Let importedModule be ! HostResolveImportedModule(module,
    //           in.[[ModuleRequest]]).
    moduleRequest = in->moduleRequest();
    importedModule = HostResolveImportedModule(cx, module, moduleRequest,
                                               MODULE_STATUS_LINKING);
    if (!importedModule) {
      return false;
    }

    localName = in->localName();
    importName = in->importName();

    // Step 7.c. If in.[[ImportName]] is namespace-object, then:
    if (!importName) {
      // Step 7.c.i. Let namespace be ? GetModuleNamespace(importedModule).
      Rooted<ModuleNamespaceObject*> ns(
          cx, GetOrCreateModuleNamespace(cx, importedModule));
      if (!ns) {
        return false;
      }

      // Step 7.c.ii. Perform ! env.CreateImmutableBinding(in.[[LocalName]],
      // true). This happens when the environment is created.

      // Step 7.c.iii. Perform ! env.InitializeBinding(in.[[LocalName]],
      // namespace).
      InitNamespaceBinding(cx, env, localName, ns);
    } else {
      // Step 7.d. Else:
      // Step 7.d.i. Let resolution be ?
      // importedModule.ResolveExport(in.[[ImportName]]).
      if (!ModuleResolveExport(cx, importedModule, importName, &resolution)) {
        return false;
      }

      // Step 7.d.ii. If resolution is null or ambiguous, throw a SyntaxError
      //              exception.
      if (!IsResolvedBinding(cx, resolution)) {
        ThrowResolutionError(cx, module, resolution, true, importName,
                             in->lineNumber(), in->columnNumber());
        return false;
      }

      auto* binding = &resolution.toObject().as<ResolvedBindingObject>();
      sourceModule = binding->module();
      bindingName = binding->bindingName();

      // Step 7.d.iii. If resolution.[[BindingName]] is namespace, then:
      if (bindingName == cx->names().starNamespaceStar) {
        // Step 7.d.iii.1. Let namespace be ?
        //                 GetModuleNamespace(resolution.[[Module]]).
        Rooted<ModuleNamespaceObject*> ns(
            cx, GetOrCreateModuleNamespace(cx, sourceModule));
        if (!ns) {
          return false;
        }

        // Step 7.d.iii.2. Perform !
        //                 env.CreateImmutableBinding(in.[[LocalName]], true).
        // Step 7.d.iii.3. Perform ! env.InitializeBinding(in.[[LocalName]],
        //                 namespace).
        //
        // This should be InitNamespaceBinding, but we have already generated
        // bytecode assuming an indirect binding. Instead, ensure a special
        // "*namespace*"" binding exists on the target module's environment. We
        // then generate an indirect binding to this synthetic binding.
        EnsureModuleEnvironmentNamespace(cx, sourceModule, ns);
        if (!env->createImportBinding(cx, localName, sourceModule,
                                      bindingName)) {
          return false;
        }
      } else {
        // Step 7.d.iv. Else:
        // Step 7.d.iv.1. 1. Perform env.CreateImportBinding(in.[[LocalName]],
        //                   resolution.[[Module]], resolution.[[BindingName]]).
        if (!env->createImportBinding(cx, localName, sourceModule,
                                      bindingName)) {
          return false;
        }
      }
    }
  }

  // Steps 8-26.
  //
  // Some of these do not need to happen for practical purposes. For steps
  // 21-23, the bindings that can be handled in a similar way to regulars
  // scripts are done separately. Function Declarations are special due to
  // hoisting and are handled within this function. See ModuleScope and
  // ModuleEnvironmentObject for further details.

  // Step 24. For each element d of lexDeclarations, do:
  // Step 24.a. For each element dn of the BoundNames of d, do:
  // Step 24.a.iii. If d is a FunctionDeclaration, a GeneratorDeclaration, an
  //                AsyncFunctionDeclaration, or an AsyncGeneratorDeclaration,
  //                then:
  // Step 24.a.iii.1 Let fo be InstantiateFunctionObject of d with arguments env
  //                 and privateEnv.
  // Step 24.a.iii.2. Perform ! env.InitializeBinding(dn, fo).
  return ModuleObject::instantiateFunctionDeclarations(cx, module);
}

// https://tc39.es/ecma262/#sec-moduledeclarationlinking
// ES2023 16.2.1.5.1 Link
bool js::ModuleInstantiate(JSContext* cx, Handle<ModuleObject*> module) {
  // Step 1. Assert: module.[[Status]] is not linking or evaluating.
  MOZ_ASSERT(module->status() != MODULE_STATUS_LINKING &&
             module->status() != MODULE_STATUS_EVALUATING);

  // Step 2. Let stack be a new empty List.
  Rooted<ModuleVector> stack(cx);

  // Step 3. Let result be Completion(InnerModuleLinking(module, stack, 0)).
  size_t ignored;
  bool ok = InnerModuleLinking(cx, module, &stack, 0, &ignored);

  // Step 4. If result is an abrupt completion, then:
  if (!ok) {
    // Step 4.a. For each Cyclic Module Record m of stack, do:
    for (ModuleObject* m : stack) {
      // Step 4.a.i. Assert: m.[[Status]] is linking.
      MOZ_ASSERT(m->status() == MODULE_STATUS_LINKING);
      // Step 4.a.ii. Set m.[[Status]] to unlinked.
      m->setStatus(MODULE_STATUS_UNLINKED);
      m->clearDfsIndexes();
    }

    // Step 4.b. Assert: module.[[Status]] is unlinked.
    MOZ_ASSERT(module->status() == MODULE_STATUS_UNLINKED);

    // Step 4.c.
    return false;
  }

  // Step 5. Assert: module.[[Status]] is linked, evaluating-async, or
  //         evaluated.
  MOZ_ASSERT(module->status() == MODULE_STATUS_LINKED ||
             module->status() == MODULE_STATUS_EVALUATED ||
             module->status() == MODULE_STATUS_EVALUATED_ERROR);

  // Step 6. Assert: stack is empty.
  MOZ_ASSERT(stack.empty());

  // Step 7. Return unused.
  return true;
}

// https://tc39.es/ecma262/#sec-InnerModuleLinking
// ES2023 16.2.1.5.1.1 InnerModuleLinking
static bool InnerModuleLinking(JSContext* cx, Handle<ModuleObject*> module,
                               MutableHandle<ModuleVector> stack, size_t index,
                               size_t* indexOut) {
  // Step 2. If module.[[Status]] is linking, linked, evaluating-async, or
  //         evaluated, then:
  if (module->status() == MODULE_STATUS_LINKING ||
      module->status() == MODULE_STATUS_LINKED ||
      module->status() == MODULE_STATUS_EVALUATED ||
      module->status() == MODULE_STATUS_EVALUATED_ERROR) {
    // Step 2.a. Return index.
    *indexOut = index;
    return true;
  }

  // Step 3. Assert: module.[[Status]] is unlinked.
  if (module->status() != MODULE_STATUS_UNLINKED) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_BAD_MODULE_STATUS);
    return false;
  }

  // Step 8. Append module to stack.
  // Do this before changing the status so that we can recover on failure.
  if (!stack.append(module)) {
    ReportOutOfMemory(cx);
    return false;
  }

  // Step 4. Set module.[[Status]] to linking.
  module->setStatus(MODULE_STATUS_LINKING);

  // Step 5. Set module.[[DFSIndex]] to index.
  module->setDfsIndex(index);

  // Step 6. Set module.[[DFSAncestorIndex]] to index.
  module->setDfsAncestorIndex(index);

  // Step 7. Set index to index + 1.
  index++;

  // Step 9. For each String required that is an element of
  //         module.[[RequestedModules]], do:
  Rooted<ArrayObject*> requestedModules(cx, &module->requestedModules());
  Rooted<ModuleRequestObject*> moduleRequest(cx);
  Rooted<ModuleObject*> requiredModule(cx);
  for (uint32_t i = 0; i != requestedModules->length(); i++) {
    moduleRequest = requestedModules->getDenseElement(i)
                        .toObject()
                        .as<RequestedModuleObject>()
                        .moduleRequest();

    // Step 9.a. Let requiredModule be ? HostResolveImportedModule(module,
    //           required).
    requiredModule = HostResolveImportedModule(cx, module, moduleRequest,
                                               MODULE_STATUS_UNLINKED);
    if (!requiredModule) {
      return false;
    }

    // Step 9.b. Set index to ? InnerModuleLinking(requiredModule, stack,
    //           index).
    if (!InnerModuleLinking(cx, requiredModule, stack, index, &index)) {
      return false;
    }

    // Step 9.c. If requiredModule is a Cyclic Module Record, then:
    // Step 9.c.i. Assert: requiredModule.[[Status]] is either linking, linked,
    //             evaluating-async, or evaluated.
    MOZ_ASSERT(requiredModule->status() == MODULE_STATUS_LINKING ||
               requiredModule->status() == MODULE_STATUS_LINKED ||
               requiredModule->status() == MODULE_STATUS_EVALUATED ||
               requiredModule->status() == MODULE_STATUS_EVALUATED_ERROR);

    // Step 9.c.ii. Assert: requiredModule.[[Status]] is linking if and only if
    //              requiredModule is in stack.
    MOZ_ASSERT((requiredModule->status() == MODULE_STATUS_LINKING) ==
               ContainsElement(stack, requiredModule));

    // Step 9.c.iii. If requiredModule.[[Status]] is linking, then:
    if (requiredModule->status() == MODULE_STATUS_LINKING) {
      // Step 9.c.iii.1. Set module.[[DFSAncestorIndex]] to
      //                 min(module.[[DFSAncestorIndex]],
      //                 requiredModule.[[DFSAncestorIndex]]).
      module->setDfsAncestorIndex(std::min(module->dfsAncestorIndex(),
                                           requiredModule->dfsAncestorIndex()));
    }
  }

  // Step 10. Perform ? module.InitializeEnvironment().
  if (!ModuleInitializeEnvironment(cx, module)) {
    return false;
  }

  // Step 11. Assert: module occurs exactly once in stack.
  MOZ_ASSERT(CountElements(stack, module) == 1);

  // Step 12. Assert: module.[[DFSAncestorIndex]] <= module.[[DFSIndex]].
  MOZ_ASSERT(module->dfsAncestorIndex() <= module->dfsIndex());

  // Step 13. If module.[[DFSAncestorIndex]] = module.[[DFSIndex]], then
  if (module->dfsAncestorIndex() == module->dfsIndex()) {
    // Step 13.a.
    bool done = false;

    // Step 13.b. Repeat, while done is false:
    while (!done) {
      // Step 13.b.i. Let requiredModule be the last element in stack.
      // Step 13.b.ii. Remove the last element of stack.
      requiredModule = stack.popCopy();

      // Step 13.b.iv. Set requiredModule.[[Status]] to linked.
      requiredModule->setStatus(MODULE_STATUS_LINKED);

      // Step 13.b.v. If requiredModule and module are the same Module Record,
      //              set done to true.
      done = requiredModule == module;
    }
  }

  // Step 14. Return index.
  *indexOut = index;
  return true;
}
