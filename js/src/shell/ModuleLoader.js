/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global getModuleLoadPath setModuleLoadHook setModuleResolveHook setModuleMetadataHook */
/* global parseModule os */ 

// A basic synchronous module loader for testing the shell.
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

const ReflectLoader = new class {
    constructor() {
        this.registry = new Map();
        this.modulePaths = new Map();
        this.loadPath = getModuleLoadPath();
    }

    resolve(name, module) {
        if (os.path.isAbsolute(name))
            return name;

        let loadPath = this.loadPath;
        if (module) {
            // Treat |name| as a relative path if it starts with either "./"
            // or "../".
            let isRelative = ReflectApply(StringPrototypeStartsWith, name, ["./"])
                          || ReflectApply(StringPrototypeStartsWith, name, ["../"])
#ifdef XP_WIN
                          || ReflectApply(StringPrototypeStartsWith, name, [".\\"])
                          || ReflectApply(StringPrototypeStartsWith, name, ["..\\"])
#endif
                             ;

            // If |name| is a relative path and |module|'s path is available,
            // load |name| relative to the referring module.
            if (isRelative && ReflectApply(MapPrototypeHas, this.modulePaths, [module])) {
                let modulePath = ReflectApply(MapPrototypeGet, this.modulePaths, [module]);
                let sepIndex = ReflectApply(StringPrototypeLastIndexOf, modulePath, ["/"]);
#ifdef XP_WIN
                let otherSepIndex = ReflectApply(StringPrototypeLastIndexOf, modulePath, ["\\"]);
                if (otherSepIndex > sepIndex)
                    sepIndex = otherSepIndex;
#endif
                if (sepIndex >= 0)
                    loadPath = ReflectApply(StringPrototypeSubstring, modulePath, [0, sepIndex]);
            }
        }

        return os.path.join(loadPath, name);
    }

    normalize(path) {
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
                writable: true, enumerable: true, configurable: true
            });
        }

        let normalized = ReflectApply(ArrayPrototypeJoin, components, [pathsep]);
#ifdef XP_WIN
        normalized = drive + normalized;
#endif
        return normalized;
    }

    fetch(path) {
        return os.file.readFile(path);
    }

    loadAndParse(path) {
        let normalized = this.normalize(path);
        if (ReflectApply(MapPrototypeHas, this.registry, [normalized]))
            return ReflectApply(MapPrototypeGet, this.registry, [normalized]);

        let source = this.fetch(path);
        let module = parseModule(source, path);
        ReflectApply(MapPrototypeSet, this.registry, [normalized, module]);
        ReflectApply(MapPrototypeSet, this.modulePaths, [module, path]);
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

    ["import"](name, referrer) {
        let path = this.resolve(name, null);
        return this.loadAndExecute(path);
    }

    populateImportMeta(module, metaObject) {
        // For the shell, use the script's filename as the base URL.

        let path;
        if (ReflectApply(MapPrototypeHas, this.modulePaths, [module])) {
            path = ReflectApply(MapPrototypeGet, this.modulePaths, [module]);
        } else {
            path = "(unknown)";
        }
        metaObject.url = path;
    }
};

setModuleLoadHook((path) => ReflectLoader.importRoot(path));

setModuleResolveHook((module, requestName) => {
    let path = ReflectLoader.resolve(requestName, module);
    return ReflectLoader.loadAndParse(path);
});

setModuleMetadataHook((module, metaObject) => {
    ReflectLoader.populateImportMeta(module, metaObject);
});

}

