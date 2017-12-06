/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function CallModuleResolveHook(module, specifier, expectedMinimumStatus)
{
    let requestedModule = HostResolveImportedModule(module, specifier);
    if (requestedModule.status < expectedMinimumStatus)
        ThrowInternalError(JSMSG_BAD_MODULE_STATUS);

    return requestedModule;
}

// 15.2.1.16.2 GetExportedNames(exportStarSet)
function ModuleGetExportedNames(exportStarSet = [])
{
    if (!IsObject(this) || !IsModule(this)) {
        return callFunction(CallModuleMethodIfWrapped, this, exportStarSet,
                            "ModuleGetExportedNames");
    }

    // Step 1
    let module = this;

    // Step 2
    if (callFunction(ArrayIncludes, exportStarSet, module))
        return [];

    // Step 3
    _DefineDataProperty(exportStarSet, exportStarSet.length, module);

    // Step 4
    let exportedNames = [];
    let namesCount = 0;

    // Step 5
    let localExportEntries = module.localExportEntries;
    for (let i = 0; i < localExportEntries.length; i++) {
        let e = localExportEntries[i];
        _DefineDataProperty(exportedNames, namesCount++, e.exportName);
    }

    // Step 6
    let indirectExportEntries = module.indirectExportEntries;
    for (let i = 0; i < indirectExportEntries.length; i++) {
        let e = indirectExportEntries[i];
        _DefineDataProperty(exportedNames, namesCount++, e.exportName);
    }

    // Step 7
    let starExportEntries = module.starExportEntries;
    for (let i = 0; i < starExportEntries.length; i++) {
        let e = starExportEntries[i];
        let requestedModule = CallModuleResolveHook(module, e.moduleRequest,
                                                    MODULE_STATUS_INSTANTIATING);
        let starNames = callFunction(requestedModule.getExportedNames, requestedModule,
                                     exportStarSet);
        for (let j = 0; j < starNames.length; j++) {
            let n = starNames[j];
            if (n !== "default" && !callFunction(ArrayIncludes, exportedNames, n))
                _DefineDataProperty(exportedNames, namesCount++, n);
        }
    }

    return exportedNames;
}

function ModuleSetStatus(module, newStatus)
{
    assert(newStatus >= MODULE_STATUS_ERRORED && newStatus <= MODULE_STATUS_EVALUATED,
           "Bad new module status in ModuleSetStatus");
    if (newStatus !== MODULE_STATUS_ERRORED)
        assert(newStatus > module.status, "New module status inconsistent with current status");

    UnsafeSetReservedSlot(module, MODULE_OBJECT_STATUS_SLOT, newStatus);
}

// 15.2.1.16.3 ResolveExport(exportName, resolveSet)
//
// Returns an object describing the location of the resolved export or
// indicating a failure.
//
// On success this returns: { resolved: true, module, bindingName }
//
// There are three failure cases:
//
//  - The resolution failure can be blamed on a particular module.
//    Returns: { resolved: false, module, ambiguous: false }
//
//  - No culprit can be determined and the resolution failure was due to star
//    export ambiguity.
//    Returns: { resolved: false, module: null, ambiguous: true }
//
//  - No culprit can be determined and the resolution failure was not due to
//    star export ambiguity.
//    Returns: { resolved: false, module: null, ambiguous: false }
//
function ModuleResolveExport(exportName, resolveSet = [])
{
    if (!IsObject(this) || !IsModule(this)) {
        return callFunction(CallModuleMethodIfWrapped, this, exportName, resolveSet,
                            "ModuleResolveExport");
    }

    // Step 1
    let module = this;

    // Step 2
    assert(module.status !== MODULE_STATUS_ERRORED, "Bad module state in ResolveExport");

    // Step 3
    for (let i = 0; i < resolveSet.length; i++) {
        let r = resolveSet[i];
        if (r.module === module && r.exportName === exportName) {
            // This is a circular import request.
            return {resolved: false, module: null, ambiguous: false};
        }
    }

    // Step 4
    _DefineDataProperty(resolveSet, resolveSet.length, {module, exportName});

    // Step 5
    let localExportEntries = module.localExportEntries;
    for (let i = 0; i < localExportEntries.length; i++) {
        let e = localExportEntries[i];
        if (exportName === e.exportName)
            return {resolved: true, module, bindingName: e.localName};
    }

    // Step 6
    let indirectExportEntries = module.indirectExportEntries;
    for (let i = 0; i < indirectExportEntries.length; i++) {
        let e = indirectExportEntries[i];
        if (exportName === e.exportName) {
            let importedModule = CallModuleResolveHook(module, e.moduleRequest,
                                                       MODULE_STATUS_UNINSTANTIATED);
            let resolution = callFunction(importedModule.resolveExport, importedModule, e.importName,
                                          resolveSet);
            if (!resolution.resolved && !resolution.module)
                resolution.module = module;
            return resolution;
        }
    }

    // Step 7
    if (exportName === "default") {
        // A default export cannot be provided by an export *.
        return {resolved: false, module: null, ambiguous: false};
    }

    // Step 8
    let starResolution = null;

    // Step 9
    let starExportEntries = module.starExportEntries;
    for (let i = 0; i < starExportEntries.length; i++) {
        let e = starExportEntries[i];
        let importedModule = CallModuleResolveHook(module, e.moduleRequest,
                                                   MODULE_STATUS_UNINSTANTIATED);
        let resolution = callFunction(importedModule.resolveExport, importedModule, exportName,
                                      resolveSet);
        if (!resolution.resolved && (resolution.module || resolution.ambiguous))
            return resolution;
        if (resolution.resolved) {
            if (starResolution === null) {
                starResolution = resolution;
            } else {
                if (resolution.module !== starResolution.module ||
                    resolution.bindingName !== starResolution.bindingName)
                {
                    return {resolved: false, module: null, ambiguous: true};
                }
            }
        }
    }

    // Step 10
    if (starResolution !== null)
        return starResolution;

    return {resolved: false, module: null, ambiguous: false};
}

