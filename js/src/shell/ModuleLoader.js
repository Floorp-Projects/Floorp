/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global getModuleLoadPath setModuleLoadHook setModuleResolveHook setModuleMetadataHook */
/* global getModulePrivate setModulePrivate parseModule os */
/* global setModuleDynamicImportHook finishDynamicModuleImport abortDynamicModuleImport */

// A basic synchronous module loader for testing the shell.
//
// Supports loading files and 'javascript:' URLs that embed JS source text.

{
// Save standard built-ins before scripts can modify them.
const ArrayPrototypeJoin = Array.prototype.join;
const MapPrototypeGet = Map.prototype.get;
const MapPrototypeHas = Map.prototype.has;
const MapPrototypeSet = Map.prototype.set;
const ObjectDefineProperty = Object.defineProperty;
const ReflectApply = Reflect.apply;
const StringPrototypeIndexOf = String.prototype.indexOf;
const StringPrototypeLastIndexOf = String.prototype.lastIndexOf;
const StringPrototypeStartsWith = String.prototype.startsWith;
const StringPrototypeSubstring = String.prototype.substring;
const ErrorClass = Error;
const PromiseClass = Promise;
const PromiseResolve = Promise.resolve;

const JAVASCRIPT_SCHEME = "javascript:";

const ReflectLoader = new class {
    constructor() {
        this.registry = new Map();
        this.loadPath = getModuleLoadPath();
    }

    isJavascriptURL(name) {
        return ReflectApply(StringPrototypeStartsWith, name, [JAVASCRIPT_SCHEME]);
    }

    resolve(name, referencingInfo) {
        if (name === "") {
            throw new ErrorClass("Invalid module specifier");
        }

        if (this.isJavascriptURL(name) || os.path.isAbsolute(name)) {
            return name;
        }

        let loadPath = this.loadPath;

        // Treat |name| as a relative path if it starts with either "./"
        // or "../".
        let isRelative = ReflectApply(StringPrototypeStartsWith, name, ["./"])
                      || ReflectApply(StringPrototypeStartsWith, name, ["../"])
#ifdef XP_WIN
                      || ReflectApply(StringPrototypeStartsWith, name, [".\\"])
                      || ReflectApply(StringPrototypeStartsWith, name, ["..\\"])
#endif
                         ;

        // If |name| is a relative path and the referencing module's path is
        // available, load |name| relative to the that path.
        if (isRelative) {
            if (!referencingInfo) {
                throw new ErrorClass("No referencing module for relative import");
            }

            let path = referencingInfo.path;

            let sepIndex = ReflectApply(StringPrototypeLastIndexOf, path, ["/"]);
#ifdef XP_WIN
            let otherSepIndex = ReflectApply(StringPrototypeLastIndexOf, path, ["\\"]);
            if (otherSepIndex > sepIndex)
                sepIndex = otherSepIndex;
#endif
            if (sepIndex >= 0)
                loadPath = ReflectApply(StringPrototypeSubstring, path, [0, sepIndex]);
        }

        return os.path.join(loadPath, name);
    }

    normalize(path) {
        if (this.isJavascriptURL(path)) {
            return path;
        }

#ifdef XP_WIN
        // Replace all forward slashes with backward slashes.
        // NB: It may be tempting to replace this loop with a call to
        // String.prototype.replace, but user scripts may have modified
        // String.prototype or RegExp.prototype built-in functions, which makes
        // it unsafe to call String.prototype.replace.
        let newPath = "";
        let lastSlash = 0;
        while (true) {
            let i = ReflectApply(StringPrototypeIndexOf, path, ["/", lastSlash]);
            if (i < 0) {
                newPath += ReflectApply(StringPrototypeSubstring, path, [lastSlash]);
                break;
            }
            newPath += ReflectApply(StringPrototypeSubstring, path, [lastSlash, i]) + "\\";
            lastSlash = i + 1;
        }
        path = newPath;

        // Remove the drive letter, if present.
        let isalpha = c => ("A" <= c && c <= "Z") || ("a" <= c && c <= "z");
        let drive = "";
        if (path.length > 2 && isalpha(path[0]) && path[1] === ":" && path[2] === "\\") {
            drive = ReflectApply(StringPrototypeSubstring, path, [0, 2]);
            path = ReflectApply(StringPrototypeSubstring, path, [2]);
        }
#endif

        const pathsep =
#ifdef XP_WIN
        "\\";
#else
        "/";
#endif

        let n = 0;
        let components = [];

        // Normalize the path by removing redundant path components.
        // NB: See above for why we don't call String.prototype.split here.
        let lastSep = 0;
        while (lastSep < path.length) {
            let i = ReflectApply(StringPrototypeIndexOf, path, [pathsep, lastSep]);
            if (i < 0)
                i = path.length;
            let part = ReflectApply(StringPrototypeSubstring, path, [lastSep, i]);
            lastSep = i + 1;

            // Remove "." when preceded by a path component.
            if (part === "." && n > 0)
                continue;

            if (part === ".." && n > 0) {
                // Replace "./.." with "..".
                if (components[n - 1] === ".") {
                    components[n - 1] = "..";
                    continue;
                }

                // When preceded by a non-empty path component, remove ".." and
                // the preceding component, unless the preceding component is also
                // "..".
                if (components[n - 1] !== "" && components[n - 1] !== "..") {
                    components.length = --n;
                    continue;
                }
            }

            ObjectDefineProperty(components, n++, {
                __proto__: null,
                value: part,
                writable: true, enumerable: true, configurable: true,
            });
        }

        let normalized = ReflectApply(ArrayPrototypeJoin, components, [pathsep]);
#ifdef XP_WIN
        normalized = drive + normalized;
#endif
        return normalized;
    }

    fetch(path) {
        if (this.isJavascriptURL(path)) {
            return ReflectApply(StringPrototypeSubstring, path, [JAVASCRIPT_SCHEME.length]);
        }

        return os.file.readFile(path);
    }

    loadAndParse(path) {
        let normalized = this.normalize(path);
        if (ReflectApply(MapPrototypeHas, this.registry, [normalized]))
            return ReflectApply(MapPrototypeGet, this.registry, [normalized]);

        let source = this.fetch(path);
        let module = parseModule(source, path);
        let moduleInfo = { path: normalized };
        setModulePrivate(module, moduleInfo);
        ReflectApply(MapPrototypeSet, this.registry, [normalized, module]);
        return module;
    }

    loadAndExecute(path) {
        let module = this.loadAndParse(path);
        module.declarationInstantiation();
        return module.evaluation();
    }

    importRoot(path) {
        return this.loadAndExecute(path);
    }

    ["import"](name, referencingInfo) {
        let path = this.resolve(name, null);
        return this.loadAndExecute(path);
    }

    populateImportMeta(moduleInfo, metaObject) {
        // For the shell, use the module's normalized path as the base URL.

        let path;
        if (moduleInfo) {
            path = moduleInfo.path;
        } else {
            path = "(unknown)";
        }
        metaObject.url = path;
    }

    dynamicImport(referencingInfo, specifier, promise) {
        ReflectApply(PromiseResolve, PromiseClass, [])
            .then(_ => {
                let path = ReflectLoader.resolve(specifier, referencingInfo);
                ReflectLoader.loadAndExecute(path);
                finishDynamicModuleImport(referencingInfo, specifier, promise);
            }).catch(err => {
                abortDynamicModuleImport(referencingInfo, specifier, promise, err);
            });
    }
};

setModuleLoadHook((path) => ReflectLoader.importRoot(path));

setModuleResolveHook((referencingInfo, requestName) => {
    let path = ReflectLoader.resolve(requestName, referencingInfo);
    return ReflectLoader.loadAndParse(path);
});

setModuleMetadataHook((module, metaObject) => {
    ReflectLoader.populateImportMeta(module, metaObject);
});

setModuleDynamicImportHook((referencingInfo, specifier, promise) => {
    ReflectLoader.dynamicImport(referencingInfo, specifier, promise);
});
}
