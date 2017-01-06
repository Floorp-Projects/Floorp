/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function CallModuleResolveHook(module, specifier, expectedMinimumState)
{
    let requestedModule = HostResolveImportedModule(module, specifier);
    if (requestedModule.state < expectedMinimumState)
        ThrowInternalError(JSMSG_BAD_MODULE_STATE);

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
                                                    MODULE_STATE_INSTANTIATED);
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

// 15.2.1.16.3 ResolveExport(exportName, resolveSet, exportStarSet)
function ModuleResolveExport(exportName, resolveSet = [], exportStarSet = [])
{
    if (!IsObject(this) || !IsModule(this)) {
        return callFunction(CallModuleMethodIfWrapped, this, exportName, resolveSet,
                            exportStarSet, "ModuleResolveExport");
    }

    // Step 1
    let module = this;

    // Step 2
    for (let i = 0; i < resolveSet.length; i++) {
        let r = resolveSet[i];
        if (r.module === module && r.exportName === exportName)
            return null;
    }

    // Step 3
    _DefineDataProperty(resolveSet, resolveSet.length, {module: module, exportName: exportName});

    // Step 4
    let localExportEntries = module.localExportEntries;
    for (let i = 0; i < localExportEntries.length; i++) {
        let e = localExportEntries[i];
        if (exportName === e.exportName)
            return {module: module, bindingName: e.localName};
    }

    // Step 5
    let indirectExportEntries = module.indirectExportEntries;
    for (let i = 0; i < indirectExportEntries.length; i++) {
        let e = indirectExportEntries[i];
        if (exportName === e.exportName) {
            let importedModule = CallModuleResolveHook(module, e.moduleRequest,
                                                       MODULE_STATE_INSTANTIATED);
            let indirectResolution = callFunction(importedModule.resolveExport, importedModule,
                                                  e.importName, resolveSet, exportStarSet);
            if (indirectResolution !== null)
                return indirectResolution;
        }
    }

    // Step 6
    if (exportName === "default") {
        // A default export cannot be provided by an export *.
        ThrowSyntaxError(JSMSG_BAD_DEFAULT_EXPORT);
    }

    // Step 7
    if (callFunction(ArrayIncludes, exportStarSet, module))
        return null;

    // Step 8
    _DefineDataProperty(exportStarSet, exportStarSet.length, module);

    // Step 9
    let starResolution = null;

    // Step 10
    let starExportEntries = module.starExportEntries;
    for (let i = 0; i < starExportEntries.length; i++) {
        let e = starExportEntries[i];
        let importedModule = CallModuleResolveHook(module, e.moduleRequest,
                                                   MODULE_STATE_INSTANTIATED);
        let resolution = callFunction(importedModule.resolveExport, importedModule,
                                      exportName, resolveSet, exportStarSet);
        if (resolution === "ambiguous")
            return resolution;

        if (resolution !== null) {
            if (starResolution === null) {
                starResolution = resolution;
            } else {
                if (resolution.module !== starResolution.module ||
                    resolution.exportName !== starResolution.exportName)
                {
                    return "ambiguous";
                }
            }
        }
    }

    return starResolution;
}