// 15.2.1.18 GetModuleNamespace(module)
function GetModuleNamespace(module)
{
    // Step 1
    assert(IsModule(module), "GetModuleNamespace called with non-module");

    // Step 2
    assert(module.status !== MODULE_STATUS_UNINSTANTIATED &&
           module.status !== MODULE_STATUS_ERRORED,
           "Bad module state in GetModuleNamespace");

    // Step 3
    let namespace = module.namespace;

    // Step 3
    if (typeof namespace === "undefined") {
        let exportedNames = callFunction(module.getExportedNames, module);
        let unambiguousNames = [];
        for (let i = 0; i < exportedNames.length; i++) {
            let name = exportedNames[i];
            let resolution = callFunction(module.resolveExport, module, name);
            if (resolution.resolved)
                _DefineDataProperty(unambiguousNames, unambiguousNames.length, name);
        }
        namespace = ModuleNamespaceCreate(module, unambiguousNames);
    }

    // Step 4
    return namespace;
}

// 9.4.6.13 ModuleNamespaceCreate(module, exports)
function ModuleNamespaceCreate(module, exports)
{
    callFunction(ArraySort, exports);

    let ns = NewModuleNamespace(module, exports);

    // Pre-compute all bindings now rather than calling ResolveExport() on every
    // access.
    for (let i = 0; i < exports.length; i++) {
        let name = exports[i];
        let binding = callFunction(module.resolveExport, module, name);
        assert(binding.resolved, "Failed to resolve binding");
        AddModuleNamespaceBinding(ns, name, binding.module, binding.bindingName);
    }

    return ns;
}

function GetModuleEnvironment(module)
{
    assert(IsModule(module), "Non-module passed to GetModuleEnvironment");

    // Check for a previous failed attempt to instantiate this module. This can
    // only happen due to a bug in the module loader.
    if (module.status === MODULE_STATUS_ERRORED)
        ThrowInternalError(JSMSG_MODULE_INSTANTIATE_FAILED, module.status);

    assert(module.status >= MODULE_STATUS_INSTANTIATING,
           "Attempt to access module environement before instantation");

    let env = UnsafeGetReservedSlot(module, MODULE_OBJECT_ENVIRONMENT_SLOT);
    assert(IsModuleEnvironment(env),
           "Module environment slot contains unexpected value");

    return env;
}

function RecordModuleError(module, error)
{
    // Set the module's status to 'errored' to indicate a failed module
    // instantiation and record the exception. The environment slot is also
    // reset to 'undefined'.

    assert(IsObject(module) && IsModule(module), "Non-module passed to RecordModuleError");

    ModuleSetStatus(module, MODULE_STATUS_ERRORED);
    UnsafeSetReservedSlot(module, MODULE_OBJECT_ERROR_SLOT, error);
    UnsafeSetReservedSlot(module, MODULE_OBJECT_ENVIRONMENT_SLOT, undefined);
}

