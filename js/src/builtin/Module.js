/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function ModuleResolveExport(exportName, resolveSet = [], exportStarSet = [])
{
    // 15.2.1.16.3 ResolveExport(exportName, resolveSet, exportStarSet)

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
    resolveSet.push({module: module, exportName: exportName});

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
            let importedModule = HostResolveImportedModule(module, e.moduleRequest);
            let indirectResolution = importedModule.resolveExport(e.importName,
                                                                  resolveSet,
                                                                  exportStarSet);
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
    if (module in exportStarSet)
        return null;

    // Step 8
    exportStarSet.push(module);

    // Step 9
    let starResolution = null;

    // Step 10
    let starExportEntries = module.starExportEntries;
    for (let i = 0; i < starExportEntries.length; i++) {
        let e = starExportEntries[i];
        let importedModule = HostResolveImportedModule(module, e.moduleRequest);
        let resolution = importedModule.resolveExport(exportName, resolveSet, exportStarSet);
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

    // Step 8
    let requestedModules = module.requestedModules;
    for (let i = 0; i < requestedModules.length; i++) {
        let required = requestedModules[i];
        let requiredModule = HostResolveImportedModule(module, required);
        requiredModule.declarationInstantiation();
    }

    // Step 9
    let indirectExportEntries = module.indirectExportEntries;
    for (let i = 0; i < indirectExportEntries.length; i++) {
        let e = indirectExportEntries[i];
        let resolution = module.resolveExport(e.exportName);
        if (resolution === null)
            ThrowSyntaxError(JSMSG_MISSING_INDIRECT_EXPORT);
        if (resolution === "ambiguous")
            ThrowSyntaxError(JSMSG_AMBIGUOUS_INDIRECT_EXPORT);
    }

    // Step 12
    let importEntries = module.importEntries;
    for (let i = 0; i < importEntries.length; i++) {
        let imp = importEntries[i];
        let importedModule = HostResolveImportedModule(module, imp.moduleRequest);
        if (imp.importName === "*") {
            // TODO
            // let namespace = GetModuleNamespace(importedModule);
            // CreateNamespaceBinding(env, imp.localName, namespace);
        } else {
            let resolution = importedModule.resolveExport(imp.importName);
            if (resolution === null)
                ThrowSyntaxError(JSMSG_MISSING_IMPORT);
            if (resolution === "ambiguous")
                ThrowSyntaxError(JSMSG_AMBIGUOUS_IMPORT);
            CreateImportBinding(env, imp.localName, resolution.module, resolution.bindingName);
        }
    }
}

// 15.2.1.16.5 ModuleEvaluation()
function ModuleEvaluation()
{
    if (!IsObject(this) || !IsModule(this))
        return callFunction(CallModuleMethodIfWrapped, this, "ModuleEvaluation");

    // Step 1
    let module = this;

    // Step 4
    if (module.evaluated)
        return undefined;

    // Step 5
    SetModuleEvaluated(this);

    // Step 6
    let requestedModules = module.requestedModules;
    for (let i = 0; i < requestedModules.length; i++) {
        let required = requestedModules[i];
        let requiredModule = HostResolveImportedModule(module, required);
        requiredModule.evaluation();
    }

    return EvaluateModule(module);
}
