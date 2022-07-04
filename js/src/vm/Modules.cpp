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

  return ModuleObject::Instantiate(cx, moduleArg.as<ModuleObject>());
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

  return ModuleObject::GetOrCreateModuleNamespace(
      cx, moduleRecord.as<ModuleObject>());
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

static ModuleObject* HostResolveImportedModule(
    JSContext* cx, Handle<ModuleObject*> module,
    Handle<ModuleRequestObject*> moduleRequest,
    ModuleStatus expectedMinimumStatus);
static bool ModuleResolveExport(JSContext* cx, Handle<ModuleObject*> module,
                                Handle<JSAtom*> exportName,
                                MutableHandle<ResolveSet> resolveSet,
                                MutableHandle<Value> result);

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

// https://tc39.es/ecma262/#sec-getexportednames
// ES2023 16.2.1.6.2 GetExportedNames
ArrayObject* js::ModuleGetExportedNames(
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