function CountArrayValues(array, value)
{
    let count = 0;
    for (let i = 0; i < array.length; i++) {
        if (array[i] === value)
            count++;
    }
    return count;
}

function ArrayContains(array, value)
{
    for (let i = 0; i < array.length; i++) {
        if (array[i] === value)
            return true;
    }
    return false;
}

// 15.2.1.16.4 ModuleInstantiate()
function ModuleInstantiate()
{
    if (!IsObject(this) || !IsModule(this))
        return callFunction(CallModuleMethodIfWrapped, this, "ModuleInstantiate");

    // Step 1
    let module = this;

    // Step 2
    if (module.status === MODULE_STATUS_INSTANTIATING ||
        module.status === MODULE_STATUS_EVALUATING)
    {
        ThrowInternalError(JSMSG_BAD_MODULE_STATUS);
    }

    // Step 3
    let stack = [];

    // Steps 4-5
    try {
        InnerModuleDeclarationInstantiation(module, stack, 0);
    } catch (error) {
        for (let i = 0; i < stack.length; i++) {
            let m = stack[i];

            assert(m.status === MODULE_STATUS_INSTANTIATING ||
                   m.status === MODULE_STATUS_ERRORED,
                   "Bad module status after failed instantiation");

            RecordModuleError(m, error);
        }

        if (stack.length === 0 &&
            typeof(UnsafeGetReservedSlot(module, MODULE_OBJECT_ERROR_SLOT)) === "undefined")
        {
            // This can happen due to OOM when appending to the stack or
            // over-recursion errors.
            RecordModuleError(module, error);
        }

        assert(module.status === MODULE_STATUS_ERRORED,
               "Bad module status after failed instantiation");
        assert(SameValue(UnsafeGetReservedSlot(module, MODULE_OBJECT_ERROR_SLOT), error),
               "Module has different error set after failed instantiation");

        throw error;
    }

    // Step 6
    assert(module.status == MODULE_STATUS_INSTANTIATED ||
           module.status == MODULE_STATUS_EVALUATED,
           "Bad module status after successful instantiation");

    // Step 7
    assert(stack.length === 0,
           "Stack should be empty after successful instantiation");

    // Step 8
    return undefined;
}
_SetCanonicalName(ModuleInstantiate, "ModuleInstantiate");

// 15.2.1.16.4.1 InnerModuleDeclarationInstantiation(module, stack, index)
function InnerModuleDeclarationInstantiation(module, stack, index)
{
    // Step 1
    // TODO: Support module records other than source text module records.

    // Step 2
    if (module.status === MODULE_STATUS_INSTANTIATING ||
        module.status === MODULE_STATUS_INSTANTIATED ||
        module.status === MODULE_STATUS_EVALUATED)
    {
        return index;
    }

    // Step 3
    if (module.status === MODULE_STATUS_ERRORED)
        throw module.error;

    // Step 4
    assert(module.status === MODULE_STATUS_UNINSTANTIATED,
          "Bad module status in ModuleDeclarationInstantiation");

    // Steps 5
    ModuleSetStatus(module, MODULE_STATUS_INSTANTIATING);

    // Step 6-8
    UnsafeSetReservedSlot(module, MODULE_OBJECT_DFS_INDEX_SLOT, index);
    UnsafeSetReservedSlot(module, MODULE_OBJECT_DFS_ANCESTOR_INDEX_SLOT, index);
    index++;

    // Step 9
    _DefineDataProperty(stack, stack.length, module);

    // Step 10
    let requestedModules = module.requestedModules;
    for (let i = 0; i < requestedModules.length; i++) {
        let required = requestedModules[i].moduleSpecifier;
        let requiredModule = CallModuleResolveHook(module, required, MODULE_STATUS_ERRORED);

        index = InnerModuleDeclarationInstantiation(requiredModule, stack, index);

        assert(requiredModule.status === MODULE_STATUS_INSTANTIATING ||
               requiredModule.status === MODULE_STATUS_INSTANTIATED ||
               requiredModule.status === MODULE_STATUS_EVALUATED,
               "Bad required module status after InnerModuleDeclarationInstantiation");

        assert((requiredModule.status === MODULE_STATUS_INSTANTIATING) ===
               ArrayContains(stack, requiredModule),
              "Required module should be in the stack iff it is currently being instantiated");

        assert(typeof requiredModule.dfsIndex === "number", "Bad dfsIndex");
        assert(typeof requiredModule.dfsAncestorIndex === "number", "Bad dfsAncestorIndex");

        if (requiredModule.status === MODULE_STATUS_INSTANTIATING) {
            UnsafeSetReservedSlot(module, MODULE_OBJECT_DFS_ANCESTOR_INDEX_SLOT,
                                  std_Math_min(module.dfsAncestorIndex,
                                               requiredModule.dfsAncestorIndex));
        }
    }

    // Step 11
    ModuleDeclarationEnvironmentSetup(module);

    // Steps 12-13
    assert(CountArrayValues(stack, module) === 1,
           "Current module should appear exactly once in the stack");
    assert(module.dfsAncestorIndex <= module.dfsIndex,
           "Bad DFS ancestor index");

    // Step 14
    if (module.dfsAncestorIndex === module.dfsIndex) {
        let requiredModule;
        do {
            requiredModule = callFunction(std_Array_pop, stack);
            ModuleSetStatus(requiredModule, MODULE_STATUS_INSTANTIATED);
        } while (requiredModule !== module);
    }

    // Step 15
    return index;
}