// 15.2.1.18 GetModuleNamespace(module)
function GetModuleNamespace(module)
{
    // Step 2
    let namespace = module.namespace;

    // Step 3
    if (typeof namespace === "undefined") {
        let exportedNames = callFunction(module.getExportedNames, module);
        let unambiguousNames = [];
        for (let i = 0; i < exportedNames.length; i++) {
            let name = exportedNames[i];
            let resolution = callFunction(module.resolveExport, module, name);
            if (resolution === null)
                ThrowSyntaxError(JSMSG_MISSING_NAMESPACE_EXPORT);
            if (resolution !== "ambiguous")
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
    callFunction(std_Array_sort, exports);

    let ns = NewModuleNamespace(module, exports);

    // Pre-compute all bindings now rather than calling ResolveExport() on every
    // access.
    for (let i = 0; i < exports.length; i++) {
        let name = exports[i];
        let binding = callFunction(module.resolveExport, module, name);
        assert(binding !== null && binding !== "ambiguous", "Failed to resolve binding");
        AddModuleNamespaceBinding(ns, name, binding.module, binding.bindingName);
    }

    return ns;
}

function GetModuleEnvironment(module)
{
    assert(IsModule(module), "Non-module passed to GetModuleEnvironment");

    // Check for a previous failed attempt to instantiate this module. This can
    // only happen due to a bug in the module loader.
    if (module.state == MODULE_STATE_FAILED)
        ThrowInternalError(JSMSG_MODULE_INSTANTIATE_FAILED);

    let env = UnsafeGetReservedSlot(module, MODULE_OBJECT_ENVIRONMENT_SLOT);
    assert(env === undefined || IsModuleEnvironment(env),
           "Module environment slot contains unexpected value");

    return env;
}

function RecordInstantationFailure(module)
{
    // Set the module's environment slot to 'null' to indicate a failed module
    // instantiation.
    assert(IsModule(module), "Non-module passed to RecordInstantationFailure");
    SetModuleState(module, MODULE_STATE_FAILED);
    UnsafeSetReservedSlot(module, MODULE_OBJECT_ENVIRONMENT_SLOT, undefined);
}

// 15.2.1.16.4 ModuleDeclarationInstantiation()
function ModuleDeclarationInstantiation()
{
    if (!IsObject(this) || !IsModule(this))
        return callFunction(CallModuleMethodIfWrapped, this, "ModuleDeclarationInstantiation");

    // Step 1
    let module = this;

    // Step 5
    if (GetModuleEnvironment(module) !== undefined)
        return;

    // Step 7
    CreateModuleEnvironment(module);
    let env = GetModuleEnvironment(module);

    SetModuleState(this, MODULE_STATE_INSTANTIATED);

    try {
        // Step 8
        let requestedModules = module.requestedModules;
        for (let i = 0; i < requestedModules.length; i++) {
            let required = requestedModules[i];
            let requiredModule = CallModuleResolveHook(module, required, MODULE_STATE_PARSED);
            callFunction(requiredModule.declarationInstantiation, requiredModule);
        }

        // Step 9
        let indirectExportEntries = module.indirectExportEntries;
        for (let i = 0; i < indirectExportEntries.length; i++) {
            let e = indirectExportEntries[i];
            let resolution = callFunction(module.resolveExport, module, e.exportName);
            if (resolution === null)
                ThrowSyntaxError(JSMSG_MISSING_INDIRECT_EXPORT, e.exportName);
            if (resolution === "ambiguous")
                ThrowSyntaxError(JSMSG_AMBIGUOUS_INDIRECT_EXPORT, e.exportName);
        }

        // Step 12
        let importEntries = module.importEntries;
        for (let i = 0; i < importEntries.length; i++) {
            let imp = importEntries[i];
            let importedModule = CallModuleResolveHook(module, imp.moduleRequest,
                                                       MODULE_STATE_INSTANTIATED);
            if (imp.importName === "*") {
                let namespace = GetModuleNamespace(importedModule);
                CreateNamespaceBinding(env, imp.localName, namespace);
            } else {
                let resolution = callFunction(importedModule.resolveExport, importedModule,
                                              imp.importName);
                if (resolution === null)
                    ThrowSyntaxError(JSMSG_MISSING_IMPORT, imp.importName);
                if (resolution === "ambiguous")
                    ThrowSyntaxError(JSMSG_AMBIGUOUS_IMPORT, imp.importName);
                CreateImportBinding(env, imp.localName, resolution.module, resolution.bindingName);
            }
        }

        // Step 16.iv
        InstantiateModuleFunctionDeclarations(module);
    } catch (e) {
        RecordInstantationFailure(module);
        throw e;
    }
}
_SetCanonicalName(ModuleDeclarationInstantiation, "ModuleDeclarationInstantiation");

// 15.2.1.16.5 ModuleEvaluation()
function ModuleEvaluation()
{
    if (!IsObject(this) || !IsModule(this))
        return callFunction(CallModuleMethodIfWrapped, this, "ModuleEvaluation");

    // Step 1
    let module = this;

    if (module.state < MODULE_STATE_INSTANTIATED)
        ThrowInternalError(JSMSG_BAD_MODULE_STATE);

    // Step 4
    if (module.state == MODULE_STATE_EVALUATED)
        return undefined;

    // Step 5
    SetModuleState(this, MODULE_STATE_EVALUATED);

    // Step 6
    let requestedModules = module.requestedModules;
    for (let i = 0; i < requestedModules.length; i++) {
        let required = requestedModules[i];
        let requiredModule = CallModuleResolveHook(module, required, MODULE_STATE_INSTANTIATED);
        callFunction(requiredModule.evaluation, requiredModule);
    }

    return EvaluateModule(module);
}
_SetCanonicalName(ModuleEvaluation, "ModuleEvaluation");
