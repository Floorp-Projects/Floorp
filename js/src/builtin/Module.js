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