// 15.2.1.16.4.2 ModuleDeclarationEnvironmentSetup(module)
function ModuleDeclarationEnvironmentSetup(module)
{
    // Step 1
    let indirectExportEntries = module.indirectExportEntries;
    for (let i = 0; i < indirectExportEntries.length; i++) {
        let e = indirectExportEntries[i];
        let resolution = callFunction(module.resolveExport, module, e.exportName);
        assert(resolution.resolved || resolution.module,
               "Unexpected failure to resolve export in ModuleDeclarationEnvironmentSetup");
        if (!resolution.resolved) {
            return ResolutionError(resolution, "indirectExport", e.exportName,
                                   e.lineNumber, e.columnNumber);
        }
    }

    // Steps 5-6
    // Note that we have already created the environment by this point.
    let env = GetModuleEnvironment(module);

    // Step 8
    let importEntries = module.importEntries;
    for (let i = 0; i < importEntries.length; i++) {
        let imp = importEntries[i];
        let importedModule = CallModuleResolveHook(module, imp.moduleRequest,
                                                   MODULE_STATUS_INSTANTIATING);
        if (imp.importName === "*") {
            let namespace = GetModuleNamespace(importedModule);
            CreateNamespaceBinding(env, imp.localName, namespace);
        } else {
            let resolution = callFunction(importedModule.resolveExport, importedModule,
                                          imp.importName);
            if (!resolution.resolved && !resolution.module)
                resolution.module = module;

            if (!resolution.resolved) {
                return ResolutionError(resolution, "import", imp.importName,
                                       imp.lineNumber, imp.columnNumber);
            }

            CreateImportBinding(env, imp.localName, resolution.module, resolution.bindingName);
        }
    }

    InstantiateModuleFunctionDeclarations(module);
}

// 15.2.1.16.4.3 ResolutionError(module)
function ResolutionError(resolution, kind, name, line, column)
{
    let module = resolution.module;
    assert(module !== null,
           "Null module passed to ResolutionError");

    assert(module.status === MODULE_STATUS_UNINSTANTIATED ||
           module.status === MODULE_STATUS_INSTANTIATING,
           "Unexpected module status in ResolutionError");

    assert(kind === "import" || kind === "indirectExport",
           "Unexpected kind in ResolutionError");

    assert(line > 0,
           "Line number should be present for all imports and indirect exports");

    let errorNumber;
    if (kind === "import") {
        errorNumber = resolution.ambiguous ? JSMSG_AMBIGUOUS_IMPORT
                                           : JSMSG_MISSING_IMPORT;
    } else {
        errorNumber = resolution.ambiguous ? JSMSG_AMBIGUOUS_INDIRECT_EXPORT
                                           : JSMSG_MISSING_INDIRECT_EXPORT;
    }

    let message = GetErrorMessage(errorNumber) + ": " + name;
    let error = CreateModuleSyntaxError(module, line, column, message);
    RecordModuleError(module, error);
    throw error;
}

// 15.2.1.16.5 ModuleEvaluate()
function ModuleEvaluate()
{
    if (!IsObject(this) || !IsModule(this))
        return callFunction(CallModuleMethodIfWrapped, this, "ModuleEvaluate");

    // Step 1
    let module = this;

    // Step 2
    if (module.status !== MODULE_STATUS_ERRORED &&
        module.status !== MODULE_STATUS_INSTANTIATED &&
        module.status !== MODULE_STATUS_EVALUATED)
    {
        ThrowInternalError(JSMSG_BAD_MODULE_STATUS);
    }

    // Step 3
    let stack = [];

    // Steps 4-5
    try {
        InnerModuleEvaluation(module, stack, 0);
    } catch (error) {
        for (let i = 0; i < stack.length; i++) {
            let m = stack[i];

            assert(m.status === MODULE_STATUS_EVALUATING,
                   "Bad module status after failed evaluation");

            RecordModuleError(m, error);
        }

        if (stack.length === 0 &&
            typeof(UnsafeGetReservedSlot(module, MODULE_OBJECT_ERROR_SLOT)) === "undefined")
        {
            // This can happen due to OOM when appending to the stack or
            // over-recursion errors.
            RecordModuleError(module, error);
        }

        assert(module.status === MODULE_STATUS_ERRORED,
               "Bad module status after failed evaluation");
        assert(SameValue(UnsafeGetReservedSlot(module, MODULE_OBJECT_ERROR_SLOT), error),
               "Module has different error set after failed evaluation");

        throw error;
    }

    assert(module.status == MODULE_STATUS_EVALUATED,
           "Bad module status after successful evaluation");
    assert(stack.length === 0,
           "Stack should be empty after successful evaluation");

    return undefined;
}
_SetCanonicalName(ModuleEvaluate, "ModuleEvaluate");

// 15.2.1.16.5.1 InnerModuleEvaluation(module, stack, index)
function InnerModuleEvaluation(module, stack, index)
{
    // Step 1
    // TODO: Support module records other than source text module records.

    // Step 2
    if (module.status === MODULE_STATUS_EVALUATING ||
        module.status === MODULE_STATUS_EVALUATED)
    {
        return index;
    }

    // Step 3
    if (module.status === MODULE_STATUS_ERRORED)
        throw module.error;

    // Step 4
    assert(module.status === MODULE_STATUS_INSTANTIATED,
          "Bad module status in ModuleEvaluation");

    // Step 5
    ModuleSetStatus(module, MODULE_STATUS_EVALUATING);

    // Steps 6-8
    UnsafeSetReservedSlot(module, MODULE_OBJECT_DFS_INDEX_SLOT, index);
    UnsafeSetReservedSlot(module, MODULE_OBJECT_DFS_ANCESTOR_INDEX_SLOT, index);
    index++;

    // Step 9
    _DefineDataProperty(stack, stack.length, module);

    // Step 10
    let requestedModules = module.requestedModules;
    for (let i = 0; i < requestedModules.length; i++) {
        let required = requestedModules[i].moduleSpecifier;
        let requiredModule =
            CallModuleResolveHook(module, required, MODULE_STATUS_INSTANTIATED);

        index = InnerModuleEvaluation(requiredModule, stack, index);

        assert(requiredModule.status == MODULE_STATUS_EVALUATING ||
               requiredModule.status == MODULE_STATUS_EVALUATED,
              "Bad module status after InnerModuleEvaluation");

        assert((requiredModule.status === MODULE_STATUS_EVALUATING) ===
               ArrayContains(stack, requiredModule),
               "Required module should be in the stack iff it is currently being evaluated");

        assert(typeof requiredModule.dfsIndex === "number", "Bad dfsIndex");
        assert(typeof requiredModule.dfsAncestorIndex === "number", "Bad dfsAncestorIndex");

        if (requiredModule.status === MODULE_STATUS_EVALUATING) {
            UnsafeSetReservedSlot(module, MODULE_OBJECT_DFS_ANCESTOR_INDEX_SLOT,
                                  std_Math_min(module.dfsAncestorIndex,
                                               requiredModule.dfsAncestorIndex));
        }
    }

    // Step 11
    ExecuteModule(module);

    // Step 12
    assert(CountArrayValues(stack, module) === 1,
           "Current module should appear exactly once in the stack");

    // Step 13
    assert(module.dfsAncestorIndex <= module.dfsIndex,
           "Bad DFS ancestor index");

    // Step 14
    if (module.dfsAncestorIndex === module.dfsIndex) {
        let requiredModule;
        do {
            requiredModule = callFunction(std_Array_pop, stack);
            ModuleSetStatus(requiredModule, MODULE_STATUS_EVALUATED);
        } while (requiredModule !== module);
    }

    // Step 15
    return index;
}
